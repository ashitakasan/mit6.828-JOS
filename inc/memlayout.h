#ifndef JOS_INC_MEMLAYOUT_H
#define JOS_INC_MEMLAYOUT_H

#ifndef __ASSEMBLER__
#include <inc/types.h>
#include <inc/mmu.h>
#endif /* not __ASSEMBLER__ */

/*
 * 该文件定义了我们的OS的内存管理，这是有关内核和用户模式软件
 */

// 全局描述符数字
#define GD_KT     0x08     // 内核文本
#define GD_KD     0x10     // 内核数据
#define GD_UT     0x18     // 用户文本
#define GD_UD     0x20     // 用户数据
#define GD_TSS0   0x28     // CPU 0 的 任务段选择

/*
 * 		虚拟内存映射:                                		权限
 *                                                    		内核/用户
 *
 *    4 Gig --------> 	+-------------------------------+ 0xffffffff	4096M
 *                    	|                           	| 	RW/--
 *                    	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                    	:              .                :
 *                    	:              .                :
 *                    	:              .                :
 *                    	|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| 	RW/--
 *                    	|                            	| 	RW/--
 *                    	|   	物理内存映射的虚拟地址		| 	RW/--
 *                    	|                         		| 	RW/--
 *    KERNBASE, ---->	+-------------------------------+ 0xf0000000	3840M  --+
 *    KSTACKTOP      	|     CPU0's 内核堆栈	      	|	RW/--KSTKSIZE 	|
 *                    	| - - - - - - - - - - - - - - -	|                   	|
 *                    	|     	无效内存 (*)  			| --/--  KSTKGAP   	|
 *                    	+-------------------------------+                    	|
 *                    	|    CPU1's 内核堆栈			| RW/--  KSTKSIZE 	|
 *                    	| - - - - - - - - - - - - - - -	|         PTSIZE		|
 *                    	|      	无效内存 (*)      		| --/--  KSTKGAP 		|
 *                    	+-------------------------------+                   	|
 *                    	:              .                :                   	|
 *                    	:              .                :                   	|
 *    MMIOLIM ------>	+-------------------------------+ 0xefc00000	3836M  --+
 *                    	|       内存映射 I/O     		| 	RW/--  PTSIZE	4MB
 * ULIM, MMIOBASE-->	+-------------------------------+ 0xef800000	3832M
 *                    	|  	Cur. 页表 (User R-)   	| 	R-/R-  PTSIZE	4MB
 *    UVPT      ---->	+-------------------------------+ 0xef400000	3828M 只读页面
 *                    	|          RO PAGES            | 	R-/R-  PTSIZE	4MB
 *    UPAGES    ---->	+-------------------------------+ 0xef000000		只读页副本
 *                    	|           RO ENVS            | 	R-/R-  PTSIZE	4MB
 * UTOP,UENVS ----->	+-------------------------------+ 0xeec00000	3820M 用户栈顶
 * UXSTACKTOP -/     	|     	用户异常堆栈     		| 	RW/RW  PGSIZE	4KB
 *                    	+-------------------------------+ 0xeebff000
 *                    	|       	无效内存 (*) 			| 	--/--   PGSIZE	4KB
 *    USTACKTOP  --->	+-------------------------------+ 0xeebfe000
 *                    	|      	普通用户堆栈			| 	RW/RW  PGSIZE	4KB
 *                    	+-------------------------------+ 0xeebfd000
 *                    	|                              	|
 *                    	|                              	|
 *                    	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                    	.                               .
 *                    	.                               .
 *                    	.                               .
 *                    	|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
 *                    	|     	程序数据和堆      		|
 *    UTEXT -------->	+-------------------------------+ 0x00800000	8MB
 *    PFTEMP ------->	|       	无效内存 (*)			|	PTSIZE
 *                    	|                              	|
 *    UTEMP -------->	+-------------------------------+ 0x00400000	4MB	  ---+
 *                    	|       	无效内存 (*)			|                   	|
 *                    	| - - - - - - - - - - - - - - - |                   	|
 *                    	|  	用户STAB数据 (可选的)   	|	PTSIZE			|
 *    USTABDATA ---->	+-------------------------------+ 0x00200000	2MB		|
 *                    	|       	无效内存 (*)			|                   	|
 *    0 ------------> 	+-------------------------------+				  ---+
 *
 * (*) 注：内核将确保“无效内存”是从不映射.
 *     "无效内存" 通常是未映射，但用户程序如果需要，也会映射；
 *     JOS用户程序映射页面暂时在 UTEP
 */


