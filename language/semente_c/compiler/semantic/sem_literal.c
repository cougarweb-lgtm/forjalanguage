/* sem_literal.c - literal inteiro em contexto real (K2/A15, E0330) e a sugestao 'expr as Tipo' do E0301.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-b07: SemConversaoObvia (o texto 'expr as Tipo' e o trecho que ele troca, usado pela sugestao e pela forma do E0301); SemSugerirConversao passa a usa-la (mesma saida).
 *            0.1.0 - Linguagem v1.11 L1: primeira versao (SemLiteralInteiroEmReal, SemSugerirConversao).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include <ctype.h>

enum {
    DIGITOS_SEMPRE_EXATOS = 7,   /* 9999999 < 2^24: cabe exato ate em f32, sem ler a fonte */
    TEXTO_SUGESTAO_MAX = 150,    /* expressao mais longa que isso fica sem sugestao */
};

#define LIMITE_INTEIRO_F64 (UINT64_C(1) << 63)

/* O inteiro (magnitude) existe exato no real? f32 e unidade guardam float; f64 guarda double ate 2^63. */
static bool InteiroExatoEmReal(uint64_t mag, TipoId tipo)
{
    if (tipo == TIPO_F64) return mag <= LIMITE_INTEIRO_F64 && (uint64_t)(double)mag == mag;
    float f = (float)mag;
    return f < 18446744073709551616.0f && (uint64_t)f == mag;
}

bool SemLiteralInteiroEmReal(Semantica *s, Expr *literal, TipoId ctx)
{
    uint64_t mag;
    char nome[96];
    literal->tipo = ctx;
    if (literal->op <= DIGITOS_SEMPRE_EXATOS) return true;
    SemDescrever(s, ctx, nome, sizeof nome);
    if (!TipoLerLiteralU64(s->fonte->texto + literal->pos_op, &mag)) {
        return SemErro(s, E0330, literal->pos, "o literal passa de 64 bits e nao existe exato em %s", nome);
    }
    if (InteiroExatoEmReal(mag, ctx)) return true;
    double vira = ctx == TIPO_F64 ? (double)mag : (double)(float)mag;
    return SemErro(s, E0330, literal->pos, "o literal %llu nao existe exato em %s (vira %.1f)", (unsigned long long)mag, nome, vira);
}

/* ---------- sugestao do E0301 ---------- */

static bool CharDePalavra(char c) { return isalnum((unsigned char)c) || c == '_'; }

/* '.' so conta com digito depois (0..<n nao e real). */
static bool PontoDeReal(const char *t, uint32_t i) { return t[i] == '.' && isdigit((unsigned char)t[i + 1]); }

/* '+' ou '-' logo depois do 'e' de um real decimal (1e-3); fora disso o sinal e uma conta. */
static bool SinalDeExpoente(const char *t, uint32_t ini, uint32_t i)
{
    bool decimal = isdigit((unsigned char)t[ini]) && t[ini + 1] != 'x';
    bool sinal = t[i] == '+' || t[i] == '-';
    return decimal && sinal && i > ini && (t[i - 1] == 'e' || t[i - 1] == 'E');
}

/* Fim do token de nome ou numero que comeca em 'ini'. */
static uint32_t FimDoToken(const char *t, uint32_t ini)
{
    uint32_t i = ini;
    while (CharDePalavra(t[i]) || PontoDeReal(t, i) || SinalDeExpoente(t, ini, i)) i++;
    return i;
}

/* Fim (offset exclusivo) do texto da expressao na fonte; 0 = forma que a sugestao nao sabe delimitar. */
static uint32_t FimDaExpressao(const char *t, const Expr *e)
{
    switch (e->tipo_no) {
    case EX_INT:
    case EX_REAL:
    case EX_NOME:
    case EX_CAMPO: return FimDoToken(t, e->pos_op);
    case EX_UNARIO: return e->op == OP_NEGAR ? FimDaExpressao(t, e->a) : 0;
    case EX_BINARIO: return FimDaExpressao(t, e->b);
    default: return 0;
    }
}

/* Parenteses do trecho [ini, fim) fecham? Um ')' que sobra logo depois do fim (a * (b + c)) entra no trecho. */
static bool ParentesesFecham(const char *t, uint32_t ini, uint32_t *fim)
{
    int abertos = 0;
    for (uint32_t i = ini; i < *fim; i++) {
        abertos += t[i] == '(' ? 1 : t[i] == ')' ? -1 : 0;
        if (abertos < 0) return false;
    }
    while (abertos > 0 && t[*fim] == ')') {
        (*fim)++;
        abertos--;
    }
    return abertos == 0;
}

/* Delimita o texto da expressao na fonte: [e->pos, *fim). false = forma que a sugestao nao cobre ou longa demais. */
static bool TrechoDaExpressao(const Semantica *s, const Expr *e, uint32_t *fim)
{
    *fim = FimDaExpressao(s->fonte->texto, e);
    if (*fim <= e->pos || *fim - e->pos > TEXTO_SUGESTAO_MAX) return false;
    return ParentesesFecham(s->fonte->texto, e->pos, fim);
}

/* Conta sem parenteses de fora precisa deles antes do 'as' ('as' amarra mais forte que '+' e '*'). */
static bool PedeParenteses(const char *t, const Expr *e, uint32_t fim)
{
    bool entre_parenteses = t[e->pos] == '(' && t[fim - 1] == ')';
    return e->tipo_no == EX_BINARIO && !entre_parenteses;
}

bool SemConversaoObvia(const Semantica *s, const Expr *e, TipoId esperado, Conversao *c)
{
    const char *t = s->fonte->texto;
    char tipo[96];
    uint32_t fim;
    if (!TipoEhNumerico(&s->tipos, e->tipo) || !TipoEhNumerico(&s->tipos, esperado)) return false;
    if (!TrechoDaExpressao(s, e, &fim)) return false;
    SemDescrever(s, esperado, tipo, sizeof tipo);
    bool parenteses = PedeParenteses(t, e, fim);
    c->ini = e->pos;
    c->fim = fim;
    snprintf(c->novo, sizeof c->novo, "%s%.*s%s as %s", parenteses ? "(" : "", (int)(fim - e->pos), t + e->pos, parenteses ? ")" : "", tipo);
    return true;
}

void SemSugerirConversao(Semantica *s, const Expr *e, TipoId esperado)
{
    char json[sizeof ((Conversao *)0)->novo + 8];
    Conversao c;
    if (!SemConversaoObvia(s, e, esperado, &c)) return;
    snprintf(json, sizeof json, "\"%s\"", c.novo);
    DiagAcrescentarExtra(s->diag, "sugestao", json);
}
