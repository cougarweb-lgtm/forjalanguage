/* ast_fim.c - fim e inicio dos nos da AST, lidos dos tokens do fonte (AST 1.0).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I1: primeira versao (ver ast_fim.h).
 * Verificacao: construir.bat && forja.exe ast <arq.fj>; python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 */
#include "frontend/ast_fim.h"

#include "frontend/lexer.h"
#include "frontend/token.h"

static inline const Token *Tok(const AstFim *f, uint32_t i)
{
    return (const Token *)VecEm(&f->toks, i);
}

static inline uint32_t ParDe(const AstFim *f, uint32_t i)
{
    return *(const uint32_t *)VecEm(&f->par, i);
}

/* ---------- pares ( ) { } [ ] e f"..." ---------- */

/* Tipo do token que fecha o par aberto por 'abre'; TK_EOF (0) se 'abre' nao abre nada. */
static TokenTipo FechadorDe(TokenTipo abre)
{
    switch (abre) {
    case TK_LPAREN: return TK_RPAREN;
    case TK_LBRACE: return TK_RBRACE;
    case TK_LCOLCHETE: return TK_RCOLCHETE;
    case TK_FTEXTO_INICIO: return TK_FTEXTO_FIM;
    case TK_FTEXTO_CHAVE: return TK_FTEXTO_FIM_CHAVE;
    default: return TK_EOF;
    }
}

static void CasarPares(AstFim *f)
{
    Vec pilha;
    VecIniciar(&pilha, sizeof(uint32_t));
    VecIniciar(&f->par, sizeof(uint32_t));
    VecReservar(&f->par, f->toks.n);
    for (uint32_t i = 0; i < f->toks.n; i++) *(uint32_t *)VecEmpurrar(&f->par) = AST_FIM_NENHUM;
    for (uint32_t i = 0; i < f->toks.n; i++) {
        TokenTipo t = (TokenTipo)Tok(f, i)->tipo;
        if (FechadorDe(t) != TK_EOF) {
            *(uint32_t *)VecEmpurrar(&pilha) = i;
        } else if (pilha.n > 0) {
            uint32_t topo = *(uint32_t *)VecEm(&pilha, pilha.n - 1);
            if (FechadorDe((TokenTipo)Tok(f, topo)->tipo) != t) continue;
            *(uint32_t *)VecEm(&f->par, i) = topo;
            *(uint32_t *)VecEm(&f->par, topo) = i;
            pilha.n--;
        }
    }
    VecLiberar(&pilha);
}

bool AstFimIniciar(AstFim *f, const Fonte *fonte, Interner *nomes, Diag *diag)
{
    Lexer lx = {.fonte = fonte, .nomes = nomes, .diag = diag, .inicio = 0};
    *f = (AstFim){0};
    if (!LexerAnalisar(&lx)) {
        LexerLiberar(&lx);
        return false;
    }
    f->toks = lx.tokens;
    CasarPares(f);
    return true;
}

void AstFimLiberar(AstFim *f)
{
    VecLiberar(&f->toks);
    VecLiberar(&f->par);
}

/* ---------- consulta por offset ---------- */

/* Primeiro token com off >= 'off' (toks.n se nao ha). */
static uint32_t IndiceDepois(const AstFim *f, uint32_t off)
{
    uint32_t lo = 0, hi = (uint32_t)f->toks.n;
    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;
        if (Tok(f, meio)->off < off) lo = meio + 1; else hi = meio;
    }
    return lo;
}

static uint32_t IndiceExato(const AstFim *f, uint32_t off)
{
    uint32_t i = IndiceDepois(f, off);
    return i < f->toks.n && Tok(f, i)->off == off ? i : AST_FIM_NENHUM;
}

static uint32_t FimTok(const AstFim *f, uint32_t i)
{
    return Tok(f, i)->off + Tok(f, i)->len;
}

/* Fim do token que comeca em 'off'; 'off' mesmo se nenhum token comeca ali (no criado pela semantica). */
static uint32_t FimEmOff(const AstFim *f, uint32_t off)
{
    uint32_t i = IndiceExato(f, off);
    return i == AST_FIM_NENHUM ? off : FimTok(f, i);
}

/* Fim do par do token i (o fechador de um abridor); sem par, o fim do proprio token. */
static uint32_t FimPar(const AstFim *f, uint32_t i)
{
    if (i >= f->toks.n) return 0;
    return ParDe(f, i) != AST_FIM_NENHUM ? FimTok(f, ParDe(f, i)) : FimTok(f, i);
}

