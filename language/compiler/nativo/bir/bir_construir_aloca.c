/* bir_construir_aloca.c - alocacao na construcao da BIR: IR_ALOCA, IR_LISTA_CRIA, IR_ANEXA, IR_LIBERA (op_aux 0..3) e IR_FRAME_RESET, sobre o runtime nativo (rt_aloc.c).
 * Versao: 0.9.0
 * Changelog: 0.9.0 - M6e P93k: com BirPrograma.guarda (--guarda-memoria) o append chama o crescer SEMPRE e CrescerLista passa o alvo a forja_lista_crescer_alvo (resize/reserve/append items= deixam de repetir sem fim). Sem a opcao, os mesmos bytes.
 *            0.8.1 - M5b P38b: o comparador de sort_by e heap_*_by vem de BirComparadorDoRuntime (no alvo linux o runtime o chama em Win64: fjtm_). Windows: mesmos bytes.
 *            0.8.0 - K17: BirRegraHeap (IR_HEAP: heap_push / heap_pop sobre a lista por BirListaDe; forja_heap_subir / forja_heap_retira_topo com o trampolim do less ou 0).
 *            0.7.0 - K08b: BirRegraOrdenaChave (sort_by_key: forja_ordc_ordenar com desloc e tipo) e BirRegraBuscaBinaria (binary_search: forja_ordc_posicao/achou; valor escalar num slot de pilha).
 *            0.6.0 - K08a: BirRegraOrdenaGeral (sort_by: forja_ordenar_estavel(ptr, len, tam, proc) do runtime).
 *            0.5.0 - to_str aceita builder de qualquer alocador: RedimensionaParaStr sai do ConferirBuilderNoHeap (R0007) e as macros dele.
 *            0.4.0 - IA1-L9f: BirRegraRedimensiona ganha REDIM_PARA_STR (to_str: RedimensionaParaStr move os bytes do builder para o str de destino, com NUL, e esvazia a lista).
 *            0.3.0- IA1-L9c: BirRegraRedimensiona despacha por op_aux: REDIM_RESIZE (o de sempre), REDIM_CAP (le o cap, com destino) e REDIM_RESERVA (cresce sem mudar o len).
 *            0.2.0 - IA1-L5b: BirRegraRedimensiona (resize, make com len), BirRegraAnexaFatia (append items =) e BirRegraOrdena (sort): so forja_lista_crescer, memset/memcpy e forja_ordenar do runtime.
 *            0.1.3 - BirRegraAloca: IR_ALOCA op_aux IR_ALOCA_ZERADO chama BirZeraBloco depois do forja_mem_alocar (new([N]T) grande sem valor inicial; sem temporario na pilha). Sem esse op_aux o conteudo de new segue sem zerar (igual ao C).
 *            0.1.2 - M2 P17b: IR_LIBERA op_aux 4 (map) e 5 (soa dinamica) entram na tabela, vindos de bir_construir_soa_mapa.c (BirLiberaMapa, BirLiberaSoa). O handle de alocador 2 (frame) segue sendo o
 *                    imediato u32 que o runtime resolve (G3): o backend nao compara epoca inline nem assume endereco do frame.
 *            0.1.1 - M2 P15b: Alocador passa a BirAlocadorDe (publica; bir_construir_formata.c usa o mesmo alocador u32 ou handle de Arena).
 *            0.1.0 - M2 P14b: primeira versao. Cada op vira CHAMA ao runtime com os MESMOS argumentos que o emit_c_instr.c escreve (alocador u32: 1 heap, 2 frame ou o handle de Arena/Pool; tamanho do
 *                    elemento pelo layout/; arquivo e linha do .fj): forja_mem_alocar, forja_lista_criar (+ GRAVA de ptr, len 0, cap e alocador no bloco de 32 B do [dynamic]T), forja_lista_crescer (so
 *                    quando len == cap), forja_mem_liberar, forja_lista_liberar (+ zera o bloco e regrava o alocador), forja_pool_devolver e forja_frame_reset. Dev chama forja_lista_viva antes do
 *                    append (R0008 da lista do frame velho); release nao. IR_LIBERA op_aux 4 e 5 (map e soa) seguem X0001, marco M2 (P17b). O conteudo de new nao e zerado (igual ao C).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/memoria
 */
#include "nativo/bir/bir_interno.h"

#define LISTA_OFFSET_PTR 0u        /* membro 0 do [dynamic]T: ptr */
#define LIBERA_AUX_QTD 6u          /* op_aux 0..3 daqui; 4 e 5 (map, soa) do P17b */
#define MARCO_LIBERA_RESTANTE "M2"

