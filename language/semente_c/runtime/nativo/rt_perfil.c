/*
 * rt_perfil.c - runtime do perfil por instrumentacao (--perfil, M8 P1; D-M8-5 e D-M8-6 do contrato_m8). Comum ao Windows e ao Linux: a parte do sistema operacional vive em
 * rt_perfil_win.c / rt_perfil_linux.c e a escrita dos 2 arquivos em rt_perfil_saida.c (os 3 sao incluidos aqui: um objeto so).
 *
 * Versao: 0.1.1
 * Changelog:
 *   0.1.1 - portao LG do G3: PERFIL_VERSAO_FORJA 0.23.0 (alinha ao FORJA_VERSAO do bump).
 *   0.1.0 - M8 P1: primeira versao. forja_perfil_entrar/forja_perfil_sair(u32 id) (o compilador as chama no inicio e antes de cada retorno de proc, P2), pilha por fio com o instante
 *           de entrada e o tempo dos filhos, evento X de 16 bytes num buffer global de 1.048.576 eventos (indice atomico; estourou = so o agregado continua e truncado = true), agregado
 *           por (fio, id) fundido no fim (total so conta a chamada mais externa de uma proc recursiva), relogio rdtsc calibrado no inicio e no fim contra sys_tempo_ns (QueryPerformanceCounter
 *           no Windows, clock_gettime no Linux), escrita no fim normal e no forja_sair (handler registrado na 1a chamada) e em forja_perfil_finalizar (o os.exit chama, P2), fechando os
 *           quadros abertos com o instante da saida. O fio e achado por id da thread numa tabela de 256 casas (sem RtFio: ele e static do rt_win.c/rt_linux.c e mexer nele mudaria o runtime
 *           de todo exe); sem rt_multithread o fio e o principal, sem custo de consulta.
 *
 * Objeto proprio (rt_win_perfil.o / rt_linux_perfil.o): so entra na ligacao quando o programa pede --perfil (P2); os outros exes saem byte a byte iguais. Sem libc; nenhum import alem
 * dos de kernel32 que ja estao na tabela fixa do ligador.
 * Tabela de nomes (FPF1, gerada pelo compilador em .rdata): u32 magic 0x31465046 ("FPF1" em little-endian), u32 contagem, u32 offsets[contagem] (bytes desde o inicio da tabela ate
 * o "modulo.proc" terminado em NUL), depois os textos. O id de uma proc e o seu indice.
 * Limites assumidos: TSC invariante e sincronizado entre nucleos (todo x86-64 recente); dur de um evento satura em 2^44 ticks (~1 h 20 a 3.5 GHz; o agregado nao satura); 255 fios; 1.048.575 procs.
 * Verificacao: python -I tests\nativo\rt\verificar_rt_perfil.py
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rt_sys.h"

#define PERFIL_VERSAO_FORJA "0.23.0"
#define PERFIL_TABELA_MAGIC 0x31465046u
#define PERFIL_FIOS_MAX 255u
#define PERFIL_CASAS 256u
#define PERFIL_EVENTOS_MAX 1048576u
#define PERFIL_ID_MAX 0xFFFFFu
#define PERFIL_DUR_MAX 0xFFFFFFFFFFFull /* 44 bits */
#define PERFIL_PILHA_INICIAL 1024u
#define PERFIL_PILHA_LIMITE 0x40000000u
#define PERFIL_ESTADO_NOVO 0u
#define PERFIL_ESTADO_INICIANDO 1u
#define PERFIL_ESTADO_ATIVO 2u
#define PERFIL_ESTADO_ENCERRADO 3u /* depois do fim, ou desligado por erro: entrar e sair viram no-op */

typedef struct {
    uint64_t chamadas;
    uint64_t total;   /* ticks, so a chamada mais externa de uma proc recursiva */
    uint64_t proprio; /* ticks, sem os filhos */
    uint32_t ativo;   /* chamadas abertas desta proc na pilha do fio */
    uint32_t reservado;
} PerfilAgg;

