/* The system's <string.h>, and for a program that asks for it what the Windows one adds. */
#include_next <string.h>
#if !defined(CEDIT_READING_BASICS) && !defined(CEDIT_STRING_H)
#define CEDIT_STRING_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
char *strrev(char *);
char *strupr(char *);
char *strlwr(char *);
char *strset(char *, int);
char *strnset(char *, int, size_t);
int stricmp(const char *, const char *);
int strcmpi(const char *, const char *);
int strnicmp(const char *, const char *, size_t);
int memicmp(const void *, const void *, size_t);
#ifdef __cplusplus
}
#endif
#endif
