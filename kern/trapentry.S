/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER定义了一个全局可见的函数来处理陷阱。 它将陷阱号码推入堆栈，然后跳转到 _alltraps。 
	对于CPU自动推送错误代码的陷阱使用 TRAPHANDLER 。
   你不应该从C调用 TRAPHANDLER 函数，
 	但是您可能需要在C中声明一个（例如，在IDT设置期间获取一个函数指针）。
 	你可以用void NAME() 声明函数; 其中NAME是传递给 TRAPHANDLER 的参数.
 */
#define TRAPHANDLER(name, num)									\
	.globl name;				/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */			\
	.align 2;				/* align function definition */		\
	name:					/* function starts here */			\
	pushl $(num);												\
	jmp _alltraps

/* 对于CPU不推送错误代码的陷阱使用 TRAPHANDLER_NOEC。 
	它推送0代替错误代码，因此陷阱框架在任何情况下具有相同的格式。
 */
#define TRAPHANDLER_NOEC(name, num)				\
	.globl name;									\
	.type name, @function;						\
	.align 2;									\
	name:										\
	pushl $0;									\
	pushl $(num);								\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(traphandler0, 	0)
TRAPHANDLER_NOEC(traphandler1, 	1)
TRAPHANDLER_NOEC(traphandler2, 	2)
TRAPHANDLER_NOEC(traphandler3, 	3)
TRAPHANDLER_NOEC(traphandler4, 	4)
TRAPHANDLER_NOEC(traphandler5, 	5)
TRAPHANDLER_NOEC(traphandler6, 	6)
TRAPHANDLER_NOEC(traphandler7, 	7)
TRAPHANDLER(traphandler8, 	8)
TRAPHANDLER_NOEC(traphandler9, 	9)
TRAPHANDLER(traphandler10, 	10)
TRAPHANDLER(traphandler11, 	11)
TRAPHANDLER(traphandler12, 	12)
TRAPHANDLER(traphandler13, 	13)
TRAPHANDLER(traphandler14, 	14)
TRAPHANDLER_NOEC(traphandler15, 	15)
TRAPHANDLER_NOEC(traphandler16, 	16)
TRAPHANDLER(traphandler17, 	17)
TRAPHANDLER_NOEC(traphandler18, 	18)
TRAPHANDLER_NOEC(traphandler19,	19)

/*
 * Lab 3: Your code here for _alltraps
 */

.globl _alltraps
_alltraps:
	pushl	%ds
	pushl	%es
	pushal

	movw	$GD_KD, %ax
	movw	%ax, %ds
	movw	%ax, %es

	pushl	%esp
	call trap


.data

.globl	trap_handlers
trap_handlers:
	.long	traphandler0
	.long	traphandler1
	.long	traphandler2
	.long	traphandler3
	.long	traphandler4
	.long	traphandler5
	.long	traphandler6
	.long	traphandler7
	.long	traphandler8
	.long	traphandler9
	.long	traphandler10
	.long	traphandler11
	.long	traphandler12
	.long	traphandler13
	.long	traphandler14
	.long	traphandler15
	.long	traphandler16
	.long	traphandler17
	.long	traphandler18
	.long	traphandler19
