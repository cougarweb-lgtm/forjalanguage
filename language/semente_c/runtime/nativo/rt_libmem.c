/*
 * rt_libmem.c - contabilidade de memoria por biblioteca estatica em DEV do runtime nativo da Forja (M6d P92d, D-M6d-5/6/8): a tabela lateral ponteiro -> {tamanho, slot, pilha}, o registro e a baixa
 * chamados pelos involucros (rt_libmem_slots.c), o relatorio R0009 "tipo":"lib" no fim do programa e o "libs" do report_since.
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - M6d P93h (dividas P2 do revisor): realloc que falha com n != 0 reinsere sem reabrir a lib (rt_lm_guardar ganha `reabre`); ponteiros de relatorio e de encerrou gravados com
 *           store-release e lidos com load-acquire; sem memoria para os grupos = 1 linha R0009 de resumo por lib (blocos e bytes, sem pilha) em vez de relatorio vazio.
 *   0.2.0 - M6d P92e (D-M6d-7/8): forja_libmem_encerrou (existe em toda variante dev; marca o slot cujos fins `ends` (off_fins da FLM1) contem o simbolo; a proxima alocacao do slot desmarca) e a
 *           classificacao final: lib sem `ends` ou com `ends` encerrada = R0009 lib; com `ends` e nao encerrada = "global" (nenhuma linha; "global": true no report_since).
 *   0.1.0 - M6d P92d: primeira versao. Hash de enderecamento aberto (sondagem linear, remocao por deslocamento para tras, sem lapide), 1024 entradas, dobra a 70%. O ARMAZENAMENTO vem do
 *           calloc/free REAIS (o dlmalloc do runtime Linux so e seguro com rt_multithread e a thread da lib nao o liga). Trava PROPRIA rt_libmem_trava, tomada SEMPRE (sys_trava_pegar_srw: a thread
 *           da lib e invisivel ao rt_multithread; zero e valida sem init, o construtor de terceiros pode alocar antes do runtime). A pilha e capturada FORA da trava e so se a thread tem fio
 *           (rt_fio_ou_nulo): thread da lib = pilha zerada, grupo "(thread da lib)". free de ponteiro DESCONHECIDO = silencio (veio do CRT, de outra origem; o free duplo e do M6e). A 1a entrada
 *           registrada liga o handler de saida (rt_rastreio_registrar_saida) e rt_libmem_relatar_fn / rt_libmem_libs_fn de rt_rastreio.c: programa sem lib nao puxa este relatorio.
 *           Neste passo toda lib e "sem ends" (a clausula chega ao runtime no P92e): vivo no fim = R0009 lib com exit 101.
 *
 * ABI: os reais vem da ucrtbase por dllimport (Windows, so variante CRT) ou do musl (Linux, System V); os involucros em rt_libmem_slots.c tem a ABI do C do alvo.
 * Incluido no FIM de rt_rastreio.c (so Linux e Windows CRT), ANTES de rt_libmem_slots.c; nao compila sozinho.
 */
#ifndef RT_RASTREIO_C
#error "rt_libmem.c so existe incluido por rt_rastreio.c"
#endif

/* forja_libmem_encerrou (M6d P92e): existe em toda variante dev (o codigo gerado a chama depois de um foreign `ends`, mesmo de DLL); so faz algo depois que a 1a alocacao de lib
 * estatica ligou rt_libmem_encerrou_fn (sem tabela registrada = nada). */
static void (*rt_libmem_encerrou_fn)(const char *simbolo); /* compartilhado: gravado 1x na 1a alocacao de lib */

void forja_libmem_encerrou(const char *simbolo)
{
    void (*encerrou)(const char *) = __atomic_load_n(&rt_libmem_encerrou_fn, __ATOMIC_ACQUIRE); /* par do store-release em rt_lm_guardar */

    if (encerrou != NULL)
        encerrou(simbolo);
}

#if defined(RT_ALVO_LINUX) || defined(FORJA_RT_CRT)

