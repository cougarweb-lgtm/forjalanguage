/*
 * rt_pilha.c - rastreio da pilha .fj nas falhas de runtime do backend x64 (forja run|test --rastreio, IA1-b12).
 *
 * Versao: 0.7.0
 * Changelog: 0.7.0 - M6e P93e: rt_guarda (bit de flags da FRT1, lido pela forja_entrada: RT_GUARDA_LER) e rt_pilha_do_contexto (pilha da falha de hardware pelo rip/rsp/rbp do contexto, nao pela do tratador).
 *            0.6.2 - M6e P93d' (E7): rt_falha_extra (ponteiro em .bss, ligado por rt_morto_pendente) tira morto_bloco/morto_pilhas/off_json/rt_morto do caminho de forja_falha; os textos do R0008 morto viram variaveis rt_txt_* (secao .rdata propria): o 01_ola dev volta a 24106 B.
 *            0.6.1 - M6e P93d: cabecalho da FRT1 de 12 palavras (RT_TAB_FLAGS, RT_TAB_OFF_LINHAS) e rt_pilha_linha_do_rip (busca da linha pelo rip na subtabela de linhas).
 *            0.6.0 - M6e P93c: rt_pilha_ras_off (pilha como offsets u32), rt_pilha_off_json e o estado RtMorto: o R0008 de bloco morto leva "bloco" depois da causa e arquivo_alocacao/linha_alocacao/pilha_alocacao/pilha_delete depois da pilha do uso (D-M6e-4).
 *            0.5.0 - M6d P92d: rt_pilha_ras_lib (captura a pilha de dentro de C de lib sem frame pointer: os involucros de memoria por lib).
 *            0.4.0 - M6 P70: rt_pilha_ras guarda os enderecos de retorno crus (ate 4 procs .fj, D-M6-7) e rt_pilha_ras_json os resolve para .fj so no relatorio (cabecalho de heap e Arena/Pool do rt_rastreio.c).
 *            0.3.0 - M6 P69: o rastreio e o padrao do runtime dev (Windows e Linux); rbp validado alinhado a 16 (era 8); o topo de uma thread do Linux e o quadro da ponte (rt_fio_linux.c), a pilha e da propria thread.
 *            0.2.0- M5 P43: o estado (topo, quadros, valor) vira campo do RtFio (RtPilha em rt_fio.h); RT_PILHA_MAX e RtQuadro mudaram de lugar. 0.1.0 - IA1-b12: primeira versao. Coluna do ponto da falha, valor do R0006 (indice e tamanho; limites da fatia) e pilha .fj pela cadeia de rbp + tabela de posicoes.
 * Verificacao: python -I ..\tests\ia_first\verificar_rastreio_b12.py --forja ..\compiler\forja.exe
 *
 * UNIDADE PROPRIA do runtime (D-IA1-13): so compila com -DFORJA_RASTREIO, a variante rt_win_dev_rast.o (e a _crt), embutida no forja.exe e escolhida pelo compilador SO com
 * --rastreio. O rt_win_dev.o e o rt_win_rel.o nao a incluem (rt_nativo.c deixa so macros vazias): o exe de quem nao pede --rastreio sai byte a byte igual.
 * Incluido por rt_nativo.c, depois dos rt_erro_* e antes de forja_falha; nao compila sozinho.
 *
 * Todo proc gerado faz "push rbp / mov rbp,rsp" (x64_quadro.c): em cada quadro .fj [rbp] = rbp do chamador e [rbp+8] = endereco de retorno. Os quadros do runtime NAO servem
 * de cadeia (o gcc do Windows aponta rbp para dentro do quadro, rsp + ate 240, para o SEH): por isso a variante e compilada com -fno-omit-frame-pointer (o rbp do proc .fj
 * chega salvo logo abaixo do endereco de retorno, "push rbp" e a 1a instrucao) e o 1o quadro .fj sai de uma varredura da pilha, de rsp ate o topo marcado em forja_entrada:
 * a palavra que e um endereco de retorno da tabela, com o rbp salvo logo abaixo coerente. Dai em diante a cadeia de rbp dos procs gerados. O compilador grava no exe
 * (so com --rastreio) a tabela forja_rastro_tabela:
 *   cabecalho (u32, 12 palavras): magia, tam_texto, nfuncoes, ncalls, off_funcoes, off_calls, main_nome, main_arquivo, main_linha, main_coluna, flags (bit0 = guarda), off_linhas (0 = sem subtabela)
 *   funcoes[nfuncoes] {inicio, nome}: inicio em .text do programa (crescente), nome = offset de texto na tabela
 *   chamadas[ncalls] {ra, arquivo, linha, coluna}: ra = offset em .text do programa logo depois de um "call" (crescente), arquivo = offset de texto na tabela
 *   linhas (so com --guarda-memoria, em off_linhas): u32 nlinhas; nlinhas x {off, arquivo, linha, coluna}, off crescente (instrucao que falhou; FRT1 so tem retornos)
 * forja_rastro_texto e o inicio do .text do programa: endereco de retorno - forja_rastro_texto = ra da tabela. Sem endereco absoluto no exe.
 * A caminhada para no primeiro endereco de retorno que sai do .text do programa (forja_entrada) e nunca le acima de rt_pilha_topo. Coluna em bytes, como o diagnostico.
 * Fora do thread principal (rsp acima do topo) nao ha pilha. Falha dentro de proc que o rt_win_ryu.o (sem frame pointer) chama: sem pilha, o resto da linha R segue igual.
 */
