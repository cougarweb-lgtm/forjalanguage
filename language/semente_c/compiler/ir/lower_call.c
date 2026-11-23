/* lower_call.c - lowering de chamadas (procs e embutidos print/assert/panic), try e or_else.
 * Versao: 0.23.0
 * Changelog: 0.23.0 - M6c P88: LwChamada emite IR_HANDLE_SOLTA antes e IR_HANDLE_NASCE depois do foreign com clausula (lower_handle.c).
 *            0.22.0 - K15: EXATOS ganha EMB_SET_UNION, EMB_SET_INTERSECT e EMB_SET_DIFF (LwSet*, lower_lista.c), prototipos locais.
 *            0.21.0 - K09: EXATOS ganha EMB_INDEX_BY, EMB_FIND_BY, EMB_ANY_BY, EMB_ALL_BY e EMB_COUNT_BY (Lw*By, lower_lista.c), prototipos locais.
 *            0.20.0 - K17: EXATOS ganha EMB_HEAP_PUSH, EMB_HEAP_PUSH_BY, EMB_HEAP_POP e EMB_HEAP_POP_BY (LwHeap*, lower_lista.c), prototipos locais.
 *            0.19.0 - K08c: EXATOS ganha EMB_DEDUP (LwDedup, lower_lista.c), prototipo local; lower_interno.h segue CONGELADO.
 *            0.18.0 - S2b: EXATOS ganha EMB_SHUFFLE e EMB_CHOICE (LwShuffle e LwChoice, lower_sorteio.c), prototipos locais.
 *            0.17.0 - K08b: EXATOS ganha EMB_SORT_BY_KEY (LwSortByKey) e EMB_BINARY_SEARCH (LwBinarySearch), prototipos locais.
 *            0.16.0 - K08a: EXATOS ganha EMB_SORT_BY (LwSortBy, lower_memoria.c; prototipo local, lower_interno.h congelado).
 *            0.15.0 - IA1-L9f: LowerFormato desvia format(into =) (tipo vazio) para LwFormatoInto (lower_texto_builder.c).
 *            0.14.0- IA1-L9e: FAIXAS ganha EMB_MIN_OF (LwEstat; lower_estat.c, lower_mapa_vistas.c e lower_texto_bytes.c).
 *            0.13.0 - IA1-L9c/L9d: FAIXAS ganha EMB_CAP (LwLista; lower_lista.c e lower_busca.c).
 *            0.12.0 - IA1-L9a/L9b: FAIXAS ganha EMB_WRAPPING_ADD (LwAritmetica; lower_aritmetica.c e lower_bits.c).
 *            0.11.0 - IA1-L5b: EXATOS ganha EMB_RESIZE (LwResize) e EMB_SORT (LwSort).
 *            0.10.0 - IA1-L5a: eprint (EMB_EPRINT -> LowerEPrint: IR_FORMATA op_aux 3, stderr).
 *            0.9.0 - Linguagem v1.12 M3 P21v-b: LwChamadaInd (EX_CHAMADA_IND -> IR_CHAMADA_IND: o chamado e avaliado antes dos argumentos; o tipo proc nao tem padrao).
 *            0.8.2 -Linguagem v1.11 L14b: abs(q) (EMB_ABS com argumento quat) despachado para LwAbsQuat.
 *            0.8.1 - Linguagem v1.11 L14a: conj (EMB_CONJ) despachado para LwConjQuat.
 *            0.8.0 -Linguagem v1.11 L13: min/max/abs/clamp/sqrt (EMB_MIN em diante) despachados para LwMatematica.
 *            0.7.0 -Linguagem v1.11 L9: size_of/align_of/offset_of/static_assert (EMB_SIZE_OF em diante) despachados para LwLayoutChamada.
 *            0.6.0 - Linguagem v1.9 P4: insert/remove/has (EMB_INSERT em diante) despachados para LwEmbutidoV19.
 *            0.5.0 - Linguagem v1.7 P8: format/format_c e print de f"..." (LwFormata); or_else de ?T e ?^T (IR_OPC_TEM/IR_OPC_VALOR).
 *            0.4.0- Linguagem v1.6 P7a: new/make/append/delete/frame_reset despachados para LwMemoria.
 *            0.3.0 - Linguagem v1.3 P6: LowerLen (IR_TAMANHO_TEXTO); mensagem vazia do assert e cstr.
 *            0.2.0 - Linguagem v1.2: embutido transmute (IR_TRANSMUTA).
 *            0.1.1 - Fase 5g-M: struct local passado por valor e copiado na ordem do fonte (antes de um argumento seguinte que
 *                    o altera por ponteiro). Caso: tests\lowering\72_argumento_struct_copiado_na_ordem.fj.
 *            0.1.0 - Fase 5d: primeira versao. Argumentos na ordem do fonte, gravados no slot do parametro; padrao do parametro
 *                    avaliado no chamador; try expande os defers de todos os escopos antes de devolver o erro.
 * Verificacao: construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include "ir/lower_interno.h"
#include "ir/lower_lista.h"
#include "ir/lower_ops.h"

/* M6c P88 (lower_handle.c; lower_interno.h segue CONGELADO) */
void LwHandleSolta(LowerCtx *c, const Decl *proc, const Valor *args, uint32_t pos);
void LwHandleNasce(LowerCtx *c, const Decl *proc, const Valor *args, const IrInstr *chamada, uint32_t pos);
void LwLibEncerra(LowerCtx *c, const Decl *proc, uint32_t pos); /* M6d P92e: IR_LIB_ENCERRA depois do foreign `ends` (lower_handle.c) */

