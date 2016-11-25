#include <inc/fs.h>
#include <inc/string.h>
#include <inc/lib.h>

#define debug		0

union Fsipc fsipcbuf __attribute__((aligned(PGSIZE)));

/*
  向文件服务器发送环境间请求，并等待回复；
  请求主体应该在 fsipcbuf 中，部分响应可以写回到 fsipcbuf；
  type: 请求代码，作为简单整数IPC值传递；
  dstva: 接收回复页面的虚拟地址，如果没有则为0；
  返回文件服务器的结果
 */
static int fsipc(unsigned type, void *dstva){
	static envid_t envid;
	if(fsenv == 0)
		fsenv = ipc_find_env(ENV_TYPE_FS);

	static_assert(sizeof(fsipcbuf) == PGSIZE);
	if (debug)
		cprintf("[%08x] fsipc %d %08x\n", thisenv->env_id, type, *(uint32_t *)&fsipcbuf);

	ipc_send(fsenv, type, &fsipcbuf, PTE_P | PTE_U | PTE_W);
	return ipc_send(NULL, dstva, NULL);
}

static int devfile_flush(struct Fd *fd);
static ssize_t devfile_read(struct Fd *fd, void *buf, size_t n);
static ssize_t devfile_write(struct Fd *fd, const void *buf, size_t n);
static int devfile_stat(struct Fd *fd, struct Stat *stat);
static int devfile_trunc(struct Fd *fd, off_t newsize);

struct Dev devfile = {
	.dev_id = 'f',
	.dev_name = "file",
	.dev_close = devfile_flush,
	.dev_read = devfile_read,
	.dev_write = devfile_write,
	.dev_stat = devfile_stat,
	.dev_trunc = devfile_trunc
};

/*
  打开文件 (或目录)
  成功时的返回文件描述符索引，错误返回小于 0：
  	-E_BAD_PATH: 如果路径太长 (>= MAXPATHLEN)
 */
int open(const char *path, int mode){
	// 使用 fd_alloc 查找未使用的文件描述符页面。然后向文件服务器发送文件打开请求
	// 在请求中包含 'path' 和 'omode'，并将返回的文件描述符页映射到适当的 fd 地址
	// FSREQ_OPEN 成功返回 0，失败返回小于 0
	// fd_alloc 不分配页面，它只是返回一个未使用的 fd 地址。你需要分配一个页面
	// 返回文件描述符的索引。如果 fd_alloc 之后的任何步骤失败，请使用 fd_close 释放文件描述符
	
	int r;
	struct Fd *fd;

	if(strlen(path) >= MAXPATHLEN)
		return -E_BAD_PATH;

	if((r = fd_alloc(&fd)) < 0)
		return r;

	strcpy(fsipcbuf.open.req_path, path);
	fsipcbuf.open.req_omode = mode;

	if((r = fsipc(FSREQ_OPEN, fd)) < 0){
		fd_close(fd, 0);
		return r;
	}
	return fd2num(fd);
}

/*
  清除文件描述符。此后 fileid 无效；
  此函数由 fd_close 调用。fd_close 将负责从此环境中取消映射 FD 页面；
  由于服务器使用 FD 页上的引用计数来检测哪些文件是打开的，因此取消映射足以释放服务器端资源；
  除此之外，我们只需要确保我们的更改被刷新到磁盘
 */
static int devfile_flush(struct Fd *fd){
	fsipcbuf.flush.req_fileid = fd->fd_file._id;
	return fdipc(FSREQ_FLUSH, NULL);
}

/*
  从当前位置的 'fd' 中读取最多 'n' 个字节为 'buf'
  成功返回读取的字节数，错误返回小于 0
 */
static ssize_t devfile_read(struct Fd *fd, void *buf, size_t n){
	// 在使用请求参数填充 fsipcbuf.read 后，向文件系统服务器发出 FSREQ_READ 请求；
	// 读取的字节将由文件系统服务器写回到 fsipcbuf
	
	int r;

	fsipcbuf.read.req_fileid = fd->fd_file._id;
	fsipcbuf.read.req_n = n;
	if((r = fsipc(FSREQ_READ, NULL)) < 0)
		return r;

	assert(r <= n);
	assert(r <= PGSIZE);
	memmove(buf, fsipcbuf.readRet.ret_buf, r);
	return r;
}

/*
  在当前寻找位置处从 'buf' 到 'fd' 写入最多 'n' 个字节
  成功返回写入的字节数，错误返回小于 0
 */
static ssize_t devfile_write(struct Fd *fd, const void *buf, size_t n){
	// 向文件系统服务器发出 FSREQ_WRITE 请求
	// 注意： fsipcbuf.write.req_buf 只有这么大，但请记住，写总是允许写入比请求的字节少
	// LAB 5
	
	int r;
	n = MIN(n, PGSIZE - (sizeof(int) + sizeof(size_t)));

	fsipcbuf.write.req_fileid = fd->fd_file._id;
	fsipcbuf.write.req_n = n;

	memmove(fsipcbuf.write.req_buf, buf, n);

	return fsipc(FSREQ_WRITE, NULL);
}

static int devfile_stat(struct Fd *fd, struct Stat *stat){
	int r;

	fsipcbuf.stat.req_fileid = fd->fd_file._id;
	if((r = fsipc(FSREQ_STAT, NULL)) < 0)
		return r;

	strcpy(st->st_name, fsipcbuf.statRet.ret_name);
	st->st_size = fsipcbuf.statRet.ret_size;
	st->st_isdir = fsipcbuf.statRet.ret_isdir;
	return 0;
}

/*
  截断或扩展一个打开的文件为 'size' 字节
 */
static int devfile_trunc(struct Fd *fd, off_t newsize){
	fsipcbuf.set_size.req_fileid = fd->fd_file._id;
	fsipcbuf.set_size.req_size = newsize;
	return fsipc(FSREQ_SET_SIZE, NULL);
}

/*
  使磁盘与缓冲区高速缓存同步
 */
int sync(void){
	// 请求文件服务器通过在缓冲区高速缓存中写入任何脏块来更新磁盘
	return fsipc(FSREQ_SYNC, NULL);
}
