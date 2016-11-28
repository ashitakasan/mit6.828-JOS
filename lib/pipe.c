#include <inc/lib.h>

#define debug		0

static ssize_t devpipe_read(struct Fd *fd, void *buf, size_t n);
static ssize_t devpipe_write(struct Fd *fd, const void *buf, size_t n);
static int devpipe_stat(struct Fd *fd, struct Stat *stat);
static int devpipe_close(struct Fd *fd);

struct Dev devpipe = {
	.dev_id = 'p',
	.dev_name = "pipe",
	.dev_read = devpipe_read,
	.dev_write = devpipe_write,
	.dev_close = devpipe_close,
	.dev_stat = devpipe_stat,
};

#define PIPEBUFSIZ	32

struct Pipe {
	off_t p_rpos;				// 读取位置
	off_t p_wpos;				// 写入位置
	uint8_t p_buf[PIPEBUFSIZ];	// 数据缓冲
};

int pipe(int pfd[2]){
	int r;
	struct Fd *fd0, *fd1;
	void *va;

	// 分配文件描述符表条目
	if((r = fd_alloc(&fd0)) < 0 || (r = sys_page_alloc(0, fd0, PTE_P | PTE_W | PTE_U | PTE_SHARE)) < 0)
		goto err;

	if((r = fd_alloc(&fd1)) < 0 || (r = sys_page_alloc(0, fd1, PTE_P | PTE_W | PTE_U | PTE_SHARE)) < 0)
		goto err1;

	// 将管道结构分配为两者的第一数据页，两个文件描述符的数据页映射到一个物理页
	va = fd2data(fd0);
	if((r = sys_page_alloc(0, va, PTE_P | PTE_W | PTE_U | PTE_SHARE)) < 0)
		goto err2;
	if((r = sys_page_map(0, va, 0, fd2data(fd1), PTE_P | PTE_W | PTE_U | PTE_SHARE)) < 0)
		goto err3;

	// 设置 fd 结构体
	fd0->fd_dev_id = devpipe.dev_id;
	fd0->fd_omode = O_RDONLY;

	fd1->fd_dev_id = devpipe.dev_id;
	fd1->fd_omode = O_WRONLY;

	if (debug)
		cprintf("[%08x] pipecreate %08x\n", thisenv->env_id, uvpt[PGNUM(va)]);

	pfd[0] = fd2num(fd0);
	pfd[1] = fd2num(fd1);
	return 0;

err3:
	sys_page_unmap(0, va);
err2:
	sys_page_unmap(0, fd1);
err1:
	sys_page_unmap(0, fd0);
err:
	return r;
}

static int _pipeisclosed(struct Fd *fd, struct Pipe *p){
	int n, nn, ret;

	while(1){
		n = thisenv->env_runs;
		ret = pageref(fd) == pageref(p);
		nn = thisenv->env_runs;
		if(n == nn)
			return ret;
		if(n != nn && ret == 1)
			cprintf("pipe race avoided\n", n, thisenv->env_runs, ret);
	}
}

int pipeisclosed(int fdnum){
	struct Fd *fd;
	struct Pipe *p;
	int r;

	if((r = fd_lookup(fdnum, &fd)) < 0)
		return r;
	p = (struct Pipe *)fd2data(fd);
	return _pipeisclosed(fd, p);
}

static ssize_t devpipe_read(struct Fd *fd, void *vbuf, size_t n){
	uint8_t *buf;
	size_t i;
	struct Pipe *p;

	p = (struct Pipe *)fd2data(fd);
	if (debug)
		cprintf("[%08x] devpipe_read %08x %d rpos %d wpos %d\n",
			thisenv->env_id, uvpt[PGNUM(p)], n, p->p_rpos, p->p_wpos);

	buf = vbuf;
	for(i = 0; i < n; i++){
		while(p->p_rpos == p->p_wpos){
			// pipe 为空，如果我们有任何数据，返回
			if(i > 0)
				return i;
			// 如果所有的写入都没了，注意 eof
			if(_pipeisclosed(fd, p))
				return 0;
			// yield and see what happens
			if (debug)
				cprintf("devpipe_read yield\n");
			sys_yield();
		}
		// 有一个字节可读，读取它。等待增加 rpos，直到获取该字节
		buf[i] = p->p_buf[p->p_rpos % PIPEBUFSIZ];
		p->p_rpos++;
	}
	return i;
}

static ssize_t devpipe_write(struct Fd *fd, const void *vbuf, size_t n){
	const uint8_t *buf;
	size_t i;
	struct Pipe *p;

	p = (struct Pipe *)fd2data(fd);
	if (debug)
		cprintf("[%08x] devpipe_write %08x %d rpos %d wpos %d\n",
			thisenv->env_id, uvpt[PGNUM(p)], n, p->p_rpos, p->p_wpos);

	buf = vbuf;
	for(i = 0; i < n; i++){
		while(p->p_wpos >= p->p_rpos + sizeof(p->p_buf)){
			// pipe 满了，如果所有的读取都没了（这只是现在唯一的写入），注意 eof
			if(_pipeisclosed(fd, p))
				return 0;
			// yield and see what happens
			if (debug)
				cprintf("devpipe_write yield\n");
			sys_yield();
		}
		// 有一个字节的空间，存储它。等待增加 wpos，直到字节被存储
		p->p_buf[p->p_wpos % PIPEBUFSIZ] = buf[i];
		p->p_wpos++;
	}
	return i;
}

static int devpipe_stat(struct Fd *fd, struct Stat *stat){
	struct Pipe *p = (struct Pipe *)fd2data(fd);
	strcpy(stat->st_name, "<pipe>");
	stat->st_size = p->p_wpos - p->p_rpos;
	stat->st_isdir = 0;
	stat->st_dev = &devpipe;
	return 0;
}

static int devpipe_close(struct Fd *fd){
	(void) sys_page_unmap(0, fd);
	return sys_page_unmap(0, fd2data(fd));
}
