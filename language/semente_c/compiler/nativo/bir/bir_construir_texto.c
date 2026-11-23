/* bir_construir_texto.c - texto na construcao da BIR: literal de str e de cstr, print de str, == e != de str, 'as' de cstr para str e len de str.
 * Versao: 0.1.5
 * Changelog: 0.1.5 - O0 P13: cstr as str com nulo no release com --checagem sim: R0005 por guarda inline (BirFalhaSeNulo) antes do forja_str_de_cstr.
 *            0.1.4 - M3 F9b-3: decisoes extraidas de BirRegraStrIgual.
 *            0.1.3 - M2 P17b: BirRegraTamanhoTexto desvia map e soa dinamica (e a lista) para BirTamanhoDeColecao (bir_construir_soa_mapa.c).
 *            0.1.2 - M2 P14b: BirRegraTamanhoTexto desvia a lista ([dynamic]T e ^[dynamic]T) para BirTamanhoDeLista (bir_construir_array.c).
 *            0.1.1 - M2 P13b: BirRegraTamanhoTexto tambem le o len da fatia []T (mesmo layout {ptr, len} do str).
 *            0.1.0 - M2 P13a: str e um agregado de 16 B {ptr, len} (BirEhAgregado) e viaja como os outros agregados. Literal de str = slot com o endereco do texto (END_SIMBOLO, nunca
 *                    ponteiro absoluto em .rdata: nada de ADDR64) e o len em bytes (2 GRAVA); literal de cstr = END_SIMBOLO num vreg I64. Os servicos do runtime nativo
 *                    (forja_print_str, forja_str_igual, forja_str_de_cstr) recebem o str por valor, como qualquer agregado (copia privada + endereco).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/texto
 *
 * Offsets de ptr e len vem do layout/ (LayoutOffsetCampo de TIPO_STR), o mesmo que nativo_layout.c confere contra forja_rt_layout do runtime.
 */
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define STR_CAMPO_PTR 0u
#define STR_CAMPO_LEN 1u

static uint32_t OffsetDoCampoStr(BirCtx *c, uint32_t campo)
{
    return LayoutOffsetCampo(BirLayouts(c), TIPO_STR, campo);
}

/* CHAMA a uma funcao do runtime sem retorno em registrador (BC_NADA: nada, ou agregado pelo CHAMA.a); quem devolve valor ajusta a classe depois. */
static BirInstr *ChamarRuntime(BirCtx *c, const char *funcao, BirOperando *args, uint32_t nargs)
{
    BirInstr *chama = BirEmitir(c, BIR_CHAMA, BC_NADA);

    chama->simbolo = BirSimboloRuntime(c, funcao);
    chama->args = args;
    chama->nargs = nargs;
    return chama;
}

bool BirTextoLiteral(BirCtx *c, const Valor *v, uint32_t *endereco)
{
    const char *texto = InternerTexto(c->e->nomes, v->id);
    uint32_t tam = InternerTam(c->e->nomes, v->id);
    BirSlot forma;
    uint32_t base, ptr;

    if (!BirForma(c, TIPO_STR, &forma))
        return false;
    base = BirEmitirEndSlot(c, BirNovoSlot(c, forma.tam, forma.alinhamento), BirNovoVreg(c, BC_I64));
    ptr = BirVregDoTexto(c, texto, tam);
    BirEmitirGrava(c, base, (int32_t)OffsetDoCampoStr(c, STR_CAMPO_PTR), OpVreg(ptr, BC_I64));
    BirEmitirGrava(c, base, (int32_t)OffsetDoCampoStr(c, STR_CAMPO_LEN), OpImed(tam, BC_I64));
    *endereco = base;
    return true;
}

bool BirTextoCstr(BirCtx *c, const Valor *v, BirOperando *saida)
{
    uint32_t vreg = BirVregDoTexto(c, InternerTexto(c->e->nomes, v->id), InternerTam(c->e->nomes, v->id));

    *saida = OpVreg(vreg, BC_I64);
    return true;
}

