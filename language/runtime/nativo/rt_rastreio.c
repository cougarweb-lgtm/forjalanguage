/*
 * rt_rastreio.c - rastreio de blocos de heap em DEV do runtime nativo da Forja (M2 P14a).
 *
 * Versao: 0.10.2
 * Changelog:
 *   0.10.2 - M6e P93k: rt_vazamento_do_teste escolhe os 'novos' pela serie de nascimento (nao pela diferenca de contagem); rt_rastreio_vivo pega o gate rt_falha_reservar antes de gravar rt_morto.
 *   0.10.1 - M6e P93h: os ponteiros rt_libmem_relatar_fn / rt_libmem_libs_fn sao lidos com load-acquire (par do store-release de rt_libmem.c 0.3.0); sem trava.
 *   0.10.0 - M6e P93e: inclui rt_guarda.c (alocador de guarda, cemiterio e pre-tratador); rt_rastreio_baixa manda o bloco de guarda ao cemiterio (so com rt_guarda). Sem rt_pilha.c, rt_guarda e a constante false.
 *   0.9.0 - M6e P93c: cabecalho com ras_off[8] u32 (pilha da alocacao [0..3] e do delete [4..7], D-M6e-3); a baixa preserva a da alocacao e grava a causa da morte (RT_MORTE_*) em `alocador`; o R0008 de bloco
 *           morto leva "bloco" e as 3 pilhas (rt_morto_pendente, rt_pilha.c). rt_rastreio_mover/reencadear saem: o append de heap em dev nao realoca mais (aloca, copia e baixa, rt_aloc.c).
 *   0.8.0 - M6d P92d: inclui rt_libmem.c (tabela lateral por lib); handler de saida chama rt_libmem_relatar_fn e o report_since acrescenta "libs" (rt_libmem_libs_fn), ambos so depois da 1a alocacao de lib.
 *   0.7.1 - M6d P92c: inclui rt_libmem_slots.c no fim (involucros forja_libmem_*; so Linux e Windows CRT).
 *   0.7.0 - M6 P71b: handles genericos de lib C (track_handle/untrack_handle de std:mem) em rt_handles.c, incluido no FIM deste arquivo; o handler de saida soma os handles vivos aos de Arena/Pool (mesmo R0009 e exit 101).
 *   0.6.0 - M6 P71a: snapshot e report_since de std:mem (forja_memsnap_serie|vivos|bytes|relatar). O campo `alocador` do cabecalho, que em bloco VIVO de heap ninguem le (rt_pool_conferir so le em marca POOL;
 *           o mapa usa o alocador dele), guarda a SERIE DE NASCIMENTO uint32 (rt_serie_prox, atribuida sob rt_rastreio_trava): o cabecalho segue com 80 B. Nascido depois do snapshot = (int32_t)(serie - serie_do_snapshot) >= 0.
 *           RESSALVA: a serie da a volta a cada 2^32 (~4 bilhoes) de alocacoes; um snapshot com mais de 2^31 alocacoes de idade confunde nascidos com anteriores. Novo contador rt_vivos_bytes (sob a trava).
 *   0.5.0 - M6 P70: cabecalho de 80 B (+4 enderecos de retorno da pilha .fj de quem alocou, D-M6-7); a linha R0009 leva "pilha" (resolvida so no relatorio, rt_pilha.c); Arena e Pool vivos no
 *           fim viram linhas R0009 com "tipo":"arena"|"pool" e a pilha da criacao (rt_alocs_vivos_relatar, em rt_aloc.c); quarentena de ~64 MiB dos blocos liberados (D-M6-15a): a marca MORTA
 *           sobrevive a muitas alocacoes (delete duplo = R0008 sempre) e a pilha do delete fica no cabecalho do bloco retido. Sem FORJA_RASTREIO a pilha e vazia e a linha sai como antes.
 *   0.4.0 - M5 P44: lista dos vivos, rt_vivos_n e rt_atexit_ok sob rt_rastreio_trava (SRW, so com rt_multithread): criar, baixar e os relatorios de saida travam sozinhos; rt_rastreio_mover exige a trava de quem chama (forja_heap_realocar a segura em volta do realocar). O vazamento por teste (rt_vazamento_do_teste) segue sem trava: o executor de testes e de uma thread so.
 *   0.3.0 - M3 P22a: rt_vazamentos_relatar_dll (DETACH da DLL por FreeLibrary: relata sem sair; corpo proprio para nao mudar a ordem das strings do .rodata e o sha256 dos exes em dev).
 *   0.2.0 - Linguagem v1.11 L16b: rt_vazamento_do_teste (fim de um teste que passou: os blocos vivos alem dos de antes falham o teste com linhas R0009 e saem da lista); a causa do R0009 passa por
 *           rt_vazamento_causa e a lista de linhas por rt_vazamentos_listar (mesmo texto e mesma ordem de antes).
 *   0.1.0 - primeira versao: cabecalho de 48 B (marca viva/morta, tamanho, arquivo e linha), lista dos blocos vivos (mais antigo primeiro),
 *           conferencia do bloco (R0008), marcas de Arena e Pool e o R0009 de vazamento no fim do programa (forja_rt_ao_sair registrado na
 *           1a alocacao de heap). Portado de forja_rt.h:984-1181 com os mesmos textos e a mesma ordem da lista.
 *
 * So existe com FORJA_DEV (em release nao ha cabecalho nem relatorio). Vem ANTES de rt_aloc.c na unidade e depois de rt_nativo.c, de quem usa
 * RtCausa, RtErroLote, rt_sem_relatorio e forja_falha. O R0009 descarrega o stdout e sai com 101 (forja_sair nao e usado: ele rodaria os handlers de novo).
 * Incluido por rt_win.c (P14b); nao compila sozinho.
 */