uint32_t BirChamarRuntime(BirCtx *c, const BirChamadaRt *chamada, BirOperando *args)
{
    uint32_t dst = chamada->retorno == BC_NADA ? BIR_SEM_VREG : BirNovoVreg(c, chamada->retorno);
    BirInstr *chama = BirEmitir(c, BIR_CHAMA, chamada->retorno);

    chama->simbolo = BirSimboloRuntime(c, chamada->funcao);
    chama->dst = dst;
    chama->args = args;
    chama->nargs = chamada->nargs;
    return dst;
}

/* Arquivo (vreg com o texto) e linha (imediato) do .fj: os dois ultimos argumentos de quase todo servico de alocacao. */
static void ArquivoELinha(BirCtx *c, const IrInstr *i, BirOperando saida[2])
{
    uint32_t linha;

    saida[0] = BirVregDoArquivo(c, i->pos, &linha);
    saida[1] = OpImed(linha, BC_I32);
}

/* Alocador: V_INT (1 heap, 2 frame) vira imediato u32; o handle de Arena/Pool e um valor u32 (vreg I32). */
bool BirAlocadorDe(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida)
{
    if (v->tipo_no == V_INT) {
        *saida = OpImed((int64_t)(uint32_t)v->inteiro, BC_I32);
        return true;
    }
    if (!BirOperandoDe(c, i, v, saida))
        return false;
    return saida->classe == BC_I32 || BirFalhaInconsistente(c, "alocador que nao e u32");
}

/* ---- IR_ALOCA ---- */

/* temp(^T) = forja_mem_alocar(alocador, sizeof(T), arquivo, linha); op_aux IR_ALOCA_ZERADO zera o bloco depois (BIR_ZERA_MEM, ou memset do runtime acima de 64 B). */
bool BirRegraAloca(BirCtx *c, const IrInstr *i)
{
    TipoId apontado = TipoDe(c->e->tipos, i->tipo)->a;
    BirOperando *args = BirArgs(c, 4);
    BirSlot forma;
    uint32_t dst;

    if (!BirAlocadorDe(c, i, &i->a, &args[0]) || !BirForma(c, apontado, &forma) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    args[1] = OpImed(forma.tam, BC_I64);
    ArquivoELinha(c, i, &args[2]);
    BirMover(c, dst, OpVreg(BirChamarRuntime(c, &(BirChamadaRt){"forja_mem_alocar", BC_I64, 4}, args), BC_I64));
    if (i->op_aux == IR_ALOCA_ZERADO)
        BirZeraBloco(c, dst, forma.tam);
    return true;
}

/* ---- IR_LISTA_CRIA ---- */

/* O que forja_lista_criar recebe: alocador, capacidade e o tamanho do elemento. */
typedef struct { BirOperando alocador, cap; uint32_t tam_elem; } PedidoLista;

/* ptr = forja_lista_criar(alocador, cap, sizeof(T), &lista.epoca, arquivo, linha); a epoca (0 fora do frame) e gravada pelo proprio servico. */
static uint32_t ChamarListaCriar(BirCtx *c, const IrInstr *i, const BirListaRef *lista, const PedidoLista *pedido)
{
    BirOperando *args = BirArgs(c, 6);

    args[0] = pedido->alocador;
    args[1] = pedido->cap;
    args[2] = OpImed(pedido->tam_elem, BC_I64);
    args[3] = OpVreg(BirEnderecoMais(c, lista->endereco, BirOffsetDaLista(c, lista, LISTA_MEMBRO_EPOCA)), BC_I64);
    ArquivoELinha(c, i, &args[4]);
    return BirChamarRuntime(c, &(BirChamadaRt){"forja_lista_criar", BC_I64, 6}, args);
}

/* temp([dynamic]T) = {ptr, len = 0, cap, alocador, epoca}: o bloco de 32 B vive no slot do temporario. */
bool BirRegraListaCria(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot slot = {.temporario = true, .id = i->destino};
    BirListaRef lista = {.tipo = i->tipo};
    PedidoLista pedido;
    BirSlot elemento;
    uint32_t ptr;

    if (!BirAlocadorDe(c, i, &i->a, &pedido.alocador) || !BirInteiro64(c, i, &i->b, &pedido.cap))
        return false;
    if (!BirForma(c, i->tipo, &slot.forma) || !BirForma(c, TipoDe(c->e->tipos, i->tipo)->a, &elemento) || !BirDefinirSlot(c, &slot, &lista.endereco))
        return false;
    pedido.tam_elem = elemento.tam;
    ptr = ChamarListaCriar(c, i, &lista, &pedido);
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_OFFSET_PTR), OpVreg(ptr, BC_I64));
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_LEN), OpImed(0, BC_I64));
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_CAP), pedido.cap);
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_ALOCADOR), pedido.alocador);
    return true;
}

/* ---- IR_ANEXA ---- */

