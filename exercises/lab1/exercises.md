## Exercise 5.

修改boot/Makefrag文件中的 -Ttext 0x7C00 为 0x7A00；
编译后的代码基本不变，boot.S 的起始地址依然在 0x7C00 地址处，但是由于编译器设置代码段链接地址为 0x7A00，程序从这里开始执行，于是就出现了程序执行错误，系统启动引导失败
Triple fault.  Halting for inspection via QEMU monitor.


## Exercise 7.

```ASM
(gdb) x/10i 0x10000c
=> 0x10000c:    movw   $0x1234,0x472        # kernel start
   0x100015:    mov    $0x110000,%eax
   0x10001a:    mov    %eax,%cr3
   0x10001d:    mov    %cr0,%eax
   0x100020:    or     $0x80010001,%eax
   0x100025:    mov    %eax,%cr0            # turn on paging
   0x100028:    mov    $0xf010002f,%eax     # 内存地址在这里会转换
   0x10002d:    jmp    *%eax
   0x10002f:    mov    $0x0,%ebp
   0x100034:    mov    $0xf0110000,%esp
```
在执行 mov %eax,%cr0 之前，0x00100000 与 0xF0100000 值不同，
执行后，0x00100000 映射到 0xF0100000 上，二者值相同


## Exercise 9.

kernel开始执行后，第一个执行的代码为：
```ASM
movw    $0x1234,0x472				# warm boot
```
此时还没有虚拟地址，所以 kernel现在在 bootloader所在的 1MB地址空间上执行，
但是kernel的C代码应该在 内存的高地址空间执行，因此需要通过地址转换，
将虚拟的高地址空间 [3840MB, 3844MB]映射到物理的低地址空间 [0, 4MB]。
kernel执行的第一步，就是将物理地址页表（entrypgdir.c）加载到 cr3寄存器中，启用页表；
然后从低地址空间跳到高地址空间，清除帧指针寄存器（EBP），设置设置栈指针，开始执行C代码。
```ASM
movl    $0x0,%ebp					# nuke frame pointer
movl    $(bootstacktop), %esp		# Set the stack pointer
call    i386_init				# now to C code
```


