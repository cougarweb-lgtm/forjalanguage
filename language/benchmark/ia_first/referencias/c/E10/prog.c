#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 7) {
        return 2;
    }
    long long v[6];
    for (int i = 0; i < 6; i++) {
        v[i] = strtoll(argv[i + 1], NULL, 10);
    }
    printf("%lld %lld\n", v[0] * v[4] + v[1] * v[5], v[2] * v[4] + v[3] * v[5]);
    return 0;
}
