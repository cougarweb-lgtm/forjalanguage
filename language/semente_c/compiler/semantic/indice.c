/* indice.c - indice semantico do projeto: converte as ligacoes da AST 1.0 (AstRefs) em usos com alvo, dono e posicao (ver indice.h).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - G3 G0c: ordenacao total (CompararUsos + IndUso.ordem); repetido continua definido so pela chave de CompararUsos (fica o primeiro coletado).
 *            0.1.1 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            0.1.0 - IA1-I2: primeira versao. Nada e reanalisado: o alvo de cada uso e o que a semantica ligou (Expr.ref/ref_id/tipo) ou, nos tipos escritos e nos cases,
 *            a declaracao global do nome (a mesma regra do "ref" do 'forja ast').
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "semantic/indice.h"

IndAlvo IndiceAlvoDaDecl(const Decl *d)
{
    switch ((DeclTipo)d->tipo_no) {
    case D_CONST: return IND_CONST;
    case D_STRUCT: return IND_STRUCT;
    case D_ENUM: return IND_ENUM;
    case D_UNIAO: return IND_UNIAO;
    case D_PROC: return IND_PROC;
    default: return IND_UNIDADE;
    }
}

static bool EhDeclDeTipo(const Decl *d)
{
    return d->tipo_no == D_STRUCT || d->tipo_no == D_ENUM || d->tipo_no == D_UNIAO || d->tipo_no == D_UNIDADE_BASE || d->tipo_no == D_UNIDADE_DERIV;
}

int32_t IndiceIndiceDoMembro(const Decl *d, int32_t membro)
{
    for (uint32_t k = 0; k < d->nmembros; k++) {
        if (d->membros[k].nome == membro) return (int32_t)k;
    }
    return -1;
}

const Decl *IndiceDeclDoNome(const Indice *ind, int32_t nome, uint8_t modulo)
{
    return AstRefsDeclDoNome(&ind->refs, nome, 0, modulo);
}

const char *IndiceCategoria(const Decl *d)
{
    switch ((DeclTipo)d->tipo_no) {
    case D_UNIDADE_BASE:
    case D_UNIDADE_DERIV: return "unit";
    case D_CONST: return "const";
    case D_STRUCT: return "struct";
    case D_ENUM: return "enum";
    case D_PROC: return d->externo ? "foreign" : "proc";
    case D_TEST: return "teste";
    case D_LINK: return "link";
    case D_UNIAO: return "union";
    case D_IMPORT: return "import";
    case D_ASSERT: return "static_assert";
    }
    return "?";
}

const char *IndiceNomeDoAlvo(IndAlvo alvo)
{
    static const char *const NOMES[] = {"proc", "struct", "enum", "union", "const", "unit", "membro", "campo", "param", "local", "embutido"};
    return NOMES[alvo];
}

static IndUso UsoBase(uint32_t pos, uint32_t pos_nome, int32_t nome, const Decl *dono)
{
    return (IndUso){.pos = pos, .pos_nome = pos_nome, .decl_pos = AST_REF_SEM_POS, .dono = dono, .nome = nome, .indice = -1};
}

/* Uso de um local ou parametro do proc/test que contem o uso. */
static bool UsoDeLocal(const AstRefEntrada *r, IndUso *u)
{
    bool param = r->e->ref == REF_PARAM;

    if (r->decl_pos == AST_REF_SEM_POS || r->e->pos_op == r->decl_pos) return false; /* uso na propria declaracao = o delete que o 'em = escopo' cria, nao esta no fonte */
    u->alvo_tipo = param ? IND_PARAM : IND_LOCAL;
    u->alvo = r->dono;
    u->decl_pos = r->decl_pos;
    u->indice = param ? r->e->ref_id : -1;
    return true;
}

/* Uso de campo ou membro: o alvo e a struct/union/enum do tipo da base (ou do proprio enum). */
static bool UsoDeMembro(const Indice *ind, const Expr *e, IndUso *u)
{
    bool membro = e->ref == REF_MEMBRO;
    const Decl *d = AstRefsDeclDoTipo(&ind->refs, membro ? e->tipo : e->a->tipo);

    if (d == NULL) return false;
    if (!membro && e->a->pos_op == e->pos_op) return false; /* 'a.id' que a semantica cria no lugar de 'a' (em = <Arena>): nao esta no fonte */
    u->alvo = d;
    u->alvo_tipo = membro ? IND_MEMBRO : IND_CAMPO;
    u->indice = e->ref_id;
    return true;
}

/* O nome do embutido esta mesmo escrito em pos_op? O delete que 'em = escopo' cria nasce na posicao do let, sem o texto 'delete'. */
static bool EscritoNaPosicao(const Semantica *sem, const Expr *e)
{
    uint32_t n = InternerTam(sem->nomes, e->nome);

    return e->pos_op + (size_t)n <= sem->fonte->tam && memcmp(sem->fonte->texto + e->pos_op, InternerTexto(sem->nomes, e->nome), n) == 0;
}

