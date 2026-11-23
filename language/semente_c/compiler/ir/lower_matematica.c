/* lower_matematica.c - lowering de min, max, abs, clamp e sqrt (v1.11 L13, A13). Sem IrOp nova:
 *   min(a, b)  = (b < a) ? b : a        max(a, b) = (a < b) ? b : a        (com NaN devolvem a: a comparacao e falsa)
 *   clamp(x, lo, hi): t = (x < lo) ? lo : x; (hi < t) ? hi : t
 *   abs inteiro = x < 0 ? 0 - x : x pelo SUB checado (abs(MIN) = R0002 "subtracao" em dev, MIN em release)
 *   abs real = transmute para o inteiro sem sinal de mesma largura, AND com a mascara sem o bit de sinal, transmute de volta (-0.0 vira 0.0, NaN fica NaN)
 *   sqrt = IR_UNARIO com OP_RAIZ (a unica operacao nova de backend da fatia)
 * Cada "? :" e um local mutavel + IR_SE (o mesmo molde de e/ou e or_else). Em [N]T (min, max e clamp) a conta sai por elemento, em LwPorElemento (lower_vetor.c).
 * Interface (lower_interno.h): LwMatematica.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.11 L13: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

enum { MATEMATICA_OPERANDOS_MAX = 3 };
#define MASCARA_SEM_SINAL_32 0x7FFFFFFFLL
#define MASCARA_SEM_SINAL_64 0x7FFFFFFFFFFFFFFFLL

/* O escalar em curso: tipo do elemento (ou do resultado, fora de [N]T) e a posicao das instrucoes. */
typedef struct { LowerCtx *c; TipoId tipo; uint32_t pos; } Escalar;

typedef Valor (*Operacao)(const Escalar *s, const Valor *op);

typedef struct { Escalar escalar; Operacao operacao; uint32_t noperandos; Valor op[MATEMATICA_OPERANDOS_MAX]; } Matematica;

/* ---------- pecas ---------- */

static Valor NovaInstrComTemp(const Escalar *s, IrOp op, TipoId tipo, Valor a, Valor b)
{
    IrInstr *i = LwNovaInstr(s->c, op, s->pos);
    i->destino = LwNovoTemp(s->c);
    i->tipo = tipo;
    i->a = a;
    i->b = b;
    return LwValorTemp(i->destino, tipo);
}

static Valor Binario(const Escalar *s, uint8_t operador, TipoId tipo, Valor a, Valor b)
{
    IrInstr *i = LwNovaInstr(s->c, IR_BINARIO, s->pos);
    i->destino = LwNovoTemp(s->c);
    i->tipo = tipo;
    i->op_aux = operador;
    i->a = a;
    i->b = b;
    return LwValorTemp(i->destino, tipo);
}

/* Local mutavel do tipo do escalar, iniciado com 'inicial'. */
static int32_t NovoLocalCom(const Escalar *s, Valor inicial)
{
    IrInstr *def = LwNovaInstr(s->c, IR_LOCAL, s->pos);
    def->destino = LwNovoLocal(s->c);
    def->tipo = s->tipo;
    def->mutavel = true;
    def->a = inicial;
    return def->destino;
}

static void Atribuir(const Escalar *s, int32_t local, Valor valor)
{
    IrInstr *i = LwNovaInstr(s->c, IR_ATRIBUICAO, s->pos);
    i->destino = local;
    i->tipo = s->tipo;
    i->a = valor;
}

/* Abre "se cond { ... }": devolve o bloco de fora; as instrucoes seguintes entram no corpo ate LwTrocarBloco(c, externo). */
static IrBloco *EntrarSe(const Escalar *s, Valor cond)
{
    IrInstr *se = LwNovaInstr(s->c, IR_SE, s->pos);
    se->a = cond;
    return LwTrocarBloco(s->c, &se->corpo);
}

/* se cond { local = novo } */
static void SeAtribui(const Escalar *s, Valor cond, int32_t local, Valor novo)
{
    IrBloco *externo = EntrarSe(s, cond);
    Atribuir(s, local, novo);
    LwTrocarBloco(s->c, externo);
}

/* ---------- as operacoes ---------- */

static Valor OperacaoMin(const Escalar *s, const Valor *op)
{
    int32_t resultado = NovoLocalCom(s, op[0]);
    SeAtribui(s, Binario(s, OP_MENOR, TIPO_BOOL, op[1], op[0]), resultado, op[1]);
    return LwValorLocal(resultado, s->tipo);
}

static Valor OperacaoMax(const Escalar *s, const Valor *op)
{
    int32_t resultado = NovoLocalCom(s, op[0]);
    SeAtribui(s, Binario(s, OP_MENOR, TIPO_BOOL, op[0], op[1]), resultado, op[1]);
    return LwValorLocal(resultado, s->tipo);
}

