/*
 * teste_rt_nativo.c - 14 casos do runtime nativo da Forja (P2a, P7a, P10a, P12b, P13a). Escolha o caso com -DCASO=1..14.
 *
 * Versao: 0.8.0
 * Changelog:
 *   0.8.0 - Linguagem v1.11 L16b: casos 29 (vazamento por teste: o teste que deixa bloco de heap vivo falha com R0009) e 30 (queda por teste: acesso invalido e instrucao ilegal viram R0011 e o proximo teste roda); os dois em dev
 *           e com FORJA_TESTE_JSON=1 (a 1a falha vai ao JSON).
 *   0.7.0 -Linguagem v1.11 L16a: casos 27 (executor de testes com FORJA_TESTE_FILTRO) e 28 (com FORJA_TESTE_JSON=1); o verificador poe as variaveis no ambiente do oraculo e do nativo.
 *   0.6.0 - M2 P16: caso 26 (forja_rt_conta_bits por SWAR contra __builtin_popcountll: bordas, um bit de cada posicao e 200 mil valores de xorshift64), em release.
 *   0.5.0 - M2 P13a: caso 14 (forja_str_igual e forja_str_de_cstr, em release: NULL vira str vazia).
 *   0.4.0 - M2 P12b: caso 13 (forja_rt_salvar/forja_rt_saltar do rt_salto.c contra o setjmp/longjmp da CRT: 1e6 voltas, registradores preservados, rsp, xmm6).
 *   0.8.1 - Ryu d2fixed P4: o caso 10 (forja_print_f32) saiu junto com a funcao (codigo morto no nativo: print de real vira f"{x}"); o numero 10 nao e reusado.
 *   0.3.0 - P10a: casos 10 (forja_print_f32 de borda), 11 (R0002 de real em dev: NaN as i64) e 12 (saturacao
 *           de todos os S em release). O caso 11 pede -DFORJA_DEV no oraculo e no rt_win.o.
 *   0.2.0 - caso 9: forja_panic (R0003, exit 101) contra o forja_rt.h.
 *   0.1.0 - primeira versao: casos 1 a 8 (inteiros e bool, texto e str, 3000 linhas, R0001, estouro u64,
 *           conversoes, R0004 e handlers de saida).
 *
 * -DORACULO: usa o forja_rt.h e o CRT do gcc. Sem -DORACULO: liga com rt_win.o (entrada forja_entrada).
 * Cada caso termina com um codigo de saida; verificar_rt_nativo.py compara stdout, stderr e exit com o oraculo.
 * Uso: gcc ... -DCASO=1 -DORACULO teste_rt_nativo.c -o oraculo.exe
 *      gcc ... -DCASO=1 -c teste_rt_nativo.c -o teste.o && gcc -nostdlib -ffreestanding -mno-stack-arg-probe
 *          -Wl,-e,forja_entrada teste.o rt_win.o -lkernel32 -o nativo.exe
 */
