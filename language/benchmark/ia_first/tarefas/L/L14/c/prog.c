#include <stdio.h>

static int ordenar(int *a, int n) {
    int trocas = 0;
    for (int i = 0; i < n; i++) {
        int lim = n - 1 - i;
        for (int j = 0; j < lim; j++) {
            if (a[j] > a[j + 1]) {
                int t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
                trocas += 1;
            }
        }
    }
    return trocas;
}

int main(void) {
    int v[6] = {5, 2, 9, 1, 7, 3};
    printf("%d\n", ordenar(v, 6));
    for (int i = 0; i < 6; i++) {
        printf("%d\n", v[i]);
    }
    return 0;
}
