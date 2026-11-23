/* sem_clong.c - c_long/c_ulong so como TIPO DE FRONTEIRA (M6c P83, D-M6c-8, E0342).
 * Versao: 0.1.3
 * Changelog: 0.1.3 - Linguagem v1.13 L1a: SemEntrarNaDecl no lugar de modulo_atual = d->modulo.
 *            0.1.2 - M6c P91b: '&x' direto de c_long para destino ^T de outro tipo (let/var, argumento, campo de literal, return) = E0342; campo de nome de uma letra de swizzle (x y z w r g b a) nao conta como c_long (falso E0342 em v.x). 0.1.1 - M6c P84: argumentos de size_of/align_of/offset_of nao sao leitura (offset_of(GzHeaderS.time) com time c_ulong dava E0342). 0.1.0 - M6c P83: regra sintatica sem fluxo (assinaturas, campos, const, let/var, argumento, atribuicao, '+=' e '-=').
 * Porque existe: c_long e apelido de i32/int por alvo (sem_tipo.c, D-M6c-7); esta regra impede que o apelido vaze (codigo que so compila num alvo).
 * Roda ANTES de tipar o corpo (CorposDoPrograma): no Linux a tipagem diria E0301 onde o Windows diria nada, e o diagnostico precisa ser o mesmo nos 2 alvos.
 * So liga se o programa escreve c_long ou c_ulong (s->usa_clong, InternerAchar): programa sem c_long nao paga nada.
 * Um valor e "c_long" (CLKind) pelo que o TEXTO diz: 'x as c_long', local declarado ou iniciado assim, campo de struct declarado assim (por nome de campo),
 * retorno de proc foreign declarado assim. Nao ha inferencia: o que a sintaxe nao mostra (campo de struct de mesmo nome em tipos diferentes, ponteiro
 * passado por varias funcoes) fica de fora; ponteiro ^c_long lido nao e conferido contra parametro de outro tipo (so o '&x' escrito direto) nem em atribuicao.
 * Limite (P91b): um campo c_long chamado x/y/z/w/r/g/b/a nao e conferido (v.x de vetor builtin e igual na sintaxe).
 * Verificacao: python ..\tests\semantica\verificar_semantica.py (err_clong_*, ok_clong_fronteira)
 */
#include "semantic/sem_interno.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum { K_NENHUM, K_LONG, K_ULONG, K_PTR_LONG, K_PTR_ULONG } CLKind;
typedef enum { DEST_LIVRE, DEST_CONV, DEST_VALOR, DEST_PTR, DEST_PTR_OUTRO } CLDest; /* LIVRE: c_long lido aqui e erro; CONV: operando de 'as' (ou let sem tipo): vale; VALOR/PTR: destino c_long / ^c_long; PTR_OUTRO: destino ^T de outro tipo (so o '&x' direto de c_long e erro) */
typedef struct { int32_t nome; uint8_t kind; } CLLocal;
typedef struct { Semantica *s; Vec locais; const Decl *proc; } CLCtx;

enum { CAMPO_DISCORDA = 0xFF, TRECHO_MAX = 96, FORMA_MAX = 384 };

static const char *const CAUSA_TIPO = "c_long e c_ulong so valem na fronteira com o C (parametro e retorno de proc foreign, campo de struct, let/var e ^c_long): "
                                      "aqui o tamanho mudaria por alvo (32 bits no Windows, 64 no Linux)";

/* ---------- tipo escrito ---------- */

static bool EhNomeCLong(const Semantica *s, const TipoRef *t)
{
    if (t->forma != TR_NOME || t->modulo != 0 || t->nome < 0) return false;
    if (t->nome != s->nome_clong && t->nome != s->nome_culong) return false;
    return SemAcharSimbolo(s, t->nome, 0)->categoria == SIM_NENHUM; /* como SemResolverNomeTipo: so quando o nome nao tem simbolo */
}

static CLKind KindDoTipo(const Semantica *s, const TipoRef *t)
{
    if (EhNomeCLong(s, t)) return t->erro >= 0 ? K_NENHUM : t->nome == s->nome_clong ? K_LONG : K_ULONG;
    if ((t->forma == TR_PONTEIRO || t->forma == TR_ANULAVEL) && t->alvo != NULL && EhNomeCLong(s, t->alvo)) {
        return t->alvo->nome == s->nome_clong ? K_PTR_LONG : K_PTR_ULONG;
    }
    return K_NENHUM;
}