// 在这个地址映射所有的物理内存
#define	KERNBASE	0xF0000000

// 在 IOPHYSMEM (640K)上，由 为I/O 保留的 384KB 内存空洞.
// 内核中，IOPHYSMEM 映射到内存地址 KERNBASE + IOPHYSMEM.
// 空洞结束处为物理地址 EXTPHYSMEM.
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

// 内核堆栈栈顶
#define KSTACKTOP	KERNBASE
#define KSTKSIZE		(8*PGSIZE)   		// 内核堆栈大小
#define KSTKGAP		(8*PGSIZE)   		// 内核堆栈预防的大小

// 内存映射 I/O
#define MMIOLIM		(KSTACKTOP - PTSIZE)
#define MMIOBASE		(MMIOLIM - PTSIZE)	// 内存映射 I/O 基地址

#define ULIM			(MMIOBASE)

/*
 * 用户只读映射! 在 UTOP 以下的内存地址对用户只读；
 * 他们是在ENV分配时，映射的全局网页
 */

// 用户只读虚拟页表 (see 'uvpt' below)
#define UVPT			(ULIM - PTSIZE)
// 只读页面结构的副本
#define UPAGES		(UVPT - PTSIZE)
// 只读全局的env结构的副本
#define UENVS		(UPAGES - PTSIZE)

/*
 * 用户VM的顶端，用户可以使用的虚拟地址是从 UTOP-1 往下
 */

// 顶端的用户可访问的 VM
#define UTOP			UENVS
// 栈顶第一页为用户异常栈
#define UXSTACKTOP	UTOP
// 下一页保留，防止异常栈溢出，然后才是普通用户栈
#define USTACKTOP	(UTOP - 2*PGSIZE)

// 常用的用户程序
#define UTEXT		(2*PTSIZE)

// 用于临时页面的映射.  Typed 'void*' for convenience
#define UTEMP		((void*) PTSIZE)
// 用于临时页映射用户页面错误处理 （不应该与其他临时页映射冲突）
#define PFTEMP		(UTEMP + PTSIZE - PGSIZE)
// 用户级的STABS数据结构的位置
#define USTABDATA	(PTSIZE / 2)

#ifndef __ASSEMBLER__

typedef uint32_t pte_t;
typedef uint32_t pde_t;

#if JOS_USER

/*
 对应于虚拟地址 [UVPT, UVPT + PTSIZE] 的页目录项指向页目录自己；
 因此，页目录被视为页表以及一个页面目录。
 将页目录视为页表，所有PTEs都能通过虚拟地址 UVPT处的虚拟页表来访问；
 页编号为 N的PTE 保存在 uvpt[N] 中。
 第二个结果是当前页目录的内容在虚拟地址 (UVPT + (UVPT >> PGSHIFT)) 处
 总是可见的，uvpd 在 lib/entry.S中设置。
 */
extern volatile pte_t uvpt[];	// 虚拟页表
extern volatile pde_t uvpd[];	// 当前页目录

#endif

/*
 页描述符结构，在 UPAGES映射；对内核可读/写，对用户只读；
 每个PageInfo结构体保存了一个物理页面的元数据；
 它并不是物理页面本身，但是PageInfo结构体与物理页面是一对一的关系；
 可以用 page2pa() 来映射一个PageInfo结构体到对应的物理地址。
 */
struct PageInfo {
	struct PageInfo *pp_link;	// 链表上下一个页面

	// pp_ref 是指向当前页的指针数（通常在页表项中），对于页面使用page_alloc分配；
	// 启动时的页面分配使用 pmap.c 中的 boot_alloc，它没有有效的引用计数字段。
	uint16_t pp_ref;
};

#endif /* !__ASSEMBLER__ */
#endif /* !JOS_INC_MEMLAYOUT_H */
