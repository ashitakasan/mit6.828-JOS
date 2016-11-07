// 为用户环境的 cprintf输出的实现，基于 printfmt() 和 sys_cputs() 系统调用

// cprintf是一个调试语句，而不是通用的输出语句
// 非常重要的是它总是去控制台，特别是当调试文件描述符代码时

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/lib.h>

/*
  收集最多256个字符到一个缓冲区，并执行一个系统调用打印所有的，
  以使行输出到控制台原子性和防止中断导致控制台输出线中间的上下文切换等。
 */
struct printbuf {
	int idx;			// 当前缓冲索引
	int cnt;			// 目前已经打印比特数
	char buf[256];
};

static void putch(int ch, struct printbuf *b){
	b->buf[b->idx++] = ch;
	if(b->idx == 256-1){
		sys_cputs(b->buf, b->idx);
		b->idx = 0;
	}
	b->cnt++;
}

int vcprintf(const char *fmt, va_list ap){
	struct printbuf b;

	b.idx = 0;
	b.cnt = 0;
	vprintfmt((void *)putch, &b, fmt, ap);
	sys_cputs(b.buf, b.idx);

	return b.cnt;
}

int cprintf(const char *fmt, ...){
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}
