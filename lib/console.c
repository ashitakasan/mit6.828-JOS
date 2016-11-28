#include <inc/string.h>
#include <inc/lib.h>

void cputchar(int ch){
	char c = ch;
	// 与标准Unix的putchar不同，cputchar函数 总是 输出到系统控制台
	sys_cputs(&c, 1);
}

int getchar(void){
	unsigned char c;
	int r;

	// 但是，JOS支持标准的 _input_ 重定向，允许用户将脚本文件重定向到 shell 等
	// getchar() 从文件描述符 0 读取一个字符
	r = read(0, &c, 1);
	if(r < 0)
		return r;
	if(r < 1)
		return -E_EOF;
	return c;
}

// 真实 控制台文件描述符实现
// 上面的 putchar/getchar 函数在默认情况下仍然会出现在这里，但现在可以通过 fd 层重定向到文件，管道等

static ssize_t devcons_read(struct Fd *, void *, size_t);
static ssize_t devcons_write(struct Fd *, const void *, size_t);
static int devcons_close(struct Fd *);
static int devcons_stat(struct Fd *, struct Stat *);

struct Dev devcons = {
	.dev_id = 'c',
	.dev_name = "cons",
	.dev_read = devcons_read,
	.dev_write = devcons_write,
	.dev_close = devcons_close,
	.dev_stat = devcons_stat
};

int iscons(int fdnum){
	int r;
	struct Fd *fd;

	if((r = fd_lookup(fdnum, &fd)) < 0)
		return r;
	return fd->fd_dev_id == devcons.dev_id;
}

int opencons(void){
	int r;
	struct Fd *fd;

	if((r = fd_alloc(&fd)) < 0)
		return r;
	if((r = sys_page_alloc(0, fd, PTE_P | PTE_U | PTE_W | PTE_SHARE)) < 0)
		return r;

	fd->fd_dev_id = devcons.dev_id;
	fd->fd_omode = O_RDWR;
	return fd2num(fd);
}

static ssize_t devcons_read(struct Fd *fd, void *vbuf, size_t n){
	int c;
	if(n == 0)
		return 0;

	while((c = sys_cgetc()) == 0)
		sys_yield();
	if(c < 0)
		return c;
	if(c == 0x04)					// ctrl-d is EOF
		return 0;
	*(char *)vbuf = c;
	return 1;
}

static ssize_t devcons_write(struct Fd *fd, const void *vbuf, size_t n){
	int tot, m;
	char buf[128];

	// 错误：必须 nul 终止 arg 到 sys_cputs，所以我们必须将vbuf复制到 buf 中的块和 nul 终止
	for(tot = 0; tot < n; tot += m){
		m = n - tot;
		if(m > sizeof(buf) - 1)
			m = sizeof(buf) - 1;
		memmove(buf, (char *)vbuf + tot, m);
		sys_cputs(buf, m);
	}
	return tot;
}

static int devcons_close(struct Fd *fd){
	USED(fd);
	return 0;
}

static int devcons_stat(struct Fd *fd, struct Stat *stat){
	strcpy(stat->st_name, "<cons>");
	return 0;
}