typedef void (*LowerEmbutido)(LowerCtx *c, const Expr *e);

static bool UsaVariavelLocal(const Expr *e);

/* ---------- apoio ---------- */

/* Instrucao com destino em temporario novo; chamar depois de calcular os operandos. */
static IrInstr *InstrComTemp(LowerCtx *c, IrOp op, const Expr *e, TipoId tipo)
{
    IrInstr *i = LwNovaInstr(c, op, e->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = tipo;
    return i;
}

static Valor ResultadoTemp(const IrInstr *i)
{
    return LwValorTemp(i->destino, i->tipo);
}

static void AtribuirLocal(LowerCtx *c, int32_t local, Valor v, uint32_t pos)
{
    IrInstr *i = LwNovaInstr(c, IR_ATRIBUICAO, pos);
    i->destino = local;
    i->tipo = v.tipo;
    i->a = v;
}

/* ---------- argumentos ---------- */

/* Struct local passado por valor: um argumento seguinte pode alterar o original por ponteiro (f(c, b = mexe(&c))), entao a
 * copia sai aqui, na ordem do fonte. Escalar nao precisa: so struct tem endereco (&x). */
static Valor CopiarStructLocal(LowerCtx *c, Valor v, uint32_t pos)
{
    if (v.tipo_no != V_LOCAL || TipoClasseDe(&c->sem->tipos, v.tipo) != TC_STRUCT) return v;
    IrInstr *i = LwNovaInstr(c, IR_LOCAL, pos);
    i->destino = LwNovoLocal(c);
    i->tipo = v.tipo;
    i->a = v;
    return LwValorLocal(i->destino, v.tipo);
}

/* Avalia os argumentos na ordem do fonte; cada valor vai para o slot Arg.indice. Slot vazio = V_NENHUM. */
static Valor *AvaliarArgumentos(LowerCtx *c, const Expr *e, uint32_t nslots)
{
    Valor *slots = LwArgs(c, nslots);
    for (uint32_t i = 0; i < e->nargs; i++) {
        const Arg *g = &e->args[i];
        if (g->indice < 0 || (uint32_t)g->indice >= nslots) {
            FalhaInterna("lowering: argumento sem indice de parametro (offset %u)", (unsigned)g->valor->pos_op);
        }
        slots[g->indice] = LwExpr(c, g->valor);
        if (i + 1 < e->nargs) slots[g->indice] = CopiarStructLocal(c, slots[g->indice], g->valor->pos_op);
    }
    return slots;
}

static bool UsaLocalNosArgumentos(const Expr *e)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (UsaVariavelLocal(e->args[i].valor)) return true;
    }
    return false;
}

/* O padrao roda no chamador: nome de local ou parametro apontaria para a funcao errada. */
static bool UsaVariavelLocal(const Expr *e)
{
    if (e == NULL) return false;
    if (e->tipo_no == EX_NOME) return e->ref == REF_LOCAL || e->ref == REF_PARAM;
    return UsaVariavelLocal(e->a) || UsaVariavelLocal(e->b) || UsaLocalNosArgumentos(e);
}

