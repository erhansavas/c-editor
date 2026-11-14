/* Force-included into every program C Editor compiles, so a forgotten #include of these only warns, as GCC on Windows
   does. Names the system adds and Windows has not are hidden: a beginner's own mergesort, getline or global index must not clash. */
#ifndef CEDIT_COMPAT_H
#define CEDIT_COMPAT_H
#define CEDIT_READING_BASICS
#define getline cedit_hidden_getline
#define getdelim cedit_hidden_getdelim
#define index cedit_hidden_index
#define rindex cedit_hidden_rindex
#define heapsort cedit_hidden_heapsort
#define mergesort cedit_hidden_mergesort
#define radixsort cedit_hidden_radixsort
#define random cedit_hidden_random
#define srandom cedit_hidden_srandom
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef getline
#undef getdelim
#undef index
#undef rindex
#undef heapsort
#undef mergesort
#undef radixsort
#undef random
#undef srandom
#undef CEDIT_READING_BASICS
#ifdef CEDIT_WINDOWS_1254 /* compiled from a Windows-1254 file: the runtime turns its text into the console's UTF-8 */
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((visibility("hidden"))) int cedit_windows_1254 = 1;
#ifdef __cplusplus
}
#endif
#endif
#endif
