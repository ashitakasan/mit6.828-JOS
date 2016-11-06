## 1. What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)

异常/中断 由很多种类，有些中断（如缺页中断）为非致命中断，只需要加载页面就能继续执行用户程序；
而有些异常（如数组访问异常），则需要退出用户进程；还有些终端（如硬件故障），则可能引起系统重启；
将各种异常的处理函数封装到 IDT 门中，可以对不同的异常采取不同的处理方法，并能提供不同的异常提示信息。


## 2. Did you have to do anything to make the user/softint program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but softint's code says int $14. Why should this produce interrupt vector 13? What happens if the kernel actually allows softint's int $14 instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

每个用户进程去访问 IDT 时，都有一个特权等级 CPL，这个 CPL 必须小于 IDT 中对应的中断向量的 特权等级 DPL，
这里在 trap 初始化时，我们将所有中断的 DPL 都设置为 0，即 kernel 等级，
而用户进程发生异常后，将带着特权等级 CPL = 3 来访问 IDT，这将导致一个 CPU 保护错误；
如果允许 int $14 去调用 kernel 的页面错误回调，会导致 普通用户进程 不仅过 kernel 检查，就分配内存。
