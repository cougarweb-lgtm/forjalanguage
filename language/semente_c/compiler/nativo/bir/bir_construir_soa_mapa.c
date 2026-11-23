/* bir_construir_soa_mapa.c - soa (fixa e dinamica) e map na construcao da BIR: IR_SOA_*, IR_MAPA_*, IR_LIBERA op_aux 4 e 5 e o len(m) de map e de soa dinamica, sobre o runtime nativo (rt_soa.c, rt_mapa.c).
 * Versao: 0.1.4
 * Changelog: 0.1.4 - M1 split (arquivo no limite de 800 linhas): ChaveEhPorValor e ChamarMapaPorValor saem para bir_construir_soa_mapa_valor.c 0.1.0; MapaRef, ServicoMapa e PedidoMapa vao para bir_interno.h 0.31.0; sem mudar codigo.
 *            0.1.3 - M1: chave inteira (i32, u32, int, uint, enum) de 4 ou 8 bytes sem texto vai por valor ao runtime (forja_mapa_<nome>_k4/_k8), sem slot temporario; as demais seguem no slot.
 *            0.1.2 - O0 P13: indice da soa conferido tambem no release com --checagem sim (BirConfereLimites; dinamica pelo len, sem a epoca). 0.1.1 - M3 F9b-3: decisoes extraidas de BirRegraMapaItem (BirRegraSoaColuna e BirRegraSoaCria ficam na divida: arquivo no limite de 800 linhas).
 *            0.1.0 - M2 P17b: primeira versao. As MESMAS chamadas e argumentos que o emit_c_v19.c:468-761 escreve. soa fixa e a struct de colunas f_<campo>[N] do layout/ (membro k = coluna k); soa
 *                    dinamica e {void *col[ncol]; len; cap; alocador; epoca} (membros 0..4 do layout/, colunas em 8 * k). Indice conferido em dev: soa fixa por BirConferirIndice (forja_falha_indice, R0006),
 *                    dinamica por forja_indice_lista (epoca R0008 + R0006); release nao confere. gather/scatter copiam membro a membro entre a struct S e o item de cada coluna. forja_soa_criar/crescer
 *                    recebem os tamanhos das colunas num slot de 8 * ncol bytes (o (const int64_t[]){...} do C). Map: a chave vai num slot temporario (fj_k) e o runtime recebe o endereco dele, o tamanho SZ,
 *                    o texto TX (chave str) e o tamanho do valor SV; achar/inserir devolvem o indice denso e o valor mora em valores + indice * SV (offsets do forja_mapa em layout.h, conferidos com
 *                    o forja_rt_layout). G2: o codigo gerado SO chama o runtime (forja_soa_*, forja_mapa_*); le e grava o bloco do map/soa apenas nos offsets conferidos na compilacao.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/v19b
 */
#include "nativo/bir/bir_interno.h"

#define SOA_MEMBRO_LEN 1u        /* membros da soa dinamica no layout/: 0 = col[ncol], depois len, cap, alocador, epoca */
#define SOA_MEMBRO_CAP 2u
#define SOA_MEMBRO_ALOCADOR 3u
#define SOA_MEMBRO_EPOCA 4u
#define SOA_BYTES_POR_COLUNA 8u  /* um void * por coluna */
#define FATIA_CAMPO_PTR 0u
#define FATIA_CAMPO_LEN 1u
#define MAPA_AUX_ITEM_CHAVE 0u
#define MAPA_AUX_ITEM_VALOR 1u
#define MAPA_AUX_ITEM_ENDERECO 2u

/* ---- auxiliares ---- */

/* Arquivo (vreg com o texto) e linha (imediato) do .fj: os dois ultimos argumentos de todo servico. */
static void ArquivoELinha(BirCtx *c, const IrInstr *i, BirOperando saida[2])
{
    uint32_t linha;

    saida[0] = BirVregDoArquivo(c, i->pos, &linha);
    saida[1] = OpImed(linha, BC_I32);
}

/* vreg novo = [base + offset]. */
static uint32_t Carregar(BirCtx *c, uint32_t base, uint32_t offset, uint8_t classe)
{
    uint32_t dst = BirNovoVreg(c, classe);

    BirEmitirCarrega(c, dst, base, (int32_t)offset);
    return dst;
}

/* base + indice * tam num vreg novo; indice imediato soma a constante, tam 1 dispensa o MUL. */
static uint32_t SomarItem(BirCtx *c, uint32_t base, BirOperando indice, uint32_t tam)
{
    BirOperando deslocamento;

    if (indice.tipo == BO_IMED)
        deslocamento = OpImed(indice.imed * (int64_t)tam, BC_I64);
    else if (tam == 1)
        deslocamento = indice;
    else
        deslocamento = OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = BIR_MUL, .a = indice, .b = OpImed(tam, BC_I64)}), BC_I64);
    if (deslocamento.tipo == BO_IMED && deslocamento.imed == 0)
        return base;
    return BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(base, BC_I64), .b = deslocamento});
}