#ifdef RT_ALVO_LINUX
#define RT_LIBMEM_REAL(tipo) extern RT_ABI_SYSV tipo
#define RT_LIBMEM_REAL_STRDUP strdup
#else
#define RT_LIBMEM_REAL(tipo) __declspec(dllimport) tipo
#define RT_LIBMEM_REAL_STRDUP _strdup
#endif

RT_LIBMEM_REAL(void *) malloc(size_t n);
RT_LIBMEM_REAL(void *) calloc(size_t n, size_t m);
RT_LIBMEM_REAL(void *) realloc(void *p, size_t n);
RT_LIBMEM_REAL(void) free(void *p);
RT_LIBMEM_REAL(char *) RT_LIBMEM_REAL_STRDUP(const char *s);

#define RT_LIBMEM_INICIAL 1024u
#define RT_LIBMEM_GRUPOS_MAX 16        /* linhas por lib; o resto vai numa linha "(outros)" */
#define RT_LIBMEM_GRUPOS_TRABALHO 128  /* grupos distintos acompanhados por lib no relatorio; o que passa disso soma em "outros" */
#define FORJA_LIBMEM_SLOTS_MAX 16u     /* FORJA_LIBMEM_SLOTS do ligador (libmem.h) */

static int rt_libmem_relatar(void);
static void rt_libmem_libs_json(RtErroLote *lote);
static void rt_libmem_encerrou(const char *simbolo);

/* Tabela do ligador (libmem.c, D-M6d-4): u32 "FLM1" | u32 qtd | qtd x {off_nome, off_arquivo, off_fins} | textos; offsets a partir do inicio. */
extern const uint8_t forja_libmem_tabela[];

typedef struct {
    uintptr_t p; /* 0 = entrada livre */
    uint64_t n;
    uintptr_t ras[RT_RAS_MAX];
    uint8_t slot;
} RtLibmemEntrada;

typedef struct {
    uintptr_t ras[RT_RAS_MAX];
    int64_t blocos, bytes;
    const char *arquivo;
    int32_t linha;
} RtLibmemGrupo;

static RtLibmemEntrada *rt_lm_tab;  /* compartilhado: sob rt_libmem_trava */
static uint64_t rt_lm_cap;          /* compartilhado: idem rt_lm_tab (potencia de 2) */
static uint64_t rt_lm_n;            /* compartilhado: idem rt_lm_tab */
static bool rt_lm_registrado;       /* compartilhado: idem rt_lm_tab (o relatorio de saida ja foi ligado) */
static SysTrava rt_libmem_trava;    /* compartilhado: tomada SEMPRE */
static bool rt_lm_encerrada[FORJA_LIBMEM_SLOTS_MAX]; /* compartilhado: idem rt_lm_tab; o fim `ends` da lib ja foi chamado e nada alocou nela depois */

static inline void rt_lm_pegar(void)
{
    sys_trava_pegar_srw(&rt_libmem_trava);
    RT_BARREIRA(); /* o gcc nao pode usar leitura do estado travado feita antes da trava */
}

static inline void rt_lm_soltar(void)
{
    RT_BARREIRA(); /* as escritas do estado travado saem antes da trava soltar */
    sys_trava_soltar_srw(&rt_libmem_trava);
}

static uint64_t rt_lm_indice(uintptr_t p, uint64_t cap)
{
    return (uint64_t)(((uint64_t)(p >> 3) * 0x9E3779B97F4A7C15ull) >> 32) & (cap - 1u);
}

/* Poe a entrada na tabela (ja ha espaco); ponteiro repetido (liberado por fora do nosso alcance) e sobrescrito. */
static void rt_lm_inserir(const RtLibmemEntrada *e)
{
    uint64_t i = rt_lm_indice(e->p, rt_lm_cap);

    while (rt_lm_tab[i].p != 0 && rt_lm_tab[i].p != e->p)
        i = (i + 1u) & (rt_lm_cap - 1u);
    if (rt_lm_tab[i].p == 0)
        rt_lm_n++;
    rt_lm_tab[i] = *e;
}

