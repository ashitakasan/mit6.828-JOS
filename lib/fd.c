#include <inc/lib.h>

#define debug		0

// 程序可能同时保持打开的文件描述符的最大数目
#define MAXFD		32

// 文件描述符区域的底部
#define FDTABLE			0xD0000000

// 文件数据区底部。我们为每个 FD 保留一个数据页，设备可以选择使用
#define FILEDATA			(FDTABLE + MAXFD * PGSIZE)

// 返回文件描述符索引 i 的 'struct Fd *'
#define INDEX2FD(i)		((struct Fd *)(FDTABLE + (i) * PGSIZE))
// 返回文件描述符索引 i 的文件数据页
#define INDEX2DATA(i)	((char *)(FILEDATA + (i) * PGSIZE))

// --------------------------------------------------------------
// File descriptor manipulators
// --------------------------------------------------------------

int fd2num(struct Fd *fd){
	return ((uintptr_t)fd - FDTABLE) / PGSIZE;
}

char *fd2data(struct Fd *fd){
	return INDEX2DATA(fd2num(fd));
}

/*
  找到从 0 到 MAXFD-1 没有其fd页面映射的最小 i，将 *fd_store 设置为相应的 fd 页面虚拟地址
  fd_alloc实际上不分配 fd 页面，它是由调用者以某种方式分配页面；
  这意味着如果有人在一行中调用 fd_alloc 两次而不分配我们返回的第一个页面，我们将第二次返回相同的页面；
  成功返回 0，错误则设置 *fd_store = 0 并且返回小于 0：
  	-E_MAX_FD，没有更多的文件描述符
 */
int fd_alloc(struct Fd **fd_store){
	int i;
	struct Fd *fd;

	for(i = 0; i < MAXFD; i++){
		fd = INDEX2FD(i);
		if((uvpd[PDX(fd)] & PTE_P) == 0 || (uvpt[PGNUM(fd)] & PTE_P) == 0){
			*fd_store = fd;
			return 0;
		}
	}
	*fd_store = 0;
	return -E_MAX_OPEN;
}

/*
  检查 fdnum 是否在范围内并已映射。 如果是，将 *fd_store 设置为 fd 页面虚拟地址
  成功返回 0 (该页面在范围内并被映射)，错误返回小于 0：
  	-E_INVAL: fdnum 不在范围内或未映射
 */
int fd_lookup(int fdnum, struct Fd **fd_store){
	struct Fd *fd;

	if(fdnum < 0 || fdnum >= MAXFD){
		if (debug)
			cprintf("[%08x] bad fd %d\n", thisenv->env_id, fdnum);
		return -E_INVAL;
	}

	fd = INDEX2FD(fdnum);
	if(!(uvpt[PDX(fd)] & PTE_P) || !(uvpt[PGNUM(fd)] & PTE_P)){
		if (debug)
			cprintf("[%08x] closed fd %d\n", thisenv->env_id, fdnum);
		return -E_INVAL;
	}
	*fd_store = fd;
	return 0;
}

/*
  通过关闭相应的文件并取消映射文件描述符页来释放文件描述符 'fd'；
  如果 'must_exist' 为 0，则 fd 可以是一个关闭或不存在的文件描述符; 该函数将返回 0 并且没有其他效果；
  如果 'must_exist' 为 1，则 fd_close 在传递封闭或不存在的文件描述符时返回 -E_INVAL；
  成功返回 0，错误返回小于 0：
 */
int fd_close(struct Fd *fd, bool must_exist){
	struct Fd *fd2;
	struct Dev *dev;
	int r;

	if((r = fd_lookup(fd2num(fd), &fd2)) < 0 || fd != fd2)
		return (must_exist ? r : 0);
	if((r = dev_lookup(fd->fd_dev_id, &dev)) >= 0){
		if(dev->dev_close)
			r = (*dev->dev_close)(fd);
		else
			r = 0;
	}

	// 确保fd未映射。如果 (*dev->dev_close)(fd) 已经取消映射，可能是无操作
	sys_page_unmap(0, fd);
	return r;
}

// --------------------------------------------------------------
// File functions
// --------------------------------------------------------------

static struct Dev *devtab[] = {
	&devfile,
	&devpipe,
	&devcons,
	0
};

int dev_lookup(int dev_id, struct Dev **dev){
	int i;
	for(i = 0; devtab[i]; i++){
		if(devtab[i]->dev_id == dev_id){
			*dev = devtab[i];
			return 0;
		}
	}
	cprintf("[%08x] unknown device type %d\n", thisenv->env_id, dev_id);
	*dev = 0;
	return -E_INVAL
}

