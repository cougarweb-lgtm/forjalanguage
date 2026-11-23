/*
 * rt_aloc.c - alocacao do runtime nativo da Forja: heap, frame, listas [dynamic]T, Arena e Pool (M2 P14a).
 *
 * Versao: 0.7.0
 * Changelog:
 *   0.7.0 - M6e P93k: forja_lista_crescer_alvo (so dev; o BIR a chama com --guarda-memoria, passando o bloco da lista): novo = max(len + 1, alvo), sem o laco sem fim do crescer exato (sai o len + 1 do forja_lista_crescer); o bloco novo do append herda arquivo/linha do velho (D-M6e-3).
 *   0.6.0 - M6e P93e: com rt_guarda (--guarda-memoria, so dev) o heap vem do alocador de guarda (rt_guarda.c), o crescimento da lista de heap e EXATO (len + 1) e os pedacos da Arena vem da guarda e vao ao cemiterio no destroy; release e dev sem a opcao intocados.
 *   0.5.0 - M6e P93c (D-M6e-5): em dev, forja_heap_realocar aloca bloco novo, copia min(bytes) e baixa o velho com causa append_moveu (sem sys_heap_realocar); release intocado.
 *   0.4.0 - M6 P70: em dev, o registro de Arena/Pool guarda a pilha da criacao (ForjaAlocador.ras) e rt_alocs_vivos_relatar lista os nao destruidos no fim; forja_mem_liberar deixa a devolucao
 *           ao sistema para a quarentena do rt_rastreio.c. Release inalterado (macros vazias).
 *   0.3.0 - M5 P44: registro de Arena e Pool (rt_alocs*) e seus blocos sob rt_aloc_trava (SRW, so com rt_multithread): as funcoes publicas de arena, pool, mem_alocar e rt_aloc_lista a pegam; em dev,
 *           forja_heap_realocar segura rt_rastreio_trava em volta do realocar + reencadear. Heap sem rastreio (release) nao trava: HeapAlloc ja e seguro entre threads; o frame e do RtFio.
 *   0.2.0 - M5 P43: rt_frame vira campo do RtFio (rt_fio.h): o frame e por thread; o RtFrame mudou de lugar.
 *   0.1.0 - primeira versao: portado de forja_rt.h:930-1592 (sem soa e map, que sao do P17a) com os mesmos textos e codigos de saida.
 *           R0007 (sem memoria, cap negativo, Pool/lista), R0008 (liberar duplo, handle velho, Pool de outro dono, lista do frame resetado).
 *           Heap por sys_heap_*; frame de 16 MiB por sys_paginas_reservar (reserva + commit); blocos de Arena e Pool e o registro de
 *           alocadores tambem por sys_heap_* (sem cabecalho, fora do rastreio). Dev: cabecalho (80 B desde o P70) e rastreio em rt_rastreio.c
 *           (incluido ANTES deste arquivo); release: sem cabecalho, sem preenchimento 0xDD, sem R0009.
 *
 * Handle de alocador = (geracao << 12) | idx; idx 0 (Arena{} zerada), 1 (heap) e 2 (frame) sao reservados.
 * Estado so em zero-init (.bss): a epoca do frame e guardada menos 1 (o C comeca em 1).
 * Incluido por rt_win.c (P14b) depois de rt_nativo.c e, em dev, de rt_rastreio.c; nao compila sozinho.
 */
#include "forja_rt_nativo.h"

#if defined(FORJA_DEV) && !defined(RT_RASTREIO_C)
#error "rt_aloc.c com FORJA_DEV precisa de rt_rastreio.c incluido antes"
#endif

#ifndef FORJA_FRAME_BYTES
#define FORJA_FRAME_BYTES ((int64_t)16 * 1024 * 1024)
#endif
#define FORJA_ALOCS_MAX 4096u
#define FORJA_ALOC_RESERVADOS 3u
#define FORJA_ALOC_GERACAO_MAX 0xFFFFFu
#define FORJA_TIPO_ARENA 1
#define FORJA_TIPO_POOL 2
#define FORJA_BLOCO_MIN 64
#define FORJA_ARQ_STD "std:mem"
#define RT_ALINHA(x) (((x) + 15) & ~(int64_t)15)

