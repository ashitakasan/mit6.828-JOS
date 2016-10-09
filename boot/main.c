#include <inc/x86.h>
#include <inc/elf.h>

/**
 * 这是一个简单的启动引导，它唯一的工作就是从IDE硬盘启动ELF格式内核镜像
 * 硬盘布局：
 * 	这个程序(boot.S 和 main.c)是引导程序，它应该存放在硬盘的第一扇区
 * 	第2扇区之后保存内核镜像
 * 	内核镜像必须是ELF格式
 *
 * 启动步骤
 * 	CPU启动后，加载BOIS到内存中并执行
 * 	BIOS会初始化设备，设置终端子程序，读取启动硬盘的第一扇区到内存中并跳转到它
 * 	假设引导程序储存在硬盘的第一扇区，引导程序接管系统后，
 * 	boot.S 开始控制，设置保护模式，以及一个堆栈以便 C程序运行，然后调用 bootmain
 * 	bootmain函数开始接管系统，读取内核然后跳转
 */

// 扇区大小为 512
#define SECTSIZE		512
// 将内核加载到内存的起始地址
#define ELFHDR		((struct Elf *) 0x10000)		// 暂存空间

// 读取硬盘扇区的内容
void readsect(void*, uint32_t);
// 读取内核镜像的程序段到内存中
void readseg(uint32_t, uint32_t, uint32_t);

// bootmain C 引导程序
void bootmain(void){
	// 定义两个程序头表项指针
	struct Proghdr *ph, *eph;

	// 将硬盘上从第一扇区开始的4096字节数据读取到内存中地址为 0x0010000 处
	readseg((uint32_t) ELFHDR, SECTSIZE * 8, 0);

	// 检查者是否是一个合法的ELF文件
	if(ELFHDR->e_magic != ELF_MAGIC)
		goto bad;

	// 找到第一程序头表项的起始地址
	ph = (struct Proghdr *)((uint8_t *) ELFHDR + ELFHDR->e_phoff);
	// 程序头表项的结束位置
	eph = ph + ELFHDR->e_phnum;

	// 将内核加载进内存中
	for(; ph < eph; ph++)
		// p_pa 就是该程序段应该加载到内存中的位置（物理地址）
		readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

	// 开始执行内核，调用ELF头的entry指针，该函数不会返回
	((void (*)(void)) (ELFHDR->e_entry))();

bad:
	outw(0x8A00, 0x8A00);
	outw(0x8A00, 0x8E00);
	while(1)
		;
}

// 从ELF文件偏移量为offset处，读取count个字节到内存地址pa处
void readseg(uint32_t pa, uint32_t count, uint32_t offset){
	// 段的结束地址
	uint32_t end_pa;
	end_pa = pa + count;

	// 将 pa 设置为512字节对齐的地方
	pa &= ~(SECTSIZE - 1);

	// 将相对于ELF头文件的偏移量转换为扇区，ELF格式的内核镜像存放在第一扇区中
	offset = (offset / SECTSIZE) + 1;

	// 如果这里太慢，我们可以一次性读取多个扇区
	// 这里可能写入更多数据到内存中，但这无影响，因为读取是有序的
	while(pa < end_pa){
		// 由于我们还没有允许内存分页，我们使用确定的段映射，直接使用物理地址
		// 读取程序的一个扇区大小，将offset扇区中的数据读到物理地址为pa的地方
		readsect((uint8_t*)pa, offset);
		pa += SECTSIZE;
		offset++;
	}
}

void waitdisk(void){
	// 等待硬盘准备好
	while((inb(0x1F7) & 0xC0) != 0x40)
		;
}

// 读取硬盘文件
void readsect(void *dst, uint32_t offset){
	waitdisk();

	outb(0x1F2, 1);
	outb(0x1F3, offset);
	outb(0x1F4, offset >> 8);
	outb(0x1F5, offset >> 16);
	outb(0x1F6, (offset >> 24) | 0xE0);
	outb(0x1F7, 0x20);				// 命令 0x20, 读取扇区

	waitdisk();

	// 读取一个扇区
	insl(0x1F0, dst, SECTSIZE / 4);
}