/* vreg novo com o membro da lista (0 = ptr). */
static uint32_t CarregarMembro(BirCtx *c, const BirListaRef *lista, uint32_t membro, uint8_t classe)
{
    uint32_t dst = BirNovoVreg(c, classe);

    BirEmitirCarrega(c, dst, lista->endereco, (int32_t)BirOffsetDaLista(c, lista, membro));
    return dst;
}

/* Com --guarda-memoria (P93k): lista.ptr = forja_lista_crescer_alvo(&lista, sizeof(T), alvo, arquivo, linha): crescimento exato, novo = max(len + 1, alvo).
 * LIMITE: parametros 5 (o contexto, a instrucao, a lista, o tamanho do elemento e o alvo; agrupar criaria um tipo de uso unico). */
static void CrescerListaAlvo(BirCtx *c, const IrInstr *i, const BirListaRef *lista, uint32_t tam_elem, BirOperando alvo)
{
    BirOperando *args = BirArgs(c, 5);
    uint32_t novo;

    args[0] = OpVreg(BirEnderecoMais(c, lista->endereco, BirOffsetDaLista(c, lista, LISTA_OFFSET_PTR)), BC_I64);
    args[1] = OpImed(tam_elem, BC_I64);
    args[2] = alvo;
    ArquivoELinha(c, i, &args[3]);
    novo = BirChamarRuntime(c, &(BirChamadaRt){"forja_lista_crescer_alvo", BC_I64, 5}, args);
    BirEmitirGrava(c, lista->endereco, (int32_t)BirOffsetDaLista(c, lista, LISTA_OFFSET_PTR), OpVreg(novo, BC_I64));
}

/* lista.ptr = forja_lista_crescer(ptr, len, &lista.cap, sizeof(T), lista.alocador, &lista.epoca, arquivo, linha); com a guarda, o alvo vai para CrescerListaAlvo.
 * LIMITE: parametros 5 (o contexto, a instrucao, a lista, o tamanho do elemento e o alvo; agrupar criaria um tipo de uso unico). */
static void CrescerLista(BirCtx *c, const IrInstr *i, const BirListaRef *lista, uint32_t tam_elem, BirOperando alvo)
{
    BirOperando *args;
    uint32_t novo;

    if (c->p->guarda) {
        CrescerListaAlvo(c, i, lista, tam_elem, alvo);
        return;
    }
    args = BirArgs(c, 8);
    args[0] = OpVreg(CarregarMembro(c, lista, LISTA_OFFSET_PTR, BC_I64), BC_I64);
    args[1] = OpVreg(CarregarMembro(c, lista, LISTA_MEMBRO_LEN, BC_I64), BC_I64);
    args[2] = OpVreg(BirEnderecoMais(c, lista->endereco, BirOffsetDaLista(c, lista, LISTA_MEMBRO_CAP)), BC_I64);
    args[3] = OpImed(tam_elem, BC_I64);
    args[4] = OpVreg(CarregarMembro(c, lista, LISTA_MEMBRO_ALOCADOR, BC_I32), BC_I32);
    args[5] = OpVreg(BirEnderecoMais(c, lista->endereco, BirOffsetDaLista(c, lista, LISTA_MEMBRO_EPOCA)), BC_I64);
    ArquivoELinha(c, i, &args[6]);
    novo = BirChamarRuntime(c, &(BirChamadaRt){"forja_lista_crescer", BC_I64, 8}, args);
    BirEmitirGrava(c, lista->endereco, (int32_t)BirOffsetDaLista(c, lista, LISTA_OFFSET_PTR), OpVreg(novo, BC_I64));
}

/* ptr[len] = item (GRAVA de escalar, COPIA_MEM de agregado) e len += 1; ptr e len sao relidos porque a lista pode ter crescido. */
static bool GravarItem(BirCtx *c, const IrInstr *i, const BirListaRef *lista, uint32_t tam_elem)
{
    uint32_t ptr = CarregarMembro(c, lista, LISTA_OFFSET_PTR, BC_I64);
    uint32_t len = CarregarMembro(c, lista, LISTA_MEMBRO_LEN, BC_I64);
    uint32_t deslocamento = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_MUL, .a = OpVreg(len, BC_I64), .b = OpImed(tam_elem, BC_I64)});
    uint32_t alvo = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(ptr, BC_I64), .b = OpVreg(deslocamento, BC_I64)});
    uint32_t novo_len;

    if (!BirGravarEm(c, i, &(BirAlvoMem){.base = alvo, .offset = 0, .tipo = i->tipo}, &i->b))
        return false;
    novo_len = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(len, BC_I64), .b = OpImed(1, BC_I64)});
    BirEmitirGrava(c, lista->endereco, (int32_t)BirOffsetDaLista(c, lista, LISTA_MEMBRO_LEN), OpVreg(novo_len, BC_I64));
    return true;
}