#ifndef FORJA_DEV
/* Release: sem cabecalho. As marcas existem so para as chamadas de rt_aloc_cabecalho (que nao fazem nada). */
#define FORJA_MARCA_ARENA 0x4A524F4641524E41ull
#define FORJA_MARCA_POOL 0x4A524F46504F4F4Cull
#define FORJA_MARCA_MORTA 0x4A524F464D4F5254ull
#define RT_ALOC_EXTRA ((int64_t)0)
#define rt_aloc_cabecalho(p, marca, h) ((void)(p), (void)(marca), (void)(h))
#define rt_aloc_origem(p, bytes, arquivo, linha) ((void)(p), (void)(bytes), (void)(arquivo), (void)(linha))
#define rt_pool_conferir(h, p, arquivo, linha) ((void)0)
#define RT_RAS_LOCAL(ras)
#define rt_aloc_guardar_ras(h, ras) ((void)0)
#define rt_aloc_saida_registrar() ((void)0)
#endif

/* O frame e do fio da thread (RtFrame em rt_fio.h): cada thread tem o seu e o reset de uma nao toca o da outra. */
#define rt_frame (rt_fio()->frame)

#ifndef FORJA_DEV
int64_t forja_rt_vivos_qtd(void)
{
    return 0;
}
#endif

_Noreturn void forja_falha_alocacao(const char *oque, int64_t valor, const char *arquivo, int linha)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, oque);
    rt_causa_texto(&causa, " (");
    rt_causa_i64(&causa, valor);
    rt_causa_texto(&causa, ")");
    forja_falha(FORJA_R0007, rt_causa_fim(&causa), arquivo, linha);
}

/* ---- frame ---- */

static uint32_t rt_frame_epoca(void)
{
    return rt_frame.geracao + 1u;
}

bool forja_no_frame(const void *p)
{
    uintptr_t u = (uintptr_t)p;
    uintptr_t b = (uintptr_t)rt_frame.base;

    return rt_frame.base != NULL && u >= b && u < b + (uintptr_t)FORJA_FRAME_BYTES;
}

static void rt_frame_reservar(const char *arquivo, int linha)
{
    rt_frame.base = (unsigned char *)sys_paginas_reservar(FORJA_FRAME_BYTES);
    if (rt_frame.base == NULL)
        forja_falha_alocacao("sem memoria para o frame em bytes", FORJA_FRAME_BYTES, arquivo, linha);
}

void *forja_frame_alocar(int64_t bytes, const char *arquivo, int linha)
{
    int64_t alinhado;
    void *p;

    if (bytes > FORJA_FRAME_BYTES)
        forja_falha_alocacao("frame cheio: pedido maior que o frame inteiro em bytes", bytes, arquivo, linha);
    alinhado = RT_ALINHA(bytes);
    if (rt_frame.base == NULL)
        rt_frame_reservar(arquivo, linha);
    if (alinhado > FORJA_FRAME_BYTES - rt_frame.topo)
        forja_falha_alocacao("frame cheio: chame frame_reset() a cada quadro; bytes pedidos", bytes, arquivo, linha);
    p = rt_frame.base + rt_frame.topo;
    rt_frame.topo += alinhado;
    return p;
}

void *forja_frame_realocar(void *velho, int64_t bytes_velho, int64_t bytes_novo, const char *arquivo, int linha)
{
    void *novo = forja_frame_alocar(bytes_novo, arquivo, linha);

    if (velho != NULL && bytes_velho > 0)
        memcpy(novo, velho, (size_t)bytes_velho);
    return novo;
}

/* Dev enche o frame usado com 0xDD (uso depois do reset vira lixo reconhecivel); a epoca sobe e, no maximo, volta a 1. */
void forja_frame_reset(void)
{
#ifdef FORJA_DEV
    if (rt_frame.base != NULL && rt_frame.topo > 0)
        memset(rt_frame.base, RT_BYTE_BLOCO_MORTO, (size_t)rt_frame.topo);
#endif
    rt_frame.topo = 0;
    rt_frame.geracao = rt_frame.geracao == UINT32_MAX - 1u ? 0u : rt_frame.geracao + 1u;
}

/* ---- heap ---- */

