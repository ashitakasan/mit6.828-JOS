### 1. At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?

#cr0置1，cr0中第0位为1表示处于保护模式，为0表示处于实模式
movl	%eax, %cr0

#跳转到32位模式中的下一条指令，将处理请切换为32位工作模式
#下边的指令会将 $PORT_MODE_CSEG（0x8） 加载到CS中，
#CS对应的高速缓存储存器会加载代码段描述符，同样将 $protcseg 加载到 IP 中
ljmp	$PORT_MODE_CSEG, $protcseg


### 2. What is the last instruction of the boot loader executed, and what is the first instruction of the kernel it just loaded?

for (; ph < eph; ph++)

#((void (*)(void)) (ELFHDR->e_entry))();
call   *(%di)		# call *0x10018


### 3. Where is the first instruction of the kernel?

#call *0x10018
#x/10i *0x10018
movw   $0x1234,0x472


### 4. How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?

ELF格式内核镜像的前4096个字节，记录了内核的格式信息、大小等。



## Exercise 5.

修改boot/Makefrag文件中的 -Ttext 0x7C00 为 0x7A00；
编译后的代码基本不变，boot.S 的起始地址依然在 0x7C00 地址处，但是由于编译器设置代码段链接地址为 0x7A00，程序从这里开始执行，于是就出现了程序执行错误，系统启动引导失败
Triple fault.  Halting for inspection via QEMU monitor.
