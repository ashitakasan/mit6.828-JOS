### Achieve the desired behavior

```C
static void barrier()
{
	pthread_mutex_lock(&bstate.barrier_mutex);
	bstate.nthread++;
	if(bstate.nthread == nthread){
		bstate.nthread = 0;
		bstate.round++;
		pthread_cond_broadcast(&bstate.barrier_cond);
	}
	else{
		pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
	}
	pthread_mutex_unlock(&bstate.barrier_mutex);
}
```
注意：
- bstate.nthread 是一个非线程安全的变量，每次获取或设置它的值时，必须加 mutex 锁；
- pthread_cond_wait 虽然内部 执行了 pthread_mutex_unlock()，释放了 CPU，但是当 当前线程接收到 pthread_cond_broadcast 或 pthread_cond_signal 信号后，会再次对 mutex 进行加锁，所以在 barrier() 的最后需要对 mutex 释放锁操作；
- lock - unlock 之间的代码每次只能由一个线程执行，因此可以根据 bstate.nthread 对线程进行分支，只允许一个线程执行 pthread_cond_broadcast
