/*
 * rt_teste.c - executor de testes do runtime nativo da Forja (M2 P12b): registro, laco de execucao e expect.
 *
 * Versao: 0.3.2
 * Changelog:
 *   0.3.2 - M6e P93k: o executor guarda a serie de nascimento (forja_memsnap_serie) ao entrar no teste, em vez da contagem de vivos; rt_vazamento_do_teste recebe a serie.
 *   0.3.1 - M3 F9a: rt_teste_titulo_casa abaixo de 6 decisoes (rt_teste_trecho_no_inicio); comportamento igual.
 *   0.3.0 - Linguagem v1.11 L16b: vazamento por teste e queda por teste (R0011). Dev: o executor marca forja_rt_vivos_qtd() ao entrar no teste; teste que passou e deixou bloco de heap vivo falha com as
 *           linhas R0009 (rt_vazamento_do_teste, em rt_rastreio.c) e o rastreador esquece os blocos. forja_testes_rodar liga o tratador de excecoes (sys_excecao_ligar): acesso invalido ou instrucao ilegal
 *           dentro de teste vira R0011 pelo forja_falha (o mesmo salto do teste), a 2a queda no teste e a queda fora de teste seguem como antes. Texto igual ao do forja_rt.h 0.10.2 (oraculo).
 *   0.2.0 -Linguagem v1.11 L16a: forja test --filtro/--json. forja_testes_rodar le FORJA_TESTE_FILTRO e FORJA_TESTE_JSON por sys_ambiente (GetEnvironmentVariableA, importado so por quem roda 'forja test'):
 *           os testes cujo titulo nao contem o trecho contam em "filtrados"; com JSON uma linha {"teste", "resultado", e na falha "codigo", "arquivo", "linha", "causa" da 1a falha}
 *           por teste e {"resumo": {"total", "ok", "falhou", "filtrados"}}. O texto e o do forja_rt.h 0.10.1 (oraculo). Sem as variaveis a saida e a de antes, byte a byte.
 *   0.1.0 - M2 P12b: forja_teste_registrar, forja_testes_rodar e forja_expect (com o ramo de dentro de teste), com os textos do forja_rt.h 0.9.1:851-912
 *           ("ok   <nome>" e "FALHOU <nome>" em stdout, "<n> testes, <f> falharam", "  expect falhou: <texto> (<arq>:<linha>)" em stderr). Cada teste roda
 *           depois de forja_rt_salvar; uma falha de runtime (forja_falha) marca o teste como falho e volta aqui por forja_rt_saltar (rt_nativo.c).
 *           Fora de teste forja_expect continua sendo R0003 "expect falhou: <texto da condicao>" (causa de ate 255 bytes, como o snprintf de 256 do forja_rt.h).
 *
 * O estado (rt_em_teste, rt_teste_falhou, rt_sem_relatorio, rt_salto) mora em rt_nativo.c, que vem antes na mesma unidade.
 * Incluido por rt_win.c; nao compila sozinho.
 */
#define RT_TESTES_MAX 4096

typedef struct {
    const char *nome;
    ForjaTesteFn fn;
} RtTesteEntrada;

static RtTesteEntrada rt_testes[RT_TESTES_MAX];
static int rt_testes_total;
static int rt_teste_atual;

/* A 1a falha do teste em curso para o --json (L16a): a do forja_falha (R-code) ou a do expect; so a 1a conta, como no forja_rt.h. */
#define RT_TESTE_CAUSA_BYTES 512
#define RT_TESTE_FILTRO_BYTES 256
static bool rt_falha_anotada;
static const char *rt_falha_codigo;
static const char *rt_falha_arquivo;
static int rt_falha_linha;
static char rt_falha_causa[RT_TESTE_CAUSA_BYTES];

/* A causa e copiada (o buffer de quem falhou some no salto), cortada em 511 bytes como o snprintf do forja_rt.h. */
static void rt_teste_anotar(const char *codigo, const char *causa, const char *arquivo, int linha)
{
    int64_t k = 0;

    if (rt_falha_anotada)
        return;
    rt_falha_anotada = true;
    rt_falha_codigo = codigo;
    rt_falha_arquivo = arquivo;
    rt_falha_linha = linha;
    for (; causa != NULL && causa[k] != '\0' && k < RT_TESTE_CAUSA_BYTES - 1; k++)
        rt_falha_causa[k] = causa[k];
    rt_falha_causa[k] = '\0';
}

