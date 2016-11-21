#ifndef JOS_INC_ARGS_H
#define JOS_INC_ARGS_H

struct Argstate;

// 从 argc 和 argv 初始化 argstate 缓冲区
void argstart(int *argc, char **argv, struct Argstate *args);

/*
  返回参数列表中的下一个 flag，如果没有下一个则返回 -1
  标志停在非标志 (不以'-'开头的任何内容)，在参数列表的结尾，
   '-' 之前或 '-' 之后，以先到者为准。不返回任何 '-' 参数
  使用 argc/argv 数组中传递给argstart的参数。
  如果你使用一个 argc/argv 数组 ["sh", "-i", "foo"]，
   第一次调用argnext将返回 'i' 并将数组更改为 ["sh"，"foo"]
  因此，当 argnext 返回 -1 时，argc/argv 数组只包含非标志参数
 */
int argnext(struct Argstate *);

/*
  返回当前标志的下一个值，如果没有值，则返回 0；
  例如，给定参数列表 ["-fval1", "val2", "val3"]，对 argnext() 的调用将返回 "f"，
  	之后对 argnextvalue 的重复调用将返回 "val1", "val2" 和 "val3"；
  从 argc/argv 数组使用参数
 */
char *argnextvalue(struct Argstate *);

/*
  返回当前标志的值，如果没有值，则返回 0
  行为像 argnextvalue，除了重复调用 argvalue 将返回相同的值
 */
char *argvalue(struct Argstate *);

// 示例
// 
//	#include <inc/lib.h>
//
//	void
//	umain(int argc, char **argv)
//	{
//		int i;
//		struct Argstate args;
//
//		argstart(&argc, argv, &args);
//		while ((i = argnext(&args)) >= 0)
//			switch (i) {
//			case 'r':
//			case 'x':
//				cprintf("'-%c' flag\n", i);
//				break;
//			case 'f':
//				cprintf("'-f %s' flag\n", argvalue(&args));
//				break;
//			default:
//				cprintf("unknown flag\n");
//			}
//
//		for (i = 1; i < argc; i++)
//			cprintf("argument '%s'\n", argv[i]);
//	}
//	
// 如果此程序使用如下参数运行： ["-rx", "-f", "foo", "--", "-r", "duh"]
// 他会打印出： 
//	'-r' flag
//	'-x' flag
//	'-f foo' flag
//	argument '-r'
//	argument 'duh'

struct Argstate {
	int *argc;
	const char **argv;
	const char *curarg;
	const char *argvalue;
};

#endif
