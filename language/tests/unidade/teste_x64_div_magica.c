/* teste_x64_div_magica.c - confere DivMagicaPlanejar e DivMagicaEmular (nativo/x64/x64_div_magica.c) contra a divisao nativa de largura exata (/ e % do C, que truncam para zero).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - 8 bits: todo d x todo a, com e sem sinal; 16 bits: todo d x amostra (bordas, multiplos de d, 1 a cada 251, ALEATORIOS_16 por d; --exaustivo: todo a);
 *                    32 e 64 bits: d de borda, 2^k +-1, 1..2000, -2..-2000, primos grandes e aleatorios (seed fixo) x bordas, multiplos de d e ALEATORIOS_32_64 por d;
 *                    --selftest rapido (<10 s).
 * Verificacao: gcc -std=c11 -O2 -Wall -Wextra -Werror -I compiler tests\unidade\teste_x64_div_magica.c compiler\nativo\x64\x64_div_magica.c -o <tmp>\teste_x64_div_magica.exe
 *              <tmp>\teste_x64_div_magica.exe --selftest   (rapido)
 *              <tmp>\teste_x64_div_magica.exe              (bateria completa)
 *              <tmp>\teste_x64_div_magica.exe --exaustivo  (16 bits: todo d x todo a)
 *              Sanitizadores: a mesma compilacao com -g -O1 -fsanitize=undefined -fno-sanitize-recover=all (libubsan; o MinGW desta casa nao traz libasan/libubsan).
 * Saida: 0 = todos os casos batem | 1 = divergencia (d, a, esperado e obtido em stdout; sai logo) | 2 = uso invalido.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nativo/x64/x64_div_magica.h"

#define SEMENTE_FIXA 0x9E3779B97F4A7C15ull
#define ALEATORIOS_16 100000u
#define ALEATORIOS_32_64 64000u
#define ALEATORIOS_D 10000u
#define PRIMOS_GRANDES 200u
#define DIVISORES_MAX 16384u
#define STRIDE_16 251u
#define SELFTEST_ALEATORIOS 300u

/* Um divisor com o plano ja planejado: as varreduras de a reusam o plano, que e o custo caro. */
typedef struct {
    uint32_t bits;
    bool sinal;
    uint64_t d;
    DivMagicaPlano plano;
} Alvo;

/* Contagem de pares (d, a) conferidos, por largura e sinal. */
static uint64_t CONTAGEM[4][2];
static uint64_t ESTADO_RNG = SEMENTE_FIXA;

static uint32_t IndiceBits(uint32_t bits)
{
    return bits == 8u ? 0u : bits == 16u ? 1u : bits == 32u ? 2u : 3u;
}

static uint64_t MascaraRef(uint32_t bits)
{
    return bits >= 64u ? ~0ull : (1ull << bits) - 1ull;
}

/* Valor com sinal de um padrao de N bits, escrito de outra forma que a do planejador (subtrai 2^N se o bit N-1 esta ligado). */
static int64_t EstenderRef(uint64_t padrao, uint32_t bits)
{
    uint64_t v = padrao & MascaraRef(bits);
    if (bits == 64u)
        return (int64_t)v;
    if ((v & (1ull << (bits - 1u))) != 0u)
        return (int64_t)v - (int64_t)(1ull << bits);
    return (int64_t)v;
}

/* xorshift64*: determinista, para que duas execucoes conferiam os mesmos casos. */
static uint64_t Aleatorio(void)
{
    ESTADO_RNG ^= ESTADO_RNG >> 12;
    ESTADO_RNG ^= ESTADO_RNG << 25;
    ESTADO_RNG ^= ESTADO_RNG >> 27;
    return ESTADO_RNG * 0x2545F4914F6CDD1Dull;
}

static void SemearRng(uint64_t semente)
{
    ESTADO_RNG = semente != 0u ? semente : SEMENTE_FIXA;
}

/* Referencia: / e % nativos sobre o tipo de largura exata (int64_t com sinal, uint64_t sem sinal), truncando para zero.
 * Chamada so com d != 0 e, com sinal, d != -1 (o MIN / -1 estoura e o planejador nao recebe). */
