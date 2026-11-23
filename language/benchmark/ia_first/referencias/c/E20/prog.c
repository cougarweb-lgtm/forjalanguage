#include <stdio.h>

typedef struct {
    const char *nome;
    int quantidade;
    double preco;
} Item;

int main(void) {
    const Item itens[] = {{"Maca", 12, 3.5}, {"Banana", 5, 12.25}, {"Uva", 100, 0.99}};
    int total_qtd = 0;
    double total_valor = 0.0;
    printf("%-10s%5s%8s\n", "Item", "Qtd", "Preco");
    for (size_t i = 0; i < sizeof itens / sizeof itens[0]; i++) {
        printf("%-10s%5d%8.2f\n", itens[i].nome, itens[i].quantidade, itens[i].preco);
        total_qtd += itens[i].quantidade;
        total_valor += itens[i].quantidade * itens[i].preco;
    }
    printf("%-10s%5d%8.2f\n", "Total", total_qtd, total_valor);
    return 0;
}
