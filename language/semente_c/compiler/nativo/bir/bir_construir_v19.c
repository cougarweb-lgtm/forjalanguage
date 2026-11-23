/* bir_construir_v19.c - IR -> BIR: bit_set (IR_CONJUNTO) e matrix escalar (IR_MATRIZ, IR_MATRIZ_ELEMENTO, IR_MATRIZ_END_ELEMENTO) da Linguagem v1.9.
 * Versao: 0.1.4
 * Changelog: 0.1.4 - P2: matrix[i, j] com literal dentro do limite fica imediato no release (BirIndiceNoLimite): sem vreg nem conferencia; dev identico.
 *            0.1.3 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.1.2 - O0 P13: linha e coluna da matrix conferidas tambem no release com --checagem sim (BirConfereLimites).
 *            0.1.1 - M3 F9b-3: decisoes extraidas de Transposta, Literal e Somar.
 *            0.1.0 - M2 P16: primeira versao. bit_set = inteiro sem sinal de 8/16/32/64 bits (BirClasseDoConjunto); as sete formas de IR_CONJUNTO seguem as expressoes do emit_c_v19.c:
 *                    diferenca = a AND (NOT b) na largura do bit_set; tem/insere/remove/adiciona montam o bit 1 << b em 64 bits (b imediato vira constante; b variavel passa pela guarda
 *                    b < 64 que o validador exige do SHL) e conferem/alteram o valor alargado ou truncado; conta CHAMA forja_rt_conta_bits(u64); membro e o proprio indice como enum (I32).
 *                    matrix R x C do tipo f32/f64 e agregado de R*C elementos guardados por COLUNAS (elemento (i, j) em [j*R + i]); cada forma de IR_MATRIZ vira laco desenrolado de
 *                    CARREGA/ADD/SUB/MUL/NEG/GRAVA escalares no slot do resultado. Produto e matriz x vetor somam em ordem de k crescente a partir de 0 (0 + x, nao x: o -0.0 muda), sem FMA.
 *                    matrix[i, j] confere a linha e DEPOIS a coluna em dev (BirConferirIndice -> forja_falha_indice, R0006); release nao confere. Mais de 4096 termos desenrolados
 *                    (R*C*K do produto, R*K do vetor, R*C do resto) recusa com X0001 (a troca por laco fica para quando houver medida).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/v19a
 */
#include "nativo/bir/bir_interno.h"

#define BITS_I64 64
#define MATRIZ_TERMOS_MAX 4096u /* teto do desenrolamento (contrato P16); acima disso X0001 */
#define MARCO_V19 "M2"
#define OP_CONJUNTO_PRIMEIRO 2
#define OP_CONJUNTO_ULTIMO 8
#define OP_MATRIZ_PRODUTO 2     /* matriz x matriz */
#define OP_MATRIZ_ESCALAR_ESQ 4 /* escalar x matriz */

/* ---- tipo bit_set ---- */

bool BirClasseDoConjunto(const Tipos *tipos, TipoId tipo, uint8_t *classe)
{
    uint32_t membros;

    if (TipoClasseDe(tipos, tipo) != TC_BIT_SET)
        return false;
    membros = (uint32_t)TipoDe(tipos, tipo)->b;
    *classe = membros <= 8 ? BC_I8 : (membros <= 16 ? BC_I16 : (membros <= 32 ? BC_I32 : BC_I64));
    return true;
}

static BirOperando EmVreg(BirCtx *c, BirOperando o)
{
    BirOperandoVreg(c, o, &o);
    return o;
}

/* O operando alargado a 64 bits (o (uint64_t)(a) do C com EXT_Z; o indice de enum com EXT_S), sempre num vreg. */
static BirOperando Alargado64(BirCtx *c, BirOperando o, BirOp extensao)
{
    o = EmVreg(c, o);
    if (o.classe == BC_I64)
        return o;
    return OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = extensao, .a = o}), BC_I64);
}

