## 1. Assuming that the following JOS kernel code is correct, what type should variable x have, uintptr_t or physaddr_t?
```C
mystery_t x;
char* value = return_a_pointer();
*value = 10;
x = (mystery_t) value;
```
mystery_t 应该是 uintptr_t；
