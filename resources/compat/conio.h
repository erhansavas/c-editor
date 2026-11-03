/* <conio.h> on the C Editor console. */
#ifndef CEDIT_CONIO_H
#define CEDIT_CONIO_H
#ifdef __cplusplus
extern "C" {
#endif
int getch(void);
int getche(void);
int kbhit(void);
int putch(int);
int ungetch(int);
int cprintf(const char *, ...);
int cputs(const char *);
int cscanf(const char *, ...);
char *cgets(char *);
#ifdef __cplusplus
}
#endif
#define _getch getch
#define _getche getche
#define _kbhit kbhit
#define _putch putch
#endif
