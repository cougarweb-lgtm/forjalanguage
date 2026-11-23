#include <stdio.h>

int main(void) {
    printf("%d\n", 3);
    for (int i = 0; i < 2; i++) {
        printf("%d\n", 20 + i);
        printf("%d\n", 10 + i);
    }
    printf("%d\n", 4);
    printf("%d\n", 2);
    printf("%d\n", 1);
    return 0;
}