#ifndef FORJA_RASTREIO
#error "rt_pilha.c so existe com -DFORJA_RASTREIO"
#endif

#define RT_PILHA_VARREDURA_PALAVRAS 8192 /* quanto da pilha do runtime a varredura do 1o quadro .fj percorre (64 KiB) */
#define RT_PILHA_RAS_MAX 4              /* quadros guardados no cabecalho de heap dev e no registro de Arena/Pool (D-M6-7) */
#define RT_PILHA_PASSOS_TOTAL 1000000   /* teto da caminhada (recursao muito funda) */
#define RT_PILHA_PILHA_MAX (1ull << 28) /* distancia maxima entre rsp e o topo para haver pilha */
#define RT_PILHA_MAGIA 0x31545246u      /* "FRT1" */
#define RT_PILHA_VALOR_NENHUM 0
#define RT_PILHA_VALOR_INDICE_TIPO 1
#define RT_PILHA_VALOR_FATIA_TIPO 2

enum { RT_TAB_MAGIA, RT_TAB_TAM_TEXTO, RT_TAB_NFUNCOES, RT_TAB_NCALLS, RT_TAB_OFF_FUNCOES, RT_TAB_OFF_CALLS, RT_TAB_MAIN_NOME, RT_TAB_MAIN_ARQUIVO, RT_TAB_MAIN_LINHA, RT_TAB_MAIN_COLUNA, RT_TAB_FLAGS, RT_TAB_OFF_LINHAS };
#define RT_TAB_FLAG_GUARDA 1u           /* RT_TAB_FLAGS bit0: exe construido com --guarda-memoria */

typedef struct { uint32_t inicio, nome; } RtRastroFuncao;
typedef struct { uint32_t ra, arquivo, linha, coluna; } RtRastroChamada;
typedef struct { uint32_t off, arquivo, linha, coluna; } RtRastroLinha;
extern const uint8_t forja_rastro_texto[];
extern const uint32_t forja_rastro_tabela[];

/* M6e P93e (D-M6e-8): o exe foi construido com --guarda-memoria. Gravado 1x pela forja_entrada (thread unica, antes do main); so o bit e lido la, todo o resto da guarda mora em rt_guarda.c
 * e so e alcancavel pelo caminho de alocacao com guarda (o 01_ola dev nao paga). */
RT_USADO static bool rt_guarda; /* compartilhado: gravado 1x na forja_entrada, antes de qualquer thread */
#define RT_GUARDA_LER() (rt_guarda = (forja_rastro_tabela[RT_TAB_FLAGS] & RT_TAB_FLAG_GUARDA) != 0)

/* O estado da pilha .fj e do fio da thread (RtPilha em rt_fio.h); rt_pilha_topo fica NULL fora da thread principal (sem pilha .fj para caminhar). */
#define rt_pilha_topo (rt_fio()->pilha.topo)
#define rt_quadros (rt_fio()->pilha.quadros)
#define rt_quadros_n (rt_fio()->pilha.quadros_n)
#define rt_quadros_total (rt_fio()->pilha.quadros_total)
#define rt_valor_tipo (rt_fio()->pilha.valor_tipo)
#define rt_valor_a (rt_fio()->pilha.valor_a)
#define rt_valor_b (rt_fio()->pilha.valor_b)
#define rt_valor_c (rt_fio()->pilha.valor_c)