/* Dobra (ou cria) a tabela quando passa de 70%. false = sem memoria (o ponteiro fica sem conta). Sob a trava. */
static bool rt_lm_crescer(void)
{
    RtLibmemEntrada *antiga = rt_lm_tab;
    uint64_t cap_antiga = rt_lm_cap;
    uint64_t cap = cap_antiga == 0 ? RT_LIBMEM_INICIAL : cap_antiga * 2u;
    RtLibmemEntrada *nova;

    if ((rt_lm_n + 1u) * 10u <= cap_antiga * 7u)
        return true;
    nova = (RtLibmemEntrada *)malloc((size_t)cap * sizeof *nova); /* malloc+memset, nao calloc: no PE o import do calloc so existe se a lib o cita */
    if (nova == NULL)
        return false;
    memset(nova, 0, (size_t)cap * sizeof *nova);
    rt_lm_tab = nova;
    rt_lm_cap = cap;
    rt_lm_n = 0;
    for (uint64_t i = 0; i < cap_antiga; i++) {
        if (antiga[i].p != 0)
            rt_lm_inserir(&antiga[i]);
    }
    if (antiga != NULL)
        free(antiga);
    return true;
}

/* Tira p da tabela (deslocando para tras os vizinhos da mesma sondagem) e devolve a entrada; false = desconhecido. Sob a trava. */
static bool rt_lm_tirar(uintptr_t p, RtLibmemEntrada *saida)
{
    uint64_t mascara = rt_lm_cap - 1u;
    uint64_t i;
    uint64_t j;

    if (rt_lm_cap == 0)
        return false;
    i = rt_lm_indice(p, rt_lm_cap);
    while (rt_lm_tab[i].p != p) {
        if (rt_lm_tab[i].p == 0)
            return false;
        i = (i + 1u) & mascara;
    }
    *saida = rt_lm_tab[i];
    for (j = (i + 1u) & mascara; rt_lm_tab[j].p != 0; j = (j + 1u) & mascara) {
        uint64_t base = rt_lm_indice(rt_lm_tab[j].p, rt_lm_cap);
        bool fica = i <= j ? (i < base && base <= j) : (i < base || base <= j); /* base entre i (exclusive) e j (inclusive): o vizinho nao pode ocupar i */

        if (fica)
            continue;
        rt_lm_tab[i] = rt_lm_tab[j];
        i = j;
    }
    rt_lm_tab[i].p = 0;
    rt_lm_n--;
    return true;
}

/* A pilha .fj de quem chamou; thread sem fio (criada pela lib, ou antes da forja_entrada) = zeros: rt_fio() ali mataria o processo. Fora da trava. */
static void rt_lm_pilha(uintptr_t *ras)
{
    if (rt_fio_ou_nulo() == NULL) {
        for (int k = 0; k < RT_RAS_MAX; k++)
            ras[k] = 0;
        return;
    }
    rt_ras_capturar_lib(ras);
}

/* Poe a entrada na tabela sob a trava; a 1a vez liga o relatorio de saida. `reabre` = alocacao nova na lib (desmarca o encerramento);
 * o realloc que falha so devolve o bloco velho e NAO reabre (a lib encerrada segue encerrada). */
static void rt_lm_guardar(const RtLibmemEntrada *e, bool reabre)
{
    bool ligar;
    bool cabe;

    rt_lm_pegar();
    cabe = rt_lm_crescer();
    if (cabe) {
        rt_lm_inserir(e);
        if (reabre && e->slot < FORJA_LIBMEM_SLOTS_MAX)
            rt_lm_encerrada[e->slot] = false; /* a proxima alocacao reabre a lib (sqlite3_initialize depois do shutdown) */
    }
    ligar = cabe && !rt_lm_registrado;
    if (ligar)
        rt_lm_registrado = true;
    rt_lm_soltar();
    if (!cabe)
        forja_falha_alocacao("sem memoria para a tabela de memoria por lib em bytes", (int64_t)sizeof *e, "std:mem", 0);
    if (ligar) {
        /* publicados com store-release: quem le (rt_rastreio.c, forja_libmem_encerrou) le com load-acquire, sem trava */
        __atomic_store_n(&rt_libmem_relatar_fn, rt_libmem_relatar, __ATOMIC_RELEASE);
        __atomic_store_n(&rt_libmem_libs_fn, rt_libmem_libs_json, __ATOMIC_RELEASE);
        __atomic_store_n(&rt_libmem_encerrou_fn, rt_libmem_encerrou, __ATOMIC_RELEASE);
        rt_rastreio_registrar_saida();
    }
}