/* Fim do par cujo abridor e o token logo depois do nome em 'off_nome' (chamada f(...) e literal Nome{...}). */
static uint32_t FimPareadoAposNome(const AstFim *f, uint32_t off_nome)
{
    uint32_t i = IndiceExato(f, off_nome);
    return i == AST_FIM_NENHUM ? off_nome : FimPar(f, i + 1);
}

/* Fim do ultimo token da linha que comeca em 'desde' (para no NL, no EOF e no '}' que fecha o bloco). */
static uint32_t FimLinha(const AstFim *f, uint32_t desde)
{
    uint32_t fim = desde;
    for (uint32_t i = IndiceDepois(f, desde); i < f->toks.n; i++) {
        TokenTipo t = (TokenTipo)Tok(f, i)->tipo;
        if (t == TK_NL || t == TK_EOF || t == TK_RBRACE) break;
        fim = FimTok(f, i);
    }
    return fim;
}

/* pos_fim do parser: o '}' (ou ')') do no, ou o NL que o encerra (entao o fim e o do token anterior). */
static uint32_t FimDePosFim(const AstFim *f, uint32_t pos_fim)
{
    uint32_t i = IndiceExato(f, pos_fim);
    if (i == AST_FIM_NENHUM) return pos_fim;
    TokenTipo t = (TokenTipo)Tok(f, i)->tipo;
    if ((t == TK_NL || t == TK_EOF) && i > 0) return FimTok(f, i - 1);
    return FimTok(f, i);
}

/* Limite de faixa e valor de membro: '-' opcional e o numero. */
static uint32_t FimLimite(const AstFim *f, uint32_t off)
{
    uint32_t i = IndiceExato(f, off);
    if (i == AST_FIM_NENHUM) return off;
    if (Tok(f, i)->tipo == TK_MINUS) i++;
    return FimTok(f, i);
}

/* ---------- tipos ---------- */

/* 'T ! E', '! E' e '(...) ! E': o erro vem depois do corpo do tipo; com 'apelido.E' o nome e o 3o token. */
static uint32_t FimErro(const AstFim *f, const TipoRef *t, uint32_t fim_corpo)
{
    uint32_t i = IndiceDepois(f, fim_corpo);
    if (i < f->toks.n && Tok(f, i)->tipo == TK_BANG) i++;
    if (t->modulo_erro != 0) i += 2;
    return i < f->toks.n ? FimTok(f, i) : fim_corpo;
}

static uint32_t FimCorpoDoTipo(const AstFim *f, const TipoRef *t)
{
    switch ((TipoRefForma)t->forma) {
    case TR_NOME: return FimEmOff(f, t->nome >= 0 && t->modulo != 0 ? t->pos_tamanho : t->pos);
    case TR_TUPLA: return FimPar(f, IndiceExato(f, t->pos));
    case TR_PROC:
        if (t->pos_tamanho == 1) return AstFimTipo(f, &t->alvo[0]);
        return FimPareadoAposNome(f, t->pos);
    case TR_MAPA: return AstFimTipo(f, &t->alvo[1]);
    case TR_BIT_SET: return FimPareadoAposNome(f, t->pos);
    default: return AstFimTipo(f, t->alvo);
    }
}

uint32_t AstFimTipo(const AstFim *f, const TipoRef *t)
{
    uint32_t fim = FimCorpoDoTipo(f, t);
    return t->erro >= 0 ? FimErro(f, t, fim) : fim;
}

/* ---------- expressoes ---------- */

static uint32_t FimUltimoArg(const AstFim *f, const Expr *e)
{
    return e->nargs > 0 ? AstFimExpr(f, e->args[e->nargs - 1].valor) : e->pos_op;
}

static uint32_t FimChamadaInd(const AstFim *f, const Expr *e)
{
    uint32_t i = IndiceExato(f, e->pos_op);
    if (i == AST_FIM_NENHUM) return e->pos_op;
    return FimPar(f, Tok(f, i)->tipo == TK_LPAREN ? i : i + 1);
}

