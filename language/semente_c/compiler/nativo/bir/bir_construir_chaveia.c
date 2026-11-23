/* bir_construir_chaveia.c - IR -> BIR: IR_CHAVEIA (switch) de enum e de resultado, e o valor C do membro de enum.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - M3 F9b-3: decisoes extraidas de PrepararDiscriminante, LigarCaso e BirRegraChaveia.
 *            0.2.0 - M2 P15a: CHAVEIA de ?T (discriminante = membro tem; 1 valor, 0 none), ?^T e ?cstr (discriminante = ponteiro != 0, a ligacao recebe o ponteiro) e union (discriminante = tag u32;
 *                    a ligacao recebe a variante do caso); LigarParte reaproveita o caminho do resultado. O default e o none nao ligam nada.
 *            0.1.0 - M2 P12a: primeira versao. Cadeia CMP + DESVIA na ordem dos casos (o degrau 1 do M7 troca por tabela); o enum casa pelo valor C do membro (IrEnum.valores ou o
 *                    indice) e o resultado por e_erro (0 = valor, 1 = erro, como o switch ((r).e_erro ? 1 : 0) do emit_c); o caso padrao roda no fim da cadeia; sem queda entre casos.
 *                    A ligacao ('switch r in') recebe o valor ou o erro antes do corpo (COPIA_MEM se agregado). ?T, ?^T e union seguem X0001 (P15a).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/resultado
 */
#include "nativo/bir/bir_interno.h"

/* Estado de uma chaveia: o discriminante (enum I32, e_erro/tem I8, tag de union I32, ponteiro != 0 I8), o fim comum dos casos, o endereco do slot do agregado
 * (resultado, ?T, union) e, em ?^T e ?cstr, o proprio ponteiro (vreg I64) que a ligacao recebe. */
typedef struct { BirOperando disc; BirRotulo fim; uint32_t endereco; BirOperando ponteiro; } BirChaveia;

/* ---- valor C do membro de enum ---- */

static int64_t ValorDoMembro(const BirCtx *c, TipoId tipo, int64_t indice)
{
    const IrPrograma *ir = c->e->ir;
    int32_t nome = (int32_t)TipoDe(c->e->tipos, tipo)->a;

    for (uint32_t k = 0; k < ir->nenums; k++) {
        const IrEnum *en = &ir->enums[k];

        if (en->nome != nome)
            continue;
        return en->valores != NULL && indice >= 0 && (uint64_t)indice < en->nmembros ? en->valores[indice] : indice;
    }
    return indice;
}

int64_t BirInteiroDoValor(const BirCtx *c, const Valor *v)
{
    if (TipoClasseDe(c->e->tipos, v->tipo) != TC_ENUM)
        return v->inteiro;
    return (int32_t)ValorDoMembro(c, v->tipo, v->inteiro);
}

/* ---- discriminante e rotulo de cada caso ---- */

/* ?^T e ?cstr: o discriminante e (ponteiro != 0), 1 = tem valor. */
static bool PrepararPonteiro(BirCtx *c, const IrInstr *i, BirChaveia *s)
{
    uint32_t tem;

    if (!BirOperandoDe(c, i, &i->a, &s->ponteiro) || !BirOperandoVreg(c, s->ponteiro, &s->ponteiro))
        return false;
    tem = BirComparar(c, BCOND_NE, s->ponteiro, OpImed(0, BC_I64));
    s->disc = OpVreg(tem, BC_I8);
    return true;
}

/* Resultado (e_erro), ?T (tem) e union (tag u32): o discriminante e o membro 0, no offset 0 do slot. */
static bool PrepararAgregado(BirCtx *c, const IrInstr *i, BirChaveia *s)
{
    uint8_t classe = TipoClasseDe(c->e->tipos, i->a.tipo) == TC_UNIAO ? BC_I32 : BC_I8;
    uint32_t membro;

    if (!BirEnderecoDe(c, i, &i->a, &s->endereco))
        return false;
    membro = BirNovoVreg(c, classe);
    BirEmitirCarrega(c, membro, s->endereco, 0);
    s->disc = OpVreg(membro, classe);
    return true;
}

/* Resultado, ?T e union; qualquer outra classe e IR errada. */
static bool PrepararAgregadoOuFalha(BirCtx *c, const IrInstr *i, BirChaveia *s)
{
    switch (TipoClasseDe(c->e->tipos, i->a.tipo)) {
    case TC_RESULTADO: case TC_OPCIONAL: case TC_UNIAO:
        return PrepararAgregado(c, i, s);
    default:
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    }
}

static bool PrepararDiscriminante(BirCtx *c, const IrInstr *i, BirChaveia *s)
{
    BirOperando valor;

    if (TipoClasseDe(c->e->tipos, i->a.tipo) == TC_ENUM)
        return BirOperandoDe(c, i, &i->a, &valor) && BirOperandoVreg(c, valor, &s->disc);
    if (BirEhPonteiroOpcional(c->e->tipos, i->a.tipo))
        return PrepararPonteiro(c, i, s);
    return PrepararAgregadoOuFalha(c, i, s);
}

