#include <stdio.h>
#include <string.h>

typedef struct {
    const char *titular;
    long long saldo;
} Conta;

static int depositar(Conta *conta, long long valor) {
    if (valor <= 0) {
        return 0;
    }
    conta->saldo += valor;
    return 1;
}

static int sacar(Conta *conta, long long valor) {
    if (valor <= 0 || valor > conta->saldo) {
        return 0;
    }
    conta->saldo -= valor;
    return 1;
}

/* Devolve quantas das 4 verificacoes falharam. */
static int autoteste(void) {
    int falhas = 0;
    Conta c = {"t", 100};
    falhas += !(depositar(&c, 50) && c.saldo == 150);
    falhas += !(!depositar(&c, 0) && c.saldo == 150);
    falhas += !(!sacar(&c, 200) && c.saldo == 150);
    falhas += !(sacar(&c, 150) && c.saldo == 0);
    return falhas;
}

static void cenario(void) {
    Conta c = {"Rita", 1000};
    int ok = depositar(&c, 500);
    printf("deposito 500: %s, saldo %lld\n", ok ? "ok" : "recusado", c.saldo);
    ok = sacar(&c, 2000);
    printf("saque 2000: %s, saldo %lld\n", ok ? "ok" : "recusado", c.saldo);
    ok = sacar(&c, 700);
    printf("saque 700: %s, saldo %lld\n", ok ? "ok" : "recusado", c.saldo);
    ok = depositar(&c, -5);
    printf("deposito -5: %s, saldo %lld\n", ok ? "ok" : "recusado", c.saldo);
    printf("titular %s saldo final %lld\n", c.titular, c.saldo);
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "autoteste") == 0) {
        int falhas = autoteste();
        if (falhas == 0) {
            printf("autoteste: 4 ok\n");
            return 0;
        }
        printf("autoteste: falhou %d\n", falhas);
        return 1;
    }
    cenario();
    return 0;
}
