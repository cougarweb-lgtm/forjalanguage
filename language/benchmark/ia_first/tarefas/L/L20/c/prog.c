#include <stdio.h>

int main(void) {
    int a = 7;
    int b = 123;
    printf("a=%03d b=%5d|\n", a, b);
    printf("%d%d\n", a, b);
    printf("{x} %d\n", a + b);
    return 0;
}