/* Valor que o discriminante tem quando o caso casa: enum = valor C do membro; resultado = 1 no caso do erro e 0 no do valor; ?T/?^T = 1 (valor) ou 0 (none); union = a tag. */
static bool RotuloDoCaso(BirCtx *c, const IrInstr *i, const IrCaso *k, int64_t *rotulo)
{
    const TipoInfo *info = TipoDe(c->e->tipos, i->a.tipo);

    if (info->classe == TC_ENUM) {
        *rotulo = (int32_t)ValorDoMembro(c, i->a.tipo, k->membro);
        return true;
    }
    if (info->classe != TC_RESULTADO) { /* ?T, ?^T e union: o membro do caso ja e o valor do discriminante */
        *rotulo = k->membro;
        return true;
    }
    if (k->membro == (int32_t)info->b)
        *rotulo = 1;
    else if (k->membro == (int32_t)info->a)
        *rotulo = 0;
    else
        return BirFalhaInconsistente(c, "caso de chaveia que nao e o valor nem o erro do resultado");
    return true;
}

/* ---- ligacao do 'switch r in' ---- */

static bool LigarAgregado(BirCtx *c, int32_t local, uint32_t endereco, const BirCampoInfo *parte)
{
    BirPedidoSlot pedido = {.id = local};
    uint32_t dst;

    if (!BirForma(c, parte->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirCopiaBloco(c, dst, BirEnderecoMais(c, endereco, parte->offset), pedido.forma.tam);
    return true;
}

/* Escalar com '&' tomado vive num slot: CARREGA da parte do resultado e GRAVA no slot do local. */
static bool LigarEscalarEmMemoria(BirCtx *c, int32_t local, uint32_t valor, uint8_t classe)
{
    BirPedidoSlot pedido = {.id = local};
    uint32_t endereco;

    if (!BirForma(c, c->tipo_local[local], &pedido.forma) || !BirDefinirSlot(c, &pedido, &endereco))
        return false;
    BirEmitirGrava(c, endereco, 0, OpVreg(valor, classe));
    return true;
}

static bool LigarEscalar(BirCtx *c, int32_t local, uint32_t endereco, const BirCampoInfo *parte)
{
    uint8_t classe = BC_NADA;
    uint32_t vreg;

    if (!BirClasseDoTipo(c->e->tipos, parte->tipo, &classe))
        return BirFalhaInconsistente(c, "ligacao de chaveia com tipo sem classe na BIR");
    if (local >= 0 && (uint32_t)local < c->n_local && c->em_memoria[local]) {
        vreg = BirNovoVreg(c, classe);
        BirEmitirCarrega(c, vreg, endereco, (int32_t)parte->offset);
        return LigarEscalarEmMemoria(c, local, vreg, classe);
    }
    if (!BirVregDeclarar(c, local, classe, &vreg))
        return false;
    BirEmitirCarrega(c, vreg, endereco, (int32_t)parte->offset);
    return true;
}

/* O local recebe o membro `parte` do agregado (COPIA_MEM se agregado, CARREGA se escalar). */
static bool LigarParte(BirCtx *c, int32_t local, uint32_t endereco, const BirCampoInfo *parte)
{
    BirRegistrarTipoLocal(c, local, parte->tipo);
    if (BirEhAgregado(c->e->tipos, parte->tipo))
        return LigarAgregado(c, local, endereco, parte);
    return LigarEscalar(c, local, endereco, parte);
}

/* O caso do resultado com ligacao: o local recebe o valor (caso do T) ou o erro (caso do E). */
static bool LigarResultado(BirCtx *c, const IrInstr *i, const IrCaso *k, const BirChaveia *s)
{
    const TipoInfo *info = TipoDe(c->e->tipos, i->a.tipo);
    BirCampoInfo parte;
    bool erro = k->membro == (int32_t)info->b;

    if (!BirParteDoResultado(c, i->a.tipo, erro ? RES_PARTE_ERRO : RES_PARTE_VALOR, &parte))
        return false;
    return LigarParte(c, k->ligacao, s->endereco, &parte);
}

/* ?T de valor (caso 1): o local recebe o membro v. */
static bool LigarOpcional(BirCtx *c, const IrInstr *i, const IrCaso *k, const BirChaveia *s)
{
    BirCampoInfo parte = {.offset = LayoutOffsetCampo(BirLayouts(c), i->a.tipo, OPC_MEMBRO_VALOR), .tipo = (TipoId)TipoDe(c->e->tipos, i->a.tipo)->a};

    return LigarParte(c, k->ligacao, s->endereco, &parte);
}

/* union: o local recebe a variante da tag do caso. */
static bool LigarUniao(BirCtx *c, const IrInstr *i, const IrCaso *k, const BirChaveia *s)
{
    const IrUniao *u = LayoutUniao(c->e->tipos, c->e->ir, i->a.tipo);
    BirCampoInfo parte;

    if (u == NULL || k->membro < 0 || (uint32_t)k->membro >= u->nvariantes)
        return BirFalhaInconsistente(c, "caso de union com tag fora das variantes");
    parte = (BirCampoInfo){.offset = LayoutOffsetCampo(BirLayouts(c), i->a.tipo, UNIAO_MEMBRO_VARIANTE), .tipo = u->variantes[k->membro]};
    return LigarParte(c, k->ligacao, s->endereco, &parte);
}

/* ?^T e ?cstr (caso 1): o local recebe o proprio ponteiro, com o tipo da ligacao (IrCaso.tipo). */
static bool LigarPonteiro(BirCtx *c, const IrCaso *k, const BirChaveia *s)
{
    uint32_t vreg;

    BirRegistrarTipoLocal(c, k->ligacao, k->tipo);
    if ((uint32_t)k->ligacao < c->n_local && c->em_memoria[k->ligacao])
        return LigarEscalarEmMemoria(c, k->ligacao, s->ponteiro.id, BC_I64);
    if (!BirVregDeclarar(c, k->ligacao, BC_I64, &vreg))
        return false;
    BirMover(c, vreg, s->ponteiro);
    return true;
}

/* ?T/?^T: o none e o enum nao ligam nada; o caso 1 liga o ponteiro ou o membro v. */
static bool LigarDeOpcional(BirCtx *c, const IrInstr *i, const IrCaso *k, const BirChaveia *s)
{
    if (k->membro == 0 || TipoClasseDe(c->e->tipos, i->a.tipo) == TC_ENUM)
        return true;
    return BirEhPonteiroOpcional(c->e->tipos, i->a.tipo) ? LigarPonteiro(c, k, s) : LigarOpcional(c, i, k, s);
}

/* O caso com ligacao ('switch r in'): resultado (valor ou erro), ?T/?^T (o valor, no caso 1) e union (a variante). O default e o none nao ligam nada. */
static bool LigarCaso(BirCtx *c, const IrInstr *i, const IrCaso *k, const BirChaveia *s)
{
    TipoClasse classe = TipoClasseDe(c->e->tipos, i->a.tipo);

    if (k->padrao || k->ligacao < 0)
        return true;
    if (classe == TC_RESULTADO)
        return LigarResultado(c, i, k, s);
    if (classe == TC_UNIAO)
        return LigarUniao(c, i, k, s);
    return LigarDeOpcional(c, i, k, s);
}

/* ---- casos ---- */

static bool CorpoDoCaso(BirCtx *c, const IrInstr *i, const IrCaso *k, BirChaveia *s)
{
    if (!LigarCaso(c, i, k, s) || !BirConstruirBloco(c, &k->corpo))
        return false;
    BirRotuloSalta(c, &s->fim);
    return true;
}

/* disc == rotulo -> corpo do caso; senao segue a cadeia no bloco `proximo` (que fica corrente). */
static bool TestarCaso(BirCtx *c, const IrInstr *i, const IrCaso *k, BirChaveia *s)
{
    int64_t rotulo = 0;
    uint32_t igual, corpo, proximo;

    if (!RotuloDoCaso(c, i, k, &rotulo))
        return false;
    igual = BirComparar(c, BCOND_EQ, s->disc, OpImed(rotulo, s->disc.classe));
    corpo = BirNovoBloco(c, false);
    proximo = BirNovoBloco(c, false);
    BirDesvia(c, OpVreg(igual, BC_I8), corpo, proximo);
    c->cur = corpo;
    if (!CorpoDoCaso(c, i, k, s))
        return false;
    c->cur = proximo;
    return true;
}

/* Testa os casos em ordem; o default (se houver) fica em *padrao para depois da cadeia. */
static bool TestarCasos(BirCtx *c, const IrInstr *i, BirChaveia *s, const IrCaso **padrao)
{
    for (uint32_t k = 0; k < i->ncasos; k++) {
        if (i->casos[k].padrao)
            *padrao = &i->casos[k];
        else if (!TestarCaso(c, i, &i->casos[k], s))
            return false;
    }
    return true;
}

bool BirRegraChaveia(BirCtx *c, const IrInstr *i)
{
    BirChaveia s = {.fim = {BIR_SEM_BLOCO}};
    const IrCaso *padrao = NULL;

    if (!PrepararDiscriminante(c, i, &s) || !TestarCasos(c, i, &s, &padrao))
        return false;
    if (padrao != NULL && !CorpoDoCaso(c, i, padrao, &s))
        return false;
    BirRotuloSalta(c, &s.fim);
    BirRotuloColoca(c, &s.fim);
    return true;
}
