/* Linked into every program C Editor builds: what code written on Windows expects of its C runtime and console, on
   macOS. Everything is weak, so a program's own strrev wins, and hidden, so these answer before the system's. What the
   system does underneath is looked up by name: a program's own read() or close() must not end up being called from here. */
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <termios.h>

#define WEAK __attribute__((weak, visibility("hidden")))
#define SYSTEM(name, type) ((type)dlsym(RTLD_NEXT, name))

/* A console shows text at once, so what a program printed survives its crash. */
__attribute__((constructor)) static void show_output_at_once(void) {
    if (SYSTEM("isatty", int (*)(int))(1)) setvbuf(stdout, NULL, _IONBF, 0);
}

/* ---- Locale, flushing, gets -------------------------------------------------------------------------------- */

/* Windows locale names become macOS UTF-8 locales: Turkish ones ("Turkish", "Turkish_Turkey.1254", "tr-TR", ...) the
   Turkish locale, English ones the English one. Other names macOS does not know fall back to the Turkish locale programs run with. */
WEAK char* setlocale(int category, const char* locale) {
    char* (*real)(int, const char*) = SYSTEM("setlocale", char* (*)(int, const char*));
    if (locale && (strncasecmp(locale, "turk", 4) == 0 || strncasecmp(locale, "tr", 2) == 0 || strstr(locale, "1254") || strstr(locale, "857")))
        locale = "tr_TR.UTF-8";
    else if (locale && (strncasecmp(locale, "english", 7) == 0 || strncasecmp(locale, "en", 2) == 0))
        locale = "en_US.UTF-8";
    char* result = real(category, locale);
    return result ? result : real(category, "");
}

/* fflush(stdin) discards the rest of the typed line, as the Windows C runtime does. */
WEAK int fflush(FILE* stream) {
    return stream == stdin ? fpurge(stream) : SYSTEM("fflush", int (*)(FILE*))(stream);
}

/* Without the warning macOS prints at run time. */
WEAK char* gets(char* target) {
    char* at = target;
    int c = getchar();
    if (c == EOF) return NULL;
    for (; c != EOF && c != '\n'; c = getchar()) *at++ = (char)c;
    *at = '\0';
    return target;
}

WEAK int fputchar(int c) { return putc(c, stdout); }
WEAK int fgetchar(void) { return getc(stdin); }

/* ---- Text and numbers --------------------------------------------------------------------------------------- */

WEAK char* strrev(char* text) {
    const size_t length = strlen(text);
    for (size_t i = 0; i < length / 2; ++i) {
        const char kept = text[i];
        text[i] = text[length - 1 - i];
        text[length - 1 - i] = kept;
    }
    return text;
}

WEAK char* strupr(char* text) {
    for (char* p = text; *p; ++p)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
    return text;
}

WEAK char* strlwr(char* text) {
    for (char* p = text; *p; ++p)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return text;
}

WEAK char* strnset(char* text, int c, size_t count) {
    for (char* p = text; *p && count > 0; ++p, --count) *p = (char)c;
    return text;
}

WEAK char* strset(char* text, int c) { return strnset(text, c, SIZE_MAX); }
WEAK int stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
WEAK int strcmpi(const char* a, const char* b) { return strcasecmp(a, b); }
WEAK int strnicmp(const char* a, const char* b, size_t count) { return strncasecmp(a, b, count); }

WEAK int memicmp(const void* a, const void* b, size_t count) {
    const unsigned char* x = a;
    const unsigned char* y = b;
    for (; count > 0; --count, ++x, ++y) {
        const int difference = tolower(*x) - tolower(*y);
        if (difference != 0) return difference;
    }
    return 0;
}

/* A negative number has a sign only in base 10; otherwise its bits are the number. */
static char* to_text(unsigned long long rest, bool negative, char* target, int radix) {
    char digits[66];
    int count = 0;
    do {
        digits[count++] = "0123456789abcdefghijklmnopqrstuvwxyz"[rest % (unsigned)radix];
        rest /= (unsigned)radix;
    } while (rest > 0);
    char* at = target;
    if (negative) *at++ = '-';
    while (count > 0) *at++ = digits[--count];
    *at = '\0';
    return target;
}