typedef struct {
    uint32_t id;
    uint32_t reservado;
    uint64_t entrada; /* ticks */
    uint64_t filhos;  /* ticks gastos pelos filhos ja terminados */
} PerfilQuadro;

typedef struct {
    uint32_t indice; /* 1..255: o tid do JSON */
    uint32_t profundidade;
    uint32_t capacidade;
    uint32_t pulados; /* entradas sem espaco na pilha: as saidas correspondentes sao ignoradas */
    PerfilQuadro *pilha;
    PerfilAgg *agg; /* perfil_tabela_n entradas */
} PerfilFio;

/* Fornecido pelo codigo gerado (P2): a tabela FPF1. */
extern const uint32_t forja_perfil_tabela[];

void forja_rt_ao_sair(void (*fn)(void));
void forja_perfil_entrar(uint32_t id);
void forja_perfil_sair(uint32_t id);
void forja_perfil_finalizar(void);

static volatile uint32_t perfil_estado; /* compartilhado: PERFIL_ESTADO_*, so 0 -> 1 -> 2 -> 3 (ou 1 -> 3) */
static uint32_t perfil_tabela_n; /* compartilhado: escrito 1x na partida, antes do estado 2 */
static const uint32_t *perfil_tabela; /* compartilhado: idem */
static uint64_t *perfil_eventos; /* compartilhado: 2 palavras por evento; NULL se faltou memoria */
static uint32_t perfil_eventos_n; /* compartilhado: indice atomico do proximo evento */
static volatile uint8_t perfil_truncado; /* compartilhado: 1 quando um evento nao coube (ou nao ha buffer) */
static uint64_t perfil_tsc0; /* compartilhado: escrito 1x na partida */
static int64_t perfil_ns0; /* compartilhado: idem */
static uint64_t perfil_escala; /* compartilhado: escrito 1x no fim, antes de qualquer leitura da conversao */
static uint32_t perfil_escala_bits; /* compartilhado: idem */
static PerfilFio *perfil_principal; /* compartilhado: escrito 1x na partida */
static uint32_t perfil_fios_n; /* compartilhado: fios criados (atomico) */
static PerfilFio *perfil_lista[PERFIL_FIOS_MAX]; /* compartilhado: fios na ordem de criacao; cada casa e publicada 1x (release) */
static uint32_t perfil_tids[PERFIL_CASAS]; /* compartilhado: tid dono de cada casa (0 = livre), reivindicada por CAS */
static PerfilFio *perfil_casas[PERFIL_CASAS]; /* compartilhado: o fio de cada casa, publicado depois do tid (release) */

#ifdef RT_ALVO_WIN
#include "rt_perfil_win.c"
#else
#include "rt_perfil_linux.c"
#endif

/* ---- relogio ---- */

static uint64_t perfil_tsc(void)
{
    return __builtin_ia32_rdtsc();
}

/* ticks -> ns pela escala fixada no fim (perfil_calibrar): (ticks * escala) >> bits, em 128 bits (sem divisao de 128 bits, que pediria libgcc). */
static uint64_t perfil_ticks_para_ns(uint64_t ticks)
{
    return (uint64_t)(((unsigned __int128)ticks * perfil_escala) >> perfil_escala_bits);
}

/* Calibra os ticks contra o relogio do sistema entre a partida e agora: escala = (dns << k) / dtsc, com k o maior que mantem dns << k abaixo de 2^63. */
static void perfil_calibrar(uint64_t tsc_fim, int64_t ns_fim)
{
    uint64_t dtsc = tsc_fim - perfil_tsc0;
    uint64_t dns = ns_fim > perfil_ns0 ? (uint64_t)(ns_fim - perfil_ns0) : 0;
    uint32_t k;

    if (dtsc == 0 || dns == 0) {
        perfil_escala = 1;
        perfil_escala_bits = 0;
        return;
    }
    k = (uint32_t)__builtin_clzll(dns) - 1u; /* dns < 2^63: ao menos 1 zero a esquerda */
    perfil_escala = (dns << k) / dtsc;
    perfil_escala_bits = k;
}

