## Exercise 11. Make sure you understand why user/faultalloc and user/faultallocbad behave differently

make run-faultalloc 显示：
```
6828 decimal is 15254 octal!
Physical memory: 131072K available, base = 640K, extended = 130432K
check_page_free_list() succeeded!
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
region_alloc env[00001000] at va = 00200000 with len = 18607
region_alloc env[00001000] at va = 00800020 with len = 5194
region_alloc env[00001000] at va = 00802000 with len = 12
region_alloc env[00001000] at va = eebfd000 with len = 4096
---------------user process starts----------------
fault deadbeef
this string was faulted in at deadbeef
fault cafebffe
fault cafec000
this string was faulted in at cafebffe
[00001000] exiting gracefully
[00001000] free env 00001000
No runnable environments in the system!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
```

make run-faultallocbad 显示：
```
6828 decimal is 15254 octal!
Physical memory: 131072K available, base = 640K, extended = 130432K
check_page_free_list() succeeded!
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
region_alloc env[00001000] at va = 00200000 with len = 18598
region_alloc env[00001000] at va = 00800020 with len = 5162
region_alloc env[00001000] at va = 00802000 with len = 12
region_alloc env[00001000] at va = eebfd000 with len = 4096
---------------user process starts----------------
[00001000] user_mem_check assertion failure for va deadbeef
[00001000] free env 00001000
No runnable environments in the system!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands
```

原因：
1. faultallocbad 仅显示 `user_mem_check assertion failure`，
用户函数 sys_cputs() 直接执行了系统调用，系统执行 kern/syscall.c 中的 sys_cputs() 时，进行内存访问权限检查时，发现用户不能访问 0xdeadbeef 这个地址，于是直接报错，退出用户进程；

2. faultalloc 在 0xDeadBeef 上打印一条 fault 信息，在 0xCafeBffe 上打印两条；在这里，用户进程是正常运行的，没有出错。
因为 faultalloc 中，用户进程调用了 cprintf 函数，这个用户级函数有输出缓冲区：
```C
int vcprintf(const char *fmt, va_list ap){
	struct printbuf b;

	b.idx = 0;
	b.cnt = 0;
	vprintfmt((void*)putch, &b, fmt, ap);
	sys_cputs(b.buf, b.idx);

	return b.cnt;
}
```
缓冲区 printbuf 在正常的用户栈上创建，在第一次写入该 buf 时，系统发现用户程序没有写入权限，于是发生 T_PGFLT 中断，由于用户程序此前已经在 env 上注册了 用户级页错误处理程序，系统将通过以下流程调用用户处理程序：<br>
`_alltraps --> trap() --> trap_dispatch() --> page_fault_handler() --> env_pgfault_upcall() --> handler()`<br>
用户处理程序 handler 尝试重新 sys_page_alloc() 分配一页内存页，并用 snprintf() 填充输出缓冲区（填充这个步骤不是必须），然后恢复用户程序执行。

faultalloc 在 0xDeadBeef 上打印一条 fault 信息，在 0xCafeBffe 上打印两条 fault 信息，<br>
这是因为在 0xDeadBeef 上，分配的内存页起始虚拟地址为：0xdeadb000，从 0xDeadBeef 开始，该页面仍然有 273 字节可用，对于 snprintf 要填充 100 字节够用了；<br>
然而在 0xCafeBffe 上，重新分配的内存页起始虚拟地址为： 0xcafeb000，从 0xCafeBffe 开始该页面还有 2 字节可用，此时在 snprintf 填充缓冲区时，将再次发生 页访问中断 T_PGFLT，再次调用 用户页错误handler，重新分配一页内存，然后恢复用户程序执行。
