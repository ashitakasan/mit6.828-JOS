## 1. Part One: Eliminate allocation from sbrk()

sbrk函数应当向高地址移动 堆指针，而新的 sbrk函数却没有增加 堆指针，程序将尝试引用它认为它可以访问的内存地址，于是出错。
```C
int sys_sbrk(void){
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  proc->sz += n;
  //if(growproc(n) < 0)
  //  return -1;
  return addr;
}
```


## 2. Part Two: Lazy allocation

如果移动堆指针失败，即分配页面失败，则尝试用 kalloc 直接分配页面，然后用 mappages 映射虚拟地址（边界）到新分配的物理页面；
该虚拟地址即为：内存访问失败时，sbrk指针所指向的虚拟地址的 页面边界。

```C
if(tf->trapno == T_PGFLT){
	char* mem = kalloc();
	if(mem == 0){
		cprintf("Memory lazy alloction failed: ");
	}
	else{
		memset(mem, 0, PGSIZE);
		mappages(proc->pgdir, (void *)PGROUNDDOWN(rcr2()), PGSIZE, V2P(mem), PTE_W | PTE_U);
		return;
	}
}
```
