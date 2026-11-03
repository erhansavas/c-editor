/* The system's <stdlib.h>, and for a program that asks for it what the Windows one adds. */
#include_next <stdlib.h>
#if !defined(CEDIT_READING_BASICS) && !defined(CEDIT_STDLIB_H)
#define CEDIT_STDLIB_H
#ifdef __cplusplus
extern "C" {
#endif
char *itoa(int, char *, int);
char *ltoa(long, char *, int);
char *ultoa(unsigned long, char *, int);
char *lltoa(long long, char *, int);
char *ulltoa(unsigned long long, char *, int);
#ifdef __cplusplus
}
#endif
#endif
