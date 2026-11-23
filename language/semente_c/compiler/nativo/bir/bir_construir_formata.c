/* bir_construir_formata.c - texto formatado na construcao da BIR: IR_FORMATA (f"..." que imprime, format -> str e format_c -> cstr), sobre o runtime nativo (rt_formata.c).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-L5a: FMT_CURTO no formato escolhe forja_fmt_f32_curto/f64_curto (print de real); op_aux 3 (eprint) chama forja_fmt_imprimir_erro.
 *            0.1.1 - M3 F9b-3: decisoes extraidas de ArgumentoDaParte.
 *            0.1.0 - M2 P15b: primeira versao. As MESMAS chamadas e argumentos que o emit_c_instr.c escreve: forja_fmt_inicio(); uma forja_fmt_<tipo>(valor, formato) por par de args, na
 *                    ordem (inteiro com sinal estendido a 64 por forja_fmt_i64, sem sinal e bool por forja_fmt_u64/forja_fmt_bool, f32 alargado a double antes de forja_fmt_f32 como o
 *                    (double)(v) do C, f64 direto, str por valor, cstr pelo ponteiro); e o final conforme op_aux: 0 forja_fmt_imprimir(); 1 forja_fmt_str_final(alocador, arquivo, linha)
 *                    com o str no slot do temporario (retorno agregado pelo CHAMA.a); 2 forja_fmt_cstr_final(alocador, arquivo, linha) com o ponteiro num vreg I64. Alocador = u32
 *                    (V_INT 1 heap, 2 frame) ou o handle de Arena (BirAlocadorDe).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/formato
 */
#include "nativo/bir/bir_interno.h"

#define FORMATA_AUX_QTD 4u

/* O que cada classe de tipo chama: funcao do runtime e extensao do argumento (os de 8, 16 e 32 bits seguem estendidos a 64). TC_TEXTO vai a parte (agregado). */
typedef struct { TipoClasse classe; const char *funcao; uint8_t extensao; } ParteFormata;

static const ParteFormata PARTES[] = {
    {TC_CSTR, "forja_fmt_cstr", BE_NENHUMA}, {TC_BOOL, "forja_fmt_bool", BE_ZERO},
    {TC_I8, "forja_fmt_i64", BE_SINAL}, {TC_I16, "forja_fmt_i64", BE_SINAL}, {TC_I32, "forja_fmt_i64", BE_SINAL}, {TC_INT, "forja_fmt_i64", BE_NENHUMA},
    {TC_U8, "forja_fmt_u64", BE_ZERO}, {TC_U16, "forja_fmt_u64", BE_ZERO}, {TC_U32, "forja_fmt_u64", BE_ZERO}, {TC_UINT, "forja_fmt_u64", BE_NENHUMA},
    {TC_REAL, "forja_fmt_f32", BE_NENHUMA}, {TC_F64, "forja_fmt_f64", BE_NENHUMA},
};

static const ParteFormata PARTE_TEXTO = {TC_TEXTO, "forja_fmt_str", BE_NENHUMA};

static const ParteFormata *AcharParte(TipoClasse classe)
{
    if (classe == TC_TEXTO)
        return &PARTE_TEXTO;
    for (uint32_t k = 0; k < sizeof PARTES / sizeof PARTES[0]; k++)
        if (PARTES[k].classe == classe)
            return &PARTES[k];
    return NULL;
}

/* A parte escolhida pela classe do tipo e o valor da IR que ela escreve. */
typedef struct { const ParteFormata *parte; const Valor *valor; } PedidoParte;

/* cstr e real viajam em vreg. */
static bool ParteEmVreg(TipoClasse classe)
{
    switch (classe) {
    case TC_CSTR: case TC_REAL: case TC_F64:
        return true;
    default:
        return false;
    }
}

/* f32 alarga a double antes de forja_fmt_f32, como o (double)(v) do C. */
static void AlargarRealParaDouble(BirCtx *c, TipoClasse classe, BirOperando *saida)
{
    if (classe == TC_REAL && saida->classe == BC_F32)
        *saida = OpVreg(BirContaNova(c, BC_F64, (BirPedido){.op = BIR_F_PARA_F, .a = *saida}), BC_F64);
}

/* O valor como argumento: str por copia privada + endereco; real e cstr em vreg (f32 alarga a double); inteiro e bool com a extensao da tabela. */
static bool ArgumentoDaParte(BirCtx *c, const IrInstr *i, const PedidoParte *pedido, BirOperando *saida)
{
    const ParteFormata *parte = pedido->parte;
    const Valor *valor = pedido->valor;

    if (parte->classe == TC_TEXTO)
        return BirArgumentoAgregado(c, i, valor, saida);
    if (!BirOperandoDe(c, i, valor, saida))
        return false;
    if (ParteEmVreg(parte->classe) && !BirOperandoVreg(c, *saida, saida))
        return false;
    AlargarRealParaDouble(c, parte->classe, saida);
    *saida = OpComExtensao(*saida, parte->extensao);
    return true;
}

