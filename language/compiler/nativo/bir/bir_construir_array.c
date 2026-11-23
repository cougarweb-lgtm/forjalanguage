/* bir_construir_array.c - arrays [N]T, fatias []T e slice_from na construcao da BIR: IR_ARRAY, IR_ELEMENTO, IR_ENDERECO_ELEMENTO, IR_FATIA e IR_FATIA_DE_PONTEIRO.
 * Versao: 0.3.5
 * Changelog: 0.3.5 - 0.21.0 F5-a: IndiceDoElemento perde o ramo morto do literal (BirIndiceNoLimite): o passe checagem ja prova o literal; codigo gerado igual byte a byte.
 *            0.3.4 - P2: indice literal dentro do tamanho fixo do array fica imediato no release (BirIndiceNoLimite): sem vreg nem conferencia; dev identico.
 *            0.3.3 - O0 P13: indice, fatia e vetor C conferidos tambem no release com --checagem sim (BirConfereLimites); a epoca R0008 segue so em dev.
 *            0.3.2 - M3 F9b-3: decisoes extraidas de EnderecoDoElemento, BirRegraFatia, LiteralDeArray e BirRegraArray.
 *            0.3.1 - M2 P17b: IR_ARRAY de soa e de map sem elementos ('{}') e o bloco zerado (ZERA_MEM), como o (T){0} do C.
 *            0.3.0 - M2 P16: BirConferirIndice (a conferencia de indice de dev, antes static) passa a publica para matrix[i, j]; IR_ARRAY de matrix sem elementos ('{}') e a matriz zerada (ZERA_MEM).
 *            0.2.0 - M2 P14b: bases de lista ([dynamic]T e ^[dynamic]T: BirListaDe, ptr/len/epoca carregados do bloco de 32 B). Dev: indice por forja_indice_lista (epoca R0008 + R0006, devolve o indice) salvo op_aux 1;
 *                    fatia chama forja_lista_viva antes do forja_fatia_confere; BirTamanhoDeLista (len); BirInteiro64 passa a publica.
 *                    IR_ARRAY de [dynamic]T sem elementos ('{}') e a lista zerada (ZERA_MEM de 32 B).
 *            0.1.0 - M2 P13b: [N]T e []T sao agregados (BirEhAgregado) que viajam como endereco. [N]T = o proprio bloco de N elementos (membro v no offset 0 do layout/); []T = {ptr, len}
 *                    de 16 B (como o str). Elemento de [N]T por valor ou ^[N]T = base + i * tam; de []T = [slot+ptr] + i * tam. Dev (op_aux != 1) confere o indice por CMP sem sinal contra o
 *                    tamanho (pega o negativo) e desvia ao frio forja_falha_indice (R0006); a fatia chama forja_fatia_confere antes de montar {ptr + lo * tam, len}; slice_from chama
 *                    forja_vetor_c_confere (R0006 len < 0, R0005 nulo com len > 0). Release nao confere nada. [dynamic]T e do 0.2.0. Nenhum op novo na BIR.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/array
 *
 * Os offsets de ptr e len da fatia vem do layout/ (LayoutOffsetCampo do proprio tipo da fatia).
 */
#include "nativo/bir/bir_interno.h"

#define FATIA_CAMPO_PTR 0u
#define FATIA_CAMPO_LEN 1u

/* A base de um acesso: dados = vreg I64 com o endereco do 1o elemento; n = quantidade de elementos (imediato ou vreg I64); elemento e o tipo dele.
 * lista: base [dynamic]T; em dev, epoca = vreg I32 com a epoca do frame da lista (so o frame usa). */
typedef struct { uint32_t dados; BirOperando n; TipoId elemento; bool lista; uint32_t epoca; } BirBaseIndexavel;

static uint32_t OffsetDaFatia(BirCtx *c, TipoId fatia, uint32_t campo)
{
    return LayoutOffsetCampo(BirLayouts(c), fatia, campo);
}

