/* x64_div_magica.c - planejador e emulador de divisao por constante (magic number): parte pura do backend x64, sem BIR nem emissor.
 * Versao: 1.1.0
 * Changelog: 1.1.0 - M7b F1 rodada 2: DivMagicaEmular reproduz o resto com sinal do emissor (SINAL_MUL/SINAL_64: (n - (n<0 ? |d| : 0)) - q0*|d|; POT2_SINAL: ((n + t) and mascara) - t).
 *            1.0.0 - estrategias UM, POT2, POT2_SINAL, LEMIRE, SEM_SINAL_64, MAIOR_SEM_SINAL, SINAL_MUL, SINAL_64 e SINAL_MIN; emulador em 64 bits com hi64 portatil.
 * Verificacao: tests\unidade\teste_x64_div_magica.c --selftest (ver o cabecalho desse teste).
 */
#include "nativo/x64/x64_div_magica.h"

#define DIV_MAGICA_TOPO_64 (1ull << 63)

/* Mascara dos N bits baixos (N = 64 inteiro). */
static uint64_t Mascara(uint32_t bits)
{
    return bits >= 64u ? ~0ull : (1ull << bits) - 1ull;
}

static bool BitsValidos(uint32_t bits)
{
    return bits == 8u || bits == 16u || bits == 32u || bits == 64u;
}

/* Padrao de N bits com o bit N-1 ligado vira negativo: devolve o valor com sinal. */
static int64_t EstenderSinal(uint64_t padrao, uint32_t bits)
{
    uint64_t valor = padrao & Mascara(bits);
    if (bits < 64u && ((valor >> (bits - 1u)) & 1ull) != 0u)
        valor |= ~Mascara(bits);
    return (int64_t)valor;
}

static bool EhPotDois(uint64_t x)
{
    return (x & (x - 1ull)) == 0u;
}

/* Expoente de uma potencia de dois (x = 2^k, k <= 63). */
static uint32_t Log2Exato(uint64_t x)
{
    uint32_t k = 0;
    while (k < 63u && (1ull << k) < x)
        k++;
    return k;
}

/* Menor l com 2^l >= x (x <= 2^63): piso do bit que contem x, usado como precisao do magic. */
static uint32_t Log2Teto(uint64_t x)
{
    uint32_t l = 0;
    while (l < 63u && (1ull << l) < x)
        l++;
    return l;
}

/* floor(2^p / d) em 128 bits (qhi:qlo), por divisao restauradora bit a bit (so o bit p do dividendo esta ligado).
 * Existe porque 2^p nao cabe em 64 bits para os magic de u64 e i64. d > 0, p <= 127. */
static void DivisaoPotDois(uint64_t d, uint32_t p, uint64_t *qlo, uint64_t *qhi)
{
    uint64_t resto = 0;
    *qlo = 0;
    *qhi = 0;
    for (uint32_t i = p + 1u; i-- > 0u;) {
        /* o resto pode ter 65 bits depois do deslocamento: o bit que sai vai para transbordo */
        bool transbordo = (resto >> 63) != 0u;
        resto = (resto << 1) | (i == p ? 1ull : 0ull);
        if (transbordo || resto >= d) {
            resto -= d;
            if (i >= 64u)
                *qhi |= 1ull << (i - 64u);
            else
                *qlo |= 1ull << i;
        }
    }
}

static bool PlanejarSemSinal(DivMagicaPlano *saida)
{
    uint64_t d = saida->divisor;
    uint32_t l;
    uint64_t qlo, qhi;

    if (d == 0u)
        return false;
    if (d == 1u) {
        saida->estrategia = DIV_MAGICA_UM;
        return true;
    }
    if (EhPotDois(d)) {
        saida->estrategia = DIV_MAGICA_POT2;
        saida->shift = Log2Exato(d);
        return true;
    }
    /* N <= 32: c = ceil(2^64 / d) basta para todo a < 2^32 (erro a * e < 2^64) */
    if (saida->bits <= 32u) {
        saida->estrategia = DIV_MAGICA_LEMIRE;
        saida->magic = (~0ull / d) + 1ull;
        return true;
    }
    if (d >= DIV_MAGICA_TOPO_64) {
        saida->estrategia = DIV_MAGICA_MAIOR_SEM_SINAL;
        return true;
    }
    /* u64: M = floor(2^(64+l) / d) + 1 e sempre >= 2^64 (d <= 2^l), entao vai como 65 bits: magic = M - 2^64 */
    l = Log2Teto(d);
    DivisaoPotDois(d, 64u + l, &qlo, &qhi);
    saida->estrategia = DIV_MAGICA_SEM_SINAL_64;
    saida->magic = qlo + 1ull;
    saida->shift = l;
    return true;
}

