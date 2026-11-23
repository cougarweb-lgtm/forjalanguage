/* lower_decl.c - declaracoes no lowering: tabelas por Decl.indice e IrStruct, IrEnum e IrConst do programa.
 * Versao: 0.4.3
 * Changelog: 0.4.3 - B01a: LwDeclaracoes registra a struct oculta do Allocator (Semantica.decl_allocator).
 *            0.4.2 - Linguagem v1.11 L15a: LwDeclaracoes registra as structs ocultas de [dynamic, N]T (Semantica.decls_listas_fixas).
 *            0.4.1 - Linguagem v1.11 L14a: LwDeclaracoes registra a struct oculta do quat (Semantica.decl_quat).
 *            0.4.0 -Linguagem v1.11 L9: LwDeclaracoes calcula o layout (LwLayoutCalcular) entre as structs e as constantes e confere os static_assert de topo no fim.
 *            0.3.0 - Linguagem v1.8b P3b: PreencherStruct grava opaco; PreencherEnum grava valores (enum com valor).
 *            0.2.0 - Linguagem v1.7 P8: D_UNIAO -> IrUniao em IrPrograma.unioes.
 *            0.1.0- Fase 5d: primeira versao. Const escalar e dobrada na ordem do fonte (pode usar as anteriores); const de
 *                    struct nao entra em IrPrograma.consts (o uso refaz o lowering do init).
 * Verificacao: construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include "ir/lower_interno.h"

/* ---------- tabelas por indice ---------- */

static void AlocarTabelas(LowerCtx *c)
{
    const Semantica *s = c->sem;
    c->procs = LwAlocar(c, s->qtd_proc * sizeof(Decl *));
    c->structs = LwAlocar(c, s->qtd_struct * sizeof(Decl *));
    c->enums = LwAlocar(c, s->qtd_enum * sizeof(Decl *));
    c->consts = LwAlocar(c, s->qtd_const * sizeof(Decl *));
    c->const_valor = LwAlocar(c, s->qtd_const * sizeof(Valor));
    c->const_escalar = LwAlocar(c, s->qtd_const * sizeof(bool));
    c->prog->nunioes = s->qtd_uniao;
    c->prog->unioes = LwAlocar(c, s->qtd_uniao * sizeof(IrUniao));
}

/* union: variantes na ordem do fonte (tag = indice); o TipoId da union ja foi criado pela semantica. */
static void PreencherUniao(LowerCtx *c, const Decl *d)
{
    IrUniao *saida = &c->prog->unioes[d->indice];
    saida->nome = d->chave; /* IA1-L8: struct, enum, union e const levam a chave por modulo */
    saida->tipo = TipoUniao(&c->sem->tipos, d->chave);
    saida->nvariantes = d->ncampos;
    saida->variantes = LwAlocar(c, d->ncampos * sizeof(TipoId));
    for (uint32_t k = 0; k < d->ncampos; k++) saida->variantes[k] = d->campos[k].tipo.tipo;
}

static bool ConstEhEscalar(const LowerCtx *c, const Decl *d)
{
    TipoClasse classe = TipoClasseDe(&c->sem->tipos, d->tipo.tipo);
    return classe != TC_STRUCT && classe != TC_ARRAY;
}

static void RegistrarConst(LowerCtx *c, Decl *d)
{
    c->consts[d->indice] = d;
    c->const_escalar[d->indice] = ConstEhEscalar(c, d);
    if (c->const_escalar[d->indice]) c->prog->nconsts++;
}

static void RegistrarDeclaracao(LowerCtx *c, Decl *d)
{
    switch (d->tipo_no) {
    case D_PROC: c->procs[d->indice] = d; break;
    case D_STRUCT: c->structs[d->indice] = d; break;
    case D_ENUM: c->enums[d->indice] = d; break;
    case D_CONST: RegistrarConst(c, d); break;
    case D_UNIAO: PreencherUniao(c, d); break;
    default: break;
    }
}

/* ---------- structs e enums ---------- */