WEAK char* itoa(int value, char* target, int radix) {
    const bool negative = radix == 10 && value < 0;
    return to_text(negative ? 0u - (unsigned)value : (unsigned)value, negative, target, radix);
}

WEAK char* lltoa(long long value, char* target, int radix) {
    const bool negative = radix == 10 && value < 0;
    return to_text(negative ? 0ull - (unsigned long long)value : (unsigned long long)value, negative, target, radix);
}

WEAK char* ltoa(long value, char* target, int radix) { return lltoa(value, target, radix); }
WEAK char* ultoa(unsigned long value, char* target, int radix) { return to_text(value, false, target, radix); }
WEAK char* ulltoa(unsigned long long value, char* target, int radix) { return to_text(value, false, target, radix); }

/* ---- The keyboard: <conio.h> and "press any key" ---------------------------------------------------------------- */

static void set_key_mode(int fd, struct termios* saved, tcflag_t also_off) {
    tcgetattr(fd, saved);
    struct termios raw = *saved;
    raw.c_lflag &= (tcflag_t)~(ICANON | also_off);
    raw.c_iflag &= (tcflag_t)~ICRNL;  /* Enter is 13 to getch on Windows */
    tcsetattr(fd, TCSANOW, &raw);
}

static int pending_key = -1;  /* a key put back, or the second half of a key that reads as two codes */

static bool more_keys(void) {
    if (stdin->_r > 0) return true;
    fd_set ready;
    FD_ZERO(&ready);
    FD_SET(0, &ready);
    struct timeval now = {0, 0};
    return SYSTEM("select", int (*)(int, fd_set*, fd_set*, fd_set*, struct timeval*))(1, &ready, NULL, NULL, &now) > 0;
}

/* As on Windows: 224 and then the scan code for an arrow, 8 for Backspace, and 3 for Control-C rather than stopping. */
static int read_key(tcflag_t also_off) {
    if (pending_key >= 0) {
        const int key = pending_key;
        pending_key = -1;
        return key;
    }
    struct termios saved;
    fflush(stdout);
    set_key_mode(0, &saved, also_off | ISIG);
    int key = getchar();
    if (key == 0x7F) key = 8;
    if (key == 0x1B && more_keys() && getchar() == '[') {
        const int code = getchar();
        pending_key = code == 'A' ? 72 : code == 'B' ? 80 : code == 'C' ? 77 : code == 'D' ? 75 : -1;
        if (pending_key >= 0) key = 224;
    }
    tcsetattr(0, TCSANOW, &saved);
    return key;
}

WEAK int getch(void) { return read_key(ECHO); }
WEAK int getche(void) { return read_key(0); }
WEAK int putch(int c) { return putchar(c); }
WEAK int cputs(const char* text) { return fputs(text, stdout) == EOF ? EOF : 0; }

WEAK int cprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int result = vprintf(format, args);
    va_end(args);
    return result;
}

WEAK int kbhit(void) {
    if (pending_key >= 0) return 1;
    struct termios saved;
    set_key_mode(0, &saved, ECHO);
    const bool waiting = more_keys();
    tcsetattr(0, TCSANOW, &saved);
    return waiting;
}

/* Only one key can wait; a second is refused. */
WEAK int ungetch(int c) {
    if (pending_key >= 0) return EOF;
    pending_key = (unsigned char)c;
    return pending_key;
}

static int read_echoed_key(void* cookie, char* buffer, int capacity) {
    (void)cookie;
    (void)capacity;
    buffer[0] = (char)getche();
    return 1;
}

/* Key by key; the key that ended a field stays for the next read. */
WEAK int cscanf(const char* format, ...) {
    FILE* keys = funopen(NULL, read_echoed_key, NULL, NULL, NULL);
    if (!keys) return EOF;
    setvbuf(keys, NULL, _IONBF, 0);
    va_list args;
    va_start(args, format);
    const int result = vfscanf(keys, format, args);
    va_end(args);
    if (keys->_r > 0) ungetch(getc(keys));
    fclose(keys);
    return result;
}

