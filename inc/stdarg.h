/*	$NetBSD: stdarg.h,v 1.12 1995/12/25 23:15:31 mycroft Exp $	*/

#ifndef JOS_INC_STDARG_H
#define	JOS_INC_STDARG_H

/**
 * 可变参数实现原理：
 * 1) 根据函数参数列表的最后一个已知类型的参数，得到参数列表的第一个参数
 * 2) 根据调用者指定的每个参数类型，通过地址及参数类型的size获取该参数
 * 3) 遍历，直到访问完整个可变参数列表
 * 可变参数的实现，主要依赖于函数调用栈，以及调用者指定的参数类型
 * 函数调用时，将参数列表从后往前依次压入堆栈，所以需要先获取最后一个参数
 */

// 声明一个指向参数列表的 字符型指针变量
typedef __builtin_va_list va_list;

/**
 * 功能： 宏va_arg()用于给函数传递可变长度的参数列表。
 * 首先，必须调用va_start() 传递有效的参数列表va_list和
 * 		函数强制的第一个参数。第一个参数代表将要传递的参数的个数；
 * 其次，调用va_arg()传递参数列表va_list 和将被返回的参数的类型，
 * 		va_arg()的返回值是当前的参数；
 * 再次，对所有的参数重复调用va_arg()；
 * 最后，调用va_end()传递va_list对完成后的清除是必须的。
 */

// 用于指定可变参数列表中参数的个数
// ap: 指向可变参数字符串的变量，last: 可变参数的第一个参数
#define va_start(ap, last) __builtin_va_start(ap, last)

// 用于给函数传递可变长度的参数列表
// ap: 指向可变参数字符串的变量，type: 可变参数的类型。
#define va_arg(ap, type) __builtin_va_arg(ap, type)

// 将存放可变参数字符串的变量清空
#define va_end(ap) __builtin_va_end(ap)

#endif
/* !JOS_INC_STDARG_H */