static uint32_t FimNatural(const AstFim *f, const Expr *e)
{
    switch ((ExprTipo)e->tipo_no) {
    case EX_CHAMADA:
    case EX_LITERAL_STRUCT: return FimPareadoAposNome(f, e->pos_op);
    case EX_LITERAL_ARRAY:
    case EX_INDICE:
    case EX_FATIA: return FimPar(f, IndiceExato(f, e->pos_op));
    case EX_CHAMADA_IND: return FimChamadaInd(f, e);
    case EX_FTEXTO: return FimPar(f, IndiceExato(f, e->pos));
    case EX_UNARIO: return e->op == OP_DEREF ? FimEmOff(f, e->pos_op) : AstFimExpr(f, e->a);
    case EX_BINARIO:
    case EX_OR_ELSE: return AstFimExpr(f, e->b);
    case EX_CONVERSAO:
    case EX_TIPO: return AstFimTipo(f, e->alvo);
    case EX_TRY:
    case EX_EMBRULHA:
    case EX_OPC_TESTE: return AstFimExpr(f, e->a);
    case EX_TUPLA: return FimUltimoArg(f, e);
    default: return FimEmOff(f, e->pos_op);
    }
}

uint32_t AstFimExpr(const AstFim *f, const Expr *e)
{
    uint32_t fim = FimNatural(f, e);
    uint32_t i = IndiceExato(f, e->pos);
    if (i != AST_FIM_NENHUM && Tok(f, i)->tipo == TK_LPAREN && ParDe(f, i) != AST_FIM_NENHUM && FimTok(f, ParDe(f, i)) > fim) {
        fim = FimTok(f, ParDe(f, i));
    }
    return fim;
}

/* ---------- comandos ---------- */

static uint32_t FimIf(const AstFim *f, const Stmt *s)
{
    uint32_t i = IndiceExato(f, s->pos_fim);
    uint32_t fim = FimDePosFim(f, s->pos_fim);
    if (s->senao == NULL || i == AST_FIM_NENHUM || i + 2 >= f->toks.n) return fim;
    /* '}' else ('if' ... | '{' ... '}') */
    if (Tok(f, i + 2)->tipo == KW_IF) return AstFimStmt(f, s->senao);
    return FimPar(f, i + 2);
}

uint32_t AstFimStmt(const AstFim *f, const Stmt *s)
{
    switch ((StmtTipo)s->tipo_no) {
    case ST_DEFER: return s->corpo != NULL ? AstFimStmt(f, s->corpo) : FimEmOff(f, s->pos);
    case ST_IF: return FimIf(f, s);
    case ST_WHILE:
    case ST_FOR:
    case ST_SWITCH: return FimDePosFim(f, s->pos_fim);
    default: return FimLinha(f, s->expr != NULL ? AstFimExpr(f, s->expr) : FimEmOff(f, s->pos));
    }
}

uint32_t AstFimCaso(const AstFim *f, const Caso *c)
{
    const Stmt *ultimo = c->corpo;
    if (ultimo == NULL) return FimLinha(f, c->pos);
    while (ultimo->prox != NULL) ultimo = ultimo->prox;
    return AstFimStmt(f, ultimo);
}

/* ---------- partes e declaracoes ---------- */

uint32_t AstFimParam(const AstFim *f, const Param *p)
{
    return p->padrao != NULL ? AstFimExpr(f, p->padrao) : AstFimTipo(f, &p->tipo);
}

uint32_t AstFimCampo(const AstFim *f, const Campo *c)
{
    if (c->padrao != NULL) return AstFimExpr(f, c->padrao);
    return c->tem_faixa ? FimLimite(f, c->pos_max) : AstFimTipo(f, &c->tipo);
}

uint32_t AstFimMembro(const AstFim *f, const Membro *m)
{
    return m->tem_valor ? FimLimite(f, m->pos_valor) : FimEmOff(f, m->pos);
}

uint32_t AstFimDecl(const AstFim *f, const Decl *d)
{
    switch ((DeclTipo)d->tipo_no) {
    case D_UNIDADE_BASE:
    case D_UNIDADE_DERIV:
    case D_IMPORT: return FimLinha(f, d->pos);
    case D_CONST: return FimLinha(f, AstFimExpr(f, d->init));
    default: return FimDePosFim(f, d->pos_fim);
    }
}

uint32_t AstInicioDecl(const AstFim *f, const Decl *d)
{
    uint32_t i = IndiceExato(f, d->pos);
    if (i == AST_FIM_NENHUM) return d->pos;
    bool pos_e_palavra = d->tipo_no == D_IMPORT || d->tipo_no == D_TEST || d->tipo_no == D_ASSERT; /* nos demais Decl.pos e o nome */
    if (!pos_e_palavra && i > 0) i--;
    if (i > 0 && Tok(f, i - 1)->tipo == KW_EXPORT) i--;
    if (i > 0 && Tok(f, i - 1)->tipo == KW_PUB) i--;
    return Tok(f, i)->off;
}