/* [enderecos[0]] = [enderecos[1]] para um membro de tipo T: escalar = CARREGA + GRAVA, agregado = COPIA_MEM. */
static bool CopiarMembro(BirCtx *c, const IrInstr *i, TipoId tipo, const uint32_t enderecos[2])
{
    uint8_t classe = BC_NADA;
    BirSlot forma;
    uint32_t valor;

    if (BirEhAgregado(c->e->tipos, tipo)) {
        if (!BirForma(c, tipo, &forma))
            return false;
        BirCopiaBloco(c, enderecos[0], enderecos[1], forma.tam);
        return true;
    }
    if (!BirClasseDoTipo(c->e->tipos, tipo, &classe))
        return BirFalhaTipo(c, i, "-", tipo);
    valor = Carregar(c, enderecos[1], 0, classe);
    BirEmitirGrava(c, enderecos[0], 0, OpVreg(valor, classe));
    return true;
}

/* ---- soa: a soa, suas colunas e a linha ---- */

/* O bloco da soa: endereco = vreg I64 (fixa: as colunas f_<campo>[N]; dinamica: col[], len, cap, alocador, epoca); estrutura = a struct S das linhas. */
typedef struct { uint32_t endereco; TipoId tipo; bool fixa; const IrStruct *estrutura; } SoaRef;

/* Uma linha da soa: a soa e o indice (ja conferido em dev). */
typedef struct { SoaRef ref; BirOperando indice; } LinhaSoa;

static uint32_t OffsetDaSoa(BirCtx *c, const SoaRef *r, uint32_t membro)
{
    return LayoutOffsetCampo(BirLayouts(c), r->tipo, membro);
}

/* tipo, fixa e estrutura de um tipo soa; o endereco fica por conta de quem chama. */
static bool SoaDoTipo(BirCtx *c, const IrInstr *i, TipoId tipo, SoaRef *ref)
{
    const Tipos *tipos = c->e->tipos;
    TipoId colunas;

    if (TipoClasseDe(tipos, tipo) != TC_SOA)
        return BirFalhaTipo(c, i, "-", tipo);
    colunas = TipoDe(tipos, tipo)->a;
    ref->tipo = tipo;
    ref->fixa = TipoClasseDe(tipos, colunas) == TC_ARRAY;
    ref->estrutura = LayoutEstrutura(tipos, c->e->ir, TipoDe(tipos, colunas)->a);
    return ref->estrutura != NULL || BirFalhaInconsistente(c, "soa sem a struct dos itens na IR");
}

/* A soa de um operando soa (por valor: o slot) ou ^soa (o ponteiro). */
static bool SoaDe(BirCtx *c, const IrInstr *i, const Valor *v, SoaRef *ref)
{
    const Tipos *tipos = c->e->tipos;
    BirOperando ponteiro;

    if (TipoClasseDe(tipos, v->tipo) != TC_PONTEIRO)
        return SoaDoTipo(c, i, v->tipo, ref) && BirEnderecoDe(c, i, v, &ref->endereco);
    if (!SoaDoTipo(c, i, TipoDe(tipos, v->tipo)->a, ref))
        return false;
    if (!BirOperandoDe(c, i, v, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    ref->endereco = ponteiro.id;
    return true;
}

/* vreg com o ponteiro da coluna k: fixa = endereco do membro f_<campo>; dinamica = o col[k] carregado. */
static uint32_t BaseDaColuna(BirCtx *c, const SoaRef *r, uint32_t k)
{
    if (r->fixa)
        return BirEnderecoMais(c, r->endereco, OffsetDaSoa(c, r, k));
    return Carregar(c, r->endereco, k * SOA_BYTES_POR_COLUNA, BC_I64);
}

/* Dev, soa dinamica: forja_indice_lista(indice, len, epoca, arquivo, linha) confere a epoca do frame (R0008) e o indice (R0006) e devolve o indice. */
static BirOperando IndiceListaDev(BirCtx *c, const IrInstr *i, const SoaRef *r, BirOperando indice)
{
    BirOperando *args = BirArgs(c, 5);
    uint32_t linha;

    args[0] = indice;
    args[1] = OpVreg(Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_LEN), BC_I64), BC_I64);
    args[2] = OpVreg(Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_EPOCA), BC_I32), BC_I32);
    args[3] = BirVregDoArquivo(c, i->pos, &linha);
    args[4] = OpImed(linha, BC_I32);
    return OpVreg(BirChamarRuntime(c, &(BirChamadaRt){"forja_indice_lista", BC_I64, 5}, args), BC_I64);
}

/* O indice b da instrucao; dev confere (soa fixa: 0..<N pelo forja_falha_indice; dinamica: forja_indice_lista), release nao. */
static bool IndiceDaSoa(BirCtx *c, const IrInstr *i, const SoaRef *r, BirOperando *indice)
{
    TipoId colunas = TipoDe(c->e->tipos, r->tipo)->a;

    if (!BirInteiro64(c, i, &i->b, indice))
        return false;
    if (!BirConfereLimites(c))
        return true;
    if (!BirOperandoVreg(c, *indice, indice))
        return false;
    if (r->fixa || c->p->modo != BM_DEV) /* release com --checagem sim: dinamica pelo len, sem a epoca R0008 */
        BirConferirIndice(c, i, *indice, r->fixa ? OpImed(TipoDe(c->e->tipos, colunas)->b, BC_I64) : OpVreg(Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_LEN), BC_I64), BC_I64));
    else
        *indice = IndiceListaDev(c, i, r, *indice);
    return true;
}

