#include <inc/lib.h>
#include <inc/elf.h>

#define UTEMP2USTACK(addr)	((void *)(addr) + (USTACKTOP - PGSIZE) - UTEMP)
#define UTEMP2				(UTEMP + PGSIZE)
#define UTEMP3				(UTEMP2 + PGSIZE)

// spawn 帮助函数
static int init_stack(envid_t child, const char **argv, uintptr_t *init_esp);
static int map_segment(envid_t child, uintptr_t va, size_t memsz, 
					int fd, size_t filesz, off_t fileoffset, int perm);
static int copy_shared_pages(envid_t child);


/*
  从文件系统加载的程序映像生成子进程
  prog: 要运行的程序的路径名
  argv: 指向字符串指针的空终止数组的指针，它将作为其命令行参数传递给子进程
  成功返回子进程的 envid, 错误返回小于 0
 */
int spawn(const char *prog, const char **argv){
	unsigned char elf_buf[512];
	struct Trapframe child_tf;
	envid_t child;

	int fd, i, r;
	struct Elf *elf;
	struct Proghdr *ph;
	int perm;

	// 此代码遵循此过程
	//  - 打开程序文件
	//  - 读取 ELF 头，和你之前一样，仔细检查它的幻数。(查看你的 load_icode！)
	//  - 使用 sys_exofork() 创建一个新环境
	//  - 将 child_tf 设置为子结构的初始结构 Trapframe
	//  - 调用上面的 init_stack() 函数为子环境设置初始堆栈页面
	//  - 将 p_type 为 ELF_PROG_LOAD 的所有程序段映射到新环境的地址空间中，
	//  		使用每个段的 Proghdr 中的 p_flags 字段来确定如何映射段
	//  - 如果 ELF 标志不包括 ELF_PROG_FLAG_WRITE，则段包含文本和只读数据
	//  - 使用 read_map() 读取这个段的内容，并将它直接返回的页面映射到子进程，
	//  		以便同一个程序的多个实例共享同一个程序文本的副本，
	//  		请务必在子进程中将只读的程序文本映射。Read_map 就像读，
	//  		但返回一个指向 *blk 中的数据的指针，而不是将数据复制到另一个缓冲区
	//  - 如果 ELF 段标志 DO 包括 ELF_PROG_FLAG_WRITE，则该段包含读/写数据和 bss
	//  		与 LAB 3中的 load_icode() 一样，这样的ELF段在内存中占用 p_memsz 字节，
	//  		但只有段的 FIRST p_filesz 字节实际上是从可执行文件加载的 - 您必须将其余部分清零；
	//  		对于要为读/写段映射的每个页面，在 UTEMP 中临时分配父页面，
	//  		将文件的适当部分 read() 分配到该页面和/或使用 memset() 将零加载部分；
	//  		如果 page_alloc() 已经返回零页面，如果你喜欢你可以避免调用 memset()；
	//  		然后将页面映射插入子进程；查看 init_stack() 获取灵感；
	//  		确保你理解为什么你不能在这里使用 read_map()。
	//  		注意：上面的段地址或长度都不能保证页面对齐，因此您必须正确处理这些非页面对齐的值；
	//  		然而，ELF 链接器确保没有两个段在同一页上重叠; 并且保证 PGOFF(ph->p_offset) == PGOFF(ph->p_va)
	//  - 调用 sys_env_set_trapframe(child, &child_tf) 在子进程中设置正确的初始 eip 和 esp 值
	//  - 使用 sys_env_set_status() 启动运行的子进程
	
	if((r = open(prog, O_RDONLY)) < 0)
		return r;
	fd = r;

	// 读取 elf 头
	elf = (struct Elf *e)elf_buf;
	if(readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf) || elf->e_magic != ELF_MAGIC){
		close(fd);
		cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
		return -E_NOT_EXEC;
	}

	// 创建一个新的子环境
	if((r = sys_exofork()) < 0)
		return r;
	child = r;

	child_tf = envs[ENVX(child)].env_tf;
	child_tf.tf_eip = elf->entry;

	if((r = init_stack(child, argv, &child_tf.tf_esp)) < 0)
		return r;

	// 设置ELF头中定义的程序段
	ph = (struct Proghdr *)(elf_buf + elf->phoff);
	for(i = 0; i < elf->e_phnum; i++, ph++){
		if(ph->p_type != ELF_PROG_LOAD)
			continue;
		perm = PTE_P | PTE_U;
		if(ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;
		if((r = map_segment(child, ph->p_va, ph->p_memsz, fd, ph->p_filesz, ph->p_offset, perm)) < 0)
			goto error;
	}
	close(fd);
	fd = -1;

	// 复制共享库状态
	if((r = copy_shared_pages(child)) < 0)
		panic("copy_shared_pages: %e", r);

	child_tf.tf_eflags |= FL_IOPL_3;
	if((r = sys_env_set_trapframe(child, &child_tf)) < 0)
		panic("sys_env_set_trapframe: %e", r);

	if((r = sys_env_set_status(child, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return child;

error:
	sys_env_destroy(child);
	close(fd);
	return r;
}

/*
  生成，直接在命令行参数数组上
  注意：在args的末尾必须有一个哨点 NULL (没有 args 可能为 NULL)
 */
int spawnl(const char *prog, const char *arg0, ...){
	// 我们通过推进 args 计算 argc，直到我们命中 NULL
	// 函数的约定保证最后一个参数将始终为 NULL，并且其他参数不会为 NULL
	int argc = 0;
	va_list vl;

	va_start(vl, arg0);
	while(va_arg(vl, void *) != NULL)
		argc++;
	va_end(vl);

	const char *argv[argc + 2];
	argv[0] = arg0;
	argv[argc + 1] = NULL;

	va_start(vl, arg0);
	unsigned i;
	for(i = 0; i < argc; i++)
		argv[i + 1] = va_arg(vl, const char *);
	va_end(vl);

	return spawn(prog, argv);
}

/*
  使用 'argv' 指向的参数数组为 envid = 'child' 设置新子进程的初始堆栈页面，
  这是一个以 null 结束的指向空值终止字符串的指针数组；
  成功返回 0 并且设置 *init_esp 为子进程应该开始的初始堆栈指针，错误返回小于 0
 */
static int init_stack(envid_t envid, const char **argv, uintptr_t *init_esp){
	size_t string_size;
	int argc, i, r;
	char *string_store;
	uintptr_t *argv_store;

	// 计算参数的数量 (argc) 和字符串所需的空间总量 (string_size)
	string_size = 0;
	for(argc = 0; argv[argc] != 0; argc++)
		string_size += strlen(argv[argc] + 1);

	// 确定放置字符串和 argv 数组的位置
	// 设置指向临时页面 'UTEMP' 的指针; 我们稍后将映射一个页面，
	// 然后将该页面重新映射到子环境中 (USTACKTOP - PGSIZE)，字符串是堆栈上最重要的东西
	string_store = (char *)UTEMP + PGSIZE - string_size;

	// argv 低于 string_store。每个参数有一个参数指针，加上一个空指针
	argv_store = (uintptr_t *)(ROUNDDOWN(string_store, 4) - 4 * (argc + 1));

	// 确保argv，字符串和包含 'argc' 和 'argv' 的 2 个字本身都适合单个堆栈页面
	if((void *)(argv_store - 2) < (void *)UTEMP)
		return -E_NO_MEM;

	if((r = sys_page_alloc(0, (void *)UTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		return r;

	// 初始化 'argv_store [i]' 以指向参数字符串i，对于所有 0 <= i < argc
	// 	另外，将参数字符串从 'argv' 复制到新分配的堆栈页面；
	// 将 'argv_store[argc]' 设置为 0 以终止 args 数组；
	// 在 'args' 下面将两个单词推送到子节点的堆栈，包含要传递给子节点的 umain() 函数的 argc 和 argv 参数
	// 	argv 应该低于堆栈上的 argc，(同样，argv 应该使用在子环境中有效的地址)；
	// 将 *init_esp 设置为子级的初始堆栈指针 (再次使用子环境中有效的地址)
	for(i = 0; i < argc; i++){
		argv_store[i] = UTEMP2USTACK(string_store);
		strcpy(string_store, argv[i]);
		string_store += strlen(argv[i]) + 1;
	}
	argv_store[argc] = 0;
	assert(string_store == (char *)UTEMP + PGSIZE);

	argv_store[-1] = UTEMP2USTACK(argv_store);
	argv_store[-2] = argc;

	*init_esp = UTEMP2USTACK(&argv_store[-2]);

	if((r = sys_page_map(0, UTEMP, child, (void *)(USTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
		goto error;
	if((r = sys_page_unmap(0, UTEMP)) < 0)
		goto error;
	
	return 0;

error:
	sys_page_unmap(0, UTEMP);
	return r;
}

static int map_segment(envid_t envid, uintptr_t va, size_t memsz, 
						int fd, size_t filesz, off_t fileoffset, int perm){
	int i, r;
	void *blk;

	//cprintf("map_segment %x+%x\n", va, memsz);
	
	if((i = PGOFF(va))){
		va -= i;
		mmesz += i;
		filesz += i;
		fileoffset -= i;
	}

	for(i = 0; i < memsz, i += PGSIZE){
		if(i >= filesz){
			// 分配一个空白页
			if((r = sys_page_alloc(child, (void *)(va + i), perm)) < 0)
				return r;
		}
		else{
			if((r = sys_page_alloc(0, UTEMP, PTE_P | PTE_U | PTE_W)) < 0)
				return r;
			if((r = seek(fd, fileoffset + i)) < 0)
				return r;
			if((r = readn(fd, UTEMP, MIN(PGSIZE, filesz - i))) < 0)
				return r;
			if((r = sys_page_map(0, UTEMP, child, (void *)(va + i), perm)) < 0)
				panic("spawn: sys_page_map data: %e", r);
			sys_page_unmap(0, UTEMP);
		}
	}
	return 0;
}

/*
  将共享页面的映射复制到子地址空间
 */
static int copy_shared_pages(envid_t envid){
	// LAB 5
	return 0;
}