#ifndef CASO
#error "compile com -DCASO=1..14 ou 26"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ORACULO
#include "../../../runtime/forja_rt.h"
#define REGISTRAR_AO_SAIR(fn) atexit(fn)
/* Caso 13: o oraculo do salto e o setjmp/longjmp da CRT. */
typedef jmp_buf SaltoTeste;
#define SALVAR(s) setjmp(s)
#define SALTAR(s, v) longjmp((s), (v))
#else
/* Mesmos nomes e assinaturas do forja_rt.h 0.9.0; implementados em rt_nativo.c. */
#define FORJA_R0001 "R0001"
typedef struct forja_str {
    const char *ptr;
    int64_t len;
} forja_str;
#define FORJA_STR(lit, n) ((forja_str){(lit), (n)})
void forja_print_i32(int32_t valor);
void forja_print_i64(int64_t valor);
void forja_print_u64(uint64_t valor);
void forja_print_bool(bool valor);
void forja_print_texto(const char *texto);
void forja_print_str(forja_str s);
bool forja_str_igual(forja_str a, forja_str b);
forja_str forja_str_de_cstr(const char *c, const char *arquivo, int linha);
_Noreturn void forja_falha(const char *codigo, const char *causa, const char *arquivo, int linha);
_Noreturn void forja_panic(const char *msg, const char *arquivo, int linha);
_Noreturn void forja_falha_estouro(const char *operacao, int64_t a, int64_t b, const char *arquivo, int linha);
_Noreturn void forja_falha_estouro_u(const char *operacao, uint64_t a, uint64_t b, const char *arquivo, int linha);
_Noreturn void forja_falha_conversao_i(int64_t valor, const char *alvo, const char *arquivo, int linha);
_Noreturn void forja_falha_conversao_u(uint64_t valor, const char *alvo, const char *arquivo, int linha);
void forja_main_erro(const char *arquivo, int erro);
void forja_rt_ao_sair(void (*fn)(void));
int8_t forja_as_i8_de_f64(double v, const char *arquivo, int linha);
int16_t forja_as_i16_de_f64(double v, const char *arquivo, int linha);
int32_t forja_as_i32_de_f64(double v, const char *arquivo, int linha);
uint8_t forja_as_u8_de_f64(double v, const char *arquivo, int linha);
uint16_t forja_as_u16_de_f64(double v, const char *arquivo, int linha);
uint32_t forja_as_u32_de_f64(double v, const char *arquivo, int linha);
int64_t forja_as_i64_de_f64(double v, const char *arquivo, int linha);
uint64_t forja_as_u64_de_f64(double v, const char *arquivo, int linha);
int64_t forja_rt_conta_bits(uint64_t v);
/* Casos 27 e 28: o executor de testes (rt_teste.c). */
typedef void (*ForjaTesteFn)(void);
void forja_expect(bool cond, const char *texto_cond, const char *arquivo, int linha);
void forja_teste_registrar(const char *nome, ForjaTesteFn fn);
int forja_testes_rodar(void);
/* Caso 29: alocacao de heap (rt_aloc.c, com o rastreio de rt_rastreio.c em dev). */
void *forja_heap_alocar(int64_t bytes, const char *arquivo, int linha);
void forja_mem_liberar(void *p, const char *arquivo, int linha);
#define REGISTRAR_AO_SAIR(fn) forja_rt_ao_sair(fn)
/* Caso 13: o salto proprio do rt_salto.c (mesmo layout do ForjaSalto). */
typedef struct {
    _Alignas(16) uint64_t gpr[10];
    uint8_t xmm[160];
    uint32_t mxcsr;
} SaltoTeste;
int forja_rt_salvar(SaltoTeste *s) __attribute__((returns_twice));
_Noreturn void forja_rt_saltar(SaltoTeste *s, int v);
#define SALVAR(s) forja_rt_salvar(&(s))
#define SALTAR(s, v) forja_rt_saltar(&(s), (v))
#endif

#define ARQUIVO_TESTE "teste_rt.fj"

