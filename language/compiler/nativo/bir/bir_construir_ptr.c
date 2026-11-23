/* bir_construir_ptr.c - ponteiros na construcao da BIR: &x, p^, gravacao por ponteiro, &p.campo, transmute, faixa de campo e 'as' entre ponteiros.
 * Versao: 0.3.1
 * Changelog: 0.3.1 - O0 P13: ConversaoChecada (nulo) vale tambem no release com --checagem sim (BirConfereLimites).
 *            0.3.0 - IA1-L5a: IR_TRANSMUTA de agregado (str <-> []u8, do indice e da fatia de str) copia o bloco para o slot do temporario (TransmutaAgregado).
 *            0.2.3 - M3 F9b-3: decisoes extraidas de BirRegraEnderecoCampo, BirRegraTransmuta e BirConversaoPonteiro.
 *            0.2.2 - M3 P21v-c: 'as' entre ponteiros cobre proc ('x as proc(...)' de rawptr ou ?^T confere nulo em dev, R0005; os demais pares sao copia).
 *            0.2.1 - M2 P15a: 'as' entre ponteiros cobre cstr (rawptr ou ?^T para cstr confere nulo em dev, como o emit_c; cstr para rawptr e copia).
 *            0.2.0 - M2 P12a: & e p^ de resultado (T ! E) valem como os de struct (BirEhAgregado).
 *            0.1.0 - M2 P11a: OP_ENDERECO (COPIA do endereco do slot do local), OP_DEREF (CARREGA ou COPIA_MEM), IR_GRAVA_PONTEIRO, IR_ENDERECO_CAMPO (ADD), IR_TRANSMUTA
 *                    (mesma classe = COPIA; int <-> real passa por um slot de 8 bytes: GRAVA na classe de origem e CARREGA na do destino), IR_VERIFICA_FAIXA (so dev: CMP
 *                    com min e max em 64 bits, frio chama forja_faixa/forja_faixa_u64 do runtime) e 'as' de ponteiro (rawptr ou ?^T para ^T: dev confere nulo e chama
 *                    forja_falha_nulo; release nao confere).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/struct
 */
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define TEXTO_NAO_NULO "conversao 'as' para ponteiro nao nulo"

/* ---- & e p^ ---- */

bool BirRegraEndereco(BirCtx *c, const IrInstr *i)
{
    uint32_t endereco, dst;

    if (i->a.tipo_no != V_LOCAL)
        return BirFalhaInconsistente(c, "& de algo que nao e local");
    if (!BirVregLocal(c, i->a.id, &endereco))
        return false;
    if (!BirEhAgregado(c->e->tipos, i->a.tipo) && !c->em_memoria[i->a.id])
        return BirFalhaTipo(c, i, "ENDERECO", i->a.tipo);
    if (!BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_COPIA, .dst = dst, .a = OpVreg(endereco, BC_I64)});
    return true;
}

static bool DerefStruct(BirCtx *c, const IrInstr *i, uint32_t ponteiro)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t dst;

    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirCopiaBloco(c, dst, ponteiro, pedido.forma.tam);
    return true;
}