#ifndef FORJA_DEV
#error "rt_rastreio.c so existe com -DFORJA_DEV"
#endif
#define RT_RASTREIO_C 1

#define FORJA_MARCA_VIVA 0x4A524F4656495641ull
#define FORJA_MARCA_MORTA 0x4A524F464D4F5254ull
#define FORJA_MARCA_ARENA 0x4A524F4641524E41ull
#define FORJA_MARCA_POOL 0x4A524F46504F4F4Cull
#define FORJA_VAZAMENTOS_MAX 16
#define RT_BYTE_BLOCO_MORTO 0xDD

#define RT_RAS_MAX 4 /* enderecos de retorno guardados na criacao (D-M6-7) */
#define RT_QUARENTENA_BYTES ((int64_t)64 * 1024 * 1024)

#ifdef FORJA_RASTREIO
_Static_assert(RT_RAS_MAX == RT_PILHA_RAS_MAX, "RT_RAS_MAX tem de ser o RT_PILHA_RAS_MAX do rt_pilha.c");
#define rt_ras_capturar(ras) rt_pilha_ras(ras)
#define rt_ras_capturar_lib(ras) rt_pilha_ras_lib(ras)
#define rt_ras_json(lote, ras) rt_pilha_ras_json((lote), (ras))
#define rt_ras_chamador(ras, arquivo, linha) rt_pilha_ras_chamador((ras), (arquivo), (linha))
/* M6e P93c: a pilha do cabecalho de heap e em offsets u32 (D-M6e-3); morto_pendente deixa o R0008 levar bloco e as pilhas (rt_pilha.c, RtMorto). */
#define rt_ras_capturar_off(off) rt_pilha_ras_off(off)
#define rt_ras_json_off(lote, off) rt_pilha_off_json((lote), "pilha", (off), false)
#define rt_morto_pendente(bloco, c) \
    (rt_falha_extra = rt_pilha_morto_fase, rt_morto = (RtMorto){true, (bloco), (c)->arquivo, (c)->linha, {(c)->ras_off[0], (c)->ras_off[1], (c)->ras_off[2], (c)->ras_off[3], (c)->ras_off[4], (c)->ras_off[5], (c)->ras_off[6], (c)->ras_off[7]}})
#else
/* Sem a variante de rastreio (rt_pilha.c ausente) nao ha pilha: o cabecalho guarda zeros e a linha R sai como antes. */
static void rt_ras_capturar(uintptr_t *ras)
{
    for (int k = 0; k < RT_RAS_MAX; k++)
        ras[k] = 0;
}
#define rt_ras_capturar_lib(ras) rt_ras_capturar(ras)
#define rt_ras_json(lote, ras) ((void)(ras))
#define rt_ras_chamador(ras, arquivo, linha) ((void)(ras))
static void rt_ras_capturar_off(uint32_t *off)
{
    for (int k = 0; k < RT_RAS_MAX; k++)
        off[k] = 0;
}
#define rt_ras_json_off(lote, off) ((void)(off))
#define rt_morto_pendente(bloco, c) ((void)(bloco), (void)(c))
#define rt_guarda false /* sem rt_pilha.c nao ha FRT1 com o bit: a guarda nunca liga */
#endif

/* Cabecalho de cada bloco de heap em DEV: 80 B (48 + 8 offsets u32; mantem o alinhamento de 16). Os blocos vivos formam uma lista (mais antigo primeiro).
 * Vivo: ras_off[0..3] = pilha da alocacao. Morto (M6e P93c, D-M6e-3): [0..3] mantem a da alocacao, [4..7] = a do delete (ou append que moveu, ou arena_destroy), e `alocador`
 * guarda a CAUSA DA MORTE (RT_MORTE_*); ant/prox encadeiam a quarentena (prox so, mais antigo primeiro). Offset = endereco de retorno - forja_rastro_texto; 0 = sem quadro. */