/* append(a, item = b): dev confere a epoca do frame; len == cap cresce; grava o item. a e ^[dynamic]T, i->tipo e o elemento. */
bool BirRegraAnexa(BirCtx *c, const IrInstr *i)
{
    BirListaRef lista;
    BirSlot elemento;
    uint32_t cheia, cresce, segue;

    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_PONTEIRO)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!BirListaDe(c, i, &i->a, &lista) || !BirForma(c, i->tipo, &elemento))
        return false;
    if (c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, CarregarMembro(c, &lista, LISTA_MEMBRO_EPOCA, BC_I32));
    if (c->p->guarda) { /* P93k (D-M6e-5): todo append chama o crescer (o runtime move lista de heap e ignora os outros alocadores com folga) */
        uint32_t len = CarregarMembro(c, &lista, LISTA_MEMBRO_LEN, BC_I64);
        uint32_t mais_um = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(len, BC_I64), .b = OpImed(1, BC_I64)});

        CrescerLista(c, i, &lista, elemento.tam, OpVreg(mais_um, BC_I64));
        return GravarItem(c, i, &lista, elemento.tam);
    }
    cheia = BirComparar(c, BCOND_EQ, OpVreg(CarregarMembro(c, &lista, LISTA_MEMBRO_LEN, BC_I64), BC_I64),
                        OpVreg(CarregarMembro(c, &lista, LISTA_MEMBRO_CAP, BC_I64), BC_I64));
    cresce = BirNovoBloco(c, false);
    segue = BirNovoBloco(c, false);
    BirDesvia(c, OpVreg(cheia, BC_I8), cresce, segue);
    c->cur = cresce;
    CrescerLista(c, i, &lista, elemento.tam, OpImed(0, BC_I64));
    BirSalta(c, segue);
    c->cur = segue;
    return GravarItem(c, i, &lista, elemento.tam);
}

/* ---- IR_LIBERA ---- */

/* op_aux 0: forja_mem_liberar(a, arquivo, linha); a = ^T, ?^T, cstr ou ?cstr (nulo e bloco do frame o runtime ignora). */
static bool LiberaBloco(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 3);

    if (!BirOperandoDe(c, i, &i->a, &args[0]))
        return false;
    ArquivoELinha(c, i, &args[1]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_mem_liberar", BC_NADA, 3}, args);
    return true;
}

/* op_aux 1: forja_lista_liberar(ptr, alocador, arquivo, linha) e a lista volta a {0} mantendo o alocador. */
static bool LiberaLista(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 4);
    BirListaRef lista;
    BirSlot forma;
    uint32_t alocador;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirForma(c, lista.tipo, &forma))
        return false;
    alocador = CarregarMembro(c, &lista, LISTA_MEMBRO_ALOCADOR, BC_I32);
    args[0] = OpVreg(CarregarMembro(c, &lista, LISTA_OFFSET_PTR, BC_I64), BC_I64);
    args[1] = OpVreg(alocador, BC_I32);
    ArquivoELinha(c, i, &args[2]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_lista_liberar", BC_NADA, 4}, args);
    BirZeraBloco(c, lista.endereco, forma.tam);
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_ALOCADOR), OpVreg(alocador, BC_I32));
    return true;
}

/* op_aux 2: forja_mem_liberar(a.ptr, arquivo, linha); a e o str de um format (o ptr esta no membro 0). */
static bool LiberaTexto(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 3);
    uint32_t endereco, ptr = BirNovoVreg(c, BC_I64);

    if (!BirEnderecoDe(c, i, &i->a, &endereco))
        return false;
    BirEmitirCarrega(c, ptr, endereco, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, 0));
    args[0] = OpVreg(ptr, BC_I64);
    ArquivoELinha(c, i, &args[1]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_mem_liberar", BC_NADA, 3}, args);
    return true;
}

/* op_aux 3: forja_pool_devolver(handle do Pool = b, bloco = a, arquivo, linha). */
static bool LiberaPool(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 4);

    if (!BirAlocadorDe(c, i, &i->b, &args[0]) || !BirOperandoDe(c, i, &i->a, &args[1]))
        return false;
    ArquivoELinha(c, i, &args[2]);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_pool_devolver", BC_NADA, 4}, args);
    return true;
}

typedef bool (*LiberaFn)(BirCtx *c, const IrInstr *i);

/* Indexada por op_aux; 4 (map) e 5 (soa dinamica) vivem em bir_construir_soa_mapa.c. */
static const LiberaFn LIBERAS[LIBERA_AUX_QTD] = {LiberaBloco, LiberaLista, LiberaTexto, LiberaPool, BirLiberaMapa, BirLiberaSoa};

bool BirRegraLibera(BirCtx *c, const IrInstr *i)
{
    if (i->op_aux < LIBERA_AUX_QTD)
        return LIBERAS[i->op_aux](c, i);
    return BirFalhaX0001(c, i->pos, (const char *const[3]){BirNomeDoOp(i->op), NULL, NULL}, MARCO_LIBERA_RESTANTE);
}