static void DividirRef(uint32_t bits, bool sinal, uint64_t a, uint64_t d, uint64_t *q, uint64_t *r)
{
    uint64_t mascara = MascaraRef(bits);
    int64_t num;
    int64_t den;

    if (!sinal) {
        *q = (a / d) & mascara;
        *r = (a % d) & mascara;
        return;
    }
    num = EstenderRef(a, bits);
    den = EstenderRef(d, bits);
    *q = (uint64_t)(num / den) & mascara;
    *r = (uint64_t)(num % den) & mascara;
}

static void ImprimirValor(uint64_t padrao, uint32_t bits, bool sinal)
{
    if (sinal)
        printf("%lld", (long long)EstenderRef(padrao, bits));
    else
        printf("%llu", (unsigned long long)(padrao & MascaraRef(bits)));
}

/* Divergencia: imprime o caso e sai com 1; a primeira basta para o portao. */
static void Falhar(const Alvo *alvo, uint64_t a, uint64_t qe, uint64_t re, uint64_t qo, uint64_t ro)
{
    printf("DIVERGENCIA bits=%u %s d=", alvo->bits, alvo->sinal ? "com_sinal" : "sem_sinal");
    ImprimirValor(alvo->d, alvo->bits, alvo->sinal);
    printf(" a=");
    ImprimirValor(a, alvo->bits, alvo->sinal);
    printf(" esperado q=");
    ImprimirValor(qe, alvo->bits, alvo->sinal);
    printf(" r=");
    ImprimirValor(re, alvo->bits, alvo->sinal);
    printf(" obtido q=");
    ImprimirValor(qo, alvo->bits, alvo->sinal);
    printf(" r=");
    ImprimirValor(ro, alvo->bits, alvo->sinal);
    printf("\n");
    exit(1);
}

static void ConferirA(const Alvo *alvo, uint64_t a)
{
    uint64_t dividendo = a & MascaraRef(alvo->bits);
    uint64_t qe;
    uint64_t re;
    uint64_t ro;
    uint64_t qo;

    DividirRef(alvo->bits, alvo->sinal, dividendo, alvo->d, &qe, &re);
    qo = DivMagicaEmular(&alvo->plano, dividendo, &ro);
    CONTAGEM[IndiceBits(alvo->bits)][alvo->sinal ? 1 : 0]++;
    if (qe != qo || re != ro)
        Falhar(alvo, dividendo, qe, re, qo, ro);
}

/* Planeja d. A recusa (false) so e aceita para d = 0 e, com sinal, d = -1; qualquer outra recusa sai com 1. */
static bool PrepararAlvo(Alvo *alvo, uint32_t bits, bool sinal, uint64_t d)
{
    bool sem_plano_esperado;

    alvo->bits = bits;
    alvo->sinal = sinal;
    alvo->d = d & MascaraRef(bits);
    if (DivMagicaPlanejar(bits, sinal, alvo->d, &alvo->plano))
        return true;
    sem_plano_esperado = alvo->d == 0u || (sinal && EstenderRef(alvo->d, bits) == -1);
    if (!sem_plano_esperado) {
        printf("FALHA planejador recusou d valido: bits=%u %s d=", bits, sinal ? "com_sinal" : "sem_sinal");
        ImprimirValor(alvo->d, bits, sinal);
        printf("\n");
        exit(1);
    }
    return false;
}

/* Bordas: 0, 1, 2, 3, MAX, MAX-1, MAX-2, MIN, MIN+1, MIN-1 e, para todo k, 2^k - 1, 2^k e 2^k + 1. */
static void ConferirBordas(const Alvo *alvo)
{
    uint64_t mascara = MascaraRef(alvo->bits);
    uint64_t minimo = 1ull << (alvo->bits - 1u);
    uint64_t fixos[] = {0u, 1u, 2u, 3u, mascara, mascara - 1u, mascara - 2u, minimo, minimo + 1u, minimo - 1u};
    uint64_t potencia;

    for (size_t i = 0; i < sizeof(fixos) / sizeof(fixos[0]); i++)
        ConferirA(alvo, fixos[i] & mascara);
    for (uint32_t k = 0; k < alvo->bits; k++) {
        potencia = 1ull << k;
        ConferirA(alvo, (potencia - 1u) & mascara);
        ConferirA(alvo, potencia & mascara);
        ConferirA(alvo, (potencia + 1u) & mascara);
    }
}