#if CASO == 1
/* Inteiros nas bordas e bool: sinal, INT32_MIN, INT64_MIN (negar estoura i64) e UINT64_MAX. */
int forja_programa(void)
{
    forja_print_i32(INT32_MIN);
    forja_print_i32(INT32_MAX);
    forja_print_i32(0);
    forja_print_i32(-1);
    forja_print_i64(INT64_MIN);
    forja_print_i64(INT64_MAX);
    forja_print_i64(0);
    forja_print_u64(UINT64_MAX);
    forja_print_u64(0);
    forja_print_bool(true);
    forja_print_bool(false);
    return 0;
}
#elif CASO == 2
/* Texto e str: vazio, \n (vira CRLF), \r\n (o \r fica e o \n vira CRLF), UTF-8 e NUL dentro de str. */
int forja_programa(void)
{
    forja_print_texto("");
    forja_print_texto("linha um\nlinha dois");
    forja_print_texto("crlf\r\nfim");
    forja_print_texto("a\xC3\xA7\xC3\xA3o \xE6\x97\xA5 \xE2\x9C\x93");
    forja_print_str(FORJA_STR("", 0));
    forja_print_str(FORJA_STR("a\nb\r\n", 5));
    forja_print_str(FORJA_STR("zero\0meio", 9));
    forja_print_str(FORJA_STR("\xC3\xA7\xE6\x97\xA5", 5));
    return 0;
}
#elif CASO == 3
/* 3000 linhas: cruza o buffer de 4096 B do stdout varias vezes; linhas pares com \n embutido. */
int forja_programa(void)
{
    int64_t i;

    for (i = 1; i <= 3000; i++) {
        if (i % 2 == 0)
            forja_print_str(FORJA_STR("par\n", 4));
        else
            forja_print_i64(i);
    }
    return 0;
}
#elif CASO == 4
/* R0001 com aspas, barra, \n, \t e \x01: exit 101 e o stdout anterior tem que sair inteiro. */
int forja_programa(void)
{
    forja_print_texto("antes do erro");
    forja_falha(FORJA_R0001, "valor \"x\" com \\ barra\n\ttab e controle \x01 fim", "C:\\projeto\\teste_rt.fj", 12);
    return 0;
}
#elif CASO == 5
/* Estouro u64: a causa tem que mostrar 18446744073709551615 e nao -1 (o %lld do estouro i64). */
int forja_programa(void)
{
    forja_print_texto("antes do estouro");
    forja_falha_estouro_u("soma u64", UINT64_MAX, 1, ARQUIVO_TESTE, 7);
    return 0;
}
#elif CASO == 6
/* Conversoes: valores dentro da faixa saem; -300 para u8 e fora da faixa e falha R0002 com sinal. */
int forja_programa(void)
{
    forja_print_u64((uint64_t)(uint8_t)255);
    forja_print_i64((int64_t)-128);
    forja_falha_conversao_i(-300, "u8", ARQUIVO_TESTE, 9);
    return 0;
}
#elif CASO == 7
/* main -> ! E que devolve erro: R0004 em stderr e o main sai com 1. */
int forja_programa(void)
{
    forja_print_texto("antes do erro de main");
    forja_main_erro(ARQUIVO_TESTE, 3);
    return 1;
}
#elif CASO == 8
/* Tres handlers de saida registrados em ordem A, B, C: rodam C, B, A antes de o stdout descarregar. */
static void AoSairA(void)
{
    forja_print_texto("saida A");
}

static void AoSairB(void)
{
    forja_print_texto("saida B");
}

static void AoSairC(void)
{
    forja_print_texto("saida C");
}

int forja_programa(void)
{
    forja_print_texto("inicio");
    REGISTRAR_AO_SAIR(AoSairA);
    REGISTRAR_AO_SAIR(AoSairB);
    REGISTRAR_AO_SAIR(AoSairC);
    return 0;
}
#elif CASO == 9
/* forja_panic: R0003 com aspas e \n na mensagem, exit 101; o stdout anterior sai inteiro. */
int forja_programa(void)
{
    forja_print_texto("antes do panic");
    forja_panic("fim da proc sem retorno \"IR\"\n", ARQUIVO_TESTE, 11);
    return 0;
}
#elif CASO == 11
/* R0002 de real em dev: as valido nas bordas da faixa sai; NaN as i64 falha com exit 101 e o stdout anterior sai inteiro. */
static double DoubleDeBits(uint64_t bits)
{
    union {
        uint64_t u;
        double d;
    } x;

    x.u = bits;
    return x.d;
}

int forja_programa(void)
{
    forja_print_i64(forja_as_i32_de_f64(2147483647.9, ARQUIVO_TESTE, 5));
    forja_print_i64(forja_as_i32_de_f64(-2147483648.9, ARQUIVO_TESTE, 6));
    forja_print_u64(forja_as_u8_de_f64(255.9, ARQUIVO_TESTE, 7));
    forja_print_u64(forja_as_u64_de_f64(-0.5, ARQUIVO_TESTE, 8));
    forja_print_texto("antes do nan");
    forja_print_i64(forja_as_i64_de_f64(DoubleDeBits(0x7FF8000000000000ULL), ARQUIVO_TESTE, 9));
    forja_print_texto("nao deve chegar aqui");
    return 0;
}
#elif CASO == 12
/* Release: real -> inteiro SATURA em todos os S: acima do teto vira o teto, negativo sem sinal vira 0, NaN vira 0. */
static double DoubleDeBits(uint64_t bits)
{
    union {
        uint64_t u;
        double d;
    } x;

    x.u = bits;
    return x.d;
}

