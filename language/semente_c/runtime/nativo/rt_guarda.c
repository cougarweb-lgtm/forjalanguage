/*
 * rt_guarda.c - alocador de guarda (--guarda-memoria), cemiterio e pre-tratador de falha do runtime nativo da Forja (M6e P93e, D-M6e-7 e D-M6e-8).
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - M6e P93k: o relato descarrega o stdout sem rt_saida_trava (autotravamento dentro do print) e a 2a thread so espera a dona ate RT_GUARDA_ESPERA_NS; registro cheio (~14,9 M blocos) tem R0007 proprio;
 *           a pilha da alocacao entra no registro antes da publicacao da contagem.
 *   0.2.0 - M6e P93f: Linux. Commit recusado por limite de mapeamentos (sys_guarda_limite_mapas) vira R0007 com o texto do vm.max_map_count; o resto do SO ficou atras de sys_*.
 *   0.1.0 - M6e P93e: primeira versao (Windows). Reserva de 1 GiB por faixa (teto de 1 TiB), bloco = paginas com commit + 1 pagina de guarda sem commit (o corpo termina no fim da ultima
 *           pagina, alinhado a 16), registro de cada bloco num vetor so de acrescimo em paginas proprias, morte = causa publicada no registro + paginas fora do commit (o endereco nunca volta),
 *           pre-tratador que acha o bloco pelo endereco da falha e relata o R0008 com as 3 pilhas pelo CONTEXTO (rip, rsp, rbp).
 *
 * Incluido por rt_rastreio.c (so DEV), depois da trava e antes do ciclo de vida do bloco; nao compila sozinho. So e alcancavel quando rt_guarda e true (o bit de flags da FRT1, lido pela
 * forja_entrada): a 1a alocacao com guarda reserva o vetor de registros e liga o pre-tratador (sys_falha_ligar) sob rt_rastreio_trava; o 01_ola dev nao paga nada (teto de 24 KB).
 *
 * Disposicao de um bloco de heap: [... folga][reg* 8 B + 8 B][ForjaCabecalho 80 B][corpo ate o fim da ultima pagina commitada][pagina de guarda]. O ponteiro para o RtGuardaReg fica antes do cabecalho:
 * achar o registro a partir de p e uma leitura. Cada RtGuardaReg tem a parte do nascimento (base, fim, bytes, arquivo, linha, ras_off[0..3]) e a da morte (ras_off[4..7], causa publicada por
 * ULTIMO com store-release): o tratador le SEM trava (load-acquire da contagem e da causa), entao uma falha dentro de trecho que segura a trava nao trava o relato.
 * Verificacao: python tests\nativo\m6e\verificar_m6e.py --forja compiler\forja.exe --grupo guarda --alvo windows
 */
#define RT_GUARDA_PAGINA ((int64_t)4096)
#define RT_GUARDA_FAIXA_BYTES ((int64_t)1 << 30)      /* reserva de enderecos por faixa (D-M6e-7) */
#define RT_GUARDA_RESERVA_MAX ((int64_t)1 << 40)      /* teto de enderecos reservados: 1 TiB (R0007 passando disso); so blocos grandes chegam la: com blocos pequenos o registro esgota antes */
#define RT_GUARDA_FAIXAS_MAX 1024                      /* 1 TiB / 1 GiB */
#define RT_GUARDA_REGS_RESERVA ((int64_t)1 << 30)      /* enderecos do vetor de registros: 1 GiB / sizeof(RtGuardaReg) = ~14,9 M blocos (R0007 "o registro da guarda esgotou N blocos") */
#define RT_GUARDA_ESPERA_NS ((int64_t)3000000000)      /* P93k: quanto uma 2a thread espera a dona do relato antes de devolver "nao e meu" */
#define RT_GUARDA_REGS_PASSO ((int64_t)64 * 1024)      /* commit do vetor de registros, de 64 em 64 KiB */
#define RT_GUARDA_DESCRITOR ((int64_t)16)              /* ponteiro para o registro (8 B) + 8 B de alinhamento, antes do cabecalho */
#define RT_GUARDA_ALINHA16(x) (((x) + 15) & ~(int64_t)15)