/* Uso de uma expressao que a semantica ligou (nome, chamada, campo, literal de struct); false = a AST tambem nao traz "ref" (ver ast_json_ref.c). */
static bool UsoDeExpr(const Indice *ind, const AstRefEntrada *r, IndUso *u)
{
    const Expr *e = r->e;
    const Decl *d;

    *u = UsoBase(e->pos, e->pos_op, e->nome, r->dono);
    u->chamada = e->tipo_no == EX_CHAMADA;
    switch ((RefTipo)e->ref) {
    case REF_LOCAL:
    case REF_PARAM: return UsoDeLocal(r, u);
    case REF_MEMBRO:
    case REF_CAMPO: return UsoDeMembro(ind, e, u);
    case REF_EMBUTIDO: u->alvo_tipo = IND_EMBUTIDO; return EscritoNaPosicao(ind->sem, e);
    case REF_CONST:
    case REF_ENUM:
    case REF_PROC:
    case REF_STRUCT:
        d = AstRefsDeclDoNome(&ind->refs, e->nome, e->modulo, r->dono != NULL ? r->dono->modulo : 0);
        if (d == NULL) return false;
        u->alvo = d;
        u->alvo_tipo = IndiceAlvoDaDecl(d);
        return true;
    default: return false;
    }
}

/* Uso de nome de tipo (tipo escrito, case, nome de tipo em expressao); 'case Enum.membro' aponta o membro. */
static bool UsoDeTipo(const Indice *ind, const AstRefTipo *t, IndUso *u)
{
    const Decl *d = AstRefsDeclDoNome(&ind->refs, t->nome, t->modulo, t->dono != NULL ? t->dono->modulo : 0);
    int32_t membro;

    if (d == NULL || (t->classe == AST_USO_NOME_DE_TIPO && !EhDeclDeTipo(d))) return false;
    *u = UsoBase(t->pos, t->pos, t->nome, t->dono);
    u->alvo = d;
    u->alvo_tipo = IndiceAlvoDaDecl(d);
    membro = (t->classe == AST_USO_CASO && t->membro >= 0 && d->tipo_no == D_ENUM) ? IndiceIndiceDoMembro(d, t->membro) : -1;
    if (membro >= 0) {
        u->alvo_tipo = IND_MEMBRO;
        u->indice = membro;
    }
    return true;
}

static int CompararNumeros(int64_t a, int64_t b)
{
    return (a > b) - (a < b);
}

static int CompararUsos(const void *pa, const void *pb)
{
    const IndUso *a = pa, *b = pb;
    int r = CompararNumeros(a->pos, b->pos);

    if (r == 0) r = CompararNumeros(a->pos_nome, b->pos_nome);
    if (r == 0) r = CompararNumeros(a->alvo_tipo, b->alvo_tipo);
    if (r == 0) r = CompararNumeros(a->indice, b->indice);
    if (r == 0) r = CompararNumeros(a->decl_pos, b->decl_pos);
    if (r == 0) r = CompararNumeros(a->alvo != NULL ? a->alvo->pos : 0, b->alvo != NULL ? b->alvo->pos : 0);
    return r;
}

/* Ordem total: a chave de CompararUsos e, nos empates, a ordem de coleta (quem vem primeiro e o que RemoverRepetidos guarda). */
static int OrdenarUsos(const void *pa, const void *pb)
{
    int r = CompararUsos(pa, pb);

    return r != 0 ? r : CompararNumeros(((const IndUso *)pa)->ordem, ((const IndUso *)pb)->ordem);
}

/* A semantica gera comandos sinteticos (defer do 'em = escopo') que repetem a posicao do uso original: ficam so os distintos. */
static void RemoverRepetidos(Vec *usos)
{
    size_t saida = 0;

    for (size_t i = 0; i < usos->n; i++) {
        if (saida > 0 && CompararUsos(VecEm(usos, saida - 1), VecEm(usos, i)) == 0) continue;
        *(IndUso *)VecEm(usos, saida++) = *(const IndUso *)VecEm(usos, i);
    }
    usos->n = saida;
}

void IndiceMontar(Indice *ind, const Semantica *sem)
{
    ind->sem = sem;
    AstRefsMontar(&ind->refs, sem);
    VecIniciar(&ind->usos, sizeof(IndUso));
    for (size_t i = 0; i < ind->refs.entradas.n; i++) {
        IndUso u;
        if (UsoDeExpr(ind, VecEm(&ind->refs.entradas, i), &u)) *(IndUso *)VecEmpurrar(&ind->usos) = u;
    }
    for (size_t i = 0; i < ind->refs.tipos.n; i++) {
        IndUso u;
        if (UsoDeTipo(ind, VecEm(&ind->refs.tipos, i), &u)) *(IndUso *)VecEmpurrar(&ind->usos) = u;
    }
    for (size_t i = 0; i < ind->usos.n; i++) ((IndUso *)VecEm(&ind->usos, i))->ordem = (uint32_t)i;
    qsort(ind->usos.dados, ind->usos.n, sizeof(IndUso), OrdenarUsos);
    RemoverRepetidos(&ind->usos);
}

void IndiceLiberar(Indice *ind)
{
    VecLiberar(&ind->usos);
    AstRefsLiberar(&ind->refs);
}
