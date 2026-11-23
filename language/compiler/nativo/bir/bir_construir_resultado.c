/* bir_construir_resultado.c - IR -> BIR: o resultado T ! E (RES_OK, RES_ERRO, RES_E_ERRO, RES_VALOR, RES_ERRO_DE) e o forja_programa de main -> ! E.
 * Versao: 0.1.2
 * Changelog: 0.1.2 - M3 F9b-3: decisoes extraidas de BirParteDoResultado.
 *            0.1.1 - M2 P15a: BirSlotDoDestino (era NovoResultado) e BirTempDoCampo (o fim de LerParte) viram publicas: bir_construir_opcional.c e bir_construir_chaveia.c leem membros de ?T, union e tupla pelo mesmo caminho.
 *            0.1.0 - M2 P12a: primeira versao. O resultado e um agregado {bool e_erro; T valor; E erro} (sem 'valor' quando T e vazio) na ordem e nos offsets do layout/; vive num slot
 *                    (BirEhAgregado) e as cinco operacoes viram GRAVA/CARREGA/COPIA_MEM sobre ele. Nenhum op novo na BIR.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/resultado
 *
 * Semantica (ir.h 0.14.0): RES_OK grava e_erro = 0 e o valor; RES_ERRO grava e_erro = 1 e o erro; o resto do slot fica sem definir (quem le o erro de um resultado ok, ou o valor de
 * um resultado erro, nao existe: a IR so le a parte certa). forja_programa de 'main -> ! E' chama fj_main e, se e_erro, chama forja_main_erro(arquivo, erro) e devolve 1.
 */
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define SAIDA_COM_ERRO 1
#define SAIDA_SEM_ERRO 0

/* ---- partes do resultado ---- */

/* Sem valor (T vazio) o erro ocupa o membro 1. */
static uint32_t IndiceDaParte(BirParteResultado parte, bool tem_valor)
{
    return parte == RES_PARTE_ERRO && !tem_valor ? 1u : (uint32_t)parte;
}

static TipoId TipoDaParte(const TipoInfo *t, BirParteResultado parte)
{
    switch (parte) {
    case RES_PARTE_FLAG: return TIPO_BOOL;
    case RES_PARTE_VALOR: return t->a;
    default: return t->b;
    }
}

bool BirParteDoResultado(BirCtx *c, TipoId resultado, BirParteResultado parte, BirCampoInfo *info)
{
    const Tipos *tipos = c->e->tipos;
    const TipoInfo *t = TipoDe(tipos, resultado);
    bool tem_valor = TipoClasseDe(tipos, t->a) != TC_VAZIO;
    uint32_t indice = IndiceDaParte(parte, tem_valor);
    TipoId tipo = TipoDaParte(t, parte);

    if (TipoClasseDe(tipos, resultado) != TC_RESULTADO)
        return BirFalhaInconsistente(c, "parte de resultado pedida a um tipo que nao e T ! E");
    if (parte == RES_PARTE_VALOR && !tem_valor)
        return BirFalhaInconsistente(c, "valor de um resultado sem T");
    *info = (BirCampoInfo){.offset = LayoutOffsetCampo(BirLayouts(c), resultado, indice), .tipo = tipo};
    return true;
}

/* O slot do temporario de destino, recem-definido (END_SLOT); base = vreg com o endereco. */
bool BirSlotDoDestino(BirCtx *c, const IrInstr *i, uint32_t *base)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};

    return BirForma(c, i->tipo, &pedido.forma) && BirDefinirSlot(c, &pedido, base);
}

/* ---- construtores ---- */

bool BirRegraResOk(BirCtx *c, const IrInstr *i)
{
    BirCampoInfo valor;
    uint32_t base;

    if (!BirSlotDoDestino(c, i, &base))
        return false;
    BirEmitirGrava(c, base, 0, OpImed(0, BC_I8));
    if (i->a.tipo_no == V_NENHUM) /* T vazio */
        return true;
    if (!BirParteDoResultado(c, i->tipo, RES_PARTE_VALOR, &valor))
        return false;
    return BirGravarEm(c, i, &(BirAlvoMem){.base = base, .offset = valor.offset, .tipo = valor.tipo}, &i->a);
}