static void rt_libmem_registrar(void *p, size_t n, uint8_t slot)
{
    RtLibmemEntrada e;

    if (p == NULL)
        return;
    e.p = (uintptr_t)p;
    e.n = (uint64_t)n;
    e.slot = slot;
    rt_lm_pilha(e.ras);
    rt_lm_guardar(&e, true);
}

static bool rt_libmem_baixar(void *p, RtLibmemEntrada *e)
{
    bool achou;

    if (p == NULL)
        return false;
    rt_lm_pegar();
    achou = rt_lm_tirar((uintptr_t)p, e);
    rt_lm_soltar();
    return achou;
}

static void *rt_libmem_malloc(uint8_t slot, size_t n)
{
    void *p = malloc(n);

    rt_libmem_registrar(p, n, slot);
    return p;
}

static void *rt_libmem_calloc(uint8_t slot, size_t n, size_t m)
{
    void *p = calloc(n, m);

    rt_libmem_registrar(p, n * m, slot);
    return p;
}

static char *rt_libmem_strdup(uint8_t slot, const char *s)
{
    char *p = RT_LIBMEM_REAL_STRDUP(s);
    size_t n = 0;

    while (p != NULL && p[n] != '\0')
        n++;
    rt_libmem_registrar(p, n + 1u, slot);
    return p;
}

static void rt_libmem_free(void *p)
{
    RtLibmemEntrada e;

    (void)rt_libmem_baixar(p, &e); /* desconhecido = silencio (CRT, outra origem, free duplo: M6e) */
    free(p);
}

/* realloc: p conhecido sai da tabela ANTES do real (o endereco velho pode ser reaproveitado por outra thread); o novo herda o slot e a pilha ORIGINAL; falha do real devolve o velho. */
static void *rt_libmem_realloc(uint8_t slot, void *p, size_t n)
{
    RtLibmemEntrada e;
    void *novo;

    if (p == NULL)
        return rt_libmem_malloc(slot, n);
    if (!rt_libmem_baixar(p, &e))
        return realloc(p, n);
    novo = realloc(p, n);
    if (novo == NULL && n != 0) {
        rt_lm_guardar(&e, false); /* o bloco velho segue vivo: nao e alocacao nova, a lib encerrada continua encerrada */
        return NULL;
    }
    if (novo != NULL) {
        e.p = (uintptr_t)novo;
        e.n = (uint64_t)n;
        rt_lm_guardar(&e, true);
    }
    return novo;
}

/* ---- relatorio ---- */

static const char *rt_lm_nome(uint32_t slot, uint32_t campo)
{
    const uint8_t *t = forja_libmem_tabela;
    const uint32_t *entrada = (const uint32_t *)(t + 8u + 12u * slot);

    return (const char *)t + entrada[campo];
}

static uint32_t rt_lm_qtd_slots(void)
{
    return *(const uint32_t *)(forja_libmem_tabela + 4);
}

/* Os fins `ends` do slot (D-M6d-4): "a\0b\0\0" a partir de off_fins; NULL = a lib nao declara `ends`. */
static size_t rt_lm_tamanho_texto(const char *texto)
{
    size_t n = 0;

    while (texto[n] != '\0')
        n++;
    return n;
}

static const char *rt_lm_fins(uint32_t slot)
{
    uint32_t off = ((const uint32_t *)(forja_libmem_tabela + 8u + 12u * slot))[2];

    return off == 0 ? NULL : (const char *)forja_libmem_tabela + off;
}