/* ---- expect ---- */

/* "  expect falhou: <texto> (<arq>:<linha>)" em stderr: dentro de teste o expect so relata e o teste segue. */
static void rt_expect_relatar(const char *texto_cond, const char *arquivo, int linha)
{
    RtErroLote lote;

    lote.usado = 0;
    rt_erro_texto(&lote, "  expect falhou: ");
    rt_erro_texto(&lote, texto_cond);
    rt_erro_texto(&lote, " (");
    rt_erro_texto(&lote, arquivo);
    rt_erro_texto(&lote, ":");
    rt_erro_numero(&lote, linha);
    rt_erro_texto(&lote, ")\n");
    rt_erro_fim(&lote);
}

/* "expect falhou: <texto>" cortado em 255 bytes (o snprintf de 256 do forja_rt.h). */
static const char *rt_expect_causa(RtCausa *causa, const char *texto_cond)
{
    rt_causa_iniciar(causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(causa, "expect falhou: ");
    rt_causa_texto(causa, texto_cond);
    return rt_causa_fim(causa);
}

static _Noreturn void rt_expect_panic(const char *texto_cond, const char *arquivo, int linha)
{
    RtCausa causa;

    forja_panic(rt_expect_causa(&causa, texto_cond), arquivo, linha);
}

/* expect: dentro de teste registra a falha e segue; fora de teste vira forja_panic. */
void forja_expect(bool cond, const char *texto_cond, const char *arquivo, int linha)
{
    RtCausa causa;

    if (cond)
        return;
    if (!rt_em_teste)
        rt_expect_panic(texto_cond, arquivo, linha);
    rt_teste_falhou = true;
    rt_teste_anotar(FORJA_R0003, rt_expect_causa(&causa, texto_cond), arquivo, linha);
    rt_expect_relatar(texto_cond, arquivo, linha);
}

/* ---- registro e execucao ---- */

/* Estouro da tabela e falha interna (R0003), como no forja_rt.h. */
void forja_teste_registrar(const char *nome, ForjaTesteFn fn)
{
    if (rt_testes_total >= RT_TESTES_MAX)
        forja_falha(FORJA_R0003, "limite de testes excedido (FORJA_TESTES_MAX)", __FILE__, __LINE__);
    rt_testes[rt_testes_total].nome = nome;
    rt_testes[rt_testes_total].fn = fn;
    rt_testes_total++;
}

/* ---- vazamento e queda por teste (L16b) ---- */

#define FORJA_R0011 "R0011"
#define RT_EXCECAO_ACESSO 0xC0000005UL /* EXCEPTION_ACCESS_VIOLATION */
#define RT_EXCECAO_ILEGAL 0xC000001DUL /* EXCEPTION_ILLEGAL_INSTRUCTION */

/* Blocos de heap vivos ao entrar no teste em curso e a queda ja tratada nele (estado de arquivo: o executor fica sem locais). */
static uint32_t rt_teste_serie_antes; /* compartilhado: so o executor de testes (uma thread) a grava; serie de nascimento do proximo bloco ao entrar no teste (P93k; antes era a contagem de vivos) */
static bool rt_queda_no_teste;

/* Vazamento do teste: em dev e de rt_rastreio.c (que vem depois nesta unidade); em release nao ha rastreio. */
#ifdef FORJA_DEV
static bool rt_vazamento_do_teste(uint32_t serie_antes);
#define rt_teste_serie() forja_memsnap_serie()
#else
#define rt_vazamento_do_teste(serie_antes) ((void)(serie_antes), false)
#define rt_teste_serie() 0u
#endif

/* Roda o teste rt_teste_atual. noinline e sem locais: o salvar fica isolado do laco do chamador (o -Wclobbered do -Wextra nao tem o que reclamar). */
static RT_NAO_INLINE bool rt_teste_executar(void)
{
    rt_teste_falhou = false;
    rt_falha_anotada = false;
    rt_queda_no_teste = false;
    rt_teste_serie_antes = rt_teste_serie();
    rt_em_teste = true;
    if (forja_rt_salvar(&rt_salto) == 0)
        rt_testes[rt_teste_atual].fn();
    rt_em_teste = false;
    if (!rt_teste_falhou && rt_vazamento_do_teste(rt_teste_serie_antes))
        rt_teste_falhou = true;
    return rt_teste_falhou;
}

/* Acesso a memoria invalida ou instrucao ilegal; outra excecao, NULL. */
static const char *rt_queda_motivo(unsigned long codigo)
{
    if (codigo == RT_EXCECAO_ACESSO)
        return "acesso a memoria invalida";
    if (codigo == RT_EXCECAO_ILEGAL)
        return "instrucao ilegal";
    return NULL;
}

/* Tratador de excecoes: dentro de teste a 1a queda vira R0011 (forja_falha salta para o executor, o teste falha, o proximo roda); 0 = seguir a busca (o processo cai como antes). */
static int rt_queda_tratar(unsigned long codigo)
{
    const char *motivo = rt_queda_motivo(codigo);
    RtCausa causa;

    if (!rt_em_teste || rt_queda_no_teste || motivo == NULL)
        return 0;
    rt_queda_no_teste = true;
    rt_causa_iniciar(&causa, RT_CAUSA_FAIXA_BYTES);
    rt_causa_texto(&causa, "o teste \"");
    rt_causa_texto(&causa, rt_testes[rt_teste_atual].nome);
    rt_causa_texto(&causa, "\" causou uma excecao do processador: ");
    rt_causa_texto(&causa, motivo);
    forja_falha(FORJA_R0011, rt_causa_fim(&causa), "", 0);
}

/* "<prefixo><nome>\n" em stdout. */
static void rt_teste_linha(const char *prefixo, const char *nome)
{
    rt_saida_stdout(prefixo, (int64_t)strlen(prefixo));
    rt_saida_stdout(nome, (int64_t)strlen(nome));
    rt_saida_stdout("\n", 1);
    rt_saida_fim_print();
}

/* ---- saida de stdout em pedacos (a linha do --json e o resumo) ---- */

static void rt_teste_texto(const char *s)
{
    rt_saida_stdout(s, (int64_t)strlen(s));
}

static void rt_teste_numero(int64_t valor)
{
    unsigned char digitos[24];

    rt_saida_stdout(digitos, rt_fmt_i64(digitos, valor));
}

/* Escape curto de JSON de um byte (aspas, barra, \n, \r, \t); NULL se o byte nao tem escape curto. */
static const char *rt_teste_escape_curto(unsigned char b)
{
    switch (b) {
    case '"': return "\\\"";
    case '\\': return "\\\\";
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\t': return "\\t";
    default: return NULL;
    }
}

/* Texto em stdout escapado para JSON, como o forja_json_texto_em do forja_rt.h (NULL = vazio). */
static void rt_teste_json_texto(const char *texto)
{
    const unsigned char *p = (const unsigned char *)(texto != NULL ? texto : "");
    unsigned char hex[4];

    for (; *p != '\0'; p++) {
        const char *curto = rt_teste_escape_curto(*p);

        if (curto != NULL) {
            rt_teste_texto(curto);
        } else if (*p < 0x20) {
            rt_teste_texto("\\u");
            rt_fmt_hex4(hex, *p);
            rt_saida_stdout(hex, 4);
        } else {
            rt_saida_stdout(p, 1);
        }
    }
}

/* ---- --filtro e --json (L16a): o driver passa FORJA_TESTE_FILTRO e FORJA_TESTE_JSON no ambiente do exe ---- */

/* O trecho inteiro aparece no inicio do titulo. */
static bool rt_teste_trecho_no_inicio(const char *titulo, const char *trecho)
{
    int64_t k = 0;

    while (trecho[k] != '\0' && titulo[k] == trecho[k])
        k++;
    return trecho[k] == '\0';
}

/* O titulo contem o trecho (sensivel a caixa); trecho NULL ou vazio casa tudo. */
static bool rt_teste_titulo_casa(const char *titulo, const char *trecho)
{
    if (trecho == NULL || trecho[0] == '\0')
        return true;
    for (; *titulo != '\0'; titulo++) {
        if (rt_teste_trecho_no_inicio(titulo, trecho))
            return true;
    }
    return false;
}

static bool rt_teste_json_pedido(void)
{
    char valor[4];

    return sys_ambiente("FORJA_TESTE_JSON", valor, sizeof valor) == 1 && valor[0] == '1';
}

/* FORJA_TESTE_FILTRO em buf (ate 255 bytes); NULL se ausente, vazio ou maior que o buffer (o driver recusa mais de 255). */
static const char *rt_teste_filtro(char *buf, int64_t n)
{
    return sys_ambiente("FORJA_TESTE_FILTRO", buf, n) > 0 ? buf : NULL;
}

/* --json: {"teste": "<titulo>", "resultado": "ok"} ou, em falha, "resultado": "falhou" mais codigo, arquivo, linha e causa da 1a falha. */
static void rt_teste_json(const char *titulo, bool falhou)
{
    rt_teste_texto("{\"teste\": \"");
    rt_teste_json_texto(titulo);
    if (!falhou) {
        rt_teste_texto("\", \"resultado\": \"ok\"}\n");
        return;
    }
    rt_teste_texto("\", \"resultado\": \"falhou\", \"codigo\": \"");
    rt_teste_json_texto(rt_falha_codigo);
    rt_teste_texto("\", \"arquivo\": \"");
    rt_teste_json_texto(rt_falha_arquivo);
    rt_teste_texto("\", \"linha\": ");
    rt_teste_numero(rt_falha_linha);
    rt_teste_texto(", \"causa\": \"");
    rt_teste_json_texto(rt_falha_causa);
    rt_teste_texto("\"}\n");
}

/* Uma linha por teste: "ok   <titulo>" / "FALHOU <titulo>" ou o JSON do --json. */
static void rt_teste_saida(bool json, const char *titulo, bool falhou)
{
    if (json)
        rt_teste_json(titulo, falhou);
    else
        rt_teste_linha(falhou ? "FALHOU " : "ok   ", titulo);
    rt_saida_fim_print();
}

/* "<rodados> testes, <f> falharam[, <k> filtrados]\n" ou {"resumo": {...}} (total = todos os registrados), em stdout. */
static void rt_teste_resumo(bool json, int filtrados, int falhas)
{
    int rodados = rt_testes_total - filtrados;

    if (json) {
        rt_teste_texto("{\"resumo\": {\"total\": ");
        rt_teste_numero(rt_testes_total);
        rt_teste_texto(", \"ok\": ");
        rt_teste_numero(rodados - falhas);
        rt_teste_texto(", \"falhou\": ");
        rt_teste_numero(falhas);
        rt_teste_texto(", \"filtrados\": ");
        rt_teste_numero(filtrados);
        rt_teste_texto("}}\n");
    } else {
        rt_teste_numero(rodados);
        rt_teste_texto(" testes, ");
        rt_teste_numero(falhas);
        rt_teste_texto(" falharam");
        if (filtrados > 0) {
            rt_teste_texto(", ");
            rt_teste_numero(filtrados);
            rt_teste_texto(" filtrados");
        }
        rt_teste_texto("\n");
    }
    rt_saida_fim_print();
}

/* Roda os testes registrados (todos, ou so os do FORJA_TESTE_FILTRO). Devolve 0 se tudo passou, 1 se algum falhou. */
int forja_testes_rodar(void)
{
    char filtro_buf[RT_TESTE_FILTRO_BYTES];
    const char *filtro = rt_teste_filtro(filtro_buf, sizeof filtro_buf);
    bool json = rt_teste_json_pedido();
    int falhas = 0;
    int filtrados = 0;
    int i;

    sys_excecao_ligar(rt_queda_tratar);
    for (i = 0; i < rt_testes_total; i++) {
        bool falhou;

        if (!rt_teste_titulo_casa(rt_testes[i].nome, filtro)) {
            filtrados++;
            continue;
        }
        rt_teste_atual = i;
        falhou = rt_teste_executar();
        falhas += falhou ? 1 : 0;
        rt_teste_saida(json, rt_testes[i].nome, falhou);
    }
    rt_teste_resumo(json, filtrados, falhas);
    if (falhas != 0)
        rt_sem_relatorio = true;
    return falhas == 0 ? 0 : 1;
}