#define RT_PILHA_TOPO() (rt_pilha_topo = (uintptr_t *)__builtin_frame_address(0))
#define RT_PILHA_CAPTURAR() rt_pilha_capturar()
#define RT_PILHA_VALOR_INDICE(i, n) (rt_valor_tipo = RT_PILHA_VALOR_INDICE_TIPO, rt_valor_a = (i), rt_valor_b = (n))
#define RT_PILHA_VALOR_FATIA(lo, hi, n) (rt_valor_tipo = RT_PILHA_VALOR_FATIA_TIPO, rt_valor_a = (lo), rt_valor_b = (hi), rt_valor_c = (n))
#define RT_FALHA_LINHA(lote, linha) rt_pilha_linha((lote), (linha))
#define RT_FALHA_FIM(lote) rt_pilha_fim((lote))
#define RT_MAIN_ERRO_FIM(lote) rt_pilha_fim_main((lote))

static const char *rt_pilha_texto(uint32_t off)
{
    return (const char *)forja_rastro_tabela + off;
}

/* A chamada da tabela cujo endereco de retorno (offset em .text do programa) e ra, ou NULL. Busca binaria: a tabela sai em ordem crescente de ra. */
static const RtRastroChamada *rt_pilha_chamada(uint32_t ra)
{
    const RtRastroChamada *v = (const RtRastroChamada *)((const uint8_t *)forja_rastro_tabela + forja_rastro_tabela[RT_TAB_OFF_CALLS]);
    uint32_t lo = 0, hi = forja_rastro_tabela[RT_TAB_NCALLS];

    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;

        if (v[meio].ra == ra)
            return &v[meio];
        if (v[meio].ra < ra)
            lo = meio + 1;
        else
            hi = meio;
    }
    return NULL;
}

/* A linha da instrucao em off (offset em .text do programa): a ultima entrada da subtabela de linhas que comeca em ou antes dele; NULL se o exe nao tem a subtabela
 * (sem --guarda-memoria) ou off vem antes da 1a entrada. Usada pelo tratador de falha da guarda (P93e). */
RT_USADO static const RtRastroLinha *rt_pilha_linha_do_rip(uint32_t off)
{
    uint32_t off_tab = forja_rastro_tabela[RT_TAB_OFF_LINHAS];
    const uint8_t *base = (const uint8_t *)forja_rastro_tabela + off_tab;
    const RtRastroLinha *v = (const RtRastroLinha *)(base + sizeof(uint32_t));
    uint32_t lo = 0, hi;

    if (off_tab == 0)
        return NULL;
    hi = *(const uint32_t *)base;
    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;

        if (v[meio].off <= off)
            lo = meio + 1;
        else
            hi = meio;
    }
    return lo == 0 ? NULL : &v[lo - 1];
}

/* Nome do proc que contem o offset: a ultima funcao que comeca em ou antes dele. */
static const char *rt_pilha_proc(uint32_t off)
{
    const RtRastroFuncao *v = (const RtRastroFuncao *)((const uint8_t *)forja_rastro_tabela + forja_rastro_tabela[RT_TAB_OFF_FUNCOES]);
    uint32_t lo = 0, hi = forja_rastro_tabela[RT_TAB_NFUNCOES];

    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;

        if (v[meio].inicio <= off)
            lo = meio + 1;
        else
            hi = meio;
    }
    return lo == 0 ? "?" : rt_pilha_texto(v[lo - 1].nome);
}

static void rt_pilha_registrar(uint32_t ra)
{
    const RtRastroChamada *ch = rt_pilha_chamada(ra);

    if (ch == NULL)
        return;
    if (rt_quadros_n < RT_PILHA_MAX) {
        rt_quadros[rt_quadros_n] = (RtQuadro){rt_pilha_proc(ra - 1), rt_pilha_texto(ch->arquivo), ch->linha, ch->coluna};
        rt_quadros_n++;
    }
    rt_quadros_total++;
}

/* O offset em .text do programa se ra cai dentro dele (um endereco de retorno nunca e o inicio), senao 0. */
static uint32_t rt_pilha_offset(uintptr_t ra)
{
    uintptr_t base = (uintptr_t)forja_rastro_texto;

    return ra > base && ra - base <= forja_rastro_tabela[RT_TAB_TAM_TEXTO] ? (uint32_t)(ra - base) : 0;
}

