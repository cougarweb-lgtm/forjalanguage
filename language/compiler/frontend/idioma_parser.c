/* idioma_parser.c - o parser reconhece os idiomas de outras linguagens da parte sintatica da tabela do contrato IA1 (3.4) e os recusa com E0110
 * ("forma de outra linguagem; na Forja e X") e a sugestao aplicavel, em vez do E0101 generico. A Forja NAO aceita nenhum deles: o parser so troca o erro.
 * Textos e ids na tabela de common/idiomas.c; o lexer cuida de ';' e do comentario de bloco (lexer.c). Entradas: ParserIdioma (chamada por ErroSintaxe, com o que o
 * parser esperava), ParserIdiomaOperador ('&&' e '||'), ParserIdiomaNegacao ('!'), ParserIdiomaTryBloco ('try {'), ParserIdiomaMain ('proc main() -> int'),
 * ParserIdiomaArray ('[1, 2]') e ParserIdiomaImport ('import os'); 'throw x' e 'raise x' entram por ParserIdioma. Os idiomas que dependem de saber o que o nome
 * declara (nil, range, string, x.len()...) ficam em semantic/sem_idioma.c.
 * Versao: 0.2.2
 * Changelog: 0.2.2 - O0 P13: ParserIdiomaXor ('a ^ b' vira E0110 com a troca por '~').
 *            0.2.1 - IA1-L3c: 'import file' (e 'import arquivo', antigo) sugerem "std:file".
 *            0.2.0 - IA1-L2: throw/raise, '[1, 2]' (com a troca aplicavel em let/var) e 'import os' sem aspas viram E0110.
 *            0.1.0 - IA1-L1: primeira versao.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_e0110_l1.py --forja compiler\forja.exe && python ..\tests\ia_first\verificar_m_d4.py --forja compiler\forja.exe
 */
#include "frontend/parser_interno.h"

#include "common/idiomas.h"

#include <string.h>

/* O idioma achado: onde, o que estava escrito, e a sugestao (palavra != NULL: troca [ini, fim) pela palavra, certa; senao so a dica). */
typedef struct { IdiomaId id; uint32_t ini, fim; const char *achado, *palavra, *dica; } Achado;

enum { NAO_ACHEI = UINT32_MAX, RECUO_MAX = 20 };

static bool Relatar(Parser *p, const Achado *a)
{
    IdiomaRelatar(p->diag, a->id, a->ini, a->achado);
    if (a->palavra != NULL) IdiomaSugerirPalavra(p->diag, p->fonte, a->ini, a->fim, a->palavra);
    else DiagSugerir(&p->diag->sug, DIAG_SO_DICA, "%s", a->dica);
    return true;
}

/* O token k encosta no seguinte (sem espaco entre os dois): '&' '&', '+' '+', ':' '='. So vale com k antes do EOF. */
static bool Colados(const Parser *p, uint32_t k)
{
    return p->toks[k].off + p->toks[k].len == p->toks[k + 1].off;
}

static bool FimDeLinha(TokenTipo tipo)
{
    return tipo == TK_NL || tipo == TK_EOF;
}

/* O nome (IDENT) do token k e exatamente um destes. 'palavras' termina em NULL. */
static bool NomeEntre(const Parser *p, uint32_t k, const char *const *palavras)
{
    const Token *t = &p->toks[k];

    if (t->tipo != TK_IDENT) return false;
    for (; *palavras != NULL; palavras++) {
        if (strlen(*palavras) == t->len && memcmp(p->fonte->texto + t->off, *palavras, t->len) == 0) return true;
    }
    return false;
}

/* ---------- '&&', '||' e '!' ---------- */

bool ParserIdiomaOperador(Parser *p)
{
    TokenTipo t = TipoAtual(p);
    uint32_t off = TokenAtual(p)->off;
    bool e_logico = t == TK_AMP;

    if (TipoSeguinte(p) != t || !Colados(p, p->cur)) return false;
    return Relatar(p, &(Achado){e_logico ? IDI_E_LOGICO : IDI_OU_LOGICO, off, off + 2, e_logico ? "&&" : "||", e_logico ? "and" : "or", NULL});
}

void ParserIdiomaNegacao(Parser *p)
{
    uint32_t off = TokenAtual(p)->off;

    Relatar(p, &(Achado){IDI_NEGACAO, off, off + 1, "!", "not", NULL});
}

