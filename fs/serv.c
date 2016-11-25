/*
  文件系统服务器主循环 - 提供来自其他环境的IPC请求
 */

#include <inc/x86.h>
#include <inc/string.h>

#include "fs.h"

#define debug		0

/*
  文件系统服务器为每个打开的文件维护三个结构
  1. 磁盘上的 'struct File' 映射到映射磁盘的内存部分。此内存对文件服务器是私有的
  2. 每个打开的文件也有一个 'struct Fd'，这种类型对应于一个 Unix 文件描述符；
  	这个 'struct Fd' 保存在内存中的自己的页面上，并且与文件打开的任何环境共享
  3. 'struct OpenFile' 链接这两个结构，并且对文件服务器保持私有；
  	服务器维护所有打开文件的数组，用 'file ID' 索引 (最多可以同时打开 MAXOPEN 个文件)；
	客户端使用文件 ID 与服务器通信；文件 ID 和内核中的环境 ID 很像；
	使用 openfile_lookup 将文件 ID 转换为 struct OpenFile
 */
struct OpenFile {
	uint32_t o_fileid;			// 文件 id
	struct File *o_file;			// 打开文件的映射描述符
	int o_mode;					// 打开模式
	struct Fd *o_fd;				// fd 页
};

// 同时在文件系统中打开的文件的最大数量
#define MAXOPEN		1024
#define FILEVA		0xD0000000

// 初始化强制进入数据段
struct OpenFile opentab[MAXOPEN] = {
	{ 0, 0, 1, 0 }
};

// 用于接收包含客户端请求的页面映射的虚拟地址
union Fsipc *fsreq = (union Fsipc *)0x0ffff000;

void serve_init(void){
	int i;
	uintptr_t va = FILEVA;
	for(i = 0; i < MAXOPEN; i++){
		opentab[i].o_fileid = i;
		opentab[i].o_fd = (struct Fd *)va;
		va += PGSIZE;
	}
}

/*
  分配打开的文件
 */
int openfile_alloc(struct OpenFile **o){
	int i, r;

	// 查找可用的打开文件表条目
	for(i = 0; i < MAXOPEN; i++){
		switch(pageref(opentab[i].o_fd)){
		case 0:
			if((r = sys_page_alloc(0, opentab[i].o_fd, PTE_P | PTE_U | PTE_W)) < 0)
				return r;
		case 1:
			opentab[i].o_fileid += MAXOPEN;
			*o = &opentab[i];
			memset(opentab[i].o_fd, 0, PGSIZE);
			return (*o)->o_fileid;
		}
	}
	return -E_MAX_OPEN;
}

/*
  查找 envid 的打开文件
 */
int openfile_lookup(envid_t envid, uint32_t fileid, struct OpenFile **po){
	struct OpenFile *o;

	o = &opentab[fileid % MAXOPEN];
	if(pageref(o->o_fd) <= 1 || o->o_fileid != fileid)
		return -E_INVAL;

	*po = o;
	return 0;
}

/*
  在模式 req->req_omode 中打开 req->req_path，
  分别存储 Fd 页面、返回到调用环境的权限到 *pg_store 和 *perm_store 中
 */
int serve_open(envid_t envid, struct Fsreq_open *req, void **pg_store, int *prem_store){
	char path[MAXPATHLEN];
	struct File *f;
	int fileid;
	int r;
	struct OpenFile *o;

	if(debug)
		cprintf("serve_open %08x %s 0x%x\n", envid, req->req_path, req->req_omode);

	// 在路径中复制，确保它以 null 终止
	memmove(path, req->req_path, MAXPATHLEN);
	path[MAXPATHLEN - 1] = 0;

	if((r = openfile_alloc(&o)) < 0){
		if(debug)
			cprintf("openfile_alloc failed: %e", r);
		return r;
	}
	fileid = r;

	// 打开文件
	if(req->req_omode & O_CREAT){
		if((r = file_create(path, &f)) < 0){
			if(!(req->req_omode & O_EXCL) && r == -E_FILE_EXISTS)
				goto try_open;
			if(debug)
				cprintf("file_create failed: %e", r);
			return r;
		}
	}
	else{
try_open:
		if((r = file_open(path, &f)) < 0){
			if(debug)
				cprintf("file_open failed: %e", r);
			return r;
		}
	}

	// 截短 Truncate
	if(req->req_omode & O_TRUNC){
		if((r = file_set_size(f, 0)) < 0){
			if(debug)
				cprintf("file_set_size failed: %e", r);
			return r;
		}
	}
	if((r = file_open(path, &f)) < 0){
		if(debug)
			cprintf("file_open failed: %e", r);
		return r;
	}

	// 保存文件指针
	o->o_file = f;

	// 填充 Fd 结构体
	o->o_fd->fd_file.id = o->o_fileid;
	o->o_fd->fd_omode = req->req_omode & O_ACCMODE;
	o->o_fd->fd_dev_id = devfile.dev_id;
	o->o_mode = req->req_omode;

	if(debug)
		cprintf("sending success, page %08x\n", (uintptr_t) o->o_fd);

	// 通过设置 *pg_store 与调用者共享 FD 页面，将其权限存储在 *perm_store 中
	*pg_store = o->o_fd;
	*perm_store = PTE_P | PTE_U | PTE_W;

	return 0;
}

/*
  将 req->req_fileid 的大小设置为 req->req_size 字节，根据需要截断或扩展文件
 */
