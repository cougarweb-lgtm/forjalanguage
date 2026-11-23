#include <stdio.h>
#include <string.h>

/* Sem tabela hash na biblioteca padrao do C: busca linear nas palavras ja vistas. */
int main(int argc, char **argv) {
    int total = argc - 1;
    const char *palavras[256];
    int contagem[256];
    int distintas = 0;
    for (int i = 0; i < total && i < 256; i++) {
        int achada = -1;
        for (int j = 0; j < distintas; j++) {
            if (strcmp(palavras[j], argv[i + 1]) == 0) {
                achada = j;
            }
        }
        if (achada >= 0) {
            contagem[achada]++;
        } else {
            palavras[distintas] = argv[i + 1];
            contagem[distintas] = 1;
            distintas++;
        }
    }
    for (int j = 0; j < distintas; j++) {
        printf("%s: %d\n", palavras[j], contagem[j]);
    }
    return 0;
}
