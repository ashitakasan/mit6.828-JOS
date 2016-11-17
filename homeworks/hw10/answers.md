### Modify bmap() so that it implements a doubly-indirect block, in addition to direct blocks and a singly-indirect block.

fs.c:
```C
static uint bmap(struct inode *ip, uint bn){
	uint addr, *a;
	struct buf *bp, *bp2;

	if(bn < NDIRECT){
		if((addr = ip->addrs[bn]) == 0)
			ip->addrs[bn] = addr = balloc(ip->dev);
		return addr;
	}
	bn -= NDIRECT;

	if(bn < NINDIRECT){
		if((addr = ip->addrs[NDIRECT]) == 0)
			ip->addrs[NDIRECT] = addr = balloc(ip->dev);
		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if((addr = a[bn]) == 0){
			a[bn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}
	bn -= NINDIRECT;

	if(bn < NINDIRECT * NINDIRECT){
		if((addr = ip->addrs[NDIRECT + 1]) == 0)
			ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);

		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if((addr = a[bn / NINDIRECT]) == 0){
			a[bn / NINDIRECT] = addr = balloc(ip->dev);
			log_write(bp);
		}

		bp2 = bread(ip->dev, addr);
		a = (uint *)bp2->data;
		if((addr = a[bn % NINDIRECT]) == 0){
			a[bn % NINDIRECT] = addr = balloc(ip->dev);
			log_write(bp2);
		}

		brelse(bp);
		brelse(bp2);
		return addr;
	}

	panic("bmap: out of range");
}
```

fs.h:
```C
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)

// On-disk inode structure
struct dinode {
	short type;           // File type
	short major;          // Major device number (T_DEV only)
	short minor;          // Minor device number (T_DEV only)
	short nlink;          // Number of links to inode in file system
	uint size;            // Size of file (bytes)
	uint addrs[NDIRECT+2];   // Data block addresses
};
```

file.h
```C
struct inode {
	uint dev;           // Device number
	uint inum;          // Inode number
	int ref;            // Reference count
	struct sleeplock lock;
	int flags;          // I_VALID

	short type;         // copy of disk inode
	short major;
	short minor;
	short nlink;
	uint size;
	uint addrs[NDIRECT+2];
};
```

inode 分布：
- 直接块： addrs[0-10]
- 单间接块： addrs[11] - NINDIRECT
- 双间接块： addrs[12] - NINDIRECT -- NINDIRECT * NINDIRECT

对于双间接块，一层间接块的号用 `bn / NINDIRECT` 来获取，二层间接块的号用 `bn % NINDIRECT` 来获取。
