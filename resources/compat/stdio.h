/* The system's <stdio.h>, and for a program that asks for it what the Windows one adds. */
#include_next <stdio.h>
#if !defined(CEDIT_READING_BASICS) && !defined(CEDIT_STDIO_H)
#define CEDIT_STDIO_H
#ifdef __cplusplus
extern "C" {
#endif
int fputchar(int);
int fgetchar(void);
#ifdef __cplusplus
}
#endif
#endif
