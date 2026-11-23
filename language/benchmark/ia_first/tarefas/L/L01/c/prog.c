#include <stdio.h>

int main(void) {
    int total = 0;
    for (int i = 1; i <= 12; i++) {
        if (i % 3 == 0) {
            continue;
        }
        if (i > 10) {
            break;
        }
        total += i;
    }
    printf("%d\n", total);
    return 0;
}