/* D-M6d-8: lib COM `ends` e nao encerrada = "global" (a sobra e da lib inteira, nao e vazamento); o resto vaza. */
static bool rt_lm_global(uint32_t slot)
{
    bool encerrada;

    if (rt_lm_fins(slot) == NULL)
        return false;
    rt_lm_pegar();
    encerrada = slot < FORJA_LIBMEM_SLOTS_MAX && rt_lm_encerrada[slot];
    rt_lm_soltar();
    return !encerrada;
}

/* forja_libmem_encerrou: marca encerrada o slot cujos fins contem o simbolo (a proxima alocacao do slot desmarca, em rt_lm_guardar). */
static void rt_libmem_encerrou(const char *simbolo)
{
    uint32_t qtd = rt_lm_qtd_slots();

    for (uint32_t slot = 0; slot < qtd && slot < FORJA_LIBMEM_SLOTS_MAX; slot++) {
        const char *fim = rt_lm_fins(slot);

        for (; fim != NULL && *fim != '\0'; fim += rt_lm_tamanho_texto(fim) + 1u) {
            if (!rt_texto_igual(fim, simbolo))
                continue;
            rt_lm_pegar();
            rt_lm_encerrada[slot] = true;
            rt_lm_soltar();
            return;
        }
    }
}

/* Soma a entrada no grupo (pilha igual) da lib; grupo novo se couber; senao em "outros". */
static void rt_lm_somar(RtLibmemGrupo *grupos, int *usados, RtLibmemGrupo *outros, const RtLibmemEntrada *e)
{
    RtLibmemGrupo *g = NULL;

    for (int i = 0; i < *usados && g == NULL; i++) {
        bool igual = true;

        for (int k = 0; igual && k < RT_RAS_MAX; k++)
            igual = grupos[i].ras[k] == e->ras[k];
        if (igual)
            g = &grupos[i];
    }
    if (g == NULL && *usados < RT_LIBMEM_GRUPOS_TRABALHO) {
        g = &grupos[(*usados)++];
        for (int k = 0; k < RT_RAS_MAX; k++)
            g->ras[k] = e->ras[k];
    }
    if (g == NULL)
        g = outros;
    g->blocos++;
    g->bytes += (int64_t)e->n;
}

/* Resolve arquivo:linha de cada grupo (o 1o quadro .fj) para ordenar e imprimir. */
static void rt_lm_resolver(RtLibmemGrupo *g)
{
    uintptr_t primeiro[RT_RAS_MAX] = {g->ras[0]};

    g->arquivo = g->ras[0] == 0 ? "(thread da lib)" : "(lib)";
    g->linha = 0;
    rt_ras_chamador(primeiro, &g->arquivo, &g->linha);
}

static bool rt_lm_antes(const RtLibmemGrupo *a, const RtLibmemGrupo *b)
{
    int d;

    if (a->bytes != b->bytes)
        return a->bytes > b->bytes;
    d = 0;
    for (const char *x = a->arquivo, *y = b->arquivo; d == 0; x++, y++) {
        d = (int)(unsigned char)*x - (int)(unsigned char)*y;
        if (*x == '\0' || *y == '\0')
            break;
    }
    if (d != 0)
        return d < 0;
    if (a->linha != b->linha)
        return a->linha < b->linha;
    for (int k = 0; k < RT_RAS_MAX; k++) {
        if (a->ras[k] != b->ras[k])
            return a->ras[k] < b->ras[k];
    }
    return false;
}

/* Ordenacao por insercao: bytes desc, depois arquivo e linha (deterministico). */
static void rt_lm_ordenar(RtLibmemGrupo *v, int n)
{
    for (int i = 1; i < n; i++) {
        RtLibmemGrupo x = v[i];
        int j = i - 1;

        for (; j >= 0 && rt_lm_antes(&x, &v[j]); j--)
            v[j + 1] = v[j];
        v[j + 1] = x;
    }
}

