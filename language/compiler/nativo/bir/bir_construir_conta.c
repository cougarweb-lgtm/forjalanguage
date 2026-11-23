/* bir_construir_conta.c - contas da IR na BIR: negar, nao, ~, E/OU, & | ^, comparacoes, soma/sub/mul (dev: *_OV com bloco frio) e conversao 'as' entre
 * inteiros de qualquer largura (i8 i16 i32 int u8 u16 u32 uint). DIV, REM e deslocamento moram em bir_construir_divisao.c.
 * Versao: 0.3.6
 * Changelog: 0.3.6 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.3.5 - IA1-L9a: SOMA_VOLTA, SUB_VOLTA e MUL_VOLTA (ContaComVolta: BIR_ADD/SUB/MUL nos dois modos).
 *            0.3.4 - M3 F9b-3: decisoes extraidas de BirRegraBinario, BirRegraUnario, FalhaDaConversao e BirRegraConversao.
 *            0.3.3 - Linguagem v1.11 L13: BirRegraUnario despacha OP_RAIZ para BirRaizReal (bir_construir_real.c).
 *            0.3.2 - M2 P13a: == e != de str vao para BirRegraStrIgual e 'as' para str (de cstr) para BirCstrParaStr (bir_construir_texto.c).
 *            0.3.1 - M2 P11a: 'as' entre ponteiros (^T, ?^T, rawptr) vai para BirConversaoPonteiro (bir_construir_ptr.c).
 *            0.3.0 - M2 P10b: operando real despacha para bir_construir_real.c (BirContaReal, BirNegarReal e, no 'as', BirConversaoReal); inteiro igual.
 *            0.2.0 - M2 P9: todas as larguras (BC_I8 e BC_I16 nas contas), BIT_E BIT_OU BIT_XOU (AND OR XOR na classe do operando) e BIT_NAO (NOT); o
 *                    sufixo do texto vem de BirSufixoInteiro (i8..u64); BirEstendido64 estende pelo sinal do TIPO; a conversao 'as' deixa de ter
 *                    tabela 3x3 e passa a regra por largura e sinal (mesmas faixas do forja_as_<S>_de_i64|u64, _i quando a origem tem sinal e _u
 *                    quando nao tem); BirComparar e BirFrioEstouro publicos para a divisao.
 *            0.1.1 - portao M1 (P1-1): Estendido64 passa o imediato i32 como I64 (com sinal); antes -5 ia ao forja_falha_estouro como 4294967291 (MOV32).
 *            0.1.0 - backend x64 P6: semantica do emit_c (emit_c_instr.c) e do forja_rt.h 0.9.0: textos "soma i32", "subtracao i64", "multiplicacao u64",
 *                    NEGAR = forja_sub(0, a), conversao checada so em dev (forja_as_<alvo>_de_i64|u64), release e conta com wrap.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj>
 */
#include <stdio.h>
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define TEXTO_OPERACAO_MAX 48

/* Sufixo do forja_<op>_<sufixo> (SufixoInteiro): int = i64, uint = u64. */
const char *BirSufixoInteiro(TipoId tipo)
{
    switch (tipo) {
    case TIPO_I8: return "i8";
    case TIPO_I16: return "i16";
    case TIPO_I32: return "i32";
    case TIPO_U8: return "u8";
    case TIPO_U16: return "u16";
    case TIPO_U32: return "u32";
    case TIPO_UINT: return "u64";
    default: return "i64";
    }
}

uint32_t BirComparar(BirCtx *c, uint8_t cond, BirOperando a, BirOperando b)
{
    return BirContaNova(c, BC_I8, (BirPedido){.op = BIR_CMP, .aux = cond, .a = a, .b = b});
}

/* O imediato como o valor de 64 bits que o tipo tem: com sinal estende o sinal da classe, sem sinal zera o resto. */
static int64_t ImediatoEstendido(int64_t imed, uint8_t classe, bool sem_sinal)
{
    switch (classe) {
    case BC_I8: return sem_sinal ? (int64_t)(uint8_t)imed : (int64_t)(int8_t)imed;
    case BC_I16: return sem_sinal ? (int64_t)(uint16_t)imed : (int64_t)(int16_t)imed;
    default: return sem_sinal ? (int64_t)(uint32_t)imed : (int64_t)(int32_t)imed;
    }
}

