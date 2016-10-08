#ifndef JOS_STAB_H
#define JOS_STAB_H
#include <inc/types.h>

// <inc/stab.h>
// STABS debugging info

// JOS内核调试器能够理解的格式STABS一些调试信息

// 下面的常量定义各种调试器和编译器使用的一些符号类型
// JOS 使用 N_SO, N_SOL, N_FUN, 以及 N_SLINE 类型.

#define	N_GSYM		0x20		// global symbol
#define	N_FNAME		0x22		// F77 function name
#define	N_FUN		0x24		// procedure name
#define	N_STSYM		0x26		// data segment variable
#define	N_LCSYM		0x28		// bss segment variable
#define	N_MAIN		0x2a		// main function name
#define	N_PC			0x30		// global Pascal symbol
#define	N_RSYM		0x40		// register variable
#define	N_SLINE		0x44		// text segment line number
#define	N_DSLINE		0x46		// data segment line number
#define	N_BSLINE		0x48		// bss segment line number
#define	N_SSYM		0x60		// structure/union element
#define	N_SO			0x64		// main source file name
#define	N_LSYM		0x80		// stack variable
#define	N_BINCL		0x82		// include file beginning
#define	N_SOL		0x84		// included source file name
#define	N_PSYM		0xa0		// parameter variable
#define	N_EINCL		0xa2		// include file end
#define	N_ENTRY		0xa4		// alternate entry point
#define	N_LBRAC		0xc0		// left bracket
#define	N_EXCL		0xc2		// deleted include file
#define	N_RBRAC		0xe0		// right bracket
#define	N_BCOMM		0xe2		// begin common
#define	N_ECOMM		0xe4		// end common
#define	N_ECOML		0xe8		// end common (local name)
#define	N_LENG		0xfe		// length of preceding entry

// STABS 表.
struct Stab {
	uint32_t n_strx;			// 字符串表名索引
	uint8_t n_type;			// 符号类型
	uint8_t n_other;			// misc info (usually empty)
	uint16_t n_desc;			// 描述字段
	uintptr_t n_value;		// 符号值
};

#endif /* !JOS_STAB_H */