static void ImprimirSaturados(double v)
{
    forja_print_i64(forja_as_i8_de_f64(v, ARQUIVO_TESTE, 1));
    forja_print_i64(forja_as_i16_de_f64(v, ARQUIVO_TESTE, 2));
    forja_print_i64(forja_as_i32_de_f64(v, ARQUIVO_TESTE, 3));
    forja_print_u64(forja_as_u8_de_f64(v, ARQUIVO_TESTE, 4));
    forja_print_u64(forja_as_u16_de_f64(v, ARQUIVO_TESTE, 5));
    forja_print_u64(forja_as_u32_de_f64(v, ARQUIVO_TESTE, 6));
    forja_print_i64(forja_as_i64_de_f64(v, ARQUIVO_TESTE, 7));
    forja_print_u64(forja_as_u64_de_f64(v, ARQUIVO_TESTE, 8));
}

int forja_programa(void)
{
    ImprimirSaturados(300.7);
    ImprimirSaturados(-300.7);
    ImprimirSaturados(-0.5);
    ImprimirSaturados(1e300);
    ImprimirSaturados(-1e300);
    ImprimirSaturados(18446744073709551616.0);
    ImprimirSaturados(DoubleDeBits(0x7FF8000000000000ULL));
    ImprimirSaturados(DoubleDeBits(0x7FF0000000000000ULL));
    ImprimirSaturados(DoubleDeBits(0xFFF0000000000000ULL));
    return 0;
}
#elif CASO == 13
/* Salvar e saltar: 1e6 voltas. Em cada uma, 10 registradores preservados do Win64 (rbx rsi rdi r12..r15, xmm6, xmm10, xmm15) recebem valores da volta,
 * o contexto e salvo, Destruir sobrescreve todos eles e salta; na volta os valores e o rsp tem de ser os de antes do salvar. Saida: erros (0), a soma
 * dos valores devolvidos pelo salvar (0 vira 1, como no longjmp) e "salto ok". Estado em globais: nada fica vivo em registrador por cima do salvar. */
enum { VOLTAS = 1000000, REGISTRADORES = 10 };

static SaltoTeste Salto;
static uint64_t Erros, Soma, BaseDaVolta, RspAntes;
static int ValorDoSalto;

static uint64_t LerRsp(void)
{
    uint64_t rsp;

    __asm__ volatile("movq %%rsp, %0" : "=r"(rsp));
    return rsp;
}

static __attribute__((noinline)) void Destruir(void)
{
    __asm__ volatile("movq $-1, %%rbx\n\tmovq $-2, %%rsi\n\tmovq $-3, %%rdi\n\tmovq $-4, %%r12\n\tmovq $-5, %%r13\n\tmovq $-6, %%r14\n\tmovq $-7, %%r15\n\t"
                     "pcmpeqd %%xmm6, %%xmm6\n\tpcmpeqd %%xmm10, %%xmm10\n\tpcmpeqd %%xmm15, %%xmm15"
                     : : : "rbx", "rsi", "rdi", "r12", "r13", "r14", "r15", "xmm6", "xmm10", "xmm15");
    SALTAR(Salto, ValorDoSalto);
}

static void ArmarRegistradores(uint64_t base)
{
    __asm__ volatile("leaq 1(%0), %%rbx\n\tleaq 2(%0), %%rsi\n\tleaq 3(%0), %%rdi\n\tleaq 4(%0), %%r12\n\tleaq 5(%0), %%r13\n\t"
                     "leaq 6(%0), %%r14\n\tleaq 7(%0), %%r15\n\tleaq 8(%0), %%rax\n\tmovq %%rax, %%xmm6\n\t"
                     "leaq 9(%0), %%rax\n\tmovq %%rax, %%xmm10\n\tleaq 10(%0), %%rax\n\tmovq %%rax, %%xmm15"
                     : : "d"(base) : "rax", "rbx", "rsi", "rdi", "r12", "r13", "r14", "r15", "xmm6", "xmm10", "xmm15");
}

/* Uma volta: o switch e a forma que o C aceita para o valor do setjmp; o ramo 0 nao volta. */
static __attribute__((noinline)) void UmaVolta(void)
{
    uint64_t lidos[REGISTRADORES];
    int k;

    ArmarRegistradores(BaseDaVolta);
    RspAntes = LerRsp();
    switch (SALVAR(Salto)) {
    case 0:
        Destruir();
        break;
    case 1:
        Soma += 1;
        break;
    case 5:
        Soma += 5;
        break;
    default:
        Erros++;
    }
    __asm__ volatile("movq %%rbx, 0(%0)\n\tmovq %%rsi, 8(%0)\n\tmovq %%rdi, 16(%0)\n\tmovq %%r12, 24(%0)\n\tmovq %%r13, 32(%0)\n\t"
                     "movq %%r14, 40(%0)\n\tmovq %%r15, 48(%0)\n\tmovq %%xmm6, 56(%0)\n\tmovq %%xmm10, 64(%0)\n\tmovq %%xmm15, 72(%0)"
                     : : "d"(lidos) : "memory");
    for (k = 0; k < REGISTRADORES; k++) {
        if (lidos[k] != BaseDaVolta + 1 + (uint64_t)k)
            Erros++;
    }
    if (LerRsp() != RspAntes)
        Erros++;
}