/* Multiplos de d e vizinhos: k * d + {-1, 0, +1} e -(k * d) + {-1, 0, +1}, com k pequeno e k perto do maior multiplo que cabe em N bits. */
static void ConferirMultiplos(const Alvo *alvo)
{
    uint64_t mascara = MascaraRef(alvo->bits);
    int64_t dsig = EstenderRef(alvo->d, alvo->bits);
    uint64_t mag = alvo->sinal ? (dsig < 0 ? 0ull - (uint64_t)dsig : (uint64_t)dsig) : alvo->d;
    uint64_t limite = alvo->sinal ? (mascara >> 1) : mascara;
    uint64_t kext = limite / mag;
    uint64_t ks[] = {1u, 2u, 3u, 1000u, kext, kext + 1u, kext > 0u ? kext - 1u : 0u};
    uint64_t kd;

    for (size_t i = 0; i < sizeof(ks) / sizeof(ks[0]); i++) {
        kd = ks[i] * alvo->d;
        for (uint64_t j = 0; j < 3u; j++) {
            ConferirA(alvo, (kd + j - 1u) & mascara);
            ConferirA(alvo, (0ull - kd + j - 1u) & mascara);
        }
    }
}

static void ConferirAleatorios(const Alvo *alvo, uint32_t quantidade)
{
    uint64_t mascara = MascaraRef(alvo->bits);

    for (uint32_t i = 0; i < quantidade; i++)
        ConferirA(alvo, Aleatorio() & mascara);
}

/* Produto modular de 64 bits para o teste de primalidade (so o teste usa __int128). */
static uint64_t MulMod(uint64_t a, uint64_t b, uint64_t m)
{
    return (uint64_t)((unsigned __int128)a * b % m);
}

static uint64_t PotMod(uint64_t base, uint64_t expoente, uint64_t m)
{
    uint64_t r = 1u;

    base %= m;
    while (expoente != 0u) {
        if ((expoente & 1u) != 0u)
            r = MulMod(r, base, m);
        base = MulMod(base, base, m);
        expoente >>= 1;
    }
    return r;
}

/* Miller-Rabin: true = a base prova que n e composto. */
static bool TestemunhaComposta(uint64_t n, uint64_t d, uint32_t s, uint64_t base)
{
    uint64_t x = PotMod(base, d, n);

    if (x == 1u || x == n - 1u)
        return false;
    for (uint32_t i = 1; i < s; i++) {
        x = MulMod(x, x, n);
        if (x == n - 1u)
            return false;
    }
    return true;
}

/* Primalidade deterministica para n < 2^64 com as 12 primeiras primas como bases. */
static bool EhPrimo(uint64_t n)
{
    static const uint64_t BASES[] = {2u, 3u, 5u, 7u, 11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u};
    uint64_t d = n - 1u;
    uint32_t s = 0u;

    if (n < 2u)
        return false;
    for (size_t i = 0; i < sizeof(BASES) / sizeof(BASES[0]); i++) {
        if (n == BASES[i])
            return true;
        if (n % BASES[i] == 0u)
            return false;
    }
    while ((d & 1u) == 0u) {
        d >>= 1;
        s++;
    }
    for (size_t i = 0; i < sizeof(BASES) / sizeof(BASES[0]); i++) {
        if (TestemunhaComposta(n, d, s, BASES[i]))
            return false;
    }
    return true;
}

static uint64_t ProximoPrimo(uint64_t x)
{
    x |= 1u;
    while (!EhPrimo(x))
        x += 2u;
    return x;
}

/* Acrescenta um divisor normalizado a N bits; lista cheia e erro (o teste nao pode perder casos em silencio). */
static size_t Acrescentar(uint64_t *lista, size_t n, uint64_t valor, uint32_t bits)
{
    if (n >= DIVISORES_MAX) {
        printf("FALHA lista de divisores cheia (DIVISORES_MAX=%u)\n", DIVISORES_MAX);
        exit(1);
    }
    lista[n] = valor & MascaraRef(bits);
    return n + 1u;
}

