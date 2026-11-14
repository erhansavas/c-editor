/* Linked into every program C Editor builds: what code written on Windows expects of its C runtime and console, on
   macOS. C Editor ships this file as it is and the Mac's own compiler builds it, once, for the programs it compiles.
   Everything is weak, so a program's own strrev wins, and hidden, so these answer before the system's. What the
   system does underneath is looked up by name: a program's own read() or close() must not end up being called from here. */
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <locale.h>
#include <malloc/malloc.h>
#include <signal.h>
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

/* ---- The console ----------------------------------------------------------------------------------------------- */

/* The compat header sets this in a program compiled from a Windows-1254 file. Its text is one byte a letter, as on
   Windows; the console is UTF-8, so what the program writes there and reads from there changes form on the way. */
WEAK int cedit_windows_1254 = 0;

/* 0x80-0x9F of Windows-1254; a byte it leaves undefined is the control character of the same number. */
static const unsigned short windows_high[32] = {
    0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x008e, 0x008f,
    0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x009e, 0x0178,
};

static unsigned code_point_of(unsigned char byte) {
    if (byte < 0x80) return byte;
    if (byte < 0xA0) return windows_high[byte - 0x80];
    switch (byte) {
    case 0xD0: return 0x011E;  /* Ğ */
    case 0xDD: return 0x0130;  /* İ */
    case 0xDE: return 0x015E;  /* Ş */
    case 0xF0: return 0x011F;  /* ğ */
    case 0xFD: return 0x0131;  /* ı */
    case 0xFE: return 0x015F;  /* ş */
    default: return byte;
    }
}

static unsigned char byte_of(unsigned code_point) {
    if (code_point < 0x80) return (unsigned char)code_point;
    for (unsigned byte = 0x80; byte < 0x100; ++byte)
        if (code_point_of((unsigned char)byte) == code_point) return (unsigned char)byte;
    return '?';
}

static void write_all(int fd, const char* data, size_t length) {
    static long (*system_write)(int, const void*, size_t);
    if (!system_write) system_write = SYSTEM("write", long (*)(int, const void*, size_t));
    while (length > 0) {
        const long sent = system_write(fd, data, length);
        if (sent <= 0) return;
        data += sent;
        length -= (size_t)sent;
    }
}

