#include <inc/mmu.h>
#include <inc/memlayout.h>

# 右移逻辑
#define SRL(val, shamt)		(((val) >> (shamt)) & ~(-1 << (32 - (shamt))))


# kernel 代码位于地址 ~(KERNBASE + 1MB)，即高地址空间
# 但是 bootlaoder 位于地址 1MB，即地址空间
# RELOC(x) 宏，将地址 x 从链接地址（虚拟地址）映射到实际的物理地址上

#define RELOC(x) ((x) - KERNBASE)

#define MULTIBOOT_HEADER_MAGIC (0x1BADB002)
#define MULTIBOOT_HEADER_FLAGS (0)
#define CHECKSUM (-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS))

# kernel 入口点

.text

# 多重引导头部
.align 4
.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_HEADER_FLAGS
.long CHECKSUM

# “_start”指定ELF入口点，
# 当 bootloader进入这段代码时，还没有设置虚拟地址，
# 我们需要引导程序跳转到入口点的物理地址。
.globl	_start
_start = RELOC(entry)

.globl entry
entry:
	movw	$0x1234, 0x472		# 热启动，kernel第一条指令

	# 此时还没有设置虚拟地址，所以 kernel需要在 bootloader所在的 1MB地址空间上执行，
	# 但是kernel的C代码应该在 内存的高地址空间执行，因此需要通过地址转换，
	# 将虚拟的高地址空间 [3840MB, 3844MB]映射到物理的低地址空间 [0, 4MB]。
	# 在启用 mem_init 之前，这里 4MB地址空间足够用了。

	# 将物理地址页表 entry_pgdir 到 寄存器cr3中
	movl	$(RELOC(entry_pgdir)), %eax
	movl	%eax, %cr3
	# 启用分页
	movl	%cr0, %eax
	orl		$(CR0_PE|CR0_PG|CR0_WP), %eax
	movl	%eax, %cr0

	# 现在分页已经启用，但仍然运行在低地址的指令指针寄存器（EIP）
	# 在执行 C代码前，跳转到高地址空间
	mov 		$relocated, %eax
	jmp		*%eax

relocated:
	# 清除帧指针寄存器（EBP）
	# 因此，一旦我们进入调试C代码，调用栈将被正确终止
	movl	$0x0, %ebp

	# 设置堆栈指针
	movl	$(bootstacktop), %esp

	# 开始调用 C代码，初始化OS
	call	i386_init

# 自旋，这里不应该被调用
spin:	jmp spin


.data
	# 启动堆栈
	.p2align		PGSHIFT			# 强制页面对齐
	.globl		bootstack
bootstack:
	.space		KSTKSIZE
	.globl		bootstacktop
bootstacktop:

