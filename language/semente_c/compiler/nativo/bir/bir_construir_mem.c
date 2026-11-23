/* bir_construir_mem.c - memoria na construcao da BIR: slots, struct (local, literal, campo), escalar com '&' e a copia de bloco.
 * Versao: 0.3.8
 * Changelog: 0.3.8 - M5b P39-B1 (D0654): memcpy e memset saem de BirPrograma.nomes_mem (linux: forja_memcpy e forja_memset), no lugar do texto fixo.
 *            0.3.7 - IA1-L5b: BirZeraDinamico e BirCopiaDinamica (memset/memcpy do runtime com o tamanho em operando).
 *            0.3.6 - M3 F9b-2: decisoes extraidas de BirEhAgregado, MarcarEnderecos, BirDefinirSlot, BirEnderecoDe e BirRegraStruct.
 *            0.3.5 - M2 P17b: BirEhAgregado cobre soa (TC_SOA: fixa = struct de colunas f_<campo>[N]; dinamica = {col[ncol], len, cap, alocador, epoca}) e map (TC_MAPA: forja_mapa de 56 B).
 *            0.3.4 - M2 P16: BirEhAgregado cobre matrix (TC_MATRIZ: R*C elementos guardados por colunas, membro m no offset 0 do layout/).
 *            0.3.3 - M2 P15a: BirEhAgregado cobre ?T de valor (TC_OPCIONAL fora do nicho), union e tupla; BirEnderecoDe aceita o none de ?T (V_NULO, slot zerado: BirNuloAgregado).
 *            0.3.2 - M2 P14b: BirEhAgregado cobre [dynamic]T (TC_DINAMICO, 32 B: {ptr, len, cap, alocador, epoca}); o resto igual.
 *            0.3.1 - M2 P13b: BirEhAgregado cobre [N]T (TC_ARRAY) e []T (TC_FATIA); o resto do arquivo segue igual (local, atribuicao, copia e endereco valem sem mudanca).
 *            0.3.0 - M2 P13a: o str (TC_TEXTO) e agregado de 16 B (BirEhAgregado); BirEnderecoDe aceita o literal de str (BirTextoLiteral).
 *            0.2.0 - M2 P12a: BirEhAgregado (struct ou resultado T ! E) no lugar de BirEhStruct em local, atribuicao, copia, endereco e campo de agregado; BirBaseDoCampo segue so com struct.
 *            0.1.0 - M2 P11a: agregado = slot + vreg I64 com o endereco (END_SLOT a cada definicao, porque o vreg nao e SSA); IR_LOCAL, IR_ATRIBUICAO, IR_CAMPO e
 *                    IR_STRUCT sobre struct; local escalar com '&' (pre-passo por funcao) vive num slot e todo acesso e CARREGA/GRAVA. Copia ate 64 bytes = COPIA_MEM;
 *                    acima, memcpy/memset do runtime. Tamanhos e offsets vem do layout/ (nada escrito a mao aqui).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/struct
 *
 * Semantica (ir.h 0.14.0): struct e valor, a atribuicao copia (nunca alias); IR_STRUCT escreve campo a campo no slot do temporario e deixa o padding sem definir;
 * IR_LOCAL de struct sem valor zera o slot. Struct como parametro, retorno ou argumento de proc e do P11b: BirOperandoDe segue recusando (X0001).
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/bir_interno.h"

/* ---- layout ---- */

const Layouts *BirLayouts(BirCtx *c)
{
    if (!c->lay_pronto) {
        LayoutCalcular(c->e->tipos, c->e->ir, &c->p->arena, &c->lay);
        c->lay_pronto = true;
    }
    return &c->lay;
}

bool BirEhStruct(const BirCtx *c, TipoId tipo)
{
    return TipoClasseDe(c->e->tipos, tipo) == TC_STRUCT;
}