/* ---- IR_FRAME_RESET ---- */

bool BirRegraFrameReset(BirCtx *c, const IrInstr *i)
{
    (void)i;
    BirChamarRuntime(c, &(BirChamadaRt){"forja_frame_reset", BC_NADA, 0}, NULL);
    return true;
}

/* ---- IR_REDIMENSIONA, IR_ANEXA_FATIA e IR_ORDENA (IA1-L5b) ---- */

#define FATIA_MEMBRO_LEN 1u /* {ptr, len} da fatia e do str */
#define MSG_RESIZE_NEGATIVO "resize com len negativo"

static uint32_t ContaI64(BirCtx *c, uint8_t op, BirOperando a, BirOperando b)
{
    return BirContaNova(c, BC_I64, (BirPedido){.op = op, .a = a, .b = b});
}

/* Enquanto cap < alvo: forja_lista_crescer (a capacidade dobra). ptr e cap voltam a ser lidos da lista a cada volta.
 * LIMITE: parametros 5 (o contexto, a instrucao e a lista que o CrescerLista repassa, mais o tamanho do elemento e o alvo; agrupar criaria um tipo de uso unico). */
static void CrescerAte(BirCtx *c, const IrInstr *i, const BirListaRef *lista, uint32_t tam_elem, BirOperando alvo)
{
    uint32_t cabeca = BirNovoBloco(c, false), corpo = BirNovoBloco(c, false), fim = BirNovoBloco(c, false);
    uint32_t menor;

    BirSalta(c, cabeca);
    c->cur = cabeca;
    menor = BirComparar(c, BCOND_LT_S, OpVreg(CarregarMembro(c, lista, LISTA_MEMBRO_CAP, BC_I64), BC_I64), alvo);
    BirDesvia(c, OpVreg(menor, BC_I8), corpo, fim);
    c->cur = corpo;
    CrescerLista(c, i, lista, tam_elem, alvo);
    BirSalta(c, cabeca);
    c->cur = fim;
}

/* len negativo (vreg I64) desvia ao frio forja_falha_alocacao(mensagem, len, arquivo, linha) (R0007); segue em bloco novo. */
static void ConferirLenNaoNegativo(BirCtx *c, const IrInstr *i, BirOperando novo)
{
    uint32_t negativo = BirComparar(c, BCOND_LT_S, novo, OpImed(0, BC_I64));
    uint32_t frio = BirNovoBloco(c, true), segue = BirNovoBloco(c, false), linha;
    BirOperando args[4];

    BirDesvia(c, OpVreg(negativo, BC_I8), frio, segue);
    c->cur = frio;
    args[0] = OpVreg(BirVregDoTexto(c, MSG_RESIZE_NEGATIVO, (uint32_t)strlen(MSG_RESIZE_NEGATIVO)), BC_I64);
    args[1] = novo;
    args[2] = BirVregDoArquivo(c, i->pos, &linha);
    args[3] = OpImed(linha, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, "forja_falha_alocacao"), args, 4);
    c->cur = segue;
}

/* Se novo > len: memset(ptr + len * tam, 0, (novo - len) * tam). ptr e lido depois do crescimento. */
static void ZerarElementosNovos(BirCtx *c, const BirListaRef *lista, uint32_t tam_elem, BirOperando novo)
{
    uint32_t len = CarregarMembro(c, lista, LISTA_MEMBRO_LEN, BC_I64);
    uint32_t cresce = BirComparar(c, BCOND_GT_S, novo, OpVreg(len, BC_I64));
    uint32_t zera = BirNovoBloco(c, false), segue = BirNovoBloco(c, false);
    uint32_t ptr, destino, quantos;

    BirDesvia(c, OpVreg(cresce, BC_I8), zera, segue);
    c->cur = zera;
    ptr = CarregarMembro(c, lista, LISTA_OFFSET_PTR, BC_I64);
    destino = ContaI64(c, BIR_ADD, OpVreg(ptr, BC_I64), OpVreg(ContaI64(c, BIR_MUL, OpVreg(len, BC_I64), OpImed(tam_elem, BC_I64)), BC_I64));
    quantos = ContaI64(c, BIR_MUL, OpVreg(ContaI64(c, BIR_SUB, novo, OpVreg(len, BC_I64)), BC_I64), OpImed(tam_elem, BC_I64));
    BirZeraDinamico(c, destino, OpVreg(quantos, BC_I64));
    BirSalta(c, segue);
    c->cur = segue;
}

/* cap(l): o membro cap do bloco da lista (a ou ^a), no destino (int). */
static bool RedimensionaCap(BirCtx *c, const IrInstr *i)
{
    BirListaRef lista;
    uint32_t dst;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirCarrega(c, dst, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_CAP));
    return true;
}