/* Estados de rt_guarda_pegar / rt_guarda_falhar. */
#define RT_GUARDA_OK 0
#define RT_GUARDA_ESGOTOU 1
#define RT_GUARDA_SEM_MEMORIA 2
#define RT_GUARDA_SEM_SUPORTE 3
#define RT_GUARDA_LIMITE_MAPAS 4
#define RT_GUARDA_REGISTRO_CHEIO 5

typedef struct {
    uintptr_t base, atual, fim;
} RtGuardaFaixa;

typedef struct {
    uintptr_t base, fim; /* [base, fim) = paginas com commit; [fim, fim + 4096) = a pagina de guarda */
    int64_t bytes;
    const char *arquivo;
    int32_t linha;
    uint32_t causa; /* 0 = vivo; RT_MORTE_* quando morreu (publicado por ultimo, store-release) */
    uint32_t ras_off[2 * RT_RAS_MAX]; /* [0..3] pilha da alocacao, [4..7] a da morte */
} RtGuardaReg;

static RtGuardaFaixa rt_g_faixas[RT_GUARDA_FAIXAS_MAX]; /* compartilhado: escrito sob rt_rastreio_trava; rt_g_faixas_n e publicado com store-release */
static int rt_g_faixas_n; /* compartilhado: idem rt_g_faixas */
static int64_t rt_g_reservado; /* compartilhado: sob rt_rastreio_trava */
static RtGuardaReg *rt_g_regs; /* compartilhado: gravado 1x sob rt_rastreio_trava, antes da 1a publicacao de rt_g_regs_n */
static int64_t rt_g_regs_commit; /* compartilhado: sob rt_rastreio_trava */
static uint64_t rt_g_regs_n; /* compartilhado: escrita sob rt_rastreio_trava, publicada com store-release; o tratador a le com load-acquire sem trava */

/* Textos em variaveis (-fdata-sections): cada uma vai para a propria secao e some do exe que nao alcanca a guarda (literal solto iria para o .rdata unico do objeto, E7). */
static const char rt_gt_vazio[] = "";
static const char rt_gt_std[] = "std:mem";
static const char rt_gt_esgotou[] = "a guarda de memoria esgotou 1 TiB de enderecos; rode um trecho menor";
static const char rt_gt_regs_cheio_a[] = "o registro da guarda esgotou ";
static const char rt_gt_regs_cheio_b[] = " blocos; rode um trecho menor";
static const char rt_gt_sem_suporte[] = "a guarda de memoria nao conseguiu reservar enderecos (sem suporte neste sistema ou sem memoria)";
static const char rt_gt_limite_mapas[] = "a guarda de memoria atingiu o limite de mapeamentos do sistema (vm.max_map_count, 65530 por padrao: cerca de 32 mil blocos vivos); rode um trecho menor";
static const char rt_gt_sem_memoria[] = "sem memoria no heap para bytes";

/* O bloco p (cabecalho ou corpo) esta numa faixa reservada pela guarda? So faixas ja publicadas; as faixas nunca mudam depois. */
static bool rt_guarda_dono(const void *p)
{
    uintptr_t u = (uintptr_t)p;
    int n = __atomic_load_n(&rt_g_faixas_n, __ATOMIC_ACQUIRE);

    for (int i = 0; i < n; i++) {
        if (u >= rt_g_faixas[i].base && u < rt_g_faixas[i].fim)
            return true;
    }
    return false;
}

/* O registro do bloco cujo corpo comeca em corpo (heap: o p do programa; pedaco de Arena: o ForjaBloco). */
static RtGuardaReg *rt_guarda_reg_do_corpo(const void *corpo)
{
    return *(RtGuardaReg **)((uintptr_t)corpo - (uintptr_t)RT_ALOC_EXTRA - (uintptr_t)RT_GUARDA_DESCRITOR);
}

#ifdef FORJA_RASTREIO
static int rt_guarda_pre(const SysFalha *f);
static void rt_guarda_ligar_pre(void)
{
    sys_falha_ligar(rt_guarda_pre);
}
#else
static void rt_guarda_ligar_pre(void)
{
}
#endif