static const TipoRef *CLongProibido(const Semantica *s, const TipoRef *t, bool topo_ok);

static const TipoRef *ProibidoEmVarios(const Semantica *s, const TipoRef *lista, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        const TipoRef *r = CLongProibido(s, &lista[k], false);
        if (r != NULL) return r;
    }
    return NULL;
}

/* O c_long escrito onde nao pode: topo_ok = o lugar aceita c_long, ^c_long e ?^c_long soltos; qualquer outro lugar (dentro de array, fatia, lista, map, ?T, proc) e proibido. */
static const TipoRef *CLongProibido(const Semantica *s, const TipoRef *t, bool topo_ok)
{
    if (t->forma == TR_NOME) return EhNomeCLong(s, t) && !(topo_ok && t->erro < 0) ? t : NULL;
    if (t->alvo == NULL) return NULL;
    if (t->forma == TR_PONTEIRO || t->forma == TR_ANULAVEL) {
        if (topo_ok && EhNomeCLong(s, t->alvo)) return NULL;
        return CLongProibido(s, t->alvo, false);
    }
    if (t->forma == TR_PROC) { /* alvo[0] = retorno (se pos_tamanho == 1); alvo[1..nome] = parametros */
        const TipoRef *r = t->pos_tamanho == 1 ? CLongProibido(s, &t->alvo[0], false) : NULL;
        return r != NULL ? r : ProibidoEmVarios(s, &t->alvo[1], (uint32_t)t->nome);
    }
    if (t->forma == TR_MAPA) return ProibidoEmVarios(s, t->alvo, 2);
    return CLongProibido(s, t->alvo, false);
}

static const char *NomeDoTipoRef(const Semantica *s, const TipoRef *t)
{
    return t != NULL && t->forma == TR_NOME && t->nome >= 0 ? SemNomeTexto(s, t->nome) : NULL;
}

/* ---------- diagnostico ---------- */

static bool ErroCLong(Semantica *s, uint32_t pos, const char *causa, const char *forma)
{
    SemErro(s, E0342, pos, "%s", causa);
    if (s->diag->tem && s->diag->codigo == E0342) DiagForma(s->diag, NULL, forma);
    return false;
}

static bool ErroTipo(Semantica *s, const TipoRef *t)
{
    return ErroCLong(s, t->pos, CAUSA_TIPO, "use i32, int ou i64 dentro da Forja e converta so na ponta: x as c_long");
}

/* O texto do primario que comeca em pos: nome, 'a.b', 'f(...)', 'a[i]', 'p^' e um 'as T' depois (so para a forma_certa). */
static size_t FimDoGrupo(const char *t, size_t i)
{
    int fundo = 0;
    for (; t[i] != '\0' && t[i] != '\n'; i++) {
        if (t[i] == '(' || t[i] == '[') fundo++;
        else if ((t[i] == ')' || t[i] == ']') && --fundo == 0) return i + 1;
    }
    return i;
}

static size_t FimDoPrimario(const char *t, size_t i)
{
    for (;;) {
        unsigned char ch = (unsigned char)t[i];
        if (isalnum(ch) || ch == '_' || ch == '.' || ch == '^') i++;
        else if (ch == '(' || ch == '[') i = FimDoGrupo(t, i);
        else break;
    }
    size_t j = i;
    while (t[j] == ' ') j++;
    if (strncmp(t + j, "as ", 3) != 0) return i;
    for (j += 3; t[j] == ' '; j++) {}
    while (isalnum((unsigned char)t[j]) || t[j] == '_' || t[j] == '^' || t[j] == '?') j++;
    return j;
}

static void TrechoDaExpr(const Semantica *s, uint32_t pos, char *buf, size_t cap)
{
    const char *t = s->fonte->texto;
    snprintf(buf, cap, "%.*s", (int)(FimDoPrimario(t, pos) - pos), t + pos);
}

/* Do pos ate o fim da linha, sem comentario nem espaco no fim (o lado direito de 'n += 1'). */
static void TrechoDaLinha(const Semantica *s, uint32_t pos, char *buf, size_t cap)
{
    const char *t = s->fonte->texto;
    size_t fim = pos;
    while (t[fim] != '\0' && t[fim] != '\n' && t[fim] != '\r' && !(t[fim] == '/' && t[fim + 1] == '/')) fim++;
    while (fim > pos && t[fim - 1] == ' ') fim--;
    snprintf(buf, cap, "%.*s", (int)(fim - pos), t + pos);
}

