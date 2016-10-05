### 1. Explain the interface between printf.c and console.c. Specifically, what function does console.c export? How is this function used by printf.c?

在 printf.c 中，putch调用 console.c 中的 cputchar函数以及 printfmt.c中的 vprintfmt函数；

函数调用链为：

putch --> cputchar --> cons_putc --> serial_putc & lpt_putc & cga_putc


### 2. Explain the following from console.c:
```
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
```
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
```
unsigned int i = 0x00646c72;
cprintf("H%x Wo%s", 57616, &i);
```
 - What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise. Here's an ASCII table that maps bytes to characters.
 - The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?
<br>


### 5. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
```
cprintf("x=%d y=%d", 3);
```
输出：x=3 y=%d；
<br>


### 6. Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change cprintf or its interface so that it would still be possible to pass it a variable number of arguments?