/* O bit do membro b variavel: 1 << b com a guarda b < 64 (membro valido nunca cai no ramo zero; o C e indefinido ali). */
static BirOperando BitVariavel(BirCtx *c, BirOperando n)
{
    uint32_t fim = BirNovoBloco(c, false), zero = BirNovoBloco(c, false), calcula = BirNovoBloco(c, false);
    uint32_t bit = BirNovoVreg(c, BC_I64);
    uint32_t fora = BirComparar(c, BCOND_GE_U, n, OpImed(BITS_I64, BC_I64));

    BirDesvia(c, OpVreg(fora, BC_I8), zero, calcula);
    c->cur = zero;
    BirMover(c, bit, OpImed(0, BC_I64));
    BirSalta(c, fim);
    c->cur = calcula;
    BirEmitirPedido(c, (BirPedido){.op = BIR_SHL, .aux = BIR_SEM_SINAL, .dst = bit, .a = OpImed(1, BC_I64), .b = n});
    BirSalta(c, fim);
    c->cur = fim;
    return OpVreg(bit, BC_I64);
}

/* vreg I64 com UINT64_C(1) << b. */
static BirOperando BitDoMembro(BirCtx *c, BirOperando b)
{
    if (b.tipo == BO_IMED)
        return EmVreg(c, OpImed((uint64_t)b.imed < BITS_I64 ? (int64_t)(UINT64_C(1) << b.imed) : 0, BC_I64));
    return BitVariavel(c, Alargado64(c, b, BIR_EXT_S));
}

/* O bit (I64) na largura do bit_set: o cast (Tp)(...) do C. */
static BirOperando BitNaLargura(BirCtx *c, BirOperando bit, uint8_t classe)
{
    if (classe == BC_I64)
        return bit;
    return OpVreg(BirContaNova(c, classe, (BirPedido){.op = BIR_TRUNC, .a = bit}), classe);
}

/* ---- IR_CONJUNTO ---- */

static bool OperandosDoConjunto(BirCtx *c, const IrInstr *i, BirOperando ab[2])
{
    return BirOperandoDe(c, i, &i->a, &ab[0]) && BirOperandoDe(c, i, &i->b, &ab[1]);
}

/* op 2: tD = (T)((a) & ~(b)). */
static bool ConjuntoDiferenca(BirCtx *c, const IrInstr *i)
{
    BirOperando ab[2];
    uint32_t dst, nao_b;

    if (!OperandosDoConjunto(c, i, ab) || !BirDefinirDestino(c, ab[0].classe, &dst))
        return false;
    ab[0] = EmVreg(c, ab[0]);
    ab[1] = EmVreg(c, ab[1]);
    nao_b = BirContaNova(c, ab[1].classe, (BirPedido){.op = BIR_NOT, .a = ab[1]});
    BirEmitirPedido(c, (BirPedido){.op = BIR_AND, .dst = dst, .a = ab[0], .b = OpVreg(nao_b, ab[1].classe)});
    return true;
}

/* op 3: tD = ((((uint64_t)a) >> b) & 1u) != 0, escrito como (a64 AND (1 << b)) != 0. */
static bool ConjuntoTem(BirCtx *c, const IrInstr *i)
{
    BirOperando ab[2], valor, bit;
    uint32_t dst, comum;

    if (!OperandosDoConjunto(c, i, ab) || !BirDefinirDestino(c, BC_I8, &dst))
        return false;
    valor = Alargado64(c, ab[0], BIR_EXT_Z);
    bit = BitDoMembro(c, ab[1]);
    comum = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_AND, .a = valor, .b = bit});
    BirEmitirPedido(c, (BirPedido){.op = BIR_CMP, .aux = BCOND_NE, .dst = dst, .a = OpVreg(comum, BC_I64), .b = OpImed(0, BC_I64)});
    return true;
}