#ifdef FORJA_DEV
void *forja_heap_alocar(int64_t bytes, const char *arquivo, int linha)
{
    ForjaCabecalho *c = NULL;

    if (rt_guarda)
        c = rt_guarda_alocar(bytes, arquivo, linha); /* --guarda-memoria (M6e P93e): paginas proprias + pagina de guarda; falta de enderecos relata R0007 la dentro */
    else if (bytes <= INT64_MAX - RT_ALOC_EXTRA)
        c = (ForjaCabecalho *)sys_heap_alocar((int64_t)((uint64_t)bytes + (uint64_t)RT_ALOC_EXTRA));
    if (c == NULL)
        forja_falha_alocacao("sem memoria no heap para bytes", bytes, arquivo, linha);
    return rt_rastreio_criar(c, bytes, arquivo, linha);
}

void *forja_heap_realocar(void *velho, int64_t bytes_novo, const char *arquivo, int linha)
{
    ForjaCabecalho *c;
    void *novo;

    if (velho == NULL)
        return forja_heap_alocar(bytes_novo, arquivo, linha);
    c = rt_rastreio_vivo(velho, "append", arquivo, linha);
    /* M6e P93c (D-M6e-5): em dev o append SEMPRE move (nunca cresce no lugar): bloco novo com a pilha do append, copia, e o velho morre com causa append_moveu (envenenado e em quarentena). */
    novo = forja_heap_alocar(bytes_novo, arquivo, linha);
    ((ForjaCabecalho *)novo - 1)->arquivo = c->arquivo; /* P93k (D-M6e-3): o bloco novo herda arquivo/linha da ALOCACAO original; so a pilha e a do append */
    ((ForjaCabecalho *)novo - 1)->linha = c->linha;
    memcpy(novo, velho, (size_t)(c->bytes < bytes_novo ? c->bytes : bytes_novo));
    rt_rastreio_baixa(c, RT_MORTE_APPEND_MOVEU);
    return novo;
}

void forja_mem_liberar(void *p, const char *arquivo, int linha)
{
    ForjaCabecalho *c;

    if (p == NULL || forja_no_frame(p))
        return;
    c = rt_rastreio_vivo(p, "delete", arquivo, linha);
    rt_rastreio_baixa(c, RT_MORTE_DELETE); /* o bloco vai para a quarentena e so volta ao sistema de la */
}

void forja_lista_viva(uint32_t epoca, const char *arquivo, int linha)
{
    RtCausa causa;

    if (epoca == 0 || epoca == rt_frame_epoca())
        return;
    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "lista do frame usada depois do frame_reset() (quadro ");
    rt_causa_u64(&causa, epoca);
    rt_causa_texto(&causa, ", agora ");
    rt_causa_u64(&causa, rt_frame_epoca());
    rt_causa_texto(&causa, ")");
    forja_falha(FORJA_R0008, rt_causa_fim(&causa), arquivo, linha);
}

int64_t forja_indice_lista(int64_t i, int64_t n, uint32_t epoca, const char *arquivo, int linha)
{
    forja_lista_viva(epoca, arquivo, linha);
    if (i < 0 || i >= n)
        forja_falha_indice(i, n, arquivo, linha);
    return i;
}
#else
void *forja_heap_alocar(int64_t bytes, const char *arquivo, int linha)
{
    void *p = sys_heap_alocar(bytes);

    if (p == NULL)
        forja_falha_alocacao("sem memoria no heap para bytes", bytes, arquivo, linha);
    return p;
}

void *forja_heap_realocar(void *velho, int64_t bytes_novo, const char *arquivo, int linha)
{
    void *p = velho == NULL ? sys_heap_alocar(bytes_novo) : sys_heap_realocar(velho, bytes_novo);

    if (p == NULL)
        forja_falha_alocacao("sem memoria no heap para bytes", bytes_novo, arquivo, linha);
    return p;
}

void forja_mem_liberar(void *p, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (p == NULL || forja_no_frame(p))
        return;
    sys_heap_liberar(p);
}
#endif

/* ---- registro de alocadores (Arena e Pool) ---- */

typedef struct {
    uint32_t geracao;
    uint8_t tipo; /* 0 livre, 1 arena, 2 pool */
    int64_t bloco, usado, tam_slot, vivos, por_bloco;
    void *blocos, *livres;
#ifdef FORJA_DEV
    uintptr_t ras[RT_RAS_MAX]; /* pilha .fj de quem criou (D-M6-7), para o R0009 de Arena/Pool vivo no fim */
#endif
} ForjaAlocador;

typedef struct ForjaBloco {
    struct ForjaBloco *prox;
    int64_t cap, topo, reservado;
} ForjaBloco;
_Static_assert(sizeof(ForjaBloco) == 32, "ForjaBloco tem de ter 32 bytes (alinhamento de 16)");

