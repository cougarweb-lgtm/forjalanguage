#include <stdio.h>
#include <stdint.h>

static int contar_bits(uint32_t n) {
    uint32_t x = n;
    int c = 0;
    while (x != 0) {
        c += (int)(x & 1);
        x = x >> 1;
    }
    return c;
}

int main(void) {
    printf("%d\n", contar_bits(0xB7));
    printf("%d\n", contar_bits(255));
    uint32_t m = 0xF0;
    printf("%d\n", (int)((m >> 4) & 0x3));
    printf("%d\n", (int)(m | 0x0F));
    return 0;
}
