/*
 * banco_math.c 0.1.0 - bits das funcoes de forja_rt_math.h em entradas deterministicas.
 * Saida: uma linha por entrada: <nome> <bits x> <bits y> <bits resultado>.
 *   double = 16 hex; float = 8 hex. Com 1 argumento, y sai 0.
 * Uso: banco_math.exe > saida.txt. Quem roda e verificar_math.py (compila duas vezes e compara).
 * Entradas: semente fixa; mistura de grade uniforme, especiais (+-0, +-1, +-inf, NaN, bordas
 * do Anexo F), perto de k*pi/2, subnormais, grandes ate 1e300, faixa perto de 1 e bits aleatorios.
 * Em 2 argumentos, 1 de cada 4 pares sai do produto cartesiano dos especiais.
 * Gerador so com inteiros e operacoes IEEE simples: a entrada nao depende de FMA nem de contracao.
 */
#include <stdio.h>
#include "forja_rt_math.h"

#define N_ENTRADAS 50000
#define N_GRADE 10000
#define PIO2 1.5707963267948966

/* Bits de double dos especiais; NaN com e sem sinal, e 3pi/2 por ultimo. */
static const unsigned long long ESPECIAIS_BITS[] = {
	0x0000000000000000ULL, 0x8000000000000000ULL, 0x3ff0000000000000ULL, 0xbff0000000000000ULL,
	0x4000000000000000ULL, 0xc000000000000000ULL, 0x3fe0000000000000ULL, 0xbfe0000000000000ULL,
	0x4008000000000000ULL, 0xc008000000000000ULL, 0x0000000000000001ULL, 0x8000000000000001ULL,
	0x7fefffffffffffffULL, 0xffefffffffffffffULL, 0x7ff0000000000000ULL, 0xfff0000000000000ULL,
	0x7ff8000000000000ULL, 0xfff8000000000001ULL, 0x3ff921fb54442d18ULL, 0x4012d97c7f3321d2ULL,
};
#define N_ESPECIAIS (sizeof(ESPECIAIS_BITS) / sizeof(ESPECIAIS_BITS[0]))

struct Funcao {
	const char *nome;
	int aridade;
	int flt;
	double (*fd)(double, double);
	float (*ff)(float, float);
};

#define FUNCOES_1(X) X(sin) X(cos) X(tan) X(asin) X(acos) X(atan) X(exp) X(exp2) \
	X(log) X(log2) X(log10) X(cbrt) X(floor) X(ceil) X(round) X(trunc)
#define FUNCOES_2(X) X(atan2) X(pow) X(hypot) X(fmod) X(copysign)

#define ADAPTA_1(N) \
	static double ad_##N(double x, double y) { (void)y; return N(x); } \
	static float af_##N(float x, float y) { (void)y; return N##f(x); }
#define ADAPTA_2(N) \
	static double ad_##N(double x, double y) { return N(x, y); } \
	static float af_##N(float x, float y) { return N##f(x, y); }
#define ENTRA_1(N) {#N, 1, 0, ad_##N, 0}, {#N "f", 1, 1, 0, af_##N},
#define ENTRA_2(N) {#N, 2, 0, ad_##N, 0}, {#N "f", 2, 1, 0, af_##N},

FUNCOES_1(ADAPTA_1)
FUNCOES_2(ADAPTA_2)

static const struct Funcao TABELA[] = {
	FUNCOES_1(ENTRA_1)
	FUNCOES_2(ENTRA_2)
};

