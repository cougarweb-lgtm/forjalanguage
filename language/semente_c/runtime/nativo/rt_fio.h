/*
 * rt_fio.h - o RtFio: o estado do runtime que e de UMA thread (M5 P43, T0-1 do plano de concorrencia).
 *
 * Versao: 0.5.0
 * Changelog:
 *   0.5.0 - M6d P92d: rt_fio_ou_nulo() (le o fio da thread sem criar e sem morrer em rt_fio_sem_slot; usado pelos involucros de memoria por lib, que a thread da lib C chama).
 *   0.4.0 - M5c-b P57: Linux com thread (variante libc): ganchos rt_fio_obter/rt_fio_gravar (pthread_key do musl, ligados pelo rt_linux_entrada_libc.c); sem eles (variante crua) o fio segue o principal global. forja_fio_da_ponte, forja_fio_principal e forja_fio_liberar passam para rt_fio_linux.c. O ramo Windows nao muda.
 *   0.3.0 - M5 P46c: rt_fio() nao aloca mais (rt_fio_novo saiu do caminho comum: puxava HeapAlloc para todo exe); forja_fio_da_ponte, chamada no prologo da ponte C -> Forja, cria o fio da thread de fora.
 *   0.2.0 - M5 P45: rt_fio_liberar (DLL_THREAD_DETACH).
 *   0.1.0 - M5 P43: primeira versao. RtFio guarda o que antes era global do runtime e nao pode ser dividido entre threads: o frame (rt_frame), o buffer do f"..." (rt_formata), o
 *           estado do executor de testes e seu salto (em_teste, teste_falhou, salto) e, so na variante --rastreio, a pilha .fj (rt_pilha_*). No Windows o fio da thread vive num slot
 *           TlsAlloc lido direto do TEB (gs:[0x1480 + 8 * indice], so com indice < 64: TlsGetValue zera o GetLastError, que o std:file e o std:os leem); thread que o runtime nao
 *           conhece (C chamando um fjt_) ganha um fio novo na primeira chamada. No Linux (ainda sem thread, M5c-b) rt_fio() devolve o fio unico.
 *           O estado compartilhado entre threads (rastreio, alocadores, saida, fs, stdin) fica nos seus arquivos, com a marca "compartilhado:" na declaracao (conferir_regras).
 *
 * Incluido por rt_win.c e rt_linux.c DEPOIS de rt_salto.c (ForjaSalto) e ANTES de rt_nativo.c; nao compila sozinho. Os campos sao acessados por macros nos .c de origem
 * (#define rt_frame (rt_fio()->frame) etc.), para o resto do codigo seguir igual.
 * Verificacao: construir.bat sem saida e python -I ..\tests\rodar_bateria.py --modo rapida --mudou nativo runtime
 */
#ifndef RT_FIO_H
#define RT_FIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *base;
    int64_t topo;
    uint32_t geracao; /* epoca - 1: o C comeca em 1; aqui zero-init basta */
} RtFrame;

typedef struct { char *dados; int64_t len, cap; } RtFormata;

#ifdef FORJA_RASTREIO
#define RT_PILHA_MAX 8 /* quadros no JSON; o resto vira "quadros": total */

typedef struct { const char *proc, *arquivo; uint32_t linha, coluna; } RtQuadro;

typedef struct {
    uintptr_t *topo; /* um endereco no quadro de forja_entrada: nada acima dele e lido; NULL fora da thread principal */
    RtQuadro quadros[RT_PILHA_MAX];
    uint32_t quadros_n;
    uint32_t quadros_total;
    uint8_t valor_tipo;
    int64_t valor_a, valor_b, valor_c;
} RtPilha;
#endif

typedef struct {
    ForjaSalto salto; /* alinhado a 16 (o asm de rt_salto.c guarda xmm); o primeiro campo mantem o RtFio alinhado */
    RtFrame frame;
    RtFormata formata;
    bool em_teste;
    bool teste_falhou;
#ifdef FORJA_RASTREIO
    RtPilha pilha;
#endif
} RtFio;

/* O fio da thread principal (a que roda forja_entrada ou o DllMain ATTACH). Em .bss: nao depende de alocacao. */
static RtFio rt_fio_principal; /* compartilhado: so o ponteiro e compartilhado; o conteudo e da thread principal */

#ifdef RT_ALVO_WIN
#define RT_FIO_SLOTS_TEB 64u /* TEB.TlsSlots tem 64 entradas, a partir do deslocamento 0x1480; indice maior fica em TlsExpansionSlots (TlsGetValue) */
#define RT_FIO_TEB_SLOTS_DESLOCAMENTO 0x1480u

