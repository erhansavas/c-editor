// Programs as a student writes them, compiled the way ⌘R does and run on a console like the app's.
#include <dirent.h>
#include <sys/stat.h>

#include <vector>

#include "core/diagnostics.hpp"
#include "support.hpp"

namespace {

// Saving never overwrites what it may not, and leaves a link pointing at the file it names.
void saving_respects_the_file() {
    const std::string read_only = scratch_dir + "/salt_okunur.c";
    CHECK(cedit::write_file_atomic(read_only, "ilk"));
    CHECK(::chmod(read_only.c_str(), 0444) == 0);
    CHECK(!cedit::write_file_atomic(read_only, "ikinci"));
    CHECK(cedit::read_file(read_only).value_or("") == "ilk");
    CHECK(::chmod(read_only.c_str(), 0644) == 0);

    const std::string target = scratch_dir + "/hedef.c";
    const std::string link = scratch_dir + "/bag.c";
    ::unlink(link.c_str());
    CHECK(cedit::write_file_atomic(target, "ilk") && ::symlink("hedef.c", link.c_str()) == 0);
    CHECK(cedit::write_file_atomic(link, "ikinci"));
    struct stat info {};
    CHECK(::lstat(link.c_str(), &info) == 0 && S_ISLNK(info.st_mode));
    CHECK(cedit::read_file(target).value_or("") == "ikinci");

    const auto first = cedit::file_stamp(target);
    CHECK(first && first == cedit::file_stamp(target));
    CHECK(cedit::write_file_atomic(target, "ucuncu!") && first != cedit::file_stamp(target));
    CHECK(!cedit::file_stamp(scratch_dir + "/olmayan.c"));
}

// A Turkish program exactly as Windows saves it: Windows-1254 with CRLF.
void compiles_and_runs_windows_program() {
    const cedit::CompileResult result = build(
        "ikisayitoplam.c",
        "# include <stdio.h>\r\n#include <locale.h>\r\n\r\nint main()\r\n{\r\n\r\n  setlocale(LC_ALL, \"Turkish\");\r\n"
        "int a,b;\r\n    printf(\"Birinci say\xFDy\xFD girin: \");\r\n    scanf(\"%d\", &a);\r\n"
        "    printf(\"\xDDkinci say\xFDy\xFD girin: \");\r\n    scanf(\"%d\", &b);\r\n    int toplam=a+b;\r\n"
        "    printf(\"\\nToplam: %d\",toplam);\r\n    system(\"pause\");\r\n    return 0;\r\n}\r\n");
    CHECK(result.ok && result.diagnostics.empty());
    CHECK(result.log.starts_with("Derleme başarılı (") && result.log.ends_with("): hata yok, uyarı yok.\n"));
    std::optional<Console> console = start(result);
    if (!console) return;
    CHECK(console->read_until("Birinci sayıyı girin: "));
    console->type("2\r");
    CHECK(console->read_until("İkinci sayıyı girin: "));
    console->type("99\r");
    CHECK(console->read_until("tuşa basın . . . "));
    console->type("x");
    CHECK(console->finish() == 0);
    CHECK(console->shown.find("Birinci sayıyı girin: 2") != std::string::npos);
    CHECK(console->shown.find("İkinci sayıyı girin: 99") != std::string::npos);
    CHECK(console->shown.find("Toplam: 101Devam etmek için bir tuşa basın . . . ") != std::string::npos);
}

// Habits GCC on Windows accepts: `main()` without int, sqrt without <math.h>, fflush(stdin) dropping the
// rest of a line, "pause" answered with a key that sends several bytes, and an English locale by its Windows name.
void runs_windows_habits() {
    const cedit::CompileResult result = build(
        "aliskanliklar.c",
        "#include <stdio.h>\r\n#include <locale.h>\r\nmain()\r\n{\r\n    setlocale(LC_ALL, \"English\");\r\n    int n; char c;\r\n    printf(\"Say\xFD: \");\r\n    scanf(\"%d\", &n);\r\n"
        "    fflush(stdin);\r\n    printf(\"Harf: \");\r\n    scanf(\"%c\", &c);\r\n    system(\"pause\");\r\n"
        "    printf(\"K\xF6k: %.1f %c\\n\", sqrt(n), c);\r\n    scanf(\"%d\", &n);\r\n    printf(\"Son: %d\\n\", n);\r\n}\r\n");
    CHECK(result.ok && result.diagnostics.size() == 2 && result.log.ends_with("): hata yok, 2 uyarı.\n"));
    for (const cedit::Diagnostic& diagnostic : result.diagnostics) CHECK(!diagnostic.error);
    std::optional<Console> console = start(result);
    if (!console) return;
    CHECK(console->read_until("Sayı: "));
    console->type("16\r");
    CHECK(console->read_until("Harf: "));
    console->type("x\r");
    CHECK(console->read_until("tuşa basın . . . "));
    console->type("\x1B[A");  // the up arrow
    CHECK(console->read_until("Kök: 4.0 x\n"));  // "English" means the English locale, as on Windows (not 4,0)
    console->type("7\r");
    CHECK(console->read_until("Son: 7\n"));
    CHECK(console->finish() == 0);
}

// How builds and programs end, in plain Turkish.
void plain_words() {
    const cedit::CompileResult broken = build("eksik.c", "int main() { return x; }\n", false);
    CHECK(!broken.ok && broken.log.find("\nDerleme başarısız (") != std::string::npos &&
          broken.log.ends_with("): 1 hata, uyarı yok.\n"));
    CHECK(cedit::duration_text(0.234) == "0,23 sn");
    CHECK(cedit::describe_exit(W_EXITCODE(3, 0), 1.25) == "Program 1,25 sn içinde bitti, çıkış kodu 3.");
    CHECK(cedit::describe_exit(W_EXITCODE(0, SIGSEGV), 0.5) ==
          "Program 0,50 sn sonra çöktü (segmentation fault).\r\nİzin verilmeyen bir bellek alanına erişti.\r\n"
          "Örneğin scanf’te & unutulmuş ya da dizinin sınırı aşılmış olabilir.");
    CHECK(cedit::describe_exit(W_EXITCODE(0, SIGABRT), 1) ==
          "Program 1,00 sn sonra abort() ile kendini durdurdu.\r\nÖrneğin bir assert koşulu sağlanmadı.");
    CHECK(cedit::describe_exit(W_EXITCODE(0, SIGINT), 2) == "Program 2,00 sn sonra Control-C ile durduruldu.");
    CHECK(cedit::describe_exit(W_EXITCODE(0, SIGTERM), 1) == "Program 1,00 sn sonra 15 numaralı sinyalle durduruldu.");
}

// Mistakes explained in Turkish, with the compiler's own fixes placed on the user's lines.
void explains_mistakes() {
    const cedit::CompileResult typo = build(
        "hatali.c", "#include <stdio.h>\nint main() {\n    int toplam = 0;\n    printf(\"%d\", topalm)\n    return 0;\n}\n",
        false);
    CHECK(typo.diagnostics.size() == 2 && typo.log.find("fix-it:") == std::string::npos);
    if (typo.diagnostics.size() == 2) {  // compilers before clang 21 report the missing ';' first
        const bool name_first = typo.diagnostics[0].message.starts_with("use of undeclared");
        const auto& name = typo.diagnostics[name_first ? 0 : 1].fix;
        const auto& semicolon = typo.diagnostics[name_first ? 1 : 0].fix;
        CHECK(name && name->line == 3 && name->column == 17 && name->end_column == 23 && name->text == "toplam");
        CHECK(semicolon && semicolon->line == 3 && semicolon->column == 24 && semicolon->text == ";");
    }
    CHECK(typo.log.find("  ➜ “topalm” tanımlı değil. “topalm” yerine “toplam” yazılmalı.\n") != std::string::npos);
    CHECK(typo.log.find("  ➜ Noktalı virgül (;) eksik. “;” yazılmalı.\n") != std::string::npos);
    CHECK(build("bos.c", "", false).log.find("  ➜ Programda main() fonksiyonu yok") != std::string::npos);
}

// A program finds its own header beside it, though it is compiled from a scratch copy.
void finds_own_headers() {
    CHECK(cedit::write_file_atomic(scratch_dir + "/kendi.h", "#define SELAM \"Selam\"\n"));
    CHECK(build("baslik.c", "#include <stdio.h>\n#include \"kendi.h\"\nint main() { printf(SELAM); return 0; }\n").ok);
}

// Apple's compiler is found where the Command Line Tools or Xcode put it.
void finds_developer_tools() {
    const auto make_compiler = [](const std::string& path) {
        for (std::size_t slash = path.find('/', 1); slash != std::string::npos; slash = path.find('/', slash + 1))
            ::mkdir(path.substr(0, slash).c_str(), 0700);
        CHECK(cedit::write_file_atomic(path, "#!/bin/sh\n") && ::chmod(path.c_str(), 0700) == 0);
    };
    make_compiler(scratch_dir + "/tools/CommandLineTools/usr/bin/clang");
    make_compiler(scratch_dir + "/tools/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang");
    ::mkdir((scratch_dir + "/tools/Empty").c_str(), 0700);
    CHECK(cedit::compiler_in(scratch_dir + "/tools/CommandLineTools") == scratch_dir + "/tools/CommandLineTools/usr/bin/clang");
    CHECK(cedit::compiler_in(scratch_dir + "/tools/Developer").has_value());
    CHECK(!cedit::compiler_in(scratch_dir + "/tools/Empty"));
}

// In C++ the replacements answer to their std:: names as well.
void compiles_cpp_program() {
    const cedit::CompileResult result = build(
        "merhaba.cpp",
        "#include <bits/stdc++.h>\nusing namespace std;\nint main() {\n    std::setlocale(LC_ALL, \"Turkish\");\n"
        "    cout << \"Merhaba\" << endl;\n    std::fflush(stdin);\n    std::system(\"pause\");\n}\n");
    CHECK(result.ok && result.diagnostics.empty());
}

// A Turkish letter is one character to Backspace: the program never receives half of it.
void backspace_takes_a_whole_letter() {
    auto console = start(build("harf.c", "#include <stdio.h>\nint main() {\n    int c;\n    while ((c = getchar()) != '\\n') printf(\"[%02X]\", c);\n"
                                         "    puts(\"son\");\n    return 0;\n}\n"));
    CHECK(console.has_value());
    if (!console) return;
    console->type("\xC5\x9F\x7F" "a\r");
    CHECK(console->read_until("son"));
    CHECK(console->shown.find("[61]son") != std::string::npos && console->shown.find("[C5]") == std::string::npos);
    CHECK(console->finish() == 0);
}

// The compiler sees the writer's lines as they are, so warnings that compare lines still come.
void warns_about_empty_bodies() {
    const cedit::CompileResult result = build(
        "bos.c", "#include <stdio.h>\nint main() {\n    int a = 1;\n    if (a == 2);\n    {\n        a = 3;\n    }\n"
                 "    for (int i = 0; i < 3; i++);\n        a++;\n    while (a < 3);\n        a++;\n    return a;\n}\n");
    CHECK(result.ok && result.diagnostics.size() == 3);
    CHECK(result.log.find("  ➜ if satırının sonunda fazladan noktalı virgül (;) var") != std::string::npos);
    CHECK(result.log.find("  ➜ Döngü satırının sonunda fazladan noktalı virgül (;) var") != std::string::npos);
    if (result.diagnostics.size() == 3) CHECK(result.diagnostics[0].line == 3 && result.diagnostics[1].line == 7 && result.diagnostics[2].line == 9);
}

// Names reach the compiler as one argument: nothing in them is ever read as a command or an option.
void odd_file_names() {
    const std::string hello = "int main(void) { return 0; }\n";
    for (const char* name : {"boşluklu ödev 1.c", "a\"b.c", "a'b$(touch kanit).c", "a;b`touch kanit`.c", "-dash.c", "back\\slash.c"}) {
        const cedit::CompileResult result = build(name, hello);
        CHECK(result.ok);
        const cedit::CompileResult broken = build(name, "int main(void) { return 0 }\n", false);
        CHECK(!broken.ok && broken.diagnostics.size() == 1 && broken.log.find("“;” yazılmalı.") != std::string::npos);
    }
    CHECK(::access((scratch_dir + "/work/kanit").c_str(), F_OK) != 0 && ::access("kanit", F_OK) != 0);
    const std::string cpp = "#include <iostream>\nint main() { std::cout << 1; }\n";
    for (const char* name : {"ODEV.CPP", "x.c++", "y.cp", "Z.Cc"}) CHECK(build(name, cpp).ok);
}

void hints_for_common_slips() {
    const auto log_of = [](const std::string& code) { return build("slip.c", code, false).log; };
    const std::string quotes = log_of("#include <stdio.h>\nint main() {\n    printf(“a”);\n    return 0;\n}\n");
    CHECK(quotes.find("  ➜ Bu tırnak işareti C’de geçmez") != std::string::npos && quotes.find("silinmeli") == std::string::npos);
    CHECK(log_of("#include <stdoi.h>\nint main() { return 0; }\n").find("  ➜ “stdoi.h” adında bir başlık dosyası bulunamadı") != std::string::npos);
    CHECK(log_of("int main() {\n    return 0;\n}\nprintf(\"a\");\n").find("  ➜ Bu satır fonksiyonun dışında kalmış") != std::string::npos);
    CHECK(log_of("int main() {\n    int a = 1;\n    1 = a;\n    return a;\n}\n").find("  ➜ = işaretinin solunda") != std::string::npos);
    CHECK(log_of("int main() {\n    const int a = 1;\n    a = 2;\n    return a;\n}\n").find("  ➜ “a” const tanımlanmış") != std::string::npos);
    CHECK(log_of("int main() {\n    break;\n}\n").find("  ➜ break yalnızca") != std::string::npos);
    CHECK(cedit::explain("non-void function 'f' should return a value", "").starts_with("Bu fonksiyon değer döndürmeli"));

    // A failed link has no error line of its own: the caret goes to the call the linker cannot find.
    const cedit::CompileResult typo = build("yazim.c", "#include <stdio.h>\nint main() {\n    int a = 1;\n    prinf(\"%d\", a);\n    return 0;\n}\n", false);
    const cedit::Diagnostic* place = cedit::first_problem(typo.diagnostics);
    CHECK(place != nullptr && place->line == 3);
    const cedit::CompileResult both = build("iki.c", "int main() {\n    unused();\n    return x;\n}\n", false);
    place = cedit::first_problem(both.diagnostics);
    CHECK(place != nullptr && place->error && place->line == 2);
    const std::vector<cedit::Diagnostic> only_warning = {{4, 0, false, "unused variable 'a'", std::nullopt}};
    CHECK(cedit::first_problem(only_warning) == nullptr);
}

// Code written on Windows, and a beginner's own names the system would otherwise clash with.
void windows_code() {
    const std::string head = "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\nint main(void) {\n";
    const auto [text, text_end] = run("metin.c", head + R"(char a[] = "abc", b[] = "kIl", c[] = "KiL", d[] = "abcd", n[70];
        printf("%s %s %s %s|", strrev(a), strupr(b), strlwr(c), strnset(d, 'y', 2)); printf("%s|%d %d %d|", strset(d, 'x'), stricmp("A", "a"), strnicmp("ab", "AC", 1), memicmp("Ab", "aC", 2) < 0);
        printf("%s|", itoa(255, n, 16)); printf("%s|", itoa(-5, n, 10)); printf("%s|", itoa(-1, n, 16)); printf("%s|", lltoa(-5000000000LL, n, 10)); printf("%s\n", ulltoa(255ULL, n, 2)); return 0; }
)");
    CHECK(text_end == 0 && has(text, "cba KIL kil yycd|xxxx|0 0 1|ff|-5|ffffffff|-5000000000|11111111"));