typedef struct ForjaCabecalho {
    uint64_t marca;
    int64_t bytes;
    const char *arquivo;
    int32_t linha;
    uint32_t alocador;
    struct ForjaCabecalho *ant, *prox;
    uint32_t ras_off[2 * RT_RAS_MAX];
} ForjaCabecalho;
#define RT_MORTE_NENHUMA 0u
#define RT_MORTE_DELETE 1u
#define RT_MORTE_APPEND_MOVEU 2u
#define RT_MORTE_ARENA_DESTROY 3u
_Static_assert(sizeof(ForjaCabecalho) == 80, "ForjaCabecalho tem de ter 80 bytes (alinhamento de 16)");

#define RT_ALOC_EXTRA ((int64_t)sizeof(ForjaCabecalho))
#define RT_RAS_LOCAL(ras) uintptr_t ras[RT_RAS_MAX]; rt_ras_capturar(ras);
#define rt_aloc_saida_registrar() rt_rastreio_registrar_saida()

static ForjaCabecalho *rt_heap_cabeca; /* compartilhado: lista dos vivos, sob rt_rastreio_trava */
static ForjaCabecalho *rt_heap_cauda; /* compartilhado: idem rt_heap_cabeca */
static int64_t rt_vivos_n; /* compartilhado: idem rt_heap_cabeca (a leitura de forja_rt_vivos_qtd e uma palavra, sem trava) */
static bool rt_atexit_ok; /* compartilhado: idem rt_heap_cabeca */
static ForjaCabecalho *rt_quar_cabeca; /* compartilhado: quarentena dos liberados (mais antigo primeiro, por prox), sob rt_rastreio_trava */
static ForjaCabecalho *rt_quar_cauda; /* compartilhado: idem rt_quar_cabeca */
static int64_t rt_quar_bytes; /* compartilhado: idem rt_quar_cabeca (bytes dos blocos retidos, com cabecalho) */
static int64_t rt_vivos_bytes; /* compartilhado: idem rt_heap_cabeca (soma de bytes dos blocos vivos, sem cabecalho; o snapshot de std:mem) */
static uint32_t rt_serie_prox; /* compartilhado: idem rt_heap_cabeca (serie do proximo bloco de heap a nascer; da a volta em 2^32 alocacoes, ver o cabecalho do arquivo) */
/* Trava da lista dos vivos. Quem chama rt_rastreio_mover a segura em volta do realocar (o bloco pode mudar de endereco: os vizinhos apontam para o velho ate o reencadear). */
static SysTrava rt_rastreio_trava; /* compartilhado: so com rt_multithread ligado */
/* ---- alocador de guarda (M6e P93e): --guarda-memoria, so alcancavel por rt_guarda == true ---- */
#include "rt_guarda.c"

/* Quantos blocos de heap seguem vivos (so DEV rastreia; em release o C devolve 0). */
int64_t forja_rt_vivos_qtd(void)
{
    return rt_vivos_n;
}

/* ---- lista dos vivos ---- */

static void rt_rastreio_encadear(ForjaCabecalho *c)
{
    c->prox = NULL;
    c->ant = rt_heap_cauda;
    if (rt_heap_cauda != NULL)
        rt_heap_cauda->prox = c;
    else
        rt_heap_cabeca = c;
    rt_heap_cauda = c;
    rt_vivos_n++;
    rt_vivos_bytes += c->bytes;
}

static void rt_rastreio_desencadear(ForjaCabecalho *c)
{
    if (c->ant != NULL)
        c->ant->prox = c->prox;
    else
        rt_heap_cabeca = c->prox;
    if (c->prox != NULL)
        c->prox->ant = c->ant;
    else
        rt_heap_cauda = c->ant;
    rt_vivos_n--;
    rt_vivos_bytes -= c->bytes;
}

/* ---- R0009: vazamento no fim do programa ---- */

typedef struct {
    const char *arquivo;
    int32_t linha;
    int64_t blocos, bytes;
    uint32_t ras[RT_RAS_MAX]; /* a pilha (offsets) do 1o (mais antigo) bloco do ponto */
} RtVazamento;

typedef struct {
    RtVazamento pontos[FORJA_VAZAMENTOS_MAX];
    int n;
    RtVazamento outros;
} RtVazamentos;