int serve_set_size(envid_t envid, struct Fsreq_set_size *req){
	struct OpenFile *o;
	int r;

	if(debug)
		cprintf("serve_set_size %08x %08x %08x\n", envid, req->req_fileid, req->req_size);

	// 每个文件系统IPC调用具有相同的一般结构
	// 首先，使用 openfile_lookup 来查找相关的打开文件。失败时，使用 ipc_send 将错误代码返回给客户端
	if((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
		return r;

	// 其次，调用相关文件系统函数 (fs/fs.c)。失败时，将错误代码返回给客户端
	return file_set_size(o->o_file, req->req_size);
}

/*
  从 ipc->read.req_fileid 中的当前寻找位置读取最多 ipc->read.req_n 字节；
  在 ipc->readRet 中将从文件中读取的字节返回给调用者，然后更新查找位置；
  成功返回 读取的比特数，错误返回小于 0
 */
int serve_read(envid_t envid, union Fsipc *ipc){
	struct Fsreq_read *req = &ipc->read;
	struct Fsret_read *ret = &ipc->readRet;

	if(debug)
		cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// LAB 5
	
	int r;
	ssize_t nbytes;
	struct OpenFile *o;

	if((r = openfile_lookup(envid, req->req_fileid, &o)) < 0){
		cprintf("serve_read: failed to lookup open file id: error %e\n", r);
		return r;
	}

	nbytes = file_read(o->o_file, (void *)ret->ret_buf, MIN(req->req_n, PGSIZE), o->o_fd->fd_offset);

	if(nbytes > 0)
		o->o_fd->fd_offset += nbytes;
	return nbytes;
}

/*
  将 req->req_n 字节从 req->req_buf 写入 req_fileid，从当前搜索位置开始，并相应地更新搜索位置
  如有必要，扩展文件。成功返回 写入的比特数，错误返回小于 0
 */
int serve_write(envid_t envid, struct Fsreq_write *req){
	if(debug)
		cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// LAB 5
	
	int r;
	ssize_t nbytes;
	struct OpenFile *o;

	if((r = openfile_lookup(envid, req->req_fileid, &o)) < 0){
		cprintf("serve_write: failed to lookup open file id: error %e", r);
		return r;
	}

	nbytes = MIN(req->req_n, PGSIZE - (sizeof(int) + sizeof(size_t)));
	nbytes = file_write(o->o_file, (void *)req->req_buf, nbytes, o->o_fd->fd_offset);

	if(nbytes > 0)
		o->o_fd->fd_offset += nbytes;
	return nbytes;
}

/*
  在 ipc->statRet 中将文件的 struct Stat 返回给调用者
 */
int serve_stat(envid_t envid, union Fsipc *ipc){
	struct Fsreq_stat *req = &ipc->stat;
	struct Fsret_stat *ret = &ipc->statRet;
	struct OpenFile *o;
	int r;

	if(debug)
		cprintf("serve_stat %08x %08x\n", envid, req->req_fileid);

	if((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
		return r;

	strcpy(ret->ret_name, o->o_file->f_name);
	ret->ret_size = o->o_file->f_size;
	ret->ret_isdir = (o->o_file->f_type == FTYPE_DIR);
	return 0;
}

/*
  将 req->req_fileid 的所有数据和元数据刷新到磁盘
 */
int serve_flush(envid_t envid, struct Fsreq_flush *req){
	struct OpenFile *o;
	int r;

	if(debug)
		cprintf("serve_flush %08x %08x\n", envid, req->req_fileid);

	if((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
		return r;

	file_flush(o->o_file);
	return 0;
}

int serve_sync(envid_t envid, union Fsipc *req){
	fs_sync();
	return 0;
}

typedef int (*fshandler)(envid_t envid, union Fsipc *req);

fshandler handlers[] = {
	[FSREQ_READ] =		serve_read,
	[FSREQ_STAT] =		serve_stat,
	[FSREQ_FLUSH] =		(fshandler)serve_flush,
	[FSREQ_WRITE] =		(fshandler)serve_write,
	[FSREQ_SET_SIZE] =	(fshandler)serve_set_size,
	[FSREQ_SYNC] =		serve_sync
};

void serve(void){
	uint32_t req, whom;
	int perm, r;
	void *pg;

	while(1){
		perm = 0;
		req = ipc_recv((int32_t *)&whom, fsreq, &perm);
		if(debug)
			cprintf("fs req %d from %08x [page %08x: %s]\n", req, whom, uvpt[PGNUM(fsreq)], fsreq);

		// 所有请求都必须包含参数页
		if(!(perm | PTE_P)){
			cprintf("Invalid request from %08x: no argument page\n", whom);
			continue;
		}

		pg = NULL;
		if(req == FSREQ_OPEN)
			r = serve_open(whom, (struct Fsreq_open *)fsreq, &pg, &perm);
		else if(req < ARRAY_SIZE(handlers) && handlers[req])
			r = handlers[req](whom, fsreq);
		else{
			cprintf("Invalid request code %d from %08x\n", req, whom);
			r = -E_INVAL;
		}
		ipc_send(whom, r, pg, perm);
		sys_page_unmap(0, fsreq);
	}
}

void umain(int argc, char **argv){
	static_assert(sizeof(struct File) == 256);
	binaryname = "fs";
	cprintf("FS is running !\n");

	// 检查我们是否能够执行 I/O
	outw(0x8A00, 0x8A00);
	cprintf("FS can do I/O\n");

	serve_init();
	fs_init();
	fs_test();
	serve();
}