/* Inteiro de 64 bits do valor (indice, limite ou len): imediato fica imediato, vreg curto alarga pelo sinal do TIPO (sem sinal zera). */
bool BirInteiro64(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida)
{
    BirOperando o;

    if (!BirOperandoDe(c, i, v, &o))
        return false;
    if (o.classe == BC_I64) {
        *saida = o;
        return true;
    }
    if (o.tipo == BO_IMED) {
        *saida = OpImed(o.imed, BC_I64);
        return true;
    }
    *saida = OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = TipoEhSemSinal(v->tipo) ? BIR_EXT_Z : BIR_EXT_S, .a = o}), BC_I64);
    return true;
}

static bool Inteiro64EmVreg(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida)
{
    return BirInteiro64(c, i, v, saida) && BirOperandoVreg(c, *saida, saida);
}

/* []T: ptr e len carregados do slot da fatia. */
static bool BaseDeFatia(BirCtx *c, const IrInstr *i, const Valor *v, BirBaseIndexavel *base)
{
    uint32_t endereco, len;

    if (!BirEnderecoDe(c, i, v, &endereco))
        return false;
    base->dados = BirNovoVreg(c, BC_I64);
    len = BirNovoVreg(c, BC_I64);
    BirEmitirCarrega(c, base->dados, endereco, (int32_t)OffsetDaFatia(c, v->tipo, FATIA_CAMPO_PTR));
    BirEmitirCarrega(c, len, endereco, (int32_t)OffsetDaFatia(c, v->tipo, FATIA_CAMPO_LEN));
    base->n = OpVreg(len, BC_I64);
    base->elemento = TipoDe(c->e->tipos, v->tipo)->a;
    return true;
}

/* ^[N]T: o proprio ponteiro aponta o 1o elemento e N e constante do tipo. */
static bool BaseDePonteiroDeArray(BirCtx *c, const IrInstr *i, const Valor *v, BirBaseIndexavel *base)
{
    TipoId array = TipoDe(c->e->tipos, v->tipo)->a;
    BirOperando ponteiro;

    if (!BirOperandoDe(c, i, v, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    base->dados = ponteiro.id;
    base->n = OpImed(TipoDe(c->e->tipos, array)->b, BC_I64);
    base->elemento = TipoDe(c->e->tipos, array)->a;
    return true;
}

/* [N]T por valor (local ou temporario): o endereco do slot e o 1o elemento. */
static bool BaseDeArray(BirCtx *c, const IrInstr *i, const Valor *v, BirBaseIndexavel *base)
{
    if (!BirEnderecoDe(c, i, v, &base->dados))
        return false;
    base->n = OpImed(TipoDe(c->e->tipos, v->tipo)->b, BC_I64);
    base->elemento = TipoDe(c->e->tipos, v->tipo)->a;
    return true;
}

/* ---- [dynamic]T ---- */

uint32_t BirOffsetDaLista(BirCtx *c, const BirListaRef *lista, uint32_t membro)
{
    return LayoutOffsetCampo(BirLayouts(c), lista->tipo, membro);
}

/* [dynamic]T por valor (o slot) ou ^[dynamic]T (o ponteiro): o endereco do bloco {ptr, len, cap, alocador, epoca}. */
bool BirListaDe(BirCtx *c, const IrInstr *i, const Valor *v, BirListaRef *lista)
{
    const Tipos *tipos = c->e->tipos;
    BirOperando ponteiro;

    if (TipoClasseDe(tipos, v->tipo) == TC_DINAMICO) {
        lista->tipo = v->tipo;
        return BirEnderecoDe(c, i, v, &lista->endereco);
    }
    if (TipoClasseDe(tipos, v->tipo) != TC_PONTEIRO || TipoClasseDe(tipos, TipoDe(tipos, v->tipo)->a) != TC_DINAMICO)
        return BirFalhaTipo(c, i, "-", v->tipo);
    lista->tipo = TipoDe(tipos, v->tipo)->a;
    if (!BirOperandoDe(c, i, v, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro))
        return false;
    lista->endereco = ponteiro.id;
    return true;
}

/* forja_lista_viva(epoca, arquivo, linha): so dev; R0008 se a lista do frame sobreviveu a um frame_reset. */
void BirChamarListaViva(BirCtx *c, const IrInstr *i, uint32_t epoca)
{
    BirOperando *args = BirArgs(c, 3);
    uint32_t linha;

    args[0] = OpVreg(epoca, BC_I32);
    args[1] = BirVregDoArquivo(c, i->pos, &linha);
    args[2] = OpImed(linha, BC_I32);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_lista_viva", BC_NADA, 3}, args);
}

/* len(l) de [dynamic]T ou ^[dynamic]T: o campo len do bloco. */
bool BirTamanhoDeLista(BirCtx *c, const IrInstr *i)
{
    BirListaRef lista;
    uint32_t dst;

    if (!BirListaDe(c, i, &i->a, &lista) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirCarrega(c, dst, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_LEN));
    return true;
}

/* ptr e len carregados do bloco; em dev tambem a epoca (I32). */
static bool BaseDeLista(BirCtx *c, const IrInstr *i, const Valor *v, BirBaseIndexavel *base)
{
    BirListaRef lista;
    uint32_t len;

    if (!BirListaDe(c, i, v, &lista))
        return false;
    base->dados = BirNovoVreg(c, BC_I64);
    len = BirNovoVreg(c, BC_I64);
    BirEmitirCarrega(c, base->dados, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, 0));
    BirEmitirCarrega(c, len, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_LEN));
    base->n = OpVreg(len, BC_I64);
    base->elemento = TipoDe(c->e->tipos, lista.tipo)->a;
    base->lista = true;
    if (c->p->modo == BM_DEV) {
        base->epoca = BirNovoVreg(c, BC_I32);
        BirEmitirCarrega(c, base->epoca, lista.endereco, (int32_t)BirOffsetDaLista(c, &lista, LISTA_MEMBRO_EPOCA));
    }
    return true;
}

