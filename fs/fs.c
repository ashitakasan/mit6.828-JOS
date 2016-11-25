#include <inc/string.h>
#include <inc/partition.h>

#include "fs.h"

// --------------------------------------------------------------
// 超级块
// --------------------------------------------------------------

/*
  验证文件系统超级块
 */
void check_super(void){
	if(super->s_magic != FS_MAGIC)
		panic("bad file system magic number");
	if(super->s_nblocks > DISKSIZE / BLKSIZE)
		panic("file system is too large");

	cprintf("superblock is good\n");
}

/*
  检查块位图是否指示块“blockno”是空闲的
  如果是空闲则返回 1，否则返回 0
 */
bool block_is_free(uint32_t blockno){
	if(super == 0 || blockno >= super->s_nblocks)
		return 0;
	if(bitmap[blockno / 32] & (1 << (blockno % 32)))
		return 1;
	return 0;
}

/*
  在位图中标记一个空闲的块
 */
void free_block(uint32_t blockno){
	// blockno 零是块号的空指针
	if(blockno == 0)
		panic("attempt to free zero block");
	bitmap[blockno / 32] |= 1 << (blockno % 32);
}

/*
  搜索空闲块的位图并分配它。分配块时，立即将更改的位图块刷新到磁盘
  成功返回分配的块号，如果超出了块则返回 -E_NO_DISK
  使用 free_block 作为操作位图的示例
 */
int alloc_block(void){
	// 位图由一个或多个块组成。单个位图块包含 BLKBITSIZE 个块的使用位
	// 在磁盘中有 super->s_nblocks 块
	// LAB 5
	
	int i, blockno;
	int reserved = super->s_nblocks / BLKBITSIZE + 3;
	int usable = super->s_nblocks - reserved;
	static int start = 0;

	for(i = 0; i < usable; i++){
		blockno = reserved + (start + i) % usable;
		if(!block_is_free(blockno))
			continue;
		
		bitmap[blockno / 32] &= ~(1 << (blockno % 32));
		start = i;

		flush_block(bitmap);
		return blockno;
	}

	return -E_NO_DISK;
}

/*
  验证文件系统位图
  检查所有保留块 - 0, 1和位图块本身都被标记为使用中
 */
void check_bitmap(void){
	uint32_t i;

	// Make sure all bitmap blocks are marked in-use
	for (i = 0; i * BLKBITSIZE < super->s_nblocks; i++)
		assert(!block_is_free(2+i));

	// Make sure the reserved and root blocks are marked in-use.
	assert(!block_is_free(0));
	assert(!block_is_free(1));

	cprintf("bitmap is good\n");
}

// --------------------------------------------------------------
// File system structures
// --------------------------------------------------------------

/*
  初始化文件系统
 */
void fs_init(void){
	static_assert(sizeof(struct File) == 256);

	if(ide_probe_disk1())
		ide_set_disk(1);
	else
		ide_set_disk(0);
	bc_init();

	// 设置“super”指向超级块
	super = diskaddr(1);
	check_super();

	// 将“位图”设置为第一个位图块的开头
	bitmap = diskaddr(2);
	check_bitmap();
}

/*
  找到文件 'f' 中 'filebno' 块的磁盘块号槽位，设置 '*ppdiskbno' 指针指向它
  该槽将是 f->f_direct[] 条目之一，或间接块中的条目
  当设置 'alloc' 时，如果需要，该函数将分配一个间接块
  成功返回 0 (注意 *ppdiskbno 可能为 0)，错误返回小于 0：
  	-E_NOT_FOUND，如果该函数需要分配一个间接块，但 alloc 是 0
  	-E_NO_DISK，如果磁盘上没有间接块的空间
  	-E_INVAL，如果 filebno 超出范围 (>= NDIRECT + NINDIRECT)
 */
static int file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc){
	// LAB 5

	int r = -1;
	uint32_t *addr;
	uint32_t diskbno;
	if(ppdiskbno == 0)
		return -1;

	if(filebno < NDIRECT){
		if((diskbno = f->f_direct[filebno]) == 0){
			if(!alloc || (r = alloc_block()) < 0)
				return r;
			f->f_direct[filebno] = (uint32_t)r;
			flush_block(f);
		}
		*ppdiskbno = &f->f_direct[filebno];
		return 0;
	}

	filebno -= NDIRECT;
	if(filebno < NINDIRECT){
		if((diskbno = f->f_direct[NDIRECT]) == 0){
			if(!alloc || (r = alloc_block()) < 0)
				return r;
			diskbno = (uint32_t)r;
			f->f_direct[NDIRECT] = (uint32_t)r;
			flush_block(f);
		}
		addr = (uint32_t *)diskaddr(diskbno);
		if((addr[filebno]) == 0){
			if(!alloc || (r = alloc_block()) < 0)
				return r;
			addr[filebno] = (uint32_t)r;
		}
		*ppdiskbno = &addr[filebno];
		flush_block(addr);
		return 0;
	}
	return -E_INVAL;
}