/* Endereco de um bloco novo (sob a trava): avanca o ponteiro da ultima faixa; faixa nova (1 GiB, ou o bloco arredondado a 64 KiB se maior) quando nao cabe. */
static int rt_guarda_endereco(int64_t total, uintptr_t *base)
{
    RtGuardaFaixa *f = rt_g_faixas_n > 0 ? &rt_g_faixas[rt_g_faixas_n - 1] : NULL;

    if (f == NULL || (int64_t)(f->fim - f->atual) < total) {
        int64_t tam = total > RT_GUARDA_FAIXA_BYTES ? ((total + 65535) & ~(int64_t)65535) : RT_GUARDA_FAIXA_BYTES;
        void *p;

        if (rt_g_reservado + tam > RT_GUARDA_RESERVA_MAX || rt_g_faixas_n >= RT_GUARDA_FAIXAS_MAX)
            return RT_GUARDA_ESGOTOU;
        p = sys_guarda_reservar(tam);
        if (p == NULL)
            return rt_g_faixas_n == 0 ? RT_GUARDA_SEM_SUPORTE : RT_GUARDA_SEM_MEMORIA;
        f = &rt_g_faixas[rt_g_faixas_n];
        f->base = f->atual = (uintptr_t)p;
        f->fim = (uintptr_t)p + (uintptr_t)tam;
        rt_g_reservado += tam;
        __atomic_store_n(&rt_g_faixas_n, rt_g_faixas_n + 1, __ATOMIC_RELEASE);
    }
    *base = f->atual;
    f->atual += (uintptr_t)total;
    return RT_GUARDA_OK;
}

/* Proxima entrada do vetor de registros (sob a trava): reserva o vetor e liga o pre-tratador na 1a vez, e faz commit de 64 em 64 KiB. */
static int rt_guarda_registro(RtGuardaReg **slot)
{
    int64_t preciso = (int64_t)(rt_g_regs_n + 1) * (int64_t)sizeof(RtGuardaReg);

    if (rt_g_regs == NULL) {
        rt_g_regs = (RtGuardaReg *)sys_guarda_reservar(RT_GUARDA_REGS_RESERVA);
        if (rt_g_regs == NULL)
            return RT_GUARDA_SEM_SUPORTE;
        rt_guarda_ligar_pre();
    }
    if (preciso > RT_GUARDA_REGS_RESERVA)
        return RT_GUARDA_REGISTRO_CHEIO;
    while (preciso > rt_g_regs_commit) {
        if (!sys_guarda_commit((char *)rt_g_regs + rt_g_regs_commit, RT_GUARDA_REGS_PASSO))
            return RT_GUARDA_SEM_MEMORIA;
        rt_g_regs_commit += RT_GUARDA_REGS_PASSO;
    }
    *slot = &rt_g_regs[rt_g_regs_n];
    return RT_GUARDA_OK;
}

/* Endereco + registro do bloco novo, sob a trava: o registro e escrito inteiro ANTES de a contagem ser publicada (store-release). total inclui a pagina de guarda. */
static int rt_guarda_pegar(int64_t total, const RtGuardaReg *modelo, RtGuardaReg **slot)
{
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);
    uintptr_t base = 0;
    int estado = rt_guarda_endereco(total, &base);

    if (estado == RT_GUARDA_OK)
        estado = rt_guarda_registro(slot);
    if (estado == RT_GUARDA_OK) {
        **slot = *modelo;
        (*slot)->base = base;
        (*slot)->fim = base + (uintptr_t)(total - RT_GUARDA_PAGINA);
        __atomic_store_n(&rt_g_regs_n, rt_g_regs_n + 1, __ATOMIC_RELEASE);
    }
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    return estado;
}

static _Noreturn void rt_guarda_falhar(int estado, int64_t bytes, const char *arquivo, int linha)
{
    if (estado == RT_GUARDA_ESGOTOU)
        forja_falha(FORJA_R0007, rt_gt_esgotou, arquivo, linha);
    if (estado == RT_GUARDA_SEM_SUPORTE)
        forja_falha(FORJA_R0007, rt_gt_sem_suporte, arquivo, linha);
    if (estado == RT_GUARDA_LIMITE_MAPAS)
        forja_falha(FORJA_R0007, rt_gt_limite_mapas, arquivo, linha);
    if (estado == RT_GUARDA_REGISTRO_CHEIO) {
        RtCausa causa;

        rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
        rt_causa_texto(&causa, rt_gt_regs_cheio_a);
        rt_causa_i64(&causa, RT_GUARDA_REGS_RESERVA / (int64_t)sizeof(RtGuardaReg));
        rt_causa_texto(&causa, rt_gt_regs_cheio_b);
        forja_falha(FORJA_R0007, rt_causa_fim(&causa), arquivo, linha);
    }
    forja_falha_alocacao(rt_gt_sem_memoria, bytes, arquivo, linha);
}