static bool EhBaseDeLista(const Tipos *tipos, TipoId tipo)
{
    TipoClasse classe = TipoClasseDe(tipos, tipo);

    return classe == TC_DINAMICO || (classe == TC_PONTEIRO && TipoClasseDe(tipos, TipoDe(tipos, tipo)->a) == TC_DINAMICO);
}

static bool BaseIndexavelDe(BirCtx *c, const IrInstr *i, const Valor *v, BirBaseIndexavel *base)
{
    const Tipos *tipos = c->e->tipos;
    TipoClasse classe = TipoClasseDe(tipos, v->tipo);

    *base = (BirBaseIndexavel){0};
    if (classe == TC_ARRAY)
        return BaseDeArray(c, i, v, base);
    if (classe == TC_FATIA)
        return BaseDeFatia(c, i, v, base);
    if (classe == TC_PONTEIRO && TipoClasseDe(tipos, TipoDe(tipos, v->tipo)->a) == TC_ARRAY)
        return BaseDePonteiroDeArray(c, i, v, base);
    if (EhBaseDeLista(tipos, v->tipo))
        return BaseDeLista(c, i, v, base);
    return BirFalhaTipo(c, i, "-", v->tipo);
}

/* ---- indice ---- */

/* Bloco frio: forja_falha_indice(i, n, arquivo, linha); o arquivo e a linha saem no proprio frio. */
static void FrioIndice(BirCtx *c, const IrInstr *i, uint32_t frio, const BirOperando ab[2])
{
    BirOperando args[4];
    uint32_t salvo = c->cur, linha;

    c->cur = frio;
    args[0] = ab[0];
    args[1] = ab[1];
    args[2] = BirVregDoArquivo(c, i->pos, &linha);
    args[3] = OpImed(linha, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, "forja_falha_indice"), args, 4);
    c->cur = salvo;
}

