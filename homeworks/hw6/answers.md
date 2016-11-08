## 1. Modify your code so that get operations run in parallel while maintaining correctness. (Hint: are the locks in get necessary for correctness in this application?)

```C
static void * thread(void *xa){
	...
	for (i = 0; i < NKEYS; i++) {
		// pthread_mutex_lock(&lock);
		struct entry *e = get(keys[i]);
		// pthread_mutex_unlock(&lock);
		if (e == 0) k++;
		else	count++;
	}
	...
}
```

在 get 函数前后加上 mutex 锁，运行结果依然有错误，同时运行时间增加到原来的 两倍：

```TXT
0: put time = 0.008494
1: put time = 0.009284
1: get time = 26.228434
1: 18997 keys missing
1: 81003 keys get
0: get time = 26.228434
0: 18997 keys missing
0: 81003 keys get
completion time = 26.237887
```


## 2. Modify your code so that some put operations run in parallel while maintaining correctness. (Hint: would a lock per bucket work?)

```C
static void * thread(void *xa){
	...
	for (i = 0; i < b; i++) {
		// printf("%d: put %d\n", n, b*n+i);
		pthread_mutex_lock(&lock);
		put(keys[b*n + i], n);
		pthread_mutex_unlock(&lock);
	}
	...
}
```

在 put 函数前后加上 mutex锁，会使得运行结果没有错误，即 0 keys missing，同时运行时间和没有 mutex 锁时差不多：

```TXT
0: put time = 0.045042
1: put time = 0.045718
1: get time = 9.790589
1: 0 keys missing
1: 100000 keys get
0: get time = 9.795630
0: 0 keys missing
0: 100000 keys get
completion time = 9.841475
```

__原因：在 put 前后加锁，会构造出正确的 hashtable，因此多线程的 get 不会出错；而如果不在 put 前后加锁，hashtable 中数据就是有误的，即使在 get 前后加锁，也不能获取正确的数据，只会增加运行时间__