int forja_programa(void)
{
    uint64_t volta;

    for (volta = 0; volta < VOLTAS; volta++) {
        BaseDaVolta = volta * 16;
        ValorDoSalto = (volta & 1) != 0 ? 0 : 5;
        UmaVolta();
    }
    forja_print_u64(Erros);
    forja_print_u64(Soma);
    forja_print_texto("salto ok");
    return Erros == 0 ? 0 : 1;
}
#elif CASO == 14
/* str (release): forja_str_igual por conteudo (iguais em ponteiros diferentes, prefixo, tamanhos diferentes, vazios com e sem ponteiro, byte alto) e
 * forja_str_de_cstr (strlen, cstr vazio, NULL vira str vazia em release). */
static void ImprimirIgual(forja_str a, forja_str b)
{
    forja_print_bool(forja_str_igual(a, b));
}

int forja_programa(void)
{
    char copia[8] = {'f', 'o', 'r', 'j', 'a', 0, 0, 0};
    const char alto1[3] = {'a', (char)0xC3, (char)0xA7};
    const char alto2[3] = {'a', (char)0xC3, (char)0xA9};
    forja_str s;

    ImprimirIgual(FORJA_STR("forja", 5), FORJA_STR(copia, 5));
    ImprimirIgual(FORJA_STR("forja", 5), FORJA_STR("forjado", 7));
    ImprimirIgual(FORJA_STR("forja", 4), FORJA_STR("forjx", 4));
    ImprimirIgual(FORJA_STR("", 0), FORJA_STR(copia, 0));
    ImprimirIgual(FORJA_STR(NULL, 0), FORJA_STR("x", 0));
    ImprimirIgual(FORJA_STR(alto1, 3), FORJA_STR(alto2, 3));
    ImprimirIgual(FORJA_STR(alto1, 3), FORJA_STR(alto1, 3));
    s = forja_str_de_cstr(copia, ARQUIVO_TESTE, 1);
    forja_print_str(s);
    forja_print_i64(s.len);
    s = forja_str_de_cstr(copia + 5, ARQUIVO_TESTE, 2);
    forja_print_i64(s.len);
    s = forja_str_de_cstr(NULL, ARQUIVO_TESTE, 3);
    forja_print_i64(s.len);
    forja_print_str(s);
    return 0;
}
#elif CASO == 27 || CASO == 28
/* L16a: o executor de testes com FORJA_TESTE_FILTRO (caso 27, o verificador poe "soma") ou FORJA_TESTE_JSON=1 (caso 28). Quatro testes: um passa, um falha em DOIS expects
 * (so o 1o vai para o JSON), um passa e tem a caixa trocada ("SOMA": o filtro e sensivel a caixa) e um estoura (R0002, titulo com aspas para o escape do JSON). Sai com 1. */
static void t_soma_certa(void)
{
    forja_expect(1 + 1 == 2, "1 + 1 == 2", ARQUIVO_TESTE, 10);
}

static void t_soma_errada(void)
{
    forja_expect(1 + 1 == 3, "1 + 1 == 3", ARQUIVO_TESTE, 14);
    forja_expect(2 + 2 == 5, "2 + 2 == 5", ARQUIVO_TESTE, 15);
}

static void t_soma_grande(void)
{
    forja_expect(40 + 2 == 42, "40 + 2 == 42", ARQUIVO_TESTE, 19);
}

static void t_estouro(void)
{
    forja_falha_estouro("soma i32", INT32_MAX, 1, ARQUIVO_TESTE, 23);
}