/* ---- tabela de nomes ---- */

static const char *perfil_nome(uint32_t id)
{
    return (const char *)perfil_tabela + perfil_tabela[2u + id];
}

/* Quantos eventos o buffer guarda de fato (o indice atomico pode ter passado do teto). */
static uint32_t perfil_eventos_gravados(void)
{
    return perfil_eventos_n < PERFIL_EVENTOS_MAX ? perfil_eventos_n : PERFIL_EVENTOS_MAX;
}

#include "rt_perfil_saida.c"

/* ---- fios ---- */

static void perfil_avisar(const char *texto)
{
    int64_t n = 0;

    while (texto[n] != '\0')
        n++;
    sys_escrever(2, texto, n);
}

/* Cria o fio (estado + pilha inicial + agregado) e o publica na lista; NULL se faltou memoria ou passou de 255 fios. */
static PerfilFio *perfil_fio_criar(void)
{
    PerfilFio *fio;
    uint32_t indice;

    if (__atomic_load_n(&perfil_fios_n, __ATOMIC_RELAXED) >= PERFIL_FIOS_MAX)
        return NULL;
    indice = __atomic_add_fetch(&perfil_fios_n, 1u, __ATOMIC_RELAXED);
    if (indice > PERFIL_FIOS_MAX)
        return NULL;
    fio = (PerfilFio *)sys_paginas_reservar((int64_t)sizeof(PerfilFio));
    if (fio == NULL)
        return NULL;
    fio->pilha = (PerfilQuadro *)sys_paginas_reservar((int64_t)(PERFIL_PILHA_INICIAL * sizeof(PerfilQuadro)));
    fio->agg = (PerfilAgg *)sys_paginas_reservar((int64_t)perfil_tabela_n * (int64_t)sizeof(PerfilAgg));
    if (fio->pilha == NULL || fio->agg == NULL)
        return NULL;
    fio->indice = indice;
    fio->capacidade = PERFIL_PILHA_INICIAL;
    __atomic_store_n(&perfil_lista[indice - 1u], fio, __ATOMIC_RELEASE);
    return fio;
}

