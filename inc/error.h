/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ERROR_H
#define JOS_INC_ERROR_H

enum {
	// 内核错误提示，与 lib/printfmt.c 同步
	E_UNSPECIFIED	= 1,	// 未知错误
	E_BAD_ENV,			// 环境不存在或以其他方式不能在要求的操作中使用
	E_INVAL,				// 参数错误
	E_NO_MEM	,			// 请求失败，由于内存不足
	E_NO_FREE_ENV,		// 尝试创建一个新的环境超出允许的最大值
	E_FAULT,				// 内存错误
	E_IPC_NOT_RECV,		// 尝试发送到 env 没有接收
	E_EOF,				// 文件意外结束

	E_NO_DISK,			// 磁盘没有剩余空间
	E_MAX_OPEN,			// 打开太多文件
	E_NOT_FOUND, 		// 文件或块没有找到
	E_BAD_PATH,			// 错误路径
	E_FILE_EXISTS,		// 文件已经存在
	E_NOT_EXEC,			// 文件不是有效的可执行文件
	E_NOT_SUPP,			// 不支持的操作

	MAXERROR
};

#endif
// !JOS_INC_ERROR_H */