## Exercise 10.
test_backtrace 的汇编代码：
```ASM
(gdb) x/35i 0xf010003e
   0xf010003e <spin>:   jmp    0xf010003e <spin>
=> 0xf0100040 <test_backtrace>: push   %ebp
   0xf0100041 <test_backtrace+1>:       mov    %esp,%ebp
   0xf0100043 <test_backtrace+3>:       push   %ebx
   0xf0100044 <test_backtrace+4>:       sub    $0xc,%esp
   0xf0100047 <test_backtrace+7>:       mov    0x8(%ebp),%ebx
   0xf010004a <test_backtrace+10>:      push   %ebx
   0xf010004b <test_backtrace+11>:      push   $0xf0101800
   0xf0100050 <test_backtrace+16>:      call   0xf01008c8 <cprintf>
   0xf0100055 <test_backtrace+21>:      add    $0x10,%esp
   0xf0100058 <test_backtrace+24>:      test   %ebx,%ebx
   0xf010005a <test_backtrace+26>:      jle    0xf010006d <test_backtrace+45>
   0xf010005c <test_backtrace+28>:      sub    $0xc,%esp
   0xf010005f <test_backtrace+31>:      lea    -0x1(%ebx),%eax
   0xf0100062 <test_backtrace+34>:      push   %eax
   0xf0100063 <test_backtrace+35>:      call   0xf0100040 <test_backtrace>
   0xf0100068 <test_backtrace+40>:      add    $0x10,%esp
   0xf010006b <test_backtrace+43>:      jmp    0xf010007e <test_backtrace+62>
   0xf010006d <test_backtrace+45>:      sub    $0x4,%esp
   0xf0100070 <test_backtrace+48>:      push   $0x0
   0xf0100072 <test_backtrace+50>:      push   $0x0
   0xf0100074 <test_backtrace+52>:      push   $0x0
   0xf0100076 <test_backtrace+54>:      call   0xf0100740 <mon_backtrace>
   0xf010007b <test_backtrace+59>:      add    $0x10,%esp
   0xf010007e <test_backtrace+62>:      sub    $0x8,%esp
   0xf0100081 <test_backtrace+65>:      push   %ebx
   0xf0100082 <test_backtrace+66>:      push   $0xf010181c
   0xf0100087 <test_backtrace+71>:      call   0xf01008c8 <cprintf>
   0xf010008c <test_backtrace+76>:      add    $0x10,%esp
   0xf010008f <test_backtrace+79>:      mov    -0x4(%ebp),%ebx
   0xf0100092 <test_backtrace+82>:      leave  
   0xf0100093 <test_backtrace+83>:      ret
```
递归调用 test_backtrace 5次后，栈帧数据为：
当前栈帧：
```ASM
(gdb) info f
Stack level 0, frame at 0xf010ef60:
 eip = 0xf010004a in test_backtrace (kern/init.c:14); saved eip = 0xf0100068
 called by frame at 0xf010ef80
 source language c.
 Arglist at 0xf010ef58, args: x=1
 Locals at 0xf010ef58, Previous frame's sp is 0xf010ef60
 Saved registers:
  ebx at 0xf010ef54, ebp at 0xf010ef58, eip at 0xf010ef5c
```
可见其参数为 x=1，当前CPU指令 EIP=0xf0100040，上一栈帧CPU指令 EIP=0xf0100068，
即为递归调用前的指令地址；<br>
其中：
- frame at 0xf010ef60，表示当前栈帧的起始地址；
- called by frame at 0xf010ef80，表示上一栈帧起始地址，两者之差即上一栈帧大小：0x20 = 32B；
- Arglist at 0xf010ef58，表示参数被压到堆栈后，参数的地址；
- Locals at 0xf010ef58，表示存放函数局部变量的地址从0xf010ef58开始；
- Saved registers，栈帧中保存的寄存器的值，都在当前栈起始地址以下。

内存的全部调用栈：
```ASM
(gdb) bt
#0  test_backtrace (x=1) at kern/init.c:13
#1  0xf0100068 in test_backtrace (x=2) at kern/init.c:16
#2  0xf0100068 in test_backtrace (x=3) at kern/init.c:16
#3  0xf0100068 in test_backtrace (x=4) at kern/init.c:16
#4  0xf0100068 in test_backtrace (x=5) at kern/init.c:16
#5  0xf01000d4 in i386_init () at kern/init.c:39
#6  0xf010003e in relocated () at kern/entry.S:80
```
从kernel启动后，栈帧开始向下迭代；#0即为当前栈帧 <br>
relocated () --> i386_init () --> test_backtrace (x=5) --> ... <br>
其中 relocated () 为汇编代码块，i386_init () 为kernel 初始化 C函数。<br>

寄存器值：
```ASM
(gdb) info registers 
eax            0x1      1
ecx            0x3d4    980
edx            0x3d5    981
ebx            0x1      1
esp            0xf010ef48       0xf010ef48
ebp            0xf010ef58       0xf010ef58
esi            0x10094  65684
edi            0x0      0
eip            0xf010004a       0xf010004a <test_backtrace+10>
eflags         0x96     [ PF AF SF ]
cs             0x8      8
ss             0x10     16
ds             0x10     16
es             0x10     16
fs             0x10     16
gs             0x10     16
```
- EIP：当前程序运行的汇编地址
- EBX：这里保存了临时变量，即参数 x的值地址
- ESP：栈当前指针，每次调用函数传参、新建变量时会改变，该变量以下内存空间是未使用的
- EBP：参数及变量地址

此后，在递归函数一层层退出时，各个上层的栈帧先被恢复，再退出，最终退出递归。
