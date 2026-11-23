/* fmt_nucleo.c - o motor do 'forja fmt' (ver fmt_nucleo.h): normaliza BOM e CRLF, le os tokens (lexer + parser, sem imports nem semantica) e acerta, vao a vao, o que ha entre
 * dois tokens: espaco (tabela de fmt_espaco.c), indentacao (fmt_indent.c), linhas em branco, espaco no fim e o espaco antes do '//' final. Nenhum token muda.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - F1 P1a: primeira versao (so o nucleo; o main_despacho chama no 'forja fmt --stdin').
 * Verificacao: construir.bat && python tests\fmt\verificar_fmt.py --forja compiler\forja.exe
 *
 * Um VAO e o texto entre dois tokens (ou entre o inicio do arquivo e o 1o, ou entre o ultimo e o fim): espaco, tab, '//' comentarios e quebras de linha. Dentro dele cada trecho e uma
 * Edicao independente; o f"..." inteiro conta como um token (nada dentro dele muda).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/compilacao.h"
#include "driver/fmt_interno.h"
#include "driver/fmt_nucleo.h"

enum { TOKEN_LEN_MAX = 65535, INDENTACAO = 4 };

/* O estado de uma formatacao: o texto normalizado, os tokens e a tabela de inicios de linha (para as posicoes das Edicoes). pend_* e a faixa de apagamento ainda nao emitida
 * (apagamentos vizinhos viram uma Edicao so). */
typedef struct {
    const char *txt;
    size_t tam;
    FmtToken *tk;
    size_t n;
    uint32_t *inicios;
    size_t ninicios;
    FmtResultado *r;
    bool pend, falhou, tudo_apagado; /* tudo_apagado: o arquivo so tinha espaco e linhas em branco; sai vazio (o motor de edicoes nao expressa essa faixa) */
    size_t pend_ini, pend_fim;
    const char *pend_regra;
} Ctx;

/* Um vao: [g0, g1) entre o token a (se tem_a) e o token b (se tem_b; senao e o fim do arquivo). */
typedef struct {
    size_t g0, g1, b;
    bool tem_a, tem_b, conteudo;
    uint32_t brancas;
    size_t fim_conteudo;
} Vao;

