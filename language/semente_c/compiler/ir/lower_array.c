/* lower_array.c - lowering de [N]T: indice a[i], endereco de cadeia (a[i], x.campo), literal '{a, b}', zero e len de array.
 * Versao: 0.6.0
 * Changelog: 0.6.0 - IA1-L5a: s[i] e s[lo..<hi] de str (LwIndiceTexto, LwFatiaTexto, TextoComoFatia: o str vira []u8 por IR_TRANSMUTA).
 *            0.5.0 - Linguagem v1.11 L15a: LwIndice, EnderecoDeElemento, LwFatia, LwTamanhoColecao, LwBaseDaColecao e LwLiteralArray desviam [dynamic, N]T para lower_lista_fixa.c.
 *            0.4.0 - Linguagem v1.9 P8: LwIndice desvia a[i] de soa (LwIndiceSoa) e EnderecoDeCampo desvia &a[i].campo de soa (LwEnderecoCampoSoa).
 *                    Linguagem v1.9 P6: LwIndice e EnderecoDeElemento desviam m[i, j] (nargs != 0) para LwIndiceMatriz e LwEnderecoMatriz.
 *                    Linguagem v1.9 P4: LwLiteralArray e LwTamanhoColecao desviam bit_set (LwLiteralV19, LwTamanhoV19); envoltorios
 *                    LwIndiceInt, LwBaseIndexada e LwEnderecavel.
 *            0.3.0 - Linguagem v1.6 P7b: LwBaseDaColecao aceita lista (base lida uma vez + tamanho por IR_TAMANHO_TEXTO).
 *            0.2.0 - Linguagem v1.6 P7a: LwEhLista; len, a[..] de [dynamic]T e ^[dynamic]T; LwBaseDaColecao recusa lista (P7b).
 *            0.1.0 - Linguagem v1.5 P7a: LwEndereco, LwIndice, LwLiteralArray, LwZeroComposto, LwTamanhoColecao (array). Fatia e o laco
 *                    de colecao entram no P7b e no P8.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"
#include "semantic/sem_interno.h"

static TipoClasse ClasseDe(const LowerCtx *c, TipoId t)
{
    return TipoClasseDe(&c->sem->tipos, t);
}

/* Instrucao com destino em temporario novo; chamar depois de calcular os operandos. */
static IrInstr *InstrComTemp(LowerCtx *c, IrOp op, uint32_t pos, TipoId tipo)
{
    IrInstr *i = LwNovaInstr(c, op, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = tipo;
    return i;
}

static bool EhDeref(const Expr *e)
{
    return e->tipo_no == EX_UNARIO && e->op == OP_DEREF;
}

/* Indice de qualquer inteiro como int: constante muda so o tipo; o resto passa por IR_CONVERSAO (o emissor decide cast puro ou checado). */
static Valor IndiceComoInt(LowerCtx *c, const Expr *e)
{
    Valor v;
    if (!LwDobrar(c, e, &v)) v = LwExpr(c, e); /* 'a[POS + 1]': a semantica nao dobra conta, o lowering dobra */
    bool bits_altos = TipoEhSemSinal(v.tipo) && v.inteiro < 0; /* uint acima de INT64_MAX: so a conversao checada decide */
    if (v.tipo == TIPO_INT) return v;
    if (v.tipo_no == V_INT && !bits_altos) {
        v.tipo = TIPO_INT;
        return v;
    }
    IrInstr *i = InstrComTemp(c, IR_CONVERSAO, e->pos_op, TIPO_INT);
    i->a = v;
    return LwValorTemp(i->destino, TIPO_INT);
}

/* O lvalor da cadeia tem endereco: local/parametro, p^, campo e elemento (por valor so quando a raiz tem endereco). */
static bool EhEnderecavel(const LowerCtx *c, const Expr *e)
{
    switch (e->tipo_no) {
    case EX_NOME: return e->ref == REF_LOCAL || e->ref == REF_PARAM;
    case EX_UNARIO: return e->op == OP_DEREF;
    case EX_CAMPO: return e->ref == REF_CAMPO && (ClasseDe(c, e->a->tipo) == TC_PONTEIRO || EhEnderecavel(c, e->a));
    case EX_INDICE: return ClasseDe(c, e->a->tipo) != TC_ARRAY || EhEnderecavel(c, e->a);
    default: return false;
    }
}

/* Indice constante conferido na compilacao (E0421 so no que a semantica nao dobra); dentro dos limites = acesso provado. */
static void ConferirIndiceConstante(LowerCtx *c, Valor indice, TipoId base, uint32_t pos, uint8_t *op_aux)
{
    char tipo[96];
    TipoId array = ClasseDe(c, base) == TC_PONTEIRO ? TipoDe(&c->sem->tipos, base)->a : base;
    if (indice.tipo_no != V_INT || ClasseDe(c, array) != TC_ARRAY) return;
    uint32_t n = TipoDe(&c->sem->tipos, array)->b;
    if (indice.inteiro >= 0 && indice.inteiro < (int64_t)n) {
        *op_aux = 1;
        return;
    }
    if (c->sem->diag->tem) return;
    SemDescrever(c->sem, array, tipo, sizeof tipo);
    DiagRelatar(c->sem->diag, E0421, pos, "indice %lld fora de %s (0..<%u)", (long long)indice.inteiro, tipo, (unsigned)n);
}

/* Ponteiro do struct dono de 'base.campo': p^ -> p; ^Struct -> o proprio valor; senao o endereco da base. */
static Valor PonteiroDaBase(LowerCtx *c, const Expr *base)
{
    if (EhDeref(base)) return LwExpr(c, base->a);
    if (ClasseDe(c, base->tipo) == TC_PONTEIRO) return LwExpr(c, base);
    return LwEndereco(c, base);
}

static Valor EnderecoDeNome(LowerCtx *c, const Expr *e)
{
    IrInstr *i = InstrComTemp(c, IR_UNARIO, e->pos_op, TipoPonteiro(&c->sem->tipos, e->tipo));
    i->op_aux = OP_ENDERECO;
    i->a = LwValorLocal(e->ref_id, e->tipo);
    return LwValorTemp(i->destino, i->tipo);
}

static Valor EnderecoDeCampo(LowerCtx *c, const Expr *e)
{
    if (__builtin_expect(c->sem->tipos.tem_soa, 0) && e->a->tipo_no == EX_INDICE && ClasseDe(c, e->a->a->tipo) == TC_SOA)
        return LwEnderecoCampoSoa(c, e);
    Valor ponteiro = PonteiroDaBase(c, e->a);
    IrInstr *i = InstrComTemp(c, IR_ENDERECO_CAMPO, e->pos_op, TipoPonteiro(&c->sem->tipos, e->tipo));
    i->a = ponteiro;
    i->nome = e->nome;
    return LwValorTemp(i->destino, i->tipo);
}

/* a[i] como endereco: base []T ou ^[N]T vem pelo valor; [N]T pelo endereco da base. */
static Valor EnderecoDeElemento(LowerCtx *c, const Expr *e)
{
    if (__builtin_expect(e->nargs != 0, 0)) return LwEnderecoMatriz(c, e);
    if (__builtin_expect(c->sem->qtd_listas_fixas != 0, 0) && LwEhListaFixa(c, e->a->tipo)) return LwEnderecoListaFixa(c, e); /* v1.11 L15a */
    Valor base = ClasseDe(c, e->a->tipo) == TC_ARRAY ? LwEndereco(c, e->a) : LwExpr(c, e->a);
    Valor indice = IndiceComoInt(c, e->b);
    IrInstr *i = InstrComTemp(c, IR_ENDERECO_ELEMENTO, e->pos_op, TipoPonteiro(&c->sem->tipos, e->tipo));
    i->a = base;
    i->b = indice;
    ConferirIndiceConstante(c, indice, base.tipo, e->pos_op, &i->op_aux);
    return LwValorTemp(i->destino, i->tipo);
}

Valor LwEndereco(LowerCtx *c, const Expr *e)
{
    switch (e->tipo_no) {
    case EX_NOME: return EnderecoDeNome(c, e);
    case EX_UNARIO:
        if (EhDeref(e)) return LwExpr(c, e->a);
        break;
    case EX_CAMPO: return EnderecoDeCampo(c, e);
    case EX_INDICE: return EnderecoDeElemento(c, e);
    default: break;
    }
    FalhaInterna("lowering: endereco de algo que nao e lvalor (offset %u)", (unsigned)e->pos_op);
}

/* Base de a[i]: []T e ^[N]T pelo valor; [N]T local pelo nome (sem copia), cadeia lvalor pelo endereco, rvalue num temporario. */
static Valor BaseDoIndice(LowerCtx *c, const Expr *base)
{
    if (ClasseDe(c, base->tipo) != TC_ARRAY) return LwExpr(c, base);
    if (base->tipo_no == EX_NOME) return LwExpr(c, base);
    return EhEnderecavel(c, base) ? LwEndereco(c, base) : LwExpr(c, base);
}

/* Envoltorios publicos (v1.9) dos static acima, para lower_v19.c e os passos de matrix/soa/map. */
Valor LwIndiceInt(LowerCtx *c, const Expr *e) { return IndiceComoInt(c, e); }
Valor LwBaseIndexada(LowerCtx *c, const Expr *base) { return BaseDoIndice(c, base); }
bool LwEnderecavel(const LowerCtx *c, const Expr *e) { return EhEnderecavel(c, e); }

/* L-02: o str e {ptr, len} como a fatia []u8; o lowering o reinterpreta (IR_TRANSMUTA) e reaproveita IR_ELEMENTO e IR_FATIA, com a mesma checagem R0006 da fatia. */
static Valor TextoComoFatia(LowerCtx *c, const Expr *texto)
{
    Valor v = LwExpr(c, texto);
    IrInstr *i = InstrComTemp(c, IR_TRANSMUTA, texto->pos_op, TipoFatia(&c->sem->tipos, TIPO_U8));
    i->a = v;
    return LwValorTemp(i->destino, i->tipo);
}

/* s[i]: o byte (u8) do str, base antes do indice (D16). */
static Valor LwIndiceTexto(LowerCtx *c, const Expr *e)
{
    Valor base = TextoComoFatia(c, e->a);
    Valor indice = IndiceComoInt(c, e->b);
    IrInstr *i = InstrComTemp(c, IR_ELEMENTO, e->pos_op, TIPO_U8);
    i->a = base;
    i->b = indice;
    return LwValorTemp(i->destino, i->tipo);
}

/* s[lo..<hi], s[lo..=hi]: str emprestado do mesmo texto; s[..] e o proprio str. Ordem: base, inicio, fim. */
static Valor LwFatiaTexto(LowerCtx *c, const Expr *e)
{
    if (e->op == FATIA_INTEIRA) return LwExpr(c, e->a);
    Valor base = TextoComoFatia(c, e->a);
    Valor inicio = IndiceComoInt(c, e->args[0].valor);
    Valor fim = IndiceComoInt(c, e->args[1].valor);
    IrInstr *i = InstrComTemp(c, IR_FATIA, e->pos_op, TipoFatia(&c->sem->tipos, TIPO_U8));
    i->a = base;
    i->b = inicio;
    i->args = LwArgs(c, 1);
    i->args[0] = fim;
    i->nargs = 1;
    i->faixa_excl = e->op == FATIA_EXCL;
    Valor fatia = LwValorTemp(i->destino, i->tipo);
    IrInstr *volta = InstrComTemp(c, IR_TRANSMUTA, e->pos_op, TIPO_TEXTO);
    volta->a = fatia;
    return LwValorTemp(volta->destino, volta->tipo);
}

Valor LwIndice(LowerCtx *c, const Expr *e)
{
    if (__builtin_expect(e->nargs != 0, 0)) return LwIndiceMatriz(c, e);
    if (e->a->tipo == TIPO_TEXTO) return LwIndiceTexto(c, e);
    if (__builtin_expect(c->sem->tipos.tem_soa, 0) && ClasseDe(c, e->a->tipo) == TC_SOA) return LwIndiceSoa(c, e);
    if (__builtin_expect(c->sem->qtd_listas_fixas != 0, 0) && LwEhListaFixa(c, e->a->tipo)) return LwIndiceListaFixa(c, e); /* v1.11 L15a */
    Valor base = BaseDoIndice(c, e->a);
    Valor indice = IndiceComoInt(c, e->b);
    IrInstr *i = InstrComTemp(c, IR_ELEMENTO, e->pos_op, e->tipo);
    i->a = base;
    i->b = indice;
    ConferirIndiceConstante(c, indice, base.tipo, e->pos_op, &i->op_aux);
    return LwValorTemp(i->destino, i->tipo);
}

/* Elementos na ordem do fonte; o tipo do literal ja veio do contexto (e->tipo). */
Valor LwLiteralArray(LowerCtx *c, const Expr *e)
{
    if (__builtin_expect(c->sem->tipos.tem_bit_set | c->sem->tipos.tem_matriz, 0) && ClasseDe(c, e->tipo) >= TC_MAPA)
        return LwLiteralV19(c, e);
    if (__builtin_expect(c->sem->qtd_listas_fixas != 0, 0) && TipoEhListaFixa(&c->sem->tipos, e->tipo)) return LwZeroDoTipo(c, e->tipo, e->pos); /* v1.11 L15a: '{}' */
    Valor *elementos = LwArgs(c, e->nargs);
    for (uint32_t k = 0; k < e->nargs; k++)
        elementos[k] = LwExpr(c, e->args[k].valor);
    IrInstr *i = InstrComTemp(c, IR_ARRAY, e->pos_op, e->tipo);
    i->args = elementos;
    i->nargs = e->nargs;
    return LwValorTemp(i->destino, i->tipo);
}

Valor LwZeroComposto(LowerCtx *c, TipoId t, uint32_t pos)
{
    IrInstr *i = InstrComTemp(c, IR_ARRAY, pos, t);
    return LwValorTemp(i->destino, i->tipo);
}

/* O [N]T de um [N]T ou ^[N]T; qualquer outro tipo (fatia) devolve ele mesmo. */
static TipoId ArrayDoTipo(const LowerCtx *c, TipoId t)
{
    return ClasseDe(c, t) == TC_PONTEIRO ? TipoDe(&c->sem->tipos, t)->a : t;
}

bool LwEhLista(const LowerCtx *c, TipoId t)
{
    if (ClasseDe(c, t) == TC_PONTEIRO) t = TipoDe(&c->sem->tipos, t)->a;
    return ClasseDe(c, t) == TC_DINAMICO;
}

/* len de array e ^[N]T: N, constante de compilacao (o argumento nao e avaliado); len de fatia e de lista le o campo len. */
Valor LwTamanhoColecao(LowerCtx *c, const Expr *arg)
{
    if (__builtin_expect(c->sem->qtd_listas_fixas != 0, 0) && LwEhListaFixa(c, arg->tipo)) return LwTamanhoListaFixa(c, arg); /* v1.11 L15a */
    TipoId array = ArrayDoTipo(c, arg->tipo);
    if (LwEhLista(c, arg->tipo)) {
        Valor lista = LwExpr(c, arg);
        IrInstr *i = InstrComTemp(c, IR_TAMANHO_TEXTO, arg->pos_op, TIPO_INT);
        i->a = lista;
        return LwValorTemp(i->destino, TIPO_INT);
    }
    if (ClasseDe(c, array) == TC_FATIA) {
        Valor fatia = LwExpr(c, arg);
        IrInstr *i = InstrComTemp(c, IR_TAMANHO_TEXTO, arg->pos_op, TIPO_INT);
        i->a = fatia;
        return LwValorTemp(i->destino, TIPO_INT);
    }
    if (ClasseDe(c, array) != TC_ARRAY) {
        if (ClasseDe(c, array) >= TC_MAPA) return LwTamanhoV19(c, arg);
        FalhaInterna("lowering: len de colecao que nao e array nem fatia (offset %u)", (unsigned)arg->pos_op);
    }
    return LwValorInt((int64_t)TipoDe(&c->sem->tipos, array)->b, TIPO_INT);
}

/* Base do 'for' de colecao e o tamanho fixado no inicio: []T pelo valor + .len; ^[N]T pelo valor + N; [N]T pelo endereco quando
 * tem (o laco le o array vivo) ou o valor do rvalue (avaliado uma vez). */
Valor LwBaseDaColecao(LowerCtx *c, const Expr *e, Valor *tamanho)
{
    if (__builtin_expect(c->sem->qtd_listas_fixas != 0, 0) && LwEhListaFixa(c, e->tipo)) return LwBaseListaFixa(c, e, tamanho); /* v1.11 L15a */
    TipoClasse classe = ClasseDe(c, e->tipo);
    if (LwEhLista(c, e->tipo)) {
        Valor lista = classe == TC_DINAMICO && EhEnderecavel(c, e) ? LwEndereco(c, e) : LwExpr(c, e);
        IrInstr *t = InstrComTemp(c, IR_TAMANHO_TEXTO, e->pos_op, TIPO_INT);
        t->a = lista;
        *tamanho = LwValorTemp(t->destino, TIPO_INT);
        return lista;
    }
    if (classe == TC_FATIA) {
        Valor base = LwExpr(c, e);
        IrInstr *i = InstrComTemp(c, IR_TAMANHO_TEXTO, e->pos_op, TIPO_INT);
        i->a = base;
        *tamanho = LwValorTemp(i->destino, TIPO_INT);
        return base;
    }
    *tamanho = LwValorInt((int64_t)TipoDe(&c->sem->tipos, ArrayDoTipo(c, e->tipo))->b, TIPO_INT);
    if (classe == TC_ARRAY && EhEnderecavel(c, e)) return LwEndereco(c, e);
    return LwExpr(c, e);
}

/* Limites constantes da fatia de array: fora de 0..=N (ou inicio > fim) e E0421; tudo dentro = limites provados (op_aux = 1). */
static void ConferirLimitesConstantes(LowerCtx *c, const Expr *e, Valor inicio, Valor fim, uint8_t *op_aux)
{
    char tipo[96];
    TipoId array = ArrayDoTipo(c, e->a->tipo);
    if (inicio.tipo_no != V_INT || fim.tipo_no != V_INT || ClasseDe(c, array) != TC_ARRAY) return;
    int64_t n = (int64_t)TipoDe(&c->sem->tipos, array)->b;
    int64_t ultimo = e->op == FATIA_INCL ? fim.inteiro + 1 : fim.inteiro; /* exclusivo: fim.inteiro <= n */
    if (inicio.inteiro >= 0 && inicio.inteiro <= ultimo && ultimo <= n) {
        *op_aux = 1;
        return;
    }
    if (c->sem->diag->tem) return;
    SemDescrever(c->sem, array, tipo, sizeof tipo);
    DiagRelatar(c->sem->diag, E0421, e->pos_op, "fatia %lld%s%lld fora de %s (0..=%lld)", (long long)inicio.inteiro,
                e->op == FATIA_INCL ? "..=" : "..<", (long long)fim.inteiro, tipo, (long long)n);
}

/* a[..]: fatia inteira. []T devolve o proprio valor; [N]T e ^[N]T viram {ptr, N} (limites provados). */
static Valor FatiaInteira(LowerCtx *c, const Expr *e, Valor base)
{
    if (ClasseDe(c, e->a->tipo) == TC_FATIA) return base;
    bool lista = LwEhLista(c, e->a->tipo);
    Valor tamanho;
    if (lista) {
        IrInstr *t = InstrComTemp(c, IR_TAMANHO_TEXTO, e->pos_op, TIPO_INT);
        t->a = base;
        tamanho = LwValorTemp(t->destino, TIPO_INT);
    } else {
        tamanho = LwValorInt((int64_t)TipoDe(&c->sem->tipos, ArrayDoTipo(c, e->a->tipo))->b, TIPO_INT);
    }
    IrInstr *i = InstrComTemp(c, IR_FATIA, e->pos_op, e->tipo);
    i->a = base;
    i->b = LwValorInt(0, TIPO_INT);
    i->args = LwArgs(c, 1);
    i->args[0] = tamanho;
    i->nargs = 1;
    i->faixa_excl = true;
    i->op_aux = lista ? 0 : 1; /* lista: o tamanho vem de execucao; o emissor confere a epoca */
    return LwValorTemp(i->destino, i->tipo);
}

/* a[lo..<hi] e a[lo..=hi]: base [N]T pelo endereco; ^[N]T e []T pelo valor. Ordem: base, inicio, fim (D16). */
Valor LwFatia(LowerCtx *c, const Expr *e)
{
    if (e->a->tipo == TIPO_TEXTO) return LwFatiaTexto(c, e);
    if (__builtin_expect(c->sem->qtd_listas_fixas != 0, 0) && LwEhListaFixa(c, e->a->tipo)) return LwFatiaListaFixa(c, e); /* v1.11 L15a */
    Valor base = ClasseDe(c, e->a->tipo) == TC_ARRAY ? LwEndereco(c, e->a) : LwExpr(c, e->a);
    if (e->op == FATIA_INTEIRA) return FatiaInteira(c, e, base);
    Valor inicio = IndiceComoInt(c, e->args[0].valor);
    Valor fim = IndiceComoInt(c, e->args[1].valor);
    IrInstr *i = InstrComTemp(c, IR_FATIA, e->pos_op, e->tipo);
    i->a = base;
    i->b = inicio;
    i->args = LwArgs(c, 1);
    i->args[0] = fim;
    i->nargs = 1;
    i->faixa_excl = e->op == FATIA_EXCL;
    ConferirLimitesConstantes(c, e, inicio, fim, &i->op_aux);
    return LwValorTemp(i->destino, i->tipo);
}