/* reserve(&l, cap = n): dev confere a epoca; cresce ate cap >= n (n <= cap, negativo inclusive, nao faz nada); len e conteudo ficam. i->tipo e o elemento. */
static bool RedimensionaReserva(BirCtx *c, const IrInstr *i)
{
    BirListaRef lista;
    BirSlot elemento;
    BirOperando novo;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirForma(c, i->tipo, &elemento) || !BirInteiro64(c, i, &i->b, &novo) || !BirOperandoVreg(c, novo, &novo))
        return false;
    if (c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, CarregarMembro(c, &lista, LISTA_MEMBRO_EPOCA, BC_I32));
    CrescerAte(c, i, &lista, elemento.tam, novo);
    return true;
}

/* resize(&l, len = n): dev confere a epoca; n < 0 falha R0007; cresce ate caber; os elementos novos saem zerados; len = n. i->tipo e o elemento. */
static bool RedimensionaResize(BirCtx *c, const IrInstr *i)
{
    BirListaRef lista;
    BirSlot elemento;
    BirOperando novo;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirForma(c, i->tipo, &elemento) || !BirInteiro64(c, i, &i->b, &novo) || !BirOperandoVreg(c, novo, &novo))
        return false;
    if (c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, CarregarMembro(c, &lista, LISTA_MEMBRO_EPOCA, BC_I32));
    ConferirLenNaoNegativo(c, i, novo);
    CrescerAte(c, i, &lista, elemento.tam, novo);
    ZerarElementosNovos(c, &lista, elemento.tam, novo);
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_LEN), novo);
    return true;
}

/* to_str(&buf): dev confere a epoca; qualquer alocador do builder (heap, frame, Arena); cresce ate cap > len, grava o NUL em ptr[len], o temp(str) = {ptr, len} (o bloco passa a ser da str, sem copia)
 * e a lista volta a {ptr = 0, len = 0, cap = 0} mantendo alocador e epoca. */
static bool RedimensionaParaStr(BirCtx *c, const IrInstr *i)
{
    BirListaRef lista;
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t str, len, ptr, fim;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirForma(c, TIPO_STR, &pedido.forma))
        return false;
    if (c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, CarregarMembro(c, &lista, LISTA_MEMBRO_EPOCA, BC_I32));
    len = CarregarMembro(c, &lista, LISTA_MEMBRO_LEN, BC_I64);
    CrescerAte(c, i, &lista, 1, OpVreg(ContaI64(c, BIR_ADD, OpVreg(len, BC_I64), OpImed(1, BC_I64)), BC_I64));
    ptr = CarregarMembro(c, &lista, LISTA_OFFSET_PTR, BC_I64);
    fim = ContaI64(c, BIR_ADD, OpVreg(ptr, BC_I64), OpVreg(len, BC_I64));
    BirEmitirGrava(c, fim, 0, OpImed(0, BC_I8));
    if (!BirDefinirSlot(c, &pedido, &str))
        return false;
    BirEmitirGrava(c, str,(int32_t)LayoutOffsetCampo(BirLayouts(c), TIPO_STR, 0), OpVreg(ptr, BC_I64));
    BirEmitirGrava(c, str, (int32_t)LayoutOffsetCampo(BirLayouts(c), TIPO_STR, FATIA_MEMBRO_LEN), OpVreg(len, BC_I64));
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_OFFSET_PTR), OpImed(0, BC_I64));
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_LEN), OpImed(0, BC_I64));
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_CAP), OpImed(0, BC_I64));
    return true;
}

/* IR_REDIMENSIONA por op_aux: REDIM_RESIZE (resize, make com len), REDIM_CAP (cap(l)), REDIM_RESERVA (reserve) e REDIM_PARA_STR (to_str). */
bool BirRegraRedimensiona(BirCtx *c, const IrInstr *i)
{
    if (i->op_aux == REDIM_CAP)
        return RedimensionaCap(c, i);
    if (i->op_aux == REDIM_RESERVA)
        return RedimensionaReserva(c, i);
    if (i->op_aux == REDIM_PARA_STR)
        return RedimensionaParaStr(c, i);
    return RedimensionaResize(c, i);
}