/* Uma linha R0009 de lib: grupo (uma chamada .fj), "(outros)" com o resto, ou (com_pilha false) o resumo sem memoria para os grupos (sem pilha). */
static void rt_lm_linha(uint32_t slot, const RtLibmemGrupo *g, const char *sufixo, bool com_pilha)
{
    RtErroLote lote;
    RtCausa causa;

    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "vazamento: ");
    rt_causa_i64(&causa, g->blocos);
    rt_causa_texto(&causa, " blocos (");
    rt_causa_i64(&causa, g->bytes);
    rt_causa_texto(&causa, " bytes) da lib ");
    rt_causa_texto(&causa, rt_lm_nome(slot, 0));
    rt_causa_texto(&causa, sufixo);
    lote.usado = 0;
    rt_erro_texto(&lote, "{\"codigo\": \"R0009\", \"tipo\": \"lib\", \"lib\": \"");
    rt_erro_json(&lote, rt_lm_nome(slot, 0));
    rt_erro_texto(&lote, "\", \"arquivo_lib\": \"");
    rt_erro_json(&lote, rt_lm_nome(slot, 1));
    rt_erro_texto(&lote, "\", \"blocos\": ");
    rt_erro_numero(&lote, g->blocos);
    rt_erro_texto(&lote, ", \"bytes\": ");
    rt_erro_numero(&lote, g->bytes);
    rt_erro_texto(&lote, ", \"arquivo\": \"");
    rt_erro_json(&lote, g->arquivo);
    rt_erro_texto(&lote, "\", \"linha\": ");
    rt_erro_numero(&lote, g->linha);
    rt_erro_texto(&lote, ", \"causa\": \"");
    rt_erro_json(&lote, rt_causa_fim(&causa));
    rt_erro_texto(&lote, "\"");
    if (com_pilha)
        rt_ras_json(&lote, g->ras);
    rt_erro_texto(&lote, "}\n");
    rt_erro_fim(&lote);
}

/* Sem memoria para os grupos: conta os blocos vivos do slot (sob a trava). */
static void rt_lm_contar(uint32_t slot, int64_t *blocos, int64_t *bytes)
{
    rt_lm_pegar();
    for (uint64_t i = 0; i < rt_lm_cap; i++) {
        if (rt_lm_tab[i].p != 0 && rt_lm_tab[i].slot == slot) {
            (*blocos)++;
            *bytes += (int64_t)rt_lm_tab[i].n;
        }
    }
    rt_lm_soltar();
}

/* Sem memoria para os grupos: uma linha R0009 de resumo por lib com bloco vivo, sem pilha (nunca relatorio vazio); devolve quantas (0 ou 1). */
static int rt_lm_resumo_slot(uint32_t slot)
{
    RtLibmemGrupo resumo;

    memset(&resumo, 0, sizeof resumo);
    rt_lm_contar(slot, &resumo.blocos, &resumo.bytes);
    if (resumo.blocos == 0)
        return 0;
    resumo.arquivo = "(resumo)";
    rt_lm_linha(slot, &resumo, ", sem memoria para agrupar por ponto de alocacao; seguem vivos no fim do programa", false);
    return 1;
}

/* Agrupa as entradas vivas do slot (sob a trava) em grupos[]; devolve quantos grupos. */
static int rt_lm_agrupar(uint32_t slot, RtLibmemGrupo *grupos, RtLibmemGrupo *outros)
{
    int usados = 0;

    rt_lm_pegar();
    for (uint64_t i = 0; i < rt_lm_cap; i++) {
        if (rt_lm_tab[i].p != 0 && rt_lm_tab[i].slot == slot)
            rt_lm_somar(grupos, &usados, outros, &rt_lm_tab[i]);
    }
    rt_lm_soltar();
    return usados;
}

