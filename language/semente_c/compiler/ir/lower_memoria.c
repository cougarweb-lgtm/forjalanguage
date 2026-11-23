/* lower_memoria.c - lowering da memoria explicita: new, make, append, delete e frame_reset.
 * Versao: 0.7.0
 * Changelog: 0.7.0 - M6e P93b: LwDelete de nome (^T, ?^T, cstr) grava o zero no nome depois do IR_LIBERA (D-M6e-2 + E1).
 *            0.6.6 - K17: TipoDeOrdenacao deixa de ser static (lower_lista.c usa no heap); mesmo texto.
 *            0.6.5 - K08b: LwBinarySearch liga usa_ordenar_chave (o C inclui forja_rt_ordenar_chave.h; antes so o sort_by_key ligava).
 *            0.6.4 - K08b: LwSortByKey (sort_by_key: IR_ORDENA_GERAL modo CHAVE, deslocamento do layout) e LwBinarySearch (IR_BUSCA_BINARIA x2 sobre um local + IR_TUPLA).
 *            0.6.3 - K08a: LwSortBy liga usa_ordenar_estavel (runtime proprio), nao usa_ordenar.
 *            0.6.2 - K08a: LwSortBy (sort_by: IR_ORDENA_GERAL com o proc de less em b).
 *            0.6.1 - B01b: LwAlocador trata ALOC_PARAM como Arena/Pool (o handle .id do parametro alloc).
 *            0.6.0 - IA1-L5b: append de fatia (IR_ANEXA_FATIA), make com len e resize (IR_REDIMENSIONA, LwResize) e sort (IR_ORDENA, LwSort).
 *            0.5.1 - LwNew: new([N]T) sem valor inicial com N >= 1024 vira IR_ALOCA op_aux IR_ALOCA_ZERADO, sem temporario nem IR_GRAVA_PONTEIRO (o temporario de 5,3 MiB estourava a pilha do x64).
 *            0.5.0 - Linguagem v1.11 L15a: LwAppend de ^[dynamic, N]T vai para LwAppendListaFixa (lower_lista_fixa.c).
 *            0.4.0 - Linguagem v1.9 P11: LwMakeV19 de map (IR_MAPA_CRIA) e LwDelete de map (IR_LIBERA op_aux 4).
 *                    Linguagem v1.9 P8: LwMake desvia soa (LwMakeV19 = IR_SOA_CRIA), LwAppend de soa dinamica (IR_SOA_ANEXA) e LwDelete de soa (IR_LIBERA op_aux 5).
 *            0.3.0 - Linguagem v1.8 P6b: LwAlocador por e->alocacao (Arena/Pool = handle .id); delete de Pool = IR_LIBERA op_aux 3.
 *            0.2.1 - Linguagem v1.8 P6a: LwAlocador e LwDelete recusam Arena/Pool (FalhaInterna provisoria ate o P6b).
 *            0.2.0 - Linguagem v1.7 P8: Alocador vira LwAlocador (publico); delete de str = IR_LIBERA op_aux 2.
 *            0.1.0- Linguagem v1.6 P7a: LwNew, LwMake, LwAppend, LwDelete, LwFrameReset e o despacho LwMemoria.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

#define ALOCADOR_HEAP 1
#define ALOCADOR_FRAME 2

/* O argumento ligado ao parametro 'indice' (a ordem no vetor e a do fonte); NULL se o fonte nao o escreveu. */
static const Expr *ArgumentoDe(const Expr *e, int32_t indice)
{
    for (uint32_t k = 0; k < e->nargs; k++) {
        if (e->args[k].indice == indice) return e->args[k].valor;
    }
    return NULL;
}

/* e->alocacao (gravada pela semantica): heap e escopo viram 1, frame 2; Arena/Pool leem o handle .id que a semantica
 * deixou no 'em'. Qualquer outro valor nao chega aqui. */
Valor LwAlocador(LowerCtx *c, const Expr *e)
{
    switch (e->alocacao) {
    case ALOC_HEAP:
    case ALOC_ESCOPO: return LwValorInt(ALOCADOR_HEAP, TIPO_U32);
    case ALOC_FRAME: return LwValorInt(ALOCADOR_FRAME, TIPO_U32);
    case ALOC_ARENA:
    case ALOC_PARAM: /* B01b: o 'em' ja virou o .id do parametro 'alloc: Allocator' (sem_memoria.c) */
    case ALOC_POOL: return LwExpr(c, ArgumentoDe(e, 1));
    default: FalhaInterna("lowering: alocacao %u sem 'em' (offset %u)", (unsigned)e->alocacao, (unsigned)e->pos_op);
    }
}

