#include <inc/args.h>
#include <inc/string.h>

void argstart(int *argc, char **argv, struct Argstate *args){
	args->argc = argc;
	args->argv = (const char **)argv;
	args->curarg = (*argc > 1 && argv ? "" : 0);
	args->argvalue = 0;
}

int argnext(struct Argstate *args){
	int arg;

	args->argvalue = 0;

	// 如果 args->curarg == 0，完成处理参数
	if(args->curarg == 0)
		return -1;

	if(!*args->curarg){
		// 需要处理下一个参数检查参数列表的结尾
		if(*args->argc == 1 || args->argv[1][0] != '-' || args->argv[1][1] == '\0')
			goto endofargs;
		// 将参数向下移一个
		args->curarg = args->argv[1] + 1;
		memmove(args->argv + 1, args->argv + 2, sizeof(const char *) * (*args->argc - 1));
		(*args->argc)--;
		// 检查“ - ”：参数列表
		if(args->curarg[0] == '-' && args->curarg[1] == '\0')
			goto endofargs;
	}

	arg = (unsigned char) *args->curarg;
	args->curarg++;
	return arg;

endofargs:
	args->curarg = 0;
	return -1;
}

char *argnextvalue(struct Argstate *args){
	if(!args->curarg)
		return 0;
	if(*args->curarg){
		args->argvalue = args->curarg;
		args->curarg = "";
	}
	else if(*args->argc > 1){
		args->argvalue = args->argv[1];
		memmove(args->argv + 1, args->argv + 2, sizeof(const char *) * (*args->argc - 1));
		(*args->argc)--;
	}
	else{
		args->argvalue = 0;
		args->curarg = 0;
	}
	return (char *)args->argvalue;
}