/* Bloco de `bytes` de corpo: paginas com commit (descritor + cabecalho de 80 B + corpo alinhado a 16, o corpo colado na pagina de guarda) e a pagina de guarda. Devolve o corpo, ou NULL se
 * bytes e absurdo (quem chama relata "sem memoria"); falta de enderecos ou de memoria relata aqui (R0007). *slot = o registro do bloco.
 * LIMITE: parametros 5 (o bloco, sua origem, o registro de saida e a pilha da alocacao; agrupar criaria um tipo de uso unico). */
static void *rt_guarda_bloco(int64_t bytes, const char *arquivo, int linha, RtGuardaReg **slot, const uint32_t *off_aloc)
{
    RtGuardaReg modelo = {0, 0, bytes, arquivo != NULL ? arquivo : rt_gt_vazio, (int32_t)linha, 0, {0}};
    int64_t corpo, paginas;
    uintptr_t inicio;
    int estado;

    if (bytes < 0 || bytes > RT_GUARDA_RESERVA_MAX / 2)
        return NULL;
    for (int k = 0; k < RT_RAS_MAX && off_aloc != NULL; k++)
        modelo.ras_off[k] = off_aloc[k]; /* P93k: a pilha da alocacao entra no registro ANTES de a contagem ser publicada */
    corpo = RT_GUARDA_ALINHA16(bytes);
    paginas = (RT_GUARDA_DESCRITOR + RT_ALOC_EXTRA + corpo + RT_GUARDA_PAGINA - 1) / RT_GUARDA_PAGINA;
    estado = rt_guarda_pegar((paginas + 1) * RT_GUARDA_PAGINA, &modelo, slot);
    if (estado == RT_GUARDA_OK && !sys_guarda_commit((void *)(*slot)->base, paginas * RT_GUARDA_PAGINA))
        estado = sys_guarda_limite_mapas() ? RT_GUARDA_LIMITE_MAPAS : RT_GUARDA_SEM_MEMORIA;
    if (estado != RT_GUARDA_OK)
        rt_guarda_falhar(estado, bytes, arquivo, linha);
    inicio = (*slot)->fim - (uintptr_t)corpo;
    *(RtGuardaReg **)(inicio - (uintptr_t)RT_ALOC_EXTRA - (uintptr_t)RT_GUARDA_DESCRITOR) = *slot;
    return (void *)inicio;
}

/* Bloco de heap com guarda: o cabecalho de 80 B (que rt_rastreio_criar preenche) e a pilha da alocacao no registro. NULL = bytes absurdo. */
static ForjaCabecalho *rt_guarda_alocar(int64_t bytes, const char *arquivo, int linha)
{
    RtGuardaReg *slot;
    uint32_t off[RT_RAS_MAX];
    void *corpo;

    rt_ras_capturar_off(off);
    corpo = rt_guarda_bloco(bytes, arquivo, linha, &slot, off);
    if (corpo == NULL)
        return NULL;
    return (ForjaCabecalho *)corpo - 1;
}

/* Pedaco de Arena com guarda: arquivo:linha e a pilha da criacao da Arena (ras, de arena_create) ficam como a "alocacao" do pedaco (D-M6e-7). */
static void *rt_guarda_arena(int64_t bytes, const uintptr_t *ras)
{
    RtGuardaReg *slot;
    const char *arquivo = rt_gt_std;
    int32_t linha = 0;
    uint32_t off[RT_RAS_MAX] = {0};
    void *corpo;

    rt_ras_chamador(ras, &arquivo, &linha);
#ifdef FORJA_RASTREIO
    for (int k = 0; k < RT_RAS_MAX; k++)
        off[k] = rt_pilha_offset(ras[k]);
#endif
    corpo = rt_guarda_bloco(bytes, arquivo, linha, &slot, off);
    if (corpo == NULL)
        rt_guarda_falhar(RT_GUARDA_SEM_MEMORIA, bytes, arquivo, linha);
    return corpo;
}