/* O fio da thread `tid`: acha a casa pelo hash do tid (sondagem linear), ou reivindica uma casa livre por CAS e cria o fio. NULL = sem fio para esta thread. */
static PerfilFio *perfil_fio_da_thread(uint32_t tid)
{
    uint32_t casa = (tid * 2654435761u) >> 24;
    uint32_t i;

    for (i = 0; i < PERFIL_CASAS; i++, casa++) {
        uint32_t *dono = &perfil_tids[casa & (PERFIL_CASAS - 1u)];
        uint32_t visto = __atomic_load_n(dono, __ATOMIC_ACQUIRE);

        if (visto == 0 && __atomic_compare_exchange_n(dono, &visto, tid, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            PerfilFio *novo = perfil_fio_criar();

            __atomic_store_n(&perfil_casas[casa & (PERFIL_CASAS - 1u)], novo, __ATOMIC_RELEASE);
            return novo;
        }
        if (visto == tid)
            return __atomic_load_n(&perfil_casas[casa & (PERFIL_CASAS - 1u)], __ATOMIC_ACQUIRE);
    }
    return NULL;
}

/* O fio da thread atual: sem outra thread (rt_multithread desligado) e o principal. */
static PerfilFio *perfil_fio_atual(void)
{
    if (!rt_multithread)
        return perfil_principal;
    return perfil_fio_da_thread(perfil_so_tid());
}

/* Dobra a pilha do fio (so chamada com a pilha cheia). false = sem memoria ou limite. */
static bool perfil_pilha_crescer(PerfilFio *fio)
{
    uint32_t nova_capacidade = fio->capacidade * 2u;
    PerfilQuadro *nova;
    uint32_t i;

    if (fio->capacidade >= PERFIL_PILHA_LIMITE)
        return false;
    nova = (PerfilQuadro *)sys_paginas_reservar((int64_t)nova_capacidade * (int64_t)sizeof(PerfilQuadro));
    if (nova == NULL)
        return false;
    for (i = 0; i < fio->profundidade; i++)
        nova[i] = fio->pilha[i];
    sys_paginas_liberar(fio->pilha, (int64_t)fio->capacidade * (int64_t)sizeof(PerfilQuadro));
    fio->pilha = nova;
    fio->capacidade = nova_capacidade;
    return true;
}

/* ---- eventos e quadros ---- */

/* Grava o evento X de 16 bytes: palavra 0 = ts (56 bits, ticks desde a partida) << 8 | fio; palavra 1 = dur (44 bits, ticks, satura) << 20 | id. */
static void perfil_evento_gravar(uint32_t fio, uint32_t id, uint64_t entrada, uint64_t dur)
{
    uint64_t ts = entrada > perfil_tsc0 ? entrada - perfil_tsc0 : 0;
    uint32_t indice;

    if (perfil_eventos == NULL || __atomic_load_n(&perfil_eventos_n, __ATOMIC_RELAXED) >= PERFIL_EVENTOS_MAX) {
        perfil_truncado = 1;
        return;
    }
    indice = __atomic_fetch_add(&perfil_eventos_n, 1u, __ATOMIC_RELAXED);
    if (indice >= PERFIL_EVENTOS_MAX) {
        perfil_truncado = 1;
        return;
    }
    perfil_eventos[2u * indice] = (ts << 8) | fio;
    perfil_eventos[2u * indice + 1u] = ((dur < PERFIL_DUR_MAX ? dur : PERFIL_DUR_MAX) << 20) | id;
}

/* Fecha o quadro do topo da pilha no instante `agora`: agrega, soma a duracao ao pai e grava o evento. */
static void perfil_fechar_quadro(PerfilFio *fio, uint64_t agora)
{
    const PerfilQuadro *q = &fio->pilha[--fio->profundidade];
    uint64_t dur = agora > q->entrada ? agora - q->entrada : 0;
    uint64_t proprio = dur > q->filhos ? dur - q->filhos : 0;
    PerfilAgg *agg = &fio->agg[q->id];

    if (fio->profundidade > 0)
        fio->pilha[fio->profundidade - 1u].filhos += dur;
    agg->chamadas++;
    agg->proprio += proprio;
    if (--agg->ativo == 0)
        agg->total += dur;
    perfil_evento_gravar(fio->indice, q->id, q->entrada, dur);
}

/* ---- partida ---- */

/* Confere a tabela FPF1, abre o buffer de eventos, marca o inicio do relogio, cria o fio principal e registra a escrita no forja_sair. false = perfil desligado (aviso no stderr). */
static bool perfil_preparar(void)
{
    uint32_t qtd;

    if (forja_perfil_tabela[0] != PERFIL_TABELA_MAGIC || forja_perfil_tabela[1] == 0 || forja_perfil_tabela[1] > PERFIL_ID_MAX) {
        perfil_avisar("perfil: tabela de nomes invalida; perfil desligado\n");
        return false;
    }
    perfil_tabela = forja_perfil_tabela;
    qtd = forja_perfil_tabela[1];
    perfil_tabela_n = qtd;
    perfil_eventos = (uint64_t *)sys_paginas_reservar((int64_t)PERFIL_EVENTOS_MAX * 16);
    perfil_tsc0 = perfil_tsc();
    perfil_ns0 = sys_tempo_ns();
    perfil_principal = perfil_fio_da_thread(perfil_so_tid());
    if (perfil_principal == NULL) {
        perfil_avisar("perfil: sem memoria; perfil desligado\n");
        return false;
    }
    forja_rt_ao_sair(forja_perfil_finalizar);
    return true;
}

/* Primeira chamada (ou chamada concorrente durante a partida): quem ganha o CAS prepara, os outros esperam. true se o perfil esta ativo. */
static bool perfil_iniciar(void)
{
    uint32_t esperado = PERFIL_ESTADO_NOVO;

    if (!__atomic_compare_exchange_n(&perfil_estado, &esperado, PERFIL_ESTADO_INICIANDO, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&perfil_estado, __ATOMIC_ACQUIRE) == PERFIL_ESTADO_INICIANDO)
            __builtin_ia32_pause();
        return __atomic_load_n(&perfil_estado, __ATOMIC_ACQUIRE) == PERFIL_ESTADO_ATIVO;
    }
    if (!perfil_preparar()) {
        __atomic_store_n(&perfil_estado, PERFIL_ESTADO_ENCERRADO, __ATOMIC_RELEASE);
        return false;
    }
    __atomic_store_n(&perfil_estado, PERFIL_ESTADO_ATIVO, __ATOMIC_RELEASE);
    return true;
}

