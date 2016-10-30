## 1. Begin by restarting qemu and gdb, and set a break-point at 0x7c00, the start of the boot block (bootasm.S). Single step through the instructions (type si at the gdb prompt). Where in bootasm.S is the stack pointer initialized? (Single step until you see an instruction that moves a value into %esp, the register for the stack pointer.)

```ASM
   0x7c40:      mov    $0x7c00,%sp
   0x7c43:      add    %al,(%bx,%si)
   0x7c45:      call   0x7d06
```
在 0x7c40 处，设置栈指针 esp，开始调用 Ｃ代码，bootmain()引导程序


## 2. Single step through the call to bootmain; what is on the stack now?

```
(gdb) x/10i 0x7d08
=> 0x7d08:      push   %ebp
   0x7d09:      mov    %esp,%ebp
   0x7d0b:      push   %esi
   0x7d0c:      push   %ebx
   0x7d0d:      push   $0x0
   0x7d0f:      push   $0x1000
   0x7d14:      push   $0x10000
   0x7d19:      call   0x7ccf
   0x7d1e:      add    $0xc,%esp
   0x7d21:      cmpl   $0x464c457f,0x10000

(gdb) x/10x $esp 
0x7bfc: 0x00007c4a      0xc031fcfa      0xc08ed88e      0x64e4d08e
0x7c0c: 0xfa7502a8      0x64e6d1b0      0x02a864e4      0xdfb0fa75
0x7c1c: 0x010f60e6      0x0f7c6416
(gdb) si
=> 0x7d09:      mov    %esp,%ebp
0x00007d09 in ?? ()
(gdb) x/10x $esp
0x7bf8: 0x00000000      0x00007c4a      0xc031fcfa      0xc08ed88e
0x7c08: 0x64e4d08e      0xfa7502a8      0x64e6d1b0      0x02a864e4
0x7c18: 0xdfb0fa75      0x010f60e6
```
bootmain 的汇编代码，此时栈帧为空，在每次执行 push 指令时，数据都会压入栈。
0x7bfc为栈帧结束地址。


## 3. What do the first assembly instructions of bootmain do to the stack? Look for bootmain in bootblock.asm.

```ASM
(gdb) x/10i 0x7d08
=> 0x7d08:      push   %ebp
   0x7d09:      mov    %esp,%ebp
   0x7d0b:      push   %esi
   0x7d0c:      push   %ebx
```
即 push %ebp，将段基地址压入堆栈


## 4. Continue tracing via gdb (using breakpoints if necessary -- see hint below) and look for the call that changes eip to 0x10000c. What does that call do to the stack? (Hint: Think about what this call is trying to accomplish in the boot sequence and try to identify this point in bootmain.c, and the corresponding instruction in the bootmain code in bootblock.asm. This might help you set suitable breakpoints to speed things up.)

```ASM
(gdb) x/10i 0x7d5e
=> 0x7d5e:      call   *0x10018
   0x7d64:      mov    $0x8a00,%edx
   0x7d69:      mov    $0xffff8a00,%eax
   0x7d6e:      out    %ax,(%dx)
   0x7d70:      mov    $0xffff8e00,%eax
   0x7d75:      out    %ax,(%dx)
   0x7d77:      jmp    0x7d77
   0x7d79:      add    %al,(%eax)
   0x7d7b:      add    %al,(%eax)
   0x7d7d:      add    %al,(%eax)
```
运行到 0x7d5e 时，eip = 0x7d5e；<br>
单步，程序运行到 0x10000c，eip = 0x10000c；<br>
```ASM
(gdb) x/24x $esp
0x7bec: 0x00007d64      0x00000000      0x00000000      0x00000000
0x7bfc: 0x00007c4a      0xc031fcfa      0xc08ed88e      0x64e4d08e
0x7c0c: 0xfa7502a8      0x64e6d1b0
```
0x7bfc为栈帧结束地址，此时栈帧数据为：
```ASM
0x7bec: 0x00007d64      0x00000000      0x00000000      0x00000000
```
ebp = 0x7bf8，为段基地址。<br>
从这里开始，kernel开始执行，接下来执行的主要任务有：<br>
虚拟页表映射、启用分页、跳转到高地址执行C、OS初始化。
