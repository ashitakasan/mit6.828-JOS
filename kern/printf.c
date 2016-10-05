// kernel的console输出，cprintf的简单实现；
// 依赖于 printfmt() 和 kernel的 console的 cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>

// 输出一个字符，cnt值 自增1
static void putch(int ch, int *cnt){
	cputchar(ch);
	*cnt++;
}

// 输出一个字符串，返回输出字符数目
int vcprintf(const char *fmt, va_list ap){
	int cnt = 0;
	
	vprintfmt((void*)putch, &cnt, fmt, ap);
	return cnt;
}

// 输出字符串，返回输出字符数目
int cprintf(const char *fmt, ...){
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}
