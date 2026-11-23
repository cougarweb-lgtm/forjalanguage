/* bir_construir_divisao.c - divisao, resto e deslocamento da IR na BIR, com os blocos explicitos que garantem as pre-condicoes de BIR_DIV, BIR_REM,
 * BIR_SHL e BIR_SHR (o validador confere a forma: bir_validar.c, GuardaValida).
 * Versao: 0.1.3
 * Changelog: 0.1.3 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.1.2 - M3 F9b-3: decisoes extraidas de BirRegraDivisao.
 *            0.1.1 - M2 P10b: a divisao de real sai pela BirContaReal (sem guarda); o resto e o deslocamento so existem em inteiro.
 *            0.1.0 - M2 P9: semantica do forja_rt.h 0.10.0. Divisor 0 devolve 0 nos dois modos. Com sinal e divisor -1: divisao = 0 - a (wrap) e resto = 0;
 *                    em dev, i32/i64 falham em a == MIN nas duas contas ("divisao <sufixo>", "resto <sufixo>") e i8/i16 so na divisao (o resto de
 *                    i8/i16 nao falha). Deslocamento so sem sinal: contador >= largura (sem sinal) devolve 0. Divisor ou contador imediato seguro
 *                    vira uma instrucao so; imediato 0 vira CONST 0.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj>
 */
#include "nativo/bir/bir_interno.h"

/* A conta em construcao: operandos, destino, classe e o bloco de juncao (so existe quando o divisor ou o contador e um vreg). */
typedef struct {
    BirCtx *c;
    const IrInstr *i;
    BirOperando a, b;
    uint32_t dst, fim;
    uint8_t classe;
    bool sem_sinal, resto;
} Conta;

static unsigned LarguraEmBits(uint8_t classe)
{
    return classe == BC_I8 ? 8u : (classe == BC_I16 ? 16u : (classe == BC_I32 ? 32u : 64u));
}

static void EmitirOp(const Conta *k, BirOp op)
{
    BirEmitirPedido(k->c, (BirPedido){.op = op, .aux = k->sem_sinal ? BIR_SEM_SINAL : BIR_SINAL, .dst = k->dst, .a = k->a, .b = k->b});
}

static void Zerar(const Conta *k)
{
    BirMover(k->c, k->dst, OpImed(0, k->classe));
}

/* Bloco do ramo "divisor 0" ou "contador fora da largura": dst = 0 e segue para a juncao. */
static void BlocoDeZero(const Conta *k, uint32_t bloco)
{
    k->c->cur = bloco;
    Zerar(k);
    BirSalta(k->c, k->fim);
}

/* Bloco que faz a conta (precedido da guarda) e segue para a juncao. */
static void BlocoDaConta(const Conta *k, uint32_t bloco, BirOp op)
{
    k->c->cur = bloco;
    EmitirOp(k, op);
    BirSalta(k->c, k->fim);
}

/* ---- deslocamento ---- */

static void Deslocamento(Conta *k)
{
    BirCtx *c = k->c;
    BirOp op = k->i->op_aux == OP_DESLOCA_ESQ ? BIR_SHL : BIR_SHR;
    unsigned bits = LarguraEmBits(k->classe);
    uint32_t zero, calcula, fora;

    if (k->b.tipo == BO_IMED) {
        if ((uint64_t)k->b.imed >= bits)
            Zerar(k);
        else
            EmitirOp(k, op);
        return;
    }
    k->fim = BirNovoBloco(c, false);
    zero = BirNovoBloco(c, false);
    calcula = BirNovoBloco(c, false);
    fora = BirComparar(c, BCOND_GE_U, k->b, OpImed(bits, k->classe));
    BirDesvia(c, OpVreg(fora, BC_I8), zero, calcula);
    BlocoDeZero(k, zero);
    BlocoDaConta(k, calcula, op);
    c->cur = k->fim;
}

/* ---- divisao e resto ---- */

/* dev: a == MIN com divisor -1 nao cabe no tipo, e a conta falha por forja_falha_estouro (i8/i16 so na divisao). */
static void TestarMinimo(Conta *k)
{
    BirCtx *c = k->c;
    uint32_t frio = BirNovoBloco(c, true), segue = BirNovoBloco(c, false);
    uint32_t eh_minimo = BirComparar(c, BCOND_EQ, k->a, OpImed(TipoInteiroMin(k->i->a.tipo), k->classe));

    BirDesvia(c, OpVreg(eh_minimo, BC_I8), frio, segue);
    BirFrioEstouro(c, frio, k->resto ? "resto" : "divisao", (const BirOperando[2]){k->a, k->b});
    c->cur = segue;
}

