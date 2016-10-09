#ifndef JOS_INC_MEMLAYOUT_H
#define JOS_INC_MEMLAYOUT_H

#ifndef __ASSEMBLER__
#include <inc/types.h>
#include <inc/mmu.h>
#endif /* not __ASSEMBLER__ */

/*
 * 该文件定义了我们的OS的内存管理，这是有关内核和用户模式软件
 */

// 全局数字描述
#define GD_KT     0x08     // 内核文本
#define GD_KD     0x10     // 内核数据
#define GD_UT     0x18     // 用户文本
#define GD_UD     0x20     // 用户数据
#define GD_TSS0   0x28     // CPU 0 的 任务段选择

/*
 * 		虚拟内存映射:                                		权限
 *                                                    		内核/用户
 *
 *    4 Gig -------->  +------------------------------+
 *                     |                           	| 	RW/--
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     :              .               :
 *                     :              .               :
 *                     :              .               :
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| 	RW/--
 *                     |                            	| 	RW/--
 *                     |   	重新映射的物理内存			| 	RW/--
 *                     |                         		| 	RW/--
 *    KERNBASE, ----> +-------------------------------+ 0xf0000000    --+
 *    KSTACKTOP    	|     CPU0's 内核堆栈	      	|	RW/--KSTKSIZE 	|
 *                     | - - - - - - - - - - - - - - -	|                   	|
 *                     |     	无效内存 (*)  			| --/--  KSTKGAP   	|
 *                     +-------------------------------+                    	|
 *                     |    CPU1's 内核堆栈			| RW/--  KSTKSIZE 	|
 *                     | - - - - - - - - - - - - - - -	|         PTSIZE
 *                     |      	无效内存 (*)      		| --/--  KSTKGAP 	|
 *                     +-------------------------------+                   	|
 *                     :              .                :                   	|
 *                     :              .                :                   	|
 *    MMIOLIM ------> +-------------------------------+ 0xefc00000      --+
 *                     |       内存映射 I/O     		| 	RW/--  PTSIZE
 * ULIM, MMIOBASE -->+-------------------------------+ 0xef800000
 *                     |  	Cur. 页表 (User R-)   	| 	R-/R-  PTSIZE
 *    UVPT      ----> +-------------------------------+ 0xef400000
 *                     |          RO PAGES            | 	R-/R-  PTSIZE
 *    UPAGES    ----> +-------------------------------+ 0xef000000
 *                     |           RO ENVS            | 	R-/R-  PTSIZE
 * UTOP,UENVS ------> +-------------------------------+ 0xeec00000
 * UXSTACKTOP -/     	|     	用户异常堆栈     		| 	RW/RW  PGSIZE
 *                     +-------------------------------+ 0xeebff000
 *                     |       	无效内存 (*) 			| --/--  PGSIZE
 *    USTACKTOP  ---> +-------------------------------+ 0xeebfe000
 *                     |      	普通用户堆栈			| 	RW/RW  PGSIZE
 *                     +-------------------------------+ 0xeebfd000
 *                     |                              	|
 *                     |                              	|
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     .                              .
 *                     .                              .
 *                     .                              .
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
 *                     |     	程序数据和堆      		|
 *    UTEXT --------> +-------------------------------+ 0x00800000
 *    PFTEMP -------> |       	无效内存 (*)			|	PTSIZE
 *                     |                              	|
 *    UTEMP --------> +-------------------------------+ 0x00400000      --+
 *                     |       	无效内存 (*)			|                   	|
 *                     | - - - - - - - - - - - - - - - |                   	|
 *                     |  	用户STAB数据 (可选的)   	|	PTSIZE
 *    USTABDATA ----> +-------------------------------+ 0x00200000        	|
 *                     |       	无效内存 (*)			|                   	|
 *    0 ------------>  +-------------------------------+                 --+
 *
 * (*) 注：内核将确保“无效内存”是从不映射.
 *     "无效内存" 通常是未映射，但用户程序可能如果需要，也映射；
 *     JOS用户程序映射页面暂时在UTEP
 */


// 在这个地址映射所有的物理内存
#define	KERNBASE	0xF0000000

// 在 IOPHYSMEM (640K)上，由 为I/O 保留的 384KB 内存空洞.
// 内核中，IOPHYSMEM 映射到内存地址 KERNBASE + IOPHYSMEM.
// 空洞结束处为物理地址 EXTPHYSMEM.
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

// 内核堆栈.
#define KSTACKTOP	KERNBASE
#define KSTKSIZE		(8*PGSIZE)   		// 内核堆栈大小
#define KSTKGAP		(8*PGSIZE)   		// 内核堆栈预防的大小

// 内存映射 IO.
#define MMIOLIM		(KSTACKTOP - PTSIZE)
#define MMIOBASE		(MMIOLIM - PTSIZE)

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
// Top of one-page user exception stack
#define UXSTACKTOP	UTOP
// Next page left invalid to guard against exception stack overflow; then:
// Top of normal user stack
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

#endif /* !__ASSEMBLER__ */
#endif /* !JOS_INC_MEMLAYOUT_H */