/* op 4 e 5: *(a) = (T)(*(a) | (T)(1 << b)) ou (*(a) & (T)~(1 << b)); a = ^bit_set. */
static bool ConjuntoAltera(BirCtx *c, const IrInstr *i)
{
    BirOperando ab[2], bit;
    uint8_t classe = BC_NADA;
    uint32_t atual, novo;

    if (!BirClasseDoTipo(c->e->tipos, TipoDe(c->e->tipos, i->a.tipo)->a, &classe))
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!OperandosDoConjunto(c, i, ab))
        return false;
    ab[0] = EmVreg(c, ab[0]);
    atual = BirNovoVreg(c, classe);
    BirEmitirCarrega(c, atual, ab[0].id, 0);
    bit = BitNaLargura(c, BitDoMembro(c, ab[1]), classe);
    if (i->op_aux == 5)
        bit = OpVreg(BirContaNova(c, classe, (BirPedido){.op = BIR_NOT, .a = bit}), classe);
    novo = BirContaNova(c, classe, (BirPedido){.op = i->op_aux == 5 ? BIR_AND : BIR_OR, .a = OpVreg(atual, classe), .b = bit});
    BirEmitirGrava(c, ab[0].id, 0, OpVreg(novo, classe));
    return true;
}

/* op 6: tD = (int64_t)popcount((uint64_t)a), pelo servico do runtime (sem libgcc). */
static bool ConjuntoConta(BirCtx *c, const IrInstr *i)
{
    BirOperando a;
    BirOperando *args;
    uint32_t dst, conta;

    if (!BirOperandoDe(c, i, &i->a, &a) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    args = BirArgs(c, 1);
    args[0] = Alargado64(c, a, BIR_EXT_Z);
    conta = BirChamarRuntime(c, &(BirChamadaRt){"forja_rt_conta_bits", BC_I64, 1}, args);
    BirMover(c, dst, OpVreg(conta, BC_I64));
    return true;
}

/* op 7: tD = (T)((a) | (T)(1 << b)). */
static bool ConjuntoAdiciona(BirCtx *c, const IrInstr *i)
{
    BirOperando ab[2], bit;
    uint8_t classe = BC_NADA;
    uint32_t dst;

    if (!OperandosDoConjunto(c, i, ab) || !BirClasseDoTipo(c->e->tipos, i->tipo, &classe) || !BirDefinirDestino(c, classe, &dst))
        return false;
    ab[0] = EmVreg(c, ab[0]);
    bit = BitNaLargura(c, BitDoMembro(c, ab[1]), classe);
    BirEmitirPedido(c, (BirPedido){.op = BIR_OR, .dst = dst, .a = ab[0], .b = bit});
    return true;
}

/* op 8: tD = (Enum)(b): o membro de indice b (o enum sem valor explicito vale o proprio indice). */
static bool ConjuntoMembro(BirCtx *c, const IrInstr *i)
{
    BirOperando b;
    uint32_t dst;

    if (!BirOperandoDe(c, i, &i->b, &b) || !BirDefinirDestino(c, BC_I32, &dst))
        return false;
    if (b.tipo == BO_IMED)
        BirMover(c, dst, OpImed((int32_t)b.imed, BC_I32));
    else if (b.classe == BC_I64)
        BirEmitirPedido(c, (BirPedido){.op = BIR_TRUNC, .dst = dst, .a = b});
    else
        BirMover(c, dst, b);
    return true;
}

typedef bool (*BirRegraFn)(BirCtx *c, const IrInstr *i);

/* Indexada por op_aux - 2. */
static const BirRegraFn REGRAS_CONJUNTO[] = {
    ConjuntoDiferenca, ConjuntoTem, ConjuntoAltera, ConjuntoAltera, ConjuntoConta, ConjuntoAdiciona, ConjuntoMembro,
};

bool BirRegraConjunto(BirCtx *c, const IrInstr *i)
{
    if (i->op_aux < OP_CONJUNTO_PRIMEIRO || i->op_aux > OP_CONJUNTO_ULTIMO)
        return BirFalhaInconsistente(c, "IR_CONJUNTO com op_aux fora de 2..8");
    return REGRAS_CONJUNTO[i->op_aux - OP_CONJUNTO_PRIMEIRO](c, i);
}

/* ---- matrix: forma e leitura ---- */

typedef struct { uint32_t linhas, colunas, tam; uint8_t classe; TipoId elemento; } MatrizForma;

/* Dimensoes, elemento (classe real e tamanho) de uma matrix ou ^matrix. */
static bool FormaDaMatriz(BirCtx *c, TipoId tipo, MatrizForma *f)
{
    const Tipos *tipos = c->e->tipos;
    BirSlot slot;

    if (TipoClasseDe(tipos, tipo) == TC_PONTEIRO)
        tipo = TipoDe(tipos, tipo)->a;
    if (TipoClasseDe(tipos, tipo) != TC_MATRIZ)
        return BirFalhaInconsistente(c, "operacao de matrix com operando que nao e matrix");
    f->linhas = TipoMatrizLinhas(tipos, tipo);
    f->colunas = TipoMatrizColunas(tipos, tipo);
    f->elemento = TipoDe(tipos, tipo)->a;
    if (!BirClasseDoTipo(tipos, f->elemento, &f->classe) || !BirClasseEhReal(f->classe))
        return BirFalhaInconsistente(c, "matrix com elemento que nao e f32 nem f64");
    if (!BirForma(c, f->elemento, &slot))
        return false;
    f->tam = slot.tam;
    return true;
}

static BirOperando LerElemento(BirCtx *c, uint32_t base, uint32_t desloc, uint8_t classe)
{
    uint32_t v = BirNovoVreg(c, classe);

    BirEmitirCarrega(c, v, base, (int32_t)desloc);
    return OpVreg(v, classe);
}

/* Estado de uma IR_MATRIZ em construcao: enderecos (vregs I64) dos operandos e do resultado. */
typedef struct {
    BirCtx *c;
    const IrInstr *i;
    MatrizForma f;      /* a matrix que a forma percorre (o resultado, ou o operando a em transposta e matriz x vetor) */
    uint32_t a, b, dst; /* a, b: enderecos dos operandos; dst: o slot do resultado */
    uint32_t k;         /* termos de cada soma do produto */
    BirOperando escalar;
} Mat;

static bool LimiteDeTermos(const Mat *m, uint32_t termos)
{
    if (termos <= MATRIZ_TERMOS_MAX)
        return true;
    return BirFalhaX0001(m->c, m->i->pos, (const char *const[3]){"IR_MATRIZ", "desenrolada grande", NULL}, MARCO_V19);
}

/* ---- matrix: soma, subtracao, escalar x matriz, matriz x escalar e negacao (op_aux 0, 1, 3, 4, 6) ---- */

typedef enum { FONTE_A, FONTE_B, FONTE_ESCALAR, FONTE_NENHUMA } OrigemOperando;
typedef struct { uint8_t op; OrigemOperando esq, dir; } FormaElementar;

static const FormaElementar ELEMENTAR[] = {
    [0] = {BIR_ADD, FONTE_A, FONTE_B},
    [1] = {BIR_SUB, FONTE_A, FONTE_B},
    [3] = {BIR_MUL, FONTE_A, FONTE_ESCALAR},
    [4] = {BIR_MUL, FONTE_ESCALAR, FONTE_B},
    [6] = {BIR_NEG, FONTE_A, FONTE_NENHUMA},
};

/* Escalar das formas 3 (b) e 4 (a): avaliado uma vez, num vreg. */
static bool EscalarDe(Mat *m)
{
    const Valor *v = m->i->op_aux == OP_MATRIZ_ESCALAR_ESQ ? &m->i->a : &m->i->b;
    BirOperando o;

    if (m->i->op_aux != 3 && m->i->op_aux != OP_MATRIZ_ESCALAR_ESQ)
        return true;
    if (!BirOperandoDe(m->c, m->i, v, &o))
        return false;
    m->escalar = EmVreg(m->c, o);
    return true;
}

static bool EnderecosElementar(Mat *m)
{
    const IrInstr *i = m->i;
    bool usa_a = i->op_aux != OP_MATRIZ_ESCALAR_ESQ;
    bool usa_b = i->op_aux <= 1 || i->op_aux == OP_MATRIZ_ESCALAR_ESQ;

    if (usa_a && !BirEnderecoDe(m->c, i, &i->a, &m->a))
        return false;
    if (usa_b && !BirEnderecoDe(m->c, i, &i->b, &m->b))
        return false;
    return BirSlotDoDestino(m->c, i, &m->dst);
}

static BirOperando FonteElemento(Mat *m, OrigemOperando fonte, uint32_t desloc)
{
    if (fonte == FONTE_ESCALAR)
        return m->escalar;
    return LerElemento(m->c, fonte == FONTE_A ? m->a : m->b, desloc, m->f.classe);
}

/* tD.m[q] = a.m[q] op b.m[q] (ou o escalar no lado da forma); a ordem dos operandos segue a da expressao (a antes de b). */
static void ElementoElementar(Mat *m, uint32_t q)
{
    const FormaElementar *forma = &ELEMENTAR[m->i->op_aux];
    uint32_t desloc = q * m->f.tam, v = BirNovoVreg(m->c, m->f.classe);
    BirOperando esq = FonteElemento(m, forma->esq, desloc);

    if (forma->op == BIR_NEG)
        BirNegarRealEm(m->c, esq, v);
    else
        BirEmitirPedido(m->c, (BirPedido){.op = forma->op, .dst = v, .a = esq, .b = FonteElemento(m, forma->dir, desloc)});
    BirEmitirGrava(m->c, m->dst, (int32_t)desloc, OpVreg(v, m->f.classe));
}

static bool Elementar(Mat *m)
{
    if (!FormaDaMatriz(m->c, m->i->tipo, &m->f) || !LimiteDeTermos(m, m->f.linhas * m->f.colunas))
        return false;
    if (!EscalarDe(m) || !EnderecosElementar(m))
        return false;
    for (uint32_t q = 0; q < m->f.linhas * m->f.colunas; q++)
        ElementoElementar(m, q);
    return true;
}

/* ---- matrix: produto e matriz x vetor (op_aux 2 e 5) ---- */

/* s = 0; para k crescente: s = s + A[ini_a + k * passo_a] * B[ini_b + k * tam]. */
typedef struct { uint32_t base_a, base_b, ini_a, ini_b, passo_a, termos, tam; uint8_t classe; } Soma;

static uint32_t SomaDeProdutos(BirCtx *c, const Soma *s)
{
    uint32_t acc = BirNovoVreg(c, s->classe);

    BirMover(c, acc, OpImed(0, s->classe));
    for (uint32_t k = 0; k < s->termos; k++) {
        BirOperando x = LerElemento(c, s->base_a, s->ini_a + k * s->passo_a, s->classe);
        BirOperando y = LerElemento(c, s->base_b, s->ini_b + k * s->tam, s->classe);
        uint32_t produto = BirContaNova(c, s->classe, (BirPedido){.op = BIR_MUL, .a = x, .b = y});

        BirEmitirPedido(c, (BirPedido){.op = BIR_ADD, .dst = acc, .a = OpVreg(acc, s->classe), .b = OpVreg(produto, s->classe)});
    }
    return acc;
}

/* O elemento (lin, col) do resultado: a linha lin de a contra a coluna col de b (o vetor e a coluna 0). */
static void GravarSoma(Mat *m, uint32_t lin, uint32_t col)
{
    const MatrizForma *f = &m->f;
    Soma s = {.base_a = m->a, .base_b = m->b, .ini_a = lin * f->tam, .ini_b = col * m->k * f->tam, .passo_a = f->linhas * f->tam,
              .termos = m->k, .tam = f->tam, .classe = f->classe};
    uint32_t soma = SomaDeProdutos(m->c, &s);

    BirEmitirGrava(m->c, m->dst, (int32_t)((col * f->linhas + lin) * f->tam), OpVreg(soma, f->classe));
}

/* m->f = a forma de forma_de (o resultado no produto, o operando a no vetor); desenrolados: R*C*K no produto e R*K no vetor. */
static bool PrepararSoma(Mat *m, TipoId forma_de, uint32_t termos)
{
    const IrInstr *i = m->i;
    uint32_t colunas;

    if (!FormaDaMatriz(m->c, forma_de, &m->f))
        return false;
    m->k = termos;
    colunas = i->op_aux == OP_MATRIZ_PRODUTO ? m->f.colunas : 1;
    if (!LimiteDeTermos(m, m->f.linhas * colunas * termos))
        return false;
    if (!BirEnderecoDe(m->c, i, &i->a, &m->a) || !BirEnderecoDe(m->c, i, &i->b, &m->b))
        return false;
    return BirSlotDoDestino(m->c, i, &m->dst);
}

/* a [R, K] x b [K, C]: o resultado [R, C] vem do tipo da instrucao, K das colunas de a. */
static bool Produto(Mat *m)
{
    MatrizForma a;

    if (!FormaDaMatriz(m->c, m->i->a.tipo, &a) || !PrepararSoma(m, m->i->tipo, a.colunas))
        return false;
    for (uint32_t col = 0; col < m->f.colunas; col++) {
        for (uint32_t lin = 0; lin < m->f.linhas; lin++)
            GravarSoma(m, lin, col);
    }
    return true;
}

/* a [R, C] x b [C]T -> [R]T. */
static bool Vetor(Mat *m)
{
    MatrizForma a;

    if (!FormaDaMatriz(m->c, m->i->a.tipo, &a) || !PrepararSoma(m, m->i->a.tipo, a.colunas))
        return false;
    for (uint32_t lin = 0; lin < m->f.linhas; lin++)
        GravarSoma(m, lin, 0);
    return true;
}

/* ---- matrix: transposta e literal (op_aux 7 e 8) ---- */

/* Copia cada elemento (lin, col) de a para (col, lin) do resultado, em colunas de a. */
static void CopiarTransposta(Mat *m)
{
    for (uint32_t col = 0; col < m->f.colunas; col++) {
        for (uint32_t lin = 0; lin < m->f.linhas; lin++) {
            BirOperando v = LerElemento(m->c, m->a, (col * m->f.linhas + lin) * m->f.tam, m->f.classe);

            BirEmitirGrava(m->c, m->dst, (int32_t)((lin * m->f.colunas + col) * m->f.tam), v);
        }
    }
}

/* a [R, K] -> tD [K, R]: tD.m[lin * K + col] = a.m[col * R + lin]. */
static bool Transposta(Mat *m)
{
    const IrInstr *i = m->i;

    if (!FormaDaMatriz(m->c, i->a.tipo, &m->f) || !LimiteDeTermos(m, m->f.linhas * m->f.colunas))
        return false;
    if (!BirEnderecoDe(m->c, i, &i->a, &m->a) || !BirSlotDoDestino(m->c, i, &m->dst))
        return false;
    CopiarTransposta(m);
    return true;
}

/* Grava cada args[k] em m[(k % C) * R + k / C]. */
static bool GravarArgsDoLiteral(Mat *m)
{
    const IrInstr *i = m->i;

    for (uint32_t k = 0; k < i->nargs; k++) {
        uint32_t indice = (k % m->f.colunas) * m->f.linhas + k / m->f.colunas;
        BirAlvoMem alvo = {.base = m->dst, .offset = indice * m->f.tam, .tipo = m->f.elemento};

        if (!BirGravarEm(m->c, i, &alvo, &i->args[k]))
            return false;
    }
    return true;
}

/* Os R*C args em ordem de linhas: o k-esimo vai para m[(k % C) * R + k / C]. */
static bool Literal(Mat *m)
{
    const IrInstr *i = m->i;

    if (!FormaDaMatriz(m->c, i->tipo, &m->f))
        return false;
    if (i->nargs != m->f.linhas * m->f.colunas)
        return BirFalhaInconsistente(m->c, "literal de matrix com elementos diferentes de R*C");
    if (!LimiteDeTermos(m, i->nargs) || !BirSlotDoDestino(m->c, i, &m->dst))
        return false;
    return GravarArgsDoLiteral(m);
}

typedef bool (*MatrizFn)(Mat *m);

/* Indexada por op_aux 0..8. */
static const MatrizFn FORMAS_MATRIZ[] = {Elementar, Elementar, Produto, Elementar, Elementar, Vetor, Elementar, Transposta, Literal};

bool BirRegraMatriz(BirCtx *c, const IrInstr *i)
{
    Mat m = {.c = c, .i = i};

    if (i->op_aux >= sizeof FORMAS_MATRIZ / sizeof FORMAS_MATRIZ[0])
        return BirFalhaInconsistente(c, "IR_MATRIZ com op_aux fora de 0..8");
    return FORMAS_MATRIZ[i->op_aux](&m);
}

/* ---- matrix[i, j] e &matrix[i, j] ---- */

typedef struct { uint32_t base; MatrizForma f; BirOperando linha, coluna; } Acesso;

/* A base do acesso: o endereco do slot (matrix por valor) ou o ponteiro (^matrix). */
static bool BaseDaMatriz(BirCtx *c, const IrInstr *i, uint32_t *base)
{
    BirOperando ponteiro;

    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_PONTEIRO)
        return BirEnderecoDe(c, i, &i->a, base);
    if (!BirOperandoDe(c, i, &i->a, &ponteiro))
        return false;
    *base = EmVreg(c, ponteiro).id;
    return true;
}