static bool AbrirLinha(BirCtx *c, const IrInstr *i, LinhaSoa *linha)
{
    return SoaDe(c, i, &i->a, &linha->ref) && IndiceDaSoa(c, i, &linha->ref, &linha->indice);
}

/* O endereco do item da coluna k na linha. */
static bool EnderecoDoItem(BirCtx *c, const LinhaSoa *linha, uint32_t k, uint32_t *endereco)
{
    BirSlot forma;

    if (!BirForma(c, linha->ref.estrutura->campos[k].tipo, &forma))
        return false;
    *endereco = SomarItem(c, BaseDaColuna(c, &linha->ref, k), linha->indice, forma.tam);
    return true;
}

/* Posicao do campo (nome internado) na struct da soa; ausente = IR errada. */
static bool PosicaoDoCampo(BirCtx *c, const SoaRef *r, int32_t nome, uint32_t *posicao)
{
    for (uint32_t k = 0; k < r->estrutura->ncampos; k++) {
        if (r->estrutura->campos[k].nome == nome) {
            *posicao = k;
            return true;
        }
    }
    return BirFalhaInconsistente(c, "campo de soa inexistente na struct");
}

/* gather (espalha = false): o item de cada coluna vai para o membro da struct em `estrutura`; scatter (espalha = true): o contrario. */
typedef struct { const LinhaSoa *linha; uint32_t estrutura; TipoId tipo_estrutura; bool espalha; } Movimento;

static bool MoverLinha(BirCtx *c, const IrInstr *i, const Movimento *m)
{
    for (uint32_t k = 0; k < m->linha->ref.estrutura->ncampos; k++) {
        uint32_t item, membro, enderecos[2];

        if (!EnderecoDoItem(c, m->linha, k, &item))
            return false;
        membro = BirEnderecoMais(c, m->estrutura, LayoutOffsetCampo(BirLayouts(c), m->tipo_estrutura, k));
        enderecos[0] = m->espalha ? item : membro;
        enderecos[1] = m->espalha ? membro : item;
        if (!CopiarMembro(c, i, m->linha->ref.estrutura->campos[k].tipo, enderecos))
            return false;
    }
    return true;
}

/* ---- IR_SOA_ELEMENTO, IR_SOA_CAMPO, IR_SOA_END_CAMPO, IR_SOA_GRAVA ---- */

/* temp(S) = a[b] (gather): a struct sai no slot do temporario. */
bool BirRegraSoaElemento(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    LinhaSoa linha;
    uint32_t destino;

    if (!AbrirLinha(c, i, &linha) || !BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &destino))
        return false;
    return MoverLinha(c, i, &(Movimento){&linha, destino, i->tipo, false});
}

/* O item do campo `nome` na linha: a posicao do campo, o tipo dele e o endereco do item. */
typedef struct { uint32_t endereco; TipoId tipo; } ItemSoa;

static bool ItemDoCampo(BirCtx *c, const IrInstr *i, ItemSoa *item)
{
    LinhaSoa linha;
    uint32_t k = 0;

    if (!AbrirLinha(c, i, &linha) || !PosicaoDoCampo(c, &linha.ref, i->nome, &k))
        return false;
    item->tipo = linha.ref.estrutura->campos[k].tipo;
    return EnderecoDoItem(c, &linha, k, &item->endereco);
}

/* temp = a[b].campo: escalar = CARREGA, agregado = COPIA_MEM para o slot do temporario. */
bool BirRegraSoaCampo(BirCtx *c, const IrInstr *i)
{
    ItemSoa item;

    if (!ItemDoCampo(c, i, &item))
        return false;
    return BirTempDoCampo(c, i, item.endereco, &(BirCampoInfo){.offset = 0, .tipo = item.tipo});
}

/* temp(^campo) = &a[b].campo. */
bool BirRegraSoaEndCampo(BirCtx *c, const IrInstr *i)
{
    ItemSoa item;
    uint32_t dst;

    if (!ItemDoCampo(c, i, &item) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_COPIA, .dst = dst, .a = OpVreg(item.endereco, BC_I64)});
    return true;
}

/* a[b] = args[0] (scatter): a struct S espalhada pelas colunas. O indice e conferido antes de ler o valor, como o fj_ix do C. */
bool BirRegraSoaGrava(BirCtx *c, const IrInstr *i)
{
    LinhaSoa linha;
    uint32_t valor;

    if (!AbrirLinha(c, i, &linha) || !BirEnderecoDe(c, i, &i->args[0], &valor))
        return false;
    return MoverLinha(c, i, &(Movimento){&linha, valor, i->args[0].tipo, true});
}

/* ---- IR_SOA_COLUNA ---- */

/* len da coluna: N na soa fixa, o len atual na dinamica. */
static BirOperando LenDaColuna(BirCtx *c, const SoaRef *r)
{
    if (r->fixa)
        return OpImed(TipoDe(c->e->tipos, TipoDe(c->e->tipos, r->tipo)->a)->b, BC_I64);
    return OpVreg(Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_LEN), BC_I64), BC_I64);
}