/* Argumento de 64 bits da falha: o vreg segue estendido pelo sinal do TIPO (i32 -5 chega como -5, u32 4000000000 como 4000000000); o imediato
 * ja guarda o valor, entao passa a classe I64 com o valor estendido (com a classe curta e sem extensao o MOV32 zeraria a metade alta). */
BirOperando BirEstendido64(BirOperando o, TipoId tipo)
{
    bool sem_sinal = TipoEhSemSinal(tipo);

    if (o.classe == BC_I64)
        return o;
    if (o.tipo == BO_VREG)
        return OpComExtensao(o, sem_sinal ? BE_ZERO : BE_SINAL);
    return OpImed(ImediatoEstendido(o.imed, o.classe, sem_sinal), BC_I64);
}

BirOperando BirVregDoArquivo(BirCtx *c, uint32_t pos, uint32_t *linha)
{
    const char *arquivo;

    BirArquivoELinha(c, pos, &arquivo, linha);
    return OpVreg(BirVregDoTexto(c, arquivo, (uint32_t)strlen(arquivo)), BC_I64);
}

/* ---- soma, sub, mul, negar ---- */

typedef struct { uint8_t ov, simples; const char *texto; } BirAritmetica;

/* Na ordem OP_SOMA, OP_SUB, OP_MUL. */
static const BirAritmetica ARITMETICA[] = {
    {BIR_ADD_OV, BIR_ADD, "soma"}, {BIR_SUB_OV, BIR_SUB, "subtracao"}, {BIR_MUL_OV, BIR_MUL, "multiplicacao"},
};

/* forja_falha_estouro[_u]("<operacao> <sufixo>", a, b, arquivo, linha), no bloco frio. */
void BirFrioEstouro(BirCtx *c, uint32_t frio, const char *operacao, const BirOperando ab[2])
{
    char texto[TEXTO_OPERACAO_MAX];
    BirOperando args[5];
    uint32_t salvo = c->cur, linha;
    TipoId tipo = c->atual->a.tipo;
    int n = snprintf(texto, sizeof texto, "%s %s", operacao, BirSufixoInteiro(tipo));

    c->cur = frio;
    args[0] = OpVreg(BirVregDoTexto(c, texto, (uint32_t)n), BC_I64);
    args[1] = BirEstendido64(ab[0], tipo);
    args[2] = BirEstendido64(ab[1], tipo);
    args[3] = BirVregDoArquivo(c, c->atual->pos, &linha);
    args[4] = OpImed(linha, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, TipoEhSemSinal(tipo) ? "forja_falha_estouro_u" : "forja_falha_estouro"), args, 5);
    c->cur = salvo;
}

/* Conta com saida para o frio quando estoura (dev). O frio nasce antes da instrucao e e preenchido logo depois dela. */
static bool ContaChecada(BirCtx *c, BirOp op_ov, const char *operacao, const BirOperando ab[2])
{
    uint32_t dst, frio;
    BirInstr *ins;

    if (!BirDefinirDestino(c, ab[0].classe, &dst))
        return false;
    frio = BirNovoBloco(c, true);
    ins = BirEmitirPedido(c, (BirPedido){.op = op_ov, .dst = dst, .a = ab[0], .b = ab[1]});
    ins->aux = TipoEhSemSinal(c->atual->a.tipo) ? BIR_SEM_SINAL : BIR_SINAL;
    ins->alvo[0] = frio;
    BirFrioEstouro(c, frio, operacao, ab);
    return true;
}

static bool Aritmetica(BirCtx *c, const IrInstr *i, const BirOperando ab[2])
{
    const BirAritmetica *ar = &ARITMETICA[i->op_aux - OP_SOMA];
    uint32_t dst;

    if (c->p->modo == BM_DEV)
        return ContaChecada(c, (BirOp)ar->ov, ar->texto, ab);
    if (!BirDefinirDestino(c, ab[0].classe, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = ar->simples, .dst = dst, .a = ab[0], .b = ab[1]});
    return true;
}