/* Parametro sem argumento: o padrao do proc chamado, avaliado aqui (no chamador). */
static void PreencherPadroes(LowerCtx *c, const Decl *proc, Valor *slots, uint32_t pos)
{
    if (c->sem->diag->tem) return;
    for (uint32_t k = 0; k < proc->nparams; k++) {
        const Expr *padrao = proc->params[k].padrao;
        if (slots[k].tipo_no != V_NENHUM) continue;
        if (padrao == NULL || UsaVariavelLocal(padrao)) {
            FalhaInterna("lowering: parametro %u do proc sem valor nem padrao utilizavel (offset %u)", (unsigned)k, (unsigned)pos);
        }
        slots[k] = LwExpr(c, padrao);
    }
}

/* ---------- embutidos ---------- */

static void LowerPrint(LowerCtx *c, const Expr *e)
{
    if (e->args[0].valor->tipo_no == EX_FTEXTO) {
        IrInstr *formata;
        LwFormata(c, e->args[0].valor, 0, LwValorInt(0, TIPO_U32), &formata);
        return;
    }
    Valor v = LwExpr(c, e->args[0].valor);
    LwNovaInstr(c, IR_IMPRIME, e->pos_op)->a = v;
}

/* eprint(x): a semantica ja trocou o argumento por f"..." (SemFTextoDoArgumento); sai em stderr com '\n' (IR_FORMATA op_aux 3). */
static Valor LowerEPrint(LowerCtx *c, const Expr *e)
{
    IrInstr *formata;
    LwFormata(c, e->args[0].valor, 3, LwValorInt(0, TIPO_U32), &formata);
    return LwValorNenhum();
}

/* assert(cond, msg = ""): sem mensagem, texto vazio. */
static void LowerAssert(LowerCtx *c, const Expr *e)
{
    Valor *slots = AvaliarArgumentos(c, e, 2);
    IrInstr *i = LwNovaInstr(c, IR_ASSERT, e->pos_op);
    i->a = slots[0];
    i->b = slots[1].tipo_no == V_NENHUM ? LwValorTexto(Internar(c->sem->nomes, "", 0), TIPO_CSTR) : slots[1];
}

static void LowerPanic(LowerCtx *c, const Expr *e)
{
    Valor *slots = AvaliarArgumentos(c, e, 1);
    LwNovaInstr(c, IR_PANIC, e->pos_op)->a = slots[0];
}

static const LowerEmbutido EMBUTIDOS[] = {[EMB_PRINT] = LowerPrint, [EMB_ASSERT] = LowerAssert, [EMB_PANIC] = LowerPanic};

/* ---------- chamada ---------- */

/* transmute(x, para = T): so o valor e avaliado; 'para' e so tipo (ja em e->tipo). */
static Valor LowerTransmute(LowerCtx *c, const Expr *e)
{
    Valor origem = LwExpr(c, e->args[0].valor);
    IrInstr *i = InstrComTemp(c, IR_TRANSMUTA, e, e->tipo);
    i->a = origem;
    return ResultadoTemp(i);
}

/* len(s): bytes do str, lidos do campo len; len de array e ^[N]T e a constante N. */
static Valor LowerLen(LowerCtx *c, const Expr *e)
{
    if (e->args[0].valor->tipo != TIPO_TEXTO) return LwTamanhoColecao(c, e->args[0].valor);
    Valor texto = LwExpr(c, e->args[0].valor);
    IrInstr *i = InstrComTemp(c, IR_TAMANHO_TEXTO, e, TIPO_INT);
    i->a = texto;
    return ResultadoTemp(i);
}