/* Dev, op_aux != 1: indice (vreg I64) >= n sem sinal (o negativo vira enorme) desvia ao frio. Publica: matrix[i, j] confere linha e coluna com ela. */
void BirConferirIndice(BirCtx *c, const IrInstr *i, BirOperando indice, BirOperando n)
{
    uint32_t fora = BirComparar(c, BCOND_GE_U, indice, n);
    uint32_t frio = BirNovoBloco(c, true);
    uint32_t segue = BirNovoBloco(c, false);

    BirDesvia(c, OpVreg(fora, BC_I8), frio, segue);
    FrioIndice(c, i, frio, (BirOperando[2]){indice, n});
    c->cur = segue;
}

/* Dev, lista, op_aux != 1: forja_indice_lista(i, n, epoca, arquivo, linha) confere a epoca do frame (R0008) e o indice (R0006) e devolve o indice. */
static BirOperando ConferirIndiceLista(BirCtx *c, const IrInstr *i, const BirBaseIndexavel *base, BirOperando indice)
{
    BirOperando *args = BirArgs(c, 5);
    uint32_t linha;

    args[0] = indice;
    args[1] = base->n;
    args[2] = OpVreg(base->epoca, BC_I32);
    args[3] = BirVregDoArquivo(c, i->pos, &linha);
    args[4] = OpImed(linha, BC_I32);
    return OpVreg(BirChamarRuntime(c, &(BirChamadaRt){"forja_indice_lista", BC_I64, 5}, args), BC_I64);
}

/* dados + indice * tam num vreg novo; indice imediato soma a constante, tam 1 dispensa o MUL. */
static uint32_t SomarDeslocamento(BirCtx *c, const BirBaseIndexavel *base, BirOperando indice, uint32_t tam)
{
    BirOperando deslocamento;

    if (indice.tipo == BO_IMED)
        deslocamento = OpImed(indice.imed * (int64_t)tam, BC_I64);
    else if (tam == 1)
        deslocamento = indice;
    else
        deslocamento = OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = BIR_MUL, .a = indice, .b = OpImed(tam, BC_I64)}), BC_I64);
    if (deslocamento.tipo == BO_IMED && deslocamento.imed == 0)
        return base->dados;
    return BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(base->dados, BC_I64), .b = deslocamento});
}

/* Dev (op_aux != 1): indice em vreg e conferido; senao o indice como vier. */
static bool IndiceDoElemento(BirCtx *c, const IrInstr *i, const BirBaseIndexavel *base, BirOperando *indice)
{
    if (!BirConfereLimites(c) || i->op_aux == 1)
        return BirInteiro64(c, i, &i->b, indice);
    if (!BirInteiro64(c, i, &i->b, indice))
        return false;
    if (!BirOperandoVreg(c, *indice, indice))
        return false;
    if (base->lista && c->p->modo == BM_DEV)   /* a epoca (R0008) so existe em dev; o release confere pelo BirConferirIndice */
        *indice = ConferirIndiceLista(c, i, base, *indice);
    else
        BirConferirIndice(c, i, *indice, base->n);
    return true;
}

/* Base + indice (conferido em dev) + endereco do elemento: o miolo de IR_ELEMENTO e IR_ENDERECO_ELEMENTO. */
static bool EnderecoDoElemento(BirCtx *c, const IrInstr *i, uint32_t *endereco)
{
    BirBaseIndexavel base;
    BirOperando indice;
    BirSlot forma;

    if (!BaseIndexavelDe(c, i, &i->a, &base) || !BirForma(c, base.elemento, &forma))
        return false;
    if (!IndiceDoElemento(c, i, &base, &indice))
        return false;
    *endereco = SomarDeslocamento(c, &base, indice, forma.tam);
    return true;
}

static bool ElementoAgregado(BirCtx *c, const IrInstr *i, uint32_t endereco)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t dst;

    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirCopiaBloco(c, dst, endereco, pedido.forma.tam);
    return true;
}