/* new([N]T) sem valor inicial com N a partir daqui zera o bloco no proprio IR_ALOCA: o zero do array e todo bytes 0 (IR_ARRAY sem args),
 * e montar sizeof([N]T) bytes num temporario poe o bloco na pilha do x64 (reserva de 2 MiB do PE; [1400000]f32 = 5,3 MiB estourava). 1024 elementos
 * deixa de fora os arrays pequenos, que seguem pelo temporario com o mesmo IR e o mesmo C de antes. */
#define NEW_ZERO_NO_DESTINO_MIN 1024u

static bool NewZeraNoDestino(const LowerCtx *c, TipoId apontado)
{
    const TipoInfo *tipo = TipoDe(&c->sem->tipos, apontado);

    return tipo->classe == TC_ARRAY && tipo->b >= NEW_ZERO_NO_DESTINO_MIN;
}

/* Ordem: valor inicial, aloca, grava o valor no bloco (ou, no array grande sem valor, o proprio aloca zera o bloco). */
static Valor LwNew(LowerCtx *c, const Expr *e)
{
    TipoId apontado = TipoDe(&c->sem->tipos, e->tipo)->a;
    bool zera_no_destino = e->b == NULL && NewZeraNoDestino(c, apontado);
    Valor inicial = {0};
    if (e->b != NULL)
        inicial = LwExpr(c, e->b);
    else if (!zera_no_destino)
        inicial = LwZeroDoTipo(c, apontado, e->pos_op);
    Valor alocador = LwAlocador(c, e); /* antes da instrucao: o handle de Arena/Pool emite o proprio .id */
    IrInstr *aloca = LwNovaInstr(c, IR_ALOCA, e->pos_op);
    aloca->destino = LwNovoTemp(c);
    aloca->tipo = e->tipo;
    aloca->a = alocador;
    Valor ponteiro = LwValorTemp(aloca->destino, aloca->tipo);
    if (zera_no_destino) {
        aloca->op_aux = IR_ALOCA_ZERADO;
        return ponteiro;
    }
    IrInstr *grava = LwNovaInstr(c, IR_GRAVA_PONTEIRO, e->pos_op);
    grava->a = ponteiro;
    grava->b = inicial;
    grava->nome = -1;
    grava->tipo = apontado;
    return ponteiro;
}