/* ---------- valor de uma expressao ---------- */

static CLKind BuscarLocal(const CLCtx *c, int32_t nome)
{
    for (size_t i = c->locais.n; i > 0; i--) {
        const CLLocal *l = VecEm(&c->locais, i - 1);
        if (l->nome == nome) return (CLKind)l->kind;
    }
    return K_NENHUM;
}

static void EmpurrarLocal(CLCtx *c, int32_t nome, CLKind k)
{
    CLLocal *l = VecEmpurrar(&c->locais);
    l->nome = nome;
    l->kind = (uint8_t)k;
}

static const Decl *ProcDaChamada(const CLCtx *c, const Expr *e)
{
    if (e->nome < 0) return NULL;
    const Simbolo *y = SemAcharSimbolo(c->s, e->nome, e->modulo);
    return y->categoria == SIM_PROC ? y->decl : NULL;
}

static const Decl *ProcExterno(const CLCtx *c, const Expr *e)
{
    if (e->nome < 0) return NULL;
    const Simbolo *y = SemAcharSimbolo(c->s, e->nome, e->modulo);
    return y->categoria == SIM_PROC && y->decl->externo ? y->decl : NULL;
}

static CLKind KindDeExpr(const CLCtx *c, const Expr *e);

/* Nome de campo de uma letra so dos grupos de swizzle "xyzw"/"rgba" (sem_vetor.c): a regra roda antes de tipar e nao sabe se v.x e vetor ou struct. */
static bool EhLetraDeVetor(const Semantica *s, int32_t nome)
{
    if (nome < 0) return false;
    const char *t = SemNomeTexto(s, nome);
    return t[0] != '\0' && t[1] == '\0' && strchr("xyzwrgba", t[0]) != NULL;
}

static CLKind KindDeUnario(const CLCtx *c, const Expr *e)
{
    if (e->op != OP_ENDERECO && e->op != OP_DEREF) return K_NENHUM;
    CLKind k = KindDeExpr(c, e->a);
    if (e->op == OP_ENDERECO) return k == K_LONG ? K_PTR_LONG : k == K_ULONG ? K_PTR_ULONG : K_NENHUM;
    return k == K_PTR_LONG ? K_LONG : k == K_PTR_ULONG ? K_ULONG : K_NENHUM;
}

static CLKind KindDeExpr(const CLCtx *c, const Expr *e)
{
    const Semantica *s = c->s;
    switch (e->tipo_no) {
    case EX_CONVERSAO: return e->alvo != NULL ? KindDoTipo(s, e->alvo) : K_NENHUM;
    case EX_NOME: return e->modulo == 0 ? BuscarLocal(c, e->nome) : K_NENHUM;
    case EX_CAMPO: {
        if (EhLetraDeVetor(s, e->nome)) return K_NENHUM; /* v.x de vetor builtin e indistinguivel de um campo c_long 'x' antes de tipar */
        uint8_t v = e->nome >= 0 && (uint32_t)e->nome < s->qtd_nomes ? s->clong_campos[e->nome] : 0;
        return v == 0 || v == CAMPO_DISCORDA ? K_NENHUM : (CLKind)(v - 1);
    }
    case EX_CHAMADA: {
        const Decl *d = ProcExterno(c, e);
        return d != NULL && d->tem_retorno ? KindDoTipo(s, &d->retorno) : K_NENHUM;
    }
    case EX_UNARIO: return KindDeUnario(c, e);
    default: return K_NENHUM;
    }
}

static bool EhValor(CLKind k)
{
    return k == K_LONG || k == K_ULONG;
}

static CLDest DestinoDe(CLKind k)
{
    return EhValor(k) ? DEST_VALOR : (k == K_PTR_LONG || k == K_PTR_ULONG) ? DEST_PTR : DEST_LIVRE;
}

/* Destino pelo tipo escrito: c_long e ^c_long como DestinoDe; ^T ou ?^T de outro tipo = PTR_OUTRO; o resto, LIVRE. */
static CLDest DestinoDoTipo(const Semantica *s, const TipoRef *t)
{
    CLKind k = KindDoTipo(s, t);
    if (k != K_NENHUM) return DestinoDe(k);
    return t->forma == TR_PONTEIRO || t->forma == TR_ANULAVEL ? DEST_PTR_OUTRO : DEST_LIVRE;
}