/* '^' pos-fixo seguido, na mesma linha, de nome ou numero (a ^ b, h ^ 0xFF): so pode ser o xor de C; p^.campo, p^[i], p^ + 1 e p^ sozinho nao entram. */
bool ParserIdiomaXor(Parser *p)
{
    TokenTipo seguinte = TipoSeguinte(p);
    uint32_t off = TokenAtual(p)->off;

    if (seguinte != TK_IDENT && seguinte != TK_INT && seguinte != TK_REAL) return false;
    return Relatar(p, &(Achado){IDI_XOR_CIRCUNFLEXO, off, off + 1, "^", "~", NULL});
}

/* ---------- i++ e i-- ---------- */

static bool DetectarIncremento(Parser *p, const char *esperado)
{
    TokenTipo t = TipoAtual(p);
    uint32_t off = TokenAtual(p)->off;
    bool prefixo = strcmp(esperado, "comando") == 0;

    if ((t != TK_PLUS && t != TK_MINUS) || TipoSeguinte(p) != t || !Colados(p, p->cur)) return false;
    if (!prefixo && strcmp(esperado, "'=', '+=' ou '-='") != 0) return false;
    if (prefixo || !FimDeLinha((TokenTipo)p->toks[p->cur + 2].tipo))
        return Relatar(p, &(Achado){IDI_INCREMENTO, off, off + 2, t == TK_PLUS ? "++" : "--", NULL,
                                    "troque ++ por += 1 (e -- por -= 1) como comando proprio, depois do nome da variavel"});
    return Relatar(p, &(Achado){IDI_INCREMENTO, off, off + 2, t == TK_PLUS ? "++" : "--", t == TK_PLUS ? "+= 1" : "-= 1", NULL});
}

/* ---------- 'x?' (Rust) e 'c ? a : b' ---------- */

/* Indice do '(' ou '[' que casa com o fecha-parenteses/colchete em 'fecha' (na mesma linha), ou NAO_ACHEI. */
static uint32_t AbridorDe(const Parser *p, uint32_t fecha)
{
    TokenTipo abre = p->toks[fecha].tipo == TK_RPAREN ? TK_LPAREN : TK_LCOLCHETE, fecho = (TokenTipo)p->toks[fecha].tipo;
    int32_t fundo = 0;

    for (uint32_t k = fecha + 1; k > 0; k--) {
        TokenTipo t = (TokenTipo)p->toks[k - 1].tipo;
        if (t == TK_NL || t == TK_EOF) return NAO_ACHEI;
        if (t == fecho) fundo++;
        if (t == abre && --fundo == 0) return k - 1;
    }
    return NAO_ACHEI;
}

/* Inicio da cadeia nome(.nome | (..) | [..] | ^)* que termina logo antes do token atual; NAO_ACHEI se a expressao nao e uma cadeia. */
static uint32_t InicioDaCadeia(const Parser *p)
{
    uint32_t k = p->cur;

    while (k > 0) {
        TokenTipo t = (TokenTipo)p->toks[k - 1].tipo;
        if (t == TK_RPAREN || t == TK_RCOLCHETE) {
            k = AbridorDe(p, k - 1);
            if (k == NAO_ACHEI) return NAO_ACHEI;
        } else if (t == TK_CARET) {
            k--;
        } else if (t == TK_IDENT) {
            k--;
            if (k == 0 || p->toks[k - 1].tipo != TK_DOT) return k;
            k--;
        } else {
            return NAO_ACHEI;
        }
    }
    return NAO_ACHEI;
}

static bool FechaExpressao(TokenTipo t)
{
    return FimDeLinha(t) || t == TK_RPAREN || t == TK_COMMA || t == TK_RBRACE || t == TK_RCOLCHETE;
}

/* O '?' atual fecha um 'x?' de Rust: certa quando a cadeia anterior e clara (try entra antes dela e o '?' sai). */
static bool RelatarTryDeRust(Parser *p)
{
    uint32_t off = TokenAtual(p)->off, inicio = InicioDaCadeia(p);

    IdiomaRelatar(p->diag, IDI_TRY_RUST, off, "?");
    if (inicio == NAO_ACHEI) {
        DiagSugerir(&p->diag->sug, DIAG_SO_DICA, "tire o '?' e escreva try antes da expressao");
        return true;
    }
    DiagSugerir(&p->diag->sug, DIAG_CERTA, "escreva try antes da expressao e tire o '?'");
    DiagEditar(&p->diag->sug, p->toks[inicio].off, p->toks[inicio].off, "try ");
    DiagEditar(&p->diag->sug, off, off + 1, "");
    return true;
}