/* Agregado por valor: vive num slot e viaja como endereco. O resultado T ! E e uma struct {e_erro, valor, erro} no layout/; o str e a fatia []T sao {ptr, len} (16 B); o [N]T e o bloco de N elementos; o [dynamic]T e {ptr, len, cap, alocador, epoca} (32 B);
 * ?T e {tem, v}, a union {tag, variantes} e a tupla a struct dos elementos. */
bool BirEhAgregado(const Tipos *tipos, TipoId tipo)
{
    TipoClasse classe = TipoClasseDe(tipos, tipo);

    if (classe == TC_OPCIONAL)
        return !BirEhPonteiroOpcional(tipos, tipo); /* ?T de valor = {tem, v}; ?cstr e o ponteiro */
    switch (classe) {
    case TC_STRUCT: case TC_RESULTADO: case TC_TEXTO: case TC_ARRAY: case TC_FATIA: case TC_DINAMICO:
    case TC_UNIAO: case TC_TUPLA: case TC_MATRIZ: case TC_SOA: case TC_MAPA: return true;
    default: return false;
    }
}

/* Tamanho e alinhamento do layout/; um tipo sem forma em C (tam 0) vira X0904 em vez de FalhaInterna. */
bool BirForma(BirCtx *c, TipoId tipo, BirSlot *forma)
{
    const Layouts *l = BirLayouts(c);

    if (tipo >= l->qtd || l->tipos[tipo].tam == 0)
        return BirFalhaInconsistente(c, "tipo sem forma em C no layout");
    *forma = (BirSlot){.tam = l->tipos[tipo].tam, .alinhamento = l->tipos[tipo].ali};
    return true;
}

bool BirCampoDe(BirCtx *c, TipoId tipo_struct, int32_t nome, BirCampoInfo *info)
{
    const IrStruct *s = LayoutEstrutura(c->e->tipos, c->e->ir, tipo_struct);

    if (s == NULL)
        return BirFalhaInconsistente(c, "struct sem definicao na IR");
    for (uint32_t k = 0; k < s->ncampos; k++) {
        if (s->campos[k].nome != nome)
            continue;
        *info = (BirCampoInfo){.offset = LayoutOffsetCampo(BirLayouts(c), tipo_struct, k), .tipo = s->campos[k].tipo};
        return true;
    }
    return BirFalhaInconsistente(c, "campo inexistente na struct");
}

/* ---- mapas por funcao ---- */

static int32_t *MapaDeSlots(int32_t *mapa, uint32_t n)
{
    mapa = realloc(mapa, (n + 1u) * sizeof *mapa);
    if (mapa == NULL)
        FalhaInterna("bir: memoria esgotada nos mapas de slot");
    for (uint32_t k = 0; k < n; k++)
        mapa[k] = -1;
    return mapa;
}

static bool EhEnderecoDeLocal(const IrInstr *i)
{
    return i->op == IR_UNARIO && i->op_aux == OP_ENDERECO && i->a.tipo_no == V_LOCAL;
}

/* Locais com '&': varre a funcao inteira (blocos filhos e casos) uma vez antes de construir. */
static void MarcarEnderecos(BirCtx *c, const IrBloco *bloco)
{
    for (const IrInstr *i = bloco->primeira; i != NULL; i = i->prox) {
        if (EhEnderecoDeLocal(i) && i->a.id >= 0 && (uint32_t)i->a.id < c->n_local)
            c->em_memoria[i->a.id] = 1;
        MarcarEnderecos(c, &i->cond);
        MarcarEnderecos(c, &i->corpo);
        MarcarEnderecos(c, &i->senao);
        for (uint32_t k = 0; k < i->ncasos; k++)
            MarcarEnderecos(c, &i->casos[k].corpo);
    }
}

void BirMemPrepararFuncao(BirCtx *c, const IrFuncao *irf)
{
    c->slot_local = MapaDeSlots(c->slot_local, irf->nlocais);
    c->slot_temp = MapaDeSlots(c->slot_temp, irf->ntemps);
    free(c->tipo_local);
    free(c->em_memoria);
    c->tipo_local = calloc(irf->nlocais + 1u, sizeof *c->tipo_local);
    c->em_memoria = calloc(irf->nlocais + 1u, sizeof *c->em_memoria);
    if (c->tipo_local == NULL || c->em_memoria == NULL)
        FalhaInterna("bir: memoria esgotada nos tipos de local");
    for (uint32_t k = 0; k < irf->nparams && k < irf->nlocais; k++)
        c->tipo_local[k] = irf->params[k].tipo;
    MarcarEnderecos(c, &irf->corpo);
}

