/* lower_fluxo.c - lowering dos comandos de fluxo (if, while, for, break/continue, defer, switch) e a pilha de defer.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - Linguagem v1.9 P4: for sobre bit_set desvia para LwStmtForV19.
 *            0.3.0 - Linguagem v1.7 P8: LwCaso por classe do valor (?T, ?^T, union); IrCaso.tipo sempre preenchido.
 *            0.2.0- Linguagem v1.6 P7b: for sobre lista (op_aux 0 no item: acesso com checagem de epoca).
 *            0.1.0 - Fase 5d: primeira versao (contrato em lower_interno.h; regras do arquiteto em MEM-LJ-ADR-38).
 * Verificacao: gcc -std=c11 -Wall -Wextra -Werror -I. -D__USE_MINGW_ANSI_STDIO=1 -fsyntax-only ir/lower_fluxo.c;
 *              construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include "ir/lower_interno.h"

/* ---- pilha de defer ---- */

void LwExpandirDefers(LowerCtx *c, uint32_t escopo_alvo)
{
    if (escopo_alvo >= c->escopos.n) return;
    uint32_t inicio = ((const LwEscopo *)VecEm(&c->escopos, escopo_alvo))->inicio;
    for (size_t k = c->defers.n; k > inicio; k--) {
        const Stmt *adiado = *(Stmt *const *)VecEm(&c->defers, k - 1);
        LwStmt(c, adiado->corpo);
    }
}

uint32_t LwEscopoDoLaco(LowerCtx *c)
{
    for (size_t k = c->escopos.n; k > 0; k--) {
        if (((const LwEscopo *)VecEm(&c->escopos, k - 1))->e_laco) return (uint32_t)(k - 1);
    }
    FalhaInterna("lowering: break/continue fora de laco (a semantica devia ter barrado)");
}

/* Faz o lowering de 'lista' como bloco { } dentro de 'destino', restaurando o bloco atual depois. */
static void BlocoEm(LowerCtx *c, IrBloco *destino, const Stmt *lista, bool e_laco)
{
    IrBloco *anterior = LwTrocarBloco(c, destino);
    LwBloco(c, lista, e_laco);
    LwTrocarBloco(c, anterior);
}

void LwStmtDefer(LowerCtx *c, const Stmt *s)
{
    if (s->corpo == NULL) LwFalhaStmt(c, s, "defer sem comando");
    *(const Stmt **)VecEmpurrar(&c->defers) = s;
}

/* ---- if / while / for ---- */

/* 'else if' chega como senao com um unico ST_IF: o bloco do senao o trata igual a qualquer outro comando. */
void LwStmtIf(LowerCtx *c, const Stmt *s)
{
    Valor condicao = LwExpr(c, s->expr);
    IrInstr *i = LwNovaInstr(c, IR_SE, s->pos);
    i->a = condicao;
    BlocoEm(c, &i->corpo, s->corpo, false);
    BlocoEm(c, &i->senao, s->senao, false);
}

/* A condicao fica no bloco 'cond' da instrucao: o backend a recalcula a cada volta. */
void LwStmtWhile(LowerCtx *c, const Stmt *s)
{
    IrInstr *i = LwNovaInstr(c, IR_ENQUANTO, s->pos);
    IrBloco *anterior = LwTrocarBloco(c, &i->cond);
    i->a = LwExpr(c, s->expr);
    LwTrocarBloco(c, anterior);
    BlocoEm(c, &i->corpo, s->corpo, true);
}

/* Prologo do corpo do for de colecao: o item (copia ou ponteiro) lido do elemento da volta; acesso provado (0..<n). */
static void PrologoDoItem(LowerCtx *c, const Stmt *s, Valor base, int32_t contador)
{
    IrInstr *e = LwNovaInstr(c, s->item_ref ? IR_ENDERECO_ELEMENTO : IR_ELEMENTO, s->pos);
    e->destino = LwNovoTemp(c);
    e->tipo = s->tipo_local;
    e->a = base;
    e->b = LwValorLocal(contador, TIPO_INT);
    e->op_aux = LwEhLista(c, base.tipo) ? 0 : 1; /* lista: o tamanho vem da leitura inicial; em dev o emissor confere a epoca */
    IrInstr *l = LwNovaInstr(c, IR_LOCAL, s->pos);
    l->destino = s->local_id;
    l->tipo = s->tipo_local;
    l->a = LwValorTemp(e->destino, e->tipo);
    l->mutavel = false;
}

