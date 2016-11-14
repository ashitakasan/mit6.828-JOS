// 用户级页面故障处理程序支持
// 不是直接用内核作为页错误处理程序来注册C页错误处理程序，
// 而是在 pfentry.S 中注册汇编语言包装程序，这又调用注册的C函数

#include <inc/lib.h>

// 汇编语言在 lib/pfentry.S 中定义的 pgfault entrypoint
extern void _pgfault_upcall(void);

// 指向当前安装的C语言 pgfault 处理程序的指针
void (*_pgfault_handler)(struct UTrapframe *utf);

/*
  设置页面错误处理函数
  如果还没有， _pgfault_handler 将为 0
  第一次注册处理程序时，我们需要分配一个异常堆栈 (一页内存，其顶部在 UXSTACKTOP)，
  并告诉内核在发生页面故障时调用汇编语言 _pgfault_upcall 例程
 */
void set_pgfault_handler(void (*handler)(struct UTrapframe *utf)){
	int r;

	if(_pgfault_handler == 0){
		// LAB 4
		void *va = (void *)(UXSTACKTOP - PGSIZE);
		envid_t envid = sys_getenvid();

		if(sys_page_alloc(envid, va, PTE_P | PTE_U | PTE_W) < 0)
			panic("Unable to allocate memory for pgfault expection stack\n");

		if(sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0)
			panic("set_pgfault_handler: can't set exception handler\n");
	}

	// 保存处理程序指针以供程序集调用
	_pgfault_handler = handler;
}
