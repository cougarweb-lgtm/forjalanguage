#include <stdio.h>
#include <stdlib.h>

static void validar(int n) {
    if (n > 3) {
        printf("limite\n");
        exit(3);
    }
}

int main(void) {
    for (int i = 1; i <= 10; i++) {
        validar(i);
        printf("%d\n", i);
    }
    printf("%d\n", 100);
    return 0;
}