static ForjaAlocador *rt_alocs; /* compartilhado: registro de Arena e Pool, sob rt_aloc_trava */
static uint32_t rt_alocs_n; /* compartilhado: idem rt_alocs */
static uint32_t rt_alocs_cap; /* compartilhado: idem rt_alocs */
/* Trava do registro de Arena e Pool e dos blocos deles (so com rt_multithread). Nao recursiva: so as funcoes publicas a pegam. */
static SysTrava rt_aloc_trava; /* compartilhado: so com rt_multithread ligado */

/* Cresce o vetor de registros (8, depois dobra); a parte nova nasce zerada, como o realloc + memset do forja_rt.h. */
static void rt_aloc_garantir(void)
{
    ForjaAlocador *novo;
    uint32_t cap;
    int64_t bytes;

    if (rt_alocs_n < rt_alocs_cap)
        return;
    cap = rt_alocs_cap == 0 ? 8u : rt_alocs_cap * 2u;
    bytes = (int64_t)cap * (int64_t)sizeof(ForjaAlocador);
    novo = (ForjaAlocador *)(rt_alocs == NULL ? sys_heap_alocar(bytes) : sys_heap_realocar(rt_alocs, bytes));
    if (novo == NULL)
        forja_falha_alocacao("sem memoria para o registro de alocadores em entradas", cap, FORJA_ARQ_STD, 0);
    memset(novo + rt_alocs_cap, 0, (size_t)(cap - rt_alocs_cap) * sizeof(ForjaAlocador));
    rt_alocs = novo;
    rt_alocs_cap = cap;
}

/* Primeira entrada livre a partir do idx 3 (abre uma nova se nao houver). */
static uint32_t rt_aloc_livre(void)
{
    uint32_t i = FORJA_ALOC_RESERVADOS;

    if (rt_alocs_n == 0) {
        rt_aloc_garantir();
        rt_alocs_n = FORJA_ALOC_RESERVADOS;
    }
    while (i < rt_alocs_n && rt_alocs[i].tipo != 0)
        i++;
    if (i == rt_alocs_n) {
        if (rt_alocs_n >= FORJA_ALOCS_MAX)
            forja_falha_alocacao("alocadores demais vivos ao mesmo tempo (limite)", FORJA_ALOCS_MAX, FORJA_ARQ_STD, 0);
        rt_aloc_garantir();
        rt_alocs_n++;
    }
    return i;
}

/* A geracao sobe a cada reuso da entrada e volta a 1 no teto. */
static uint32_t rt_aloc_registrar(uint8_t tipo, int64_t bloco)
{
    uint32_t i = rt_aloc_livre();
    ForjaAlocador *a = &rt_alocs[i];
    uint32_t ger = a->geracao >= FORJA_ALOC_GERACAO_MAX ? 1u : a->geracao + 1u;

    *a = (ForjaAlocador){.geracao = ger, .tipo = tipo, .bloco = bloco};
    return (ger << 12) | i;
}

static uint32_t rt_aloc_handle(const ForjaAlocador *a)
{
    return (a->geracao << 12) | (uint32_t)(a - rt_alocs);
}

#ifdef FORJA_DEV
/* Guarda a pilha de quem criou o alocador h (sob rt_aloc_trava, logo depois do registrar). */
static void rt_aloc_guardar_ras(uint32_t h, const uintptr_t *ras)
{
    ForjaAlocador *a = &rt_alocs[h & 0xFFFu];

    for (int k = 0; k < RT_RAS_MAX; k++)
        a->ras[k] = ras[k];
}
#endif

/* Entrada do handle; desconhecido e R0007, entrada livre ou de outra geracao (usado depois do destroy) e R0008. */
static ForjaAlocador *rt_aloc_de(uint32_t h, const char *arquivo, int linha)
{
    uint32_t idx = h & 0xFFFu;
    ForjaAlocador *a;
    RtCausa causa;

    if (idx < FORJA_ALOC_RESERVADOS || idx >= rt_alocs_n)
        forja_falha_alocacao("alocador desconhecido (id): crie com arena_create ou pool_create", (int64_t)h, arquivo, linha);
    a = &rt_alocs[idx];
    if (a->tipo == 0 || a->geracao != (h >> 12)) {
        rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
        rt_causa_texto(&causa, "Arena ou Pool usado depois do destroy (id ");
        rt_causa_u64(&causa, h);
        rt_causa_texto(&causa, ")");
        forja_falha(FORJA_R0008, rt_causa_fim(&causa), arquivo, linha);
    }
    return a;
}