/* slice_from(p, len = n): p ja e ponteiro; n i32 vira int (sempre cabe). Ordem de avaliacao: a do fonte. */
static Valor LowerSliceFrom(LowerCtx *c, const Expr *e)
{
    Valor *slots = AvaliarArgumentos(c, e, 2);
    Valor quantidade = slots[1];
    if (quantidade.tipo != TIPO_INT && quantidade.tipo_no == V_INT) {
        quantidade.tipo = TIPO_INT;
    } else if (quantidade.tipo != TIPO_INT) {
        IrInstr *conversao = InstrComTemp(c, IR_CONVERSAO, e, TIPO_INT);
        conversao->a = quantidade;
        quantidade = ResultadoTemp(conversao);
    }
    IrInstr *i = InstrComTemp(c, IR_FATIA_DE_PONTEIRO, e, e->tipo);
    i->a = slots[0];
    i->b = quantidade;
    return ResultadoTemp(i);
}

/* format(f"...", em = A) -> str; format_c -> cstr; o destino e o temporario da instrucao. */
static Valor LowerFormato(LowerCtx *c, const Expr *e)
{
    IrInstr *i;
    if (e->tipo == TIPO_VAZIO) return LwFormatoInto(c, e); /* format(f"...", into = &buf): instrucao sem valor (L9f) */
    LwFormata(c, e->args[0].valor, e->ref_id == EMB_FORMAT ? 1 : 2, LwAlocador(c, e), &i);
    return ResultadoTemp(i);
}

typedef Valor (*TratadorEmbutido)(LowerCtx *c, const Expr *e);

/* K08a: prototipo local (lower_memoria.c); lower_interno.h esta CONGELADO e so o main o altera. */
Valor LwSortBy(LowerCtx *c, const Expr *e);
/* K08b: prototipos locais (lower_memoria.c); lower_interno.h segue CONGELADO. */
Valor LwSortByKey(LowerCtx *c, const Expr *e);
Valor LwBinarySearch(LowerCtx *c, const Expr *e);
/* S2b: prototipos locais (lower_sorteio.c); lower_interno.h segue CONGELADO. */
Valor LwShuffle(LowerCtx *c, const Expr *e);
Valor LwChoice(LowerCtx *c, const Expr *e);
/* K08c: prototipo local (lower_lista.c); lower_interno.h segue CONGELADO. */
Valor LwDedup(LowerCtx *c, const Expr *e);
/* K17: prototipos locais (lower_lista.c); lower_interno.h segue CONGELADO. */
Valor LwHeapPush(LowerCtx *c, const Expr *e);
Valor LwHeapPushBy(LowerCtx *c, const Expr *e);
Valor LwHeapPop(LowerCtx *c, const Expr *e);
Valor LwHeapPopBy(LowerCtx *c, const Expr *e);
/* K09: index_by, find_by, any_by, all_by e count_by (lower_lista.c). */
Valor LwIndexBy(LowerCtx *c, const Expr *e);
Valor LwFindBy(LowerCtx *c, const Expr *e);
Valor LwAnyBy(LowerCtx *c, const Expr *e);
Valor LwAllBy(LowerCtx *c, const Expr *e);
Valor LwCountBy(LowerCtx *c, const Expr *e);
/* K15: set_union, set_intersect e set_diff sobre map[K]bool (lower_lista.c). */
Valor LwSetUnion(LowerCtx *c, const Expr *e);
Valor LwSetIntersect(LowerCtx *c, const Expr *e);
Valor LwSetDiff(LowerCtx *c, const Expr *e);

/* Embutidos com tratador proprio, por id exato (vencem as faixas: EMB_CONJ, p.ex., esta dentro da faixa de EMB_MIN). */
static const TratadorEmbutido EXATOS[] = {
    [EMB_FORMAT] = LowerFormato, [EMB_FORMAT_C] = LowerFormato, [EMB_SLICE_FROM] = LowerSliceFrom,
    [EMB_CONJ] = LwConjQuat,     [EMB_TRANSMUTE] = LowerTransmute, [EMB_LEN] = LowerLen,
    [EMB_EPRINT] = LowerEPrint,
    [EMB_RESIZE] = LwResize,     [EMB_SORT] = LwSort,
    [EMB_SORT_BY] = LwSortBy,    [EMB_SORT_BY_KEY] = LwSortByKey,
    [EMB_BINARY_SEARCH] = LwBinarySearch,
    [EMB_SHUFFLE] = LwShuffle,   [EMB_CHOICE] = LwChoice,
    [EMB_DEDUP] = LwDedup,
    [EMB_HEAP_PUSH] = LwHeapPush, [EMB_HEAP_PUSH_BY] = LwHeapPushBy,
    [EMB_HEAP_POP] = LwHeapPop,   [EMB_HEAP_POP_BY] = LwHeapPopBy,
    [EMB_INDEX_BY] = LwIndexBy,   [EMB_FIND_BY] = LwFindBy,
    [EMB_ANY_BY] = LwAnyBy,       [EMB_ALL_BY] = LwAllBy,
    [EMB_COUNT_BY] = LwCountBy,
    [EMB_SET_UNION] = LwSetUnion, [EMB_SET_INTERSECT] = LwSetIntersect,
    [EMB_SET_DIFF] = LwSetDiff,
};