static bool DetectarInterrogacao(Parser *p, const char *esperado)
{
    (void)esperado;
    if (TipoAtual(p) != TK_INTERROGA || p->cur == 0 || p->toks[p->cur - 1].tipo == TK_NL) return false;
    if (FechaExpressao((TokenTipo)p->toks[p->cur + 1].tipo)) return RelatarTryDeRust(p);
    for (uint32_t k = p->cur + 1; !FimDeLinha((TokenTipo)p->toks[k].tipo); k++) {
        if (p->toks[k].tipo != TK_COLON) continue;
        return Relatar(p, &(Achado){IDI_TERNARIO, TokenAtual(p)->off, 0, "?", NULL,
                                    "troque o 'c ? a : b' por um if com else que atribui a variavel"});
    }
    return false;
}

/* ---------- ':=' ---------- */

static bool DetectarAtribuicaoCurta(Parser *p, const char *esperado)
{
    uint32_t off = TokenAtual(p)->off, inicio = p->cur;

    if (TipoAtual(p) != TK_COLON || TipoSeguinte(p) != TK_ASSIGN || !Colados(p, p->cur)) return false;
    if (strcmp(esperado, "'=', '+=' ou '-='") != 0) return false;
    while (inicio > 0 && p->toks[inicio - 1].tipo != TK_NL) inicio--;
    IdiomaRelatar(p->diag, IDI_ATRIBUICAO_CURTA, off, ":=");
    if (inicio + 1 != p->cur || p->toks[inicio].tipo != TK_IDENT) {
        DiagSugerir(&p->diag->sug, DIAG_SO_DICA, "escreva let antes do nome e troque ':=' por '='");
        return true;
    }
    DiagSugerir(&p->diag->sug, DIAG_CERTA, "escreva let antes do nome e troque ':=' por '='");
    DiagEditar(&p->diag->sug, p->toks[inicio].off, p->toks[inicio].off, "let ");
    DiagEditar(&p->diag->sug, off, off + 2, "=");
    return true;
}

/* ---------- elif, fn/func/def/function, for ( ---------- */

static bool DetectarElif(Parser *p, const char *esperado)
{
    static const char *const PALAVRAS[] = {"elif", "elsif", "elseif", NULL};
    const Token *t = TokenAtual(p);

    if (strcmp(esperado, "fim de linha") != 0 || p->cur == 0 || p->toks[p->cur - 1].tipo != TK_RBRACE || !NomeEntre(p, p->cur, PALAVRAS)) return false;
    char achado[16];
    snprintf(achado, sizeof achado, "%.*s", (int)t->len, p->fonte->texto + t->off);
    return Relatar(p, &(Achado){IDI_ELIF, t->off, t->off + t->len, achado, "else if", NULL});
}

static bool DetectarPalavraDeProc(Parser *p, const char *esperado)
{
    static const char *const PALAVRAS[] = {"fn", "func", "def", "function", "fun", NULL};
    const Token *t = TokenAtual(p);

    if (strncmp(esperado, "declaracao", 10) != 0 || !NomeEntre(p, p->cur, PALAVRAS)) return false;
    if (p->toks[p->cur + 1].tipo != TK_IDENT || p->toks[p->cur + 2].tipo != TK_LPAREN) return false;
    char achado[16];
    snprintf(achado, sizeof achado, "%.*s", (int)t->len, p->fonte->texto + t->off);
    return Relatar(p, &(Achado){IDI_PALAVRA_DE_PROC, t->off, t->off + t->len, achado, "proc", NULL});
}

static bool DetectarForC(Parser *p, const char *esperado)
{
    if (strcmp(esperado, "nome da variavel do laco") != 0 || TipoAtual(p) != TK_LPAREN || p->cur == 0 || p->toks[p->cur - 1].tipo != KW_FOR) return false;
    return Relatar(p, &(Achado){IDI_FOR_ESTILO_C, p->toks[p->cur - 1].off, 0, "for (...)", NULL,
                                "troque o cabecalho pelo for da Forja: for i in 0..<n { ou for item in lista {"});
}

/* ---------- bloco de um comando na mesma linha: '{' comando '}' ---------- */

/* Indice do '}' que fecha o '{' anterior ao token atual, se esta na mesma linha e e o ultimo token dela; NAO_ACHEI senao (else depois, '{' dentro, varias linhas). */
static uint32_t FechoNaLinha(const Parser *p)
{
    int32_t fundo = 1;

    for (uint32_t k = p->cur; !FimDeLinha((TokenTipo)p->toks[k].tipo); k++) {
        if (p->toks[k].tipo == TK_LBRACE) return NAO_ACHEI;
        if (p->toks[k].tipo == TK_RBRACE && --fundo == 0) return FimDeLinha((TokenTipo)p->toks[k + 1].tipo) ? k : NAO_ACHEI;
    }
    return NAO_ACHEI;
}

