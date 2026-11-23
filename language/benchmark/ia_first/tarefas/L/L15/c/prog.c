#include <stdio.h>

int main(void) {
    int a[3] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        int x = a[i];
        printf("%d\n", x * 2);
    }
    for (int i = 0; i < 3; i++) {
        int *x = &a[i];
        *x = *x * 10;
    }
    int s = 0;
    for (int i = 0; i < 3; i++) {
        s += a[i] * (i + 1);
    }
    printf("%d\n", s);
    printf("%d\n", a[2]);
    return 0;
}