/* Grupos de ids por intervalo: vale o 1o cuja base e <= ao id, entao a tabela vai do maior grupo para o menor. */
static const struct { int32_t base; TratadorEmbutido tratar; } FAIXAS[] = {
    {EMB_MIN_OF, LwEstat}, {EMB_CAP, LwLista}, {EMB_WRAPPING_ADD, LwAritmetica}, {EMB_MIN, LwMatematica}, {EMB_SIZE_OF, LwLayoutChamada}, {EMB_INSERT, LwEmbutidoV19}, {EMB_NEW, LwMemoria},
};

static TratadorEmbutido TratadorExato(int32_t id)
{
    if (id < 0 || (size_t)id >= sizeof EXATOS / sizeof EXATOS[0]) return NULL;
    return EXATOS[id];
}

static TratadorEmbutido TratadorDaFaixa(int32_t id)
{
    for (size_t k = 0; k < sizeof FAIXAS / sizeof FAIXAS[0]; k++) {
        if (id >= FAIXAS[k].base) return FAIXAS[k].tratar;
    }
    return NULL;
}

/* abs(q) com q quat: a norma (v1.11 L14b); o abs de numero fica na faixa de EMB_MIN. */
static bool EhAbsDeQuat(const LowerCtx *c, const Expr *e)
{
    return e->ref_id == EMB_ABS && c->sem->tipo_quat != 0 && e->args[0].valor->tipo == c->sem->tipo_quat;
}

/* O tratador de e (REF_EMBUTIDO) que devolve Valor, ou NULL (print, assert e panic, que nao devolvem, ficam em EMBUTIDOS). */
static TratadorEmbutido TratadorDe(const LowerCtx *c, const Expr *e)
{
    TratadorEmbutido exato = TratadorExato(e->ref_id);
    if (exato != NULL) return exato;
    if (EhAbsDeQuat(c, e)) return LwAbsQuat;
    return TratadorDaFaixa(e->ref_id);
}

Valor LwChamada(LowerCtx *c, const Expr *e)
{
    TratadorEmbutido tratador = e->ref == REF_EMBUTIDO ? TratadorDe(c, e) : NULL;
    if (tratador != NULL) return tratador(c, e);
    if (e->ref == REF_EMBUTIDO && e->ref_id >= 0 && (size_t)e->ref_id < sizeof EMBUTIDOS / sizeof EMBUTIDOS[0]) {
        EMBUTIDOS[e->ref_id](c, e);
        return LwValorNenhum();
    }
    if (e->ref != REF_PROC) FalhaInterna("lowering: chamada sem ligacao da semantica (offset %u)", (unsigned)e->pos_op);
    const Decl *proc = c->procs[e->ref_id];
    Valor *args = AvaliarArgumentos(c, e, proc->nparams);
    PreencherPadroes(c, proc, args, e->pos_op);
    if (proc->posse_c != POSSE_C_NENHUMA) LwHandleSolta(c, proc, args, e->pos_op); /* M6c P88: rastreio automatico do handle */
    IrInstr *i = LwNovaInstr(c, IR_CHAMADA, e->pos_op);
    i->nome = proc->chave;
    i->tipo = e->tipo;
    i->args = args;
    i->nargs = proc->nparams;
    if (e->tipo != TIPO_VAZIO) i->destino = LwNovoTemp(c);
    if (proc->posse_c != POSSE_C_NENHUMA) {
        LwHandleNasce(c, proc, args, i, e->pos_op);
        LwLibEncerra(c, proc, e->pos_op); /* M6d P92e: `ends` */
    }
    return e->tipo == TIPO_VAZIO ? LwValorNenhum() : ResultadoTemp(i);
}

