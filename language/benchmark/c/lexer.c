/* lexer.c - tokeniza o arquivo .fj inteiro (E0001 no primeiro erro).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"

#include <stdio.h>
#include <stdlib.h>

static bool IsDigit(char ch) { return ch >= '0' && ch <= '9'; }
static bool IsIdentStart(char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'; }
static bool IsIdentPart(char ch) { return IsIdentStart(ch) || IsDigit(ch); }

static bool PushToken(Ctx *c, TokKind kind, uint32_t off, uint32_t len, int32_t sym)
{
    if (c->ntoks == c->captoks) {
        uint32_t cap = c->captoks ? c->captoks * 2 : 4096;
        Token *novo = realloc(c->toks, (size_t)cap * sizeof(Token));
        if (novo == NULL) {
            fprintf(stderr, "forja: memoria esgotada com %u tokens\n", c->ntoks);
            return false;
        }
        c->toks = novo;
        c->captoks = cap;
    }
    Token *t = &c->toks[c->ntoks++];
    t->off = off;
    t->len = (uint16_t)(len > UINT16_MAX ? UINT16_MAX : len);
    t->sym = sym;
    t->kind = (uint8_t)kind;
    return true;
}

/* Quebra de linha vira 1 token NL; linhas em branco e comentarios colapsam no primeiro. */
static bool PushNewline(Ctx *c, uint32_t off)
{
    if (c->ntoks == 0 || c->toks[c->ntoks - 1].kind == TK_NL) return true;
    return PushToken(c, TK_NL, off, 1, -1);
}

static uint32_t LexNumber(Ctx *c, uint32_t i, bool *is_real)
{
    const char *s = c->src;
    uint32_t j = i;
    while (IsDigit(s[j])) j++;
    *is_real = false;
    if (s[j] == '.' && IsDigit(s[j + 1])) {
        *is_real = true;
        j++;
        while (IsDigit(s[j])) j++;
    }
    return j;
}

/* Devolve o comprimento do simbolo em s[i], ou 0 se nao for simbolo valido. */
static uint32_t MatchSymbol(const char *s, TokKind *kind)
{
    char a = s[0], b = s[1];
    switch (a) {
    case '(': *kind = TK_LPAREN; return 1;
    case ')': *kind = TK_RPAREN; return 1;
    case '{': *kind = TK_LBRACE; return 1;
    case '}': *kind = TK_RBRACE; return 1;
    case ',': *kind = TK_COMMA; return 1;
    case ':': *kind = TK_COLON; return 1;
    case '*': *kind = TK_STAR; return 1;
    case '/': *kind = TK_SLASH; return 1;
    case '%': *kind = TK_PERCENT; return 1;
    case '^': *kind = TK_CARET; return 1;
    case '&': *kind = TK_AMP; return 1;
    case '=': if (b == '=') { *kind = TK_EQ; return 2; } *kind = TK_ASSIGN; return 1;
    case '!': if (b == '=') { *kind = TK_NE; return 2; } return 0;
    case '<': if (b == '=') { *kind = TK_LE; return 2; } *kind = TK_LT; return 1;
    case '>': if (b == '=') { *kind = TK_GE; return 2; } *kind = TK_GT; return 1;
    case '+': if (b == '=') { *kind = TK_PLUSEQ; return 2; } *kind = TK_PLUS; return 1;
    case '-':
        if (b == '=') { *kind = TK_MINUSEQ; return 2; }
        if (b == '>') { *kind = TK_ARROW; return 2; }
        *kind = TK_MINUS; return 1;
    case '.':
        if (b == '.' && s[2] == '<') { *kind = TK_DOTDOTLT; return 3; }
        if (b == '.' && s[2] == '=') { *kind = TK_DOTDOTEQ; return 3; }
        if (b == '.') { *kind = TK_DOTDOT; return 2; }
        *kind = TK_DOT; return 1;
    default: return 0;
    }
}

static bool LexString(Ctx *c, uint32_t *i)
{
    const char *s = c->src;
    uint32_t j = *i + 1;
    while (s[j] != '"' && s[j] != '\n' && j < c->src_len) j++;
    if (s[j] != '"') return Report(c, E0001, *i, "string sem fechar na linha");
    if (!PushToken(c, TK_STR, *i, j + 1 - *i, -1)) return false;
    *i = j + 1;
    return true;
}

static bool LexIdent(Ctx *c, uint32_t *i)
{
    const char *s = c->src;
    uint32_t j = *i + 1;
    while (IsIdentPart(s[j])) j++;
    int32_t sym = InternSym(c, s + *i, j - *i);
    if (sym < 0) return false;
    TokKind kind = sym < KEYWORD_COUNT ? (TokKind)(KW_CONST + sym) : TK_IDENT;
    if (!PushToken(c, kind, *i, j - *i, sym)) return false;
    *i = j;
    return true;
}

static bool LexInvalidChar(Ctx *c, uint32_t i)
{
    unsigned char ch = (unsigned char)c->src[i];
    if (ch >= 0x21 && ch < 0x7F) return Report(c, E0001, i, "caractere invalido '%c'", ch);
    return Report(c, E0001, i, "caractere invalido (byte 0x%02X)", ch);
}

/* Trata um token que comeca em s[*i]; avanca *i. */
static bool LexOne(Ctx *c, uint32_t *i)
{
    char ch = c->src[*i];
    TokKind kind;
    if (IsIdentStart(ch)) return LexIdent(c, i);
    if (ch == '"') return LexString(c, i);
    if (IsDigit(ch)) {
        bool is_real;
        uint32_t fim = LexNumber(c, *i, &is_real);
        if (!PushToken(c, is_real ? TK_REAL : TK_INT, *i, fim - *i, -1)) return false;
        *i = fim;
        return true;
    }
    uint32_t n = MatchSymbol(c->src + *i, &kind);
    if (n == 0) return LexInvalidChar(c, *i);
    if (!PushToken(c, kind, *i, n, -1)) return false;
    *i += n;
    return true;
}

bool LexFile(Ctx *c)
{
    const char *s = c->src;
    uint32_t i = 0, n = (uint32_t)c->src_len;
    /* ~1 token a cada 3 bytes no corpus gerado: evita varias copias do vetor ao crescer. */
    c->toks = malloc(((size_t)n / 3 + 64) * sizeof(Token));
    if (c->toks == NULL) return false;
    c->captoks = n / 3 + 64;
    while (i < n) {
        char ch = s[i];
        if (ch == '\n') {
            if (!PushNewline(c, i)) return false;
            i++;
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            i++;
        } else if (ch == '/' && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') i++;
        } else if (!LexOne(c, &i)) {
            return false;
        }
    }
    return PushToken(c, TK_EOF, n, 0, -1);
}