/* Arvore so de literais inteiros ('-7', '2 * 3'): vale como c_long (o tipo vem do destino). */
static bool EhLiteralInteiro(const Expr *e)
{
    if (e->tipo_no == EX_INT) return true;
    if (e->tipo_no == EX_UNARIO) return e->op == OP_NEGAR && EhLiteralInteiro(e->a);
    return e->tipo_no == EX_BINARIO && EhLiteralInteiro(e->a) && EhLiteralInteiro(e->b);
}

/* ---------- erros por posicao ---------- */

static bool ErroLeitura(const CLCtx *c, const Expr *e, CLKind k, const char *destino)
{
    char trecho[TRECHO_MAX], forma[FORMA_MAX];
    TrechoDaExpr(c->s, e->pos, trecho, sizeof trecho);
    snprintf(forma, sizeof forma, "%s as %s", trecho, destino != NULL ? destino : k == K_ULONG ? "uint" : "int");
    return ErroCLong(c->s, e->pos, "um c_long so passa por 'as', por argumento de parametro c_long ou por let/var declarado c_long: "
                     "ler ou misturar com i32/int daria tamanho diferente por alvo", forma);
}

static bool ErroDestinoValor(const CLCtx *c, const Expr *e, const char *destino)
{
    char trecho[TRECHO_MAX], forma[FORMA_MAX];
    TrechoDaExpr(c->s, e->pos, trecho, sizeof trecho);
    snprintf(forma, sizeof forma, "%s as %s", trecho, destino != NULL ? destino : "c_long");
    return ErroCLong(c->s, e->pos, "um c_long so recebe literal inteiro, 'x as c_long' ou outro c_long", forma);
}

static bool ErroDestinoPtr(const CLCtx *c, const Expr *e)
{
    return ErroCLong(c->s, e->pos, "um ^c_long so recebe '&x' de um c_long ou none", "passe &x de um local declarado c_long (ou none em ?^c_long)");
}

static bool ErroEnderecoOutro(const CLCtx *c, const Expr *e)
{
    char trecho[TRECHO_MAX], forma[FORMA_MAX];
    TrechoDaExpr(c->s, e->a->pos, trecho, sizeof trecho);
    snprintf(forma, sizeof forma, "so um ^c_long recebe &%s: declare o destino ^c_long ou guarde o valor em i32/int e converta com 'as'", trecho);
    return ErroCLong(c->s, e->pos, "'&x' de um c_long e um ^c_long: ^i32 (ou outro ^T) leria o tamanho errado em um dos alvos (4 bytes no Windows, 8 no Linux)", forma);
}

static bool ErroConta(const CLCtx *c, const Stmt *st, CLKind k)
{
    char alvo[TRECHO_MAX], direita[TRECHO_MAX], forma[FORMA_MAX];
    TrechoDaExpr(c->s, st->alvo->pos, alvo, sizeof alvo);
    TrechoDaLinha(c->s, st->expr->pos, direita, sizeof direita);
    bool com_espaco = strchr(direita, ' ') != NULL;
    snprintf(forma, sizeof forma, "%s = (%s as %s %c %s%s%s) as %s", alvo, alvo, k == K_LONG ? "int" : "uint", st->op == ATRIB_SOMA ? '+' : '-',
             com_espaco ? "(" : "", direita, com_espaco ? ")" : "", k == K_LONG ? "c_long" : "c_ulong");
    return ErroCLong(c->s, st->alvo->pos, "um c_long nao faz conta: '+=' e '-=' somariam i32 no Windows e int no Linux", forma);
}

/* ---------- expressoes ---------- */

static bool Visitar(CLCtx *c, const Expr *e, CLDest dest, const char *destino);

static bool VisitarArgs(CLCtx *c, const Expr *e)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (!Visitar(c, e->args[i].valor, DEST_LIVRE, NULL)) return false;
    }
    return true;
}

static bool TipoLivre(CLCtx *c, const TipoRef *t)
{
    const TipoRef *p = CLongProibido(c->s, t, true);
    return p == NULL || ErroTipo(c->s, p);
}

