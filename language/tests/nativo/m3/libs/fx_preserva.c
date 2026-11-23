/* fx_preserva.c - programa de teste da DLL nativa da Forja (M3 P22b; bateria 10.2 e, direcao Forja exporta): os registradores nao-volateis voltam intactos de um export pesado.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22b: primeira versao (a troca de registradores esta em fx_preserva_stub.asm).
 * Uso: fx_preserva.exe <dll> <export>
 * Carrega a DLL, acha o export (ate 4 argumentos escalares, int ou real, devolvendo f64) e o chama 3 vezes pelo stub de NASM, que enche rbx rbp rdi rsi r12-r15 e xmm6-xmm15
 * com sentinelas antes da chamada e confere cada um na volta. Imprime uma linha por chamada (o resultado f64 vai inteiro, em hexa, para comparar o backend C com o x64) e,
 * se algum registrador mudou, o nome dele. Codigo 0 so com os 18 registradores intactos nas 3 chamadas.
 * Verificacao: python -I ..\..\abi\verificar_abi.py --lado export --forja <copia>
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define QTD_CHAMADAS 3
#define QTD_ARGUMENTOS 4
#define QTD_PRESERVADOS 18

uint64_t fx_preserva_chamar(void *funcao, const uint64_t *argumentos, uint32_t *mascara, double *resultado);

/* A ordem dos bits da mascara que o stub grava. */
static const char *const NOMES[QTD_PRESERVADOS] = {"rbx", "rbp", "rdi", "rsi", "r12", "r13", "r14", "r15", "xmm6", "xmm7",
                                                    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"};

/* n, x, m, y do export: os mesmos 4 slots viram rcx rdx r8 r9 e xmm0-xmm3 (cada posicao usa o seu). */
static void PrepararArgumentos(uint64_t *a, int64_t n, double x, int64_t m, double y)
{
    a[0] = (uint64_t)n;
    memcpy(&a[1], &x, sizeof x);
    a[2] = (uint64_t)m;
    memcpy(&a[3], &y, sizeof y);
}

static int Chamar(void *funcao, const char *nome, int64_t n)
{
    uint64_t argumentos[QTD_ARGUMENTOS];
    uint32_t mascara = 0;
    double resultado = 0.0;
    uint64_t bits;

    PrepararArgumentos(argumentos, n, 2.5 + (double)n, n + 3, 0.25);
    fx_preserva_chamar(funcao, argumentos, &mascara, &resultado);
    memcpy(&bits, &resultado, sizeof bits);
    printf("fx_preserva: %s(%lld) = 0x%016llx; mascara de registradores alterados = 0x%05x\n", nome, (long long)n, (unsigned long long)bits, mascara);
    for (int k = 0; k < QTD_PRESERVADOS; k++) {
        if ((mascara >> k) & 1u)
            printf("fx_preserva: REGISTRADOR NAO-VOLATIL ALTERADO por %s(%lld): %s\n", nome, (long long)n, NOMES[k]);
    }
    return mascara != 0;
}

int main(int argc, char **argv)
{
    static const int64_t ENTRADAS[QTD_CHAMADAS] = {1, 7, 40};
    HMODULE m;
    void *funcao;
    int alterados = 0;

    if (argc < 3) {
        fprintf(stderr, "uso: fx_preserva.exe <dll> <export>\n");
        return 2;
    }
    m = LoadLibraryA(argv[1]);
    funcao = m == NULL ? NULL : (void *)GetProcAddress(m, argv[2]);
    if (funcao == NULL) {
        printf("fx_preserva: nao achei %s em %s\n", argv[2], argv[1]);
        return 3;
    }
    for (int k = 0; k < QTD_CHAMADAS; k++)
        alterados += Chamar(funcao, argv[2], ENTRADAS[k]);
    fflush(stdout);
    return alterados == 0 ? 0 : 1;
}