static Valor OperacaoClamp(const Escalar *s, const Valor *op)
{
    int32_t resultado = NovoLocalCom(s, op[0]);
    Valor atual = LwValorLocal(resultado, s->tipo);
    SeAtribui(s, Binario(s, OP_MENOR, TIPO_BOOL, atual, op[1]), resultado, op[1]);
    SeAtribui(s, Binario(s, OP_MENOR, TIPO_BOOL, op[2], atual), resultado, op[2]);
    return atual;
}

/* x < 0 ? 0 - x : x ; o 0 - x e o SUB checado do escalar. */
static Valor AbsInteiro(const Escalar *s, Valor x)
{
    int32_t resultado = NovoLocalCom(s, x);
    Valor zero = LwValorInt(0, s->tipo);
    IrBloco *externo = EntrarSe(s, Binario(s, OP_MENOR, TIPO_BOOL, x, zero));
    Atribuir(s, resultado, Binario(s, OP_SUB, s->tipo, zero, x));
    LwTrocarBloco(s->c, externo);
    return LwValorLocal(resultado, s->tipo);
}

/* Bits sem o bit de sinal: o inteiro sem sinal da mesma largura do real. */
static Valor AbsReal(const Escalar *s, Valor x)
{
    bool dupla = s->tipo == TIPO_F64;
    TipoId bits = dupla ? TIPO_UINT : TIPO_U32; /* u64 = uint */
    Valor cru = NovaInstrComTemp(s, IR_TRANSMUTA, bits, x, LwValorNenhum());
    Valor mascara = LwValorInt(dupla ? MASCARA_SEM_SINAL_64 : MASCARA_SEM_SINAL_32, bits);
    Valor limpo = Binario(s, OP_BIT_E, bits, cru, mascara);
    return NovaInstrComTemp(s, IR_TRANSMUTA, s->tipo, limpo, LwValorNenhum());
}

static Valor OperacaoAbs(const Escalar *s, const Valor *op)
{
    return TipoEhInteiro(s->tipo) ? AbsInteiro(s, op[0]) : AbsReal(s, op[0]);
}

/* sqrt de 'a' (f32 ou f64 = tipo): o IR_UNARIO OP_RAIZ, unico ponto que o monta (sqrt do fonte e abs(quat)). */
Valor LwRaiz(LowerCtx *c, uint32_t pos, TipoId tipo, Valor a)
{
    IrInstr *i = LwNovaInstr(c, IR_UNARIO, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = tipo;
    i->op_aux = OP_RAIZ;
    i->a = a;
    return LwValorTemp(i->destino, tipo);
}

static Valor OperacaoRaiz(const Escalar *s, const Valor *op)
{
    return LwRaiz(s->c, s->pos, s->tipo, op[0]);
}

/* Por EmbutidoId - EMB_MIN. */
static const Operacao OPERACOES[] = {
    [EMB_MIN - EMB_MIN] = OperacaoMin, [EMB_MAX - EMB_MIN] = OperacaoMax, [EMB_ABS - EMB_MIN] = OperacaoAbs,
    [EMB_CLAMP - EMB_MIN] = OperacaoClamp, [EMB_SQRT - EMB_MIN] = OperacaoRaiz,
};

/* ---------- [N]T ---------- */

/* O elemento 'indice' do resultado: a operacao sobre os elementos 'indice' de cada operando [N]T. */
static Valor ElementoDaMatematica(LowerCtx *c, const void *dados, Valor indice)
{
    const Matematica *m = dados;
    Valor elementos[MATEMATICA_OPERANDOS_MAX];
    for (uint32_t k = 0; k < m->noperandos; k++) elementos[k] = LwElementoDoOperando(c, m->op[k], indice, m->escalar.pos);
    return m->operacao(&m->escalar, elementos);
}

Valor LwMatematica(LowerCtx *c, const Expr *e)
{
    bool vetor = TipoClasseDe(&c->sem->tipos, e->tipo) == TC_ARRAY;
    Matematica m = {.operacao = OPERACOES[e->ref_id - EMB_MIN], .noperandos = e->nargs};
    m.escalar = (Escalar){c, vetor ? (TipoId)TipoDe(&c->sem->tipos, e->tipo)->a : e->tipo, e->pos_op};
    for (uint32_t i = 0; i < e->nargs; i++) m.op[e->args[i].indice] = LwExpr(c, e->args[i].valor);
    if (!vetor) return m.operacao(&m.escalar, m.op);
    PorElemento por = {.tipo = e->tipo, .pos = e->pos_op, .elemento = ElementoDaMatematica, .dados = &m};
    return LwPorElemento(c, &por);
}