static const Param *ParamDoArg(const Decl *d, const Arg *a, uint32_t i)
{
    if (a->nome < 0) return i < d->nparams ? &d->params[i] : NULL;
    for (uint32_t k = 0; k < d->nparams; k++) {
        if (d->params[k].nome == a->nome) return &d->params[k];
    }
    return NULL;
}

/* Argumento de parametro foreign declarado c_long/^c_long e destino VALOR/PTR; os demais (inclusive proc da Forja e embutido) sao LIVRE. */
static bool FilhosChamada(CLCtx *c, const Expr *e)
{
    const Decl *d = e->tipo_no == EX_CHAMADA ? ProcDaChamada(c, e) : NULL;
    for (uint32_t i = 0; i < e->nargs; i++) {
        const Param *p = d != NULL ? ParamDoArg(d, &e->args[i], i) : NULL;
        if (!Visitar(c, e->args[i].valor, p != NULL ? DestinoDoTipo(c->s, &p->tipo) : DEST_LIVRE, p != NULL ? NomeDoTipoRef(c->s, &p->tipo) : NULL)) return false;
    }
    return true;
}

static const Campo *CampoDoArg(const Decl *d, const Arg *a, uint32_t i)
{
    if (a->nome < 0) return i < d->ncampos ? &d->campos[i] : NULL;
    for (uint32_t k = 0; k < d->ncampos; k++) {
        if (d->campos[k].nome == a->nome) return &d->campos[k];
    }
    return NULL;
}

static bool FilhosLiteralStruct(CLCtx *c, const Expr *e)
{
    const Simbolo *y = e->nome >= 0 ? SemAcharSimbolo(c->s, e->nome, e->modulo) : NULL;
    const Decl *d = y != NULL && y->categoria == SIM_STRUCT ? y->decl : NULL;
    for (uint32_t i = 0; i < e->nargs; i++) {
        const Campo *f = d != NULL ? CampoDoArg(d, &e->args[i], i) : NULL;
        if (!Visitar(c, e->args[i].valor, f != NULL ? DestinoDoTipo(c->s, &f->tipo) : DEST_LIVRE, f != NULL ? NomeDoTipoRef(c->s, &f->tipo) : NULL)) return false;
    }
    return true;
}

/* size_of/align_of/offset_of recebem tipo ou campo, nunca um valor lido: offset_of(S.t) com t c_ulong nao e leitura (P84, D-M6c-8). */
static bool EhChamadaDeLayout(const Expr *e)
{
    return e->tipo_no == EX_CHAMADA && e->modulo == 0 && (e->nome == SYM_SIZE_OF || e->nome == SYM_ALIGN_OF || e->nome == SYM_OFFSET_OF);
}

static bool Filhos(CLCtx *c, const Expr *e)
{
    switch (e->tipo_no) {
    case EX_INT: case EX_REAL: case EX_BOOL: case EX_TEXTO: case EX_NOME: case EX_NONE: return true;
    case EX_CONVERSAO: return TipoLivre(c, e->alvo) && Visitar(c, e->a, DEST_CONV, NULL);
    case EX_TIPO: return TipoLivre(c, e->alvo);
    case EX_CHAMADA: return EhChamadaDeLayout(e) || FilhosChamada(c, e);
    case EX_LITERAL_STRUCT: return FilhosLiteralStruct(c, e);
    case EX_UNARIO: return Visitar(c, e->a, e->op == OP_ENDERECO || e->op == OP_DEREF ? DEST_CONV : DEST_LIVRE, NULL);
    case EX_LITERAL_ARRAY: case EX_TUPLA: case EX_FTEXTO: return VisitarArgs(c, e);
    case EX_FATIA: case EX_INDICE: return Visitar(c, e->a, DEST_LIVRE, NULL) && Visitar(c, e->b, DEST_LIVRE, NULL) && VisitarArgs(c, e);
    case EX_CHAMADA_IND: return Visitar(c, e->a, DEST_LIVRE, NULL) && FilhosChamada(c, e);
    default: return Visitar(c, e->a, DEST_LIVRE, NULL) && Visitar(c, e->b, DEST_LIVRE, NULL);
    }
}

