/* bir_construir_real.c - reais (f32, f64 e unidade) da IR na BIR: literal, soma/sub/mul/div, comparacao, negar e a conversao 'as' (inteiro <-> real, f32 <-> f64).
 * Versao: 0.2.2
 * Changelog: 0.2.2 - G3 G0c: literal f32 por conversao direta (float)double, sem o texto "%.9g"; so difere do caminho antigo no literal que cai perto do ponto medio entre dois floats.
 *            0.2.1 - M3 F9b-3: decisoes extraidas de BirContaReal, InteiroParaReal e BirConversaoReal.
 *            0.2.0 - Linguagem v1.11 L13: BirRaizReal (OP_RAIZ vira BIR_RAIZ).
 *            0.1.1 - M2 P16: BirNegarRealEm (a negacao num vreg dado, extraida de BirNegarReal) para a negacao de matrix; o comportamento de BirNegarReal nao muda.
 *            0.1.0 - M2 P10b: semantica do emit_c (emit_c_instr.c) e do forja_rt.h: conta de real direta (sem checagem de estouro, sem FMA); literal f32 = strtof do
 *                    "%.9g" do double (o mesmo texto que o emit_c escreve) e f64 = o proprio double; negar = xorps com a mascara de sinal (BS_DADO); inteiro para real e
 *                    sempre puro (i8 i16 u8 u16 alargam a 32 bits; o uint de 64 bits vira dois blocos como o gcc: test, js, ou metade|bit baixo, converte e dobra);
 *                    real para inteiro CHAMA forja_as_<S>_de_f64 (f32 vai antes a f64, como o (double)(v) do C) e o runtime do modo confere (dev) ou satura (release).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/real
 */
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define MASCARA_BYTES 16u        /* xorps xmm,m128 exige 16 bytes alinhados em 16 */
#define MASCARA_ALINHAMENTO 16u
#define ARGS_AS_DE_REAL 3u       /* (double, arquivo, linha) */

/* ---- literal ---- */

/* f32: conversao direta double -> float (sem passar por texto); f64: os bits do double. O f32 fica nos 32 bits baixos. */
static int64_t BitsDoReal(double valor, uint8_t classe)
{
    uint32_t bits32;
    int64_t bits64;
    float f32;

    if (classe == BC_F64) {
        memcpy(&bits64, &valor, sizeof bits64);
        return bits64;
    }
    f32 = (float)valor;
    memcpy(&bits32, &f32, sizeof bits32);
    return (int64_t)bits32;
}

bool BirImediatoReal(const Valor *v, uint8_t classe, BirOperando *saida)
{
    double valor = v->tipo_no == V_REAL ? v->real : (double)v->inteiro;

    *saida = OpImed(BitsDoReal(valor, classe), classe);
    return true;
}

/* ---- conta, comparacao e negar ---- */

/* Na ordem OP_SOMA, OP_SUB, OP_MUL, OP_DIV. */
static const uint8_t OPS_REAIS[] = {BIR_ADD, BIR_SUB, BIR_MUL, BIR_DIV};
/* Na ordem OP_IGUAL..OP_MAIOR_IGUAL. */
static const uint8_t CONDS_REAIS[] = {BCOND_FEQ, BCOND_FNE, BCOND_FLT, BCOND_FLE, BCOND_FGT, BCOND_FGE};

static bool EhComparacaoReal(uint8_t op)
{
    return op >= OP_IGUAL && op <= OP_MAIOR_IGUAL;
}

static bool EhAritmeticaReal(uint8_t op)
{
    return op >= OP_SOMA && op <= OP_DIV;
}

bool BirContaReal(BirCtx *c, const IrInstr *i, const BirOperando ab[2])
{
    bool compara = EhComparacaoReal(i->op_aux);
    uint32_t dst;

    if (!compara && !EhAritmeticaReal(i->op_aux))
        return BirFalhaInconsistente(c, "operador sem sentido em real");
    if (!BirDefinirDestino(c, compara ? BC_I8 : ab[0].classe, &dst))
        return false;
    if (compara)
        BirEmitirPedido(c, (BirPedido){.op = BIR_CMP, .aux = CONDS_REAIS[i->op_aux - OP_IGUAL], .dst = dst, .a = ab[0], .b = ab[1]});
    else
        BirEmitirPedido(c, (BirPedido){.op = OPS_REAIS[i->op_aux - OP_SOMA], .dst = dst, .a = ab[0], .b = ab[1]});
    return true;
}