static unsigned long long prox(unsigned long long *est)
{
	unsigned long long z;
	*est += 0x9E3779B97F4A7C15ULL;
	z = *est;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

static double de_bits_d(unsigned long long b)
{
	union { double d; unsigned long long u; } v;
	v.u = b;
	return v.d;
}

static unsigned long long bits_d(double d)
{
	union { double d; unsigned long long u; } v;
	v.d = d;
	return v.u;
}

static float de_bits_f(unsigned int b)
{
	union { float f; unsigned int u; } v;
	v.u = b;
	return v.f;
}

static unsigned int bits_f(float f)
{
	union { float f; unsigned int u; } v;
	v.f = f;
	return v.u;
}

static double grade_d(unsigned long long i, unsigned long long sel)
{
	static const double lo[3] = {-8.0, -800.0, 0.0};
	static const double hi[3] = {8.0, 800.0, 1000.0};
	double s = (double)(i % N_GRADE) / (double)(N_GRADE - 1);
	return lo[sel] + (hi[sel] - lo[sel]) * s;
}

/* Perto de k*pi/2: k em [-20000, 20000] e um deslocamento pequeno. */
static double pio2_perto(unsigned long long r)
{
	long long k = (long long)((r >> 8) % 40001ULL) - 20000LL;
	long long d = (long long)((r >> 40) % 2001ULL) - 1000LL;
	return (double)k * PIO2 + (double)d * 1e-16;
}

static double amostra_d(unsigned long long *est, unsigned long long i)
{
	unsigned long long r = prox(est);
	unsigned long long sinal = (r & 1ULL) << 63;
	unsigned long long mant = (r >> 12) & 0x000FFFFFFFFFFFFFULL;
	switch ((r >> 1) % 10ULL) {
	case 0: case 1: return grade_d(i, (r >> 8) % 3ULL);
	case 2: return de_bits_d(ESPECIAIS_BITS[(r >> 8) % N_ESPECIAIS]);
	case 3: return pio2_perto(r);
	case 4: return de_bits_d(sinal | mant);
	case 5: return de_bits_d(sinal | ((1023ULL + (r >> 8) % 997ULL) << 52) | mant);
	case 6: return de_bits_d(sinal | ((993ULL + (r >> 8) % 61ULL) << 52) | mant);
	default: return de_bits_d(r);
	}
}

static float amostra_f(unsigned long long *est, unsigned long long i)
{
	unsigned long long r = prox(est);
	unsigned int sinal = (unsigned int)(r & 1ULL) << 31;
	unsigned int mant = (unsigned int)((r >> 12) & 0x007FFFFFULL);
	switch ((r >> 1) % 10ULL) {
	case 0: case 1: return (float)grade_d(i, (r >> 8) % 3ULL);
	case 2: return (float)de_bits_d(ESPECIAIS_BITS[(r >> 8) % N_ESPECIAIS]);
	case 3: return (float)pio2_perto(r);
	case 4: return de_bits_f(sinal | mant);
	case 5: return de_bits_f(sinal | ((127U + (unsigned int)((r >> 8) % 127ULL)) << 23) | mant);
	case 6: return de_bits_f(sinal | ((107U + (unsigned int)((r >> 8) % 41ULL)) << 23) | mant);
	default: return de_bits_f((unsigned int)(r >> 32));
	}
}

/* Par de especiais: j = i/4 percorre os 20 x 20 pares. */
static void par_especial(unsigned long long j, double *x, double *y)
{
	*x = de_bits_d(ESPECIAIS_BITS[(j / N_ESPECIAIS) % N_ESPECIAIS]);
	*y = de_bits_d(ESPECIAIS_BITS[j % N_ESPECIAIS]);
}

static void gera_par_d(int aridade, unsigned long long *est, unsigned long long i, double *x, double *y)
{
	*x = amostra_d(est, i);
	*y = 0.0;
	if (aridade != 2)
		return;
	if (i % 4 == 0) {
		par_especial(i / 4, x, y);
		return;
	}
	*y = amostra_d(est, i);
}

static void gera_par_f(int aridade, unsigned long long *est, unsigned long long i, float *x, float *y)
{
	double dx = 0.0, dy = 0.0;
	*x = amostra_f(est, i);
	*y = 0.0F;
	if (aridade != 2)
		return;
	if (i % 4 == 0) {
		par_especial(i / 4, &dx, &dy);
		*x = (float)dx;
		*y = (float)dy;
		return;
	}
	*y = amostra_f(est, i);
}

static void imprime_d(const struct Funcao *f, unsigned long long semente)
{
	unsigned long long est = semente, i;
	double x, y, r;
	for (i = 0; i < N_ENTRADAS; i++) {
		gera_par_d(f->aridade, &est, i, &x, &y);
		r = f->fd(x, y);
		printf("%s %016llx %016llx %016llx\n", f->nome, bits_d(x), bits_d(y), bits_d(r));
	}
}

static void imprime_f(const struct Funcao *f, unsigned long long semente)
{
	unsigned long long est = semente, i;
	float x, y, r;
	for (i = 0; i < N_ENTRADAS; i++) {
		gera_par_f(f->aridade, &est, i, &x, &y);
		r = f->ff(x, y);
		printf("%s %08x %08x %08x\n", f->nome, bits_f(x), bits_f(y), bits_f(r));
	}
}

int main(void)
{
	size_t k;
	for (k = 0; k < sizeof(TABELA) / sizeof(TABELA[0]); k++) {
		unsigned long long semente = 0x5EED0000ULL + (unsigned long long)k * 0x9E37ULL;
		if (TABELA[k].flt)
			imprime_f(&TABELA[k], semente);
		else
			imprime_d(&TABELA[k], semente);
	}
	return 0;
}