static bool Visitar(CLCtx *c, const Expr *e, CLDest dest, const char *destino)
{
    if (e == NULL) return true;
    CLKind k = KindDeExpr(c, e);
    if (dest == DEST_LIVRE && EhValor(k)) return ErroLeitura(c, e, k, destino);
    if (dest == DEST_VALOR && !EhValor(k) && !EhLiteralInteiro(e)) return ErroDestinoValor(c, e, destino);
    if (dest == DEST_PTR_OUTRO && e->tipo_no == EX_UNARIO && e->op == OP_ENDERECO && (k == K_PTR_LONG || k == K_PTR_ULONG)) return ErroEnderecoOutro(c, e);
    if (dest == DEST_PTR && k != K_PTR_LONG && k != K_PTR_ULONG && e->tipo_no != EX_NONE) return ErroDestinoPtr(c, e);
    return Filhos(c, e);
}

/* ---------- comandos ---------- */

static bool Corpo(CLCtx *c, const Stmt *st);

/* let/var: com tipo escrito o destino e o tipo; sem tipo o local herda o que o valor e (let n = x as c_long). */
static bool Declaracao(CLCtx *c, const Stmt *st)
{
    CLKind k = K_NENHUM;
    CLDest dest = DEST_CONV;
    const char *destino = NULL;
    if (st->tem_tipo) {
        const TipoRef *p = CLongProibido(c->s, &st->tipo, true);
        if (p != NULL) return ErroTipo(c->s, p);
        k = KindDoTipo(c->s, &st->tipo);
        dest = DestinoDoTipo(c->s, &st->tipo);
        destino = NomeDoTipoRef(c->s, &st->tipo);
    }
    if (!Visitar(c, st->expr, dest, destino)) return false;
    if (!st->tem_tipo && st->expr != NULL) k = KindDeExpr(c, st->expr);
    EmpurrarLocal(c, st->nome, k);
    return true;
}

static bool Atribuicao(CLCtx *c, const Stmt *st)
{
    const Expr *alvo = st->alvo;
    CLKind k = KindDeExpr(c, alvo);
    bool descarte = alvo->tipo_no == EX_NOME && alvo->modulo == 0 && alvo->nome == c->s->nome_descarte;
    if (!Filhos(c, alvo)) return false;
    if (EhValor(k) && st->op != ATRIB_SIMPLES) return ErroConta(c, st, k);
    return Visitar(c, st->expr, descarte ? DEST_CONV : DestinoDe(k), k == K_LONG ? "c_long" : k == K_ULONG ? "c_ulong" : NULL);
}

static bool CorpoDoCaso(CLCtx *c, const Stmt *st)
{
    for (uint32_t i = 0; i < st->ncasos; i++) {
        const Caso *caso = &st->casos[i];
        if (caso->tipo_escrito != NULL && !TipoLivre(c, caso->tipo_escrito)) return false;
        size_t marca = c->locais.n;
        EmpurrarLocal(c, st->nome, K_NENHUM);
        bool ok = Corpo(c, caso->corpo);
        c->locais.n = marca;
        if (!ok) return false;
    }
    return true;
}

static bool CorpoDoFor(CLCtx *c, const Stmt *st)
{
    if (!Visitar(c, st->expr, DEST_LIVRE, NULL) || !Visitar(c, st->expr2, DEST_LIVRE, NULL)) return false;
    size_t marca = c->locais.n;
    EmpurrarLocal(c, st->nome, K_NENHUM);
    if (st->nome_indice >= 0) EmpurrarLocal(c, st->nome_indice, K_NENHUM);
    bool ok = Corpo(c, st->corpo);
    c->locais.n = marca;
    return ok;
}

static bool Comando(CLCtx *c, const Stmt *st)
{
    switch (st->tipo_no) {
    case ST_LET: case ST_VAR: return Declaracao(c, st);
    case ST_ATRIBUICAO: return Atribuicao(c, st);
    case ST_IF: case ST_WHILE: return Visitar(c, st->expr, DEST_LIVRE, NULL) && Corpo(c, st->corpo) && Corpo(c, st->senao);
    case ST_FOR: return CorpoDoFor(c, st);
    case ST_RETURN: return Visitar(c, st->expr, c->proc->tem_retorno && c->proc->retorno.forma != TR_TUPLA ? DestinoDoTipo(c->s, &c->proc->retorno) : DEST_LIVRE, NULL);
    case ST_EXPECT: return Visitar(c, st->expr, DEST_LIVRE, NULL);
    case ST_CHAMADA: return Visitar(c, st->expr, DEST_CONV, NULL);
    case ST_DEFER: return Corpo(c, st->corpo);
    case ST_SWITCH: return Visitar(c, st->expr, DEST_LIVRE, NULL) && CorpoDoCaso(c, st);
    case ST_LET_MULTIPLO:
        if (!Visitar(c, st->expr, DEST_LIVRE, NULL)) return false;
        for (uint32_t i = 0; i < st->nnomes; i++) EmpurrarLocal(c, st->nomes[i].nome, K_NENHUM);
        return true;
    default: return true;
    }
}