/* O runtime nativo nao tem strcmp: igualdade de texto C. */
static bool rt_texto_igual(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static RtVazamento *rt_vazamento_achar(RtVazamentos *v, const ForjaCabecalho *c)
{
    int i;

    for (i = 0; i < v->n; i++) {
        if (v->pontos[i].linha == c->linha && rt_texto_igual(v->pontos[i].arquivo, c->arquivo))
            return &v->pontos[i];
    }
    return NULL;
}

/* Soma o bloco no ponto (arquivo, linha) que ja existe; ponto novo se couber; senao em "(outros)". */
static void rt_vazamento_somar(RtVazamentos *v, const ForjaCabecalho *c)
{
    RtVazamento *p = rt_vazamento_achar(v, c);

    if (p == NULL && v->n < FORJA_VAZAMENTOS_MAX) {
        p = &v->pontos[v->n++];
        p->arquivo = c->arquivo;
        p->linha = c->linha;
        for (int k = 0; k < RT_RAS_MAX; k++)
            p->ras[k] = c->ras_off[k];
    }
    if (p == NULL)
        p = &v->outros;
    p->blocos++;
    p->bytes += c->bytes;
}

/* A causa do R0009: "vazamento: <blocos> blocos (<bytes> bytes) <sufixo>" (o snprintf do forja_rt.h). */
static const char *rt_vazamento_causa(RtCausa *causa, const RtVazamento *v, const char *sufixo)
{
    rt_causa_iniciar(causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(causa, "vazamento: ");
    rt_causa_i64(causa, v->blocos);
    rt_causa_texto(causa, " blocos (");
    rt_causa_i64(causa, v->bytes);
    rt_causa_texto(causa, " bytes) ");
    rt_causa_texto(causa, sufixo);
    return rt_causa_fim(causa);
}

/* Uma linha R0009 no formato do forja_falha (forja_rt.h:1033). */
static void rt_vazamento_linha(const RtVazamento *v, const char *sufixo)
{
    RtErroLote lote;
    RtCausa causa;

    lote.usado = 0;
    rt_erro_texto(&lote, "{\"codigo\": \"R0009\", \"tipo\": \"runtime\", \"arquivo\": \"");
    rt_erro_json(&lote, v->arquivo);
    rt_erro_texto(&lote, "\", \"linha\": ");
    rt_erro_numero(&lote, v->linha);
    rt_erro_texto(&lote, ", \"causa\": \"");
    rt_erro_json(&lote, rt_vazamento_causa(&causa, v, sufixo));
    rt_erro_texto(&lote, "\"");
    rt_ras_json_off(&lote, v->ras);
    rt_erro_texto(&lote, "}\n");
    rt_erro_fim(&lote);
}

/* Arena ou Pool vivo no fim (P70, D-M6-7): uma linha R0009 com "tipo" arena|pool (no lugar de runtime), os bytes retidos e a pilha da criacao. arquivo:linha = quem chamou
 * arena_create/pool_create (o 2o quadro; o 1o e o wrapper de std:mem). Chamada por rt_aloc.c com a trava do registro de alocadores. */
static void rt_alocador_linha(const char *tipo, const char *destruir, int64_t bytes, const uintptr_t *ras)
{
    RtErroLote lote;
    RtCausa causa;
    const char *arquivo = "std:mem";
    int32_t linha = 0;

    rt_ras_chamador(ras, &arquivo, &linha);
    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "vazamento: ");
    rt_causa_texto(&causa, tipo);
    rt_causa_texto(&causa, " viva no fim do programa (");
    rt_causa_i64(&causa, bytes);
    rt_causa_texto(&causa, " bytes em blocos): falta ");
    rt_causa_texto(&causa, destruir);
    lote.usado = 0;
    rt_erro_texto(&lote, "{\"codigo\": \"R0009\", \"tipo\": \"");
    rt_erro_texto(&lote, tipo);
    rt_erro_texto(&lote, "\", \"arquivo\": \"");
    rt_erro_json(&lote, arquivo);
    rt_erro_texto(&lote, "\", \"linha\": ");
    rt_erro_numero(&lote, linha);
    rt_erro_texto(&lote, ", \"causa\": \"");
    rt_erro_json(&lote, rt_causa_fim(&causa));
    rt_erro_texto(&lote, "\"");
    rt_ras_json(&lote, ras);
    rt_erro_texto(&lote, "}\n");
    rt_erro_fim(&lote);
}

/* Definido em rt_aloc.c (vem depois): relata as Arenas e os Pools nao destruidos; devolve quantos eram. */
static int rt_alocs_vivos_relatar(void);
/* Definido em rt_handles.c (incluido no fim deste arquivo): relata os handles de track_handle sem untrack; devolve quantos eram. */
static int rt_handles_vivos_relatar(void);

/* Ligados por rt_libmem.c na 1a alocacao de lib estatica contada (M6d P92d): sem lib NULL e o programa nao puxa o relatorio. relatar devolve quantas linhas R0009 lib saiu; libs_json
 * acrescenta ", "libs": [...]" a linha do report_since. */
static int (*rt_libmem_relatar_fn)(void); /* compartilhado: gravado 1x na 1a alocacao de lib, antes de qualquer relatorio */
static void (*rt_libmem_libs_fn)(RtErroLote *lote); /* compartilhado: idem rt_libmem_relatar_fn */

/* Uma linha por ponto de alocacao (sufixo_ponto) e a de "(outros)" se passou do limite. */
static void rt_vazamentos_listar(RtVazamentos *v, const char *sufixo_ponto)
{
    int i;

    for (i = 0; i < v->n; i++)
        rt_vazamento_linha(&v->pontos[i], sufixo_ponto);
    if (v->outros.blocos > 0) {
        v->outros.arquivo = "(outros)";
        rt_vazamento_linha(&v->outros, "em outros pontos de alocacao");
    }
}

/* Handler de saida (so DEV): relata os blocos de heap que seguem vivos e sai com 101; falha ja relatada = silencio (forja_rt.h:1044). */
static void rt_vazamentos_relatar(void)
{
    RtVazamentos v;
    const ForjaCabecalho *c;
    int alocs;
    int (*relatar_lib)(void);
    bool tomou;

    if (rt_sem_relatorio)
        return;
    memset(&v, 0, sizeof v);
    tomou = sys_trava_pegar(&rt_rastreio_trava);
    for (c = rt_heap_cabeca; c != NULL; c = c->prox)
        rt_vazamento_somar(&v, c);
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    rt_vazamentos_listar(&v, "alocados aqui seguem vivos no fim do programa");
    alocs = rt_alocs_vivos_relatar();
    alocs += rt_handles_vivos_relatar();
    relatar_lib = __atomic_load_n(&rt_libmem_relatar_fn, __ATOMIC_ACQUIRE); /* par do store-release em rt_libmem.c (P93h) */
    if (relatar_lib != NULL)
        alocs += relatar_lib();
    if (v.n == 0 && alocs == 0)
        return;
    rt_saida_descarregar();
    sys_sair(RT_CODIGO_SAIDA_FALHA);
}

/* DETACH de DLL por FreeLibrary (M3 P22a, D29 da v1.8): relata como o handler de saida, mas o processo do hospedeiro segue vivo (o FORJA_BIBLIOTECA do forja_rt.h:1235).
 * Corpo proprio, sem helper em comum com o handler: um helper compartilhado muda a ordem das strings do .rodata e, com ela, o sha256 de todo exe em dev. */
static void rt_vazamentos_relatar_dll(void)
{
    RtVazamentos v;
    const ForjaCabecalho *c;

    bool tomou;

    if (rt_sem_relatorio)
        return;
    tomou = sys_trava_pegar(&rt_rastreio_trava);
    if (rt_heap_cabeca == NULL) {
        sys_trava_soltar(&rt_rastreio_trava, tomou);
        return;
    }
    memset(&v, 0, sizeof v);
    for (c = rt_heap_cabeca; c != NULL; c = c->prox)
        rt_vazamento_somar(&v, c);
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    rt_vazamentos_listar(&v, "alocados aqui seguem vivos no fim do programa");
}

/* ---- snapshot e report_since de std:mem (P71a, D-M6-8) ---- */

#define RT_SNAP_GRUPOS_MAX 128 /* grupos (local + pilha) distintos acompanhados num relatorio; o que passa disso soma em "outros" */
#define RT_SNAP_TOPO 20        /* grupos impressos, os de mais bytes primeiro */

typedef struct {
    RtVazamento pontos[RT_SNAP_GRUPOS_MAX];
    int n;
    RtVazamento outros;
} RtSnapGrupos;

/* Serie do proximo bloco de heap a nascer: o snapshot guarda esse valor e os blocos nascidos depois tem serie >= ele. */
uint32_t forja_memsnap_serie(void)
{
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);
    uint32_t serie = rt_serie_prox;

    sys_trava_soltar(&rt_rastreio_trava, tomou);
    return serie;
}

