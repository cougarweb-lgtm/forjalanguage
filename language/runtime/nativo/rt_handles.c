/*
 * rt_handles.c - rastreador generico de handles de lib C em DEV do runtime nativo da Forja (M6 P71b, D-M6-8): std:mem track_handle(kind, h) e untrack_handle(h).
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M6c P88 (D-M6c-6): forja_handle_nasce(kind, h, indireto, destruidor) e forja_handle_solta(h), chamados pelo codigo gerado em dev ao lado do foreign com clausula creates/destroys/takes.
 *           O no automatico guarda a pilha a partir do proc do usuario (a linha R0009 usa o 1o quadro, nao o 2o do wrapper de std:mem) e o nome do destruidor (campo "destruidor" da linha, que o
 *           driver usa na forma_certa). solta de handle DESCONHECIDO = silencio (nunca R0008: handle vindo de proc sem clausula, de Detach ou de dentro da lib). Mesma lista e mesma trava de track_handle.
 *   0.1.0 - M6 P71b: primeira versao. Lista PROPRIA de handles (fora do cabecalho de heap de 80 B), cada no alocado por sys_heap_alocar (sem cabecalho de rastreio), sob rt_rastreio_trava (D0657:
 *           a lista so e lida/escrita depois do pegar). Cada no guarda o kind COPIADO (ate RT_HANDLE_KIND_MAX-1 bytes: o cstr do usuario pode apontar para memoria que some), h e a pilha de ate 4
 *           quadros de quem chamou track_handle (rt_ras_capturar). track_handle de h ja rastreado so ATUALIZA o kind e a pilha (nao e erro: o handle de lib C costuma ser reaproveitado pela
 *           lib). h nulo e ignorado nas duas chamadas (malloc que falhou nao tem o que vazar; untrack(NULL) e no-op como free(NULL)). Handle vivo no fim = linha R0009 {"tipo": "handle",
 *           "kind": ...} com a pilha .fj, no mesmo handler de saida e no mesmo exit 101 do R0009 de heap/Arena/Pool; untrack de handle desconhecido = R0008 (mesmo caminho do delete duplo,
 *           arquivo std:mem). A DLL nao relata (como Arena/Pool: o DETACH so olha o heap).
 *
 * So existe com FORJA_DEV; incluido no FIM de rt_rastreio.c (usa rt_rastreio_trava, rt_ras_*, rt_erro_*, rt_causa_*); nao compila sozinho.
 */
#ifndef RT_RASTREIO_C
#error "rt_handles.c so existe incluido por rt_rastreio.c"
#endif

#define RT_HANDLE_KIND_MAX 48

typedef struct RtHandle {
    struct RtHandle *prox;
    const void *h;
    uintptr_t ras[RT_RAS_MAX];
    char kind[RT_HANDLE_KIND_MAX];
    char destruidor[RT_HANDLE_KIND_MAX]; /* so no no automatico: nome Forja do proc destroys do tipo; vazio = nenhum */
    bool automatico;                     /* nasceu de forja_handle_nasce: a pilha ja comeca no proc do usuario */
} RtHandle;

static RtHandle *rt_hand_cabeca; /* compartilhado: lista dos handles vivos (mais antigo primeiro), sob rt_rastreio_trava */
static RtHandle *rt_hand_cauda; /* compartilhado: idem rt_hand_cabeca */

/* Copia o kind (truncado em RT_HANDLE_KIND_MAX-1); kind nulo vira "handle". */
static void rt_handle_kind(RtHandle *n, const char *kind)
{
    int i = 0;

    if (kind == NULL)
        kind = "handle";
    for (; i < RT_HANDLE_KIND_MAX - 1 && kind[i] != '\0'; i++)
        n->kind[i] = kind[i];
    n->kind[i] = '\0';
}

/* O no de h, ou NULL; quem chama segura rt_rastreio_trava. */
static RtHandle *rt_handle_achar(const void *h)
{
    for (RtHandle *n = rt_hand_cabeca; n != NULL; n = n->prox) {
        if (n->h == h)
            return n;
    }
    return NULL;
}

/* Copia o texto (truncado em tam-1); nulo vira vazio. */
static void rt_handle_copiar(char *dest, const char *texto, int tam)
{
    int i = 0;

    for (; texto != NULL && i < tam - 1 && texto[i] != '\0'; i++)
        dest[i] = texto[i];
    dest[i] = '\0';
}

