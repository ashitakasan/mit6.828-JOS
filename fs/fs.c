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
  使用free_block作为操作位图的示例
 */
int alloc_block(void){
	// 位图由一个或多个块组成。单个位图块包含 BLKBITSIZE 个块的使用位
	// 在磁盘中有 super->s_nblocks 块
	// LAB 5
	panic("alloc_block not implemented");
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
  
 */
static int file_block_walk(struct File *f, uint32_t fileno, uint32_t **ppdiskno, bool alloc){
	// LAB 5
	panic("file_block_walk not implemented");
}