/* Na ordem OP_IGUAL..OP_MAIOR_IGUAL. */
static const uint8_t CONDS_COM_SINAL[] = {BCOND_EQ, BCOND_NE, BCOND_LT_S, BCOND_LE_S, BCOND_GT_S, BCOND_GE_S};
static const uint8_t CONDS_SEM_SINAL[] = {BCOND_EQ, BCOND_NE, BCOND_LT_U, BCOND_LE_U, BCOND_GT_U, BCOND_GE_U};

static bool Comparacao(BirCtx *c, const IrInstr *i, const BirOperando ab[2])
{
    bool sem_sinal = TipoEhSemSinal(i->a.tipo) || i->a.tipo == TIPO_BOOL;
    const uint8_t *conds = sem_sinal ? CONDS_SEM_SINAL : CONDS_COM_SINAL;
    uint32_t dst;

    if (!BirDefinirDestino(c, BC_I8, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_CMP, .aux = conds[i->op_aux - OP_IGUAL], .dst = dst, .a = ab[0], .b = ab[1]});
    return true;
}

/* SOMA_VOLTA, SUB_VOLTA e MUL_VOLTA (IA1-L9a): a conta crua do release, tambem em dev (sem *_OV e sem frio). */
static const uint8_t OPS_DE_VOLTA[] = {BIR_ADD, BIR_SUB, BIR_MUL};

static bool ContaComVolta(BirCtx *c, const IrInstr *i, const BirOperando ab[2])
{
    uint32_t dst;

    if (!BirDefinirDestino(c, ab[0].classe, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = OPS_DE_VOLTA[i->op_aux - OP_SOMA_VOLTA], .dst = dst, .a = ab[0], .b = ab[1]});
    return true;
}

/* BIT_E, BIT_OU e BIT_XOU (consecutivos no enum Operador): a conta na largura do operando, sem checagem nos dois modos. */
static const uint8_t OPS_DE_BIT[] = {BIR_AND, BIR_OR, BIR_XOR};

static bool ContaDeBits(BirCtx *c, const IrInstr *i, const BirOperando ab[2])
{
    uint32_t dst;

    if (!BirDefinirDestino(c, ab[0].classe, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = OPS_DE_BIT[i->op_aux - OP_BIT_E], .dst = dst, .a = ab[0], .b = ab[1]});
    return true;
}

/* E e OU logicos: AND e OR na classe i8. */
static bool ContaLogica(BirCtx *c, const IrInstr *i, const BirOperando ab[2])
{
    uint32_t dst;

    if (!BirDefinirDestino(c, BC_I8, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = i->op_aux == OP_E ? BIR_AND : BIR_OR, .dst = dst, .a = ab[0], .b = ab[1]});
    return true;
}

static bool BinarioInteiro(BirCtx *c, const IrInstr *i, const BirOperando ab[2])
{
    switch (i->op_aux) {
    case OP_SOMA: case OP_SUB: case OP_MUL:
        return Aritmetica(c, i, ab);
    case OP_SOMA_VOLTA: case OP_SUB_VOLTA: case OP_MUL_VOLTA:
        return ContaComVolta(c, i, ab);
    case OP_BIT_E: case OP_BIT_OU: case OP_BIT_XOU:
        return ContaDeBits(c, i, ab);
    case OP_E: case OP_OU:
        return ContaLogica(c, i, ab);
    default:
        return Comparacao(c, i, ab);
    }
}

bool BirRegraBinario(BirCtx *c, const IrInstr *i)
{
    BirOperando ab[2];

    if (i->a.tipo == TIPO_STR)
        return BirRegraStrIgual(c, i);
    if (!BirOperandoDe(c, i, &i->a, &ab[0]) || !BirOperandoDe(c, i, &i->b, &ab[1]))
        return false;
    if (BirClasseEhReal(ab[0].classe))
        return BirContaReal(c, i, ab);
    return BinarioInteiro(c, i, ab);
}

