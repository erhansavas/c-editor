// What the editor does without a file system or a compiler: text, encodings, colours, the console screen, messages.
#include <iconv.h>

#include <vector>

#include "core/diagnostics.hpp"
#include "core/editor.hpp"
#include "core/syntax.hpp"
#include "support.hpp"

using namespace std::string_literals;

namespace {

void encoding_matches_system_iconv() {
    const iconv_t converter = iconv_open("UTF-8", "WINDOWS-1254");
    CHECK(converter != reinterpret_cast<iconv_t>(-1));
    int defined = 0;
    for (int value = 0x80; value <= 0xFF; ++value) {
        char in[1] = {static_cast<char>(value)};
        char out[8] = {};
        char* in_pointer = in;
        char* out_pointer = out;
        std::size_t in_left = 1;
        std::size_t out_left = sizeof out;
        const bool ok = iconv(converter, &in_pointer, &in_left, &out_pointer, &out_left) != static_cast<std::size_t>(-1);
        const std::string ours = cedit::decode(std::string_view(in, 1), cedit::Encoding::windows1254);
        if (ok) {
            ++defined;
            CHECK(ours == std::string(out, sizeof out - out_left));
            const auto back = cedit::encode(ours, cedit::Encoding::windows1254);
            CHECK(back && *back == std::string(in, 1));
        } else {
            const auto back = cedit::encode(ours, cedit::Encoding::windows1254);  // kept as the C1 control of that number, as Windows does
            CHECK(ours != "\xEF\xBF\xBD" && back && *back == std::string(in, 1));
        }
    }
    iconv_close(converter);
    CHECK(defined == 121);  // 0x81 0x8D 0x8E 0x8F 0x90 0x9D 0x9E are undefined
}

void file_formats_round_trip() {
    const std::string windows = "printf(\"Ya\xFE\xFDn\xFDz\xFD\");\r\n\xDDkinci \xF0\xFE\r\n";
    const cedit::LoadedText loaded = cedit::load_text(windows);
    CHECK(loaded.format.encoding == cedit::Encoding::windows1254);
    CHECK(loaded.format.crlf);
    CHECK(loaded.text == "printf(\"Yaşınızı\");\nİkinci ğş\n");
    const auto stored = cedit::store_text(loaded.text, loaded.format);
    CHECK(stored && *stored == windows);

    const cedit::LoadedText utf8 = cedit::load_text("\xEF\xBB\xBFint ş;\n");
    CHECK(utf8.format.encoding == cedit::Encoding::utf8 && utf8.format.byte_order_mark && !utf8.format.crlf);
    CHECK(utf8.text == "int ş;\n");
    CHECK(cedit::store_text(utf8.text, utf8.format).value_or("") == "\xEF\xBB\xBFint ş;\n");

    const cedit::LoadedText ascii = cedit::load_text("int a;\n");  // reads the same in both: Windows-1254
    CHECK(ascii.format.encoding == cedit::Encoding::windows1254 && !ascii.format.crlf);
    CHECK(cedit::store_text(ascii.text, ascii.format).value_or("") == "int a;\n");
    CHECK(cedit::load_text("int ş;\n").format.encoding == cedit::Encoding::utf8);
    CHECK(cedit::store_text("printf(\"ş\");\n", cedit::windows_turkish).value_or("") == "printf(\"\xFE\");\r\n");
    // A column counted in one-byte Windows-1254 letters, as a byte column of the UTF-8 line; past the end it stays past.
    CHECK(cedit::utf8_column("Şşı = y;", 3) == 6 && cedit::utf8_column("Şşı = y;", 7) == 10);
    CHECK(cedit::utf8_column("int a;", 4) == 4 && cedit::utf8_column("ğ", 3) == 4);

    const auto unrepresentable = cedit::encode("ab€c→", cedit::Encoding::windows1254);
    CHECK(!unrepresentable && unrepresentable.error() == 6);  // € is in 1254, → is not
}

void editing() {
    cedit::Editor editor("int a;\n    b;");
    CHECK(editor.line_count() == 2 && editor.line(1) == "    b;");
    editor.set_caret(editor.text().size(), false);
    editor.insert_newline();
    CHECK(editor.text() == "int a;\n    b;\n    ");
    for (const char* key : {"ş", "ı", "x"}) editor.insert(key);
    CHECK(editor.text().ends_with("    şıx") && editor.modified());
    CHECK(editor.undo() && editor.text() == "int a;\n    b;\n    ");  // typing undoes as one step
    CHECK(editor.undo() && editor.text() == "int a;\n    b;" && !editor.modified());
    CHECK(editor.redo() && editor.redo() && editor.text().ends_with("şıx"));
    editor.delete_backward();
    CHECK(editor.text().ends_with("şı"));
    editor.move(cedit::Move::word_left, true);
    CHECK(editor.selected_text() == "şı");
    editor.mark_saved();
    editor.insert("y");
    CHECK(editor.text().ends_with("    y") && editor.modified());

    cedit::Editor columns("\tab\nxyşz");
    CHECK(columns.column_of(1) == 4 && columns.offset_at(0, 5) == 2);
    columns.set_caret(columns.offset_at(0, 6), false);  // after "b"
    columns.move(cedit::Move::down, false);
    CHECK(columns.caret() == columns.text().size());  // column 6 clamps to the end of "xyşz"
    columns.move(cedit::Move::up, false);
    CHECK(columns.caret() == 3);  // preferred column restored
    columns.select_word(columns.line_start(1) + 2);
    CHECK(columns.selected_text() == "xyşz");

    cedit::Editor indent("    x");
    indent.set_caret(2, false);
    indent.insert_newline();
    CHECK(indent.text() == "  \n    x");  // only the indentation in front of the caret is repeated

    cedit::Editor block("int main() {");
    block.set_caret(block.text().size(), false);
    block.insert_newline();
    CHECK(block.text() == "int main() {\n    ");  // one level deeper after '{'
    block.insert("if (a) {");
    block.insert_newline();
    block.insert("b;");
    block.insert_newline();
    CHECK(block.text() == "int main() {\n    if (a) {\n        b;\n        ");
    block.insert("}");
    CHECK(block.text() == "int main() {\n    if (a) {\n        b;\n    }");  // lines up with its '{'
    CHECK(block.undo() && block.text() == "int main() {\n    if (a) {\n        b;\n        ");
    block.redo();
    block.insert_newline();
    block.insert("}");
    CHECK(block.text().ends_with("    }\n}"));
    block.insert("}");
    CHECK(block.text().ends_with("    }\n}}"));  // after other text a brace is just typed
    cedit::Editor pair("int main() {}");  // Enter between "{" and "}"
    pair.set_caret(pair.text().size() - 1, false);
    pair.insert_newline();
    CHECK(pair.text() == "int main() {\n    \n}" && pair.caret() == 17);
    cedit::Editor tail("int main() {\n    return 0;}");  // Enter in front of a '}' after other text
    tail.set_caret(tail.text().size() - 1, false);
    tail.insert_newline();
    CHECK(tail.text() == "int main() {\n    return 0;\n}");
    CHECK(tail.undo() && tail.text() == "int main() {\n    return 0;}");
    cedit::Editor nested("    if (a) {  }");
    nested.set_caret(12, false);
    nested.insert_newline();
    CHECK(nested.text() == "    if (a) {\n        \n    }");  // the blanks before '}' go
    cedit::Editor string_pair("    printf(\"{}\");");  // braces in quotes are text: Enter just splits
    string_pair.set_caret(13, false);
    string_pair.insert_newline();
    CHECK(string_pair.text() == "    printf(\"{\n    }\");");
    cedit::Editor stray("int main() {}\n    x;}");  // a '}' without its '{' is left where it is
    stray.set_caret(stray.text().size() - 1, false);
    stray.insert_newline();
    CHECK(stray.text() == "int main() {}\n    x;\n    }");
    cedit::Editor back("int main() {\n        x;\n     ");
    back.set_caret(back.line_start(1) + 8, false);
    back.delete_backward();
    CHECK(back.line(1) == "    x;");  // one level of indentation per Backspace
    back.delete_backward();
    CHECK(back.line(1) == "x;");
    back.delete_backward();
    CHECK(back.line(0) == "int main() {x;");  // at column 0 it joins the lines as before
    back.set_caret(back.text().size(), false);
    back.delete_backward();
    CHECK(back.line(1) == "    ");  // from five spaces back to the level at four
    cedit::Editor gap("int main() {\n    x;\n    \n}");  // Backspace in front of '}' removes the blank line
    gap.set_caret(gap.line_start(3), false);
    gap.delete_backward();
    CHECK(gap.text() == "int main() {\n    x;\n}");  // its spaces go too, so '}' stays in line
    CHECK(gap.undo() && gap.text() == "int main() {\n    x;\n    \n}" && gap.caret() == gap.line_start(3));
    gap.set_caret(gap.line_end(2), false);
    gap.delete_forward();  // the same with Delete at the end of the blank line
    CHECK(gap.text() == "int main() {\n    x;\n}");
    gap.set_caret(gap.line_start(2), false);
    gap.delete_to_line_start();  // and with ⌘⌫ at the start of a line
    CHECK(gap.text() == "int main() {\n    x;}");
    cedit::Editor inside("    x = 1;  ");
    inside.set_caret(inside.text().size(), false);
    inside.delete_backward();
    CHECK(inside.text() == "    x = 1; ");  // after text, one space at a time
    cedit::Editor tabbed("\t\tx");
    tabbed.set_caret(2, false);
    tabbed.delete_backward();
    CHECK(tabbed.text() == "\tx");  // a tab is a level of its own
    cedit::Editor odd("    if (a) {\n     b;\n     ");  // a five-space indentation
    odd.set_caret(odd.text().size(), false);
    odd.insert("}");
    CHECK(odd.text() == "    if (a) {\n     b;\n    }");
    cedit::Editor lone("     ");
    lone.set_caret(5, false);
    lone.insert("}");
    CHECK(lone.text() == "     }");  // no opening brace: typed as is

    cedit::Editor braceless("    if (sayi % 2 == 0)");
    braceless.set_caret(braceless.text().size(), false);
    braceless.insert_newline();
    braceless.insert("a();");
    braceless.insert_newline();
    braceless.insert("else");
    braceless.insert_newline();
    braceless.insert("b();");
    braceless.insert_newline();
    braceless.insert("return 0;");
    CHECK(braceless.text() == "    if (sayi % 2 == 0)\n        a();\n    else\n        b();\n    return 0;");
    cedit::Editor allman("if (f(a))");
    allman.set_caret(allman.text().size(), false);
    allman.insert_newline();
    allman.insert("{");
    allman.insert_newline();
    allman.insert("x;");
    allman.insert_newline();
    allman.insert("}");
    CHECK(allman.text() == "if (f(a))\n{\n    x;\n}");  // the brace lines up with its "if"
    CHECK(allman.undo() && allman.undo() && allman.text() == "if (f(a))\n{\n    x;");
    cedit::Editor closed("    if (a) {\n        b;\n    } else");
    closed.set_caret(closed.text().size(), false);
    closed.insert_newline();
    closed.insert("c;");
    closed.insert_newline();
    CHECK(closed.text() == "    if (a) {\n        b;\n    } else\n        c;\n    ");
    cedit::Editor narrow("    if (a)\n     b;");  // a one-space statement indentation
    narrow.set_caret(narrow.text().size(), false);
    narrow.insert_newline();
    CHECK(narrow.text() == "    if (a)\n     b;\n    ");
    cedit::Editor plain("    if (a) b();\n    for (i = 0; i < 3; i++)");
    plain.set_caret(plain.line_end(0), false);
    plain.insert_newline();
    CHECK(plain.line(1) == "    ");  // a statement on the same line opens nothing
    plain.set_caret(plain.text().size(), false);
    plain.insert_newline();
    CHECK(plain.text().ends_with("i++)\n        "));

    const cedit::Editor guides("int main() {\n    if (a) {\n        b;\n\n        c;\n    }\n\n    if (d)\n     e;\n\treturn 0;\n}");
    const auto lines = guides.indent_guides();
    using Columns = std::vector<std::size_t>;
    CHECK(lines.size() == 11);
    CHECK(lines[0].empty() && lines[1].empty());  // column 0 is the gutter's edge
    CHECK(lines[2] == Columns{4} && lines[3] == Columns{4} && lines[4] == Columns{4});
    CHECK(lines[5].empty() && lines[6].empty());  // the block is closed above the blank line
    CHECK(lines[8] == Columns{4} && lines[9].empty() && lines[10].empty());  // a tab is 4 columns
    const auto uneven = cedit::Editor("    int i;\n     for (;;) { // loop\n      x;\n     }").indent_guides();
    CHECK(uneven[1].empty() && uneven[2] == Columns{5} && uneven[3].empty());  // only block lines open guides
    cedit::Editor quoted("if (c == ')')\n    a;\nwhile (c != '(')");  // parentheses in quotes do not count
    quoted.set_caret(quoted.line_end(0), false);
    quoted.insert_newline();
    CHECK(quoted.line(1) == "    ");
    quoted.set_caret(quoted.text().size(), false);
    quoted.insert_newline();
    CHECK(quoted.text().ends_with("'(')\n    "));
    cedit::Editor literal("int main() {\n    printf(\"{\"); // {\n    ");  // nor braces in quotes or comments
    literal.set_caret(literal.text().size(), false);
    literal.insert("}");
    CHECK(literal.text().ends_with("(\"{\"); // {\n}"));
    cedit::Editor commented("    if (a) // yorum");
    commented.set_caret(commented.text().size(), false);
    commented.insert_newline();
    CHECK(commented.text() == "    if (a) // yorum\n        ");

    cedit::Editor saved;
    saved.insert("a");
    saved.mark_saved();
    saved.insert("b");
    CHECK(saved.undo() && saved.text() == "a" && !saved.modified());  // saving ends the typing step
}

void highlighting() {
    std::vector<cedit::Span> spans;
    cedit::LineState state = cedit::highlight_c("# include <stdio.h>", {}, spans);
    CHECK(spans.size() == 2 && spans[0].token == cedit::Token::preprocessor && spans[1].token == cedit::Token::string);
    state = cedit::highlight_c("int a = 10; /* start", state, spans);
    CHECK(state.in_comment && spans.size() == 3 && spans[0].token == cedit::Token::type &&
          spans[1].token == cedit::Token::number && spans[2].token == cedit::Token::comment);
    state = cedit::highlight_c("end */ return \"x\\\"y\";", state, spans);
    CHECK(!state.in_comment && spans.size() == 5 && spans[0].end == 6 && spans[1].token == cedit::Token::keyword &&
          spans[2].token == cedit::Token::string && spans[3].token == cedit::Token::escape && spans[3].begin == 16 &&
          spans[3].end == 18 && spans[4].token == cedit::Token::string && spans[4].end == 20);
    cedit::highlight_c("uint16_t x;", {}, spans);
    CHECK(spans.size() == 1 && spans[0].token == cedit::Token::type);

    // The escape-sequence lesson: a // comment ending in a backslash takes in the next line, as in C.
    state = cedit::highlight_c("    printf(\"C:\\\\Program Files\\\\\"); // C:\\Program Files\\", {}, spans);
    CHECK(state.in_line_comment && spans.back().token == cedit::Token::comment);
    state = cedit::highlight_c("    x = 1; \\ ", state, spans);
    CHECK(state.in_line_comment && spans.size() == 1 && spans[0].token == cedit::Token::comment && spans[0].end == 13);
    state = cedit::highlight_c("    return 0;", state, spans);
    CHECK(!state.in_line_comment && spans.size() == 1 && spans[0].token == cedit::Token::comment);
    state = cedit::highlight_c("    return 0;", state, spans);
    CHECK(spans.size() == 2 && spans[0].token == cedit::Token::keyword);
    state = cedit::highlight_c("    printf(\"\\\\\");", {}, spans);
    CHECK(!state.in_line_comment);  // a backslash inside a string is not a comment's

    // Each escape is one span: two characters, up to three octal digits, or \x and its hex digits.
    cedit::highlight_c("\"a\\n\\101\\x1Fz\\ş\" '\\0'", {}, spans);
    using T = cedit::Token;
    const std::vector<std::pair<std::size_t, T>> pieces = {{2, T::string},  {4, T::escape},     {8, T::escape},
                                                           {12, T::escape}, {13, T::string},    {16, T::escape},
                                                           {17, T::string}, {19, T::character}, {21, T::escape},
                                                           {22, T::character}};
    CHECK(spans.size() == pieces.size());
    for (std::size_t k = 0; k < std::min(spans.size(), pieces.size()); ++k)
        CHECK(spans[k].end == pieces[k].first && spans[k].token == pieces[k].second);
}

void terminal() {
    cedit::Terminal bell;
    bell.feed("Uyari sesi: \a\a");
    CHECK(bell.bells() == 2 && utf8_of(bell.lines()[0]) == "Uyari sesi: ");  // beeps, prints nothing

    cedit::Terminal screen;
    screen.feed("Birinci say\xC4");  // split code point
    screen.feed("\xB1y\xC4\xB1 girin: 2\r\n\x1B[1mx\x1B[0m\b\bab\rZ");
    CHECK(screen.lines().size() == 2);
    CHECK(utf8_of(screen.lines()[0]) == "Birinci sayıyı girin: 2");
    CHECK(utf8_of(screen.lines()[1]) == "Zb" && screen.cursor_column() == 1);
    screen.feed("\x1B[2J\x1B[Hcleared");
    CHECK(screen.lines().size() == 1 && utf8_of(screen.lines()[0]) == "cleared");

    constexpr std::size_t columns = cedit::Terminal::columns;
    cedit::Terminal wrapped;  // a full line and its newline leave no blank line; a longer one wraps
    wrapped.feed(std::string(columns, 'a') + "\n" + std::string(columns + 5, 'b'));
    CHECK(wrapped.lines().size() == 3 && wrapped.lines()[1].size() == columns && wrapped.lines()[2] == U"bbbbb");

    cedit::Terminal clamped;  // cursor requests stay on the screen and within the scrollback
    clamped.feed("\x1B[1;4000000000Hx\x1B[99999999999C\ty");
    CHECK(clamped.lines().size() == 1 && clamped.lines()[0].size() == columns && clamped.lines()[0].back() == U'y');
    clamped.feed("\x1B[999999999999B");
    CHECK(clamped.lines().size() == cedit::Terminal::max_lines);

    cedit::Terminal endless;  // a program printing forever keeps only the last max_lines lines
    for (int i = 0; i < 12'000; ++i) endless.feed("Merhaba " + std::to_string(i) + "\n");
    CHECK(endless.lines().size() == cedit::Terminal::max_lines && utf8_of(endless.lines().front()) == "Merhaba 2001");
}

void diagnostics() {
    const auto parsed = cedit::parse_diagnostics(
        "/x/a.c:3:5: warning: unused variable 'b' [-Wunused-variable]\n"
        "/x/a.c:9:1: error: expected ';' after expression\n"
        "/x/a.c:9:1: note: something\n/other.h:1:1: error: elsewhere\n",
        "/x/a.c");
    CHECK(parsed.size() == 2 && !parsed[0].error && parsed[0].line == 2 && parsed[0].column == 4);
    CHECK(parsed[1].error && parsed[1].message == "expected ';' after expression");

    const auto fixes = cedit::parse_diagnostics(
        "/x/a.c:4:18: error: use of undeclared identifier 'topalm'; did you mean 'toplam'?\n"
        "    4 |     printf(\"%d\", topalm)\n"
        "fix-it:\"/x/a.c\":{5:18-5:24}:\"toplam\"\n"
        "/x/a.c:3:9: note: 'toplam' declared here\n"
        "/x/a.c:6:11: warning: using the result of an assignment as a condition without parentheses\n"
        "/x/a.c:6:11: note: use '==' to turn this assignment into an equality comparison\n"
        "fix-it:\"/x/a.c\":{7:11-7:12}:\"==\"\n"
        "/x/a.c:8:3: warning: escaped\n"
        "fix-it:\"/x/a.c\":{9:3-9:3}:\" = '\\\\0' \\303\\274\"\n"
        "/x/a.c:10:1: error: two edits\n"
        "fix-it:\"/x/a.c\":{11:1-11:1}:\"(\"\nfix-it:\"/x/a.c\":{11:5-11:5}:\")\"\n"
        "/x/a.c:12:1: error: across lines\n"
        "fix-it:\"/x/a.c\":{13:1-14:2}:\"\"\n",
        "/x/a.c");
    CHECK(fixes.size() == 5);
    CHECK(fixes[0].fix && fixes[0].fix->line == 4 && fixes[0].fix->column == 17 && fixes[0].fix->end_column == 23 &&
          fixes[0].fix->text == "toplam");
    CHECK(!fixes[1].fix);  // a note's suggestion is not a sure fix
    CHECK(fixes[2].fix && fixes[2].fix->text == " = '\\0' ü");
    CHECK(!fixes[3].fix && !fixes[4].fix);  // several edits, or one across lines

    CHECK(cedit::explain("expected ';' after expression", "") == "Noktalı virgül (;) eksik.");
    CHECK(cedit::explain("use of undeclared identifier 'topalm'; did you mean 'toplam'?", "") ==
          "“topalm” tanımlı değil.");
    CHECK(cedit::explain("format specifies type 'int *' but the argument has type 'int'", "    scanf(\"%d\", a);") ==
          "scanf’te değişkenin önüne & koymayı unutmuşsunuz (örneğin &x).");
    CHECK(cedit::explain("format specifies type 'char *' but the argument has type 'int'", "    printf(\"%s\", c);")
              .starts_with("Biçim belirteci"));
    CHECK(cedit::explain("call to undeclared library function 'sqrt' with type 'double (double)'; ISO C99 and later "
                         "do not support implicit function declarations",
                         "") == "“sqrt” için #include <math.h> satırı eksik.");
    // The same slip in the words of the compilers before clang 15, which an older Mac may have.
    CHECK(cedit::explain("implicitly declaring library function 'sqrt' with type 'double (double)'", "") == "“sqrt” için #include <math.h> satırı eksik.");
    CHECK(cedit::explain("implicit declaration of function 'getch' is invalid in C99", "") == "“getch” için #include <conio.h> satırı eksik.");
    CHECK(cedit::explain("unsequenced modification and access to 'i'", "").starts_with("“i” aynı ifadede hem değişiyor hem okunuyor"));
    CHECK(cedit::explain("C++ requires a type specifier for all declarations", "").starts_with("Tür yazılmamış"));
    // In a Windows-1254 file the compiler only says that a byte is not code: the line says which character it was.
    CHECK(cedit::explain("source file is not valid UTF-8", "    int yaş = 1;", 10).starts_with("Türkçe harfler yalnızca"));
    CHECK(cedit::explain("source file is not valid UTF-8", "    printf(“a”);", 11).starts_with("Bu tırnak işareti"));
    CHECK(cedit::explain("variable 'x' is uninitialized when used here", "") ==
          "“x” değişkenine değer verilmeden kullanılıyor; önce bir değer atayın.");
    // Messages as clang on this machine words them for the classic slips.
    CHECK(cedit::explain("result of comparison against a string literal is unspecified (use an explicit string "
                       "comparison function instead)",
                       "")
              .starts_with("Metinler == ile"));
    CHECK(cedit::explain("array type 'char[20]' is not assignable", "").starts_with("Bir diziye = ile metin"));
    CHECK(cedit::explain("array type 'int[5]' is not assignable", "").starts_with("Bir dizi = ile toptan"));
    CHECK(cedit::explain("for loop has empty body", "").starts_with("Döngü satırının"));
    CHECK(cedit::explain("more '%' conversions than data arguments", "").starts_with("Biçim metninde"));
    CHECK(cedit::explain("void function 'g' should not return a value", "").starts_with("void bir fonksiyon"));
    CHECK(cedit::explain("a message no explanation knows", "").empty());
    CHECK(cedit::explain_link_error("Undefined symbols for architecture arm64:\n  \"_main\", referenced from:\n")
              .starts_with("Programda main() fonksiyonu yok"));
    CHECK(cedit::explain_link_error("  \"_getch\", referenced from:\n") == "“getch” için #include <conio.h> satırı eksik.");
}

// Comments are not code: braces and control lines in them do not count, and a comment after a
// control line does not hide it. /* ... */ runs over lines and apostrophes in it are not quotes.
void comments_do_not_indent() {
    const auto after_enter = [](const std::string& text) {
        cedit::Editor editor(text);
        editor.set_caret(text.size(), false);
        editor.insert_newline();
        return editor.text().substr(text.size());
    };
    CHECK(after_enter("/* yorum {") == "\n");
    CHECK(after_enter("/*\n    if (a)") == "\n    ");  // inside a comment the indentation only stays
    CHECK(after_enter("    if (a) /* yorum */") == "\n        ");
    CHECK(after_enter("int main() { /* yorum */") == "\n    ");
    CHECK(after_enter("/* don't */ int main() {") == "\n    ");
    CHECK(after_enter("int a; /* { */") == "\n");
    const auto after_brace = [](const std::string& text, std::string_view brace) {
        cedit::Editor editor(text);
        editor.set_caret(text.size(), false);
        editor.insert(brace);
        return editor.text().substr(text.rfind('\n') + 1);
    };
    CHECK(after_brace("int main() {\n    /* { */\n    ", "}") == "}");
    CHECK(after_brace("int main() {\n    /*\n    {\n    */\n    ", "}") == "}");
    CHECK(after_brace("int main() {\n    if (a) {\n        /* } */\n        ", "}") == "    }");
    CHECK(after_brace("int main() {\n    /* don't */\n    ", "}") == "}");
    CHECK(after_brace("int main() {\n    /*\n        ", "}") == "        }");  // text in a comment stays where it is typed
    CHECK(after_brace("if (a) /* yorum */\n    ", "{") == "{");
    cedit::Editor header("    if (a)\n        // yorum\n        b;");  // a comment line between does not end the statement
    header.set_caret(header.text().size(), false);
    header.insert_newline();
    CHECK(header.text().ends_with("b;\n    "));
    cedit::Editor before("int main() {\n    /* } */");  // Enter in front of a '}' in a comment only splits
    before.set_caret(before.line_start(1) + 7, false);
    before.insert_newline();
    CHECK(before.text() == "int main() {\n    /* \n    } */");
    const auto guides = cedit::Editor("/*\n  if (a)\n    x\n*/\nif (b) /* y */\n    c;").indent_guides();
    CHECK(guides[2].empty() && guides[5].empty());  // column 0 is left out; nothing opens inside the comment
    CHECK(cedit::Editor("/*\n    if (a)\n        x\n*/").indent_guides()[2].empty());
}

void redo_returns_the_caret() {
    cedit::Editor pair("int main() {}");
    pair.set_caret(12, false);
    pair.insert_newline();
    const std::size_t caret = pair.caret();
    pair.undo();
    pair.redo();
    CHECK(pair.caret() == caret && pair.caret() == pair.line_end(1));
    cedit::Editor history("a");
    CHECK(!history.can_undo() && !history.can_redo());
    history.insert("ş");
    CHECK(history.can_undo() && !history.can_redo());
    history.undo();
    CHECK(!history.can_undo() && history.can_redo());
}

// What was read is what is written, whatever the bytes are.
void every_byte_round_trips() {
    const auto exact = [](const std::string& bytes) {
        const cedit::LoadedText loaded = cedit::load_text(bytes);
        const auto stored = cedit::store_text(loaded.text, loaded.format);
        return stored && *stored == bytes;
    };
    std::string all;
    for (int value = 0; value < 256; ++value) all.push_back(static_cast<char>(value));
    CHECK(exact(all));
    CHECK(exact("a\x81\x8D\x8E\x8F\x90\x9D\x9E\n"));          // the bytes Windows-1254 leaves undefined
    CHECK(exact("\xEF\xBB\xBF\xC5\x9F \xFF\n"));              // a BOM over invalid UTF-8
    CHECK(exact("\xFF\xFEi\0n\0"s) && exact("a\0b\r\n"s));    // UTF-16 and NUL bytes
    CHECK(exact("\xEF\xBB\xBFint \xC5\x9F;\r\n") && exact("a\r\nb\r\n") && exact("a\nb\n") && exact("a\rb\n") && exact(""));
    CHECK(cedit::load_text("\xEF\xBB\xBF\xC5\x9F \xFF\n").format.encoding == cedit::Encoding::windows1254);
    // Mixed line ends cannot round trip: the editor says so, and the larger part decides.
    const cedit::LoadedText mixed = cedit::load_text("a\nb\nc\r\n");
    CHECK(mixed.mixed_line_ends && !mixed.format.crlf && mixed.text == "a\nb\nc\n");
    const cedit::LoadedText mostly_crlf = cedit::load_text("a\r\nb\r\nc\n");
    CHECK(mostly_crlf.mixed_line_ends && mostly_crlf.format.crlf);
    CHECK(!cedit::load_text("a\r\nb\r\n").mixed_line_ends && !cedit::load_text("a\nb\n").mixed_line_ends);
}

void terminal_controls() {
    const auto show = [](std::string_view bytes) {
        cedit::Terminal terminal;
        terminal.feed(bytes);
        std::string out;
        for (const auto& line : terminal.lines()) out += utf8_of(line) + "|";
        return out;
    };
    CHECK(show("abc\x1B[2K") == "|");
    CHECK(show("abcd\x1B[2D\x1B[1K") == "   d|");
    CHECK(show("abcd\x1B[2D\x1B[K") == "ab|");
    CHECK(show("ab\ncd\x1B[A\x1B[J") == "ab|");
    CHECK(show("ab\ncd\x1B[1J") == "|  |");
    CHECK(show("a\x1B]0;title\ab") == "ab|");            // a window title is not text, nor its end a bell
    CHECK(show("a\x1B]0;title\x1B\\b") == "ab|");
    CHECK(show("a\x1B(Bb") == "ab|");                    // a character-set choice
    cedit::Terminal title;
    title.feed("\x1B]0;title\a");
    CHECK(title.bells() == 0);

    cedit::Terminal screen;  // positions count from the top of the 25 rows on screen, not of the scrollback
    for (int i = 0; i < 40; ++i) screen.feed(std::to_string(i) + "\n");
    screen.feed("\x1B[1;1HX");
    CHECK(screen.lines().size() == 41 && screen.cursor_line() == 41 - cedit::Terminal::rows);
    CHECK(utf8_of(screen.lines()[screen.cursor_line()]).starts_with("X"));
    screen.feed("\x1B[999;1HY");
    CHECK(screen.cursor_line() == 40 && screen.lines().size() == 41);

    cedit::Terminal endless;  // an endless escape sequence takes no endless memory, and ends like any other
    endless.feed("\x1B[");
    for (int i = 0; i < 1000; ++i) endless.feed(std::string(1000, '1'));
    endless.feed("mok");
    CHECK(utf8_of(endless.lines()[0]) == "ok");
}

}  // namespace

void core_tests() {
    encoding_matches_system_iconv();
    file_formats_round_trip();
    editing();
    highlighting();
    terminal();
    diagnostics();
    comments_do_not_indent();
    redo_returns_the_caret();
    every_byte_round_trips();
    terminal_controls();
}