void BirMemLiberar(BirCtx *c)
{
    free(c->slot_local);
    free(c->slot_temp);
    free(c->tipo_local);
    free(c->em_memoria);
}

void BirRegistrarTipoLocal(BirCtx *c, int32_t local, TipoId tipo)
{
    if (local >= 0 && (uint32_t)local < c->n_local)
        c->tipo_local[local] = tipo;
}

/* ---- instrucoes de memoria ---- */

uint32_t BirEmitirEndSlot(BirCtx *c, uint32_t slot, uint32_t dst)
{
    BirInstr *m = BirEmitir(c, BIR_END_SLOT, BC_I64);

    m->dst = dst;
    m->a = OpImed(slot, BC_I64);
    return dst;
}

void BirEmitirCarrega(BirCtx *c, uint32_t dst, uint32_t base, int32_t desloc)
{
    BirInstr *m = BirEmitir(c, BIR_CARREGA, c->f->classe_vreg[dst]);

    m->dst = dst;
    m->a = OpVreg(base, BC_I64);
    m->desloc = desloc;
}

void BirEmitirGrava(BirCtx *c, uint32_t base, int32_t desloc, BirOperando valor)
{
    BirInstr *m = BirEmitir(c, BIR_GRAVA, valor.classe);

    m->a = OpVreg(base, BC_I64);
    m->b = valor;
    m->desloc = desloc;
}

/* memcpy/memset do runtime: destino, origem (ou 0) e n bytes; o resultado fica descartado. */
static void ChamarBloco(BirCtx *c, const char *funcao, const BirOperando args[3])
{
    BirOperando *copia = BirArgs(c, 3);
    BirInstr *chama;

    memcpy(copia, args, 3 * sizeof *copia);
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, funcao);
    chama->args = copia;
    chama->nargs = 3;
}

void BirCopiaBloco(BirCtx *c, uint32_t destino, uint32_t origem, uint32_t tam)
{
    BirInstr *m;

    if (tam > BIR_COPIA_MEM_MAX) {
        ChamarBloco(c, c->p->nomes_mem.copia, (BirOperando[3]){OpVreg(destino, BC_I64), OpVreg(origem, BC_I64), OpImed(tam, BC_I64)});
        return;
    }
    m = BirEmitir(c, BIR_COPIA_MEM, BC_NADA);
    m->a = OpVreg(destino, BC_I64);
    m->b = OpVreg(origem, BC_I64);
    m->desloc = (int32_t)tam;
}

void BirZeraBloco(BirCtx *c, uint32_t destino, uint32_t tam)
{
    BirInstr *m;

    if (tam > BIR_COPIA_MEM_MAX) {
        ChamarBloco(c, c->p->nomes_mem.zera, (BirOperando[3]){OpVreg(destino, BC_I64), OpImed(0, BC_I32), OpImed(tam, BC_I64)});
        return;
    }
    m = BirEmitir(c, BIR_ZERA_MEM, BC_NADA);
    m->a = OpVreg(destino, BC_I64);
    m->desloc = (int32_t)tam;
}

/* IA1-L5b: memset/memcpy do runtime com o numero de bytes so conhecido na execucao (resize e append de fatia). */
void BirZeraDinamico(BirCtx *c, uint32_t destino, BirOperando bytes)
{
    ChamarBloco(c, c->p->nomes_mem.zera, (BirOperando[3]){OpVreg(destino, BC_I64), OpImed(0, BC_I32), bytes});
}