/* Recuo (espacos e tabs) da linha que tem o offset 'off', em 'recuo'; false se passa de RECUO_MAX. */
static bool RecuoDaLinha(const Fonte *f, uint32_t off, char *recuo)
{
    uint32_t k = off, n = 0;

    while (k > 0 && f->texto[k - 1] != '\n') k--;
    for (; f->texto[k] == ' ' || f->texto[k] == '\t'; k++) {
        if (n == RECUO_MAX) return false;
        recuo[n++] = f->texto[k];
    }
    recuo[n] = '\0';
    return true;
}

/* Quebra a linha em duas edicoes: depois do '{' e antes do '}'. O fim de linha acompanha o arquivo (CRLF se a linha termina em \r). */
static void SugerirQuebraDoBloco(Parser *p, uint32_t abre, uint32_t fecha)
{
    const Fonte *f = p->fonte;
    char recuo[RECUO_MAX + 1], novo[DIAG_NOVO_MAX];
    uint32_t corpo = p->toks[p->cur].off, fim_espaco = p->toks[fecha].off, k = fim_espaco;
    const char *eol = "\n";

    if (!RecuoDaLinha(f, p->toks[abre].off, recuo)) {
        DiagSugerir(&p->diag->sug, DIAG_SO_DICA, "ponha o corpo em linhas proprias, depois do '{' e antes do '}'");
        return;
    }
    while (f->texto[k] != '\n' && f->texto[k] != '\0') k++;
    if (k > 0 && f->texto[k - 1] == '\r') eol = "\r\n";
    DiagSugerir(&p->diag->sug, DIAG_CERTA, "ponha o corpo em linhas proprias, depois do '{' e antes do '}'");
    snprintf(novo, sizeof novo, "%s%s    ", eol, recuo);
    DiagEditar(&p->diag->sug, p->toks[abre].off + 1, corpo, novo);
    while (fim_espaco > corpo && (f->texto[fim_espaco - 1] == ' ' || f->texto[fim_espaco - 1] == '\t')) fim_espaco--;
    snprintf(novo, sizeof novo, "%s%s", eol, recuo);
    DiagEditar(&p->diag->sug, fim_espaco, p->toks[fecha].off, novo);
}

static bool DetectarBlocoNaLinha(Parser *p, const char *esperado)
{
    uint32_t abre = p->cur - 1, fecha;

    if (strcmp(esperado, "fim de linha depois de '{'") != 0 || p->cur == 0 || p->toks[abre].tipo != TK_LBRACE) return false;
    fecha = FechoNaLinha(p);
    IdiomaRelatar(p->diag, IDI_BLOCO_NA_LINHA, p->toks[abre].off, "{");
    if (fecha == NAO_ACHEI) DiagSugerir(&p->diag->sug, DIAG_SO_DICA, "ponha o corpo em linhas proprias, depois do '{' e antes do '}'");
    else SugerirQuebraDoBloco(p, abre, fecha);
    return true;
}

/* ---------- throw e raise ---------- */

static bool DetectarThrow(Parser *p, const char *esperado)
{
    static const char *const PALAVRAS[] = {"throw", "raise", NULL};
    const Token *t = p->cur > 0 ? &p->toks[p->cur - 1] : NULL;
    char achado[16];
    bool sozinha = p->cur >= 1 && (p->cur == 1 || p->toks[p->cur - 2].tipo == TK_NL || p->toks[p->cur - 2].tipo == TK_LBRACE);
    bool membro = TipoAtual(p) == TK_IDENT && p->toks[p->cur + 1].tipo == TK_DOT && p->toks[p->cur + 2].tipo == TK_IDENT &&
                  FimDeLinha((TokenTipo)p->toks[p->cur + 3].tipo);

    if (strcmp(esperado, "'=', '+=' ou '-='") != 0 || !sozinha || !NomeEntre(p, p->cur - 1, PALAVRAS)) return false;
    snprintf(achado, sizeof achado, "%.*s", (int)t->len, p->fonte->texto + t->off);
    if (!membro) {
        return Relatar(p, &(Achado){IDI_THROW, t->off, 0, achado, NULL, "troque a palavra por return e devolva um membro do enum de erro (return Erro.falhou)"});
    }
    return Relatar(p, &(Achado){IDI_THROW, t->off, t->off + t->len, achado, "return", NULL});
}