/* Blocos de heap vivos agora. */
int64_t forja_memsnap_vivos(void)
{
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);
    int64_t n = rt_vivos_n;

    sys_trava_soltar(&rt_rastreio_trava, tomou);
    return n;
}

/* Bytes dos blocos de heap vivos agora (sem o cabecalho do rastreador). */
int64_t forja_memsnap_bytes(void)
{
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);
    int64_t n = rt_vivos_bytes;

    sys_trava_soltar(&rt_rastreio_trava, tomou);
    return n;
}

/* O grupo (arquivo, linha, pilha) do bloco c; NULL se ainda nao existe. */
static RtVazamento *rt_snap_achar(RtSnapGrupos *v, const ForjaCabecalho *c)
{
    for (int i = 0; i < v->n; i++) {
        RtVazamento *p = &v->pontos[i];
        bool igual = p->linha == c->linha && rt_texto_igual(p->arquivo, c->arquivo);

        for (int k = 0; igual && k < RT_RAS_MAX; k++)
            igual = p->ras[k] == c->ras_off[k];
        if (igual)
            return p;
    }
    return NULL;
}

/* Soma o bloco no grupo dele; grupo novo se couber; senao em "outros". */
static void rt_snap_somar(RtSnapGrupos *v, const ForjaCabecalho *c)
{
    RtVazamento *p = rt_snap_achar(v, c);

    if (p == NULL && v->n < RT_SNAP_GRUPOS_MAX) {
        p = &v->pontos[v->n++];
        p->arquivo = c->arquivo;
        p->linha = c->linha;
        for (int k = 0; k < RT_RAS_MAX; k++)
            p->ras[k] = c->ras_off[k];
    }
    if (p == NULL)
        p = &v->outros;
    p->blocos++;
    p->bytes += c->bytes;
}