void BirCopiaDinamica(BirCtx *c, uint32_t destino, uint32_t origem, BirOperando bytes)
{
    ChamarBloco(c, c->p->nomes_mem.copia, (BirOperando[3]){OpVreg(destino, BC_I64), OpVreg(origem, BC_I64), bytes});
}

/* base + off num vreg; off 0 devolve a propria base. */
uint32_t BirEnderecoMais(BirCtx *c, uint32_t base, uint32_t off)
{
    if (off == 0)
        return base;
    return BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(base, BC_I64), .b = OpImed(off, BC_I64)});
}

/* ---- agregados e escalares em slot ---- */

/* Tabelas por numero do agregado: as de temporarios ou as de locais. */
typedef struct { int32_t *vregs, *slots; uint32_t n; } TabelasDoSlot;

static TabelasDoSlot TabelasDoPedido(const BirCtx *c, const BirPedidoSlot *pedido)
{
    if (pedido->temporario)
        return (TabelasDoSlot){.vregs = c->vreg_temp, .slots = c->slot_temp, .n = c->n_temp};
    return (TabelasDoSlot){.vregs = c->vreg_local, .slots = c->slot_local, .n = c->n_local};
}

/* O numero ja tem slot, mas de outro tamanho (ou sem slot)? */
static bool SlotRedefinido(const BirCtx *c, int32_t slot, uint32_t tam)
{
    return slot < 0 || c->f->slots[slot].tam != tam;
}

/* O vreg I64 do slot de um local ou temporario: cria vreg e slot na 1a definicao e emite END_SLOT em TODA definicao. */
bool BirDefinirSlot(BirCtx *c, const BirPedidoSlot *pedido, uint32_t *vreg)
{
    TabelasDoSlot t = TabelasDoPedido(c, pedido);
    int32_t id = pedido->id;

    if (id < 0 || (uint32_t)id >= t.n)
        return BirFalhaInconsistente(c, "numero de agregado fora da funcao");
    if (t.vregs[id] < 0) {
        t.vregs[id] = (int32_t)BirNovoVreg(c, BC_I64);
        t.slots[id] = (int32_t)BirNovoSlot(c, pedido->forma.tam, pedido->forma.alinhamento);
    } else if (SlotRedefinido(c, t.slots[id], pedido->forma.tam)) {
        return BirFalhaInconsistente(c, "numero redefinido com outro agregado");
    }
    *vreg = (uint32_t)t.vregs[id];
    BirEmitirEndSlot(c, (uint32_t)t.slots[id], *vreg);
    return true;
}

static bool EhNomeAgregado(const BirCtx *c, const Valor *v)
{
    return (v->tipo_no == V_LOCAL || v->tipo_no == V_TEMP) && BirEhAgregado(c->e->tipos, v->tipo);
}

static bool EnderecoDoNome(BirCtx *c, const Valor *v, uint32_t *endereco)
{
    return v->tipo_no == V_TEMP ? BirVregTemp(c, v->id, endereco) : BirVregLocal(c, v->id, endereco);
}

bool BirEnderecoDe(BirCtx *c, const IrInstr *i, const Valor *v, uint32_t *endereco)
{
    if (v->tipo_no == V_TEXTO && v->tipo == TIPO_STR)
        return BirTextoLiteral(c, v, endereco);
    if (v->tipo_no == V_NULO && BirEhAgregado(c->e->tipos, v->tipo)) /* none de ?T */
        return BirNuloAgregado(c, v->tipo, endereco);
    if (!EhNomeAgregado(c, v))
        return BirFalhaTipo(c, i, BirVarianteDe(c, i), v->tipo);
    return EnderecoDoNome(c, v, endereco);
}

bool BirLerLocalEmMemoria(BirCtx *c, const Valor *v, BirOperando *saida)
{
    uint8_t classe = BC_NADA;
    uint32_t endereco, dst;

    if (!BirVregLocal(c, v->id, &endereco))
        return false;
    BirClasseDoTipo(c->e->tipos, v->tipo, &classe);
    dst = BirNovoVreg(c, classe);
    BirEmitirCarrega(c, dst, endereco, 0);
    *saida = OpVreg(dst, classe);
    return true;
}