/* Morte do bloco: pilha da morte e causa no registro SOB a trava (a causa por ultimo, store-release), depois as paginas saem do commit; o endereco segue reservado e nunca volta. */
static void rt_guarda_morrer(RtGuardaReg *reg, const uint32_t *ras_morte, uint32_t causa)
{
    bool tomou = sys_trava_pegar(&rt_rastreio_trava);

    for (int k = 0; k < RT_RAS_MAX; k++)
        reg->ras_off[RT_RAS_MAX + k] = ras_morte[k];
    __atomic_store_n(&reg->causa, causa, __ATOMIC_RELEASE);
    sys_trava_soltar(&rt_rastreio_trava, tomou);
    sys_guarda_decommit((void *)reg->base, (int64_t)(reg->fim - reg->base));
}

/* delete ou append que moveu: o cabecalho (ainda com commit) ja tem a pilha da morte em ras_off[4..7]. */
static void rt_guarda_enterrar(ForjaCabecalho *c, uint32_t causa)
{
    rt_guarda_morrer(rt_guarda_reg_do_corpo(c + 1), c->ras_off + RT_RAS_MAX, causa);
}

/* arena_destroy (ou limpar) de um pedaco: a pilha da morte e a de quem chamou. */
static void rt_guarda_enterrar_bloco(void *corpo, uint32_t causa)
{
    uint32_t morte[RT_RAS_MAX];

    rt_ras_capturar_off(morte);
    rt_guarda_morrer(rt_guarda_reg_do_corpo(corpo), morte, causa);
}

#ifdef FORJA_RASTREIO
/* ---- pre-tratador de falha (D-M6e-8) ---- */

static const char rt_gt_heap[] = "heap";
static const char rt_gt_lista[] = "lista";
static const char rt_gt_arena[] = "arena";
static const char rt_gt_causa_delete[] = "uso depois do delete: bloco de ";
static const char rt_gt_causa_lista[] = "uso depois do append que moveu a lista: bloco de ";
static const char rt_gt_causa_arena[] = "uso depois do arena_destroy: bloco de ";
static const char rt_gt_causa_alem[] = "acesso alem do fim do bloco de ";
static const char rt_gt_alocado_em[] = " bytes alocado em ";
static const char rt_gt_json_cabeca[] = "{\"codigo\": \"R0008\", \"tipo\": \"runtime\", \"arquivo\": \"";
static const char rt_gt_json_linha[] = "\", \"linha\": ";
static const char rt_gt_json_causa[] = ", \"causa\": \"";

static const void *rt_guarda_relator; /* compartilhado: o fio que ganhou o relato (CAS NULL -> fio); depois o processo sai */

/* O registro cujo bloco contem o endereco da falha: um morto (qualquer byte dele, ou a pagina de guarda dele) ou a pagina de guarda de um VIVO. NULL = nao e meu. Sem trava (load-acquire). */
static const RtGuardaReg *rt_guarda_achar(uint64_t a)
{
    uint64_t n = __atomic_load_n(&rt_g_regs_n, __ATOMIC_ACQUIRE);

    for (uint64_t i = n; i > 0; i--) {
        const RtGuardaReg *r = &rt_g_regs[i - 1];

        if (a < r->base || a >= r->fim + (uint64_t)RT_GUARDA_PAGINA)
            continue;
        return (__atomic_load_n(&r->causa, __ATOMIC_ACQUIRE) != 0 || a >= r->fim) ? r : NULL;
    }
    return NULL;
}

static const char *rt_guarda_nome_bloco(uint32_t morte)
{
    if (morte == RT_MORTE_ARENA_DESTROY)
        return rt_gt_arena;
    return morte == RT_MORTE_APPEND_MOVEU ? rt_gt_lista : rt_gt_heap;
}

