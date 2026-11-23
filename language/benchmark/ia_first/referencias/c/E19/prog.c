#include <stdio.h>
#include <string.h>

typedef struct {
    const char *nome;
    int idade;
} Registro;

static const Registro TABELA[] = {{"ana", 31}, {"bruno", 45}, {"carla", 28}};

/* Opcional em C: devolve 1 e grava a idade quando acha; 0 quando nao existe. */
static int buscar_idade(const char *nome, int *idade) {
    for (size_t i = 0; i < sizeof TABELA / sizeof TABELA[0]; i++) {
        if (strcmp(TABELA[i].nome, nome) == 0) {
            *idade = TABELA[i].idade;
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        int idade = 0;
        if (buscar_idade(argv[i], &idade)) {
            printf("%s: %d\n", argv[i], idade);
        } else {
            printf("%s: ausente\n", argv[i]);
        }
    }
    return 0;
}