int close(int fdnum){
	struct Fd *fd;
	int r;

	if((r = fd_lookup(fdnum, &fd)) < 0)
		return r;
	else
		return fd_close(fd, 1);
}

void close_all(void){
	int i;
	for(i = 0; i < MAXFD; i++)
		close(i);
}

/*
  使文件描述符 'newfdnum' 是文件描述符 'oldfdnum' 的拷贝；
  例如，写入任一文件描述符将影响另一个的文件和文件偏移量；
  关闭在 'newfdnum' 上任何以前打开的文件描述符。这是实现使用虚拟内存技巧
 */
int dup(int oldfdnum, int newfdnum){
	int r;
	char *ova, *nva;
	pte_t pte;
	struct Fd *oldfd, *newfd;

	if((r = fd_lookup(oldfdnum, &oldfd)) < 0)
		return r;
	close(newfdnum);

	newfd = INDEX2FD(newfdnum);
	ova = fd2data(oldfd);
	nva = fd2data(newfd);

	if((uvpt[PDX(ova)] & PTE_P) && (uvpt[PGNUM(ova)] & PTE_P)){
		if((r = sys_page_map(0, ova, 0, nva, uvpt[PGNUM(ova)] & PTE_SYSCALL)) < 0)
			goto err;
	}
	if((r = sys_page_map(0, oldfd, 0, newfd, uvpt[PGNUM(oldfd)] & PTE_SYSCALL)) < 0)
		goto err;
	return newfdnum;

err:
	sys_page_unmap(0, newfd);
	sys_page_unmap(0, nva);
	return r;
}

ssize_t read(int fdnum, void *buf, size_t n){
	int r;
	struct Dev *dev;
	struct Fd *fd;

	if((r = fd_lookup(fdnum, &fd)) < 0 || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		return r;
	if((fd->fd_omode & O_ACCMODE) == O_WRONLY){
		cprintf("[%08x] read %d -- bad mode\n", thisenv->env_id, fdnum);
		return -E_INVAL;
	}

	if(!dev->dev_read)
		return -E_NOT_SUPP;
	return (*dev->dev_read)(fd, buf, n);
}

ssize_t readn(int fdnum, void *buf, size_t n){
	int m, tot;

	for(tot = 0; tot < n; tot += m){
		m = read(fdnum, (char *)buf + tot, n - tot);
		if(m < 0)
			return m;
		if(m == 0)
			break;
	}
	return tot;
}

ssize_t write(int fdnum, const void *buf, size_t n){
	int r;
	struct Dev *dev;
	struct Fd *fd;

	if((r = fd_lookup(fdnum, &fd)) < 0 || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		return r;
	if((fd->fd_omode & O_ACCMODE) == O_RDONLY){
		cprintf("[%08x] write %d -- bad mode\n", thisenv->env_id, fdnum);
		return -E_INVAL;
	}
	if (debug)
		cprintf("write %d %p %d via dev %s\n", fdnum, buf, n, dev->dev_name);

	if(!dev->dev_write)
		return -E_NOT_SUPP;
	return (*dev->dev_write)(fd, buf, n);
}

int seek(int fdnum, off_t offset){
	int r;
	struct Fd *fd;

	if((r = fd_lookup(fdnum, &fd)) < 0)
		return r;
	fd->fd_offset = offset;
	return 0;
}

int ftruncate(int fdnum, off_t newsize){
	int r;
	struct Dev *dev;
	struct Fd *fd;

	if((r = fd_lookup(fdnum, &fd)) < 0 || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		return r;
	if((fd->fd_omode & O_ACCMODE) == O_RDONLY){
		cprintf("[%08x] ftruncate %d -- bad mode\n", thisenv->env_id, fdnum);
		return -E_INVAL;
	}

	if(!dev->dev_trunc)
		return -E_NOT_SUPP;
	return (*dev->dev_trunc)(fd, newsize);
}

int fstat(int fdnum, struct Stat *stat){
	int r;
	struct Dev *dev;
	struct Fd *fd;

	if((r = fd_lookup(fdnum, &fd)) < 0 || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		return r;

	if(!dev->dev_stat)
		return -E_NOT_SUPP;
	stat->st_name[0] = 0;
	stat->st_size = 0;
	stat->st_isdir = 0;
	stat->st_dev = dev;
	return (*dev->dev_stat)(fd, stat);
}

int stat(const char *path, struct Stat *stat){
	int fd, r;

	if((fd = open(path, O_RDONLY)) < 0)
		return fd;
	r = fstat(fd, stat);
	close(fd);
	return r;
}