static void PreencherStruct(LowerCtx *c, IrStruct *saida, const Decl *d)
{
    saida->nome = d->chave;
    saida->ncampos = d->ncampos;
    saida->opaco = d->externo;
    saida->campos = LwAlocar(c, d->ncampos * sizeof(IrCampo));
    for (uint32_t k = 0; k < d->ncampos; k++) {
        const Campo *f = &d->campos[k];
        saida->campos[k] = (IrCampo){f->nome, f->tipo.tipo, f->tem_faixa, f->faixa_min, f->faixa_max, f->faixa_excl};
    }
}

static void PreencherEnum(LowerCtx *c, IrEnum *saida, const Decl *d)
{
    saida->nome = d->chave;
    saida->nmembros = d->nmembros;
    saida->membros = LwAlocar(c, d->nmembros * sizeof(int32_t));
    for (uint32_t k = 0; k < d->nmembros; k++) saida->membros[k] = d->membros[k].nome;
    saida->valores = NULL;
    if (d->op != 1) return;
    saida->valores = LwAlocar(c, d->nmembros * sizeof(int64_t));
    for (uint32_t k = 0; k < d->nmembros; k++) saida->valores[k] = d->membros[k].valor;
}

static void PreencherStructsEEnums(LowerCtx *c)
{
    IrPrograma *p = c->prog;
    p->nstructs = c->sem->qtd_struct;
    p->nenums = c->sem->qtd_enum;
    p->structs = LwAlocar(c, p->nstructs * sizeof(IrStruct));
    p->enums = LwAlocar(c, p->nenums * sizeof(IrEnum));
    for (uint32_t k = 0; k < p->nstructs; k++) PreencherStruct(c, &p->structs[k], c->structs[k]);
    for (uint32_t k = 0; k < p->nenums; k++) PreencherEnum(c, &p->enums[k], c->enums[k]);
}

/* ---------- constantes ---------- */

/* Dobra o init da const escalar; um init que nao e constante e erro do programa (nao do compilador). */
static void DobrarConstante(LowerCtx *c, const Decl *d, IrConst *saida)
{
    Valor v;
    if (!LwDobrar(c, d->init, &v)) {
        if (!c->sem->diag->tem) {
            DiagRelatar(c->sem->diag, E0301, d->init->pos, "o inicializador da constante '%s' precisa ser um valor constante",
                        InternerTexto(c->sem->nomes, d->nome));
        }
        return;
    }
    c->const_valor[d->indice] = v;
    *saida = (IrConst){d->chave, d->tipo.tipo, v};
}

/* Ordem do fonte: uma const pode usar as anteriores (Decl.indice das escalares segue a mesma ordem). */
static void PreencherConstantes(LowerCtx *c)
{
    uint32_t n = 0;
    c->prog->consts = LwAlocar(c, c->prog->nconsts * sizeof(IrConst));
    for (Decl *d = c->sem->programa->primeira; d != NULL && !c->sem->diag->tem; d = d->prox) {
        if (d->tipo_no == D_CONST && c->const_escalar[d->indice]) DobrarConstante(c, d, &c->prog->consts[n++]);
    }
}

void LwDeclaracoes(LowerCtx *c)
{
    AlocarTabelas(c);
    c->prog->nconsts = 0;
    for (Decl *d = c->sem->programa->primeira; d != NULL; d = d->prox) RegistrarDeclaracao(c, d);
    if (c->sem->decl_quat != NULL) RegistrarDeclaracao(c, c->sem->decl_quat); /* v1.11 L14a: a struct oculta do quat nao esta na lista do programa */
    if (c->sem->decl_allocator != NULL) RegistrarDeclaracao(c, c->sem->decl_allocator); /* B01a: idem para a struct oculta do Allocator */
    for (uint32_t k = 0; k < c->sem->qtd_listas_fixas; k++) RegistrarDeclaracao(c, c->sem->decls_listas_fixas[k]); /* v1.11 L15a: as structs ocultas de [dynamic, N]T */
    PreencherStructsEEnums(c);
    LwLayoutCalcular(c); /* v1.11 L9: size_of/align_of/offset_of das const e dos corpos precisam das structs prontas */
    PreencherConstantes(c);
    LwAssertsDeTopo(c);
}