/* "uso depois do delete: bloco de N bytes alocado em arq:linha" (ou o do append/arena_destroy; "acesso alem do fim do bloco de N bytes ..." no bloco vivo). */
static const char *rt_guarda_texto_causa(RtCausa *causa, const RtGuardaReg *r, uint32_t morte)
{
    const char *inicio = rt_gt_causa_delete;

    if (morte == 0)
        inicio = rt_gt_causa_alem;
    else if (morte == RT_MORTE_APPEND_MOVEU)
        inicio = rt_gt_causa_lista;
    else if (morte == RT_MORTE_ARENA_DESTROY)
        inicio = rt_gt_causa_arena;
    rt_causa_iniciar(causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(causa, inicio);
    rt_causa_i64(causa, r->bytes);
    rt_causa_texto(causa, rt_gt_alocado_em);
    rt_causa_texto(causa, r->arquivo);
    rt_causa_byte(causa, ':');
    rt_causa_i64(causa, r->linha);
    return rt_causa_fim(causa);
}

/* O R0008 montado a mao (nao passa por forja_falha: ele pegaria a pilha do tratador): arquivo:linha do USO = o 1o quadro da pilha pelo contexto; "bloco" e as pilhas pelo rt_morto que
 * rt_pilha_fim ja sabe imprimir. Descarrega o stdout e sai com 101 sem rodar os handlers de saida (nada de R0009 depois). */
static _Noreturn void rt_guarda_relatar(const RtGuardaReg *r, const SysFalha *f)
{
    RtErroLote lote;
    RtCausa causa;
    uint32_t morte = __atomic_load_n(&r->causa, __ATOMIC_ACQUIRE);
    const char *arquivo = rt_gt_vazio;
    int32_t linha = 0;

    rt_pilha_do_contexto(f);
    if (rt_quadros_n > 0) {
        arquivo = rt_quadros[0].arquivo;
        linha = (int32_t)rt_quadros[0].linha;
    }
    rt_morto = (RtMorto){true, rt_guarda_nome_bloco(morte), r->arquivo, r->linha, {0}};
    for (int k = 0; k < 2 * RT_RAS_MAX; k++)
        rt_morto.off[k] = r->ras_off[k];
    rt_falha_extra = rt_pilha_morto_fase;
    lote.usado = 0;
    rt_erro_texto(&lote, rt_gt_json_cabeca);
    rt_erro_json(&lote, arquivo);
    rt_erro_texto(&lote, rt_gt_json_linha);
    RT_FALHA_LINHA(&lote, linha);
    rt_erro_texto(&lote, rt_gt_json_causa);
    rt_erro_json(&lote, rt_guarda_texto_causa(&causa, r, morte));
    RT_FALHA_FIM(&lote);
    rt_erro_fim(&lote);
    rt_sem_relatorio = true;
    rt_saida_descarregar_falha(); /* P93k: sem rt_saida_trava (a falha pode ter vindo de dentro do rt_print_linha, que a segura) */
    sys_sair(RT_CODIGO_SAIDA_FALHA);
}

/* Pre-tratador (sys_falha_ligar): 0 = nao e meu (o tratador do forja test, ou a acao padrao, seguem como hoje). Reentrada (falha dentro do relato) = nao e meu; outra thread ja
 * relatando = espera ate ela sair do processo. */
static int rt_guarda_pre(const SysFalha *f)
{
    const RtGuardaReg *r;
    const void *dona;

    if (f->codigo != RT_EXCECAO_ACESSO)
        return 0;
    r = rt_guarda_achar(f->endereco);
    if (r == NULL)
        return 0;
    dona = __sync_val_compare_and_swap(&rt_guarda_relator, NULL, (const void *)rt_fio());
    if (dona == (const void *)rt_fio())
        return 0;
    if (dona != NULL) {
        /* P93k: a dona pode nunca sair do processo (relato morto no meio, handler do teste que voltou): espera ate RT_GUARDA_ESPERA_NS e, passado o prazo, devolve "nao e meu" (acao padrao). */
        int64_t limite = sys_tempo_ns() + RT_GUARDA_ESPERA_NS;

        while (__atomic_load_n(&rt_guarda_relator, __ATOMIC_ACQUIRE) == dona && sys_tempo_ns() < limite)
            __builtin_ia32_pause();
        return 0;
    }
    rt_guarda_relatar(r, f);
}
#endif