/* rbp de proc .fj plausivel (D-M6-7): alinhado a 16 (todo proc gerado faz push rbp com a pilha alinhada na chamada), acima do endereco de retorno que o aponta, com o quadro
 * inteiro (rbp salvo e retorno) abaixo do topo da pilha do fio. O topo de uma thread e o quadro da ponte (forja_fio_da_ponte): a cadeia para nele, antes da entrada do musl. */
static bool rt_pilha_rbp_ok(const uintptr_t *rbp, const uintptr_t *abaixo)
{
    return ((uintptr_t)rbp & 15u) == 0 && rbp > abaixo && rbp + 2 <= rt_pilha_topo && rbp[0] > (uintptr_t)rbp;
}

/* O 1o endereco de retorno da tabela acima de inicio cujo rbp salvo (a palavra de baixo) e coerente: o chamador do runtime e um proc .fj. NULL = nao achou. */
static const uintptr_t *rt_pilha_primeiro(const uintptr_t *inicio)
{
    for (const uintptr_t *a = inicio + 1; a + 1 < rt_pilha_topo && a - inicio < RT_PILHA_VARREDURA_PALAVRAS; a++) {
        uint32_t off = rt_pilha_offset(*a);

        if (off != 0 && rt_pilha_chamada(off) != NULL && rt_pilha_rbp_ok((const uintptr_t *)a[-1], a))
            return a;
    }
    return NULL;
}

/* A cadeia de rbp dos procs gerados a partir do rbp do 1o: cada quadro devolve em [rbp+8] o endereco de retorno no chamador. Para no 1o retorno fora do .text do programa. */
static void rt_pilha_seguir(const uintptr_t *rbp)
{
    for (uint32_t passos = 0; passos < RT_PILHA_PASSOS_TOTAL; passos++) {
        uint32_t off = rt_pilha_offset(rbp[1]);
        const uintptr_t *prox = (const uintptr_t *)rbp[0];

        if (off == 0)
            return;
        rt_pilha_registrar(off);
        if (prox <= rbp || !rt_pilha_rbp_ok(prox, rbp))
            return;
        rbp = prox;
    }
}

/* Guarda os primeiros RT_PILHA_MAX procs .fj da pilha (o mais interno primeiro) e a contagem total. noinline: o quadro proprio marca onde a varredura comeca. */
RT_NAO_INLINE static void rt_pilha_capturar(void)
{
    const uintptr_t *inicio = (const uintptr_t *)__builtin_frame_address(0);
    const uintptr_t *a;

    rt_quadros_n = 0;
    rt_quadros_total = 0;
    if (rt_pilha_topo == NULL || inicio >= rt_pilha_topo || (uint64_t)((uintptr_t)rt_pilha_topo - (uintptr_t)inicio) > RT_PILHA_PILHA_MAX)
        return;
    a = rt_pilha_primeiro(inicio);
    if (a == NULL)
        return;
    rt_pilha_registrar(rt_pilha_offset(*a));
    rt_pilha_seguir((const uintptr_t *)a[-1]);
}

/* P70 (D-M6-7): os enderecos de retorno crus dos primeiros RT_PILHA_RAS_MAX procs .fj, o mais interno primeiro; o resto zerado. Mesma caminhada validada de rt_pilha_capturar
 * (para no 1o invalido e na entrada da thread), mas sem resolver nome: a tabela so e consultada no relatorio. */
/* Grava o retorno ra na posicao k: o endereco em ras, ou o offset em off (um dos dois e NULL, constante em cada chamador: always_inline, sem custo no laco). */
static inline RT_SEMPRE_INLINE void rt_pilha_gravar(uintptr_t *ras, uint32_t *off, uint32_t k, uintptr_t ra)
{
    if (ras != NULL)
        ras[k] = ra;
    else
        off[k] = rt_pilha_offset(ra);
}