static bool EhEspaco(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* ---------- normalizacao ---------- */

/* Onde o 1o CRLF estava (linha e coluna no texto ja normalizado). */
typedef struct { uint32_t linha, coluna; } PosCrlf;

/* BOM fora e CRLF vira LF. */
static void Normalizar(FmtResultado *r, const char *fonte, size_t tam, PosCrlf *crlf)
{
    size_t i = 0;
    uint32_t linha = 1, coluna = 1;

    BufReservarTotal(&r->normalizado, tam + 1);
    BufAnexarN(&r->normalizado, "", 0);
    if (tam >= 3 && memcmp(fonte, "\xEF\xBB\xBF", 3) == 0) {
        r->tinha_bom = true;
        i = 3;
    }
    for (; i < tam; i++) {
        if (fonte[i] == '\r' && i + 1 < tam && fonte[i + 1] == '\n') {
            if (!r->tinha_crlf) {
                r->tinha_crlf = true;
                *crlf = (PosCrlf){linha, coluna};
            }
            continue;
        }
        BufAnexarN(&r->normalizado, fonte + i, 1);
        coluna = fonte[i] == '\n' ? 1 : coluna + 1;
        linha += fonte[i] == '\n';
    }
}

/* ---------- tokens ---------- */

static void RegistrarErro(FmtResultado *r, const Compilacao *c)
{
    const Diag *d = c->sem.lista.n > 0 ? &c->sem.lista.itens[0] : &c->diag;

    r->estado = FMT_NAO_ANALISADO;
    if (!d->tem && c->sem.lista.n == 0) {
        snprintf(r->motivo, sizeof r->motivo, "falha sem diagnostico");
        return;
    }
    snprintf(r->codigo, sizeof r->codigo, "%s", DiagCodigoTexto(d->codigo));
    snprintf(r->motivo, sizeof r->motivo, "%s", d->causa);
    FonteLocalizar(&c->fonte, d->off, &r->diag_linha, &r->diag_coluna);
}

/* Indice do token FTEXTO_FIM (1 byte, a aspa) do f"..." que comeca em raw[i]; SIZE_MAX = sem fim ou com pedaco de 65535 bytes ou mais. */
static size_t AcharFimDoFTexto(const Token *raw, size_t i, size_t total)
{
    for (; i < total; i++) {
        if (raw[i].len >= TOKEN_LEN_MAX) return SIZE_MAX;
        if (raw[i].tipo == TK_FTEXTO_FIM) return i;
    }
    return SIZE_MAX;
}

/* Tokens do lexer -> tokens logicos (sem NL e EOF; f"..." inteiro e um). false = token de 65535 bytes ou mais (nao_analisado). */
static bool MontarTokens(const Vec *tokens, const char *txt, FmtToken *tk, size_t *n)
{
    const Token *raw = tokens->dados;
    size_t k = 0;

    for (size_t i = 0; i < tokens->n; i++) {
        if (raw[i].tipo == TK_NL || raw[i].tipo == TK_EOF) continue;
        if (raw[i].len >= TOKEN_LEN_MAX) return false;
        tk[k] = (FmtToken){.ini = raw[i].off, .fim = raw[i].off + raw[i].len, .tipo = raw[i].tipo};
        if (raw[i].tipo == TK_FTEXTO_INICIO) {
            size_t fim = AcharFimDoFTexto(raw, i, tokens->n);

            if (fim == SIZE_MAX) return false;
            tk[k].fim = raw[fim].off + 1;
            i = fim;
        }
        tk[k].primeiro = k == 0 || memchr(txt + tk[k - 1].fim, '\n', tk[k].ini - tk[k - 1].fim) != NULL;
        k++;
    }
    *n = k;
    return true;
}

/* Lexer + parser do texto normalizado. true = tokens em *tk (malloc) e *n. false = r diz por que (nao_analisado). */
static bool LerTokens(FmtResultado *r, FmtToken **tk, size_t *n)
{
    Compilacao *c = calloc(1, sizeof *c);
    Opcoes o = {.comando = "check", .entrada = "<fmt>"};
    Vec chaves;
    bool ok;

    if (c == NULL) FalhaInterna("memoria esgotada alocando a compilacao do fmt");
    if (!FonteAdotarTexto(&c->fonte, "<fmt>", r->normalizado.dados, r->normalizado.tam)) FalhaInterna("memoria esgotada copiando o texto do fmt");
    IniciarCompilacao(c);
    c->opcoes = &o;
    c->sem.lista.teto = OpcoesMaxErros(&o);
    VecIniciar(&chaves, sizeof(uint32_t));
    c->parser.chaves_literal = &chaves;
    ok = AnalisarSintaxe(c, false) && c->sem.lista.n == 0;
    if (!ok) {
        RegistrarErro(r, c);
    } else {
        *tk = calloc(c->lexer.tokens.n + 1, sizeof **tk);
        if (*tk == NULL) FalhaInterna("memoria esgotada nos tokens do fmt");
        ok = MontarTokens(&c->lexer.tokens, r->normalizado.dados, *tk, n);
        if (!ok) {
            r->estado = FMT_NAO_ANALISADO;
            snprintf(r->motivo, sizeof r->motivo, "token de 65535 bytes ou mais");
        } else {
            FmtAtribuirPapeis(*tk, *n, &chaves);
            FmtCalcularNiveis(*tk, *n);
        }
    }
    VecLiberar(&chaves);
    LiberarCompilacao(c);
    free(c);
    return ok;
}

/* ---------- edicoes ---------- */

static void Localizar(const Ctx *x, size_t off, uint32_t *linha, uint32_t *coluna)
{
    size_t lo = 0, hi = x->ninicios;

    while (hi - lo > 1) {
        size_t meio = (lo + hi) / 2;

        if (x->inicios[meio] <= off) lo = meio;
        else hi = meio;
    }
    *linha = (uint32_t)lo + 1;
    *coluna = (uint32_t)(off - x->inicios[lo]) + 1;
}

/* Uma troca: a faixa [ini, fim) do texto vira 'novo' (NUL no fim), pela regra 'regra'. */
typedef struct { size_t ini, fim; const char *novo, *regra; } Troca;

/* Uma Edicao; nada se o texto ja e igual. */
static void EmitirFaixa(Ctx *x, Troca t)
{
    Edicao *e;
    size_t nnovo = strlen(t.novo);
    uint32_t linha_fim, coluna_fim;

    if (t.fim - t.ini == nnovo && memcmp(x->txt + t.ini, t.novo, nnovo) == 0) return;
    if (t.ini == 0 && t.fim == x->tam && nnovo == 0) x->tudo_apagado = true;
    /* O motor de edicoes nao enxerga a "linha" vazia depois do ultimo '\n': a faixa que acaba no fim do texto recua 1 byte (apagar "\n\n" ou "\n" do fim da no mesmo texto). */
    if (t.fim == x->tam && t.ini > 0 && nnovo == 0 && x->txt[t.fim - 1] == '\n' && x->txt[t.ini - 1] == '\n') {
        t.ini--;
        t.fim--;
    }
    e = VecEmpurrar(&x->r->edicoes);
    Localizar(x, t.ini, &e->linha, &e->coluna);
    Localizar(x, t.fim, &linha_fim, &coluna_fim);
    e->coluna_fim = coluna_fim;
    e->linha_fim = linha_fim > e->linha ? linha_fim : 0;
    e->antigo = ArenaCopiarTexto(&x->r->arena, x->txt + t.ini, t.fim - t.ini);
    e->novo = ArenaCopiarTexto(&x->r->arena, t.novo, nnovo);
    *(const char **)VecEmpurrar(&x->r->regras) = t.regra;
}

static void Descarregar(Ctx *x)
{
    if (!x->pend) return;
    x->pend = false;
    EmitirFaixa(x, (Troca){x->pend_ini, x->pend_fim, "", x->pend_regra});
}

static void Emitir(Ctx *x, Troca t)
{
    Descarregar(x);
    EmitirFaixa(x, t);
}

/* Apaga [ini, fim); apagamentos encostados (mesma regra) viram uma Edicao so. */
static void Apagar(Ctx *x, size_t ini, size_t fim, const char *regra)
{
    if (ini == fim) return;
    if (x->pend && x->pend_fim == ini && strcmp(x->pend_regra, regra) == 0) {
        x->pend_fim = fim;
        return;
    }
    Descarregar(x);
    x->pend = true;
    x->pend_ini = ini;
    x->pend_fim = fim;
    x->pend_regra = regra;
}

/* A faixa [ini, fim) passa a ter 'nivel' * 4 espacos. */
static void Indentar(Ctx *x, size_t ini, size_t fim, uint32_t nivel)
{
    size_t quer = (size_t)nivel * INDENTACAO;
    char *espacos;

    if (fim - ini == quer) {
        size_t k = 0;

        while (k < quer && x->txt[ini + k] == ' ') k++;
        if (k == quer) return;
    }
    espacos = ArenaAlocar(&x->r->arena, quer + 1);
    memset(espacos, ' ', quer);
    Emitir(x, (Troca){ini, fim, espacos, "indentacao"});
}

/* ---------- vaos ---------- */

static bool CharDePalavra(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/* Dois tokens da mesma linha: espaco pela tabela; par fora da tabela mantem o vao (0 fica 0, 1 ou mais vira 1); duas palavras nunca ficam coladas. */
static void VaoSemQuebra(Ctx *x, const Vao *v)
{
    const FmtToken *a = &x->tk[v->b - 1], *b = &x->tk[v->b];
    int quer = FmtEspacoDoPar((FmtPapel)a->papel, (FmtPapel)b->papel);

    if (quer < 0) quer = v->g1 == v->g0 ? 0 : 1;
    if (quer == 0 && CharDePalavra(x->txt[a->fim - 1]) && CharDePalavra(x->txt[b->ini])) quer = 1;
    Emitir(x, (Troca){v->g0, v->g1, quer ? " " : "", "espaco"});
}

/* Tira os espacos do fim de um comentario [p, e) (p aponta o '//'). */
static void AparaComentario(Ctx *x, size_t p, size_t e)
{
    size_t q = e;

    while (q > p + 2 && EhEspaco(x->txt[q - 1])) q--;
    Emitir(x, (Troca){q, e, "", "espaco_final"});
}

static uint32_t NivelDoComentario(const Ctx *x, const Vao *v)
{
    if (!v->tem_b) return 0;
    return x->tk[v->b].nivel + (FmtPapelFecha((FmtPapel)x->tk[v->b].papel) ? 1 : 0);
}

static size_t ProximaQuebra(const Ctx *x, size_t s, size_t g1)
{
    const char *nl = memchr(x->txt + s, '\n', g1 - s);

    return nl != NULL ? (size_t)(nl - x->txt) : g1;
}

/* Onde termina a ultima linha de conteudo do vao (a linha do token de antes, ou uma so de comentario), no fim do arquivo: depois dela nada fica. */
static size_t FimDoConteudo(const Ctx *x, size_t g0, size_t g1, bool tem_a)
{
    size_t fim = g0, s = g0;
    bool primeira = true;

    for (;;) {
        size_t e = ProximaQuebra(x, s, g1), p = s;
        bool termina = e < g1;

        while (p < e && EhEspaco(x->txt[p])) p++;
        if (p < e || (primeira && tem_a)) fim = termina ? e + 1 : e;
        if (!termina) return fim;
        s = e + 1;
        primeira = false;
    }
}

/* Um segmento [s, e) do vao (o que fica entre dois '\n'): p = onde o '//' comeca (p == e: so espaco), termina = acaba num '\n', primeira = o do inicio do vao (a linha do token a). */
typedef struct { size_t s, e, p; bool termina, primeira; } Seg;

/* A ultima linha do vao (sem '\n' no fim): a indentacao do token b, ou, no fim do arquivo, o '\n' que falta. */
static void SegmentoFinal(Ctx *x, const Vao *v, const Seg *g)
{
    bool da_linha_de_a = g->primeira && v->tem_a;

    if (v->tem_b) {
        Indentar(x, g->s, g->e, x->tk[v->b].nivel);
        return;
    }
    if (g->p < g->e) {
        if (da_linha_de_a) Emitir(x, (Troca){g->s, g->p, " ", "comentario"});
        else Indentar(x, g->s, g->p, 0);
        AparaComentario(x, g->p, g->e);
        Emitir(x, (Troca){g->e, g->e, "\n", "fim_de_arquivo"});
    } else if (da_linha_de_a) {
        Emitir(x, (Troca){g->s, g->e, "\n", "fim_de_arquivo"});
    } else {
        Apagar(x, g->s, g->e, "fim_de_arquivo");
    }
}

/* Uma linha em branco terminada em '\n' ([s, e) e o que vem antes do '\n'): fica no maximo uma, e nenhuma no comeco nem no fim do arquivo. */
static void SegmentoEmBranco(Ctx *x, Vao *v, const Seg *g)
{
    if (!v->tem_b && g->s >= v->fim_conteudo) {
        Apagar(x, g->s, g->e + 1, "fim_de_arquivo");
    } else if (!v->conteudo) {
        Apagar(x, g->s, g->e + 1, "linha_em_branco");
    } else if (v->brancas == 0) {
        Emitir(x, (Troca){g->s, g->e, "", "espaco_final"});
        v->brancas = 1;
    } else {
        Apagar(x, g->s, g->e + 1, "linha_em_branco");
    }
}

/* O segmento e a linha do token a (terminada em '\n'): so o espaco antes do '//' final e o espaco no fim contam. */
static void SegmentoDaLinhaDeA(Ctx *x, Vao *v, const Seg *g)
{
    if (g->p < g->e) {
        Emitir(x, (Troca){g->s, g->p, " ", "comentario"});
        AparaComentario(x, g->p, g->e);
    } else {
        Emitir(x, (Troca){g->s, g->e, "", "espaco_final"});
    }
    v->conteudo = true;
}

/* g chega com s, e, termina e primeira; aqui se acha o p. */
static void Segmento(Ctx *x, Vao *v, Seg g)
{
    g.p = g.s;
    while (g.p < g.e && EhEspaco(x->txt[g.p])) g.p++;
    if (g.p < g.e && !(x->txt[g.p] == '/' && g.p + 1 < g.e && x->txt[g.p + 1] == '/')) {
        x->falhou = true;
    } else if (!g.termina) {
        SegmentoFinal(x, v, &g);
    } else if (g.primeira && v->tem_a) {
        SegmentoDaLinhaDeA(x, v, &g);
    } else if (g.p < g.e) {
        Indentar(x, g.s, g.p, NivelDoComentario(x, v));
        AparaComentario(x, g.p, g.e);
        v->conteudo = true;
        v->brancas = 0;
    } else {
        SegmentoEmBranco(x, v, &g);
    }
}

/* O vao antes do token i (i == n: o que sobra depois do ultimo). */
static void FmtVao(Ctx *x, size_t i)
{
    Vao v = {.b = i, .tem_a = i > 0, .tem_b = i < x->n, .fim_conteudo = SIZE_MAX};
    size_t s;
    bool primeira = true;

    v.g0 = v.tem_a ? x->tk[i - 1].fim : 0;
    v.g1 = v.tem_b ? x->tk[i].ini : x->tam;
    v.conteudo = v.tem_a;
    if (memchr(x->txt + v.g0, '\n', v.g1 - v.g0) == NULL && v.tem_b) {
        if (v.tem_a) VaoSemQuebra(x, &v);
        else Indentar(x, v.g0, v.g1, x->tk[0].nivel);
        return;
    }
    if (!v.tem_b) v.fim_conteudo = FimDoConteudo(x, v.g0, v.g1, v.tem_a);
    for (s = v.g0;;) {
        size_t e = ProximaQuebra(x, s, v.g1);
        bool termina = e < v.g1;

        Segmento(x, &v, (Seg){.s = s, .e = e, .termina = termina, .primeira = primeira});
        if (!termina || x->falhou) return;
        s = e + 1;
        primeira = false;
    }
}

/* ---------- resultado ---------- */

static void MarcarPrimeiroDesvio(FmtResultado *r, PosCrlf crlf)
{
    const Edicao *edicoes = r->edicoes.dados;
    const char *const *regras = r->regras.dados;

    if (r->tinha_bom) {
        r->primeiro = (FmtDesvio){1, 1, "bom"};
        return;
    }
    if (r->edicoes.n > 0) r->primeiro = (FmtDesvio){edicoes[0].linha, edicoes[0].coluna, regras[0]};
    if (r->tinha_crlf && (r->edicoes.n == 0 || crlf.linha < edicoes[0].linha || (crlf.linha == edicoes[0].linha && crlf.coluna <= edicoes[0].coluna)))
        r->primeiro = (FmtDesvio){crlf.linha, crlf.coluna, "fim_de_linha"};
}

/* Aplica as Edicoes ao texto normalizado e fecha o resultado. */
static void Concluir(Ctx *x, PosCrlf crlf)
{
    FmtResultado *r = x->r;
    EdicaoStatus st = EDICAO_OK;

    if (x->falhou) {
        r->estado = FMT_NAO_ANALISADO;
        snprintf(r->motivo, sizeof r->motivo, "texto inesperado entre dois tokens");
        return;
    }
    if (!x->tudo_apagado) st = EdicoesAplicar(x->txt, x->tam, r->edicoes.dados, (uint32_t)r->edicoes.n, &r->formatado, NULL);
    if (st != EDICAO_OK) {
        r->estado = FMT_NAO_ANALISADO;
        snprintf(r->motivo, sizeof r->motivo, "edicao interna invalida: %s", EdicaoStatusTexto(st));
        return;
    }
    r->desvios = (uint32_t)r->edicoes.n + (r->tinha_bom ? 1u : 0u) + (r->tinha_crlf ? 1u : 0u);
    r->estado = r->desvios == 0 ? FMT_CANONICO : FMT_MUDARIA;
    MarcarPrimeiroDesvio(r, crlf);
}

/* inicios[k] = offset do 1o byte da linha k + 1. */
static void MontarInicios(Ctx *x)
{
    size_t k = 1;

    x->inicios = malloc((x->tam + 2) * sizeof *x->inicios);
    if (x->inicios == NULL) FalhaInterna("memoria esgotada na tabela de linhas do fmt");
    x->inicios[0] = 0;
    for (size_t i = 0; i < x->tam; i++)
        if (x->txt[i] == '\n') x->inicios[k++] = (uint32_t)(i + 1);
    x->ninicios = k;
}

void FmtFormatar(const char *fonte, size_t tam, FmtResultado *r)
{
    PosCrlf crlf = {0};
    Ctx x = {.r = r};

    VecIniciar(&r->edicoes, sizeof(Edicao));
    VecIniciar(&r->regras, sizeof(const char *));
    Normalizar(r, fonte, tam, &crlf);
    x.txt = r->normalizado.dados;
    x.tam = r->normalizado.tam;
    if (LerTokens(r, &x.tk, &x.n)) {
        MontarInicios(&x);
        for (size_t i = 0; i <= x.n && !x.falhou; i++) FmtVao(&x, i);
        Descarregar(&x);
        Concluir(&x, crlf);
    }
    free(x.tk);
    free(x.inicios);
}

void FmtResultadoLiberar(FmtResultado *r)
{
    BufLiberar(&r->normalizado);
    BufLiberar(&r->formatado);
    VecLiberar(&r->edicoes);
    VecLiberar(&r->regras);
    ArenaLiberar(&r->arena);
}
