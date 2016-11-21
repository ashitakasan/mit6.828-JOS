#ifndef JOS_INC_FS_H
#define JOS_INC_FS_H

#include <inc/types.h>
#include <inc/mmu.h>

// 文件节点 (内存中和磁盘上)
// 每个文件系统块的字节数 - 与页面大小相同
#define BLKSIZE			PGSIZE
#define BLKBITSIZE		(BLKSIZE * 8)

// 文件名的最大大小 (单个路径组件)，包括 null 必须是4的倍数
#define MAXNAMELEN		128

// 完整路径名的最大大小，包括null
#define MAXPATHLEN		1024

// 文件描述符中的块指针数
#define NDIRECT			10
// 间接块中的直接块指针数
#define NINDIRECT		(BLKSIZE / 4)

#define MAXFILESIZE		((NDIRECT + NINDIRECT) * BLKSIZE)

struct File {
	char f_name[MAXNAMELEN];		// 文件名
	off_t f_size;				// 文件大小
	uint32_t f_type;				// 文件类型

	// 块指针，如果块的值为不为 0，则分配块
	uint32_t f_direct[NDIRECT];	// 直接块
	uint32_t f_indirect;			// 间接块

	// 填充到256字节; 必须进行算术以防我们在64位机器上编译fsformat
	uint8_t f_pad[256 - MAXNAMELEN - 8 - 4 * NDIRECT - 4];
}__attribute__((packed));			// 仅在某些64位计算机上需要

// 一个 inode 块正好包含 BLKFILES 个 struct File
#define BLKFILES			(BLKSIZE / sizeof(struct File))

#define FTYPE_REG		0		// 常规温佳妮
#define FTYPE_DIR		1		// 目录

// 文件系统超级块（内存中和磁盘上）

#define FS_MAGIC			0x4A0530AE	// 

struct Super {
	uint32_t s_magic;			// 魔数：FS_MAGIC
	uint32_t s_nblocks;			// 磁盘上的块总数
	struct File s_root;			// 根目录节点
};

// 从客户端到文件系统的请求的定义
enum {
	FSREQ_OPEN = 1,
	FSREQ_SET_SIZE,
	// Read 在请求页面上返回 Fsret_read
	FSREQ_READ,
	FSREQ_WRITE,
	// Stat 在请求页面上返回 Fsret_stat
	FSREQ_STAT,
	FSREQ_FLUSH,
	FSREQ_REMOVE,
	FSREQ_SYNC
};

union Fsipc {
	struct Fsreq_open {
		char req_path[MAXPATHLEN];
		int req_omode;
	} open;
	struct Fsreq_set_size {
		int req_fileid;
		off_t req_size;
	} set_size;
	struct Fsreq_read {
		int req_fileid;
		size_t req_n;
	} read;
	struct Fsret_read {
		char ret_buf[PGSIZE];
	} readRet;
	struct Fsreq_write {
		int req_fileid;
		size_t req_n;
		char req_buf[PGSIZE - (sizeof(int) + sizeof(size_t))];
	} write;
	struct Fsreq_stat {
		int req_fileid;
	} stat;
	struct Fsret_stat {
		char ret_name[MAXNAMELEN];
		off_t ret_size;
		int ret_isdir;
	} statRet;
	struct Fsreq_flush {
		int req_fileid;
	} flush;
	struct Fsreq_remove {
		char req_path[MAXPATHLEN];
	} remove;

	// Ensure Fsipc is one page
	char _pad[PGSIZE];
};

#endif /* !JOS_INC_FS_H */