static inline RT_SEMPRE_INLINE void rt_pilha_coletar(const uintptr_t *inicio, uintptr_t *ras, uint32_t *off)
{
    const uintptr_t *a;
    const uintptr_t *rbp;
    uint32_t n = 0;

    for (uint32_t k = 0; k < RT_PILHA_RAS_MAX; k++)
        rt_pilha_gravar(ras, off, k, 0);
    if (rt_pilha_topo == NULL || inicio >= rt_pilha_topo || (uint64_t)((uintptr_t)rt_pilha_topo - (uintptr_t)inicio) > RT_PILHA_PILHA_MAX)
        return;
    a = rt_pilha_primeiro(inicio);
    if (a == NULL)
        return;
    rt_pilha_gravar(ras, off, n++, *a);
    rbp = (const uintptr_t *)a[-1];
    while (n < RT_PILHA_RAS_MAX && rt_pilha_offset(rbp[1]) != 0) {
        const uintptr_t *prox = (const uintptr_t *)rbp[0];

        rt_pilha_gravar(ras, off, n++, rbp[1]);
        if (prox <= rbp || !rt_pilha_rbp_ok(prox, rbp))
            return;
        rbp = prox;
    }
}

RT_NAO_INLINE static void rt_pilha_ras(uintptr_t *ras)
{
    rt_pilha_coletar((const uintptr_t *)__builtin_frame_address(0), ras, NULL);
}

/* M6d P92d: o mesmo que rt_pilha_ras para quem foi chamado por C de lib SEM frame pointer (os involucros de memoria por lib): o retorno para o .fj vem sem rbp salvo logo abaixo, entao o
 * 1o retorno da tabela vale sozinho (a tabela de chamadas e a validacao) e o rbp do proc .fj que chamou a lib e o 1o quadro plausivel acima dele. Chamar so de thread com fio. */
static const uintptr_t *rt_pilha_primeiro_lib(const uintptr_t *inicio)
{
    for (const uintptr_t *w = inicio + 1; w + 1 < rt_pilha_topo && w - inicio < RT_PILHA_VARREDURA_PALAVRAS; w++) {
        uint32_t off = rt_pilha_offset(*w);

        if (off != 0 && rt_pilha_chamada(off) != NULL)
            return w;
    }
    return NULL;
}

/* O rbp do proc .fj dono do retorno a: o 1o quadro plausivel acima dele (rbp + 8 e o retorno para o chamador do proc). Sem achar, devolve o fim da varredura (o laco de quem chama nao anda). */
static const uintptr_t *rt_pilha_rbp_acima(const uintptr_t *a)
{
    const uintptr_t *rbp = a + 1;

    for (; rbp + 2 <= rt_pilha_topo && rbp - a < RT_PILHA_VARREDURA_PALAVRAS; rbp++) {
        if (((uintptr_t)rbp & 15u) == 0 && rt_pilha_offset(rbp[1]) != 0 && rt_pilha_rbp_ok((const uintptr_t *)rbp[0], rbp))
            break;
    }
    return rbp;
}

RT_NAO_INLINE RT_USADO static void rt_pilha_ras_lib(uintptr_t *ras)
{
    const uintptr_t *inicio = (const uintptr_t *)__builtin_frame_address(0);
    const uintptr_t *a;
    const uintptr_t *rbp;
    uint32_t n = 0;

    for (uint32_t k = 0; k < RT_PILHA_RAS_MAX; k++)
        ras[k] = 0;
    if (rt_pilha_topo == NULL || inicio >= rt_pilha_topo || (uint64_t)((uintptr_t)rt_pilha_topo - (uintptr_t)inicio) > RT_PILHA_PILHA_MAX)
        return;
    a = rt_pilha_primeiro_lib(inicio);
    if (a == NULL)
        return;
    ras[n++] = *a;
    rbp = rt_pilha_rbp_acima(a);
    while (n < RT_PILHA_RAS_MAX && rbp + 2 <= rt_pilha_topo && rt_pilha_offset(rbp[1]) != 0) {
        const uintptr_t *prox = (const uintptr_t *)rbp[0];

        ras[n++] = rbp[1];
        if (prox <= rbp || !rt_pilha_rbp_ok(prox, rbp))
            return;
        rbp = prox;
    }
}

/* M6e P93c (D-M6e-3): a pilha da alocacao/delete do cabecalho de heap dev como offsets u32 em .text do programa (0 = sem quadro): o cabecalho segue em 80 B. */
RT_NAO_INLINE static void rt_pilha_ras_off(uint32_t *off)
{
    rt_pilha_coletar((const uintptr_t *)__builtin_frame_address(0), NULL, off);
}

