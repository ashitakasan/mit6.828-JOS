#ifndef JOS_INC_STDIO_H
#define JOS_INC_STDIO_H

#include <inc/stdarg.h>

#ifndef NULL
#define NULL	((void *) 0)
#endif

// 在 lib/stdio.c 中实现
void cputchar(int c);
int getchar(void);
int iscons(int fd);

// 在 lib/printfmt.c 中实现
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);
int snprintf(char *str, int size, const char *fmt, ...);
int vsnprintf(char *str, int size, const char *fmt, va_list);

// 在lib/printf.c 中实现
int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list);

// 在lib/fprintf.c 中实现
int printf(const char *fmt, ...);
int fprintf(int fd, const char *fmt, ...);
int vfprintf(int fd, const char *fmt, va_list);

// 在lib/readline.c 中实现
char* readline(const char *prompt);

#endif
/* !JOS_INC_STDIO_H */