/* h passa a ser vivo ate a baixa. Ja rastreado = atualiza kind, destruidor, pilha e origem. */
static void rt_handle_registrar(const char *kind, void *h, bool automatico, const char *destruidor)
{
    RtHandle *n;
    RtHandle *achado;
    bool tomou;

    n = (RtHandle *)sys_heap_alocar((int64_t)sizeof *n);
    if (n == NULL)
        forja_falha_alocacao("sem memoria para o rastreio de handle em bytes", (int64_t)sizeof *n, "std:mem", 0);
    n->prox = NULL;
    n->h = h;
    n->automatico = automatico;
    rt_handle_kind(n, kind);
    rt_handle_copiar(n->destruidor, destruidor, RT_HANDLE_KIND_MAX);
    rt_ras_capturar(n->ras); /* pilha da propria thread: fora da trava */
    tomou = sys_trava_pegar(&rt_rastreio_trava);
    achado = rt_handle_achar(h);
    if (achado != NULL) {
        for (int k = 0; k < RT_RAS_MAX; k++)
            achado->ras[k] = n->ras[k];
        for (int k = 0; k < RT_HANDLE_KIND_MAX; k++) {
            achado->kind[k] = n->kind[k];
            achado->destruidor[k] = n->destruidor[k];
        }
        achado->automatico = automatico;
    } else {
        if (rt_hand_cauda != NULL)
            rt_hand_cauda->prox = n;
        else
            rt_hand_cabeca = n;
        rt_hand_cauda = n;
    }
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    if (achado != NULL)
        sys_heap_liberar(n);
    rt_rastreio_registrar_saida();
}

/* track_handle(kind, h): h passa a ser vivo ate untrack_handle. Ja rastreado = atualiza kind e pilha. */
void forja_handle_track(const char *kind, void *h)
{
    if (h == NULL)
        return;
    rt_handle_registrar(kind, h, false, NULL);
}

/* Foreign com 'creates': h e o handle devolvido (indireto = 0) ou o ponteiro de saida, de onde a lib escreveu o handle (indireto != 0). Nulo e ignorado (fopen que falhou). */
void forja_handle_nasce(const char *kind, void *h, int32_t indireto, const char *destruidor)
{
    if (indireto != 0 && h != NULL)
        h = *(void *const *)h;
    if (h == NULL)
        return;
    rt_handle_registrar(kind, h, true, destruidor);
}

/* Tira h da lista; devolve false se nunca foi rastreado (ou ja saiu). */
static bool rt_handle_tirar(const void *h)
{
    RtHandle *ant = NULL;
    RtHandle *n;
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);

    for (n = rt_hand_cabeca; n != NULL && n->h != h; n = n->prox)
        ant = n;
    if (n != NULL) {
        if (ant != NULL)
            ant->prox = n->prox;
        else
            rt_hand_cabeca = n->prox;
        if (rt_hand_cauda == n)
            rt_hand_cauda = ant;
    }
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    if (n == NULL)
        return false;
    sys_heap_liberar(n);
    return true;
}

/* untrack_handle(h): handle desconhecido = R0008 (a pilha .fj sai do forja_falha). */
void forja_handle_untrack(void *h)
{
    if (h == NULL)
        return;
    if (!rt_handle_tirar(h))
        forja_falha(FORJA_R0008, "untrack_handle(h): o handle nao foi rastreado por track_handle ou ja passou por untrack_handle", "std:mem", 0);
}

/* Foreign com 'destroys' ou 'takes': h sai da lista; desconhecido (veio de proc sem clausula, de Detach, de dentro da lib) = silencio. */
void forja_handle_solta(void *h)
{
    if (h == NULL)
        return;
    (void)rt_handle_tirar(h);
}

/* Uma linha R0009 de handle vivo: arquivo:linha = quem chamou track_handle (o 2o quadro; o 1o e o wrapper de std:mem); no automatico o 1o quadro ja e o proc do usuario. */
static void rt_handle_linha(const RtHandle *n)
{
    RtErroLote lote;
    RtCausa causa;
    const char *arquivo = "std:mem";
    int32_t linha = 0;

    uintptr_t primeiro[RT_RAS_MAX] = {n->ras[0]};

    rt_ras_chamador(n->automatico ? primeiro : n->ras, &arquivo, &linha);
    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "vazamento: handle ");
    rt_causa_texto(&causa, n->kind);
    rt_causa_texto(&causa, n->automatico ? " vivo no fim do programa: falta fechar (destroys)" : " vivo no fim do programa: falta untrack_handle");
    lote.usado = 0;
    rt_erro_texto(&lote, "{\"codigo\": \"R0009\", \"tipo\": \"handle\", \"kind\": \"");
    rt_erro_json(&lote, n->kind);
    if (n->destruidor[0] != '\0') {
        rt_erro_texto(&lote, "\", \"destruidor\": \"");
        rt_erro_json(&lote, n->destruidor);
    }
    rt_erro_texto(&lote, "\", \"arquivo\": \"");
    rt_erro_json(&lote, arquivo);
    rt_erro_texto(&lote, "\", \"linha\": ");
    rt_erro_numero(&lote, linha);
    rt_erro_texto(&lote, ", \"causa\": \"");
    rt_erro_json(&lote, rt_causa_fim(&causa));
    rt_erro_texto(&lote, "\"");
    rt_ras_json(&lote, n->ras);
    rt_erro_texto(&lote, "}\n");
    rt_erro_fim(&lote);
}

/* Handler de saida: uma linha R0009 por handle vivo (mais antigo primeiro); devolve quantos eram. */
static int rt_handles_vivos_relatar(void)
{
    int vivos = 0;
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);

    for (const RtHandle *n = rt_hand_cabeca; n != NULL; n = n->prox) {
        rt_handle_linha(n);
        vivos++;
    }
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    return vivos;
}
