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
#include <inc/trap.h>

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

// pgfault.c
void set_pgfault_handler(void (*handler)(struct UTrapframe *utf));

// readline.c
char *readline(const char *buf);

// syscall.c
void sys_cputs(const char *string, size_t len);
int sys_cgetc(void);
envid_t sys_getenvid(void);
int sys_env_destroy(envid_t);

void sys_yield(void);
static envid_t sys_exofork(void);
int sys_env_set_status(envid_t env, int status);
int sys_env_set_pgfault_upcall(envid_t env, void *upcall);
int sys_page_alloc(envid_t env, void *pg, int perm);
int sys_page_map(envid_t src_env, void *stc_pg, envid_t dst_env, void *dst_pg, int perm);
int sys_page_unmap(envid_t env, void *pg);
int sys_ipc_try_send(envid_t to_env, uint32_t value, void *pg, int perm);
int sys_ipc_recv(void *rcv_pg);

// 这里必须是内联函数
static inline envid_t __attribute__((always_inline)) sys_exofork(void){
	envid_t ret;
	asm volatile("int %2"
				: "=a" (ret)
				: "a" (SYS_exofork), "i" (T_SYSCALL));
	return ret;
}

// ipc.c
void ipc_send(envid_t to_env, uint32_t value, void *pg, int perm);
int32_t ipc_recv(envid_t *from_env_store, void *pg, int *perm_store);
envid_t ipc_find_env(enum EnvType type);

// fork.c
#define PTE_SHARE 0x400
envid_t fork(void);
envid_t sfork(void);


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