/* temp = copia do elemento: escalar = CARREGA, agregado (struct, array, fatia, str...) = COPIA_MEM para o slot do temporario. */
bool BirRegraElemento(BirCtx *c, const IrInstr *i)
{
    uint8_t classe = BC_NADA;
    uint32_t endereco, dst;

    if (!EnderecoDoElemento(c, i, &endereco))
        return false;
    if (BirEhAgregado(c->e->tipos, i->tipo))
        return ElementoAgregado(c, i, endereco);
    if (!BirClasseDoTipo(c->e->tipos, i->tipo, &classe))
        return BirFalhaTipo(c, i, "-", i->tipo);
    if (!BirDefinirDestino(c, classe, &dst))
        return false;
    BirEmitirCarrega(c, dst, endereco, 0);
    return true;
}

bool BirRegraEnderecoElemento(BirCtx *c, const IrInstr *i)
{
    uint32_t endereco, dst;

    if (!EnderecoDoElemento(c, i, &endereco) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_COPIA, .dst = dst, .a = OpVreg(endereco, BC_I64)});
    return true;
}

/* ---- fatia ---- */

/* forja_fatia_confere(lo, hi, exclusiva, n, arquivo, linha): volta se os limites estao certos. */
static void ChamarConfereFatia(BirCtx *c, const IrInstr *i, const BirBaseIndexavel *base, const BirOperando limites[2])
{
    BirOperando *args = BirArgs(c, 6);
    BirInstr *chama;
    uint32_t linha;

    args[0] = limites[0];
    args[1] = limites[1];
    args[2] = OpImed(i->faixa_excl ? 1 : 0, BC_I8);
    args[3] = base->n;
    args[4] = BirVregDoArquivo(c, i->pos, &linha);
    args[5] = OpImed(linha, BC_I32);
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, "forja_fatia_confere");
    chama->args = args;
    chama->nargs = 6;
}

/* len = hi - lo (+ 1 na faixa inclusiva). */
static BirOperando TamanhoDaFatia(BirCtx *c, const IrInstr *i, const BirOperando limites[2])
{
    uint32_t len = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_SUB, .a = limites[1], .b = limites[0]});

    if (i->faixa_excl)
        return OpVreg(len, BC_I64);
    return OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(len, BC_I64), .b = OpImed(1, BC_I64)}), BC_I64);
}

/* Grava {ptr, len} no slot do temporario de destino. */
static bool GravarFatia(BirCtx *c, const IrInstr *i, BirOperando ptr, BirOperando len)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t dst;

    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirEmitirGrava(c, dst, (int32_t)OffsetDaFatia(c, i->tipo, FATIA_CAMPO_PTR), ptr);
    BirEmitirGrava(c, dst, (int32_t)OffsetDaFatia(c, i->tipo, FATIA_CAMPO_LEN), len);
    return true;
}

/* Dev: lista viva (epoca) e, salvo op_aux 1, os limites da fatia. */
static void ConferirFatiaDev(BirCtx *c, const IrInstr *i, const BirBaseIndexavel *base, const BirOperando limites[2])
{
    if (!BirConfereLimites(c))
        return;
    if (base->lista && c->p->modo == BM_DEV)
        BirChamarListaViva(c, i, base->epoca);
    if (i->op_aux != 1)
        ChamarConfereFatia(c, i, base, limites);
}

/* temp([]T) = a[lo ..< hi] ou a[lo ..= hi]: {dados + lo * tam, len}. Dev (op_aux != 1) confere os limites antes. */
bool BirRegraFatia(BirCtx *c, const IrInstr *i)
{
    BirBaseIndexavel base;
    BirOperando limites[2];
    BirSlot forma;

    if (i->nargs != 1)
        return BirFalhaInconsistente(c, "IR_FATIA sem o limite final");
    if (!BaseIndexavelDe(c, i, &i->a, &base) || !BirForma(c, base.elemento, &forma))
        return false;
    if (!Inteiro64EmVreg(c, i, &i->b, &limites[0]) || !Inteiro64EmVreg(c, i, &i->args[0], &limites[1]))
        return false;
    ConferirFatiaDev(c, i, &base, limites);
    return GravarFatia(c, i, OpVreg(SomarDeslocamento(c, &base, limites[0], forma.tam), BC_I64), TamanhoDaFatia(c, i, limites));
}

