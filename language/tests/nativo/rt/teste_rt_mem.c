/*
 * teste_rt_mem.c - teste de unidade de memcpy e memmove do runtime nativo (rt_mem.c 0.3.0, vetores de 16 bytes).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - memcpy n = 0..300 com deslocamentos 0..15 de origem e destino; memmove n = 0..300 com origem e destino
 *           no mesmo bloco (sobrepostos nos 2 sentidos, distancia -80..80); sentinelas de 32 bytes nos dois lados
 *           provam que nada fora de [destino, destino + n) e gravado. Referencia: copia byte a byte por buffer temporario.
 *
 * O rt_mem.c e incluido neste arquivo com os nomes trocados (fj_memcpy, fj_memmove...) para nao colidir com o libc.
 * Compile: gcc -std=c11 -O2 -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns teste_rt_mem.c -o teste_rt_mem.exe
 * Verificacao: teste_rt_mem.exe   (exit 0 e "teste_rt_mem: N casos, 0 falhas")
 */
#include <stdio.h>
#include <stdlib.h>

#define memcpy fj_memcpy
#define memmove fj_memmove
#define memset fj_memset
#define memcmp fj_memcmp
#define strlen fj_strlen
#include "../../../runtime/nativo/rt_mem.c"
#undef memcpy
#undef memmove
#undef memset
#undef memcmp
#undef strlen

#define TAM_BLOCO 1024
#define SENTINELA 32
#define MAX_N 300

static unsigned char Bloco[TAM_BLOCO];
static unsigned char Referencia[TAM_BLOCO];
static unsigned char Origem[TAM_BLOCO];
static long Casos = 0;
static long Falhas = 0;

static void Preencher(unsigned char *Alvo, size_t Tamanho, unsigned Semente)
{
    size_t i;
    for (i = 0; i < Tamanho; i++) {
        Semente = Semente * 1103515245u + 12345u;
        Alvo[i] = (unsigned char)(Semente >> 16);
    }
}

static void CopiarReferencia(unsigned char *Destino, const unsigned char *Fonte, size_t N)
{
    unsigned char CopiaDaFonte[TAM_BLOCO];
    size_t i;
    for (i = 0; i < N; i++)
        CopiaDaFonte[i] = Fonte[i];
    for (i = 0; i < N; i++)
        Destino[i] = CopiaDaFonte[i];
}

static void Falhou(const char *Funcao, size_t N, size_t DeslOrigem, size_t DeslDestino, long Distancia)
{
    Falhas++;
    if (Falhas <= 10)
        printf("FALHA %s n=%zu origem+%zu destino+%zu distancia=%ld\n", Funcao, N, DeslOrigem, DeslDestino, Distancia);
}

static void CasoMemcpy(size_t N, size_t DeslOrigem, size_t DeslDestino)
{
    unsigned char *Dst = Bloco + SENTINELA + DeslDestino;
    const unsigned char *Src = Origem + SENTINELA + DeslOrigem;
    void *Retorno;

    Preencher(Bloco, TAM_BLOCO, 7);
    Preencher(Origem, TAM_BLOCO, 99);
    Preencher(Referencia, TAM_BLOCO, 7);
    CopiarReferencia(Referencia + SENTINELA + DeslDestino, Src, N);
    Retorno = fj_memcpy(Dst, Src, N);
    Casos++;
    if (Retorno != Dst || fj_memcmp(Bloco, Referencia, TAM_BLOCO) != 0)
        Falhou("memcpy", N, DeslOrigem, DeslDestino, 0);
}

static void CasoMemmove(size_t N, long Distancia)
{
    size_t Base = 400;
    unsigned char *Src = Bloco + Base;
    unsigned char *Dst = Bloco + Base + Distancia;
    void *Retorno;

    Preencher(Bloco, TAM_BLOCO, 31);
    Preencher(Referencia, TAM_BLOCO, 31);
    CopiarReferencia(Referencia + Base + Distancia, Referencia + Base, N);
    Retorno = fj_memmove(Dst, Src, N);
    Casos++;
    if (Retorno != Dst || fj_memcmp(Bloco, Referencia, TAM_BLOCO) != 0)
        Falhou("memmove", N, 0, 0, Distancia);
}

int main(void)
{
    size_t N, Desl, DeslDst;
    long Distancia;

    for (N = 0; N <= MAX_N; N++)
        for (Desl = 0; Desl < 16; Desl++)
            for (DeslDst = 0; DeslDst < 16; DeslDst++)
                CasoMemcpy(N, Desl, DeslDst);
    for (N = 0; N <= MAX_N; N++)
        for (Distancia = -80; Distancia <= 80; Distancia++)
            CasoMemmove(N, Distancia);
    printf("teste_rt_mem: %ld casos, %ld falhas\n", Casos, Falhas);
    return Falhas == 0 ? 0 : 1;
}
