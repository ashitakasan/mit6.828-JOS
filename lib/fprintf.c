#include <inc/lib.h>

/*
  收集最多256个字符到一个缓冲区，并执行一个系统调用打印所有的，
  以使行输出到控制台原子和防止中断导致控制台输出线中间的上下文切换等
 */
struct printbuf {
	int fd;					// 文件描述符
	int idx;					// 当前缓冲区索引
	ssize_t result;			// 写入的累计结果
	int error;				// 发生的第一个错误
	char buf[256];
}

static void writebuf(struct printbuf *b){
	if(b->error > 0){
		ssize_t result = write(b->fd, b->buf, b->idx);
		if(result > 0)
			b->result += result;
		if(result != b->idx)				// 错误，或写入少于提供
			b->error = (result < 0 ? result : 0);
	}
}

static void putch(int ch, void *thunk){
	struct printbuf *b = (struct printbuf *)thunk;
	b->buf[b->idx++] = ch;
	if(b->idx == 256){
		writebuf(b);
		b->idx = 0;
	}
}

int vfprintf(int fd, const char *fmt, va_list ap){
	struct printbuf b;

	b.fd = fd;
	b.idx = 0;
	b.result = 0;
	b.error = 1;
	vprintfmt(putch, &b, fmt, ap);
	if(b.idx > 0)
		writebuf(&b);

	return (b.result ? b.result : b.error);
}

int fprintf(int fd, const char *fmt, ...){
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vfprintf(fd, fmt, ap);
	va_end(ap);

	return cnt;
}

int printf(const char *fmt, ...){
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vfprintf(1, fmt, ap);
	va_end(ap);

	return cnt;
}