/* temp([]campo) = a.campo: {ptr, len}; dinamica em dev confere a epoca (forja_lista_viva). LIMITE: decisoes 6 - arquivo no teto de 800 linhas e static (SoaDe, Carregar) nao publicados */
bool BirRegraSoaColuna(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    const Layouts *lay = BirLayouts(c);
    SoaRef r;
    uint32_t k = 0, fatia;
    BirOperando ptr, len;

    if (!SoaDe(c, i, &i->a, &r) || !PosicaoDoCampo(c, &r, i->nome, &k))
        return false;
    if (!r.fixa && c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, Carregar(c, r.endereco, OffsetDaSoa(c, &r, SOA_MEMBRO_EPOCA), BC_I32));
    ptr = OpVreg(BaseDaColuna(c, &r, k), BC_I64);
    len = LenDaColuna(c, &r);
    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &fatia))
        return false;
    BirEmitirGrava(c, fatia, (int32_t)LayoutOffsetCampo(lay, i->tipo, FATIA_CAMPO_PTR), ptr);
    BirEmitirGrava(c, fatia, (int32_t)LayoutOffsetCampo(lay, i->tipo, FATIA_CAMPO_LEN), len);
    return true;
}

/* ---- IR_SOA_CRIA, IR_SOA_ANEXA, IR_LIBERA op_aux 5 ---- */

/* O (const int64_t[]){sizeof(T0), ...} do C: um slot de 8 * ncol bytes com o tamanho de cada coluna; *tams = vreg com o endereco dele. */
static bool TamanhosDasColunas(BirCtx *c, const SoaRef *r, uint32_t *tams)
{
    uint32_t ncol = r->estrutura->ncampos;

    *tams = BirEmitirEndSlot(c, BirNovoSlot(c, SOA_BYTES_POR_COLUNA * ncol, SOA_BYTES_POR_COLUNA), BirNovoVreg(c, BC_I64));
    for (uint32_t k = 0; k < ncol; k++) {
        BirSlot forma;

        if (!BirForma(c, r->estrutura->campos[k].tipo, &forma))
            return false;
        BirEmitirGrava(c, *tams, (int32_t)(k * SOA_BYTES_POR_COLUNA), OpImed(forma.tam, BC_I64));
    }
    return true;
}

/* Cap e alocador do pedido de criacao. */
typedef struct { BirOperando cap, alocador; } PedidoCriaSoa;

/* forja_soa_criar(col, tams, ncol, cap, alocador, &epoca, arquivo, linha). */
static bool ChamarSoaCriar(BirCtx *c, const IrInstr *i, const SoaRef *r, const PedidoCriaSoa *pedido)
{
    BirOperando *args = BirArgs(c, 8);
    uint32_t tams;

    if (!TamanhosDasColunas(c, r, &tams))
        return false;
    args[0] = OpVreg(r->endereco, BC_I64);
    args[1] = OpVreg(tams, BC_I64);
    args[2] = OpImed(r->estrutura->ncampos, BC_I32);
    args[3] = pedido->cap;
    args[4] = pedido->alocador;
    args[5] = OpVreg(BirEnderecoMais(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_EPOCA)), BC_I64);
    ArquivoELinha(c, i, &args[6]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_soa_criar", BC_NADA, 8}, args);
    return true;
}

/* temp(soa[dynamic]S) = (T){0}; forja_soa_criar(...); tD.cap = b; tD.alocador = a. LIMITE: decisoes 6 - mesmo motivo de BirRegraSoaColuna (arquivo no teto de 800) */
bool BirRegraSoaCria(BirCtx *c, const IrInstr *i)
{
    PedidoCriaSoa pedido;
    SoaRef r;
    BirSlot forma;

    if (!SoaDoTipo(c, i, i->tipo, &r) || !BirAlocadorDe(c, i, &i->a, &pedido.alocador) || !BirInteiro64(c, i, &i->b, &pedido.cap))
        return false;
    if (!BirForma(c, i->tipo, &forma) || !BirSlotDoDestino(c, i, &r.endereco))
        return false;
    BirZeraBloco(c, r.endereco, forma.tam);
    if (!ChamarSoaCriar(c, i, &r, &pedido))
        return false;
    BirEmitirGrava(c, r.endereco, (int32_t)OffsetDaSoa(c, &r, SOA_MEMBRO_CAP), pedido.cap);
    BirEmitirGrava(c, r.endereco, (int32_t)OffsetDaSoa(c, &r, SOA_MEMBRO_ALOCADOR), pedido.alocador);
    return true;
}

/* forja_soa_crescer(col, tams, ncol, len, &cap, alocador, &epoca, arquivo, linha). */
static bool CrescerSoa(BirCtx *c, const IrInstr *i, const SoaRef *r)
{
    BirOperando *args = BirArgs(c, 9);
    uint32_t tams;

    if (!TamanhosDasColunas(c, r, &tams))
        return false;
    args[0] = OpVreg(r->endereco, BC_I64);
    args[1] = OpVreg(tams, BC_I64);
    args[2] = OpImed(r->estrutura->ncampos, BC_I32);
    args[3] = OpVreg(Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_LEN), BC_I64), BC_I64);
    args[4] = OpVreg(BirEnderecoMais(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_CAP)), BC_I64);
    args[5] = OpVreg(Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_ALOCADOR), BC_I32), BC_I32);
    args[6] = OpVreg(BirEnderecoMais(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_EPOCA)), BC_I64);
    ArquivoELinha(c, i, &args[7]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_soa_crescer", BC_NADA, 9}, args);
    return true;
}