/* Divisor -1: resto = 0; divisao = 0 - a com wrap. A guarda de dev vem antes. */
static void CaminhoMenosUm(Conta *k)
{
    bool falha_em_dev = k->c->p->modo == BM_DEV && (!k->resto || k->classe >= BC_I32);

    if (falha_em_dev)
        TestarMinimo(k);
    if (k->resto)
        Zerar(k);
    else
        BirEmitirPedido(k->c, (BirPedido){.op = BIR_SUB, .dst = k->dst, .a = OpImed(0, k->classe), .b = k->a});
}

static void DivisaoSemSinal(Conta *k, BirOp op)
{
    BirCtx *c = k->c;
    uint32_t zero = BirNovoBloco(c, false), calcula = BirNovoBloco(c, false);
    uint32_t nao_zero = BirComparar(c, BCOND_NE, k->b, OpImed(0, k->classe));

    BirDesvia(c, OpVreg(nao_zero, BC_I8), calcula, zero);
    BlocoDeZero(k, zero);
    BlocoDaConta(k, calcula, op);
    c->cur = k->fim;
}

/* b == 0 -> 0; b == -1 -> caminho do -1; senao a conta (que so e alcancada com b != 0 e b != -1). */
static void DivisaoComSinal(Conta *k, BirOp op)
{
    BirCtx *c = k->c;
    uint32_t zero = BirNovoBloco(c, false), testa = BirNovoBloco(c, false);
    uint32_t menos_um = BirNovoBloco(c, false), calcula = BirNovoBloco(c, false);
    uint32_t eh_zero = BirComparar(c, BCOND_EQ, k->b, OpImed(0, k->classe)), eh_menos_um;

    BirDesvia(c, OpVreg(eh_zero, BC_I8), zero, testa);
    BlocoDeZero(k, zero);
    c->cur = testa;
    eh_menos_um = BirComparar(c, BCOND_EQ, k->b, OpImed(-1, k->classe));
    BirDesvia(c, OpVreg(eh_menos_um, BC_I8), menos_um, calcula);
    c->cur = menos_um;
    CaminhoMenosUm(k);
    BirSalta(c, k->fim);
    BlocoDaConta(k, calcula, op);
    c->cur = k->fim;
}

/* Divisor imediato: 0 devolve 0, -1 vai ao caminho do -1 e qualquer outro e a conta direta, sem guarda. */
static void DivisorImediato(Conta *k, BirOp op)
{
    if (k->b.imed == 0)
        Zerar(k);
    else if (!k->sem_sinal && k->b.imed == -1)
        CaminhoMenosUm(k);
    else
        EmitirOp(k, op);
}

static void Divisao(Conta *k)
{
    BirOp op = k->resto ? BIR_REM : BIR_DIV;

    if (k->b.tipo == BO_IMED) {
        DivisorImediato(k, op);
        return;
    }
    k->fim = BirNovoBloco(k->c, false);
    if (k->sem_sinal)
        DivisaoSemSinal(k, op);
    else
        DivisaoComSinal(k, op);
}

static bool EhDeslocamento(uint8_t op)
{
    return op == OP_DESLOCA_ESQ || op == OP_DESLOCA_DIR;
}

/* DIV, RESTO, DESLOCA_ESQ e DESLOCA_DIR (tabela REGRAS_IR). */
bool BirRegraDivisao(BirCtx *c, const IrInstr *i)
{
    BirOperando ab[2];
    Conta k = {.c = c, .i = i, .sem_sinal = TipoEhSemSinal(i->a.tipo), .resto = i->op_aux == OP_RESTO};

    if (!BirOperandoDe(c, i, &i->a, &ab[0]) || !BirOperandoDe(c, i, &i->b, &ab[1]))
        return false;
    if (BirClasseEhReal(ab[0].classe)) /* a / b de real e a conta direta, sem a guarda do divisor */
        return BirContaReal(c, i, ab);
    if (!BirDefinirDestino(c, ab[0].classe, &k.dst))
        return false;
    k.a = ab[0];
    k.b = ab[1];
    k.classe = ab[0].classe;
    if (EhDeslocamento(i->op_aux))
        Deslocamento(&k);
    else
        Divisao(&k);
    return true;
}