bool BirRegraDeref(BirCtx *c, const IrInstr *i)
{
    BirOperando ponteiro;
    uint8_t classe = BC_NADA;
    uint32_t dst;

    if (!BirOperandoDe(c, i, &i->a, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    if (BirEhAgregado(c->e->tipos, i->tipo))
        return DerefStruct(c, i, ponteiro.id);
    if (!BirClasseDoTipo(c->e->tipos, i->tipo, &classe))
        return BirFalhaTipo(c, i, "DEREF", i->tipo);
    if (!BirDefinirDestino(c, classe, &dst))
        return false;
    BirEmitirCarrega(c, dst, ponteiro.id, 0);
    return true;
}

/* ---- gravacao por ponteiro e endereco de campo ---- */

bool BirRegraGravaPonteiro(BirCtx *c, const IrInstr *i)
{
    const Tipos *tipos = c->e->tipos;
    BirOperando ponteiro;
    BirCampoInfo campo;
    BirAlvoMem alvo;

    if (TipoClasseDe(tipos, i->a.tipo) != TC_PONTEIRO)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!BirOperandoDe(c, i, &i->a, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    alvo = (BirAlvoMem){.base = ponteiro.id, .offset = 0, .tipo = TipoDe(tipos, i->a.tipo)->a};
    if (i->nome >= 0) {
        if (!BirCampoDe(c, alvo.tipo, i->nome, &campo))
            return false;
        alvo.offset = campo.offset;
        alvo.tipo = campo.tipo;
    }
    return BirGravarEm(c, i, &alvo, &i->b);
}

static bool EhPonteiroParaStruct(BirCtx *c, TipoId tipo)
{
    const Tipos *tipos = c->e->tipos;

    return TipoClasseDe(tipos, tipo) == TC_PONTEIRO && BirEhStruct(c, TipoDe(tipos, tipo)->a);
}

/* dst = ponteiro (offset 0: COPIA) ou ponteiro + offset do campo (ADD). */
static void EmitirEnderecoDoCampo(BirCtx *c, uint32_t dst, BirOperando ponteiro, uint32_t offset)
{
    BirPedido pedido = {.op = BIR_COPIA, .dst = dst, .a = OpVreg(ponteiro.id, BC_I64)};

    if (offset != 0) {
        pedido.op = BIR_ADD;
        pedido.b = OpImed(offset, BC_I64);
    }
    BirEmitirPedido(c, pedido);
}

bool BirRegraEnderecoCampo(BirCtx *c, const IrInstr *i)
{
    const Tipos *tipos = c->e->tipos;
    BirOperando ponteiro;
    BirCampoInfo campo;
    uint32_t dst;

    if (!EhPonteiroParaStruct(c, i->a.tipo))
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!BirOperandoDe(c, i, &i->a, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    if (!BirCampoDe(c, TipoDe(tipos, i->a.tipo)->a, i->nome, &campo) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    EmitirEnderecoDoCampo(c, dst, ponteiro, campo.offset);
    return true;
}

/* ---- transmute ---- */

/* A forma do destino; origem e destino precisam ter o mesmo tamanho. */
static bool FormaDaTransmuta(BirCtx *c, const IrInstr *i, BirSlot *forma)
{
    BirSlot forma_origem;

    if (!BirForma(c, i->tipo, forma) || !BirForma(c, i->a.tipo, &forma_origem))
        return false;
    if (forma->tam != forma_origem.tam)
        return BirFalhaInconsistente(c, "transmute entre tamanhos diferentes");
    return true;
}

/* Agregado de mesmo tamanho (str <-> []u8 do indice e da fatia de str, IA1-L5a): o bloco do operando e copiado para o slot do temporario destino. */
static bool TransmutaAgregado(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    BirSlot forma_origem;
    uint32_t origem, dst;

    if (!BirForma(c, i->tipo, &pedido.forma) || !BirForma(c, i->a.tipo, &forma_origem))
        return false;
    if (pedido.forma.tam != forma_origem.tam)
        return BirFalhaInconsistente(c, "transmute entre tamanhos diferentes");
    if (!BirEnderecoDe(c, i, &i->a, &origem) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirCopiaBloco(c, dst, origem, pedido.forma.tam);
    return true;
}

bool BirRegraTransmuta(BirCtx *c, const IrInstr *i)
{
    BirOperando origem;
    BirSlot forma;
    uint8_t classe = BC_NADA;
    uint32_t dst, endereco;

    if (BirEhAgregado(c->e->tipos, i->tipo))
        return TransmutaAgregado(c, i);
    if (!BirOperandoDe(c, i, &i->a, &origem))
        return false;
    if (!BirClasseDoTipo(c->e->tipos, i->tipo, &classe))
        return BirFalhaTipo(c, i, "-", i->tipo);
    if (!FormaDaTransmuta(c, i, &forma))
        return false;
    if (!BirDefinirDestino(c, classe, &dst))
        return false;
    if (origem.classe == classe) {
        BirMover(c, dst, origem);
        return true;
    }
    endereco = BirEmitirEndSlot(c, BirNovoSlot(c, forma.tam, forma.alinhamento), BirNovoVreg(c, BC_I64));
    BirEmitirGrava(c, endereco, 0, origem);
    BirEmitirCarrega(c, dst, endereco, 0);
    return true;
}

/* ---- faixa de campo ---- */

static BirOperando Alargar64(BirCtx *c, BirOperando v, TipoId tipo)
{
    if (v.classe == BC_I64)
        return v;
    return OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = TipoEhSemSinal(tipo) ? BIR_EXT_Z : BIR_EXT_S, .a = v}), BC_I64);
}

/* vreg bool: v < min ou v > max (v >= max na faixa exclusiva), com a ordem do TIPO (sem sinal compara em 64 bits sem sinal). */
static uint32_t CondicaoFora(BirCtx *c, const IrInstr *i, BirOperando v)
{
    bool sem_sinal = TipoEhSemSinal(i->a.tipo);
    uint8_t menor = sem_sinal ? BCOND_LT_U : BCOND_LT_S;
    uint8_t maior = i->faixa_excl ? (sem_sinal ? BCOND_GE_U : BCOND_GE_S) : (sem_sinal ? BCOND_GT_U : BCOND_GT_S);
    uint32_t baixo = BirComparar(c, menor, v, OpImed(i->faixa_min, BC_I64));
    uint32_t alto = BirComparar(c, maior, v, OpImed(i->faixa_max, BC_I64));

    return BirContaNova(c, BC_I8, (BirPedido){.op = BIR_OR, .a = OpVreg(baixo, BC_I8), .b = OpVreg(alto, BC_I8)});
}

/* forja_faixa[_u64](valor, min, max, exclusiva, campo, arquivo, linha) no frio: o runtime refaz a conta e falha com R0001. */
static void FrioFaixa(BirCtx *c, const IrInstr *i, uint32_t frio, BirOperando v)
{
    const char *campo = InternerTexto(c->e->nomes, i->nome);
    BirOperando args[7];
    uint32_t salvo = c->cur, linha;

    c->cur = frio;
    args[0] = v;
    args[1] = OpImed(i->faixa_min, BC_I64);
    args[2] = OpImed(i->faixa_max, BC_I64);
    args[3] = OpImed(i->faixa_excl ? 1 : 0, BC_I8);
    args[4] = OpVreg(BirVregDoTexto(c, campo, InternerTam(c->e->nomes, i->nome)), BC_I64);
    args[5] = BirVregDoArquivo(c, i->pos, &linha);
    args[6] = OpImed(linha, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, TipoEhSemSinal(i->a.tipo) ? "forja_faixa_u64" : "forja_faixa"), args, 7);
    c->cur = salvo;
}

bool BirRegraVerificaFaixa(BirCtx *c, const IrInstr *i)
{
    BirOperando v;
    uint32_t fora, frio, segue;

    if (c->p->modo != BM_DEV)
        return true; /* release: a faixa nao e conferida */
    if (!BirOperandoDe(c, i, &i->a, &v) || !BirOperandoVreg(c, v, &v))
        return false;
    v = Alargar64(c, v, i->a.tipo);
    fora = CondicaoFora(c, i, v);
    frio = BirNovoBloco(c, true);
    segue = BirNovoBloco(c, false);
    BirDesvia(c, OpVreg(fora, BC_I8), frio, segue);
    FrioFaixa(c, i, frio, v);
    c->cur = segue;
    return true;
}

/* ---- 'as' entre ponteiros ---- */

static bool EhPonteiro(const BirCtx *c, TipoId tipo)
{
    TipoClasse k = TipoClasseDe(c->e->tipos, tipo);

    return k == TC_PONTEIRO || k == TC_ANULAVEL || k == TC_RAWPTR || k == TC_CSTR || k == TC_PROC;
}

bool BirEhConversaoPonteiro(const BirCtx *c, const IrInstr *i)
{
    return EhPonteiro(c, i->a.tipo) && EhPonteiro(c, i->tipo);
}

/* Dev, rawptr ou ?^T para ^T: se o ponteiro e nulo o frio chama forja_falha_nulo(texto, arquivo, linha) (R0005). */
static void GuardaNaoNulo(BirCtx *c, const IrInstr *i, BirOperando ponteiro)
{
    uint32_t nulo = BirComparar(c, BCOND_EQ, ponteiro, OpImed(0, BC_I64));
    uint32_t frio = BirNovoBloco(c, true);
    uint32_t segue = BirNovoBloco(c, false);
    BirOperando args[3];
    uint32_t linha;

    BirDesvia(c, OpVreg(nulo, BC_I8), frio, segue);
    c->cur = frio;
    args[0] = OpVreg(BirVregDoTexto(c, TEXTO_NAO_NULO, sizeof TEXTO_NAO_NULO - 1), BC_I64);
    args[1] = BirVregDoArquivo(c, i->pos, &linha);
    args[2] = OpImed(linha, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, "forja_falha_nulo"), args, 3);
    c->cur = segue;
}

/* Nulo so confere ao ir para ^T, cstr ou proc. */
static bool DestinoNaoNulo(TipoClasse destino)
{
    switch (destino) {
    case TC_PONTEIRO: case TC_CSTR: case TC_PROC:
        return true;
    default:
        return false;
    }
}

static bool OrigemPodeSerNula(TipoClasse origem)
{
    return origem == TC_RAWPTR || origem == TC_ANULAVEL;
}

/* Dev, rawptr ou ?^T para ^T, cstr ou proc: o nulo e conferido. */
static bool ConversaoChecada(const BirCtx *c, const IrInstr *i)
{
    return BirConfereLimites(c) && DestinoNaoNulo(TipoClasseDe(c->e->tipos, i->tipo)) && OrigemPodeSerNula(TipoClasseDe(c->e->tipos, i->a.tipo));
}

bool BirConversaoPonteiro(BirCtx *c, const IrInstr *i)
{
    bool checada = ConversaoChecada(c, i);
    BirOperando ponteiro;
    uint32_t dst;

    if (!BirOperandoDe(c, i, &i->a, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    if (checada)
        GuardaNaoNulo(c, i, ponteiro);
    if (!BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_COPIA, .dst = dst, .a = ponteiro});
    return true;
}
