// 用户支持库的主要的公共头文件，代码在 lib 文件夹中；
// 这个库大致是操作系统的标准C库，旨在链接到所有用户模式应用程序。

#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H 1

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/env.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>

#define USERD(x)			(void)(x)

// 主要的用户程序
void umain(int argc, char **argv);

// 在 libmain.c 或 entry.S 中
extern const char *binaryname;
extern const volatile struct Env *thisenv;
extern const volatile struct Env envs[NENV];
extern const volatile struct PageInfo pages[];

// exit.c
void exit(void);

// readline.c
char * readline(const char *buf);

// syscall.c
void sys_cputs(const char *string, size_t len);
int sys_cgetc(void);
envid_t sys_getenvid(void);
int sys_env_destroy(envid_t);

// 文件打开模式
#define O_RDONLY		0x0000		// 只读打开
#define O_WRONLY		0x0001		// 只写打开
#define O_RDWR		0x0002		// 读写打开
#define O_ACCMODE	0x0003		// 上边所有权限

#define O_CREAT		0x0100		// 如果不存在则创建
#define O_TRUNC		0x0200		// 截断为 0 长度
#define O_EXCL		0x0400		// 如果存在则报错
#define O_MKDIR		0x0800		// 创建目录，不是常规文件

#endif	// !JOS_INC_LIB_H
