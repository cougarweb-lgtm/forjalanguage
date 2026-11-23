#include <stdio.h>

static int passos(int n) {
    int x = n;
    int c = 0;
    while (x != 1) {
        if (x % 2 == 0) {
            x = x / 2;
        } else {
            x = 3 * x + 1;
        }
        c += 1;
    }
    return c;
}

int main(void) {
    printf("%d\n", passos(1));
    printf("%d\n", passos(6));
    printf("%d\n", passos(27));
    return 0;
}