/* Linha e coluna em 64 bits; em dev conferidas nessa ordem. Release: literal dentro do limite fica imediato, sem vreg nem conferencia. */
static bool PrepararAcesso(BirCtx *c, const IrInstr *i, Acesso *a)
{
    bool linha_no_limite, coluna_no_limite;

    if (!FormaDaMatriz(c, i->a.tipo, &a->f) || !BaseDaMatriz(c, i, &a->base))
        return false;
    if (!BirInteiro64(c, i, &i->b, &a->linha) || !BirInteiro64(c, i, &i->args[0], &a->coluna))
        return false;
    if (BirConfereLimites(c)) {
        linha_no_limite = BirIndiceNoLimite(c, a->linha, a->f.linhas);
        coluna_no_limite = BirIndiceNoLimite(c, a->coluna, a->f.colunas);
        if (!linha_no_limite)
            a->linha = EmVreg(c, a->linha);
        if (!coluna_no_limite)
            a->coluna = EmVreg(c, a->coluna);
        if (!linha_no_limite)
            BirConferirIndice(c, i, a->linha, OpImed(a->f.linhas, BC_I64));
        if (!coluna_no_limite)
            BirConferirIndice(c, i, a->coluna, OpImed(a->f.colunas, BC_I64));
    }
    return true;
}

