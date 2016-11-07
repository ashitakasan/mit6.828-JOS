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
TRAPHANDLER_NOEC(traphandler8, 	8)
TRAPHANDLER_NOEC(traphandler9, 	9)
TRAPHANDLER_NOEC(traphandler10, 	10)
TRAPHANDLER_NOEC(traphandler11, 	11)
TRAPHANDLER_NOEC(traphandler12, 	12)
TRAPHANDLER(traphandler13, 	13)
TRAPHANDLER(traphandler14, 	14)
TRAPHANDLER_NOEC(traphandler15, 	15)
TRAPHANDLER_NOEC(traphandler16, 	16)
TRAPHANDLER_NOEC(traphandler17, 	17)
TRAPHANDLER_NOEC(traphandler18, 	18)
TRAPHANDLER_NOEC(traphandler19,	19)
TRAPHANDLER_NOEC(traphandler20, 	20)
TRAPHANDLER_NOEC(traphandler21, 	21)
TRAPHANDLER_NOEC(traphandler22, 	22)
TRAPHANDLER_NOEC(traphandler23, 	23)
TRAPHANDLER_NOEC(traphandler24, 	24)
TRAPHANDLER_NOEC(traphandler25, 	25)
TRAPHANDLER_NOEC(traphandler26, 	26)
TRAPHANDLER_NOEC(traphandler27, 	27)
TRAPHANDLER_NOEC(traphandler28, 	28)
TRAPHANDLER_NOEC(traphandler29, 	29)
TRAPHANDLER_NOEC(traphandler30, 	30)
TRAPHANDLER_NOEC(traphandler31, 	31)
TRAPHANDLER_NOEC(traphandler32, 	32)
TRAPHANDLER_NOEC(traphandler33, 	33)
TRAPHANDLER_NOEC(traphandler34, 	34)
TRAPHANDLER_NOEC(traphandler35, 	35)
TRAPHANDLER_NOEC(traphandler36, 	36)
TRAPHANDLER_NOEC(traphandler37, 	37)
TRAPHANDLER_NOEC(traphandler38, 	38)
TRAPHANDLER_NOEC(traphandler39, 	39)
TRAPHANDLER_NOEC(traphandler40, 	40)
TRAPHANDLER_NOEC(traphandler41, 	41)
TRAPHANDLER_NOEC(traphandler42, 	42)
TRAPHANDLER_NOEC(traphandler43, 	43)
TRAPHANDLER_NOEC(traphandler44, 	44)
TRAPHANDLER_NOEC(traphandler45, 	45)
TRAPHANDLER_NOEC(traphandler46, 	46)
TRAPHANDLER_NOEC(traphandler47, 	47)
TRAPHANDLER_NOEC(traphandler48, 	48)
TRAPHANDLER_NOEC(traphandler49, 	49)

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
	.long	traphandler20
	.long	traphandler21
	.long	traphandler22
	.long	traphandler23
	.long	traphandler24
	.long	traphandler25
	.long	traphandler26
	.long	traphandler27
	.long	traphandler28
	.long	traphandler29
	.long	traphandler30
	.long	traphandler31
	.long	traphandler32
	.long	traphandler33
	.long	traphandler34
	.long	traphandler35
	.long	traphandler36
	.long	traphandler37
	.long	traphandler38
	.long	traphandler39
	.long	traphandler40
	.long	traphandler41
	.long	traphandler42
	.long	traphandler43
	.long	traphandler44
	.long	traphandler45
	.long	traphandler46
	.long	traphandler47
	.long	traphandler48
	.long	traphandler49