/* A base do acesso a campo: struct por valor (endereco do slot) ou ^Struct (o ponteiro). tipo_struct = a struct acessada. */
bool BirBaseDoCampo(BirCtx *c, const IrInstr *i, const Valor *base, BirBaseCampo *saida)
{
    const Tipos *tipos = c->e->tipos;
    BirOperando ponteiro;

    if (BirEhStruct(c, base->tipo)) {
        saida->tipo_struct = base->tipo;
        return BirEnderecoDe(c, i, base, &saida->endereco);
    }
    if (TipoClasseDe(tipos, base->tipo) != TC_PONTEIRO || !BirEhStruct(c, TipoDe(tipos, base->tipo)->a))
        return BirFalhaTipo(c, i, "-", base->tipo);
    saida->tipo_struct = TipoDe(tipos, base->tipo)->a;
    if (!BirOperandoDe(c, i, base, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    saida->endereco = ponteiro.id;
    return true;
}

/* Grava o valor no alvo: struct = COPIA_MEM para base+offset, escalar = GRAVA. */
bool BirGravarEm(BirCtx *c, const IrInstr *i, const BirAlvoMem *alvo, const Valor *valor)
{
    BirOperando operando;
    BirSlot forma;
    uint32_t origem;

    if (BirEhAgregado(c->e->tipos, alvo->tipo)) {
        if (!BirForma(c, alvo->tipo, &forma) || !BirEnderecoDe(c, i, valor, &origem))
            return false;
        BirCopiaBloco(c, BirEnderecoMais(c, alvo->base, alvo->offset), origem, forma.tam);
        return true;
    }
    if (!BirOperandoDe(c, i, valor, &operando))
        return false;
    BirEmitirGrava(c, alvo->base, (int32_t)alvo->offset, operando);
    return true;
}

/* ---- IR_LOCAL ---- */

bool BirLocalEmMemoria(const BirCtx *c, const IrInstr *i)
{
    uint8_t classe;

    if (BirEhAgregado(c->e->tipos, i->tipo))
        return true;
    if (i->destino < 0 || (uint32_t)i->destino >= c->n_local || !c->em_memoria[i->destino])
        return false;
    return BirClasseDoTipo(c->e->tipos, i->tipo, &classe);
}

static bool LocalAgregado(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.id = i->destino};
    uint32_t dst, origem;

    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    if (i->a.tipo_no == V_NENHUM) {
        BirZeraBloco(c, dst, pedido.forma.tam);
        return true;
    }
    if (!BirEnderecoDe(c, i, &i->a, &origem))
        return false;
    BirCopiaBloco(c, dst, origem, pedido.forma.tam);
    return true;
}

static bool LocalEscalarEmMemoria(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.id = i->destino};
    uint8_t classe = BC_NADA;
    BirOperando valor;
    uint32_t endereco;

    BirClasseDoTipo(c->e->tipos, i->tipo, &classe);
    valor = OpImed(0, classe); /* a = V_NENHUM: o local nasce zerado */
    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &endereco))
        return false;
    if (i->a.tipo_no != V_NENHUM && !BirOperandoDe(c, i, &i->a, &valor))
        return false;
    BirEmitirGrava(c, endereco, 0, valor);
    return true;
}

bool BirRegraLocalMem(BirCtx *c, const IrInstr *i)
{
    return BirEhAgregado(c->e->tipos, i->tipo) ? LocalAgregado(c, i) : LocalEscalarEmMemoria(c, i);
}

/* ---- IR_ATRIBUICAO ---- */

bool BirAtribuicaoEmMemoria(const BirCtx *c, const IrInstr *i)
{
    if (i->nome >= 0)
        return true;
    if (i->destino < 0 || (uint32_t)i->destino >= c->n_local)
        return false;
    return c->em_memoria[i->destino] || BirEhAgregado(c->e->tipos, c->tipo_local[i->destino]);
}