static bool ContaNao(BirCtx *c, BirOperando a)
{
    uint32_t dst;

    if (!BirDefinirDestino(c, BC_I8, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_XOR, .dst = dst, .a = a, .b = OpImed(1, BC_I8)});
    return true;
}

static bool UnarioInteiro(BirCtx *c, const IrInstr *i, BirOperando a)
{
    uint32_t dst;

    if (i->op_aux == OP_NAO)
        return ContaNao(c, a);
    if (i->op_aux == OP_NEGAR && c->p->modo == BM_DEV)
        return ContaChecada(c, BIR_NEG_OV, "subtracao", (const BirOperando[2]){OpImed(0, a.classe), a});
    if (!BirDefinirDestino(c, a.classe, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = i->op_aux == OP_BIT_NAO ? BIR_NOT : BIR_NEG, .dst = dst, .a = a});
    return true;
}

/* NAO = XOR #1; BIT_NAO = NOT na largura; NEGAR = forja_sub(0, a) em dev (texto "subtracao") e NEG em release. */
bool BirRegraUnario(BirCtx *c, const IrInstr *i)
{
    BirOperando a;

    if (!BirOperandoDe(c, i, &i->a, &a))
        return false;
    if (i->op_aux == OP_RAIZ)
        return BirRaizReal(c, a);
    if (BirClasseEhReal(a.classe))
        return BirNegarReal(c, a);
    return UnarioInteiro(c, i, a);
}

/* ---- conversao ---- */

typedef enum { FALHA_NENHUMA, FALHA_NEGATIVO, FALHA_FAIXA, FALHA_ACIMA } BirFalhaConversao;

/* ConversaoSempreCabe: mesmo sinal com largura >=, ou sem sinal para com sinal de largura estritamente maior. */
static bool SempreCabe(const BirCtx *c, TipoId origem, TipoId destino)
{
    uint8_t lo = TipoLarguraBits(c->e->tipos, origem), ld = TipoLarguraBits(c->e->tipos, destino);
    bool so = TipoEhSemSinal(origem), sd = TipoEhSemSinal(destino);

    return so == sd ? ld >= lo : (so && ld > lo);
}

/* So o negativo falha: a largura e a mesma, ou a origem tem sinal e o destino e mais largo. */
static bool SoNegativoFalha(uint8_t lo, uint8_t ld, bool origem_sem_sinal)
{
    return lo == ld || (!origem_sem_sinal && ld > lo);
}

/* Teste de dev quando o valor pode nao caber. Origem com sinal: so o negativo quando o destino sem sinal e mais largo (ou quando a troca de
 * sinal mantem a largura), a faixa MIN..MAX quando o destino e mais estreito. Origem sem sinal: so o teto (o bit alto, se a largura e a mesma). */
static BirFalhaConversao FalhaDaConversao(const BirCtx *c, TipoId origem, TipoId destino)
{
    uint8_t lo = TipoLarguraBits(c->e->tipos, origem), ld = TipoLarguraBits(c->e->tipos, destino);
    bool origem_sem_sinal = TipoEhSemSinal(origem);

    if (origem == destino || SempreCabe(c, origem, destino))
        return FALHA_NENHUMA;
    if (SoNegativoFalha(lo, ld, origem_sem_sinal))
        return FALHA_NEGATIVO;
    return origem_sem_sinal ? FALHA_ACIMA : FALHA_FAIXA;
}

/* vreg bool: o valor v falha a conversao? */
static uint32_t CondicaoDeFalha(BirCtx *c, uint8_t falha, BirOperando v)
{
    TipoId destino = c->atual->tipo;
    int64_t maximo = (int64_t)TipoInteiroMax(destino);
    uint32_t baixo, alto;

    if (falha == FALHA_ACIMA)
        return BirComparar(c, BCOND_GT_U, v, OpImed(maximo, v.classe));
    if (falha == FALHA_NEGATIVO)
        return BirComparar(c, BCOND_LT_S, v, OpImed(0, v.classe));
    baixo = BirComparar(c, BCOND_LT_S, v, OpImed(TipoInteiroMin(destino), v.classe));
    alto = BirComparar(c, BCOND_GT_S, v, OpImed(maximo, v.classe));
    return BirContaNova(c, BC_I8, (BirPedido){.op = BIR_OR, .a = OpVreg(baixo, BC_I8), .b = OpVreg(alto, BC_I8)});
}