/* ---- blocos ---- */

static ForjaBloco *rt_bloco_novo(ForjaBloco *prox, int64_t cap, const char *arquivo, int linha)
{
    ForjaBloco *b = NULL;

    if (cap >= 0 && cap <= INT64_MAX / 2)
        b = (ForjaBloco *)sys_heap_alocar(cap + (int64_t)sizeof(ForjaBloco));
    if (b == NULL)
        forja_falha_alocacao("sem memoria para o bloco do alocador em bytes", cap, arquivo, linha);
    b->prox = prox;
    b->cap = cap;
    b->topo = 0;
    b->reservado = 0;
    return b;
}

static unsigned char *rt_bloco_dados(ForjaBloco *b)
{
    return (unsigned char *)(b + 1);
}

static void rt_blocos_soltar(ForjaBloco *b)
{
    ForjaBloco *prox;

    for (; b != NULL; b = prox) {
        prox = b->prox;
#ifdef FORJA_DEV
        if (rt_guarda && rt_guarda_dono(b)) {
            rt_guarda_enterrar_bloco(b, RT_MORTE_ARENA_DESTROY); /* pedaco de Arena com guarda: cemiterio, nunca volta (D-M6e-7) */
            continue;
        }
#endif
        sys_heap_liberar(b);
    }
}

/* ---- Arena: bloco atual = cabeca da cadeia; sem espaco, um bloco novo vira a cabeca ---- */

#ifdef FORJA_DEV
/* Pedaco de Arena do alocador de guarda (M6e P93e): o registro leva a pilha da criacao da Arena como "alocacao". */
static ForjaBloco *rt_bloco_novo_guarda(ForjaBloco *prox, int64_t cap, const ForjaAlocador *a)
{
    ForjaBloco *b = (ForjaBloco *)rt_guarda_arena(cap + (int64_t)sizeof(ForjaBloco), a->ras);

    b->prox = prox;
    b->cap = cap;
    b->topo = 0;
    b->reservado = 0;
    return b;
}
#endif

static ForjaBloco *rt_arena_bloco(ForjaAlocador *a, int64_t alinhado, const char *arquivo, int linha)
{
    ForjaBloco *b = (ForjaBloco *)a->blocos;

    if (b != NULL && alinhado <= b->cap - b->topo)
        return b;
#ifdef FORJA_DEV
    if (rt_guarda) {
        b = rt_bloco_novo_guarda(b, alinhado > a->bloco ? alinhado : a->bloco, a);
        a->blocos = b;
        return b;
    }
#endif
    b = rt_bloco_novo(b, alinhado > a->bloco ? alinhado : a->bloco, arquivo, linha);
    a->blocos = b;
    return b;
}

static void *rt_arena_alocar(ForjaAlocador *a, int64_t bytes, const char *arquivo, int linha)
{
    ForjaBloco *b;
    int64_t alinhado;
    unsigned char *p;

    if (bytes < 0 || bytes > INT64_MAX / 2)
        forja_falha_alocacao("sem memoria na Arena para bytes", bytes, arquivo, linha);
    alinhado = RT_ALINHA(bytes + RT_ALOC_EXTRA);
    b = rt_arena_bloco(a, alinhado, arquivo, linha);
    p = rt_bloco_dados(b) + b->topo + RT_ALOC_EXTRA;
    b->topo += alinhado;
    a->usado += RT_ALINHA(bytes);
    rt_aloc_cabecalho(p, FORJA_MARCA_ARENA, rt_aloc_handle(a));
    rt_aloc_origem(p, bytes, arquivo, linha);
    return p;
}

uint32_t forja_arena_criar(int64_t bloco)
{
    RT_RAS_LOCAL(ras)
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    uint32_t h = rt_aloc_registrar(FORJA_TIPO_ARENA, bloco < FORJA_BLOCO_MIN ? FORJA_BLOCO_MIN : bloco);

    rt_aloc_guardar_ras(h, ras);
    sys_trava_soltar(&rt_aloc_trava, tomou);
    rt_aloc_saida_registrar();
    return h;
}