/* ---- API chamada pelo codigo gerado ---- */

/* Inicio de uma proc: empilha (id, instante, filhos = 0). O instante e lido por ultimo, para a propria instrumentacao ficar fora da medida da proc. */
void forja_perfil_entrar(uint32_t id)
{
    PerfilFio *fio;
    PerfilQuadro *q;

    if (__atomic_load_n(&perfil_estado, __ATOMIC_ACQUIRE) != PERFIL_ESTADO_ATIVO && !perfil_iniciar())
        return;
    fio = perfil_fio_atual();
    if (fio == NULL || id >= perfil_tabela_n)
        return;
    if (fio->profundidade == fio->capacidade && !perfil_pilha_crescer(fio)) {
        fio->pulados++;
        return;
    }
    q = &fio->pilha[fio->profundidade++];
    q->id = id;
    q->filhos = 0;
    fio->agg[id].ativo++;
    q->entrada = perfil_tsc();
}

/* Fim de uma proc (antes de cada retorno): o instante e lido primeiro. Sem quadro correspondente (fio sem pilha, entrada pulada) nao faz nada. */
void forja_perfil_sair(uint32_t id)
{
    uint64_t agora = perfil_tsc();
    PerfilFio *fio;

    if (__atomic_load_n(&perfil_estado, __ATOMIC_ACQUIRE) != PERFIL_ESTADO_ATIVO)
        return;
    fio = perfil_fio_atual();
    if (fio == NULL || id >= perfil_tabela_n)
        return;
    if (fio->pulados > 0) {
        fio->pulados--;
        return;
    }
    if (fio->profundidade > 0)
        perfil_fechar_quadro(fio, agora);
}

/* Fecha os quadros que ficaram abertos (saida no meio de uma proc: R-code, panic, os.exit) no instante `agora`. */
static void perfil_fechar_abertos(uint64_t agora)
{
    uint32_t qtd = perfil_fios_n < PERFIL_FIOS_MAX ? perfil_fios_n : PERFIL_FIOS_MAX;
    uint32_t f;

    for (f = 0; f < qtd; f++) {
        PerfilFio *fio = __atomic_load_n(&perfil_lista[f], __ATOMIC_ACQUIRE);

        if (fio == NULL)
            continue;
        fio->pulados = 0;
        while (fio->profundidade > 0)
            perfil_fechar_quadro(fio, agora);
    }
}

/* Encerra o perfil e escreve os 2 arquivos; so a 1a chamada faz algo (handler do forja_sair, os.exit e o fim normal podem chamar). Sem nenhuma proc medida nao escreve nada. */
void forja_perfil_finalizar(void)
{
    uint32_t ativo = PERFIL_ESTADO_ATIVO;
    uint64_t tsc_fim;

    if (!__atomic_compare_exchange_n(&perfil_estado, &ativo, PERFIL_ESTADO_ENCERRADO, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return;
    tsc_fim = perfil_tsc();
    perfil_calibrar(tsc_fim, sys_tempo_ns());
    perfil_fechar_abertos(tsc_fim);
    if (!perfil_escrever_resumo(tsc_fim - perfil_tsc0))
        perfil_avisar("perfil: nao consegui gravar o .perfil.txt\n");
    if (!perfil_escrever_json())
        perfil_avisar("perfil: nao consegui gravar o .perfil.json\n");
}