/* Poe nas primeiras posicoes os 'topo' grupos de mais bytes (selecao parcial); devolve quantos. */
static int rt_snap_ordenar(RtSnapGrupos *v, int topo)
{
    int limite = v->n < topo ? v->n : topo;

    for (int i = 0; i < limite; i++) {
        int maior = i;

        for (int j = i + 1; j < v->n; j++) {
            if (v->pontos[j].bytes > v->pontos[maior].bytes)
                maior = j;
        }
        if (maior != i) {
            RtVazamento troca = v->pontos[i];

            v->pontos[i] = v->pontos[maior];
            v->pontos[maior] = troca;
        }
    }
    return limite;
}

static void rt_snap_grupo_json(RtErroLote *lote, const RtVazamento *g, bool primeiro)
{
    rt_erro_texto(lote, primeiro ? "{\"arquivo\": \"" : ", {\"arquivo\": \"");
    rt_erro_json(lote, g->arquivo);
    rt_erro_texto(lote, "\", \"linha\": ");
    rt_erro_numero(lote, g->linha);
    rt_erro_texto(lote, ", \"blocos\": ");
    rt_erro_numero(lote, g->blocos);
    rt_erro_texto(lote, ", \"bytes\": ");
    rt_erro_numero(lote, g->bytes);
    rt_ras_json_off(lote, g->ras);
    rt_erro_texto(lote, "}");
}

/* report_since: UMA linha JSON no stderr com os blocos de heap nascidos depois do snapshot (serie, vivos e bytes de forja_memsnap_*) e ainda vivos, agrupados por local + pilha,
 * os RT_SNAP_TOPO de mais bytes; o resto soma em "outros". Nao sai do programa. */
void forja_memsnap_relatar(uint32_t serie, int64_t vivos_antes, int64_t bytes_antes)
{
    RtSnapGrupos *v = (RtSnapGrupos *)sys_heap_alocar((int64_t)sizeof *v); /* no heap do sistema (sem cabecalho): na pilha estoura o limite de quadro do runtime */
    RtErroLote lote;
    const ForjaCabecalho *c;
    int64_t vivos_agora, bytes_agora, novos = 0, novos_bytes = 0;
    int topo;
    bool tomou;

    if (v == NULL)
        forja_falha_alocacao("sem memoria para o relatorio de report_since em bytes", (int64_t)sizeof *v, "std:mem", 0);
    memset(v, 0, sizeof *v);
    tomou = sys_trava_pegar(&rt_rastreio_trava);
    for (c = rt_heap_cabeca; c != NULL; c = c->prox) {
        if ((int32_t)(c->alocador - serie) < 0)
            continue;
        rt_snap_somar(v, c);
        novos++;
        novos_bytes += c->bytes;
    }
    vivos_agora = rt_vivos_n;
    bytes_agora = rt_vivos_bytes;
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    topo = rt_snap_ordenar(v, RT_SNAP_TOPO);
    for (int i = topo; i < v->n; i++) {
        v->outros.blocos += v->pontos[i].blocos;
        v->outros.bytes += v->pontos[i].bytes;
    }
    lote.usado = 0;
    rt_erro_texto(&lote, "{\"tipo\": \"mem_report\", \"vivos_antes\": ");
    rt_erro_numero(&lote, vivos_antes);
    rt_erro_texto(&lote, ", \"bytes_antes\": ");
    rt_erro_numero(&lote, bytes_antes);
    rt_erro_texto(&lote, ", \"vivos_agora\": ");
    rt_erro_numero(&lote, vivos_agora);
    rt_erro_texto(&lote, ", \"bytes_agora\": ");
    rt_erro_numero(&lote, bytes_agora);
    rt_erro_texto(&lote, ", \"novos_blocos\": ");
    rt_erro_numero(&lote, novos);
    rt_erro_texto(&lote, ", \"novos_bytes\": ");
    rt_erro_numero(&lote, novos_bytes);
    rt_erro_texto(&lote, ", \"grupos\": [");
    for (int i = 0; i < topo; i++)
        rt_snap_grupo_json(&lote, &v->pontos[i], i == 0);
    rt_erro_texto(&lote, "], \"outros\": {\"blocos\": ");
    rt_erro_numero(&lote, v->outros.blocos);
    rt_erro_texto(&lote, ", \"bytes\": ");
    rt_erro_numero(&lote, v->outros.bytes);
    rt_erro_texto(&lote, "}");
    void (*libs_lib)(RtErroLote *) = __atomic_load_n(&rt_libmem_libs_fn, __ATOMIC_ACQUIRE); /* par do store-release em rt_libmem.c (P93h) */
    if (libs_lib != NULL)
        libs_lib(&lote);
    rt_erro_texto(&lote, "}\n");
    rt_erro_fim(&lote);
    sys_heap_liberar(v);
}

