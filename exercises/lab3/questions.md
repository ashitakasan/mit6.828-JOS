## 1. What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)

异常/中断 由很多种类，有些中断（如缺页中断）为非致命中断，只需要加载页面就能继续执行用户程序；
而有些异常（如数组访问异常），则需要退出用户进程；还有些终端（如硬件故障），则可能引起系统重启；
将各种异常的处理函数封装到 IDT 门中，可以对不同的异常采取不同的处理方法，并能提供不同的异常提示信息。


## 2. Did you have to do anything to make the user/softint program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but softint's code says int $14. Why should this produce interrupt vector 13? What happens if the kernel actually allows softint's int $14 instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

每个用户进程去访问 IDT 时，都有一个特权等级 CPL，这个 CPL 必须小于 IDT 中对应的中断向量的 特权等级 DPL，
这里在 trap 初始化时，我们将所有中断的 DPL 都设置为 0，即 kernel 等级，
而用户进程发生异常后，将带着特权等级 CPL = 3 来访问 IDT，这将导致一个 CPU 保护错误；
如果允许 int $14 去调用 kernel 的页面错误回调，会导致 普通用户进程 不仅过 kernel 检查，就分配内存。


## 3. The break point test case will either generate a break point exception or a general protection fault depending on how you initialized the break point entry in the IDT (i.e., your call to SETGATE from trap_init). Why? How do you need to set it up in order to get the breakpoint exception to work as specified above and what incorrect setup would cause it to trigger a general protection fault?

在 IDT 表中，将断点 breakpoint 对应的中断描述符的特权等级 DPL 设置为 3，即用户可访问的级别，此时如果发生了 breakpoint 中断，
就会直接跳到 IDT 表中指定的段上运行，因为此时用户特权等级 CPL = DPL；如果设置中断描述符表 DPL < 3，则会发生页保护异常，T_GPFLT。


## 4. What do you think is the point of these mechanisms, particularly in light of what the user/softint test program does?

内核 IDT 表的 DPL 来限制用户的访问，用户发起一个特权等级较高的中断时，CPU 发现 CPL > DPL，由此会引发一个 页保护异常 T_GPFLT；
这样就仅仅给用户提供了部分可访问的中断向量，保护 kernel。