/* buffer[0] is the most to read, buffer[1] gets the count, the text starts at buffer + 2. Enter ends it, Backspace takes a whole UTF-8 letter. */
WEAK char* cgets(char* buffer) {
    const int most = (unsigned char)buffer[0];
    int count = 0;
    while (count < most) {
        const int key = getch();
        if (key == 13) break;
        if (key == 224 || key == 0) {
            getch();
        } else if (key == 8) {
            while (count > 0 && ((unsigned char)buffer[2 + count - 1] & 0xC0) == 0x80) --count;
            if (count > 0) {
                --count;
                fputs("\b \b", stdout);
            }
        } else {
            buffer[2 + count++] = (char)key;
            putchar(key);
        }
    }
    putchar('\n');
    buffer[1] = (char)count;
    buffer[2 + count] = '\0';
    return buffer + 2;
}

static void wait_key(void) {
    const int fd = SYSTEM("open", int (*)(const char*, int, ...))("/dev/tty", O_RDONLY);
    if (fd < 0) return;
    struct termios saved;
    set_key_mode(fd, &saved, ECHO);
    char key;
    (void)SYSTEM("read", long (*)(int, void*, size_t))(fd, &key, 1);
    tcflush(fd, TCIFLUSH);  /* the rest of a key that sends several bytes: arrows, Turkish letters */
    tcsetattr(fd, TCSANOW, &saved);
    SYSTEM("close", int (*)(int))(fd);
}

/* cmd.exe built-ins: chcp, title, mode and color are no-ops (the console is UTF-8), cls clears, pause waits. */
WEAK int system(const char* command) {
    int (*real)(const char*) = SYSTEM("system", int (*)(const char*));
    if (!command) return real(command);
    while (*command == ' ' || *command == '@') ++command;
    if (strncasecmp(command, "chcp", 4) == 0 || strncasecmp(command, "title", 5) == 0 || strncasecmp(command, "mode", 4) == 0 ||
        strncasecmp(command, "color", 5) == 0)
        return 0;
    if (strncasecmp(command, "cls", 3) == 0) {
        fputs("\033[2J\033[H", stdout);
        fflush(stdout);
        return 0;
    }
    if (strncasecmp(command, "pause", 5) == 0) {
        fflush(stdout);
        if (!strstr(command, "nul")) {
            fputs("Devam etmek için bir tuşa basın . . . ", stdout);  /* Turkish Windows' own words */
            fflush(stdout);
        }
        wait_key();
        putchar('\n');
        return 0;
    }
    return real(command);
}

/* ---- <windows.h> and <direct.h> ------------------------------------------------------------------------------- */

WEAK int SetConsoleOutputCP(unsigned code_page) { (void)code_page; return 1; }
WEAK int SetConsoleCP(unsigned code_page) { (void)code_page; return 1; }
WEAK void Sleep(unsigned milliseconds) { SYSTEM("usleep", int (*)(unsigned))(milliseconds * 1000u); }

WEAK int Beep(unsigned frequency, unsigned milliseconds) {
    (void)frequency;
    putchar('\a');
    fflush(stdout);
    Sleep(milliseconds);
    return 1;
}

WEAK void* GetStdHandle(unsigned kind) {
    return kind == (unsigned)-10 ? (void*)stdin : kind == (unsigned)-11 ? (void*)stdout : kind == (unsigned)-12 ? (void*)stderr : (void*)-1;
}

/* The console has no colours. */
WEAK int SetConsoleTextAttribute(void* handle, unsigned short attributes) {
    (void)handle;
    (void)attributes;
    return 1;
}

WEAK int cedit_set_cursor(void* handle, short x, short y) {
    (void)handle;
    printf("\033[%d;%dH", y + 1, x + 1);
    fflush(stdout);
    return 1;
}

/* mkdir on Windows has no mode. */
WEAK int cedit_mkdir(const char* path) { return SYSTEM("mkdir", int (*)(const char*, unsigned short))(path, 0777); }