int forja_programa(void)
{
    forja_teste_registrar("soma certa", t_soma_certa);
    forja_teste_registrar("soma errada", t_soma_errada);
    forja_teste_registrar("SOMA grande", t_soma_grande);
    forja_teste_registrar("estouro \"i32\"", t_estouro);
    return forja_testes_rodar();
}
#elif CASO == 29
/* L16b (dev): vazamento por teste. "limpo" aloca e libera (passa); "vaza" deixa tres blocos vivos em dois pontos (64 bytes na linha 15, 8 na linha 16: o teste falha com duas linhas R0009);
 * "depois" aloca e libera de novo (a lista esqueceu os blocos que vazaram: passa e o fim do programa nao repete o R0009). Sai com 1. */
static void t_limpo(void)
{
    void *p = forja_heap_alocar(16, ARQUIVO_TESTE, 10);

    forja_expect(p != NULL, "p != NULL", ARQUIVO_TESTE, 11);
    forja_mem_liberar(p, ARQUIVO_TESTE, 12);
}

static void t_vaza(void)
{
    forja_heap_alocar(24, ARQUIVO_TESTE, 15);
    forja_heap_alocar(40, ARQUIVO_TESTE, 15);
    forja_heap_alocar(8, ARQUIVO_TESTE, 16);
}

static void t_depois(void)
{
    forja_mem_liberar(forja_heap_alocar(32, ARQUIVO_TESTE, 20), ARQUIVO_TESTE, 21);
}

int forja_programa(void)
{
    forja_teste_registrar("limpo", t_limpo);
    forja_teste_registrar("vaza", t_vaza);
    forja_teste_registrar("depois", t_depois);
    return forja_testes_rodar();
}
#elif CASO == 30
/* L16b (dev): queda por teste. "ponteiro nulo" grava no endereco 0 (acesso a memoria invalida) e "instrucao ilegal" executa ud2: os dois viram R0011 com o titulo, os testes seguintes rodam. Sai com 1. */
static void t_antes(void)
{
    forja_expect(1 + 1 == 2, "1 + 1 == 2", ARQUIVO_TESTE, 10);
}

static void t_ponteiro_nulo(void)
{
    volatile int *p = (volatile int *)0;

    *p = 1;
}

static void t_instrucao_ilegal(void)
{
    __builtin_trap();
}

static void t_depois(void)
{
    forja_expect(2 + 2 == 4, "2 + 2 == 4", ARQUIVO_TESTE, 24);
}

int forja_programa(void)
{
    forja_teste_registrar("antes", t_antes);
    forja_teste_registrar("ponteiro nulo", t_ponteiro_nulo);
    forja_teste_registrar("instrucao ilegal", t_instrucao_ilegal);
    forja_teste_registrar("depois", t_depois);
    return forja_testes_rodar();
}
#elif CASO == 26
/* bit_set: a conta de bits ligados (forja_rt_conta_bits no nativo, __builtin_popcountll no oraculo): bordas (0, tudo ligado, um bit de cada posicao, mascaras
 * alternadas, tudo menos um bit) e 200 mil valores de um xorshift64, com a soma das contas e a ultima conta impressas. */
#ifdef ORACULO
#define CONTA_BITS(v) ((int64_t)__builtin_popcountll(v))
#else
#define CONTA_BITS(v) forja_rt_conta_bits(v)
#endif

int forja_programa(void)
{
    uint64_t borda[] = {0, 1, 3, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x5555555555555555ull, 0xAAAAAAAAAAAAAAAAull,
                        0x0F0F0F0F0F0F0F0Full, 0x00000000FFFFFFFFull, 0xFFFFFFFF00000000ull, 0x8000000000000001ull};
    uint64_t x = 88172645463325252ull;
    int64_t soma = 0, ultima = 0;
    unsigned i;
    int b;

    for (i = 0; i < sizeof borda / sizeof borda[0]; i++)
        forja_print_i64(CONTA_BITS(borda[i]));
    for (b = 0; b < 64; b++) {
        forja_print_i64(CONTA_BITS(UINT64_C(1) << b));
        forja_print_i64(CONTA_BITS(~(UINT64_C(1) << b)));
    }
    for (i = 0; i < 200000; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        ultima = CONTA_BITS(x);
        soma += ultima;
    }
    forja_print_i64(soma);
    forja_print_i64(ultima);
    return 0;
}
#endif

#ifdef ORACULO
int main(void)
{
    return forja_programa();
}
#endif
