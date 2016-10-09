#ifndef JOS_KERN_KDEBUG_H
#define JOS_KERN_KDEBUG_H

#include <inc/types.h>

// 关于特殊的指令指针EIP 的调试信息
struct Eipdebuginfo {
	const char *eip_file;		// 当前EIP 的源文件名
	int eip_line;				// 当前EIP 的源文件行

	const char *eip_fn_name;		// 当前EIP 的函数名
	int eip_fn_namelen;			// 当前EIP 距离函数开始的汇编地址长度

	uintptr_t eip_fn_addr;		// 当前EIP 函数的起始地址
	int eip_fn_narg;				// 当前EIP 的函数的参数个数
};

int debuginfo_eip(uintptr_t eip, struct Eipdebuginfo *info);

#endif
