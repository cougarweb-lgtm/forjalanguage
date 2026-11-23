#include <stdio.h>

int main(void) {
    int contagem = 0;
    for (int i = 1; i <= 4; i++) {
        for (int j = 1; j <= 4; j++) {
            if (j > i) {
                break;
            }
            int s = i + j;
            if (s % 2 == 0) {
                continue;
            }
            contagem += 1;
        }
    }
    printf("%d\n", contagem);
    return 0;
}