/* Limpar: solta todos os blocos menos o da cabeca da cadeia (o mais novo); dev enche o usado dele com 0xDD. */
void forja_arena_limpar(uint32_t h)
{
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    ForjaAlocador *a = rt_aloc_de(h, FORJA_ARQ_STD, 0);
    ForjaBloco *b = (ForjaBloco *)a->blocos;

    if (b != NULL) {
        rt_blocos_soltar(b->prox);
        b->prox = NULL;
#ifdef FORJA_DEV
        memset(rt_bloco_dados(b), RT_BYTE_BLOCO_MORTO, (size_t)b->topo);
#endif
        b->topo = 0;
    }
    a->usado = 0;
    sys_trava_soltar(&rt_aloc_trava, tomou);
}

void forja_arena_destruir(uint32_t h)
{
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    ForjaAlocador *a = rt_aloc_de(h, FORJA_ARQ_STD, 0);

    rt_blocos_soltar((ForjaBloco *)a->blocos);
    *a = (ForjaAlocador){.geracao = a->geracao};
    sys_trava_soltar(&rt_aloc_trava, tomou);
}

int64_t forja_arena_usado(uint32_t h)
{
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    int64_t usado = rt_aloc_de(h, FORJA_ARQ_STD, 0)->usado;

    sys_trava_soltar(&rt_aloc_trava, tomou);
    return usado;
}

/* ---- Pool: o tamanho do slot fixa no 1o pedido; a lista livre (LIFO) guarda o proximo no proprio slot ---- */

static void rt_pool_pedido_grande(const ForjaAlocador *a, int64_t bytes, const char *arquivo, int linha)
{
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "pool de ");
    rt_causa_i64(&causa, a->tam_slot);
    rt_causa_texto(&causa, " bytes por objeto recebeu um pedido maior (");
    rt_causa_i64(&causa, bytes);
    rt_causa_texto(&causa, ")");
    forja_falha(FORJA_R0007, rt_causa_fim(&causa), arquivo, linha);
}

static void rt_pool_fixar_slot(ForjaAlocador *a, int64_t bytes, const char *arquivo, int linha)
{
    if (a->tam_slot == 0) {
        if (bytes > INT64_MAX / 2)
            forja_falha_alocacao("sem memoria no Pool para bytes", bytes, arquivo, linha);
        a->tam_slot = RT_ALINHA(bytes);
        if (a->tam_slot < 16)
            a->tam_slot = 16;
        return;
    }
    if (bytes > a->tam_slot)
        rt_pool_pedido_grande(a, bytes, arquivo, linha);
}

/* Bloco novo com por_bloco slots, todos na lista livre (o slot 0 fica no topo da lista). */
static void rt_pool_bloco(ForjaAlocador *a, const char *arquivo, int linha)
{
    int64_t passo = a->tam_slot + RT_ALOC_EXTRA;
    int64_t i;
    ForjaBloco *b;
    unsigned char *p;

    if (a->por_bloco > INT64_MAX / 2 / passo)
        forja_falha_alocacao("Pool com slots demais por bloco", a->por_bloco, arquivo, linha);
    b = rt_bloco_novo((ForjaBloco *)a->blocos, a->por_bloco * passo, arquivo, linha);
    a->blocos = b;
    for (i = a->por_bloco - 1; i >= 0; i--) {
        p = rt_bloco_dados(b) + i * passo + RT_ALOC_EXTRA;
        rt_aloc_cabecalho(p, FORJA_MARCA_MORTA, 0u);
        *(void **)p = a->livres;
        a->livres = p;
    }
}

static void *rt_pool_alocar(ForjaAlocador *a, int64_t bytes, const char *arquivo, int linha)
{
    void *p;

    rt_pool_fixar_slot(a, bytes, arquivo, linha);
    if (a->livres == NULL)
        rt_pool_bloco(a, arquivo, linha);
    p = a->livres;
    a->livres = *(void **)p;
    a->vivos++;
    rt_aloc_cabecalho(p, FORJA_MARCA_POOL, rt_aloc_handle(a));
    rt_aloc_origem(p, bytes, arquivo, linha);
    return p;
}

uint32_t forja_pool_criar(int64_t por_bloco)
{
    RT_RAS_LOCAL(ras)
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    uint32_t h = rt_aloc_registrar(FORJA_TIPO_POOL, 0);

    rt_aloc_de(h, FORJA_ARQ_STD, 0)->por_bloco = por_bloco < 1 ? 1 : por_bloco;
    rt_aloc_guardar_ras(h, ras);
    sys_trava_soltar(&rt_aloc_trava, tomou);
    rt_aloc_saida_registrar();
    return h;
}