    // A program's own functions win, and may have the names of what the runtime uses underneath.
    const auto [own, own_end] = run("kendi.c", "#include <stdio.h>\n#include <stdlib.h>\nchar *strrev(char *s) { s[0] = 'K'; return s; }\nint read(void) { return 1; }\nint close(void) { return 2; }\nint write(int a) { return a; }\n"
        "int main(void) { char a[] = \"abc\"; printf(\"%s %d\\n\", strrev(a), read() + close() + write(5)); system(\"cls\"); printf(\"son\"); return 0; }\n");
    CHECK(own_end == 0 && has(own, "son"));
    CHECK(build("sirala.c", "#include <stdlib.h>\nint mergesort(int *a, int n) { return n + a[0]; }\nint main(void) { int a[1] = {0}; return mergesort(a, 0); }\n").ok);
    CHECK(build("kr.c", "#include <stdio.h>\nint getline(char s[], int lim) { s[0] = 0; return lim; }\nint main(void) { char b[2]; return getline(b, 0); }\n").ok);
    CHECK(build("indeks.c", "#include <string.h>\nint index = 3;\nint main(void) { return index - 3; }\n").ok);
    CHECK(build("rastgele.c", "#include <stdlib.h>\nint random(int a, int b) { return a + b; }\nint main(void) { return random(0, 0); }\n").ok);
    CHECK(build("kendi2.c", "int read(void) { return 0; }\nint main(void) { return read(); }\n").ok);
    // The same names remain the system's where a program means those.
    const auto [posix, posix_end] = run("posix.c", "#include <stdio.h>\nint main(void) { printf(\"%d\", random() >= 0); return 0; }\n");
    CHECK(posix_end == 0 && has(posix, "1"));
    // Text shows at once, so a crash does not lose it; division by zero stops the program and says why; doubles do not stop.
    const auto [zero, zero_end] = run("sifir.c", "#include <stdio.h>\nint main(void) {\n    int a = 5, b = 0;\n    printf(\"once\");\n    printf(\"%d\", a / b);\n    printf(\"sonra\");\n    return 0;\n}\n");
    CHECK(zero_end == 128 + SIGFPE && has(zero, "once") && !has(zero, "sonra"));
    CHECK(has(cedit::describe_exit(W_EXITCODE(0, SIGFPE), 1), "Bir tam sayı sıfıra bölündü"));
    CHECK(run("kalan.c", "#include <stdio.h>\nint main(void) {\n    int a = 5, b = 0;\n    printf(\"%d\", a % b);\n    return 0;\n}\n").second == 128 + SIGFPE);
    CHECK(run("ondalik.c", "#include <stdio.h>\nint main(void) {\n    double a = 5, b = 0;\n    printf(\"%f\", a / b);\n    return 0;\n}\n").second == 0);
    // A local never set does not read as a 0 that looks right.
    CHECK(has(run("deger.c", "#include <stdio.h>\nint main(void) {\n    int a[2];\n    printf(\"%x\", a[1]);\n    return 0;\n}\n").first, "aaaaaaaa"));
    // Headers and <windows.h> calls that Windows code names.
    CHECK(build("basliklar.c", "#include <malloc.h>\n#include <io.h>\n#include <process.h>\n#include <direct.h>\n#include <dos.h>\nint main(void) { return mkdir(\"build_tmp_yok/x\") == 0; }\n").ok);
    const auto [windows, windows_end] = run("pencere.cpp", "#include <windows.h>\n#include <stdio.h>\nint main() {\n    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);\n    SetConsoleTextAttribute(h, FOREGROUND_RED);\n    Beep(440, 1);\n    Sleep(1);\n"
        "    COORD c = {2, 1};\n    SetConsoleCursorPosition(h, c);\n    printf(\"%d %d\", (int)sizeof(DWORD), (int)strlen(\"ab\"));\n    return 0;\n}\n");
    CHECK(windows_end == 0 && has(windows, "4 2"));
    const auto [cpp_std, cpp_end] = run("std.cpp", "#include <cstdlib>\n#include <cstdio>\n#include <clocale>\nint main() {\n    std::setlocale(LC_ALL, \"Turkish\");\n    std::fflush(stdin);\n    std::system(\"cls\");\n    std::printf(\"tamam\");\n    return 0;\n}\n");
    CHECK(cpp_end == 0 && has(cpp_std, "tamam"));
    CHECK(has(build("yok.cpp", "int main() {\n    cout << 1;\n    return 0;\n}\n", false).log, "#include <iostream> satırı eksik"));