/* append(&l, items = fatia): dev confere a epoca; cresce ate len + fatia.len caber; memcpy(ptr + len * tam, fatia.ptr, fatia.len * tam); len += fatia.len. */
bool BirRegraAnexaFatia(BirCtx *c, const IrInstr *i)
{
    BirListaRef lista;
    BirSlot elemento;
    uint32_t fatia, origem = BirNovoVreg(c, BC_I64), quantos = BirNovoVreg(c, BC_I64);
    uint32_t len, total, ptr, destino;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirForma(c, i->tipo, &elemento) || !BirEnderecoDe(c, i, &i->b, &fatia))
        return false;
    BirEmitirCarrega(c, origem, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->b.tipo, 0));
    BirEmitirCarrega(c, quantos, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->b.tipo, FATIA_MEMBRO_LEN));
    if (c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, CarregarMembro(c, &lista, LISTA_MEMBRO_EPOCA, BC_I32));
    len = CarregarMembro(c, &lista, LISTA_MEMBRO_LEN, BC_I64);
    total = ContaI64(c, BIR_ADD, OpVreg(len, BC_I64), OpVreg(quantos, BC_I64));
    CrescerAte(c, i, &lista, elemento.tam, OpVreg(total, BC_I64));
    ptr = CarregarMembro(c, &lista, LISTA_OFFSET_PTR, BC_I64);
    destino = ContaI64(c, BIR_ADD, OpVreg(ptr, BC_I64), OpVreg(ContaI64(c, BIR_MUL, OpVreg(len, BC_I64), OpImed(elemento.tam, BC_I64)), BC_I64));
    BirCopiaDinamica(c, destino, origem, OpVreg(ContaI64(c, BIR_MUL, OpVreg(quantos, BC_I64), OpImed(elemento.tam, BC_I64)), BC_I64));
    BirEmitirGrava(c, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_LEN), OpVreg(total, BC_I64));
    return true;
}

/* sort(fatia): forja_ordenar(ptr, len, ORDENA_*) do runtime (rt_ordenar.c, ligado so quando o programa o referencia). */
bool BirRegraOrdena(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 3);
    uint32_t fatia, ptr = BirNovoVreg(c, BC_I64), quantos = BirNovoVreg(c, BC_I64);

    if (!BirEnderecoDe(c, i, &i->a, &fatia))
        return false;
    BirEmitirCarrega(c, ptr, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, 0));
    BirEmitirCarrega(c, quantos, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, FATIA_MEMBRO_LEN));
    args[0] = OpVreg(ptr, BC_I64);
    args[1] = OpVreg(quantos, BC_I64);
    args[2] = OpImed(i->op_aux, BC_I32);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_ordenar", BC_NADA, 3}, args);
    return true;
}

/* sort_by_key (IR_ORDENA_GERAL, CHAVE, K08b): forja_ordc_ordenar(ptr, len, tam, desloc, tipo_chave) do runtime (rt_ordenar.c); sem proc. */
static bool BirRegraOrdenaChave(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 5);
    BirSlot elemento;
    uint32_t fatia, ptr = BirNovoVreg(c, BC_I64), quantos = BirNovoVreg(c, BC_I64);

    if (!BirForma(c, i->tipo, &elemento) || !BirEnderecoDe(c, i, &i->a, &fatia))
        return false;
    BirEmitirCarrega(c, ptr, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, 0));
    BirEmitirCarrega(c, quantos, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, FATIA_MEMBRO_LEN));
    args[0] = OpVreg(ptr, BC_I64);
    args[1] = OpVreg(quantos, BC_I64);
    args[2] = OpImed(elemento.tam, BC_I64);
    args[3] = OpImed((int64_t)i->desloc_chave, BC_I64);
    args[4] = OpImed(i->tipo_chave, BC_I64);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_ordc_ordenar", BC_NADA, 5}, args);
    return true;
}

/* Chamada de runtime cujo retorno vai para o destino ja definido (dst): o BIR_CHAMA escreve nele direto. */
static void ChamarRuntimeEm(BirCtx *c, const BirChamadaRt *chamada, BirOperando *args, uint32_t dst)
{
    BirInstr *chama = BirEmitir(c, BIR_CHAMA, chamada->retorno);

    chama->simbolo = BirSimboloRuntime(c, chamada->funcao);
    chama->dst = dst;
    chama->args = args;
    chama->nargs = chamada->nargs;
}

/* Endereco do valor da busca: agregado (str) ja vive num local; escalar e gravado num slot de pilha so para a chamada (o runtime le por ponteiro). */
static bool EnderecoDoValorBusca(BirCtx *c, const IrInstr *i, uint32_t *endereco)
{
    BirSlot forma;
    BirOperando valor;
    uint32_t slot;

    if (BirEhAgregado(c->e->tipos, i->b.tipo))
        return BirEnderecoDe(c, i, &i->b, endereco);
    if (!BirForma(c, i->b.tipo, &forma) || !BirOperandoDe(c, i, &i->b, &valor))
        return false;
    slot = BirNovoSlot(c, forma.tam, forma.alinhamento);
    *endereco = BirNovoVreg(c, BC_I64);
    BirEmitirEndSlot(c, slot, *endereco);
    BirEmitirGrava(c, *endereco, 0, valor);
    return true;
}