bool BirRegraResErro(BirCtx *c, const IrInstr *i)
{
    BirCampoInfo erro;
    uint32_t base;

    if (!BirSlotDoDestino(c, i, &base) || !BirParteDoResultado(c, i->tipo, RES_PARTE_ERRO, &erro))
        return false;
    BirEmitirGrava(c, base, 0, OpImed(1, BC_I8));
    return BirGravarEm(c, i, &(BirAlvoMem){.base = base, .offset = erro.offset, .tipo = erro.tipo}, &i->a);
}

/* Copia o agregado que mora no membro para o slot do temporario de destino. */
static bool LerParteAgregada(BirCtx *c, const IrInstr *i, uint32_t base, const BirCampoInfo *parte)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t dst;

    if (!BirForma(c, parte->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirCopiaBloco(c, dst, BirEnderecoMais(c, base, parte->offset), pedido.forma.tam);
    return true;
}

/* temp = o membro do agregado em `agregado` (resultado, ?T, union, tupla): escalar = CARREGA, agregado = COPIA_MEM para o slot do temp. */
bool BirTempDoCampo(BirCtx *c, const IrInstr *i, uint32_t agregado, const BirCampoInfo *membro)
{
    uint8_t classe = BC_NADA;
    uint32_t dst;

    if (BirEhAgregado(c->e->tipos, membro->tipo))
        return LerParteAgregada(c, i, agregado, membro);
    if (!BirClasseDoTipo(c->e->tipos, membro->tipo, &classe))
        return BirFalhaTipo(c, i, "-", membro->tipo);
    if (!BirDefinirDestino(c, classe, &dst))
        return false;
    BirEmitirCarrega(c, dst, agregado, (int32_t)membro->offset);
    return true;
}

static bool LerParte(BirCtx *c, const IrInstr *i, BirParteResultado parte)
{
    BirCampoInfo info;
    uint32_t base;

    if (!BirEnderecoDe(c, i, &i->a, &base) || !BirParteDoResultado(c, i->a.tipo, parte, &info))
        return false;
    return BirTempDoCampo(c, i, base, &info);
}

bool BirRegraResEErro(BirCtx *c, const IrInstr *i)
{
    return LerParte(c, i, RES_PARTE_FLAG);
}

bool BirRegraResValor(BirCtx *c, const IrInstr *i)
{
    return LerParte(c, i, RES_PARTE_VALOR);
}

bool BirRegraResErroDe(BirCtx *c, const IrInstr *i)
{
    return LerParte(c, i, RES_PARTE_ERRO);
}

/* ---- forja_programa de main -> ! E ---- */

/* Bloco frio: forja_main_erro(arquivo, erro) (R0004 em stderr) e a entrada do PE sai com 1. */
static void RelatarErroDeMain(BirCtx *c, uint32_t base, uint32_t offset_erro)
{
    const char *arquivo = c->e->fonte->caminho;
    BirOperando *args = BirArgs(c, 2);
    BirOperando um = OpImed(SAIDA_COM_ERRO, BC_I32);
    uint32_t erro = BirNovoVreg(c, BC_I32);
    BirInstr *chama;

    BirEmitirCarrega(c, erro, base, (int32_t)offset_erro);
    args[0] = OpVreg(BirVregDoTexto(c, arquivo, (uint32_t)strlen(arquivo)), BC_I64);
    args[1] = OpVreg(erro, BC_I32);
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, "forja_main_erro");
    chama->args = args;
    chama->nargs = 2;
    BirRetorna(c, &um);
}

bool BirMainResultado(BirCtx *c)
{
    BirSlot forma;
    BirCampoInfo erro;
    BirOperando zero = OpImed(SAIDA_SEM_ERRO, BC_I32);
    uint32_t base, flag, falha, ok;
    BirInstr *chama;

    if (!BirForma(c, c->main_tipo, &forma) || !BirParteDoResultado(c, c->main_tipo, RES_PARTE_ERRO, &erro))
        return false;
    base = BirEmitirEndSlot(c, BirNovoSlot(c, forma.tam, forma.alinhamento), BirNovoVreg(c, BC_I64));
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = c->main_simbolo;
    chama->a = OpVreg(base, BC_I64);
    chama->a.tam = forma.tam;
    flag = BirNovoVreg(c, BC_I8);
    BirEmitirCarrega(c, flag, base, 0);
    falha = BirNovoBloco(c, true);
    ok = BirNovoBloco(c, false);
    BirDesvia(c, OpVreg(flag, BC_I8), falha, ok);
    c->cur = falha;
    RelatarErroDeMain(c, base, erro.offset);
    c->cur = ok;
    BirRetorna(c, &zero);
    return true;
}
