#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Ola, mundo!\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        printf("Ola, %s!\n", argv[i]);
    }
    return 0;
}