/* M6e P93e (D-M6e-8): a pilha .fj de uma falha de hardware lida do CONTEXTO da thread que falhou (rip, rsp, rbp do SysFalha), nunca da pilha do tratador. Mesmo resultado de
 * rt_pilha_capturar: rt_quadros (o mais interno primeiro) e rt_quadros_total. Tres casos: (1) o rip esta no codigo do programa: o 1o quadro vem da subtabela de linhas (FRT1 so
 * conhece retornos) e a cadeia segue pelo rbp do contexto; (2) o rip esta no runtime (ex.: 2o delete lendo o cabecalho sem commit): a varredura a partir do rsp acha o retorno para o
 * .fj; (3) o rip esta em C de lib sem frame pointer (foreign memcpy): o 1o retorno da tabela vale sozinho e o rbp vem de rt_pilha_rbp_acima. Sem pilha .fj da thread (topo NULL), so o
 * quadro do rip. */
RT_USADO static void rt_pilha_do_contexto(const SysFalha *f)
{
    const uintptr_t *rsp = (const uintptr_t *)f->rsp;
    const uintptr_t *rbp = (const uintptr_t *)f->rbp;
    uint32_t off = rt_pilha_offset(f->rip);
    const RtRastroLinha *l = off != 0 ? rt_pilha_linha_do_rip(off) : NULL;
    const uintptr_t *a;

    rt_quadros_n = 0;
    rt_quadros_total = 0;
    if (l != NULL) {
        rt_quadros[0] = (RtQuadro){rt_pilha_proc(off), rt_pilha_texto(l->arquivo), l->linha, l->coluna};
        rt_quadros_n = 1;
        rt_quadros_total = 1;
    }
    if (rt_pilha_topo == NULL || rsp >= rt_pilha_topo || (uint64_t)((uintptr_t)rt_pilha_topo - (uintptr_t)rsp) > RT_PILHA_PILHA_MAX)
        return;
    if (l != NULL) {
        if (rt_pilha_rbp_ok(rbp, rsp - 1))
            rt_pilha_seguir(rbp);
        return;
    }
    a = rt_pilha_primeiro(rsp - 1);
    if (a != NULL) {
        rt_pilha_registrar(rt_pilha_offset(*a));
        rt_pilha_seguir((const uintptr_t *)a[-1]);
        return;
    }
    a = rt_pilha_primeiro_lib(rsp - 1);
    if (a == NULL)
        return;
    rt_pilha_registrar(rt_pilha_offset(*a));
    rbp = rt_pilha_rbp_acima(a);
    if (rbp + 2 <= rt_pilha_topo)
        rt_pilha_seguir(rbp);
}

/* A chamada da tabela do endereco de retorno ra (guardado por rt_pilha_ras), ou NULL se ra e 0 ou nao e mais da tabela. */
static const RtRastroChamada *rt_pilha_ras_chamada(uintptr_t ra)
{
    uint32_t off = ra == 0 ? 0 : rt_pilha_offset(ra);

    return off == 0 ? NULL : rt_pilha_chamada(off);
}

static void rt_pilha_quadro(RtErroLote *lote, const RtQuadro *q);

/* Arquivo e linha de quem chamou o wrapper de std:mem (arena_create/pool_create): o 2o quadro de ras (o 1o se so ha um). Sem quadro, *arquivo e *linha ficam como vieram. */
static void rt_pilha_ras_chamador(const uintptr_t *ras, const char **arquivo, int32_t *linha)
{
    const RtRastroChamada *ch = rt_pilha_ras_chamada(ras[1] != 0 ? ras[1] : ras[0]);

    if (ch == NULL)
        return;
    *arquivo = rt_pilha_texto(ch->arquivo);
    *linha = (int32_t)ch->linha;
}

/* Acrescenta ", "pilha": [...]" com os quadros de ras resolvidos para proc e arquivo:linha (nada se ras[0] e 0). */
static void rt_pilha_ras_json(RtErroLote *lote, const uintptr_t *ras)
{
    bool primeiro = true;

    for (uint32_t k = 0; k < RT_PILHA_RAS_MAX; k++) {
        const RtRastroChamada *ch = rt_pilha_ras_chamada(ras[k]);
        RtQuadro q;

        if (ch == NULL)
            break;
        q = (RtQuadro){rt_pilha_proc(rt_pilha_offset(ras[k]) - 1), rt_pilha_texto(ch->arquivo), ch->linha, ch->coluna};
        rt_erro_texto(lote, primeiro ? ", \"pilha\": [" : ", ");
        rt_pilha_quadro(lote, &q);
        primeiro = false;
    }
    if (!primeiro)
        rt_erro_texto(lote, "]");
}