static bool AtribuiCampo(BirCtx *c, const IrInstr *i)
{
    Valor local = {.tipo_no = V_LOCAL, .id = i->destino, .tipo = c->tipo_local[i->destino]};
    BirBaseCampo base;
    BirCampoInfo campo;

    if (!BirBaseDoCampo(c, i, &local, &base) || !BirCampoDe(c, base.tipo_struct, i->nome, &campo))
        return false;
    return BirGravarEm(c, i, &(BirAlvoMem){.base = base.endereco, .offset = campo.offset, .tipo = campo.tipo}, &i->a);
}

static bool AtribuiStruct(BirCtx *c, const IrInstr *i)
{
    TipoId tipo = c->tipo_local[i->destino];
    BirSlot forma;
    uint32_t dst, origem;

    if (!BirForma(c, tipo, &forma) || !BirVregLocal(c, i->destino, &dst) || !BirEnderecoDe(c, i, &i->a, &origem))
        return false;
    BirCopiaBloco(c, dst, origem, forma.tam);
    return true;
}

static bool AtribuiEscalarEmMemoria(BirCtx *c, const IrInstr *i)
{
    BirOperando valor;
    uint32_t endereco;

    if (!BirVregLocal(c, i->destino, &endereco) || !BirOperandoDe(c, i, &i->a, &valor))
        return false;
    BirEmitirGrava(c, endereco, 0, valor);
    return true;
}

bool BirRegraAtribuicaoMem(BirCtx *c, const IrInstr *i)
{
    if (i->nome >= 0)
        return AtribuiCampo(c, i);
    return BirEhAgregado(c->e->tipos, c->tipo_local[i->destino]) ? AtribuiStruct(c, i) : AtribuiEscalarEmMemoria(c, i);
}

/* ---- IR_CAMPO e IR_STRUCT ---- */

static bool CampoAgregado(BirCtx *c, const IrInstr *i, uint32_t endereco, const BirCampoInfo *campo)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t dst;

    if (!BirForma(c, campo->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirCopiaBloco(c, dst, BirEnderecoMais(c, endereco, campo->offset), pedido.forma.tam);
    return true;
}

bool BirRegraCampo(BirCtx *c, const IrInstr *i)
{
    BirBaseCampo base;
    BirCampoInfo campo;
    uint8_t classe = BC_NADA;
    uint32_t dst;

    if (!BirBaseDoCampo(c, i, &i->a, &base) || !BirCampoDe(c, base.tipo_struct, i->nome, &campo))
        return false;
    if (BirEhAgregado(c->e->tipos, campo.tipo))
        return CampoAgregado(c, i, base.endereco, &campo);
    if (!BirClasseDoTipo(c->e->tipos, campo.tipo, &classe))
        return BirFalhaTipo(c, i, "-", campo.tipo);
    if (!BirDefinirDestino(c, classe, &dst))
        return false;
    BirEmitirCarrega(c, dst, base.endereco, (int32_t)campo.offset);
    return true;
}

/* Escreve cada valor de IR_STRUCT no seu campo do slot dst. */
static bool GravarCamposDaStruct(BirCtx *c, const IrInstr *i, const IrStruct *s, uint32_t dst)
{
    for (uint32_t k = 0; k < s->ncampos; k++) {
        BirAlvoMem alvo = {.base = dst, .offset = LayoutOffsetCampo(BirLayouts(c), i->tipo, k), .tipo = s->campos[k].tipo};

        if (!BirGravarEm(c, i, &alvo, &i->args[k]))
            return false;
    }
    return true;
}

bool BirRegraStruct(BirCtx *c, const IrInstr *i)
{
    const IrStruct *s = LayoutEstrutura(c->e->tipos, c->e->ir, i->tipo);
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t dst;

    if (s == NULL || i->nargs != s->ncampos)
        return BirFalhaInconsistente(c, "IR_STRUCT com valores diferentes dos campos");
    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    if (s->ncampos == 0) {
        BirZeraBloco(c, dst, pedido.forma.tam);
        return true;
    }
    return GravarCamposDaStruct(c, i, s, dst);
}
