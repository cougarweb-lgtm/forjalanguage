#include <stdio.h>

static long long fat(int n) {
    long long r = 1;
    for (int i = 1; i <= n; i++) {
        r *= i;
    }
    return r;
}

int main(void) {
    printf("%lld\n", (long long)fat(13));
    return 0;
}