/* Acrescenta ", "<chave>": [...]" com os quadros dos offsets (ate 4, para no 1o 0). Vazio: nada, a menos que 'sempre' (entao "[]"). */
static void rt_pilha_off_json(RtErroLote *lote, const char *chave, const uint32_t *off, bool sempre)
{
    bool aberto = false;

    for (uint32_t k = 0; k < RT_PILHA_RAS_MAX && off[k] != 0; k++) {
        const RtRastroChamada *ch = rt_pilha_chamada(off[k]);
        RtQuadro q;

        if (ch == NULL)
            break;
        q = (RtQuadro){rt_pilha_proc(off[k] - 1), rt_pilha_texto(ch->arquivo), ch->linha, ch->coluna};
        if (!aberto) {
            rt_erro_texto(lote, ", \"");
            rt_erro_texto(lote, chave);
            rt_erro_texto(lote, "\": [");
        } else {
            rt_erro_texto(lote, ", ");
        }
        aberto = true;
        rt_pilha_quadro(lote, &q);
    }
    if (!aberto && !sempre)
        return;
    if (!aberto) {
        rt_erro_texto(lote, ", \"");
        rt_erro_texto(lote, chave);
        rt_erro_texto(lote, "\": [");
    }
    rt_erro_texto(lote, "]");
}

/* R0008 de bloco morto (D-M6e-4): rt_rastreio_vivo grava aqui o que a linha leva alem do padrao e chama forja_falha, que passa por rt_pilha_fim (bloco depois da causa;
 * arquivo/linha da alocacao e as pilhas da alocacao e do delete depois da pilha do uso). Quem relata e a 1a thread a falhar (rt_falha_reservar): um so escritor. */
typedef struct {
    bool ativo;
    const char *bloco;
    const char *arquivo;
    int32_t linha;
    uint32_t off[2 * RT_PILHA_RAS_MAX]; /* [0..3] pilha da alocacao, [4..7] a do delete (ou do append/arena_destroy que matou) */
} RtMorto;
static RtMorto rt_morto; /* compartilhado: so com a falha em curso (ver acima) */

/* Textos do R0008 de bloco morto em variaveis (-fdata-sections): cada uma vai para a propria secao .rdata e sai do exe que nao alcanca rt_pilha_morto_fase; literal solto
 * iria para o .rdata unico do objeto e o 01_ola dev pagaria por eles (teto de 24 KB, E7). */
static const char rt_txt_bloco[] = ", \"bloco\": \"";
static const char rt_txt_arquivo_alocacao[] = ", \"arquivo_alocacao\": \"";
static const char rt_txt_linha_alocacao[] = "\", \"linha_alocacao\": ";
static const char rt_txt_pilha_alocacao[] = "pilha_alocacao";
static const char rt_txt_pilha_delete[] = "pilha_delete";

static void rt_pilha_morto_bloco(RtErroLote *lote)
{
    if (!rt_morto.ativo)
        return;
    rt_erro_texto(lote, rt_txt_bloco);
    rt_erro_texto(lote, rt_morto.bloco);
    rt_erro_texto(lote, "\"");
}

static void rt_pilha_morto_pilhas(RtErroLote *lote)
{
    if (!rt_morto.ativo)
        return;
    rt_morto.ativo = false;
    rt_erro_texto(lote, rt_txt_arquivo_alocacao);
    rt_erro_json(lote, rt_morto.arquivo);
    rt_erro_texto(lote, rt_txt_linha_alocacao);
    rt_erro_numero(lote, rt_morto.linha);
    rt_pilha_off_json(lote, rt_txt_pilha_alocacao, rt_morto.off, true);
    rt_pilha_off_json(lote, rt_txt_pilha_delete, rt_morto.off + RT_PILHA_RAS_MAX, true);
}

/* Gancho em .bss (M6e P93d' E7): so rt_morto_pendente o liga, entao forja_falha/rt_pilha_fim nao puxam morto_*, rt_pilha_off_json e rt_morto no exe que nao tem
 * R0008 de bloco morto (o 01_ola dev fica no teto de 24 KB). fase 0 = "bloco" (depois da causa), fase 1 = arquivo/linha e as pilhas (depois da pilha do uso). */
static void (*volatile rt_falha_extra)(RtErroLote *lote, int fase);