/* for item / &item / i, item sobre array ou fatia: laco 0..<n sobre um contador (o do indice, ou um local sintetico). */
static void LwStmtForColecao(LowerCtx *c, const Stmt *s)
{
    if (__builtin_expect(TiposTemV19(&c->sem->tipos), 0) && LwEhV19(c, s->expr->tipo)) {
        LwStmtForV19(c, s);
        return;
    }
    Valor n;
    Valor base = LwBaseDaColecao(c, s->expr, &n);
    int32_t contador = s->local_indice >= 0 ? s->local_indice : LwNovoLocal(c);
    IrInstr *i = LwNovaInstr(c, IR_PARA, s->pos);
    i->destino = contador;
    i->tipo = TIPO_INT;
    i->a = LwValorInt(0, TIPO_INT);
    i->b = n;
    i->faixa_excl = true;
    IrBloco *anterior = LwTrocarBloco(c, &i->corpo);
    PrologoDoItem(c, s, base, contador);
    LwBloco(c, s->corpo, true);
    LwTrocarBloco(c, anterior);
}

/* Inicio e fim saem antes da instrucao: calculados uma vez so. */
void LwStmtFor(LowerCtx *c, const Stmt *s)
{
    if (s->expr2 == NULL) {
        LwStmtForColecao(c, s);
        return;
    }
    if (s->tipo_local == TIPO_NAO_RESOLVIDO) LwFalhaStmt(c, s, "for sem tipo do contador");
    Valor inicio = LwExpr(c, s->expr);
    Valor fim = LwExpr(c, s->expr2);
    IrInstr *i = LwNovaInstr(c, IR_PARA, s->pos);
    i->destino = s->local_id;
    i->tipo = s->tipo_local;
    i->a = inicio;
    i->b = fim;
    i->faixa_excl = s->faixa_excl;
    BlocoEm(c, &i->corpo, s->corpo, true);
}

/* ---- break / continue ---- */

/* Os defers saem ate o corpo do laco mais interno, inclusive; depois o pare/continua. */
void LwStmtBreak(LowerCtx *c, const Stmt *s)
{
    LwExpandirDefers(c, LwEscopoDoLaco(c));
    LwNovaInstr(c, IR_PARE, s->pos);
}

void LwStmtContinue(LowerCtx *c, const Stmt *s)
{
    LwExpandirDefers(c, LwEscopoDoLaco(c));
    LwNovaInstr(c, IR_CONTINUA, s->pos);
}

/* ---- switch ---- */

/* Corpo do case. A semantica cria um local de ligacao em todo case de 'switch r in'; o emissor so liga o de resultado que nao e
 * default (ligacao >= 0). Nos demais o local nasce aqui, no comeco do corpo, com o valor inteiro do switch. */
static void CorpoDoCaso(LowerCtx *c, const Caso *origem, IrCaso *destino, Valor valor)
{
    IrBloco *anterior = LwTrocarBloco(c, &destino->corpo);
    if (origem->local_id >= 0 && destino->ligacao < 0) {
        IrInstr *i = LwNovaInstr(c, IR_LOCAL, origem->pos);
        i->destino = origem->local_id;
        i->tipo = origem->tipo;
        i->a = valor;
    }
    LwBloco(c, origem->corpo, false);
    LwTrocarBloco(c, anterior);
}

/* Membro e ligacao do case por classe do valor: resultado = TipoId do caso; ?T/?^T = 1 valor / 0 none (so o valor liga);
 * union = tag da variante; enum = indice do membro. */
static void LigarCaso(const Caso *origem, IrCaso *destino, TipoClasse classe)
{
    if (classe == TC_RESULTADO) {
        destino->membro = (int32_t)origem->tipo;
        destino->ligacao = origem->local_id;
    } else if (classe == TC_OPCIONAL || classe == TC_ANULAVEL) {
        destino->membro = origem->indice_membro;
        destino->ligacao = origem->indice_membro == 1 ? origem->local_id : -1;
    } else if (classe == TC_UNIAO) {
        destino->membro = origem->indice_membro;
        destino->ligacao = origem->local_id;
    } else {
        destino->membro = origem->indice_membro;
    }
}

static void LwCaso(LowerCtx *c, const Caso *origem, IrCaso *destino, Valor valor)
{
    TipoClasse classe = TipoClasseDe(&c->sem->tipos, valor.tipo);
    destino->padrao = origem->padrao;
    destino->ligacao = -1;
    destino->tipo = origem->tipo; /* tipo da ligacao: o do case; no default e no none, o valor inteiro */
    if (!origem->padrao) LigarCaso(origem, destino, classe);
    CorpoDoCaso(c, origem, destino, valor);
}

void LwStmtSwitch(LowerCtx *c, const Stmt *s)
{
    Valor valor = LwExpr(c, s->expr);
    IrInstr *i = LwNovaInstr(c, IR_CHAVEIA, s->pos);
    i->a = valor;
    i->tipo = s->expr->tipo;
    i->ncasos = s->ncasos;
    i->casos = LwAlocar(c, s->ncasos * sizeof(IrCaso));
    for (uint32_t k = 0; k < s->ncasos; k++) LwCaso(c, &s->casos[k], &i->casos[k], valor);
}