/* Bit de sinal na faixa baixa do xmm: f32 no byte 3, f64 no byte 7. */
static const uint8_t MASCARA_F32[MASCARA_BYTES] = {[3] = 0x80};
static const uint8_t MASCARA_F64[MASCARA_BYTES] = {[7] = 0x80};

/* NEG dst = -a; o operando b leva o simbolo da mascara de sinal. -0.0 sai de 0.0 e o NaN troca o sinal, como o (-a) do C. */
void BirNegarRealEm(BirCtx *c, BirOperando a, uint32_t dst)
{
    const uint8_t *mascara = a.classe == BC_F64 ? MASCARA_F64 : MASCARA_F32;
    uint32_t simbolo = BirSimboloDado(c, (const char *)mascara, MASCARA_BYTES, MASCARA_ALINHAMENTO);

    BirEmitirPedido(c, (BirPedido){.op = BIR_NEG, .dst = dst, .a = a, .b = OpSimbolo(simbolo)});
}

bool BirNegarReal(BirCtx *c, BirOperando a)
{
    uint32_t dst;

    if (!BirDefinirDestino(c, a.classe, &dst))
        return false;
    BirNegarRealEm(c, a, dst);
    return true;
}

/* sqrt(a) (OP_RAIZ): BIR_RAIZ na classe do operando; o x64 emite sqrtss/sqrtsd (negativo da NaN, como o __builtin_sqrt de C). */
bool BirRaizReal(BirCtx *c, BirOperando a)
{
    uint32_t dst;

    if (!BirDefinirDestino(c, a.classe, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_RAIZ, .dst = dst, .a = a});
    return true;
}

/* ---- conversao ---- */

/* uint de 64 bits -> real, a sequencia do gcc: sem o bit alto e cvtsi2s direto; com ele converte (v >> 1) | (v & 1) e dobra (o bit baixo OR evita o duplo arredondamento). */
static void UintParaReal(BirCtx *c, BirOperando v, uint32_t dst)
{
    uint32_t grande = BirNovoBloco(c, false), pequeno = BirNovoBloco(c, false), fim = BirNovoBloco(c, false);
    uint32_t negativo = BirComparar(c, BCOND_LT_S, v, OpImed(0, BC_I64));
    uint32_t metade, bit, junta;

    BirDesvia(c, OpVreg(negativo, BC_I8), grande, pequeno);
    c->cur = pequeno;
    BirEmitirPedido(c, (BirPedido){.op = BIR_I_PARA_F, .aux = BIR_SINAL, .dst = dst, .a = v});
    BirSalta(c, fim);
    c->cur = grande;
    metade = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_SHR, .aux = BIR_SEM_SINAL, .a = v, .b = OpImed(1, BC_I64)});
    bit = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_AND, .a = v, .b = OpImed(1, BC_I64)});
    junta = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_OR, .a = OpVreg(metade, BC_I64), .b = OpVreg(bit, BC_I64)});
    BirEmitirPedido(c, (BirPedido){.op = BIR_I_PARA_F, .aux = BIR_SINAL, .dst = dst, .a = OpVreg(junta, BC_I64)});
    BirEmitirPedido(c, (BirPedido){.op = BIR_ADD, .dst = dst, .a = OpVreg(dst, c->f->classe_vreg[dst]), .b = OpVreg(dst, c->f->classe_vreg[dst])});
    BirSalta(c, fim);
    c->cur = fim;
}

/* i8 i16 u8 u16 alargam a 32 bits pelo sinal do tipo. */
static BirOperando AlargarA32(BirCtx *c, BirOperando v, bool sem_sinal)
{
    return OpVreg(BirContaNova(c, BC_I32, (BirPedido){.op = sem_sinal ? BIR_EXT_Z : BIR_EXT_S, .a = v}), BC_I32);
}

/* O uint de 64 bits e UintParaReal; os demais sao um I_PARA_F direto. */
static void InteiroPuroParaReal(BirCtx *c, BirOperando v, bool sem_sinal, uint32_t dst)
{
    if (v.classe == BC_I64 && sem_sinal) {
        UintParaReal(c, v, dst);
        return;
    }
    BirEmitirPedido(c, (BirPedido){.op = BIR_I_PARA_F, .aux = sem_sinal ? BIR_SEM_SINAL : BIR_SINAL, .dst = dst, .a = v});
}

