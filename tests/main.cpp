// `./build.sh test` builds the tests with sanitizers and runs them. The same binary checks an installed app when
// its compat folder is named instead: tests <compat-dir> <scratch-dir> [<folder of your own programs>].
#include <sys/stat.h>

#include "support.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: tests <compat-dir> <scratch-dir> [<own-programs-dir>]\n");
        return 2;
    }
    compat_dir = argv[1];
    scratch_dir = std::string(argv[2]) + "/tests";
    if (argc > 3) local_programs = argv[3];
    ::mkdir(scratch_dir.c_str(), 0700);
    core_tests();
    program_tests();
    std::printf(failures ? "tests: %d FAILED\n" : "tests: all passed\n", failures);
    return failures ? 1 : 0;
}