/* if (len == cap) forja_soa_crescer(...). */
static bool CrescerSeCheia(BirCtx *c, const IrInstr *i, const SoaRef *r)
{
    uint32_t len = Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_LEN), BC_I64);
    uint32_t cap = Carregar(c, r->endereco, OffsetDaSoa(c, r, SOA_MEMBRO_CAP), BC_I64);
    uint32_t cheia = BirComparar(c, BCOND_EQ, OpVreg(len, BC_I64), OpVreg(cap, BC_I64));
    uint32_t cresce = BirNovoBloco(c, false), segue = BirNovoBloco(c, false);

    BirDesvia(c, OpVreg(cheia, BC_I8), cresce, segue);
    c->cur = cresce;
    if (!CrescerSoa(c, i, r))
        return false;
    BirSalta(c, segue);
    c->cur = segue;
    return true;
}

/* a e ^soa[dynamic]S (nunca soa fixa nem por valor); dev confere a epoca do frame (forja_lista_viva). */
static bool PrepararAnexa(BirCtx *c, const IrInstr *i, SoaRef *ref)
{
    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_PONTEIRO)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!SoaDe(c, i, &i->a, ref))
        return false;
    if (ref->fixa)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, Carregar(c, ref->endereco, OffsetDaSoa(c, ref, SOA_MEMBRO_EPOCA), BC_I32));
    return true;
}

/* Espalha o item (struct em `valor`) pelas colunas na posicao len (relida: a soa pode ter crescido) e faz len += 1. */
static bool GravarNoFim(BirCtx *c, const IrInstr *i, LinhaSoa *linha, uint32_t valor)
{
    uint32_t len_offset = OffsetDaSoa(c, &linha->ref, SOA_MEMBRO_LEN);
    uint32_t novo_len;

    linha->indice = OpVreg(Carregar(c, linha->ref.endereco, len_offset, BC_I64), BC_I64);
    if (!MoverLinha(c, i, &(Movimento){linha, valor, i->b.tipo, true}))
        return false;
    novo_len = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = linha->indice, .b = OpImed(1, BC_I64)});
    BirEmitirGrava(c, linha->ref.endereco, (int32_t)len_offset, OpVreg(novo_len, BC_I64));
    return true;
}

/* append(a, item = b) em ^soa[dynamic]S: dev confere a epoca, cresce se cheia, espalha o item pelas colunas na posicao len e len += 1. */
bool BirRegraSoaAnexa(BirCtx *c, const IrInstr *i)
{
    LinhaSoa linha;
    uint32_t valor;

    if (!PrepararAnexa(c, i, &linha.ref) || !CrescerSeCheia(c, i, &linha.ref) || !BirEnderecoDe(c, i, &i->b, &valor))
        return false;
    return GravarNoFim(c, i, &linha, valor);
}

/* IR_LIBERA op_aux 5 (a = ^soa[dynamic]S): forja_soa_liberar(col, alocador, arquivo, linha); a soa volta a {0} mantendo o alocador. */
bool BirLiberaSoa(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 4);
    SoaRef r;
    BirSlot forma;
    uint32_t alocador;

    if (!SoaDe(c, i, &i->a, &r) || !BirForma(c, r.tipo, &forma))
        return false;
    alocador = Carregar(c, r.endereco, OffsetDaSoa(c, &r, SOA_MEMBRO_ALOCADOR), BC_I32);
    args[0] = OpVreg(r.endereco, BC_I64);
    args[1] = OpVreg(alocador, BC_I32);
    ArquivoELinha(c, i, &args[2]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_soa_liberar", BC_NADA, 4}, args);
    BirZeraBloco(c, r.endereco, forma.tam);
    BirEmitirGrava(c, r.endereco, (int32_t)OffsetDaSoa(c, &r, SOA_MEMBRO_ALOCADOR), OpVreg(alocador, BC_I32));
    return true;
}

/* ---- map: o map e seus tamanhos ---- */

static bool MapaDoTipo(BirCtx *c, const IrInstr *i, TipoId tipo, MapaRef *ref)
{
    const Tipos *tipos = c->e->tipos;

    if (TipoClasseDe(tipos, tipo) != TC_MAPA)
        return BirFalhaTipo(c, i, "-", tipo);
    ref->tipo = tipo;
    ref->chave = TipoDe(tipos, tipo)->a;
    ref->valor = TipoDe(tipos, tipo)->b;
    ref->texto = TipoClasseDe(tipos, ref->chave) == TC_TEXTO;
    return BirForma(c, ref->chave, &ref->forma_chave) && BirForma(c, ref->valor, &ref->forma_valor);
}

