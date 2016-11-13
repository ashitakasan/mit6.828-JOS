#include <inc/syscall.h>
#include <inc/lib.h>

static inline int32_t syscall(int num, int check, 
				uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5){
	int32_t ret;

	// 通用系统调用：在AX中传递系统调用号，在DX，CX，BX，DI，SI中最多有五个参数，使用T_SYSCALL中断内核
	// volatile 告诉汇编器不要优化这个指令，因为我们不使用返回值
	// 最后一个子句告诉汇编器这可能会更改条件代码和任意内存位置
	asm volatile("int %1\n"
		     : "=a" (ret)
		     : "i" (T_SYSCALL),
		       "a" (num),
		       "d" (a1),
		       "c" (a2),
		       "b" (a3),
		       "D" (a4),
		       "S" (a5)
		     : "cc", "memory");

	if(check && ret > 0)
		panic("syscall %d returned %d (> 0)", num, ret);
	return ret;
}

void sys_cputs(const char *s, size_t len){
	syscall(SYS_cputs, 0, (uint32_t)s, len, 0, 0, 0);
}

int sys_cgetc(void){
	return syscall(SYS_cgetc, 0, 0, 0, 0, 0, 0);
}

envid_t sys_getenvid(void){
	return syscall(SYS_getenvid, 0, 0, 0, 0, 0, 0);
}

int sys_env_destroy(envid_t envid){
	return syscall(SYS_env_destroy, 1, envid, 0, 0, 0, 0);
}

// LAB 4

int sys_page_alloc(envid_t envid, void *va, int perm){
	return syscall(SYS_page_alloc, 1, envid, (uint32_t) va, perm, 0, 0);
}

int sys_page_map(envid_t srcenv, void *srcva, envid_t dstenv, void *dstva, int perm){
	return syscall(SYS_page_map, 1, srcenv, (uint32_t) srcva, dstenv, (uint32_t) dstva, perm);
}

int sys_page_unmap(envid_t envid, void *va){
	return syscall(SYS_page_unmap, 1, envid, (uint32_t) va, 0, 0, 0);
}

// sys_exofork is inlined in lib.h

int sys_env_set_status(envid_t envid, int status){
	return syscall(SYS_env_set_status, 1, envid, status, 0, 0, 0);
}

int sys_env_set_pgfault_upcall(envid_t envid, void *upcall){
	return syscall(SYS_env_set_pgfault_upcall, 1, envid, (uint32_t) upcall, 0, 0, 0);
}

int sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, int perm){
	return syscall(SYS_ipc_try_send, 0, envid, value, (uint32_t) srcva, perm, 0);
}

int sys_ipc_recv(void *dstva){
	return syscall(SYS_ipc_recv, 1, (uint32_t)dstva, 0, 0, 0, 0);
}