/*
  将 *blk 设置为将映射文件 'f' 的第 filebno 块的内存中的地址
  成功返回 0，错误返回小于 0：
  	-E_NO_DISK，如果一个块需要分配但磁盘已满
  	-E_INVAL，如果 filebno 超出范围
 */
int file_get_block(struct File *f, uint32_t filebno, char **blk){
	// LAB 5
	
	int r;
	uint32_t *ppdiskbno;

	if((r = file_block_walk(f, filebno, &ppdiskbno, 1)) < 0)
		return r;

	*blk = (char *)diskaddr(*ppdiskbno);
	return 0;
}

/*
  尝试在 dir 中查找名为 name 的文件，如果找到了，将 *file 指向它
  成功返回 0 并设置 *file 指针，错误返回小于 0：
  	-E_NOT_FOUND，如果文件没有找到
 */
static int dir_lookup(struct File *dir, const char *name, struct File **file){
	int r;
	uint32_t i, j, nblock;
	char *blk;
	struct File *f;

	// 搜索 dir 中的文件名，目录文件的大小总是文件系统块大小的倍数
	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for(i = 0; i < nblock; i++){
		if((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		f = (struct File *)blk;
		for(j = 0; j < BLKFILES; j++){
			if(strcmp(f[j].f_name, name) == 0){
				*file = &f[j];
				return 0;
			}
		}
	}
	return -E_NOT_FOUND;
}

/*
  将 *file 设置为指向 dir 中的空闲文件结构。调用者负责填充文件字段
 */
static int dir_alloc_file(struct File *dir, struct File **file){
	int r;
	uint32_t nblock, i, j;
	char *blk;
	struct File *f;

	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for(i = 0; i < nblock; i++){
		if((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		f = (struct File *)blk;
		for(j = 0; j < BLKFILES; j++){
			if(f[j].f_name[0] == '\0'){				// 被删除或空的文件位置
				*file = &f[j];
				return 0;
			}
		}
	}
	dir->f_size += BLKSIZE;
	if((r = file_get_block(dir, i, &blk)) < 0)
		return r;
	f = (struct File *)blk;
	*file = &f[0];
	return 0;
}

/*
  跳过斜线路径间隔
 */
static const char *skip_slash(const char *p){
	while(*p == '/')
		p++;
	return p;
}

/*
  评估路径名，从根目录开始
  成功时，将 *pf 设置为我们找到的文件，并将 *pdir 设置为文件所在的目录
  如果我们找不到该文件，但找到它应该在的目录，设置 *pdir 并将最终路径元素复制到 lastelem
 */
static int walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem){
	const char *p;
	char name[MAXNAMELEN];
	struct File *dir, *f;
	int r;

	path = skip_slash(path);
	f = &super->s_root;
	dir = 0;
	name[0] = 0;

	if(pdir)
		*pdir = 0;
	*pf = 0;
	while(*path != '\0'){
		dir = f;
		p = path;
		while(*path != '/' && *path != '\0')
			path++;
		if(path - p >= MAXNAMELEN)
			return -E_BAD_PATH;
		memmove(name, p, path - p);
		name[path - p] = '\0';
		path = skip_slash(path);

		if(dir->f_type != FTYPE_DIR)
			return -E_NOT_FOUND;

		if((r = dir_lookup(dir, name, &f)) < 0){
			if((r == -E_NOT_FOUND && *path == '\0')){
				if(pdir)
					*pdir = dir;
				if(lastelem)
					strcpy(lastelem, name);
				*pf = 0;
			}
			return r;
		}
	}

	if(pdir)
		*pdir = dir;
	*pf = f;
	return 0;
}

// --------------------------------------------------------------
// File operations
// --------------------------------------------------------------

/*
  创建路径，成功则设置 *pf 指向创建的文件并返回 0， 错误返回小于 0
 */
int file_create(const char *path, struct File **pf){
	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;

	if((r = walk_path(path, &dir, &f, name)) == 0)
		return -E_FILE_EXISTS;
	if(r != -E_NOT_FOUND || dir == 0)
		return r;
	if((r = dir_alloc_file(dir, &f)) < 0)
		return r;

	strcpy(f->f_name, name);
	*pf = f;
	file_flush(dir);
	return 0;
}

/*
  打开路径，成功则设置 *pf 为打开的文件并返回 0，错误返回小于 0
 */
int file_open(const char *path, struct File **pf){
	return walk_path(path, 0, pf, 0);
}

/*
  读取 count 字节从 f 到 buf，从寻址位置偏移 offset 开始
  这意味着模仿标准 pread 函数
  返回读取的字节数，错误返回小于 0
 */
ssize_t file_read(struct File *f, void *buf, size_t count, off_t offset){
	int r, bn;
	off_t pos;
	char *blk;

	if(offset >= f->f_size)
		return 0;

	count = MIN(count, f->f_size - offset);

	for(pos = offset; pos < offset + count; ){
		if((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(buf, blk + pos % BLKSIZE, bn);
		pos += bn;
		buf += bn;
	}
	return count;
}

/*
  将 count 字节从 buf 写入 f，从寻址位置偏移 offset 开始
  这意味着模仿标准的 pwrite 函数。如有必要，扩展文件
  返回写入的字节数，错误返回小于 0
 */
ssize_t file_write(struct File *f, const void *buf, size_t count, off_t offset){
	int r, bn;
	off_t pos;
	char *blk;

	// 如有必要，扩展文件
	if(offset + count > f->f_size){
		if((r = file_set_size(f, offset + count)) < 0)
			return r;
	}

	for(pos = offset; pos < offset + count; ){
		if((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(blk + pos % BLKSIZE, buf, bn);
		buf += bn;
		pos += bn;
	}

	return count;
}

/*
  从 f 释放一个块，如果块不在那里，只是默默地成功
  成功返回 0，错误返回小于 0
 */
static int file_free_block(struct File *f, uint32_t filebno){
	int r;
	uint32_t *ptr;

	if((r = file_block_walk(f, filebno, &ptr, 0)) < 0)
		return r;
	if(*ptr){
		free_block(*ptr);
		*ptr = 0;
	}
	return 0;
}

/*
  删除文件 'f' 当前使用的任何块，但对于大小为 'newsize' 的文件不是必需的
  对于旧的和新的大小，计算出所需的块数，然后将块从 new_nblocks 清除为 old_nblocks
  如果 new_nblocks 不超过 NDIRECT，并且间接块已分配 (f->f_indirect！= 0)，则释放间接块
  记住要清除 f->f_indirect 指针，以便知道它是否有效
  不要改变 f->f_size
 */
static void file_truncate_blocks(struct File *f, off_t newsize){
	int r;
	uint32_t bno, old_nblocks, new_nblocks;

	old_nblocks = (f->f_size + BLKSIZE - 1) / BLKSIZE;
	new_nblocks = (newsize + BLKSIZE - 1) / BLKSIZE;
	for(bno = new_nblocks; bno < old_nblocks; bno++){
		if((r = file_free_block(f, bno)) < 0)
			cprintf("warning: file_free_block: %e", r);
	}

	if(new_nblocks <= NDIRECT && f->f_indirect){
		free_block(f->f_indirect);
		f->f_indirect = 0;
	}
}

/*
  根据需要设置文件 f 的大小，截断或扩展
 */
int file_set_size(struct File *f, off_t newsize){
	if(f->f_size > newsize)
		file_truncate_blocks(f, newsize);
	f->f_size = newsize;
	flush_block(f);
	return 0;
}

/*
  将文件 f 的内容和元数据清除到磁盘。循环文件中的所有块
  将文件块编号转换为磁盘块号，然后检查该磁盘块是否脏。如果是这样，写出来
 */
void file_flush(struct File *f){
	int i;
	uint32_t *pdiskbno;

	for(i = 0; i < (f->f_size + BLKSIZE - 1) / BLKSIZE; i++){
		if(file_block_walk(f, i, &pdiskbno, 0) < 0 || pdiskbno == NULL || *pdiskbno == 0)
			continue;
		flush_block(diskaddr(*pdiskbno));
	}
	flush_block(f);
	if(f->f_indirect)
		flush_block(diskaddr(f->f_indirect));
}

// 同步整个文件系统
void fs_sync(void){
	int i;
	for(i = 1; i < super->s_nblocks; i++)
		flush_block(diskaddr(i));
}