/* forja_falha_conversao_i|u(valor, "<alvo>", arquivo, linha) no frio: _u quando a origem e sem sinal. */
static void FrioConversao(BirCtx *c, uint32_t frio, BirOperando v)
{
    const IrInstr *i = c->atual;
    const char *alvo = BirSufixoInteiro(i->tipo);
    BirOperando args[4];
    uint32_t salvo = c->cur, linha;

    c->cur = frio;
    args[0] = BirEstendido64(v, i->a.tipo);
    args[1] = OpVreg(BirVregDoTexto(c, alvo, (uint32_t)strlen(alvo)), BC_I64);
    args[2] = BirVregDoArquivo(c, i->pos, &linha);
    args[3] = OpImed(linha, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, TipoEhSemSinal(i->a.tipo) ? "forja_falha_conversao_u" : "forja_falha_conversao_i"), args, 4);
    c->cur = salvo;
}

/* dev: testa a faixa e desvia para o frio; a conversao pura segue no bloco novo. */
static bool ConversaoChecada(BirCtx *c, uint8_t falha, BirOperando *v)
{
    uint32_t frio, segue, condicao;

    if (!BirOperandoVreg(c, *v, v))
        return false;
    condicao = CondicaoDeFalha(c, falha, *v);
    frio = BirNovoBloco(c, true);
    segue = BirNovoBloco(c, false);
    BirDesvia(c, OpVreg(condicao, BC_I8), frio, segue);
    FrioConversao(c, frio, *v);
    c->cur = segue;
    return true;
}

/* A operacao pura (release, ou dev quando cabe): alarga pelo sinal da ORIGEM (o C faz (uint64_t)(int32_t)x), trunca ou so copia. */
static BirOp OpDaConversao(uint8_t classe_origem, uint8_t classe_destino, bool origem_sem_sinal)
{
    if (classe_destino > classe_origem)
        return origem_sem_sinal ? BIR_EXT_Z : BIR_EXT_S;
    return classe_destino < classe_origem ? BIR_TRUNC : BIR_COPIA;
}

/* dev: checa a faixa so quando a conversao pode falhar. */
static bool ConversaoValidada(BirCtx *c, BirFalhaConversao falha, BirOperando *v)
{
    if (c->p->modo != BM_DEV || falha == FALHA_NENHUMA)
        return true;
    return ConversaoChecada(c, falha, v);
}

static bool ConversaoPura(BirCtx *c, const IrInstr *i, BirOperando v)
{
    uint8_t classe = BC_NADA;
    uint32_t dst;

    BirClasseDoTipo(c->e->tipos, i->tipo, &classe);
    if (!BirDefinirDestino(c, classe, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = OpDaConversao(v.classe, classe, TipoEhSemSinal(i->a.tipo)), .dst = dst, .a = v});
    return true;
}

static bool ConversaoInteira(BirCtx *c, const IrInstr *i)
{
    BirOperando v;

    if (!TipoEhInteiro(i->a.tipo) || !TipoEhInteiro(i->tipo))
        return BirFalhaTipo(c, i, "-", TipoEhInteiro(i->a.tipo) ? i->tipo : i->a.tipo);
    if (!BirOperandoDe(c, i, &i->a, &v))
        return false;
    if (!ConversaoValidada(c, FalhaDaConversao(c, i->a.tipo, i->tipo), &v))
        return false;
    return ConversaoPura(c, i, v);
}

bool BirRegraConversao(BirCtx *c, const IrInstr *i)
{
    if (i->tipo == TIPO_STR)
        return BirCstrParaStr(c, i);
    if (BirEhConversaoPonteiro(c, i))
        return BirConversaoPonteiro(c, i);
    if (TipoEhReal(c->e->tipos, i->a.tipo) || TipoEhReal(c->e->tipos, i->tipo))
        return BirConversaoReal(c, i);
    return ConversaoInteira(c, i);
}