static int write_as_utf8(void* cookie, const char* data, int length) {
    const int fd = ((FILE*)cookie)->_file;
    char out[4096];
    size_t used = 0;
    for (int i = 0; i < length; ++i) {
        const unsigned c = code_point_of((unsigned char)data[i]);
        if (c < 0x80) {
            out[used++] = (char)c;
        } else if (c < 0x800) {
            out[used++] = (char)(0xC0 | c >> 6);
            out[used++] = (char)(0x80 | (c & 0x3F));
        } else {
            out[used++] = (char)(0xE0 | c >> 12);
            out[used++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[used++] = (char)(0x80 | (c & 0x3F));
        }
        if (used > sizeof out - 4) {
            write_all(fd, out, used);
            used = 0;
        }
    }
    write_all(fd, out, used);
    return length;
}

/* A letter whose bytes arrive in two reads waits here for its other half. */
static int read_from_utf8(void* cookie, char* buffer, int capacity) {
    static long (*system_read)(int, void*, size_t);
    static char pending[4];
    static size_t pending_length = 0;
    if (!system_read) system_read = SYSTEM("read", long (*)(int, void*, size_t));
    const int fd = ((FILE*)cookie)->_file;
    char in[1024];
    for (;;) {
        memcpy(in, pending, pending_length);
        const size_t room = sizeof in - pending_length;
        const long got = system_read(fd, in + pending_length, room < (size_t)capacity ? room : (size_t)capacity);
        if (got <= 0) return (int)got;
        const size_t total = pending_length + (size_t)got;
        pending_length = 0;
        int written = 0;
        for (size_t i = 0; i < total;) {
            const unsigned char lead = (unsigned char)in[i];
            const size_t need = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            if (i + need > total) {
                pending_length = total - i;
                memcpy(pending, in + i, pending_length);
                break;
            }
            unsigned code_point = need == 1 ? lead : lead & (0xFFu >> (need + 1));
            for (size_t k = 1; k < need; ++k) code_point = code_point << 6 | ((unsigned char)in[i + k] & 0x3Fu);
            buffer[written++] = (char)byte_of(code_point);
            i += need;
        }
        if (written > 0) return written;
    }
}

/* A console shows text at once, so what a program printed survives its crash. This object is linked ahead of the
   program's, so this runs before anything of the program's own. */
__attribute__((constructor)) static void start_console(void) {
    int (*is_console)(int) = SYSTEM("isatty", int (*)(int));
    if (is_console(1)) setvbuf(stdout, NULL, _IONBF, 0);
    if (!cedit_windows_1254) return;
    if (is_console(0)) stdin->_read = read_from_utf8;
    if (is_console(1)) stdout->_write = write_as_utf8;
    if (is_console(2)) stderr->_write = write_as_utf8;
}

/* Text of C Editor's own, which is UTF-8 whatever the program's is. */
static void console_message(const char* utf8) {
    fflush(stdout);
    write_all(1, utf8, strlen(utf8));
}

/* ---- Locale, flushing, gets -------------------------------------------------------------------------------- */

/* Windows locale names become macOS locales in the program's own text form: Turkish ones ("Turkish",
   "Turkish_Turkey.1254", "tr-TR", ...) the Turkish locale, English ones the English one, and "" the Turkish one a
   program runs with. Other names macOS does not know fall back to that one too. */
WEAK char* setlocale(int category, const char* locale) {
    char* (*real)(int, const char*) = SYSTEM("setlocale", char* (*)(int, const char*));
    const char* turkish = cedit_windows_1254 ? "tr_TR.ISO8859-9" : "tr_TR.UTF-8";
    if (locale && (strncasecmp(locale, "turk", 4) == 0 || strncasecmp(locale, "tr", 2) == 0 || strstr(locale, "1254") || strstr(locale, "857") || !*locale))
        locale = turkish;
    else if (locale && (strncasecmp(locale, "english", 7) == 0 || strncasecmp(locale, "en", 2) == 0))
        locale = cedit_windows_1254 ? "en_US.ISO8859-1" : "en_US.UTF-8";
    char* result = real(category, locale);
    return result ? result : real(category, turkish);
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

/* Letters above ASCII change case only where a byte is a letter, by the program's locale. */
WEAK char* strupr(char* text) {
    for (char* p = text; *p; ++p)
        if ((*p >= 'a' && *p <= 'z') || (cedit_windows_1254 && (unsigned char)*p >= 0x80)) *p = (char)toupper((unsigned char)*p);
    return text;
}

WEAK char* strlwr(char* text) {
    for (char* p = text; *p; ++p)
        if ((*p >= 'A' && *p <= 'Z') || (cedit_windows_1254 && (unsigned char)*p >= 0x80)) *p = (char)tolower((unsigned char)*p);
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

/* rand as Windows has it: the same numbers after the same srand, and none above its RAND_MAX of 32767. */
static unsigned random_state = 1;
WEAK void srand(unsigned seed) { random_state = seed; }
WEAK int rand(void) {
    random_state = random_state * 214013u + 2531011u;
    return (int)(random_state >> 16 & 0x7FFF);
}

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

/* buffer[0] is the most to read, buffer[1] gets the count, the text starts at buffer + 2. Enter ends it, Backspace takes a whole letter. */
WEAK char* cgets(char* buffer) {
    const int most = (unsigned char)buffer[0];
    int count = 0;
    while (count < most) {
        const int key = getch();
        if (key == 13) break;
        if (key == 224 || key == 0) {
            getch();
        } else if (key == 8) {
            while (!cedit_windows_1254 && count > 0 && ((unsigned char)buffer[2 + count - 1] & 0xC0) == 0x80) --count;
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

static bool is_command(const char* command, size_t length, const char* name) {
    const size_t name_length = strlen(name);
    return length >= name_length && strncasecmp(command, name, name_length) == 0 &&
           (length == name_length || command[name_length] == ' ' || command[name_length] == '.');
}

/* Whether the redirections after a command ("> nul", "2>nul", ">nul 2>&1") send nothing to a file, and whether
   they hide what the command says. */
static bool goes_nowhere(const char* tail, size_t length, bool* silent) {
    *silent = false;
    for (size_t i = 0;;) {
        while (i < length && tail[i] == ' ') ++i;
        if (i == length) return true;
        bool output = true;  /* ">" and "1>" are what the command says, "2>" its errors */
        if (tail[i] == '1' || tail[i] == '2') output = tail[i++] == '1';
        if (i == length || tail[i] != '>') return false;
        while (i < length && (tail[i] == '>' || tail[i] == ' ')) ++i;
        if (length - i >= 3 && strncasecmp(tail + i, "nul", 3) == 0 && (length - i == 3 || tail[i + 3] == ' ')) {
            *silent = *silent || output;
            i += 3;
        } else if (length - i >= 2 && tail[i] == '&' && (tail[i + 1] == '1' || tail[i + 1] == '2')) {
            i += 2;
        } else {
            return false;
        }
    }
}

/* One command of a line. cmd.exe's own are carried out here: cls clears, pause waits, echo writes, and chcp, title,
   mode and color have nothing to change. Anything else, and anything written to a file, is left to macOS, where
   Windows' "nul" is /dev/null. */
static int run_command(const char* command, size_t length) {
    while (length > 0 && (*command == ' ' || *command == '@')) ++command, --length;
    const char* redirect = memchr(command, '>', length);
    size_t text = redirect ? (size_t)(redirect - command) : length;
    if (text >= 2 && (command[text - 1] == '1' || command[text - 1] == '2') && command[text - 2] == ' ') --text;  /* "2>" */
    bool silent = false;
    const bool own = goes_nowhere(command + text, length - text, &silent);
    while (text > 0 && command[text - 1] == ' ') --text;
    if (text == 0) return 0;
    if (own && (is_command(command, text, "chcp") || is_command(command, text, "title") || is_command(command, text, "mode") ||
                is_command(command, text, "color")))
        return 0;
    if (own && is_command(command, text, "cls")) {
        fputs("\033[2J\033[H", stdout);
        fflush(stdout);
        return 0;
    }
    if (own && is_command(command, text, "pause")) {
        console_message(silent ? "" : "Devam etmek için bir tuşa basın . . . ");  /* Turkish Windows' own words */
        wait_key();
        putchar('\n');
        return 0;
    }
    if (own && is_command(command, text, "echo")) {
        const char* words = text > 5 ? command + 5 : "";
        const size_t count = text > 5 ? text - 5 : 0;
        const bool switches = (count == 3 && strncasecmp(words, "off", 3) == 0) || (count == 2 && strncasecmp(words, "on", 2) == 0);
        if (!silent && !switches && text > 4) printf("%.*s\n", (int)count, words);
        return 0;
    }
    char line[length * 3 + 1];  /* "nul" grows to "/dev/null" */
    size_t used = 0;
    for (size_t i = 0; i < length;) {
        line[used++] = command[i++];
        if (line[used - 1] != '>') continue;
        while (i < length && (command[i] == '>' || command[i] == ' ')) line[used++] = command[i++];
        if (length - i >= 3 && strncasecmp(command + i, "nul", 3) == 0 && (length - i == 3 || command[i + 3] == ' ')) {
            memcpy(line + used, "/dev/null", 9);
            used += 9;
            i += 3;
        }
    }
    line[used] = '\0';
    return SYSTEM("system", int (*)(const char*))(line);
}

/* A line as cmd.exe reads it: "a & b" runs both, "a && b" the second only if the first went well. The "&" of
   "2>&1" joins nothing. */
WEAK int system(const char* command) {
    if (!command) return 1;
    for (;;) {
        const char* ampersand = command;
        while (*ampersand && (*ampersand != '&' || (ampersand > command && ampersand[-1] == '>'))) ++ampersand;
        const int result = run_command(command, (size_t)(ampersand - command));
        const bool both_must_pass = ampersand[0] == '&' && ampersand[1] == '&';
        if (!*ampersand || (both_must_pass && result != 0)) return result;
        command = ampersand + (both_must_pass ? 2 : 1);
    }
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

/* ---- Memory never set reads as nonsense, like a local never set; not the 0 macOS often leaves there ------------ */

/* Only the start of a huge block: writing to all of it would take memory the program may never touch. */
static void fill(void* block, size_t from) {
    const size_t most = (size_t)16 << 20;
    const size_t size = malloc_size(block) < most ? malloc_size(block) : most;
    if (size > from) memset((char*)block + from, 0xAA, size - from);
}

WEAK void* malloc(size_t size) {
    static void* (*real)(size_t);
    if (!real) real = SYSTEM("malloc", void* (*)(size_t));
    void* block = real(size);
    if (block) fill(block, 0);
    return block;
}

WEAK void* realloc(void* old, size_t size) {
    static void* (*real)(void*, size_t);
    if (!real) real = SYSTEM("realloc", void* (*)(void*, size_t));
    const size_t had = old ? malloc_size(old) : 0;
    void* block = real(old, size);
    if (block) fill(block, had);
    return block;
}

/* C++'s new and new[]; when there is no memory, C++'s own says so the C++ way. */
WEAK void* cedit_new(size_t size) __asm("__Znwm");
WEAK void* cedit_new_array(size_t size) __asm("__Znam");
WEAK void* cedit_new(size_t size) {
    void* block = malloc(size ? size : 1);
    return block ? block : SYSTEM("_Znwm", void* (*)(size_t))(size);
}
WEAK void* cedit_new_array(size_t size) { return cedit_new(size); }

/* ---- What only a run can see: the compiler's checks call these (see compile.cpp) ---------------------------------- */

typedef struct {
    const char* file;
    unsigned line;
    unsigned column;
} SourcePlace;

/* An index past the end of an array: said once for each place, under the program's output, and the program goes on. */
WEAK void __ubsan_handle_out_of_bounds(void* place, uintptr_t index) {
    (void)index;
    SourcePlace* source = place;
    if (source->line == 0) return;
    char line[96];
    snprintf(line, sizeof line, "\n[C Editor] %u. satır: dizinin sınırı aşıldı.\n", source->line);
    source->line = 0;
    console_message(line);
}

/* An integer divided by zero stops the program, as a PC's processor does; an Apple chip would go on with a 0. */
WEAK void __ubsan_handle_divrem_overflow(void* place, uintptr_t dividend, uintptr_t divisor) {
    (void)place;
    (void)dividend;
    (void)divisor;
    SYSTEM("signal", void (*(*)(int, void (*)(int)))(int))(SIGFPE, SIG_DFL);
    SYSTEM("raise", int (*)(int))(SIGFPE);
}