void forja_pool_devolver(uint32_t h, void *p, const char *arquivo, int linha)
{
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    ForjaAlocador *a = rt_aloc_de(h, arquivo, linha);

    if (p != NULL) {
        rt_pool_conferir(h, p, arquivo, linha);
        a->vivos--;
        *(void **)p = a->livres;
        a->livres = p;
    }
    sys_trava_soltar(&rt_aloc_trava, tomou);
}

void forja_pool_destruir(uint32_t h)
{
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    ForjaAlocador *a = rt_aloc_de(h, FORJA_ARQ_STD, 0);

    rt_blocos_soltar((ForjaBloco *)a->blocos);
    *a = (ForjaAlocador){.geracao = a->geracao};
    sys_trava_soltar(&rt_aloc_trava, tomou);
}

int64_t forja_pool_vivos(uint32_t h)
{
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    int64_t vivos = rt_aloc_de(h, FORJA_ARQ_STD, 0)->vivos;

    sys_trava_soltar(&rt_aloc_trava, tomou);
    return vivos;
}

#ifdef FORJA_DEV
/* Bytes que o alocador ainda segura: a soma dos blocos dele. */
static int64_t rt_alocador_bytes(const ForjaAlocador *a)
{
    int64_t total = 0;

    for (const ForjaBloco *b = (const ForjaBloco *)a->blocos; b != NULL; b = b->prox)
        total += b->cap;
    return total;
}

/* Handler de saida (P70): uma linha R0009 por Arena e por Pool que nao foi destruido, com a pilha da criacao. Devolve quantos eram. */
static int rt_alocs_vivos_relatar(void)
{
    int vivos = 0;
    bool tomou = sys_trava_pegar(&rt_aloc_trava);

    for (uint32_t i = FORJA_ALOC_RESERVADOS; i < rt_alocs_n; i++) {
        const ForjaAlocador *a = &rt_alocs[i];

        if (a->tipo == 0)
            continue;
        if (a->tipo == FORJA_TIPO_ARENA)
            rt_alocador_linha("arena", "arena_destroy", rt_alocador_bytes(a), a->ras);
        else
            rt_alocador_linha("pool", "pool_destroy", rt_alocador_bytes(a), a->ras);
        vivos++;
    }
    sys_trava_soltar(&rt_aloc_trava, tomou);
    return vivos;
}
#endif

/* ---- despacho por alocador e listas ---- */

static void *rt_aloc_alocar(uint32_t h, int64_t bytes, const char *arquivo, int linha)
{
    ForjaAlocador *a = rt_aloc_de(h, arquivo, linha);

    if (a->tipo == FORJA_TIPO_ARENA)
        return rt_arena_alocar(a, bytes, arquivo, linha);
    return rt_pool_alocar(a, bytes, arquivo, linha);
}

void *forja_mem_alocar(uint32_t alocador, int64_t bytes, const char *arquivo, int linha)
{
    bool tomou;
    void *p;

    if (alocador == FORJA_ALOC_HEAP)
        return forja_heap_alocar(bytes, arquivo, linha);
    if (alocador == FORJA_ALOC_FRAME)
        return forja_frame_alocar(bytes, arquivo, linha);
    tomou = sys_trava_pegar(&rt_aloc_trava);
    p = rt_aloc_alocar(alocador, bytes, arquivo, linha);
    sys_trava_soltar(&rt_aloc_trava, tomou);
    return p;
}

/* Lista em Arena/Pool: so a Arena cresce (copia para um bloco novo); Pool recusa. Trava o registro. */
static void *rt_aloc_lista(uint32_t h, int64_t bytes, const char *arquivo, int linha)
{
    bool tomou = sys_trava_pegar(&rt_aloc_trava);
    void *p;

    if (rt_aloc_de(h, arquivo, linha)->tipo == FORJA_TIPO_POOL)
        forja_falha(FORJA_R0007, "lista nao cresce em Pool", arquivo, linha);
    p = rt_aloc_alocar(h, bytes, arquivo, linha);
    sys_trava_soltar(&rt_aloc_trava, tomou);
    return p;
}

static void *rt_lista_copiar(void *novo, const void *velho, int64_t bytes)
{
    if (velho != NULL && bytes > 0)
        memcpy(novo, velho, (size_t)bytes);
    return novo;
}

