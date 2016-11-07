## JOS 执行用户函数输出 "hello, world!\n" 全过程

#### 第一步，内核初始化，包括几个初始化步骤

1. `start (kern/entry.S)`		在这里加载内核，读取整个内核 ELF文件 到内存中，进入保护模式，启用分页
2. `i386_init (kern/init.c)`	初始化内核，包括几个方面的初始化
  - `cons_init`				终端初始化，以便于在屏幕上打印运行状态信息
  - `mem_init`				内存初始化，包括读取内存总量，初始化内核页目录，为页目录、内核和用户页结构、内核地址空间、内核栈的 虚拟地址映射
  - `env_init`				系统运行环境初始化，初始化 envs 数组
  - `trap_init`				陷阱门初始化，初始化 IDT，设置几个重要的中断描述符，设置 DPL，设置中断处理函数
  - `env_create`				创建用户环境，分配用户页目录，将内核文件映射到用户进程的 内核地址空间
  - `env_run`				运行第一个用户环境，通过 iret 跳转到用户地址空间开始执行

#### 第二步，用户程序运行及系统调用

1. entry.S					用户进程中运行的第一个文件就是 entry.S, 其设置几个重要的变量，包括 envs、pages、uvpt 等，然后跳转到 libmain
2. libmain					libmain 第一步就要通过 sys_getenvid() 获取 envid，以便于获取自己的运行状态，sys_getenvid() 会发起系统调用
3. syscall					sys_getenvid() 调用 syscall() 以发起 int 30 的系统调用
  - `lib/syscall.c`			syscall() 通过内联编译到 sys_getenvid() 中，然后带着调用参数发起系统调用 int 30
  - `trapentry.S`			系统调用第一步就是根据 中断号(这里是48) 从 IDT 中查找 中断处理函数，然后执行 trap
  - `trap`					trap 首先将当前运行环境保存起来，以便于中断处理函数执行完成后 恢复用户程序执行，然后交给 中断处理分发函数
  - `trap_dispatch`			trap_dispatch 根据中断号 执行不同的处理，对于 系统调用，则执行 内核系统调用 kern/syscall.c
  - `kern/syscall`			内核syscall 根据 系统调用号码，执行不同的内核函数，并将返回值保存起来
  - `trap`					内核syscall 执行完成后，开始恢复用户函数执行
  - `env_run`				设置 cr3寄存器 为当前环境页目录，并通过 env_pop_tf 恢复保存的用户环境寄存器，通过 iret 指令跳转到用户环境继续执行
  - `libmain`				用户程序获取系统调用返回值，这里是 获取环境ID

#### 第三步，执行系统调用输出

1. umain						每个用户程序都实现了 umain，在这里执行用户输出打印函数
2. cprintf					用户环境的输出函数，在这里加了 用户输出缓冲区 printbuf，避免过多的发起系统调用；<br>
							然后通过 sys_cputs 发起系统调用，通过 内核输出函数 kern/printf 输出用户字符串
3. exit						用户函数执行完成后，要通过 exit() 发起系统调用，结束用户进程
  - `sys_env_destroy`		获取用户环境ID，用 env_destroy() 结束用户程序
  - `env_free`				将当前使用的 env 结构归还给内核 envs，释放 env 使用的系统内存，包括页目录、页表、页，设置 env 的运行状态、运行次数
  - `monitor`				执行系统监控程序