/* ---- ciclo de vida do bloco ---- */

/* Registra o relatorio de saida na 1a vez que um bloco de heap, uma Arena ou um Pool nasce. */
static void rt_rastreio_registrar_saida(void)
{
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);
    bool registrar = !rt_atexit_ok;

    rt_atexit_ok = true;
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    if (registrar)
        forja_rt_ao_sair(rt_vazamentos_relatar);
}

/* Marca o bloco bruto (c, ja com 80 B de cabecalho) como vivo, guarda a pilha de quem alocou, encadeia e registra o relatorio de saida na 1a vez. Devolve o endereco do programa.
 * A pilha e da propria thread (nenhum estado compartilhado): fica fora da trava. */
static void *rt_rastreio_criar(ForjaCabecalho *c, int64_t bytes, const char *arquivo, int linha)
{
    bool tomou;

    c->marca = FORJA_MARCA_VIVA;
    c->bytes = bytes;
    c->arquivo = arquivo != NULL ? arquivo : "";
    c->linha = (int32_t)linha;
    rt_ras_capturar_off(c->ras_off);
    tomou = sys_trava_pegar(&rt_rastreio_trava);
    c->alocador = rt_serie_prox++; /* bloco VIVO de heap: o campo nao e lido por ninguem; guarda a serie de nascimento (report_since) */
    rt_rastreio_encadear(c);
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    rt_rastreio_registrar_saida();
    return c + 1;
}

static const char *rt_rastreio_motivo(uint64_t marca)
{
    if (marca == FORJA_MARCA_ARENA)
        return ": objeto de Arena: arena_reset ou arena_destroy liberam tudo";
    if (marca == FORJA_MARCA_POOL)
        return ": objeto de Pool: use delete(p, em = pool)";
    return ": o ponteiro nao veio de new(em = heap) ou ja passou por delete";
}

/* Cabecalho do bloco de p; se a marca nao e a de vivo, R0008 com o motivo (forja_rt.h:1125). */
static ForjaCabecalho *rt_rastreio_vivo(void *p, const char *oque, const char *arquivo, int linha)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;
    RtCausa causa;

    if (c->marca == FORJA_MARCA_VIVA)
        return c;
    if (c->marca == FORJA_MARCA_MORTA && c->alocador != RT_MORTE_NENHUMA) {
        if (!rt_em_teste)
            rt_falha_reservar(); /* P93k: o gate de forja_falha ANTES de gravar rt_morto: duas threads nao pisam no relato uma da outra (a mesma thread passa de novo la dentro) */
        rt_morto_pendente(c->alocador == RT_MORTE_ARENA_DESTROY ? "arena" : (c->alocador == RT_MORTE_APPEND_MOVEU ? "lista" : "heap"), c);
    }
    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, oque);
    rt_causa_texto(&causa, rt_rastreio_motivo(c->marca));
    forja_falha(FORJA_R0008, rt_causa_fim(&causa), arquivo, linha);
}

/* Tira da lista, marca como morto, guarda a pilha do delete em ras_off[4..7] (a da alocacao em [0..3] fica) e a causa da morte (RT_MORTE_*) em `alocador`, e enche o conteudo com 0xDD.
 * O bloco entra na quarentena (D-M6-15a): so volta ao sistema quando a fila passa de RT_QUARENTENA_BYTES, entao a marca MORTA segue no cabecalho e um 2o delete (ou append) do mesmo
 * ponteiro e R0008 mesmo depois de muitas alocacoes. */
static void rt_rastreio_baixa(ForjaCabecalho *c, uint32_t causa_morte)
{
    ForjaCabecalho *vitima = NULL;
    ForjaCabecalho *proximo;
    bool tomou;

    rt_ras_capturar_off(c->ras_off + RT_RAS_MAX); /* pilha do delete: o bloco e deste fio ate a marca MORTA aparecer; outro delete simultaneo do mesmo ponteiro e erro do programa */
    tomou = sys_trava_pegar(&rt_rastreio_trava);
    rt_rastreio_desencadear(c);
    c->alocador = causa_morte; /* depois do desencadear e sob a trava: report_since le a serie de nascimento neste campo, so dos vivos da lista */
    *(volatile uint64_t *)&c->marca = FORJA_MARCA_MORTA; /* volatile: sem isso o gcc -O2 tira a escrita morta antes de a quarentena guardar o bloco */
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    if (rt_guarda && rt_guarda_dono(c)) {
        rt_guarda_enterrar(c, causa_morte); /* cemiterio + paginas fora do commit: sem 0xDD e sem quarentena (o endereco nunca volta) */
        return;
    }
    memset(c + 1, RT_BYTE_BLOCO_MORTO, (size_t)c->bytes);
    tomou = sys_trava_pegar(&rt_rastreio_trava);
    c->prox = NULL;
    if (rt_quar_cauda != NULL)
        rt_quar_cauda->prox = c;
    else
        rt_quar_cabeca = c;
    rt_quar_cauda = c;
    rt_quar_bytes += c->bytes + RT_ALOC_EXTRA;
    while (rt_quar_bytes > RT_QUARENTENA_BYTES && rt_quar_cabeca != NULL) {
        ForjaCabecalho *velho = rt_quar_cabeca;

        rt_quar_cabeca = velho->prox;
        if (rt_quar_cabeca == NULL)
            rt_quar_cauda = NULL;
        rt_quar_bytes -= velho->bytes + RT_ALOC_EXTRA;
        velho->prox = vitima;
        vitima = velho;
    }
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    for (; vitima != NULL; vitima = proximo) {
        proximo = vitima->prox;
        sys_heap_liberar(vitima);
    }
}

