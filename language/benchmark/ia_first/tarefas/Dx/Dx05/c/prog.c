#include <stdio.h>

static int fat(int n) {
    int r = 1;
    for (int i = 1; i <= n; i++) {
        r *= i;
    }
    return r;
}

int main(void) {
    printf("%lld\n", (long long)fat(13));
    return 0;
}