/* Real com FMT_CURTO (print de f32/f64) chama a variante _curto, que mora no objeto do Ryu; o resto, a funcao da tabela. */
static const char *FuncaoDaParte(const ParteFormata *parte, uint32_t fmt)
{
    if ((fmt & FMT_CURTO) == 0 || (parte->classe != TC_REAL && parte->classe != TC_F64))
        return parte->funcao;
    return parte->classe == TC_REAL ? "forja_fmt_f32_curto" : "forja_fmt_f64_curto";
}

/* forja_fmt_<tipo>(valor, formato): o formato e o V_INT do par (largura, zeros, hexa, precisao, curto). */
static bool EmitirParte(BirCtx *c, const IrInstr *i, const Valor *formato, const Valor *valor)
{
    PedidoParte pedido = {.parte = AcharParte(TipoClasseDe(c->e->tipos, valor->tipo)), .valor = valor};
    BirOperando *args = BirArgs(c, 2);

    if (pedido.parte == NULL)
        return BirFalhaTipo(c, i, "-", valor->tipo);
    if (!ArgumentoDaParte(c, i, &pedido, &args[0]))
        return false;
    args[1] = OpImed((int64_t)(uint32_t)formato->inteiro, BC_I32);
    BirChamarRuntime(c, &(BirChamadaRt){FuncaoDaParte(pedido.parte, (uint32_t)formato->inteiro), BC_NADA, 2}, args);
    return true;
}

/* ---- final: op_aux 0, 1 e 2 ---- */

static bool FinalImprime(BirCtx *c, const IrInstr *i)
{
    (void)i;
    BirChamarRuntime(c, &(BirChamadaRt){"forja_fmt_imprimir", BC_NADA, 0}, NULL);
    return true;
}

/* eprint: o buffer e '\n' em stderr. */
static bool FinalImprimeErro(BirCtx *c, const IrInstr *i)
{
    (void)i;
    BirChamarRuntime(c, &(BirChamadaRt){"forja_fmt_imprimir_erro", BC_NADA, 0}, NULL);
    return true;
}

/* alocador, arquivo, linha: os 3 argumentos de forja_fmt_str_final e forja_fmt_cstr_final. */
static bool ArgumentosFinal(BirCtx *c, const IrInstr *i, BirOperando *args)
{
    uint32_t linha;

    if (!BirAlocadorDe(c, i, &i->a, &args[0]))
        return false;
    args[1] = BirVregDoArquivo(c, i->pos, &linha);
    args[2] = OpImed(linha, BC_I32);
    return true;
}

/* temp(str) = forja_fmt_str_final(...): o str de 16 B sai no slot do temporario pelo CHAMA.a. */
static bool FinalStr(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    BirOperando *args = BirArgs(c, 3);
    uint32_t destino;
    BirInstr *chama;

    if (!ArgumentosFinal(c, i, args) || !BirForma(c, TIPO_STR, &pedido.forma) || !BirDefinirSlot(c, &pedido, &destino))
        return false;
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, "forja_fmt_str_final");
    chama->args = args;
    chama->nargs = 3;
    chama->a = OpVreg(destino, BC_I64);
    chama->a.tam = pedido.forma.tam;
    return true;
}

/* temp(cstr) = forja_fmt_cstr_final(...): o ponteiro num vreg I64. */
static bool FinalCstr(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 3);
    uint32_t dst;

    if (!ArgumentosFinal(c, i, args) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirMover(c, dst, OpVreg(BirChamarRuntime(c, &(BirChamadaRt){"forja_fmt_cstr_final", BC_I64, 3}, args), BC_I64));
    return true;
}

typedef bool (*FinalFn)(BirCtx *c, const IrInstr *i);

/* Indexada por op_aux. */
static const FinalFn FINAIS[FORMATA_AUX_QTD] = {FinalImprime, FinalStr, FinalCstr, FinalImprimeErro};

bool BirRegraFormata(BirCtx *c, const IrInstr *i)
{
    if ((i->nargs & 1u) != 0 || i->op_aux >= FORMATA_AUX_QTD)
        return BirFalhaInconsistente(c, "IR_FORMATA com args que nao sao pares ou op_aux desconhecido");
    BirChamarRuntime(c, &(BirChamadaRt){"forja_fmt_inicio", BC_NADA, 0}, NULL);
    for (uint32_t k = 0; k < i->nargs; k += 2) {
        if (!EmitirParte(c, i, &i->args[k], &i->args[k + 1]))
            return false;
    }
    return FINAIS[i->op_aux](c, i);
}
