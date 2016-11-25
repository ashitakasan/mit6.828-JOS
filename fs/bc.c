#include "fs.h"

/*
  返回此磁盘块的虚拟地址
 */
void *diskaddr(uint32_t blockno){
	if(blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char *)(DISKMAP + blockno * BLKSIZE);
}

/*
  这个虚拟地址是否已经映射
 */
bool va_is_mapped(void *va){
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

/*
  这个虚拟地址是否脏
 */
bool va_is_dirty(void *va){
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

/*
  通过从磁盘加载读取到内存中的任何磁盘块发生故障
 */
static void bc_pgfault(struct UTrapframe *utf){
	void *addr = (void *)utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// 检查故障是否在块缓存区域内
	if(addr < (void *)DISKMAP || addr >= (void *)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x", utf->utf_eip, addr, utf->utf_err);

	// 检查块号正确性
	if(super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// 在磁盘映射区域中分配一个页面，将该块的内容从磁盘读入该页面
	// 首先将 addr 向下舍入到页面边界，fs/ide.c 有读取磁盘的代码
	// LAB 5

	uint32_t align_addr = ROUNDDOWN(addr, BLKSIZE);
	if((r = sys_page_alloc(0, (void *)align_addr, PTE_SYSCALL)) < 0)
		panic("fs bc_pgfault sys_page_alloc error: %e\n", r);

	if((r = ide_read(blockno * BLKSECTS, align_addr, BLKSECTS)) < 0)
		panic("fs bc_pgfault ide_read error: %e", r);

	// 清除磁盘块页面的脏位，因为我们刚刚从磁盘读取块
	if((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	// 检查我们读取的块是否已分配，为什么我们在读完块之后这样做 ?
	if(bitmap & block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

/*
  如果需要，将包含 VA 的块的内容刷新到磁盘，然后使用 sys_page_map 清除 PTE_D 位；
  如果块不在块缓存中或不脏，什么也不做
  	使用 va_is_mapped, va_is_dirty, and ide_write
  	调用 sys_page_map 时使用 PTE_SYSCALL 常量
  	不要忘了将 addr 向下舍入
 */
void flush_block(void *addr){
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if(addr < (void *)DISKMAP || addr >= (void *)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5
	int r;

	uint32_t align_addr = ROUNDDOWN(addr, BLKSIZE);
	if(!va_is_mapped(addr) || !va_is_dirty(addr))
		return;
	
	if((r = ide_write(blockno * BLKSECTS, align_addr, BLKSECTS)) < 0)
		panic("fs flush_block ide_write error: %e", r);

	if((r = sys_page_map(0, align_addr, 0, align_addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("fs flush_block, sys_page_map error: %e", r);
}

/*
  测试块缓存工作，通过粉碎超级块并读回它
 */
static void check_bc(void){
	struct Super backup;

	// 备份超级块
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void bc_init(void){
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	check_bc();

	// 通过读取一次来缓存超级块
	memmove(&super, diskaddr(1), sizeof super);
}