/* make de tipo novo da v1.9: soa[dynamic]S = IR_SOA_CRIA, map = IR_MAPA_CRIA. Capacidade antes do alocador, como o LwMake de hoje. */
static Valor LwMakeV19(LowerCtx *c, const Expr *e, TipoClasse classe)
{
    if (classe != TC_SOA && classe != TC_MAPA) FalhaInterna("lowering: make de tipo %u (offset %u)", (unsigned)e->tipo, (unsigned)e->pos_op);
    const Expr *cap = ArgumentoDe(e, 2);
    Valor capacidade = cap != NULL ? LwExpr(c, cap) : LwValorInt(0, TIPO_INT);
    Valor alocador = LwAlocador(c, e);
    IrInstr *i = LwNovaInstr(c, classe == TC_MAPA ? IR_MAPA_CRIA : IR_SOA_CRIA, e->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = e->tipo;
    i->a = alocador;
    i->b = capacidade;
    return LwValorTemp(i->destino, i->tipo);
}

/* L5b: resize e make(len =) e a lista `lista` (variavel, temp ou ^lista) passa a ter `tamanho` elementos, os novos zerados. */
static void LwRedimensiona(LowerCtx *c, const Expr *e, Valor lista, Valor tamanho)
{
    TipoId dinamica = TipoClasseDe(&c->sem->tipos, lista.tipo) == TC_PONTEIRO ? (TipoId)TipoDe(&c->sem->tipos, lista.tipo)->a : lista.tipo;
    IrInstr *i = LwNovaInstr(c, IR_REDIMENSIONA, e->pos_op);
    i->tipo = TipoDe(&c->sem->tipos, dinamica)->a;
    i->a = lista;
    i->b = tamanho;
}

/* make([dynamic]T, alloc = A, cap = N, len = M): sem cap a capacidade e M; com len a lista sai com M zeros (IR_REDIMENSIONA depois do IR_LISTA_CRIA). */
static Valor LwMake(LowerCtx *c, const Expr *e)
{
    TipoClasse classe = TipoClasseDe(&c->sem->tipos, e->tipo);
    if (__builtin_expect(classe != TC_DINAMICO, 0)) return LwMakeV19(c, e, classe);
    const Expr *cap = ArgumentoDe(e, MAKE_PARAM_CAP), *len = ArgumentoDe(e, MAKE_PARAM_LEN);
    Valor capacidade = cap != NULL ? LwExpr(c, cap) : LwValorInt(0, TIPO_INT);
    Valor tamanho = len != NULL ? LwExpr(c, len) : LwValorInt(0, TIPO_INT);
    if (cap == NULL && len != NULL) capacidade = tamanho;
    Valor alocador = LwAlocador(c, e); /* antes da instrucao: o handle de Arena/Pool emite o proprio .id */
    IrInstr *i = LwNovaInstr(c, IR_LISTA_CRIA, e->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = e->tipo;
    i->a = alocador;
    i->b = capacidade;
    Valor lista = LwValorTemp(i->destino, i->tipo);
    if (len != NULL) LwRedimensiona(c, e, lista, tamanho);
    return lista;
}

static Valor LwAppend(LowerCtx *c, const Expr *e)
{
    const Expr *alvo = ArgumentoDe(e, 0);
    TipoId dinamica = TipoDe(&c->sem->tipos, alvo->tipo)->a; /* ^[dynamic]T -> [dynamic]T */
    if (__builtin_expect(TipoEhListaFixa(&c->sem->tipos, dinamica), 0)) return LwAppendListaFixa(c, e, alvo, ArgumentoDe(e, 1)); /* v1.11 L15a: ^[dynamic, N]T */
    if (__builtin_expect(TipoClasseDe(&c->sem->tipos, dinamica) == TC_SOA, 0)) {
        Valor soa = LwExpr(c, alvo);
        Valor item = LwExpr(c, ArgumentoDe(e, 1));
        IrInstr *anexa = LwNovaInstr(c, IR_SOA_ANEXA, e->pos_op);
        anexa->tipo = TipoDe(&c->sem->tipos, TipoDe(&c->sem->tipos, dinamica)->a)->a; /* soa[dynamic]S -> [dynamic]S -> S */
        anexa->a = soa;
        anexa->b = item;
        return LwValorNenhum();
    }
    const Expr *fatia = ArgumentoDe(e, APPEND_PARAM_ITEMS);
    Valor lista = LwExpr(c, alvo);
    Valor item = LwExpr(c, fatia != NULL ? fatia : ArgumentoDe(e, APPEND_PARAM_ITEM));
    IrInstr *i = LwNovaInstr(c, fatia != NULL ? IR_ANEXA_FATIA : IR_ANEXA, e->pos_op); /* L5b: items = fatia copia os elementos de uma vez */
    i->tipo = TipoDe(&c->sem->tipos, dinamica)->a;
    i->a = lista;
    i->b = item;
    return LwValorNenhum();
}

/* resize(&l, len = n). */
Valor LwResize(LowerCtx *c, const Expr *e)
{
    Valor lista = LwExpr(c, ArgumentoDe(e, 0));
    Valor tamanho = LwExpr(c, ArgumentoDe(e, 1));
    LwRedimensiona(c, e, lista, tamanho);
    return LwValorNenhum();
}

/* O ORDENA_* do elemento da fatia (so inteiro, real e str passam pela semantica). K17: nao e mais static (lower_lista.c usa no heap). */
uint8_t TipoDeOrdenacao(const LowerCtx *c, TipoId elemento)
{
    switch (TipoClasseDe(&c->sem->tipos, elemento)) {
    case TC_I8: return ORDENA_I8;
    case TC_I16: return ORDENA_I16;
    case TC_I32: return ORDENA_I32;
    case TC_INT: return ORDENA_I64;
    case TC_U8: return ORDENA_U8;
    case TC_U16: return ORDENA_U16;
    case TC_U32: return ORDENA_U32;
    case TC_UINT: return ORDENA_U64;
    case TC_REAL: return ORDENA_F32;
    case TC_F64: return ORDENA_F64;
    case TC_TEXTO: return ORDENA_STR;
    default: FalhaInterna("lowering: sort de elemento do tipo %u", (unsigned)elemento);
    }
}

/* sort(fatia). */
Valor LwSort(LowerCtx *c, const Expr *e)
{
    Valor fatia = LwExpr(c, ArgumentoDe(e, 0));
    IrInstr *i = LwNovaInstr(c, IR_ORDENA, e->pos_op);
    i->tipo = fatia.tipo;
    i->a = fatia;
    i->op_aux = TipoDeOrdenacao(c, (TipoId)TipoDe(&c->sem->tipos, fatia.tipo)->a);
    c->prog->usa_ordenar = true;
    return LwValorNenhum();
}

/* sort_by(s, less = p) (K08a): IR_ORDENA_GERAL, a = fatia, b = o proc p, tipo = T (o elemento). Fatia e proc saem antes da instrucao (LwExpr pode emitir). */
Valor LwSortBy(LowerCtx *c, const Expr *e)
{
    Valor fatia = LwExpr(c, ArgumentoDe(e, 0));
    Valor comparador = LwExpr(c, ArgumentoDe(e, 1));
    IrInstr *i = LwNovaInstr(c, IR_ORDENA_GERAL, e->pos_op);
    i->tipo = (TipoId)TipoDe(&c->sem->tipos, fatia.tipo)->a;
    i->a = fatia;
    i->b = comparador;
    i->op_aux = ORDENA_GERAL_ESTAVEL;
    c->prog->usa_ordenar_estavel = true;
    return LwValorNenhum();
}

/* sort_by_key(s, key = Struct.campo) (K08b): IR_ORDENA_GERAL em modo CHAVE, sem proc. O deslocamento sai do layout (como offset_of) e o ORDENA_* do tipo do campo. */
Valor LwSortByKey(LowerCtx *c, const Expr *e)
{
    const Expr *chave = ArgumentoDe(e, 1);
    Valor fatia = LwExpr(c, ArgumentoDe(e, 0));
    IrInstr *i = LwNovaInstr(c, IR_ORDENA_GERAL, e->pos_op);
    i->tipo = (TipoId)TipoDe(&c->sem->tipos, fatia.tipo)->a;
    i->a = fatia;
    i->op_aux = ORDENA_GERAL_CHAVE;
    i->desloc_chave = (int64_t)LayoutOffsetCampo(&c->layouts, chave->a->tipo, (uint32_t)chave->ref_id);
    i->tipo_chave = TipoDeOrdenacao(c, chave->tipo);
    c->prog->usa_ordenar_chave = true;
    return LwValorNenhum();
}

/* Um IR_BUSCA_BINARIA: par[0] = a fatia, par[1] = o valor (local de T). O destino e int (POSICAO) ou bool (ACHOU); tipo_chave = ORDENA_* de T. */
static Valor LwBuscaModo(LowerCtx *c, const Expr *e, uint8_t modo, const Valor *par)
{
    IrInstr *i = LwNovaInstr(c, IR_BUSCA_BINARIA, e->pos_op);
    TipoId elemento = (TipoId)TipoDe(&c->sem->tipos, par[0].tipo)->a;
    i->destino = LwNovoTemp(c);
    i->tipo = modo == BUSCA_ACHOU ? TIPO_BOOL : TIPO_INT;
    i->a = par[0];
    i->b = par[1];
    i->op_aux = modo;
    i->tipo_chave = TipoDeOrdenacao(c, elemento);
    return LwValorTemp(i->destino, i->tipo);
}

/* binary_search(s, value = v) -> (int, bool) (K08b): o valor vai para um local (o runtime le por endereco); a tupla junta as duas buscas. */
Valor LwBinarySearch(LowerCtx *c, const Expr *e)
{
    Valor par[2];
    par[0] = LwExpr(c, ArgumentoDe(e, 0));
    Valor valor = LwExpr(c, ArgumentoDe(e, 1));
    TipoId elemento = (TipoId)TipoDe(&c->sem->tipos, par[0].tipo)->a;
    int32_t local = LwNovoLocal(c);
    IrInstr *guarda = LwNovaInstr(c, IR_LOCAL, e->pos_op);
    guarda->destino = local;
    guarda->tipo = elemento;
    guarda->a = valor;
    par[1] = LwValorLocal(local, elemento);
    c->prog->usa_ordenar_chave = true; /* o C emitido precisa de forja_rt_ordenar_chave.h (forja_ordc_posicao/achou) */
    Valor posicao = LwBuscaModo(c, e, BUSCA_POSICAO, par);
    Valor achou = LwBuscaModo(c, e, BUSCA_ACHOU, par);
    IrInstr *tupla = LwNovaInstr(c, IR_TUPLA, e->pos_op);
    tupla->destino = LwNovoTemp(c);
    tupla->tipo = e->tipo;
    tupla->args = LwArgs(c, 2);
    tupla->args[0] = posicao;
    tupla->args[1] = achou;
    tupla->nargs = 2;
    return LwValorTemp(tupla->destino, e->tipo);
}

/* op_aux do IR_LIBERA de uma colecao dona: lista 1, map 4, soa dinamica 5. */
static uint8_t OpDaColecao(TipoClasse classe)
{
    return classe == TC_MAPA ? 4 : (classe == TC_SOA ? 5 : 1);
}

/* M6e P93b (D-M6e-2, E1): delete(p) grava o zero no NOME p (^T, ?^T e cstr; lista, map e soa ficam como estao).
 * So nome de local/parametro; campo e indice nao zeram. Uma escrita, dev e release, nos 2 backends (vem pelo IR). */
static void LwZeraNomeDoDelete(LowerCtx *c, const Expr *alvo, TipoClasse classe, uint32_t pos)
{
    if (alvo->tipo_no != EX_NOME || (alvo->ref != REF_LOCAL && alvo->ref != REF_PARAM)) return;
    if (classe != TC_PONTEIRO && classe != TC_ANULAVEL && alvo->tipo != TIPO_CSTR) return;
    IrInstr *i = LwNovaInstr(c, IR_ATRIBUICAO, pos);
    i->destino = alvo->ref_id;
    i->tipo = alvo->tipo;
    i->a = LwValorNulo(alvo->tipo);
    i->nome = -1;
}

/* delete(lista) libera o buffer pelo endereco da lista; delete(p) libera o bloco de new. */
static Valor LwDelete(LowerCtx *c, const Expr *e)
{
    const Expr *alvo = ArgumentoDe(e, 0);
    if (e->alocacao == ALOC_POOL) {
        Valor bloco = LwExpr(c, alvo);
        Valor handle = LwExpr(c, ArgumentoDe(e, 1)); /* os dois antes da instrucao: o .id emite a propria */
        IrInstr *devolve = LwNovaInstr(c, IR_LIBERA, e->pos_op);
        devolve->op_aux = 3; /* devolve o bloco ao Pool */
        devolve->a = bloco;
        devolve->b = handle;
        return LwValorNenhum();
    }
    TipoClasse classe = TipoClasseDe(&c->sem->tipos, alvo->tipo);
    bool lista = classe == TC_DINAMICO || classe == TC_MAPA || classe == TC_SOA;
    Valor endereco = lista ? LwEndereco(c, alvo) : LwExpr(c, alvo);
    IrInstr *i = LwNovaInstr(c, IR_LIBERA, e->pos_op);
    i->op_aux = lista ? OpDaColecao(classe) : (alvo->tipo == TIPO_TEXTO ? 2 : 0); /* 2 = str (libera a.ptr) */
    i->a = endereco;
    LwZeraNomeDoDelete(c, alvo, classe, e->pos_op);
    return LwValorNenhum();
}

Valor LwMemoria(LowerCtx *c, const Expr *e)
{
    switch (e->ref_id) {
    case EMB_NEW: return LwNew(c, e);
    case EMB_MAKE: return LwMake(c, e);
    case EMB_APPEND: return LwAppend(c, e);
    case EMB_DELETE: return LwDelete(c, e);
    case EMB_FRAME_RESET:
        LwNovaInstr(c, IR_FRAME_RESET, e->pos_op);
        return LwValorNenhum();
    default: FalhaInterna("lowering: embutido de memoria desconhecido (offset %u)", (unsigned)e->pos_op);
    }
}