/* ---------- import os (sem aspas, como em Python) ---------- */

bool ParserIdiomaImport(Parser *p)
{
    static const char *const STD[] = {"os", "mem", "file", "arquivo", NULL};
    const Token *t = TokenAtual(p);
    char achado[16], novo[24];

    if (!NomeEntre(p, p->cur, STD) || !FimDeLinha((TokenTipo)p->toks[p->cur + 1].tipo)) return false;
    snprintf(achado, sizeof achado, "%.*s", (int)t->len, p->fonte->texto + t->off);
    snprintf(novo, sizeof novo, "\"std:%s\"", strcmp(achado, "arquivo") == 0 ? "file" : achado); /* L3c: std:arquivo virou std:file */
    return Relatar(p, &(Achado){IDI_IMPORT_SEM_STD, t->off, t->off + t->len, achado, novo, NULL});
}

/* ---------- [1, 2]: literal de array de Python, JS e Rust ---------- */

/* O que o '[' atual abre, lido na linha: 'fecha' = o ']' que casa (NAO_ACHEI = nao fecha na linha); 'elementos' = quantos itens; 'tipo' = o tipo comum dos
 * literais ("int", "f32", "str", "bool") ou NULL se os itens nao sao so literais iguais. */
typedef struct { uint32_t fecha, elementos; const char *tipo; } Colchetes;

static const char *TipoDoLiteral(TokenTipo t)
{
    switch (t) {
    case TK_INT: return "int";
    case TK_REAL: return "f32";
    case TK_TEXTO: return "str";
    case KW_TRUE: case KW_FALSE: return "bool";
    default: return NULL;
    }
}

/* Soma o token k (nivel 0) ao que se sabe dos itens: virgula conta item; literal fixa o tipo; '-' de numero negativo passa; o resto desfaz o tipo. */
static void ContarItem(const Parser *p, uint32_t k, Colchetes *c, bool *misto)
{
    TokenTipo t = (TokenTipo)p->toks[k].tipo;
    const char *tipo = TipoDoLiteral(t);

    if (t == TK_COMMA) c->elementos++;
    else if (t == TK_MINUS) return;
    else if (tipo == NULL || (c->tipo != NULL && strcmp(c->tipo, tipo) != 0)) *misto = true;
    else c->tipo = tipo;
}

/* +1 ao abrir '[' '(' '{', -1 ao fechar, 0 nos demais. */
static int32_t VariacaoDeNivel(TokenTipo t)
{
    if (t == TK_LCOLCHETE || t == TK_LPAREN || t == TK_LBRACE) return 1;
    if (t == TK_RCOLCHETE || t == TK_RPAREN || t == TK_RBRACE) return -1;
    return 0;
}

/* Fecha a contagem: n virgulas = n + 1 itens ('[]' = 0 itens); virgula no fim ou item que nao e literal desfaz o tipo. */
static void FecharContagem(const Parser *p, Colchetes *c, bool misto)
{
    bool vazio = c->fecha == p->cur + 1;

    if (!vazio) c->elementos++;
    if (misto || (!vazio && p->toks[c->fecha - 1].tipo == TK_COMMA)) c->tipo = NULL;
}

static Colchetes LerColchetes(const Parser *p)
{
    Colchetes c = {NAO_ACHEI, 0, NULL};
    int32_t fundo = 0;
    bool misto = false;

    for (uint32_t k = p->cur + 1; !FimDeLinha((TokenTipo)p->toks[k].tipo); k++) {
        TokenTipo t = (TokenTipo)p->toks[k].tipo;
        int32_t variacao = VariacaoDeNivel(t);
        if (t == TK_RCOLCHETE && fundo == 0) {
            c.fecha = k;
            FecharContagem(p, &c, misto);
            return c;
        }
        fundo += variacao;
        if (fundo > 0 || variacao != 0) misto = true;
        else ContarItem(p, k, &c, &misto);
    }
    return c;
}

/* Indice do 1o token da linha do token atual. */
static uint32_t InicioDaLinha(const Parser *p)
{
    uint32_t k = p->cur;

    while (k > 0 && p->toks[k - 1].tipo != TK_NL) k--;
    return k;
}

