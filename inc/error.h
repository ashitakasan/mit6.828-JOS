/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ERROR_H
#define JOS_INC_ERROR_H

enum {
	// 内核错误提示，与 lib/printfmt.c 同步
	E_UNSPECIFIED	= 1,	// 未知错误
	E_BAD_ENV,	// 环境不存在或以其他方式不能在要求的操作中使用
	E_INVAL,				// 参数错误
	E_NO_MEM	,			// 请求失败，由于内存不足
	E_NO_FREE_ENV,		// 尝试创建一个新的环境超出允许的最大值
	E_FAULT,				// 内存错误
	MAXERROR
};

#endif
// !JOS_INC_ERROR_H */
