/* The subset of <windows.h> used by console programs. The macOS console is UTF-8, so code-page calls succeed as no-ops. Types have their Windows sizes. */
#ifndef CEDIT_WINDOWS_H
#define CEDIT_WINDOWS_H
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef short SHORT;
typedef void *HANDLE;
typedef struct { SHORT X; SHORT Y; } COORD;
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define FOREGROUND_BLUE 0x0001
#define FOREGROUND_GREEN 0x0002
#define FOREGROUND_RED 0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE 0x0010
#define BACKGROUND_GREEN 0x0020
#define BACKGROUND_RED 0x0040
#define BACKGROUND_INTENSITY 0x0080
#ifdef __cplusplus
extern "C" {
#endif
BOOL SetConsoleOutputCP(UINT);
BOOL SetConsoleCP(UINT);
void Sleep(DWORD);
BOOL Beep(DWORD, DWORD);
HANDLE GetStdHandle(DWORD);
BOOL SetConsoleTextAttribute(HANDLE, WORD);
BOOL cedit_set_cursor(HANDLE, SHORT, SHORT);
#ifdef __cplusplus
}
#endif
#define SetConsoleCursorPosition(handle, position) cedit_set_cursor((handle), (position).X, (position).Y)
#endif
