#include <stdio.h>

static int soma(int a, int b) {
    return a + b;
}

int main(void) {
    printf("%d\n", soma(1, "2"));
    return 0;
}