/* Cadeia de comandos de um bloco: os locais do bloco saem no fim. */
static bool Corpo(CLCtx *c, const Stmt *st)
{
    size_t marca = c->locais.n;
    bool ok = true;
    for (; st != NULL && ok; st = st->prox) ok = Comando(c, st);
    c->locais.n = marca;
    return ok;
}

bool SemCLongCorpo(Semantica *s, const Decl *d)
{
    CLCtx c = {s, {0}, d};
    VecIniciar(&c.locais, sizeof(CLLocal));
    for (uint32_t i = 0; i < d->nparams; i++) EmpurrarLocal(&c, d->params[i].nome, KindDoTipo(s, &d->params[i].tipo));
    bool ok = Corpo(&c, d->corpo);
    VecLiberar(&c.locais);
    return ok;
}

/* ---------- declaracoes ---------- */

static const TipoRef *ProibidoNoProc(const Semantica *s, const Decl *d)
{
    for (uint32_t i = 0; i < d->nparams; i++) {
        const TipoRef *r = CLongProibido(s, &d->params[i].tipo, d->externo);
        if (r != NULL) return r;
    }
    if (d->tem_retorno && d->retorno.forma != TR_TUPLA) return CLongProibido(s, &d->retorno, d->externo);
    for (uint32_t i = 0; d->tem_retorno && i < d->nretornos; i++) {
        const TipoRef *r = CLongProibido(s, &d->retornos[i].tipo, false);
        if (r != NULL) return r;
    }
    return NULL;
}

static const TipoRef *ProibidoNosCampos(const Semantica *s, const Decl *d, bool topo_ok)
{
    for (uint32_t i = 0; i < d->ncampos; i++) {
        const TipoRef *r = CLongProibido(s, &d->campos[i].tipo, topo_ok);
        if (r != NULL) return r;
    }
    return NULL;
}

static const TipoRef *ProibidoNaDecl(const Semantica *s, const Decl *d)
{
    switch (d->tipo_no) {
    case D_PROC: return ProibidoNoProc(s, d);
    case D_STRUCT: return ProibidoNosCampos(s, d, true);
    case D_UNIAO: return ProibidoNosCampos(s, d, false);
    case D_CONST: return CLongProibido(s, &d->tipo, false);
    default: return NULL;
    }
}

/* clong_campos[nome do campo] = 1 + CLKind; campo com o mesmo nome em structs que discordam vale CAMPO_DISCORDA (nao confere). */
static void RegistrarCampos(Semantica *s)
{
    s->clong_campos = ArenaAlocar(s->arena, s->qtd_nomes);
    memset(s->clong_campos, 0, s->qtd_nomes);
    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no != D_STRUCT) continue;
        for (uint32_t i = 0; i < d->ncampos; i++) {
            uint8_t novo = (uint8_t)(KindDoTipo(s, &d->campos[i].tipo) + 1);
            uint8_t *atual = &s->clong_campos[d->campos[i].nome];
            *atual = *atual == 0 || *atual == novo ? novo : CAMPO_DISCORDA;
        }
    }
}

bool SemCLongDecls(Semantica *s)
{
    s->nome_clong = InternerAchar(s->nomes, "c_long", 6);
    s->nome_culong = InternerAchar(s->nomes, "c_ulong", 7);
    s->usa_clong = s->nome_clong >= 0 || s->nome_culong >= 0;
    if (!s->usa_clong) return true;
    RegistrarCampos(s);
    bool ok = true;
    for (Decl *d = s->programa->primeira; d != NULL && !s->lista.cortado; d = d->prox) {
        const TipoRef *proibido = ProibidoNaDecl(s, d);
        if (proibido == NULL) continue;
        ok = false;
        d->com_erro = true;
        SemEntrarNaDecl(s, d);
        ErroTipo(s, proibido);
        if (!SemGuardarErro(s, d->modulo)) break;
    }
    return ok;
}