static BirOperando Multiplicar(BirCtx *c, BirOperando x, uint32_t k)
{
    if (x.tipo == BO_IMED)
        return OpImed(x.imed * (int64_t)k, BC_I64);
    if (k == 1)
        return x;
    return OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = BIR_MUL, .a = x, .b = OpImed(k, BC_I64)}), BC_I64);
}

static bool ImediatoZero(BirOperando o)
{
    return o.tipo == BO_IMED && o.imed == 0;
}

static BirOperando Somar(BirCtx *c, BirOperando x, BirOperando y)
{
    if (x.tipo == BO_IMED && y.tipo == BO_IMED)
        return OpImed(x.imed + y.imed, BC_I64);
    if (ImediatoZero(y))
        return x;
    if (ImediatoZero(x))
        return y;
    return OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = x, .b = y}), BC_I64);
}

/* O elemento (linha, coluna) mora em [base + (coluna * R + linha) * tam]: imediato vira deslocamento, vreg vira ADD. */
static void EnderecoDoAcesso(BirCtx *c, const Acesso *a, uint32_t *base, int32_t *desloc)
{
    BirOperando posicao = Multiplicar(c, Somar(c, Multiplicar(c, a->coluna, a->f.linhas), a->linha), a->f.tam);

    if (posicao.tipo == BO_IMED) {
        *base = a->base;
        *desloc = (int32_t)posicao.imed;
        return;
    }
    *base = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(a->base, BC_I64), .b = posicao});
    *desloc = 0;
}

bool BirRegraMatrizElemento(BirCtx *c, const IrInstr *i)
{
    Acesso a;
    uint32_t base, dst;
    int32_t desloc;

    if (!PrepararAcesso(c, i, &a) || !BirDefinirDestino(c, a.f.classe, &dst))
        return false;
    EnderecoDoAcesso(c, &a, &base, &desloc);
    BirEmitirCarrega(c, dst, base, desloc);
    return true;
}

bool BirRegraMatrizEndElemento(BirCtx *c, const IrInstr *i)
{
    Acesso a;
    uint32_t base, dst;
    int32_t desloc;

    if (!PrepararAcesso(c, i, &a) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    EnderecoDoAcesso(c, &a, &base, &desloc);
    BirEmitirPedido(c, (BirPedido){.op = BIR_COPIA, .dst = dst, .a = OpVreg(BirEnderecoMais(c, base, (uint32_t)desloc), BC_I64)});
    return true;
}