/* O map de um operando map (por valor: o slot) ou ^map (o ponteiro). */
static bool MapaDe(BirCtx *c, const IrInstr *i, const Valor *v, MapaRef *ref)
{
    const Tipos *tipos = c->e->tipos;
    BirOperando ponteiro;

    if (TipoClasseDe(tipos, v->tipo) != TC_PONTEIRO)
        return MapaDoTipo(c, i, v->tipo, ref) && BirEnderecoDe(c, i, v, &ref->endereco);
    if (!MapaDoTipo(c, i, TipoDe(tipos, v->tipo)->a, ref))
        return false;
    if (!BirOperandoDe(c, i, v, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    ref->endereco = ponteiro.id;
    return true;
}

/* O endereco do valor (ou da chave) de indice denso `indice`: valores (chaves) + indice * SV (SZ), lidos do bloco nos offsets de layout.h. */
static uint32_t EnderecoDoValor(BirCtx *c, const MapaRef *r, BirOperando indice)
{
    return SomarItem(c, Carregar(c, r->endereco, LAYOUT_MAPA_OFF_VALORES, BC_I64), indice, r->forma_valor.tam);
}

static uint32_t EnderecoDaChave(BirCtx *c, const MapaRef *r, BirOperando indice)
{
    return SomarItem(c, Carregar(c, r->endereco, LAYOUT_MAPA_OFF_CHAVES, BC_I64), indice, r->forma_chave.tam);
}

/* A chave b vai num slot temporario (o fj_k do C: <b> pode ser constante, sem endereco); *endereco = vreg com o endereco do slot. */
static bool ChaveEmSlot(BirCtx *c, const IrInstr *i, const MapaRef *r, uint32_t *endereco)
{
    *endereco = BirEmitirEndSlot(c, BirNovoSlot(c, r->forma_chave.tam, r->forma_chave.alinhamento), BirNovoVreg(c, BC_I64));
    return BirGravarEm(c, i, &(BirAlvoMem){.base = *endereco, .offset = 0, .tipo = r->chave}, &i->b);
}

static const ServicoMapa ACHAR = {"forja_mapa_achar", "forja_mapa_achar_k4", "forja_mapa_achar_k8", BC_I64, false};
static const ServicoMapa INSERIR = {"forja_mapa_inserir", "forja_mapa_inserir_k4", "forja_mapa_inserir_k8", BC_I64, true};
static const ServicoMapa REMOVER = {"forja_mapa_remover", "forja_mapa_remover_k4", "forja_mapa_remover_k8", BC_NADA, true};

/* forja_mapa_<nome>(m, &chave, SZ, TX[, SV], arquivo, linha); *resultado = o vreg do indice denso (achar e inserir; -1 = ausente) ou BIR_SEM_VREG (remover). */

static bool ChamarMapa(BirCtx *c, const PedidoMapa *p, uint32_t *resultado)
{
    uint32_t nargs = p->servico->com_valor ? 7u : 6u, chave, k = 4;
    BirOperando *args;

    if (ChaveEhPorValor(c, p->mapa))
        return ChamarMapaPorValor(c, p, resultado);
    args = BirArgs(c, nargs);
    if (!ChaveEmSlot(c, p->i, p->mapa, &chave))
        return false;
    args[0] = OpVreg(p->mapa->endereco, BC_I64);
    args[1] = OpVreg(chave, BC_I64);
    args[2] = OpImed(p->mapa->forma_chave.tam, BC_I64);
    args[3] = OpImed(p->mapa->texto ? 1 : 0, BC_I8);
    if (p->servico->com_valor)
        args[k++] = OpImed(p->mapa->forma_valor.tam, BC_I64);
    ArquivoELinha(c, p->i, &args[k]);
    *resultado = BirChamarRuntime(c, &(BirChamadaRt){p->servico->funcao, p->servico->retorno, nargs}, args);
    return true;
}

/* ---- IR_MAPA_CRIA, IR_LIBERA op_aux 4 ---- */

/* alocador (a) e cap (b) do pedido, ja como operandos. */
typedef struct { BirOperando alocador, cap; } PedidoCriaMapa;

static bool PrepararCriaMapa(BirCtx *c, const IrInstr *i, MapaRef *r, PedidoCriaMapa *pedido)
{
    BirSlot forma;

    if (!MapaDoTipo(c, i, i->tipo, r) || !BirAlocadorDe(c, i, &i->a, &pedido->alocador) || !BirInteiro64(c, i, &i->b, &pedido->cap))
        return false;
    if (!BirForma(c, i->tipo, &forma) || !BirSlotDoDestino(c, i, &r->endereco))
        return false;
    BirZeraBloco(c, r->endereco, forma.tam);
    return true;
}

/* temp(map) = (forja_mapa){0}; forja_mapa_criar(&tD, alocador, cap, SZ, SV, arquivo, linha). */
bool BirRegraMapaCria(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 7);
    PedidoCriaMapa pedido;
    MapaRef r;

    if (!PrepararCriaMapa(c, i, &r, &pedido))
        return false;
    args[0] = OpVreg(r.endereco, BC_I64);
    args[1] = pedido.alocador;
    args[2] = pedido.cap;
    args[3] = OpImed(r.forma_chave.tam, BC_I64);
    args[4] = OpImed(r.forma_valor.tam, BC_I64);
    ArquivoELinha(c, i, &args[5]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_mapa_criar", BC_NADA, 7}, args);
    return true;
}

/* IR_LIBERA op_aux 4 (a = ^map): forja_mapa_liberar(m, arquivo, linha); o runtime libera o bloco e zera o map. */
bool BirLiberaMapa(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 3);
    MapaRef r;

    if (!MapaDe(c, i, &i->a, &r))
        return false;
    args[0] = OpVreg(r.endereco, BC_I64);
    ArquivoELinha(c, i, &args[1]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_mapa_liberar", BC_NADA, 3}, args);
    return true;
}

/* ---- IR_MAPA_INSERE, IR_MAPA_REMOVE, IR_MAPA_TEM ---- */

/* insert(&m, key = b, value = args[0]): forja_mapa_inserir devolve o indice denso (existente ou novo); o valor e gravado em valores + indice * SV. */
bool BirRegraMapaInsere(BirCtx *c, const IrInstr *i)
{
    MapaRef r;
    uint32_t indice, destino;

    if (!MapaDe(c, i, &i->a, &r) || !ChamarMapa(c, &(PedidoMapa){i, &r, &INSERIR}, &indice))
        return false;
    destino = EnderecoDoValor(c, &r, OpVreg(indice, BC_I64));
    return BirGravarEm(c, i, &(BirAlvoMem){.base = destino, .offset = 0, .tipo = r.valor}, &i->args[0]);
}

/* remove(&m, key = b): forja_mapa_remover (chave ausente = nada acontece). */
bool BirRegraMapaRemove(BirCtx *c, const IrInstr *i)
{
    MapaRef r;
    uint32_t sem_resultado;

    return MapaDe(c, i, &i->a, &r) && ChamarMapa(c, &(PedidoMapa){i, &r, &REMOVER}, &sem_resultado);
}

/* temp(bool) = forja_mapa_achar(...) >= 0. */
bool BirRegraMapaTem(BirCtx *c, const IrInstr *i)
{
    MapaRef r;
    uint32_t indice, dst;

    if (!MapaDe(c, i, &i->a, &r) || !ChamarMapa(c, &(PedidoMapa){i, &r, &ACHAR}, &indice) || !BirDefinirDestino(c, BC_I8, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_CMP, .aux = BCOND_GE_S, .dst = dst, .a = OpVreg(indice, BC_I64), .b = OpImed(0, BC_I64)});
    return true;
}

/* ---- IR_MAPA_ACHA e IR_MAPA_PONTEIRO: indice >= 0 escolhe o ramo "sim", senao "nao" ---- */

typedef struct { uint32_t sim, nao, fim; } Desvio;

/* Desvia por indice >= 0 e deixa a construcao no ramo "sim". */
static Desvio AbrirDesvio(BirCtx *c, uint32_t indice)
{
    Desvio d = {BirNovoBloco(c, false), BirNovoBloco(c, false), BirNovoBloco(c, false)};
    uint32_t achou = BirComparar(c, BCOND_GE_S, OpVreg(indice, BC_I64), OpImed(0, BC_I64));

    BirDesvia(c, OpVreg(achou, BC_I8), d.sim, d.nao);
    c->cur = d.sim;
    return d;
}

/* Fecha o ramo "sim" e passa ao "nao". */
static void PassarAoNao(BirCtx *c, const Desvio *d)
{
    BirSalta(c, d->fim);
    c->cur = d->nao;
}

/* Fecha o ramo "nao" e continua no ponto de encontro. */
static void FecharDesvio(BirCtx *c, const Desvio *d)
{
    BirSalta(c, d->fim);
    c->cur = d->fim;
}

/* ?V em nicho (V ponteiro ou cstr, none = nulo): dst = achou ? valores[indice] : 0. */
static bool AchaNicho(BirCtx *c, const MapaRef *r, uint32_t indice)
{
    uint32_t dst;
    Desvio d;

    if (!BirDefinirDestino(c, BC_I64, &dst))
        return false;
    d = AbrirDesvio(c, indice);
    BirEmitirCarrega(c, dst, EnderecoDoValor(c, r, OpVreg(indice, BC_I64)), 0);
    PassarAoNao(c, &d);
    BirMover(c, dst, OpImed(0, BC_I64));
    FecharDesvio(c, &d);
    return true;
}

/* ?V em struct {tem; v}: achou grava tem = 1 e copia o valor; senao o slot inteiro zerado ((T){0} do C). */
static bool AchaStruct(BirCtx *c, const IrInstr *i, const MapaRef *r, uint32_t indice)
{
    BirSlot forma;
    uint32_t base, enderecos[2];
    Desvio d;

    if (!BirSlotDoDestino(c, i, &base) || !BirForma(c, i->tipo, &forma))
        return false;
    d = AbrirDesvio(c, indice);
    BirEmitirGrava(c, base, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->tipo, OPC_MEMBRO_TEM), OpImed(1, BC_I8));
    enderecos[0] = BirEnderecoMais(c, base, LayoutOffsetCampo(BirLayouts(c), i->tipo, OPC_MEMBRO_VALOR));
    enderecos[1] = EnderecoDoValor(c, r, OpVreg(indice, BC_I64));
    if (!CopiarMembro(c, i, r->valor, enderecos))
        return false;
    PassarAoNao(c, &d);
    BirZeraBloco(c, base, forma.tam);
    FecharDesvio(c, &d);
    return true;
}

/* temp(?V) = get(m, key = b): o valor se a chave existe, none se nao. */
bool BirRegraMapaAcha(BirCtx *c, const IrInstr *i)
{
    MapaRef r;
    uint32_t indice;

    if (!MapaDe(c, i, &i->a, &r) || !ChamarMapa(c, &(PedidoMapa){i, &r, &ACHAR}, &indice))
        return false;
    if (BirEhPonteiroOpcional(c->e->tipos, i->tipo))
        return AchaNicho(c, &r, indice);
    return AchaStruct(c, i, &r, indice);
}

/* temp(?^V) = get_ptr(&m, key = b): &valores[indice] se a chave existe, nulo se nao. */
bool BirRegraMapaPonteiro(BirCtx *c, const IrInstr *i)
{
    MapaRef r;
    uint32_t indice, dst;
    Desvio d;

    if (!MapaDe(c, i, &i->a, &r) || !ChamarMapa(c, &(PedidoMapa){i, &r, &ACHAR}, &indice) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    d = AbrirDesvio(c, indice);
    BirMover(c, dst, OpVreg(EnderecoDoValor(c, &r, OpVreg(indice, BC_I64)), BC_I64));
    PassarAoNao(c, &d);
    BirMover(c, dst, OpImed(0, BC_I64));
    FecharDesvio(c, &d);
    return true;
}

/* ---- IR_MAPA_ITEM ---- */

static bool ItemEndereco(BirCtx *c, uint32_t item)
{
    uint32_t dst;

    if (!BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_COPIA, .dst = dst, .a = OpVreg(item, BC_I64)});
    return true;
}

/* op_aux 0: dev forja_lista_viva(epoca); temp = chaves[b]. 1: temp = valores[b]. 2: temp(^V) = &valores[b]. O indice b (0..<len) nao e conferido. */
bool BirRegraMapaItem(BirCtx *c, const IrInstr *i)
{
    BirOperando indice;
    MapaRef r;

    if (i->op_aux > MAPA_AUX_ITEM_ENDERECO)
        return BirFalhaInconsistente(c, "IR_MAPA_ITEM com op_aux desconhecido");
    if (!MapaDe(c, i, &i->a, &r) || !BirInteiro64(c, i, &i->b, &indice))
        return false;
    switch (i->op_aux) {
    case MAPA_AUX_ITEM_CHAVE:
        if (c->p->modo == BM_DEV)
            BirChamarListaViva(c, i, Carregar(c, r.endereco, LAYOUT_MAPA_OFF_EPOCA, BC_I32));
        return BirTempDoCampo(c, i, EnderecoDaChave(c, &r, indice), &(BirCampoInfo){.offset = 0, .tipo = r.chave});
    case MAPA_AUX_ITEM_ENDERECO:
        return ItemEndereco(c, EnderecoDoValor(c, &r, indice));
    default:
        return BirTempDoCampo(c, i, EnderecoDoValor(c, &r, indice), &(BirCampoInfo){.offset = 0, .tipo = r.valor});
    }
}

/* ---- len(m) de map e de soa dinamica ---- */

/* IR_TAMANHO_TEXTO de map ou ^map: o membro len do bloco (offset de layout.h). */
static bool TamanhoDoMapa(BirCtx *c, const IrInstr *i)
{
    MapaRef r;
    uint32_t dst;

    if (!MapaDe(c, i, &i->a, &r) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirCarrega(c, dst, r.endereco, LAYOUT_MAPA_OFF_LEN);
    return true;
}

/* IR_TAMANHO_TEXTO de soa[dynamic]S ou ^soa[dynamic]S: o membro len. A soa fixa nao chega aqui (o N e constante da IR). */
static bool TamanhoDaSoa(BirCtx *c, const IrInstr *i)
{
    SoaRef r;
    uint32_t dst;

    if (!SoaDe(c, i, &i->a, &r))
        return false;
    if (r.fixa)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirCarrega(c, dst, r.endereco, (int32_t)OffsetDaSoa(c, &r, SOA_MEMBRO_LEN));
    return true;
}

/* len(x) de uma colecao que nao e str nem fatia: map e soa tratados aqui, [dynamic]T por BirTamanhoDeLista (senao X0001 pelo tipo). */
bool BirTamanhoDeColecao(BirCtx *c, const IrInstr *i)
{
    const Tipos *tipos = c->e->tipos;
    TipoId base = i->a.tipo;

    if (TipoClasseDe(tipos, base) == TC_PONTEIRO)
        base = TipoDe(tipos, base)->a;
    if (TipoClasseDe(tipos, base) == TC_MAPA)
        return TamanhoDoMapa(c, i);
    if (TipoClasseDe(tipos, base) == TC_SOA)
        return TamanhoDaSoa(c, i);
    return BirTamanhoDeLista(c, i);
}