/* f(a, b = x) / obj.campo(a): o chamado (nome, campo, chamada...) e avaliado antes dos argumentos; o tipo proc nao tem padrao (todo slot vem preenchido). */
Valor LwChamadaInd(LowerCtx *c, const Expr *e)
{
    Valor proc = LwExpr(c, e->a);
    uint32_t nparams = TipoProcQtd(&c->sem->tipos, e->a->tipo);
    Valor *args = AvaliarArgumentos(c, e, nparams);
    IrInstr *i = LwNovaInstr(c, IR_CHAMADA_IND, e->pos_op);

    i->a = proc;
    i->tipo = e->tipo;
    i->args = args;
    i->nargs = nparams;
    if (e->tipo == TIPO_VAZIO) return LwValorNenhum();
    i->destino = LwNovoTemp(c);
    return ResultadoTemp(i);
}

/* ---------- try ---------- */

/* Erro do resultado repassado: defers de todos os escopos rodam antes; o retorno usa o resultado do PROC (T pode diferir). */
static void RetornarErro(LowerCtx *c, const Expr *e, Valor resultado)
{
    IrInstr *erro = InstrComTemp(c, IR_RES_ERRO_DE, e, TipoDe(&c->sem->tipos, e->a->tipo)->b);
    erro->a = resultado;
    LwExpandirDefers(c, 0);
    IrInstr *novo = InstrComTemp(c, IR_RES_ERRO, e, c->decl->retorno.tipo);
    novo->a = ResultadoTemp(erro);
    LwNovaInstr(c, IR_RETORNA, e->pos_op)->a = ResultadoTemp(novo);
}

Valor LwTry(LowerCtx *c, const Expr *e)
{
    Valor resultado = LwExpr(c, e->a);
    IrInstr *e_erro = InstrComTemp(c, IR_RES_E_ERRO, e, TIPO_BOOL);
    e_erro->a = resultado;
    IrInstr *se = LwNovaInstr(c, IR_SE, e->pos_op);
    se->a = ResultadoTemp(e_erro);
    IrBloco *anterior = LwTrocarBloco(c, &se->corpo);
    RetornarErro(c, e, resultado);
    LwTrocarBloco(c, anterior);
    if (e->tipo == TIPO_VAZIO) return LwValorNenhum();
    IrInstr *valor = InstrComTemp(c, IR_RES_VALOR, e, e->tipo);
    valor->a = resultado;
    return ResultadoTemp(valor);
}

/* ---------- or_else ---------- */

/* local L = (erro ? padrao : valor); o padrao so e calculado dentro do bloco do erro.
 * Resultado: IR_RES_E_ERRO + IR_RES_VALOR. ?T e ?^T: IR_OPC_TEM (op_aux 1 = e none) + IR_OPC_VALOR. */
Valor LwOrElse(LowerCtx *c, const Expr *e)
{
    Valor resultado = LwExpr(c, e->a);
    bool opcional = TipoClasseDe(&c->sem->tipos, e->a->tipo) != TC_RESULTADO;
    int32_t local = LwNovoLocal(c);
    IrInstr *def = LwNovaInstr(c, IR_LOCAL, e->pos_op);
    def->destino = local;
    def->tipo = e->tipo;
    def->mutavel = true;
    IrInstr *e_erro = InstrComTemp(c, opcional ? IR_OPC_TEM : IR_RES_E_ERRO, e, TIPO_BOOL);
    e_erro->a = resultado;
    if (opcional) e_erro->op_aux = 1;
    IrInstr *se = LwNovaInstr(c, IR_SE, e->pos_op);
    se->a = ResultadoTemp(e_erro);
    IrBloco *anterior = LwTrocarBloco(c, &se->corpo);
    AtribuirLocal(c, local, LwExpr(c, e->b), e->pos_op);
    LwTrocarBloco(c, &se->senao);
    IrInstr *valor = InstrComTemp(c, opcional ? IR_OPC_VALOR : IR_RES_VALOR, e, e->tipo);
    valor->a = resultado;
    AtribuirLocal(c, local, ResultadoTemp(valor), e->pos_op);
    LwTrocarBloco(c, anterior);
    return LwValorLocal(local, e->tipo);
}