void *forja_lista_criar(uint32_t alocador, int64_t cap, int64_t tam_elem, uint32_t *epoca, const char *arquivo, int linha)
{
    *epoca = 0;
    if (cap < 0)
        forja_falha_alocacao("make com cap negativo", cap, arquivo, linha);
    if (cap == 0)
        return NULL;
    if (cap > INT64_MAX / tam_elem)
        forja_falha_alocacao("make com cap grande demais para a memoria", cap, arquivo, linha);
    if (alocador == FORJA_ALOC_FRAME)
        *epoca = rt_frame_epoca();
    return forja_mem_alocar(alocador, cap * tam_elem, arquivo, linha);
}

void *forja_lista_crescer(void *ptr, int64_t len, int64_t *cap, int64_t tam_elem, uint32_t alocador, uint32_t *epoca, const char *arquivo, int linha)
{
    int64_t novo;
    void *p;

    if (alocador == 0)
        forja_falha_alocacao("append numa lista sem alocador (valor zero): crie com make([dynamic]T, em = heap)", (int64_t)alocador, arquivo, linha);
    if (*cap > INT64_MAX / 2 / tam_elem)
        forja_falha_alocacao("append: a lista passou do limite de memoria com cap", *cap, arquivo, linha);
    novo = *cap < 4 ? 8 : *cap * 2;
    if (alocador == FORJA_ALOC_FRAME) {
        p = forja_frame_realocar(ptr, len * tam_elem, novo * tam_elem, arquivo, linha);
        *epoca = rt_frame_epoca();
    } else if (alocador == FORJA_ALOC_HEAP) {
        p = forja_heap_realocar(ptr, novo * tam_elem, arquivo, linha);
    } else {
        p = rt_lista_copiar(rt_aloc_lista(alocador, novo * tam_elem, arquivo, linha), ptr, len * tam_elem);
    }
    *cap = novo;
    return p;
}

#ifdef FORJA_DEV
/* M6e P93k (D-M6e-5): chamada pelo BIR so com --guarda-memoria; recebe o bloco {ptr, len, cap, alocador, epoca} da lista e devolve o ptr novo. Lista de heap: crescimento EXATO, novo = max(len + 1, alvo),
 * e TODO append move (cap() le o valor exato); os outros alocadores so crescem se cap < alvo (o dobro de sempre).
 * LIMITE: parametros 5 (a lista, o tamanho do elemento, o alvo e o ponto no fonte; o bloco da lista ja agrupa o resto). */
void *forja_lista_crescer_alvo(forja_lista_layout *lista, int64_t tam_elem, int64_t alvo, const char *arquivo, int linha)
{
    int64_t novo = lista->len + 1;
    void *p;

    if (lista->alocador == 0)
        forja_falha_alocacao("append numa lista sem alocador (valor zero): crie com make([dynamic]T, em = heap)", (int64_t)lista->alocador, arquivo, linha);
    if (lista->cap > INT64_MAX / 2 / tam_elem)
        forja_falha_alocacao("append: a lista passou do limite de memoria com cap", lista->cap, arquivo, linha);
    if (lista->alocador != FORJA_ALOC_HEAP && lista->cap >= alvo)
        return lista->ptr;
    if (lista->alocador != FORJA_ALOC_HEAP)
        novo = lista->cap < 4 ? 8 : lista->cap * 2;
    else if (alvo > novo)
        novo = alvo;
    if (lista->alocador == FORJA_ALOC_FRAME) {
        p = forja_frame_realocar(lista->ptr, lista->len * tam_elem, novo * tam_elem, arquivo, linha);
        lista->epoca = rt_frame_epoca();
    } else if (lista->alocador == FORJA_ALOC_HEAP) {
        p = forja_heap_realocar(lista->ptr, novo * tam_elem, arquivo, linha);
    } else {
        p = rt_lista_copiar(rt_aloc_lista(lista->alocador, novo * tam_elem, arquivo, linha), lista->ptr, lista->len * tam_elem);
    }
    lista->cap = novo;
    return p;
}
#endif

void forja_lista_liberar(void *ptr, uint32_t alocador, const char *arquivo, int linha)
{
    if (alocador != FORJA_ALOC_HEAP || ptr == NULL)
        return;
    forja_mem_liberar(ptr, arquivo, linha);
}