static void rt_pilha_morto_fase(RtErroLote *lote, int fase)
{
    if (fase == 0)
        rt_pilha_morto_bloco(lote);
    else
        rt_pilha_morto_pilhas(lote);
}

/* O numero da linha e, se o quadro mais interno cai nessa linha, a coluna dele (senao a coluna seria de outro ponto). */
static void rt_pilha_linha(RtErroLote *lote, int linha)
{
    rt_erro_numero(lote, linha);
    if (rt_quadros_n > 0 && linha > 0 && rt_quadros[0].linha == (uint32_t)linha) {
        rt_erro_texto(lote, ", \"coluna\": ");
        rt_erro_numero(lote, rt_quadros[0].coluna);
    }
}

static void rt_pilha_valor(RtErroLote *lote)
{
    if (rt_valor_tipo == RT_PILHA_VALOR_NENHUM)
        return;
    if (rt_valor_tipo == RT_PILHA_VALOR_INDICE_TIPO) {
        rt_erro_texto(lote, ", \"valor\": {\"indice\": ");
        rt_erro_numero(lote, rt_valor_a);
        rt_erro_texto(lote, ", \"tamanho\": ");
        rt_erro_numero(lote, rt_valor_b);
    } else {
        rt_erro_texto(lote, ", \"valor\": {\"inicio\": ");
        rt_erro_numero(lote, rt_valor_a);
        rt_erro_texto(lote, ", \"fim\": ");
        rt_erro_numero(lote, rt_valor_b);
        rt_erro_texto(lote, ", \"tamanho\": ");
        rt_erro_numero(lote, rt_valor_c);
    }
    rt_erro_texto(lote, "}");
    rt_valor_tipo = RT_PILHA_VALOR_NENHUM;
}

static void rt_pilha_quadro(RtErroLote *lote, const RtQuadro *q)
{
    rt_erro_texto(lote, "{\"proc\": \"");
    rt_erro_json(lote, q->proc);
    rt_erro_texto(lote, "\", \"arquivo\": \"");
    rt_erro_json(lote, q->arquivo);
    rt_erro_texto(lote, "\", \"linha\": ");
    rt_erro_numero(lote, q->linha);
    rt_erro_texto(lote, ", \"coluna\": ");
    rt_erro_numero(lote, q->coluna);
    rt_erro_texto(lote, "}");
}

/* Fecha o texto da causa, acrescenta valor e pilha e fecha a linha. Sem quadro (falha fora de proc .fj) nao ha "pilha". */
static void rt_pilha_fim(RtErroLote *lote)
{
    rt_erro_texto(lote, "\"");
    if (rt_falha_extra != NULL)
        rt_falha_extra(lote, 0);
    rt_pilha_valor(lote);
    if (rt_quadros_n > 0) {
        rt_erro_texto(lote, ", \"pilha\": [");
        for (uint32_t k = 0; k < rt_quadros_n; k++) {
            if (k > 0)
                rt_erro_texto(lote, ", ");
            rt_pilha_quadro(lote, &rt_quadros[k]);
        }
        rt_erro_texto(lote, "]");
        if (rt_quadros_total > rt_quadros_n) {
            rt_erro_texto(lote, ", \"quadros\": ");
            rt_erro_numero(lote, rt_quadros_total);
        }
    }
    if (rt_falha_extra != NULL)
        rt_falha_extra(lote, 1);
    rt_erro_texto(lote, "}\n");
}

/* R0004: o main ja voltou, entao a pilha e o proprio main (posicao do corpo dele), nao a cadeia de rbp. */
static void rt_pilha_fim_main(RtErroLote *lote)
{
    rt_erro_texto(lote, "\"");
    if (forja_rastro_tabela[RT_TAB_MAIN_NOME] != 0) {
        RtQuadro q = {rt_pilha_texto(forja_rastro_tabela[RT_TAB_MAIN_NOME]), rt_pilha_texto(forja_rastro_tabela[RT_TAB_MAIN_ARQUIVO]),
                      forja_rastro_tabela[RT_TAB_MAIN_LINHA], forja_rastro_tabela[RT_TAB_MAIN_COLUNA]};

        rt_erro_texto(lote, ", \"pilha\": [");
        rt_pilha_quadro(lote, &q);
        rt_erro_texto(lote, "]");
    }
    rt_erro_texto(lote, "}\n");
}