/* Potencias de dois e vizinhos: 2^k - 1, 2^k e 2^k + 1 para todo k < N. */
static size_t AcrescentarPotencias(uint32_t bits, uint64_t *lista, size_t n)
{
    uint64_t potencia;

    for (uint32_t k = 0; k < bits; k++) {
        potencia = 1ull << k;
        n = Acrescentar(lista, n, potencia - 1u, bits);
        n = Acrescentar(lista, n, potencia, bits);
        n = Acrescentar(lista, n, potencia + 1u, bits);
    }
    return n;
}

/* 1..2000, -2..-2000, extremos, >= 2^(N-1) sem sinal, primos grandes e aleatorios (seed ja semeado pelo chamador). */
static size_t ListarDivisores(uint32_t bits, bool sinal, uint64_t *lista)
{
    uint64_t mascara = MascaraRef(bits);
    uint64_t minimo = 1ull << (bits - 1u);
    uint64_t faixa = minimo - 1u;
    size_t n = 0;

    n = AcrescentarPotencias(bits, lista, n);
    for (uint64_t v = 1u; v <= 2000u; v++)
        n = Acrescentar(lista, n, v, bits);
    for (uint64_t v = 2u; v <= 2000u; v++)
        n = Acrescentar(lista, n, 0ull - v, bits);
    n = Acrescentar(lista, n, sinal ? minimo - 1u : mascara, bits);
    n = Acrescentar(lista, n, sinal ? minimo : 0u, bits);
    n = Acrescentar(lista, n, sinal ? minimo + 1u : 1u, bits);
    if (!sinal) {
        for (uint32_t i = 0; i < 500u; i++)
            n = Acrescentar(lista, n, minimo + (Aleatorio() & faixa), bits);
    }
    for (uint32_t i = 0; i < PRIMOS_GRANDES; i++)
        n = Acrescentar(lista, n, ProximoPrimo((minimo >> 1) + (Aleatorio() & ((minimo >> 1) - 1u))), bits);
    for (uint32_t i = 0; i < ALEATORIOS_D; i++)
        n = Acrescentar(lista, n, Aleatorio() & mascara, bits);
    return n;
}

static void VarrerOito(bool sinal)
{
    Alvo alvo;

    for (uint64_t d = 0; d <= 255u; d++) {
        if (!PrepararAlvo(&alvo, 8u, sinal, d))
            continue;
        for (uint64_t a = 0; a <= 255u; a++)
            ConferirA(&alvo, a);
    }
}

/* 16 bits: todo d; a = bordas, multiplos, 1 a cada STRIDE_16 e ALEATORIOS_16 por d (exaustivo: todo a). */
static void VarrerDezesseis(bool sinal, bool exaustivo)
{
    Alvo alvo;

    SemearRng(SEMENTE_FIXA ^ (sinal ? 0x16u : 0u));
    for (uint64_t d = 0; d <= 65535u; d++) {
        if (!PrepararAlvo(&alvo, 16u, sinal, d))
            continue;
        if (exaustivo) {
            for (uint64_t a = 0; a <= 65535u; a++)
                ConferirA(&alvo, a);
            continue;
        }
        ConferirBordas(&alvo);
        ConferirMultiplos(&alvo);
        for (uint64_t a = 0; a <= 65535u; a += STRIDE_16)
            ConferirA(&alvo, a);
        ConferirAleatorios(&alvo, ALEATORIOS_16);
    }
}

/* 32 e 64 bits: cada d da lista x bordas, multiplos e ALEATORIOS_32_64 aleatorios por d (seed fixo por largura e sinal). */
static void VarrerGrande(uint32_t bits, bool sinal)
{
    static uint64_t LISTA[DIVISORES_MAX];
    Alvo alvo;
    size_t n;

    SemearRng(SEMENTE_FIXA ^ ((uint64_t)bits * 2u + (sinal ? 1u : 0u)));
    n = ListarDivisores(bits, sinal, LISTA);
    for (size_t i = 0; i < n; i++) {
        if (!PrepararAlvo(&alvo, bits, sinal, LISTA[i]))
            continue;
        ConferirBordas(&alvo);
        ConferirMultiplos(&alvo);
        ConferirAleatorios(&alvo, ALEATORIOS_32_64);
    }
}