bool BirImprimeStr(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 1);

    if (!BirArgumentoAgregado(c, i, &i->a, &args[0]))
        return false;
    ChamarRuntime(c, "forja_print_str", args, 1);
    return true;
}

/* A chamada ao runtime: == escreve direto em dst; != escreve num vreg novo e inverte com XOR #1. */
static void ChamarStrIgual(BirCtx *c, const IrInstr *i, BirOperando *args, uint32_t dst)
{
    uint32_t igual = i->op_aux == OP_IGUAL ? dst : BirNovoVreg(c, BC_I8);
    BirInstr *chama = ChamarRuntime(c, "forja_str_igual", args, 2);

    chama->classe = BC_I8;
    chama->dst = igual;
    if (igual != dst)
        BirEmitirPedido(c, (BirPedido){.op = BIR_XOR, .dst = dst, .a = OpVreg(igual, BC_I8), .b = OpImed(1, BC_I8)});
}

/* forja_str_igual(a, b) -> bool (mesmo len e mesmos bytes, nos dois modos); != e o mesmo resultado com XOR #1. */
bool BirRegraStrIgual(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 2);
    uint32_t dst;

    if (i->op_aux != OP_IGUAL && i->op_aux != OP_DIFERENTE)
        return BirFalhaTipo(c, i, BirVarianteDe(c, i), i->a.tipo);
    if (!BirArgumentoAgregado(c, i, &i->a, &args[0]) || !BirArgumentoAgregado(c, i, &i->b, &args[1]))
        return false;
    if (!BirDefinirDestino(c, BC_I8, &dst))
        return false;
    ChamarStrIgual(c, i, args, dst);
    return true;
}

/* forja_str_de_cstr(c, arquivo, linha): o str sai no slot do temporario de destino (retorno agregado pelo CHAMA.a). Dev: nulo e R0005; release: str vazia. */
bool BirCstrParaStr(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    BirOperando *args = BirArgs(c, 3);
    uint32_t destino, linha;
    const char *arquivo;
    BirInstr *chama;

    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_CSTR)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!BirOperandoDe(c, i, &i->a, &args[0]) || !BirOperandoVreg(c, args[0], &args[0]))
        return false;
    if (!BirForma(c, TIPO_STR, &pedido.forma) || !BirDefinirSlot(c, &pedido, &destino))
        return false;
    if (c->p->modo != BM_DEV && BirConfereLimites(c)) { /* release com --checagem sim: o runtime release da str vazia, entao o R0005 sai aqui, antes da chamada */
        BirArquivoELinha(c, i->pos, &arquivo, &linha);
        BirFalhaSeNulo(c, args[0].id, &(BirOrigemNulo){"conversao 'as' de cstr para str", arquivo, linha});
    }
    args[1] = BirVregDoArquivo(c, i->pos, &linha);
    args[2] = OpImed(linha, BC_I32);
    chama = ChamarRuntime(c, "forja_str_de_cstr", args, 3);
    chama->a = OpVreg(destino, BC_I64);
    chama->a.tam = pedido.forma.tam;
    return true;
}

/* len(s) de str ou de fatia []T (as duas sao {ptr, len}): o campo len do slot. Lista ([dynamic]T) e do P14b. */
bool BirRegraTamanhoTexto(BirCtx *c, const IrInstr *i)
{
    uint32_t endereco, dst;

    if (i->a.tipo != TIPO_STR && TipoClasseDe(c->e->tipos, i->a.tipo) != TC_FATIA)
        return BirTamanhoDeColecao(c, i); /* map, soa dinamica e [dynamic]T, por valor ou ^ (senao X0001 pelo tipo) */
    if (!BirEnderecoDe(c, i, &i->a, &endereco) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirCarrega(c, dst, endereco, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, STR_CAMPO_LEN));
    return true;
}