/* As linhas do slot (ate RT_LIBMEM_GRUPOS_MAX grupos + "(outros)"); devolve quantas. */
static int rt_lm_slot_relatar(uint32_t slot, RtLibmemGrupo *grupos)
{
    RtLibmemGrupo outros;
    int usados;
    int topo;
    int linhas = 0;

    memset(&outros, 0, sizeof outros);
    memset(grupos, 0, RT_LIBMEM_GRUPOS_TRABALHO * sizeof *grupos);
    usados = rt_lm_agrupar(slot, grupos, &outros);
    for (int i = 0; i < usados; i++)
        rt_lm_resolver(&grupos[i]);
    rt_lm_ordenar(grupos, usados);
    topo = usados < RT_LIBMEM_GRUPOS_MAX ? usados : RT_LIBMEM_GRUPOS_MAX;
    for (int i = 0; i < topo; i++, linhas++)
        rt_lm_linha(slot, &grupos[i], ", alocados por esta chamada, seguem vivos no fim do programa", true);
    for (int i = topo; i < usados; i++) {
        outros.blocos += grupos[i].blocos;
        outros.bytes += grupos[i].bytes;
    }
    if (outros.blocos > 0) {
        memset(outros.ras, 0, sizeof outros.ras);
        outros.arquivo = "(outros)";
        rt_lm_linha(slot, &outros, ", alocados em outros pontos, seguem vivos no fim do programa", true);
        linhas++;
    }
    return linhas;
}

/* Handler de saida (D-M6d-8; sem `ends` no P92d: toda lib viva no fim vaza): uma ou mais linhas R0009 lib por slot, na ordem do slot; devolve quantas linhas.
 * Sem memoria para os grupos (P93h) cai no resumo por lib: nunca sai relatorio vazio com bloco vivo. */
static int rt_libmem_relatar(void)
{
    RtLibmemGrupo *grupos = (RtLibmemGrupo *)malloc(RT_LIBMEM_GRUPOS_TRABALHO * sizeof *grupos);
    uint32_t qtd = rt_lm_qtd_slots();
    int linhas = 0;

    for (uint32_t slot = 0; slot < qtd; slot++) {
        if (rt_lm_global(slot))
            continue;
        linhas += grupos == NULL ? rt_lm_resumo_slot(slot) : rt_lm_slot_relatar(slot, grupos);
    }
    free(grupos);
    return linhas;
}

/* report_since: ", "libs": [{"lib": .., "blocos": .., "bytes": .., "global": false}]" com os vivos AGORA por lib (so as que tem bloco vivo); "global" so existe com `ends` (P92e). */
static void rt_libmem_libs_json(RtErroLote *lote)
{
    int64_t blocos[FORJA_LIBMEM_SLOTS_MAX] = {0};
    int64_t bytes[FORJA_LIBMEM_SLOTS_MAX] = {0};
    uint32_t qtd = rt_lm_qtd_slots();
    bool primeiro = true;

    rt_lm_pegar();
    for (uint64_t i = 0; i < rt_lm_cap; i++) {
        if (rt_lm_tab[i].p != 0 && rt_lm_tab[i].slot < FORJA_LIBMEM_SLOTS_MAX) {
            blocos[rt_lm_tab[i].slot]++;
            bytes[rt_lm_tab[i].slot] += (int64_t)rt_lm_tab[i].n;
        }
    }
    rt_lm_soltar();
    rt_erro_texto(lote, ", \"libs\": [");
    for (uint32_t slot = 0; slot < qtd && slot < FORJA_LIBMEM_SLOTS_MAX; slot++) {
        if (blocos[slot] == 0)
            continue;
        rt_erro_texto(lote, primeiro ? "{\"lib\": \"" : ", {\"lib\": \"");
        rt_erro_json(lote, rt_lm_nome(slot, 0));
        rt_erro_texto(lote, "\", \"blocos\": ");
        rt_erro_numero(lote, blocos[slot]);
        rt_erro_texto(lote, ", \"bytes\": ");
        rt_erro_numero(lote, bytes[slot]);
        rt_erro_texto(lote, rt_lm_global(slot) ? ", \"global\": true}" : ", \"global\": false}");
        primeiro = false;
    }
    rt_erro_texto(lote, "]");
}

#endif