/* binary_search (IR_BUSCA_BINARIA, K08b): forja_ordc_posicao (BUSCA_POSICAO, int) ou forja_ordc_achou (BUSCA_ACHOU, bool) sobre ptr/len do slice e o endereco do valor. */
bool BirRegraBuscaBinaria(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 5);
    BirSlot elemento;
    uint8_t classe = i->op_aux == BUSCA_ACHOU ? BC_I8 : BC_I64;
    uint32_t fatia, valor, dst, ptr = BirNovoVreg(c, BC_I64), quantos = BirNovoVreg(c, BC_I64);

    if (!BirForma(c, (TipoId)TipoDe(c->e->tipos, i->a.tipo)->a, &elemento) || !BirEnderecoDe(c, i, &i->a, &fatia) || !EnderecoDoValorBusca(c, i, &valor) || !BirDefinirDestino(c, classe, &dst))
        return false;
    BirEmitirCarrega(c, ptr, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, 0));
    BirEmitirCarrega(c, quantos, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, FATIA_MEMBRO_LEN));
    args[0] = OpVreg(ptr, BC_I64);
    args[1] = OpVreg(quantos, BC_I64);
    args[2] = OpImed(elemento.tam, BC_I64);
    args[3] = OpVreg(valor, BC_I64);
    args[4] = OpImed(i->tipo_chave, BC_I64);
    ChamarRuntimeEm(c, &(BirChamadaRt){i->op_aux == BUSCA_ACHOU ? "forja_ordc_achou" : "forja_ordc_posicao", classe, 5}, args, dst);
    return true;
}

/* sort_by(s, less = p) (IR_ORDENA_GERAL, ESTAVEL): forja_ordenar_estavel(ptr, len, tam, p) do runtime; tam = o tamanho de T (i->tipo); p = o trampolim fjt_ do proc.
 * O modo CHAVE (sort_by_key, K08b) vai para BirRegraOrdenaChave. */
bool BirRegraOrdenaGeral(BirCtx *c, const IrInstr *i)
{
    BirOperando *args, comparador;
    if (i->op_aux == ORDENA_GERAL_CHAVE)
        return BirRegraOrdenaChave(c, i);
    args = BirArgs(c, 4);
    BirSlot elemento;
    uint32_t fatia, ptr = BirNovoVreg(c, BC_I64), quantos = BirNovoVreg(c, BC_I64);

    if (!BirForma(c, i->tipo, &elemento) || !BirEnderecoDe(c, i, &i->a, &fatia) || !BirComparadorDoRuntime(c, i, &i->b, &comparador))
        return false;
    BirEmitirCarrega(c, ptr, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, 0));
    BirEmitirCarrega(c, quantos, fatia, (int32_t)LayoutOffsetCampo(BirLayouts(c), i->a.tipo, FATIA_MEMBRO_LEN));
    args[0] = OpVreg(ptr, BC_I64);
    args[1] = OpVreg(quantos, BC_I64);
    args[2] = OpImed(elemento.tam, BC_I64);
    args[3] = comparador;
    BirChamarRuntime(c, &(BirChamadaRt){"forja_ordenar_estavel", BC_NADA, 4}, args);
    return true;
}

/* heap_push / heap_pop (IR_HEAP, K17): forja_heap_subir (SUBIR) ou forja_heap_retira_topo (RETIRA_TOPO) sobre ptr/len da lista (BirListaDe, como o append),
 * com o tamanho de T, o less (trampolim fjt_ do proc, nos _BY) ou 0 (ordem do sort) e o ORDENA_* de T. Mesma assinatura do C (forja_rt_heap.h). */
bool BirRegraHeap(BirCtx *c, const IrInstr *i)
{
    BirOperando *args = BirArgs(c, 5), comparador = OpImed(0, BC_I64);
    BirListaRef lista;
    BirSlot elemento;
    bool com_less = i->op_aux == HEAP_SUBIR_BY || i->op_aux == HEAP_RETIRA_TOPO_BY;
    bool retira = i->op_aux == HEAP_RETIRA_TOPO || i->op_aux == HEAP_RETIRA_TOPO_BY;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirForma(c, i->tipo, &elemento))
        return false;
    if (com_less && !BirComparadorDoRuntime(c, i, &i->b, &comparador))
        return false;
    args[0] = OpVreg(CarregarMembro(c, &lista, LISTA_MEMBRO_PTR, BC_I64), BC_I64);
    args[1] = OpVreg(CarregarMembro(c, &lista, LISTA_MEMBRO_LEN, BC_I64), BC_I64);
    args[2] = OpImed(elemento.tam, BC_I64);
    args[3] = comparador;
    args[4] = OpImed(i->tipo_chave, BC_I64);
    BirChamarRuntime(c, &(BirChamadaRt){retira ? "forja_heap_retira_topo" : "forja_heap_subir", BC_NADA, 5}, args);
    return true;
}
