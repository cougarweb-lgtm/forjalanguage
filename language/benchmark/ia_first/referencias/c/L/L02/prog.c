#include <stdio.h>

static int fib(int n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    for (int i = 0; i < 8; i++) {
        printf("%d\n", fib(i));
    }
    return 0;
}