/* Indice do slot mais 1 (0 = ainda nao alocado, para o indice 0 do TlsAlloc nao ser confundido com "sem slot"). */
static uint32_t rt_fio_indice; /* compartilhado: escrito 1x em rt_fio_iniciar, antes de qualquer outra thread do runtime */

static RtFio *rt_fio_sem_slot(void);

/* O valor do slot da thread atual: direto do TEB quando o indice cabe nos 64 slots, senao pelo TlsGetValue. */
static inline void *rt_fio_slot(uint32_t indice)
{
    uint64_t valor;

    if (indice >= RT_FIO_SLOTS_TEB)
        return sys_tls_ler(indice);
    __asm__("movq %%gs:(%1), %0" : "=r"(valor) : "r"((uint64_t)RT_FIO_TEB_SLOTS_DESLOCAMENTO + 8u * indice));
    return (void *)(uintptr_t)valor;
}

/* O fio da thread atual (nunca NULL). */
static inline RtFio *rt_fio(void)
{
    RtFio *fio = rt_fio_indice == 0 ? NULL : (RtFio *)rt_fio_slot(rt_fio_indice - 1u);

    return fio != NULL ? fio : rt_fio_sem_slot();
}

/* O fio da thread atual SEM criar e SEM morrer: NULL se a thread nao tem fio (thread da lib C, ou antes da forja_entrada). Para codigo chamado de qualquer thread (rt_libmem.c). */
static inline RtFio *rt_fio_ou_nulo(void)
{
    return rt_fio_indice == 0 ? NULL : (RtFio *)rt_fio_slot(rt_fio_indice - 1u);
}

/* Prologo da ponte C -> Forja (o fjt_ e o involucro de 'pub export proc'): thread de fora sem fio ganha um no heap e liga o flag rt_multithread. Com o slot ja
 * preenchido (thread principal ou thread que ja passou pela ponte) so le o slot e volta. E a UNICA entrada que aloca fio: o 01_ola nao tem ponte e nao puxa heap. */
void forja_fio_da_ponte(void);

/* Aloca o slot do processo e liga o fio principal a thread atual. Idempotente. Chamada por forja_entrada e pelo DllMain ATTACH. */
void rt_fio_iniciar(void);

/* Libera o fio da thread atual (frame, buffer do f"..." e o proprio fio) e zera o slot. Chamada pelo DllMain em DLL_THREAD_DETACH; o fio principal nao e liberado. */
void rt_fio_liberar(void);
#else
/* Linux (M5c-b P57, D-R3/D-R7). Variante crua (_start, sem libc, sem threads): os ganchos abaixo ficam NULL e o fio e o principal, global. Variante libc: o objeto de entrada
 * (rt_linux_entrada_libc.c) liga os ganchos ao pthread_getspecific/setspecific do musl (uma pthread_key) e grava o fio principal na chave no main; a thread de fora (pthread_create
 * de uma lib C) ganha o seu fio por forja_fio_da_ponte e o musl o devolve no destrutor da chave (forja_fio_liberar) quando ela termina. */
typedef RtFio *(*RtFioObterFn)(void);
typedef void (*RtFioGravarFn)(RtFio *fio);

RtFioObterFn rt_fio_obter = NULL; /* compartilhado: gravado 1x por forja_main_libc, antes de qualquer outra thread */
RtFioGravarFn rt_fio_gravar = NULL; /* compartilhado: idem */

static RtFio *rt_fio_sem_slot(void);

static inline RtFio *rt_fio(void)
{
    RtFio *fio;

    if (rt_fio_obter == NULL)
        return &rt_fio_principal;
    fio = rt_fio_obter();
    return fio != NULL ? fio : rt_fio_sem_slot();
}

/* O fio da thread atual SEM criar e SEM morrer: NULL se a thread nao tem fio (thread criada pela lib C). Variante crua: o fio principal. */
static inline RtFio *rt_fio_ou_nulo(void)
{
    return rt_fio_obter == NULL ? &rt_fio_principal : rt_fio_obter();
}

static inline void rt_fio_iniciar(void)
{
}

/* Prologo da ponte C -> Forja (fjt_): na variante libc, a thread de fora sem fio ganha um no heap e liga o flag rt_multithread. Cru: nada a criar. */
void forja_fio_da_ponte(void);

/* O fio da thread principal (variante libc grava na chave) e a liberacao do fio de uma thread que morre (destrutor da chave). Win64; o objeto de entrada os chama. */
RtFio *forja_fio_principal(void);
void forja_fio_liberar(RtFio *fio);
#endif

#endif
