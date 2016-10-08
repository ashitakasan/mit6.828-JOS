### 1. Explain the interface between printf.c and console.c. Specifically, what function does console.c export? How is this function used by printf.c?

在 printf.c 中，putch调用 console.c 中的 cputchar函数以及 printfmt.c中的 vprintfmt函数；

函数调用链为：

putch --> cputchar --> cons_putc --> serial_putc & lpt_putc & cga_putc


### 2. Explain the following from console.c:
```C
if (crt_pos >= CRT_SIZE) {
	int i;
	memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
	for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
		crt_buf[i] = 0x0700 | ' ';
	crt_pos -= CRT_COLS;
}
```
这段代码，在屏幕输出字符占满整个窗口时，进行整屏字符向上移动，留出新的一行准备接收输入。


### 3. For the following questions you might wish to consult the notes for Lecture2. These notes cover GCC's calling convention on the x86.
Trace the execution of the following code step-by-step:
```C
int x = 1, y = 3, z = 4;
cprintf("x %d, y %x, z %d\n", x, y, z);
```
 - In the call to cprintf(), to what does fmt point? To what does ap point?
 - List (in order of execution) each call to cons_putc, va_arg, and vcprintf. For cons_putc, list its argument as well. For va_arg, list what ap points to before and after the call. For vcprintf list the values of its two arguments.
<br/>
fmt point 是输入可变参数列表的第一个参数，需要传递给 va_start来初始化va_list；
ap指针在调用 va_start 之后，表示输入的可变参数列表
<br/>


### 4. Run the following code.
```C
unsigned int i = 0x00646c72;
cprintf("H%x Wo%s", 57616, &i);
```
 - What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise. Here's an ASCII table that maps bytes to characters.
 - The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?
<br>
运行结果：He110 World <br>
57616的16进制的值为 e110；
```ASM
   0xf010007b <test_backtrace+59>:      movl   $0x646c72,-0xc(%ebp)
   0xf0100082 <test_backtrace+66>:      add    $0xc,%esp
   0xf0100085 <test_backtrace+69>:      lea    -0xc(%ebp),%eax
=> 0xf0100088 <test_backtrace+72>:      push   %eax

(gdb) x/2s $eax-16
0xf010fecc:     "\232\t\020\360@\031\020\360\364\376\020\360@\031\020\360rld"
0xf010fee0:     "\n"
```
可以看到，字符串"cld"的值在地址 0xf010fedc处，所以打印出 He110 World <br>
分析为何 字符串"cld"出现在这里？这是因为我的机器是小端模式，movl   $0x646c72,-0xc(%ebp)这条指令在执行时，0x00646c72将从后往前一个字节一个字节的压入堆栈，
即从堆栈上由低地址向高地址读取数据时，读到的为：0x726c6400，转换为(char *)，则变为：'r' 'l' 'd' '\0'；正好完成了World的后半部分。
<br>
如果换到了大端模式的机器上，57616不需要变，57616被存放为（0x0000e110），读取将按大端模式读取，其结果不变；
只有第二个参数 0x00646c72的值需要修改，修改为：0x726c6400，其被存放为（0x726c6400）,在用（char *）读取时，从底地址向高地址读取，结果为：'r' 'l' 'd' '\0'。
<br>


### 5. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
```C
cprintf("x=%d y=%d", 3);
```
输出：x=3 y=-267321480 <br>
这是因为 cprintf() 内部调用 va_arg()来获取可变参数，而va_arg()则通过获取函数调用栈帧上push的函数参数来获取可变参数。
在这里，cprintf 仅提供了一个可变参数，但是调用了两次 va_arg()，它将去访问栈帧上非函数参数的区域，最终得到的第二个可变参数为随机值。
如果执行cprintf("x=%d y=%d", 3, 4, 5)；即提供更多参数，则无影响。
<br>


### 6. Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change cprintf or its interface so that it would still be possible to pass it a variable number of arguments?