/* ---- slice_from ---- */

/* forja_vetor_c_confere(p, n, arquivo, linha): volta se n >= 0 e (p != nulo ou n == 0). */
static void ChamarConfereVetorC(BirCtx *c, const IrInstr *i, BirOperando ponteiro, BirOperando len)
{
    BirOperando *args = BirArgs(c, 4);
    BirInstr *chama;
    uint32_t linha;

    args[0] = ponteiro;
    args[1] = len;
    args[2] = BirVregDoArquivo(c, i->pos, &linha);
    args[3] = OpImed(linha, BC_I32);
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, "forja_vetor_c_confere");
    chama->args = args;
    chama->nargs = 4;
}

/* temp([]T) = {ptr = a, len = b} de um vetor C (a: ^T ou ?^T). Dev confere len < 0 (R0006) e nulo com len > 0 (R0005); release nao confere. */
bool BirRegraFatiaDePonteiro(BirCtx *c, const IrInstr *i)
{
    BirOperando ponteiro, len;

    if (!BirOperandoDe(c, i, &i->a, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro) || !BirInteiro64(c, i, &i->b, &len))
        return false;
    if (BirConfereLimites(c))
        ChamarConfereVetorC(c, i, ponteiro, len);
    return GravarFatia(c, i, ponteiro, len);
}

/* ---- IR_ARRAY ---- */

/* Grava cada args[k] no elemento k do bloco: molde traz base e tipo do elemento (offset 0). */
static bool GravarElementos(BirCtx *c, const IrInstr *i, BirAlvoMem molde, uint32_t tam_elemento)
{
    for (uint32_t k = 0; k < i->nargs; k++) {
        BirAlvoMem alvo = {.base = molde.base, .offset = k * tam_elemento, .tipo = molde.tipo};

        if (!BirGravarEm(c, i, &alvo, &i->args[k]))
            return false;
    }
    return true;
}

/* [N]T: zera o bloco (nargs 0 ou menor que N) e grava cada args[k] no elemento k. */
static bool LiteralDeArray(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    TipoId elemento = TipoDe(c->e->tipos, i->tipo)->a;
    BirSlot forma_elemento;
    uint32_t dst;

    if (!BirForma(c, i->tipo, &pedido.forma) || !BirForma(c, elemento, &forma_elemento) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    if (i->nargs < TipoDe(c->e->tipos, i->tipo)->b)
        BirZeraBloco(c, dst, pedido.forma.tam);
    return GravarElementos(c, i, (BirAlvoMem){.base = dst, .tipo = elemento}, forma_elemento.tam);
}

/* []T com nargs 0: a fatia vazia {NULL, 0}. */
static bool FatiaVazia(BirCtx *c, const IrInstr *i)
{
    return GravarFatia(c, i, OpImed(0, BC_I64), OpImed(0, BC_I64));
}

/* [dynamic]T com nargs 0 ('{}'): a lista zerada, sem alocador (o primeiro append falha R0007); o C escreve (T){0}. */
static bool ListaZerada(BirCtx *c, const IrInstr *i)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t dst;

    if (!BirForma(c, i->tipo, &pedido.forma) || !BirDefinirSlot(c, &pedido, &dst))
        return false;
    BirZeraBloco(c, dst, pedido.forma.tam);
    return true;
}

bool BirRegraArray(BirCtx *c, const IrInstr *i)
{
    switch (TipoClasseDe(c->e->tipos, i->tipo)) {
    case TC_ARRAY:
        return LiteralDeArray(c, i);
    case TC_FATIA:
        if (i->nargs == 0)
            return FatiaVazia(c, i);
        break;
    case TC_DINAMICO: case TC_MATRIZ: case TC_SOA: case TC_MAPA:
        if (i->nargs == 0)
            return ListaZerada(c, i);
        break;
    default:
        break;
    }
    return BirFalhaTipo(c, i, "-", i->tipo);
}
