/* lower_quat.c - lowering das contas do quat (v1.11 L14a/L14b, A05): q + r, q - r, -q, q * k, k * q (k: f32), conj(q), q * r, q * [3]f32 e abs(q).
 * Sem IrOp nova: cada componente (x, y, z, w, nessa ordem) e um IR_CAMPO do operando quat + um IR_BINARIO/IR_UNARIO f32 (a regra do escalar f32, sem FMA); o
 * IR_STRUCT junta os quatro. L14b copia o raymath.h da raylib 6.2 (interop_lab\raylib\vendor\include\raymath.h) termo a termo e com a mesma associacao:
 * q * r = QuaternionMultiply (linhas 2230-2243), q * v = Vector3RotateByQuaternion (linhas 906-915), abs(q) = QuaternionLength (linhas 2185-2190,
 * sqrtf(x*x + y*y + z*z + w*w) pelo OP_RAIZ do L13). O 2 e o -2 do C viram f32 (2*q.x*q.y = (2*q.x)*q.y); o oraculo tests\v111\quat_oraculo.c confere 200 casos.
 * Interface (lower_interno.h): LwBinarioQuat, LwNegarQuat, LwConjQuat, LwProdutoQuat, LwGirarQuat e LwAbsQuat (chamados por lower_expr.c e lower_call.c).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.11 L14b: LwProdutoQuat (q * r), LwGirarQuat (q * [3]f32) e LwAbsQuat (abs(q)); LwBinarioQuat desvia para as duas primeiras.
 *            0.1.0 - Linguagem v1.11 L14a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

enum { QUAT_CAMPOS = 4, QUAT_W = 3 };

/* O campo k (x, y, z, w) do quat q: IR_CAMPO f32 sobre o valor. */
static Valor Componente(LowerCtx *c, Valor q, uint32_t k, uint32_t pos)
{
    IrInstr *i = LwNovaInstr(c, IR_CAMPO, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_F32;
    i->nome = c->sem->decl_quat->campos[k].nome;
    i->a = q;
    return LwValorTemp(i->destino, TIPO_F32);
}

/* O operando da conta no componente k: o campo k do quat; o escalar f32 vale igual para os quatro. */
static Valor Operando(LowerCtx *c, Valor v, uint32_t k, uint32_t pos)
{
    return v.tipo == c->sem->tipo_quat ? Componente(c, v, k, pos) : v;
}

static Valor Binario(LowerCtx *c, uint8_t operador, Valor a, Valor b, uint32_t pos)
{
    IrInstr *i = LwNovaInstr(c, IR_BINARIO, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_F32;
    i->op_aux = operador;
    i->a = a;
    i->b = b;
    return LwValorTemp(i->destino, TIPO_F32);
}

static Valor Negar(LowerCtx *c, Valor v, uint32_t pos)
{
    IrInstr *i = LwNovaInstr(c, IR_UNARIO, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_F32;
    i->op_aux = OP_NEGAR;
    i->a = v;
    return LwValorTemp(i->destino, TIPO_F32);
}

/* IR_STRUCT do quat com os quatro componentes ja calculados. */
static Valor Montar(LowerCtx *c, Valor *componentes, uint32_t pos)
{
    IrInstr *i = LwNovaInstr(c, IR_STRUCT, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = c->sem->tipo_quat;
    i->args = componentes;
    i->nargs = QUAT_CAMPOS;
    return LwValorTemp(i->destino, i->tipo);
}

/* ---------- L14b: q * r, q * [3]f32 e abs(q), copiados do raymath.h da raylib 6.2 (termo a termo, mesma associacao, sem FMA) ---------- */

/* A conta em curso: o contexto e a posicao das instrucoes. */
typedef struct { LowerCtx *c; uint32_t pos; } Conta;

enum { IX, IY, IZ, IW, LINHAS_ROTACAO = 3, VETOR_N = 3 };

static Valor Mul(const Conta *k, Valor a, Valor b)
{
    return Binario(k->c, OP_MUL, a, b, k->pos);
}

/* acumulado + termo (soma) ou acumulado - termo: o fold da esquerda para a direita, como o '+'/'-' do C. */
static Valor Combinar(const Conta *k, bool soma, Valor acumulado, Valor termo)
{
    return Binario(k->c, soma ? OP_SOMA : OP_SUB, acumulado, termo, k->pos);
}

/* Os quatro campos de q, lidos uma vez (a ordem x y z w). */
static void Extrair(const Conta *k, Valor q, Valor *campos)
{
    for (uint32_t i = 0; i < QUAT_CAMPOS; i++) campos[i] = Componente(k->c, q, i, k->pos);
}

/* Um termo qa[a]*qb[b] da multiplicacao de quaternions; o sinal vale do 2o termo em diante. */
typedef struct { uint8_t a, b; bool soma; } TermoHamilton;

/* QuaternionMultiply (raymath.h:2237-2240): x = qax*qbw + qaw*qbx + qay*qbz - qaz*qby, e assim por diante. */
static const TermoHamilton HAMILTON[QUAT_CAMPOS][QUAT_CAMPOS] = {
    {{IX, IW, true}, {IW, IX, true}, {IY, IZ, true}, {IZ, IY, false}},
    {{IY, IW, true}, {IW, IY, true}, {IZ, IX, true}, {IX, IZ, false}},
    {{IZ, IW, true}, {IW, IZ, true}, {IX, IY, true}, {IY, IX, false}},
    {{IW, IW, true}, {IX, IX, false}, {IY, IY, false}, {IZ, IZ, false}},
};

static Valor ComponenteHamilton(const Conta *k, const Valor *qa, const Valor *qb, uint32_t alvo)
{
    Valor acumulado = Mul(k, qa[HAMILTON[alvo][0].a], qb[HAMILTON[alvo][0].b]);
    for (uint32_t t = 1; t < QUAT_CAMPOS; t++) {
        const TermoHamilton *h = &HAMILTON[alvo][t];
        acumulado = Combinar(k, h->soma, acumulado, Mul(k, qa[h->a], qb[h->b]));
    }
    return acumulado;
}

Valor LwProdutoQuat(LowerCtx *c, const Expr *e)
{
    Conta k = {c, e->pos_op};
    Valor a = LwExpr(c, e->a);
    Valor b = LwExpr(c, e->b);
    Valor qa[QUAT_CAMPOS], qb[QUAT_CAMPOS];
    Extrair(&k, a, qa);
    Extrair(&k, b, qb);
    Valor *componentes = LwArgs(c, QUAT_CAMPOS);
    for (uint32_t i = 0; i < QUAT_CAMPOS; i++) componentes[i] = ComponenteHamilton(&k, qa, qb, i);
    return Montar(c, componentes, e->pos_op);
}

/* Soma de quadrados q[i]*q[i] de uma linha da tabela: a diagonal da rotacao (3 linhas) ou a norma (4a linha, tudo '+'). */
typedef struct { uint8_t i; bool soma; } Quadrado;

/* Vector3RotateByQuaternion (raymath.h:910-912): x*x + w*w - y*y - z*z; w*w - x*x + y*y - z*z; w*w - x*x - y*y + z*z. A linha 3 e a de QuaternionLength (raymath.h:2187). */
static const Quadrado DIAGONAL[LINHAS_ROTACAO + 1][QUAT_CAMPOS] = {
    {{IX, true}, {IW, true}, {IY, false}, {IZ, false}},
    {{IW, true}, {IX, false}, {IY, true}, {IZ, false}},
    {{IW, true}, {IX, false}, {IY, false}, {IZ, true}},
    {{IX, true}, {IY, true}, {IZ, true}, {IW, true}},
};

static Valor SomaDeQuadrados(const Conta *k, const Valor *q, uint32_t linha)
{
    Valor acumulado = Mul(k, q[DIAGONAL[linha][0].i], q[DIAGONAL[linha][0].i]);
    for (uint32_t t = 1; t < QUAT_CAMPOS; t++) {
        const Quadrado *s = &DIAGONAL[linha][t];
        acumulado = Combinar(k, s->soma, acumulado, Mul(k, q[s->i], q[s->i]));
    }
    return acumulado;
}

/* Fora da diagonal: c1*a1*b1 +/- 2*a2*b2, com c1 = 2 ou -2 (o '-2*q.w*q.x' do C e (-2*w)*x). */
typedef struct { float c1; uint8_t a1, b1, a2, b2; bool soma; } Cruzado;

static const Cruzado CRUZADO[LINHAS_ROTACAO][VETOR_N] = {
    {{0, 0, 0, 0, 0, true}, {2.0f, IX, IY, IW, IZ, false}, {2.0f, IX, IZ, IW, IY, true}},
    {{2.0f, IW, IZ, IX, IY, true}, {0, 0, 0, 0, 0, true}, {-2.0f, IW, IX, IY, IZ, true}},
    {{-2.0f, IW, IY, IX, IZ, true}, {2.0f, IW, IX, IY, IZ, true}, {0, 0, 0, 0, 0, true}},
};

/* (coeficiente * a) * b: a associacao do C para 2*q.x*q.y. */
static Valor Escalado(const Conta *k, float coeficiente, Valor a, Valor b)
{
    return Mul(k, Mul(k, LwValorReal(coeficiente, TIPO_F32), a), b);
}

static Valor TermoCruzado(const Conta *k, const Valor *q, const Cruzado *x)
{
    Valor primeiro = Escalado(k, x->c1, q[x->a1], q[x->b1]);
    return Combinar(k, x->soma, primeiro, Escalado(k, 2.0f, q[x->a2], q[x->b2]));
}

/* O elemento (linha, coluna) da matriz de rotacao que o raymath escreve entre parenteses. */
static Valor ElementoRotacao(const Conta *k, const Valor *q, uint32_t linha, uint32_t coluna)
{
    return linha == coluna ? SomaDeQuadrados(k, q, linha) : TermoCruzado(k, q, &CRUZADO[linha][coluna]);
}

/* v.x*E0 + v.y*E1 + v.z*E2 da linha dada. */
static Valor LinhaRotacao(const Conta *k, const Valor *q, const Valor *v, uint32_t linha)
{
    Valor acumulado = Mul(k, v[0], ElementoRotacao(k, q, linha, 0));
    for (uint32_t coluna = 1; coluna < VETOR_N; coluna++) {
        Valor termo = Mul(k, v[coluna], ElementoRotacao(k, q, linha, coluna));
        acumulado = Combinar(k, true, acumulado, termo);
    }
    return acumulado;
}

Valor LwGirarQuat(LowerCtx *c, const Expr *e)
{
    Conta k = {c, e->pos_op};
    Valor quat = LwExpr(c, e->a);
    Valor vetor = LwExpr(c, e->b);
    Valor q[QUAT_CAMPOS], v[VETOR_N];
    Extrair(&k, quat, q);
    for (uint32_t i = 0; i < VETOR_N; i++) v[i] = LwElementoDoOperando(c, vetor, LwValorInt((int64_t)i, TIPO_INT), e->pos_op);
    Valor *elementos = LwArgs(c, VETOR_N);
    for (uint32_t linha = 0; linha < VETOR_N; linha++) elementos[linha] = LinhaRotacao(&k, q, v, linha);
    IrInstr *i = LwNovaInstr(c, IR_ARRAY, e->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = e->tipo;
    i->args = elementos;
    i->nargs = VETOR_N;
    return LwValorTemp(i->destino, i->tipo);
}

Valor LwAbsQuat(LowerCtx *c, const Expr *e)
{
    Conta k = {c, e->pos_op};
    Valor q[QUAT_CAMPOS];
    Extrair(&k, LwExpr(c, e->args[0].valor), q);
    Valor soma = SomaDeQuadrados(&k, q, LINHAS_ROTACAO); /* antes da raiz: as instrucoes entram no bloco na ordem de criacao */
    return LwRaiz(c, e->pos_op, TIPO_F32, soma);
}

Valor LwBinarioQuat(LowerCtx *c, const Expr *e)
{
    const TipoId quat = c->sem->tipo_quat;
    if (e->a->tipo == quat && e->b->tipo == quat && e->op == OP_MUL) return LwProdutoQuat(c, e); /* L14b: q * r */
    if (e->tipo != quat) return LwGirarQuat(c, e);                                               /* L14b: q * v */
    Valor a = LwExpr(c, e->a);
    Valor b = LwExpr(c, e->b);
    Valor *componentes = LwArgs(c, QUAT_CAMPOS);
    for (uint32_t k = 0; k < QUAT_CAMPOS; k++) {
        Valor ak = Operando(c, a, k, e->pos_op);
        Valor bk = Operando(c, b, k, e->pos_op);
        componentes[k] = Binario(c, e->op, ak, bk, e->pos_op);
    }
    return Montar(c, componentes, e->pos_op);
}

Valor LwNegarQuat(LowerCtx *c, const Expr *e)
{
    Valor q = LwExpr(c, e->a);
    Valor *componentes = LwArgs(c, QUAT_CAMPOS);
    for (uint32_t k = 0; k < QUAT_CAMPOS; k++) componentes[k] = Negar(c, Componente(c, q, k, e->pos_op), e->pos_op);
    return Montar(c, componentes, e->pos_op);
}

Valor LwConjQuat(LowerCtx *c, const Expr *e)
{
    Valor q = LwExpr(c, e->args[0].valor);
    Valor *componentes = LwArgs(c, QUAT_CAMPOS);
    for (uint32_t k = 0; k < QUAT_CAMPOS; k++) {
        Valor campo = Componente(c, q, k, e->pos_op);
        componentes[k] = k == QUAT_W ? campo : Negar(c, campo, e->pos_op);
    }
    return Montar(c, componentes, e->pos_op);
}