/* ---- vazamento por teste (L16b): declarado em rt_teste.c, que vem antes nesta unidade ---- */

/* O mais antigo dos blocos nascidos depois da serie (P93k: pela serie de nascimento, como forja_memsnap_relatar; os alocados no teste ficam no fim da lista). */
static ForjaCabecalho *rt_teste_primeiro_novo(uint32_t serie_antes)
{
    ForjaCabecalho *c = rt_heap_cabeca;

    while (c != NULL && (int32_t)(c->alocador - serie_antes) < 0)
        c = c->prox;
    return c;
}

/* Tira da lista (sem liberar) os blocos de 'c' em diante: o rastreador os esquece e o R0009 do fim do programa nao os repete. */
static void rt_teste_esquecer(ForjaCabecalho *c)
{
    ForjaCabecalho *prox;

    for (; c != NULL; c = prox) {
        prox = c->prox;
        rt_rastreio_desencadear(c);
    }
}

/* Fim de um teste que passou: os blocos vivos nascidos depois da serie_antes (serie de nascimento, nao contagem: um teste que libera bloco de fora e vaza outro em igual numero tambem e visto)
 * foram alocados nele e vazaram. Uma linha R0009 por ponto de alocacao; a 1a vira a falha do --json. Devolve true se vazou. */
static bool rt_vazamento_do_teste(uint32_t serie_antes)
{
    ForjaCabecalho *primeiro = rt_teste_primeiro_novo(serie_antes);
    const ForjaCabecalho *c;
    RtVazamentos v;
    RtCausa causa;

    if (primeiro == NULL)
        return false;
    memset(&v, 0, sizeof v);
    for (c = primeiro; c != NULL; c = c->prox)
        rt_vazamento_somar(&v, c);
    rt_teste_anotar(FORJA_R0009, rt_vazamento_causa(&causa, &v.pontos[0], "alocados aqui seguem vivos no fim do teste"), v.pontos[0].arquivo, v.pontos[0].linha);
    rt_vazamentos_listar(&v, "alocados aqui seguem vivos no fim do teste");
    rt_teste_esquecer(primeiro);
    return true;
}

/* ---- Arena e Pool: cabecalho dentro do bloco do alocador (p = endereco do programa, 48 B antes dele) ---- */

static void rt_aloc_cabecalho(void *p, uint64_t marca, uint32_t h)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;

    c->marca = marca;
    c->alocador = h;
    c->ant = NULL;
    c->prox = NULL;
}

static void rt_aloc_origem(void *p, int64_t bytes, const char *arquivo, int linha)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;

    c->bytes = bytes;
    c->arquivo = arquivo != NULL ? arquivo : "";
    c->linha = (int32_t)linha;
}

/* delete(p, em = pool): o objeto tem de ser deste Pool (R0008); depois enche com 0xDD e marca como morto (forja_rt.h:1265). */
static void rt_pool_conferir(uint32_t h, void *p, const char *arquivo, int linha)
{
    ForjaCabecalho *c = (ForjaCabecalho *)p - 1;

    if (c->marca != FORJA_MARCA_POOL || c->alocador != h)
        forja_falha(FORJA_R0008, "delete(p, em = pool): o objeto nao veio deste Pool", arquivo, linha);
    memset(p, RT_BYTE_BLOCO_MORTO, (size_t)c->bytes);
    c->alocador = RT_MORTE_NENHUMA; /* sem causa de morte de heap: um delete(p) desse objeto relata sem as 3 pilhas */
    *(volatile uint64_t *)&c->marca = FORJA_MARCA_MORTA;
}

/* ---- handles genericos de lib C (P71b) ---- */
#include "rt_handles.c"
/* ---- memoria por lib estatica: tabela lateral e relatorio (M6d P92d), involucros (P92c) ---- */
#include "rt_libmem.c"
#include "rt_libmem_slots.c"