    // Keys read as on Windows; ungetch keeps one; cgets edits and stops; cscanf leaves the key that ended the field.
    std::optional<Console> keys = start(build("tus.c", "#include <stdio.h>\n#include <conio.h>\nint main(void) {\n    int i, a = 0, r;\n    char line[8] = {4};\n    r = ungetch('q'); printf(\"%c %d|\", r, ungetch('z')); printf(\"%d %c|\", kbhit() != 0, getch()); cputs(\"yaz\"); cprintf(\"%d\\n\", 7);\n    printf(\"hazir\");\n"
        "    for (i = 0; i < 5; i++) { printf(\"[%d]\", getch()); fflush(stdout); }\n    printf(\"satir\");\n    printf(\"<%s %d>\", cgets(line), line[1]);\n    printf(\"sayi\");\n    r = cscanf(\"%d\", &a);\n    printf(\"<%d %d %d>\", r, a, getch());\n    return 0;\n}\n"));
    if (keys) {
        CHECK(keys->read_until("hazir"));
        CHECK(has(keys->shown, "q -1|1 q|yaz7"));
        ::usleep(100'000);
        keys->type("\x1B[A");
        CHECK(keys->read_until("[224][72]"));
        ::usleep(50'000);
        keys->type("\r");
        CHECK(keys->read_until("[13]"));
        ::usleep(50'000);
        keys->type("\x7F");
        CHECK(keys->read_until("[8]"));
        ::usleep(50'000);
        keys->type("\x03");  // a key to getch, not an end to the program
        CHECK(keys->read_until("[3]satir"));
        ::usleep(100'000);
        keys->type("ab\x7F" "c\r");
        CHECK(keys->read_until("<ac 2>"));
        CHECK(keys->read_until("sayi"));
        ::usleep(100'000);
        keys->type("42\r");
        CHECK(keys->read_until("<1 42 13>"));
        keys->finish();
    }
}

// A file saved as Windows-1254 is compiled from its own bytes, as on Windows: a Turkish letter is one char in the
// program and the letter itself on the console, typed or printed. A UTF-8 file keeps the bytes it has.
void turkish_letters_are_single_chars() {
    const auto [values, values_end] = run("bayt.cpp", "#include <stdio.h>\n#include <string.h>\nint main() {\n    char c = '\xFE';\n    char ad[6] = \"\xDE" "eyma\";\n"
        "    printf(\"%d %d %d %s|%c\\n\", (int)sizeof(c), (int)strlen(\"\xDEifre\"), (int)strlen(ad), ad, c);\n    return 0;\n}\n");
    CHECK(values_end == 0 && has(values, "1 5 5 Şeyma|ş"));
    CHECK(has(run("utf.c", "#include <stdio.h>\n#include <string.h>\nint main(void) { printf(\"%d Şifre\\n\", (int)strlen(\"Şifre\")); return 0; }\n").first, "6 Şifre"));
    const std::string echo = "#include <stdio.h>\n#include <string.h>\nint main(void) {\n    char s[20];\n    scanf(\"%19s\", s);\n"
                             "    printf(\"%d %02X %s\\n\", (int)strlen(s), (unsigned char)s[1], s);\n    return 0;\n}\n";
    CHECK(has(run("girdi.c", echo, "aş\r").first, "2 FE aş\n"));
    const std::string streams = "#include <iostream>\n#include <string>\nusing namespace std;\nint main() {\n    string ad;\n    cout << \"Ad\xFD: \";\n    cin >> ad;\n"
                                "    cout << ad.size() << \" \" << ad << \" \xE7ok g\xFCzel\" << endl;\n    return 0;\n}\n";
    CHECK(has(run("akis.cpp", streams, "Işıl\r").first, "4 Işıl çok güzel"));  // C++ streams go the same way
    // With the Turkish locale the C library knows those bytes as letters, and numbers take a comma.
    const auto [locale, locale_end] = run("yerel.c", "#include <stdio.h>\n#include <string.h>\n#include <locale.h>\n#include <ctype.h>\nint main(void) {\n    char ad[] = \"\xE7i\xF0 k\xF6" "fte\";\n"
        "    setlocale(LC_ALL, \"Turkish\");\n    printf(\"%s %d %c %.1f\\n\", strupr(ad), isalpha((unsigned char)'\xFE') != 0, toupper((unsigned char)'\xFE'), 2.5);\n    return 0;\n}\n");
    CHECK(locale_end == 0 && has(locale, "ÇIĞ KÖFTE 1 Ş 2,5"));

    // Mistakes are said at the editor's column, and the log quotes the line with its letters.
    const cedit::CompileResult name = build("ad.c", "int main(void) {\n    int ya\xFE = 1;\n    return ya\xFE;\n}\n", false);
    CHECK(has(name.log, "  ➜ Türkçe harfler yalnızca") && has(name.log, "int yaş = 1;") && !has(name.log, "<FE>"));
    const cedit::CompileResult curly = build("tirnak.c", "#include <stdio.h>\nint main(void) { printf(\x93\xFE\x94); return 0; }\n", false);
    CHECK(has(curly.log, "  ➜ Bu tırnak işareti C’de geçmez") && !has(curly.log, "silinmeli"));
    const cedit::CompileResult wrong = build("kolon.c", "#include <stdio.h>\nint main() {\n    printf(\"\xDE\xFE\xFD\"); int x = y;\n    return 0;\n}\n", false);
    CHECK(wrong.diagnostics.size() == 1);
    if (wrong.diagnostics.size() == 1)  // the editor's bytes, where the three letters take six
        CHECK(std::string_view("    printf(\"Şşı\"); int x = y;").substr(wrong.diagnostics[0].column, 1) == "y");
    // A click on the message reads the log, which counts in letters, and lands on the same "y".
    const std::vector<cedit::Diagnostic> clicked = cedit::parse_diagnostics(wrong.log, "kolon.c");
    CHECK(clicked.size() == 1);
    if (clicked.size() == 1) {
        const std::string_view line = "    printf(\"Şşı\"); int x = y;";
        CHECK(line.substr(cedit::utf8_column(line, clicked[0].column), 1) == "y");
    }
    const std::size_t quote = wrong.log.find("printf(\"Şşı\"); int x = y;");
    const std::size_t caret = wrong.log.find('^');
    CHECK(quote != std::string::npos && caret != std::string::npos);
    if (quote != std::string::npos && caret != std::string::npos)  // the marker under the "y", counted in letters
        CHECK(caret - wrong.log.rfind('\n', caret) == wrong.log.find("y;", quote) - wrong.log.rfind('\n', quote) - 3);
}

// system() takes a line as cmd.exe does: its own commands joined by & and &&, "> nul", and the rest left to macOS.
void command_lines() {
    const auto [shown, end] = run("komut.c", "#include <stdio.h>\n#include <stdlib.h>\nint main(void) {\n    int r;\n    printf(\"silinir\");\n    system(\"color 0A & cls & echo bir\");\n"
        "    system(\"@echo off\");\n    system(\"ECHO gizli>nul\");\n    system(\"echo.\");\n    r = system(\"olmayan_komut > nul && echo yok\");\n    printf(\"<%d>\", r != 0);\n"
        "    r = system(\"echo a && echo b\");\n    printf(\"<%d>\\n\", r);\n    return 0;\n}\n");
    CHECK(end == 0 && !has(shown, "silinir") && !has(shown, "gizli") && !has(shown, "yok") && has(shown, "bir\n\n") && has(shown, "<1>a\nb\n<0>"));
    CHECK(::access((scratch_dir + "/nul").c_str(), F_OK) != 0);
    // The "&" of "2>&1" joins nothing; what is written to a file goes to the file; a long line is not cut.
    ::unlink((scratch_dir + "/yazi.txt").c_str());
    const std::string padding(1500, ' ');
    const auto [quiet, quiet_end] = run("sessiz.c", "#include <stdio.h>\n#include <stdlib.h>\nint main(void) {\n    int r = system(\"chcp 65001 >nul 2>&1\");\n    r += system(\"pause >nul 2>&1\");\n"
        "    r += system(\"echo dosyaya > yazi.txt\");\n    r += system(\"echo" + padding + "uzun\");\n    printf(\"<%d>\\n\", r);\n    return 0;\n}\n", "x");
    CHECK(quiet_end == 0 && has(quiet, "uzun\n<0>") && !has(quiet, "tuşa") && !has(quiet, "command not found") && !has(quiet, "dosyaya"));
    CHECK(cedit::read_file(scratch_dir + "/yazi.txt").value_or("") == "dosyaya\n");
}

// What Windows gives and macOS would not: its rand, memory never set that does not read as 0, and a word under the
// output when an index runs past the end of an array.
void what_a_run_shows() {
    const std::string head = "#include <stdio.h>\n#include <stdlib.h>\nint main(void) {\n";
    CHECK(has(run("rastgele.c", head + "    int a = rand(), b = rand(), c = rand();\n    printf(\"%d %d %d %d\\n\", RAND_MAX, a, b, c);\n    return 0;\n}\n").first, "32767 41 18467 6334"));
    CHECK(has(run("bellek.c", head + "    int *h = malloc(8), *r = malloc(4), *z = calloc(2, 4);\n    r[0] = 7;\n    r = realloc(r, 64);\n"
                                     "    printf(\"%x %x %x %d\\n\", h[0], r[0], r[15], z[1]);\n    return 0;\n}\n").first, "aaaaaaaa 7 aaaaaaaa 0"));
    CHECK(has(run("yeni.cpp", "#include <stdio.h>\nint main() {\n    int *p = new int, *q = new int[2];\n    printf(\"%x %x\\n\", *p, q[1]);\n    return 0;\n}\n").first, "aaaaaaaa aaaaaaaa"));
    const auto [past, past_end] = run("sinir.c", head + "    int a[2] = {1, 2}, i, k, t = 0;\n    for (k = 0; k < 2; k++)\n        for (i = 0; i <= 2; i++) t += a[i];\n"
                                                      "    printf(\"bitti %d\\n\", t != 0);\n    return 0;\n}\n");
    const std::string_view note = "[C Editor] 6. satır: dizinin sınırı aşıldı.";
    CHECK(past_end == 0 && has(past, "bitti") && has(past, note) && past.find(note) == past.rfind(note));  // once for the place
    // The runtime is ready before anything of the program's own runs: a C++ global is set up before main.
    const auto [early, early_end] = run("erken.cpp", "#include <stdio.h>\nint tablo[3];\nint yer = 4;\nint erken = tablo[yer] + printf(\"\xF6nce \");\n"
                                                     "int main() {\n    printf(\"main\\n\");\n    return 0;\n}\n");
    CHECK(early_end == 0 && has(early, "[C Editor] 4. satır: dizinin sınırı aşıldı.") && has(early, "önce main"));
    // Memory that cannot be had is said the C++ way.
    const auto [none, none_end] = run("yok.cpp", "#include <stdio.h>\n#include <new>\nint main() {\n    try {\n        volatile unsigned long cok = ~0ul / 2;\n        char* p = new char[cok];\n"
                                                 "        printf(\"%p\", (void*)p);\n    } catch (const std::bad_alloc&) {\n        printf(\"yer yok\\n\");\n    }\n    return 0;\n}\n");
    CHECK(none_end == 0 && has(none, "yer yok"));
}

std::vector<std::string> entries_of(const std::string& directory) {
    std::vector<std::string> names;
    if (DIR* entries = ::opendir(directory.c_str())) {
        while (const dirent* entry = ::readdir(entries))
            if (entry->d_name[0] != '.') names.emplace_back(entry->d_name);
        ::closedir(entries);
    }
    return names;
}

// The runtime programs link is built by this Mac's compiler the first time, and found again after that.
void runtime_is_built_once() {
    const std::string cache = scratch_dir + "/cache";
    const std::vector<std::string> objects = entries_of(cache);
    CHECK(objects.size() == 1 && objects[0].starts_with("windows_runtime-") && objects[0].ends_with(".o"));
    if (objects.size() != 1) return;
    const auto built = cedit::file_stamp(cache + "/" + objects[0]);
    CHECK(build("yine.c", "int main(void) { return 0; }\n").ok && built == cedit::file_stamp(cache + "/" + objects[0]));
}

// Your own programs keep building: every .c and .cpp in tests/local, which git ignores, and in the folders there
// (links to folders count). They are only read. Without tests/local there is nothing to check.
void local_programs_build() {
    int built = 0;
    const auto build_all = [&](const std::string& directory) {
        for (const std::string& name : entries_of(directory)) {
            if (!name.ends_with(".c") && !name.ends_with(".cpp")) continue;
            const cedit::CompileResult result = compile_file(directory + "/" + name, scratch_dir + "/local");
            if (!result.ok) std::fprintf(stderr, "%s/%s:\n%s", directory.c_str(), name.c_str(), result.log.c_str());
            CHECK(result.ok);
            ++built;
        }
    };
    if (local_programs.empty() || ::access(local_programs.c_str(), F_OK) != 0) return;
    build_all(local_programs);
    for (const std::string& name : entries_of(local_programs)) build_all(local_programs + "/" + name);
    std::printf("tests: %d of your own programs built\n", built);
    CHECK(built > 0);
}

}  // namespace

void program_tests() {
    saving_respects_the_file();
    compiles_and_runs_windows_program();
    backspace_takes_a_whole_letter();
    warns_about_empty_bodies();
    odd_file_names();
    hints_for_common_slips();
    runs_windows_habits();
    compiles_cpp_program();
    plain_words();
    explains_mistakes();
    finds_own_headers();
    windows_code();
    turkish_letters_are_single_chars();
    command_lines();
    what_a_run_shows();
    finds_developer_tools();
    runtime_is_built_once();
    local_programs_build();
}
