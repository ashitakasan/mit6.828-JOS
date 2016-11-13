#include <inc/lib.h>

extern void _pgfault_upcall(void);

void (*_pgfault_handler)(struct UTrapframe *utf);

/*
  
 */
void set_pgfault_handler(void (*handler)(struct UTrapframe *utf)){
	int r;

	if(_pgfault_handler == 0){

		panic("set_pgfault_handler not implemented");
	}

	_pgfault_handler = handler;
}