static bool PlanejarSinalNaoPotDois(DivMagicaPlano *saida, uint64_t mag)
{
    uint32_t l = Log2Teto(mag);
    uint32_t k;
    uint64_t qlo, qhi;

    saida->soma_bit_sinal = true;
    if (saida->bits <= 32u) {
        /* k = (N - 1) + l: erro |n| * e < 2^k; M cabe em N bits e n * M cabe em 32 bits (i8, i16) ou 64 (i32) */
        k = saida->bits - 1u + l;
        saida->estrategia = DIV_MAGICA_SINAL_MUL;
        saida->magic = (1ull << k) / mag + 1ull;
        saida->shift = k;
        saida->produto_64 = saida->bits == 32u;
        return true;
    }
    /* i64: M = floor(2^(63+l) / |d|) + 1 cabe em 64 bits e, como int64, e negativo: entra a correcao soma_a */
    DivisaoPotDois(mag, 63u + l, &qlo, &qhi);
    saida->estrategia = DIV_MAGICA_SINAL_64;
    saida->magic = qlo + 1ull;
    saida->shift = l - 1u;
    saida->soma_a = (int64_t)saida->magic < 0;
    return true;
}

static bool PlanejarSinal(DivMagicaPlano *saida)
{
    int64_t n = EstenderSinal(saida->divisor, saida->bits);
    uint64_t mag;

    if (n == 0 || n == -1)
        return false;
    if (saida->divisor == (1ull << (saida->bits - 1u))) {
        saida->estrategia = DIV_MAGICA_SINAL_MIN;
        return true;
    }
    saida->negar = n < 0;
    mag = saida->negar ? 0ull - (uint64_t)n : (uint64_t)n;
    if (mag == 1u) {
        saida->estrategia = DIV_MAGICA_UM;
        return true;
    }
    if (EhPotDois(mag)) {
        saida->estrategia = DIV_MAGICA_POT2_SINAL;
        saida->shift = Log2Exato(mag);
        return true;
    }
    return PlanejarSinalNaoPotDois(saida, mag);
}

bool DivMagicaPlanejar(uint32_t bits, bool com_sinal, uint64_t divisor, DivMagicaPlano *saida)
{
    *saida = (DivMagicaPlano){.estrategia = DIV_MAGICA_NAO_OTIMIZAR, .bits = bits, .com_sinal = com_sinal,
                              .divisor = divisor & Mascara(bits)};
    if (!BitsValidos(bits))
        return false;
    return com_sinal ? PlanejarSinal(saida) : PlanejarSemSinal(saida);
}

/* Produto alto 64x64 sem __int128: quatro produtos de 32x32 (a mesma conta do mulhi de Hacker's Delight). */
static uint64_t Hi64(uint64_t u, uint64_t v)
{
    uint64_t u0 = u & 0xffffffffull, u1 = u >> 32;
    uint64_t v0 = v & 0xffffffffull, v1 = v >> 32;
    uint64_t w0 = u0 * v0;
    uint64_t t = u1 * v0 + (w0 >> 32);
    uint64_t w1 = t & 0xffffffffull;
    uint64_t w2 = t >> 32;
    w1 = u0 * v1 + w1;
    return u1 * v1 + w2 + (w1 >> 32);
}

/* Produto alto com sinal (complemento de dois): corrige a conta sem sinal quando um operando e negativo. */
static uint64_t HiSinal64(int64_t x, int64_t y)
{
    uint64_t hi = Hi64((uint64_t)x, (uint64_t)y);
    if (x < 0)
        hi -= (uint64_t)y;
    if (y < 0)
        hi -= (uint64_t)x;
    return hi;
}

static uint64_t QuocienteSemSinal(const DivMagicaPlano *plano, uint64_t a)
{
    uint64_t t;

    switch (plano->estrategia) {
    case DIV_MAGICA_UM:
        return a;
    case DIV_MAGICA_POT2:
        return a >> plano->shift;
    case DIV_MAGICA_LEMIRE:
        return Hi64(a, plano->magic);
    case DIV_MAGICA_MAIOR_SEM_SINAL:
        return a >= plano->divisor ? 1u : 0u;
    case DIV_MAGICA_SEM_SINAL_64:
        t = Hi64(a, plano->magic);
        return (((a - t) >> 1) + t) >> (plano->shift - 1u);
    default:
        return 0u;
    }
}

/* Dividendo com sinal, divisor 2^s: o vies 2^s - 1 so entra para n < 0 (arredonda para zero). */
static uint64_t QuocientePot2Sinal(const DivMagicaPlano *plano, int64_t n)
{
    uint64_t vies = ((uint64_t)(n >> 63)) >> (64u - plano->shift);
    return (uint64_t)((int64_t)((uint64_t)n + vies) >> plano->shift);
}