/* Inteiro -> real, sempre puro: i8 i16 u8 u16 alargam a 32 bits (cabem com o sinal do tipo), i32 e int convertem direto, u32 zera o alto e o uint de 64 bits e UintParaReal. */
static bool InteiroParaReal(BirCtx *c, const IrInstr *i, BirOperando v, uint8_t destino)
{
    bool sem_sinal = TipoEhSemSinal(i->a.tipo);
    uint32_t dst;

    if (!BirOperandoVreg(c, v, &v) || !BirDefinirDestino(c, destino, &dst))
        return false;
    if (v.classe < BC_I32) {
        v = AlargarA32(c, v, sem_sinal);
        sem_sinal = false;
    }
    InteiroPuroParaReal(c, v, sem_sinal, dst);
    return true;
}

static bool RealParaReal(BirCtx *c, BirOperando v, uint8_t destino)
{
    uint32_t dst;

    if (!BirDefinirDestino(c, destino, &dst))
        return false;
    if (v.classe == destino)
        BirMover(c, dst, v);
    else
        BirEmitirPedido(c, (BirPedido){.op = BIR_F_PARA_F, .dst = dst, .a = v});
    return true;
}

typedef struct { TipoId tipo; const char *funcao; } BirAsDeReal;

/* A funcao do runtime de cada destino inteiro (int = i64, uint = u64): forja_as_<S>_de_f64. */
static const BirAsDeReal AS_DE_REAL[] = {
    {TIPO_I8, "forja_as_i8_de_f64"}, {TIPO_I16, "forja_as_i16_de_f64"}, {TIPO_I32, "forja_as_i32_de_f64"}, {TIPO_INT, "forja_as_i64_de_f64"},
    {TIPO_U8, "forja_as_u8_de_f64"}, {TIPO_U16, "forja_as_u16_de_f64"}, {TIPO_U32, "forja_as_u32_de_f64"}, {TIPO_UINT, "forja_as_u64_de_f64"},
};

static const char *FuncaoAsDeReal(TipoId destino)
{
    for (uint32_t k = 0; k < sizeof AS_DE_REAL / sizeof AS_DE_REAL[0]; k++) {
        if (AS_DE_REAL[k].tipo == destino)
            return AS_DE_REAL[k].funcao;
    }
    return NULL;
}

/* Real -> inteiro: forja_as_<S>_de_f64(double, arquivo, linha) do runtime do modo; o f32 vai antes a f64 (cvtss2sd), como o (double)(v) do C. */
static bool RealParaInteiro(BirCtx *c, const IrInstr *i, BirOperando v, uint8_t destino)
{
    const char *funcao = FuncaoAsDeReal(i->tipo);
    BirOperando *args = BirArgs(c, ARGS_AS_DE_REAL);
    uint32_t dst, linha;
    BirInstr *chama;

    if (funcao == NULL)
        return BirFalhaTipo(c, i, "-", i->tipo);
    if (v.classe == BC_F32)
        v = OpVreg(BirContaNova(c, BC_F64, (BirPedido){.op = BIR_F_PARA_F, .a = v}), BC_F64);
    args[0] = v;
    args[1] = BirVregDoArquivo(c, i->pos, &linha);
    args[2] = OpImed(linha, BC_I32);
    if (!BirDefinirDestino(c, destino, &dst))
        return false;
    chama = BirEmitir(c, BIR_CHAMA, destino);
    chama->dst = dst;
    chama->simbolo = BirSimboloRuntime(c, funcao);
    chama->args = args;
    chama->nargs = ARGS_AS_DE_REAL;
    return true;
}

/* O destino decide: inteiro por RealParaInteiro; real a partir de real ou de inteiro. */
static bool ConverterParaDestino(BirCtx *c, const IrInstr *i, BirOperando v, uint8_t destino)
{
    const Tipos *tipos = c->e->tipos;

    if (!TipoEhReal(tipos, i->tipo))
        return RealParaInteiro(c, i, v, destino);
    return TipoEhReal(tipos, i->a.tipo) ? RealParaReal(c, v, destino) : InteiroParaReal(c, i, v, destino);
}

bool BirConversaoReal(BirCtx *c, const IrInstr *i)
{
    const Tipos *tipos = c->e->tipos;
    uint8_t destino = BC_NADA;
    BirOperando v;

    if (!TipoEhNumerico(tipos, i->a.tipo) || !TipoEhNumerico(tipos, i->tipo))
        return BirFalhaTipo(c, i, "-", TipoEhNumerico(tipos, i->a.tipo) ? i->tipo : i->a.tipo);
    if (!BirOperandoDe(c, i, &i->a, &v) || !BirClasseDoTipo(tipos, i->tipo, &destino))
        return false;
    return ConverterParaDestino(c, i, v, destino);
}