/* Entradas que o planejador deve recusar ou aceitar de borda: largura invalida, d = 0, d = -1 com sinal, d = -1 sem sinal (valido). */
static void ChecarEntradasDeBorda(void)
{
    DivMagicaPlano plano;

    if (DivMagicaPlanejar(7u, false, 3u, &plano) || DivMagicaPlanejar(8u, false, 0u, &plano)
        || DivMagicaPlanejar(8u, true, 0xffu, &plano) || DivMagicaPlanejar(64u, true, ~0ull, &plano)
        || !DivMagicaPlanejar(64u, false, ~0ull, &plano)) {
        printf("FALHA entradas de borda do planejador\n");
        exit(1);
    }
}

static void RodarSelftest(void)
{
    static uint64_t LISTA[DIVISORES_MAX];
    Alvo alvo;
    size_t n;

    VarrerOito(false);
    VarrerOito(true);
    for (uint64_t d = 0; d <= 65535u; d += 509u) {
        for (int s = 0; s < 2; s++) {
            if (!PrepararAlvo(&alvo, 16u, s != 0, d))
                continue;
            ConferirBordas(&alvo);
            ConferirMultiplos(&alvo);
            ConferirAleatorios(&alvo, SELFTEST_ALEATORIOS);
        }
    }
    for (uint32_t bits = 32u; bits <= 64u; bits *= 2u) {
        for (int s = 0; s < 2; s++) {
            SemearRng(SEMENTE_FIXA ^ ((uint64_t)bits * 2u + (uint64_t)s));
            n = ListarDivisores(bits, s != 0, LISTA);
            for (size_t i = 0; i < n; i += 97u) {
                if (!PrepararAlvo(&alvo, bits, s != 0, LISTA[i]))
                    continue;
                ConferirBordas(&alvo);
                ConferirMultiplos(&alvo);
                ConferirAleatorios(&alvo, SELFTEST_ALEATORIOS);
            }
        }
    }
}

static void RodarBateria(bool exaustivo)
{
    for (int s = 0; s < 2; s++)
        VarrerOito(s != 0);
    for (int s = 0; s < 2; s++)
        VarrerDezesseis(s != 0, exaustivo);
    for (uint32_t bits = 32u; bits <= 64u; bits *= 2u) {
        for (int s = 0; s < 2; s++)
            VarrerGrande(bits, s != 0);
    }
}

static void ImprimirResumo(const char *modo)
{
    static const uint32_t BITS[4] = {8u, 16u, 32u, 64u};
    uint64_t total = 0;

    printf("teste_x64_div_magica 1.0.0 modo=%s\n", modo);
    for (uint32_t i = 0; i < 4u; i++) {
        printf("  bits=%-2u sem_sinal casos=%llu  com_sinal casos=%llu\n", BITS[i],
               (unsigned long long)CONTAGEM[i][0], (unsigned long long)CONTAGEM[i][1]);
        total += CONTAGEM[i][0] + CONTAGEM[i][1];
    }
    printf("RESULTADO: OK, %llu casos, 0 divergencias\n", (unsigned long long)total);
}

int main(int argc, char **argv)
{
    bool selftest = false;
    bool exaustivo = false;
    const char *modo = "padrao";

    if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
        selftest = true;
        modo = "selftest";
    } else if (argc == 2 && strcmp(argv[1], "--exaustivo") == 0) {
        exaustivo = true;
        modo = "exaustivo";
    } else if (argc != 1) {
        fprintf(stderr, "uso: %s [--selftest | --exaustivo]\n", argv[0]);
        return 2;
    }
    ChecarEntradasDeBorda();
    if (selftest)
        RodarSelftest();
    else
        RodarBateria(exaustivo);
    ImprimirResumo(modo);
    return 0;
}