/* imul de 32 bits (i8, i16) ou de 64 bits (i32); o produto cabe no operando escolhido, entao o sar e exato. */
static uint64_t QuocienteMultiplicacao(const DivMagicaPlano *plano, int64_t n)
{
    int64_t produto;

    if (plano->produto_64)
        produto = (int64_t)((uint64_t)n * plano->magic);
    else
        produto = (int32_t)((uint32_t)n * (uint32_t)plano->magic);
    return (uint64_t)(produto >> plano->shift);
}

/* i64: hi64 com sinal de n * M, mais n quando soma_a (M como int64 e negativo), depois sar s. */
static uint64_t QuocienteSinal64(const DivMagicaPlano *plano, int64_t n)
{
    uint64_t t = HiSinal64(n, (int64_t)plano->magic);

    if (plano->soma_a)
        t += (uint64_t)n;
    return (uint64_t)(((int64_t)t) >> plano->shift);
}

/* Quociente com sinal antes da negacao de d: as estrategias de magic somam 1 quando o dividendo e negativo. */
static uint64_t QuocienteComSinal(const DivMagicaPlano *plano, int64_t n, uint64_t a)
{
    uint64_t q;

    switch (plano->estrategia) {
    case DIV_MAGICA_UM:
        return (uint64_t)n;
    case DIV_MAGICA_POT2_SINAL:
        return QuocientePot2Sinal(plano, n);
    case DIV_MAGICA_SINAL_MUL:
        q = QuocienteMultiplicacao(plano, n);
        break;
    case DIV_MAGICA_SINAL_64:
        q = QuocienteSinal64(plano, n);
        break;
    case DIV_MAGICA_SINAL_MIN:
        return a == plano->divisor ? 1u : 0u;
    default:
        return 0u;
    }
    if (plano->soma_bit_sinal && n < 0)
        q += 1u;
    return q;
}

static uint64_t EmularComSinal(const DivMagicaPlano *plano, uint64_t a)
{
    int64_t n = EstenderSinal(a, plano->bits);
    uint64_t q = QuocienteComSinal(plano, n, a);

    if (plano->negar)
        q = 0u - q;
    return q;
}

/* |d| de um divisor com sinal (N bits baixos em complemento de dois). */
static uint64_t ModuloDoDivisor(const DivMagicaPlano *plano)
{
    int64_t d = EstenderSinal(plano->divisor, plano->bits);

    return d < 0 ? 0ull - (uint64_t)d : (uint64_t)d;
}

/* Resto com sinal pelas formas do emissor (o resto leva o sinal do dividendo, entao |d| serve e a negacao do quociente some):
 *   SINAL_MUL e SINAL_64: r = (n - (n < 0 ? |d| : 0)) - q0 * |d|, com q0 = quociente SEM a correcao do bit de sinal;
 *   POT2_SINAL: t = (n sar 63) shr (64 - s) (2^s - 1 se n < 0); r = ((n + t) and (2^s - 1)) - t. */
static uint64_t RestoComSinal(const DivMagicaPlano *plano, uint64_t dividendo)
{
    int64_t n = EstenderSinal(dividendo, plano->bits);
    uint64_t mag = ModuloDoDivisor(plano);
    uint64_t q0;
    uint64_t t;

    if (plano->estrategia == DIV_MAGICA_POT2_SINAL) {
        t = ((uint64_t)(n >> 63)) >> (64u - plano->shift);
        return (((uint64_t)n + t) & ((1ull << plano->shift) - 1ull)) - t;
    }
    q0 = plano->estrategia == DIV_MAGICA_SINAL_MUL ? QuocienteMultiplicacao(plano, n) : QuocienteSinal64(plano, n);
    return ((uint64_t)n - (n < 0 ? mag : 0ull)) - q0 * mag;
}

/* Demais estrategias: r = a - q * d (sub do produto; o emissor nao nega mais o produto). */
static uint64_t RestoDoPlano(const DivMagicaPlano *plano, uint64_t dividendo, uint64_t q)
{
    DivMagicaEstrategia e = plano->estrategia;

    if (plano->com_sinal && (e == DIV_MAGICA_SINAL_MUL || e == DIV_MAGICA_SINAL_64 || e == DIV_MAGICA_POT2_SINAL))
        return RestoComSinal(plano, dividendo);
    return dividendo - q * plano->divisor;
}

uint64_t DivMagicaEmular(const DivMagicaPlano *plano, uint64_t a, uint64_t *resto)
{
    uint64_t mascara = Mascara(plano->bits);
    uint64_t dividendo = a & mascara;
    uint64_t q = plano->com_sinal ? EmularComSinal(plano, dividendo) : QuocienteSemSinal(plano, dividendo);

    q &= mascara;
    if (resto)
        *resto = RestoDoPlano(plano, dividendo, q) & mascara;
    return q;
}
