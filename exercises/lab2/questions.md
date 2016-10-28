## 1. Assuming that the following JOS kernel code is correct, what type should variable x have, uintptr_t or physaddr_t?
```C
mystery_t x;
char* value = return_a_pointer();
*value = 10;
x = (mystery_t) value;
```
mystery_t 应该是 uintptr_t；


## 2. What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:
|	Entry	|	Base Virtual Address	|		Points to (logically)			|
|------------	|--------------------------	|--------------------------------------------	|
|	1023		|						|Page table for top 4MB of phys memory	|
|	1022		|						|										|
|	...		|			...			|					...					|
|	960		|		0xF0000000		|			0x0, KERNBASE				|
|	959		|		0xEFC00000		|			0xF010D000, bootstack			|
|	958		|						|										|
|	957		|		0xEF400000		|			0xF0118000, kern_pgdir		|
|	956		|		0xEF000000		|			0xF0119000, pages				|
|	...		|			...			|					...					|
|	2		|		0x00800000		|			[see next question]			|
|	1		|		0x00400000		|			[see next question]			|
|	0		|		0x00000000		|			[see next question]			|

#### memory layout
	defines											   addresses, availables
	4 Gig -------------------> +-------------------------------+ 0xFFFFFFFF
							|							|
							+-------------------------------+ 0xF0159000, (这里距离KERNBASE 1380KB)
							|							| 256KB
							+-------------------------------+ 0xF0119000, pages(32768*8=256KB), 所有页面结构体
							|						 	| 4KB
							+-------------------------------+ 0xF0118000, kern_pgdir(4KB), 内核页目录
							|							|
							+-------------------------------+ 0xF010D000, bootstack，对应物理地址
							|							|
	KERNBASE, KSTACKTOP----> +-------------------------------+ 0xF0000000, 内核虚拟地址的开始地址
						  |	|							| KSTKSIZE = 32KB
				PTSIZE = 4MB	+-------------------------------+ 0xEFFF8000, 内核栈，由物理页面支持
						  |	|							|
						 ---	+-------------------------------+ 0xEFC00000, 内核栈结束
							|							|
	UVPT	---------------->	+-------------------------------+ 0xEF400000, 其对应的页目录项为kern_pgdir首地址，保护 kern_pgdir对应的物理页
							|							|
	UPAGES	---------------->	+-------------------------------+ 0xEF000000, 其对应的物理页保存 pages，用户只读

#### memory map
map va = 0xef000000, size = 64*4096, pa = 0x119000
map va = 0xefff8000, size = 8*4096, pa = 0x10d000
map va = 0xf0000000, size = 65535*4096, pa = 0x0

查找虚拟地址对应的内存页：根据虚拟地址 va 的前 10bits，在 kern_pgdir 中查找 页目录项；
页目录项中的前 20bits 指定了该目录项对应的页表的物理地址，后 12bits 指示了该页目录项对应的页表的标志选项；
找到页表基地址后，根据 va 的中间 10bits 查找 va 对应的该页表中 页表项；
页表项的前 20bits 指定了 va 所在物理页面的物理首地址，后 12bits 指示了该页面的标志选项。


## 3. We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?

从虚拟地址的寻址过程考虑，虚拟地址寻址到对应的物理地址过程为：
虚拟地址 --> 页目录的目录项 --> 页表基地址 --> 页表项 --> 物理页的首地址；

在页目录项中，保存了整个页表的访问权限，在页表项中，保存了当前物理页面的访问权限，通过这两个权限的限制，保证了用户不能访问到一些物理页面。


## 4. What is the maximum amount of physical memory that this operating system can support? Why?

Physical memory: 131072K available, base = 640K, extended = 130432K
实际可用 128MB，qemu模拟器为 JOS分配了 128MB的物理内存；
操作系统支持 2^32 字节，即 4GB


## 5. How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?

如果有 4GB 物理内存，则内存管理开销一共 12292KB；
其中由 一个页目录 4KB，1024个页表 1024*4KB，2^20 个 PageInfo结构 8MB。


## 6. Revisit the page table setup in kern/entry.S and kern/entrypgdir.c. Immediately after we turn on paging, EIP is still a low number (a little over 1MB). At what point do we transition to running at an EIP above KERNBASE? What makes it possible for us to continue executing at a low EIP between when we enable paging and when we begin running at an EIP above KERNBASE? Why is this transition necessary?

```ASM
# 将物理地址页表 entry_pgdir 地址保存到 寄存器cr3中
movl	$(RELOC(entry_pgdir)), %eax
movl	%eax, %cr3
# 启用分页，同时，将虚拟地址的高地址[3840MB, 3844MB]与低地址[0, 4MB]都映射到 同一物理地址[0, 4MB]上，以便一些低地址上的指令能够继续运行
movl	%cr0, %eax
orl		$(CR0_PE|CR0_PG|CR0_WP), %eax
movl	%eax, %cr0

# 现在分页已经启用，但仍然运行在低地址的指令指针寄存器（EIP），在执行 C代码前，跳转到高地址空间
# 在这里将指令寄存器 EIP 指向高地址，
mov 		$relocated, %eax
jmp		*%eax
```