/* 'let|var nome = [..]' (sem tipo) e 'let|var nome: [N]T = [..]' (com array fixo): as duas formas em que a troca fecha sozinha. */
static void SugerirArray(Parser *p, const Colchetes *c)
{
    uint32_t ini = InicioDaLinha(p), abre = p->cur;
    const Token *nome = &p->toks[ini + 1];
    bool decl = p->toks[ini].tipo == KW_LET || p->toks[ini].tipo == KW_VAR;
    bool sem_tipo = decl && nome->tipo == TK_IDENT && ini + 2 == abre - 1 && p->toks[abre - 1].tipo == TK_ASSIGN;
    bool com_tipo = decl && nome->tipo == TK_IDENT && p->toks[ini + 2].tipo == TK_COLON && p->toks[ini + 3].tipo == TK_LCOLCHETE &&
                    p->toks[ini + 4].tipo == TK_INT && p->toks[abre - 1].tipo == TK_ASSIGN;
    bool fim_da_linha = c->fecha != NAO_ACHEI && FimDeLinha((TokenTipo)p->toks[c->fecha + 1].tipo);
    IdiomaTroca troca = IDIOMA_TROCA_VAZIA;
    char novo[DIAG_NOVO_MAX];

    if (fim_da_linha && sem_tipo && c->tipo != NULL && snprintf(novo, sizeof novo, ": [%u]%s = {", c->elementos, c->tipo) < (int)sizeof novo) {
        IdiomaTrocaSomar(&troca, p->fonte->texto, nome->off + nome->len, p->toks[abre].off + 1, novo);
        IdiomaTrocaSomar(&troca, p->fonte->texto, p->toks[c->fecha].off, p->toks[c->fecha].off + 1, "}");
    } else if (fim_da_linha && com_tipo) {
        IdiomaTrocaSomar(&troca, p->fonte->texto, p->toks[abre].off, p->toks[abre].off + 1, "{");
        IdiomaTrocaSomar(&troca, p->fonte->texto, p->toks[c->fecha].off, p->toks[c->fecha].off + 1, "}");
    }
    IdiomaSugerir(p->diag, DIAG_CERTA, "troque os colchetes por chaves e declare o tipo: let a: [N]T = {a, b}", &troca);
}

void ParserIdiomaArray(Parser *p)
{
    Colchetes c = LerColchetes(p);

    IdiomaRelatar(p->diag, IDI_ARRAY_COLCHETE, TokenAtual(p)->off, "[a, b]");
    SugerirArray(p, &c);
}

/* ---------- despacho ---------- */

typedef bool (*Detector)(Parser *p, const char *esperado);

static const Detector DETECTORES[] = {
    DetectarIncremento, DetectarInterrogacao, DetectarAtribuicaoCurta, DetectarElif, DetectarPalavraDeProc, DetectarForC, DetectarBlocoNaLinha, DetectarThrow,
};

bool ParserIdioma(Parser *p, const char *esperado)
{
    for (size_t i = 0; i < sizeof DETECTORES / sizeof DETECTORES[0]; i++) {
        if (DETECTORES[i](p, esperado)) return true;
    }
    return false;
}

/* ---------- try { } catch e proc main() -> int ---------- */

void ParserIdiomaTryBloco(Parser *p)
{
    Relatar(p, &(Achado){IDI_TRY_CATCH, p->toks[p->cur - 1].off, 0, "try { } catch", NULL,
                         "chame o proc numa linha e trate o resultado com switch r in f() { case T: ... case E: ... } (ou try / or_else)"});
}

static bool EhTipoInteiro(int32_t sym)
{
    return sym == SYM_INT || sym == SYM_I32 || sym == SYM_I8 || sym == SYM_I16 || sym == SYM_I64 || sym == SYM_U8 || sym == SYM_U16 || sym == SYM_U32 ||
           sym == SYM_U64 || sym == SYM_UINT;
}

bool ParserIdiomaMain(Parser *p, const Decl *d)
{
    const Token *tipo = &p->toks[p->cur + 1];
    char achado[40];

    if (d->nome != SYM_MAIN || d->nparams != 0 || TipoAtual(p) != TK_ARROW || tipo->tipo != TK_IDENT || !EhTipoInteiro(tipo->sym)) return false;
    snprintf(achado, sizeof achado, "-> %.*s", (int)tipo->len, p->fonte->texto + tipo->off);
    return Relatar(p, &(Achado){IDI_MAIN_COM_RETORNO, TokenAtual(p)->off, 0, achado, NULL,
                                "tire '-> tipo', acrescente import \"std:os\" e termine com os.exit(n) onde o codigo de saida for n"});
}
