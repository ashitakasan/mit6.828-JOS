#ifndef JOS_INC_TYPES_H
#define JOS_INC_TYPES_H

#ifndef NULL
#define NULL ((void*) 0)
#endif

// 表示 True 和 False
typedef _Bool bool;
enum {false, true};

// 各种int类型重新定义
typedef __signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

// 指针和地址都是32位，我们用指针类型重定义虚拟地址
// 用 uintptr_t 表示数值形的虚拟地址，用 physaddr_t 表示物理地址
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef uint32_t physaddr_t;

// 内存页的编号是32位的
typedef uint32_t ppn_t;

// size_t 用来表示内存对象大小
typedef uint32_t size_t;

// ssize_t 是有符号版本的 size_t，用来表示函数的错误返回值
typedef int32_t ssize_t;

// off_t 表示文件的偏移量和大小
typedef int32_t off_t;


// 最小值和最大值操作，typeof() 操作符返回参数类型，并不执行
#define MIN(_a, _b)			\
({							\
	typeof(_a)	__a = (_a);	\
	typeof(_b)	__b = (_b);	\
	__a <= __b ? __a : __b;	\
})
#define MAX(_a, _b)			\
({							\
	typeof(_a)	__a = (_a);	\
	typeof(_b)	__b = (_b);	\
	__a >= __b ? __a : __b;	\
})

// 舍入操作（当n是2的幂时很有用）
// 向下取整到 n 的整数倍
#define ROUNDDOWN(a, n)				\
({									\
	uint32_t __a = (uint32_t) (a);		\
	(typeof(a)) (__a - __a % (n));		\
})
// 向上取整到 n 的整数倍
#define ROUNDUP(a, n)				\
({									\
	uint32_t __n = (uint32_t) (n);		\
	(typeof(a)) (ROUNDDOWN((uint32_t) (a) + __n - 1, __n));	\
})

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

// 返回结构体 type 的成员变量 member 相对于结构体开头的偏移量
#define offsetof(type, member)	((size_t) (&((type*)0)->member))

#endif
/* !JOS_INC_TYPES_H */
