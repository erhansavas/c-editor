/* <direct.h> on Windows: chdir, getcwd, rmdir, and mkdir without a mode. */
#ifndef CEDIT_DIRECT_H
#define CEDIT_DIRECT_H
#include <sys/stat.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C" {
#endif
int cedit_mkdir(const char *);
#ifdef __cplusplus
}
#endif
#define mkdir(path) cedit_mkdir(path)
#endif
