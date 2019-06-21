#include <stdio.h>

#define SUSPEND 0
#define RESUME 0

void* f(int n)
{
   for(;;) {
     printf("%i\n", n++);
     SUSPEND; // returns a coroutine handle on first suspend
   }
}

int main()
{
    void* hdl = f(3);
    RESUME;
    RESUME;
    RESUME; 
    return 0;
}