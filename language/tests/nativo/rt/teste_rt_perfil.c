/*
 * teste_rt_perfil.c - programas do harness do perfil (M8 P1): cada -DCASO=N chama forja_perfil_entrar/forja_perfil_sair como o codigo gerado do P2 chamara, com uma tabela
 * FPF1 sintetica de 5 nomes (t.a, t.b, t.c, t.r, t.x). Liga com rt_win.o + rt_perfil.o (+ rt_thread_win.o no caso 4); quem confere os arquivos e o verificar_rt_perfil.py.
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M8 P1: primeira versao, 10 casos (ver verificar_rt_perfil.py).
 * Uso: gcc ... -DCASO=1 -c teste_rt_perfil.c
 */
#include <stdint.h>

#ifndef CASO
#error "defina -DCASO=<n>"
#endif

enum { A = 0, B = 1, C = 2, R = 3, X = 4 };

void forja_perfil_entrar(uint32_t id);
void forja_perfil_sair(uint32_t id);
void forja_perfil_finalizar(void);
_Noreturn void forja_sair(int codigo);
int64_t sys_tempo_ns(void);
uint64_t forja_thread_criar(uint32_t (*rotina)(void *), void *arg);
int32_t forja_thread_esperar(uint64_t h);
void forja_thread_fechar(uint64_t h);

/* FPF1: magic, contagem, offsets (a partir do inicio da tabela), textos. 8 + 5 * 4 = 28 = o offset do 1o texto. */
const struct {
    uint32_t magic;
    uint32_t contagem;
    uint32_t offsets[5];
    char textos[20];
} forja_perfil_tabela = {0x31465046u, 5u, {28u, 32u, 36u, 40u, 44u}, "t.a\0t.b\0t.c\0t.r\0t.x"};

#define UM_MS 1000000LL

#if CASO == 2 || CASO == 3 || CASO == 4 || CASO == 6 || CASO == 7 || CASO == 10
/* Espera ativa de `ns` nanossegundos (o tempo de uma proc do teste). */
static void ocupar(int64_t ns)
{
    int64_t fim = sys_tempo_ns() + ns;

    while (sys_tempo_ns() < fim) {
    }
}
#endif

#if CASO == 1
/* 3 nomes: a x3, b x2, c x1, em sequencia. */
int forja_programa(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        forja_perfil_entrar(A);
        forja_perfil_sair(A);
    }
    for (i = 0; i < 2; i++) {
        forja_perfil_entrar(B);
        forja_perfil_sair(B);
    }
    forja_perfil_entrar(C);
    forja_perfil_sair(C);
    return 0;
}

#elif CASO == 2
/* aninhamento a > b > c, 2 ms de tempo proprio cada. */
int forja_programa(void)
{
    forja_perfil_entrar(A);
    ocupar(2 * UM_MS);
    forja_perfil_entrar(B);
    ocupar(2 * UM_MS);
    forja_perfil_entrar(C);
    ocupar(2 * UM_MS);
    forja_perfil_sair(C);
    forja_perfil_sair(B);
    forja_perfil_sair(A);
    return 0;
}

#elif CASO == 3 || CASO == 10
/* recursao da proc r: 5 niveis de 1 ms (caso 3) ou 5000 niveis (caso 10: a pilha passa da capacidade inicial de 1024 quadros). */
#if CASO == 3
#define NIVEIS 5
#else
#define NIVEIS 5000
#endif
static void recursiva(int n)
{
    forja_perfil_entrar(R);
    ocupar(CASO == 3 ? UM_MS : 1000);
    if (n > 1)
        recursiva(n - 1);
    forja_perfil_sair(R);
}

int forja_programa(void)
{
    recursiva(NIVEIS);
    return 0;
}

#elif CASO == 4
/* 2 fios: a thread da Forja liga o rt_multithread com a proc `a` aberta no principal (o fio principal tem de continuar o mesmo); b roda na filha, c no principal. */
static uint32_t filha(void *arg)
{
    (void)arg;
    forja_perfil_entrar(B);
    ocupar(UM_MS);
    forja_perfil_sair(B);
    return 0;
}

int forja_programa(void)
{
    uint64_t h;

    forja_perfil_entrar(A);
    h = forja_thread_criar(filha, 0);
    forja_thread_esperar(h);
    forja_thread_fechar(h);
    forja_perfil_entrar(C);
    forja_perfil_sair(C);
    forja_perfil_sair(A);
    return 0;
}

#elif CASO == 5
/* teto estourado: 1.100.000 chamadas de a (o teto e 1.048.576 eventos). */
int forja_programa(void)
{
    int i;

    for (i = 0; i < 1100000; i++) {
        forja_perfil_entrar(A);
        forja_perfil_sair(A);
    }
    return 0;
}

#elif CASO == 6
/* saida no meio de a > b (R-code ou panic chegam aqui por forja_sair): os 2 quadros abertos fecham no instante da saida. */
int forja_programa(void)
{
    forja_perfil_entrar(A);
    ocupar(UM_MS);
    forja_perfil_entrar(B);
    ocupar(UM_MS);
    forja_sair(7);
}

#elif CASO == 7
/* o caminho do os.exit: finalizar antes de sair; o handler do forja_sair nao escreve de novo. */
int forja_programa(void)
{
    forja_perfil_entrar(X);
    forja_perfil_entrar(A);
    ocupar(UM_MS);
    forja_perfil_finalizar();
    forja_sair(0);
}

#elif CASO == 8
/* nenhuma proc medida: nenhum arquivo. */
int forja_programa(void)
{
    return 0;
}

#elif CASO == 9
/* id fora da tabela e saida sem entrada: ignorados; so a chamada valida conta. */
int forja_programa(void)
{
    forja_perfil_entrar(99);
    forja_perfil_sair(A);
    forja_perfil_sair(99);
    forja_perfil_entrar(A);
    forja_perfil_sair(A);
    return 0;
}
#endif
