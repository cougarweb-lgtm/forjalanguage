/* bir_construir_chamada.c - IR -> BIR: assinatura da funcao, chamada de proc e retorno, com struct (agregado) por valor.
 * Versao: 0.9.4
 * Changelog: 0.9.4 - Linguagem v1.13 L1a-ir: BirArquivoDoModulo vira BirArquivoDoTrecho (o IrExterno guarda o trecho da Fonte, nao o modulo).
 *            0.9.3 - O0 P13: retorno nulo de foreign e proc nulo na chamada indireta conferidos tambem no release com --checagem sim (BirConfereLimites).
 *            0.9.2 - M6 P71b: a lista SOMEM ganhou track_handle/untrack_handle (so em bir_alcance.c); nenhuma mudanca de codigo aqui, o aviso vale: em release os argumentos delas nao tem efeito (sem rastreio).
 *            0.9.1 - M6 P71a (correcao): o destino da chamada SOMEM omitida nasce zerado (BirZeraBloco / mov 0); antes ficava com lixo da pilha e `s.vivos` em release lia lixo.
 *            0.9.0 - M6 P71a: BirRegraChamada em release omite a chamada SOMEM (BirNomeSomem: snapshot e report_since de std:mem); so o destino nasce.
 *            0.8.1 - M5b P39-B4: SimboloDoImport nao troca "time" por "_time64" no alvo linux (c->p->sysv): o libc.a do musl tem "time" e nao tem "_time64".
 *            0.8.0 - M5b P38c: CHAMA_IND no alvo linux leva o descritor SysV de cada agregado por valor e do retorno agregado (AnexarDescritoresSysvDoValor);
 *                    sem descritor segue Win64, como o trampolim da mesma assinatura (AssinaturaSysv em bir_construir_fronteira.c).
 *            0.7.0 - M5b P37: AnexarDescritoresSysv e o retorno de foreign no alvo linux levam o BirSysv (BirDescritorSysv); X0001 sem descritor.
 *            0.6.1 - M3 F9b-3: decisoes extraidas de ChamarEscalar, BirRegraChamada e BirRegraChamadaInd.
 *            0.6.0 - M3 P21v-c: BirRegraChamadaInd (IR_CHAMADA_IND -> BIR_CHAMA_IND: alvo num vreg I64, dev confere nulo com R0005 "chamada por proc nulo"; argumentos estreitos estendidos, struct por copia, retorno agregado como no foreign); ConstruirArgs recebe de_fora.
 *            0.5.0 - M3 P21: BirEstenderEstreito, BirArquivoDoModulo e BirFalhaSeNulo (o R0005 de GuardaRetornoForeign, mesmos bytes) saem publicos para o trampolim fjt_ (bir_construir_fronteira.c).
 *            0.4.1 - M3 P19c: SimboloDoImport: "time" (thunk do mingw para _time64, que a ucrtbase exporta) importa _time64 quando todos os 'link' sao do CRT; antes caia na msvcrt.dll real.
 *            0.4.0 - M3 P19: chamada a foreign de fora = CHAMA ao BS_IMPORT (__imp_<simbolo C>); argumentos bool/i8/i16/u8/u16 com BE_SINAL/BE_ZERO; retorno ^T/cstr NULL em dev = R0005 ("retorno do proc foreign '<nome>'",
 *                    arquivo do modulo e linha do 'proc', o texto do involucro fj_ do backend C). O retorno estreito nao precisa de re-extensao aqui: o destino e um vreg da classe estreita (slot de 8/16/32 bits) e a carga alarga pelo tipo.
 *            0.3.0 - M2 P14b: BirExternoDe e chamada a foreign INTERNO da std (std:mem: forja_arena_*, forja_pool_*): CHAMA direto ao simbolo forja_* do runtime (foreign de fora segue X0001, marco M3).
 *            0.2.1 - M2 P13a: ArgumentoAgregado vira BirArgumentoAgregado (publica; bir_construir_texto.c passa str por valor ao runtime). Mesmo codigo.
 *            0.2.0 - M2 P12a: o resultado T ! E (BirEhAgregado) passa e retorna por valor como a struct (parametro, argumento, retorno).
 *            0.1.0 - M2 P11b: o trecho de chamada/retorno/assinatura sai do bir_construir.c e ganha o agregado por valor. Argumento struct = copia para um slot PRIVADO (alinhado a 16) e
 *                    o endereco da copia viaja no arg com tam (o chamado pode alterar a copia); parametro struct = vreg I64 com o endereco (param_tam); retorno struct = o chamador
 *                    passa o endereco de destino em CHAMA.a (tam) e a funcao devolve RETORNA.a = endereco da origem (retorno_tam). Registrador x ponteiro x ponteiro oculto: x64_abi_win.c.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/abi
 */
#include <stdio.h>
#include <string.h>

#include "common/nome_modulo.h"
#include "nativo/bir/bir_interno.h"

#define MARCO_ASSINATURA "M2"
#define CAUSA_CHAMADA_MAX 400
#define TEXTO_FOREIGN_MAX 256
#define TEXTO_RETORNO_FOREIGN "retorno do proc foreign '%s'"
#define MARCO_SYSV "M5b"

/* Classe do vreg e bytes do agregado de um valor que cruza a fronteira de um proc (tam 0 = escalar). */
typedef struct { uint8_t classe; uint32_t tam; } Passagem;

bool BirTipoPassavel(const Tipos *tipos, TipoId tipo)
{
    uint8_t classe;

    return BirClasseDoTipo(tipos, tipo, &classe) || BirEhAgregado(tipos, tipo);
}

/* O tipo ja passou por BirTipoPassavel. Struct: classe BC_I64 (o endereco) e o tamanho do layout/; falha so no X0904 de BirForma. */
static bool PassagemDe(BirCtx *c, TipoId tipo, Passagem *saida)
{
    BirSlot forma;

    *saida = (Passagem){0};
    if (BirClasseDoTipo(c->e->tipos, tipo, &saida->classe))
        return true;
    if (!BirForma(c, tipo, &forma))
        return false;
    *saida = (Passagem){.classe = BC_I64, .tam = forma.tam};
    return true;
}

/* ---- assinatura ---- */

static bool PassagemDaAssinatura(BirCtx *c, const char *variante, TipoId tipo, Passagem *saida)
{
    if (!BirTipoPassavel(c->e->tipos, tipo))
        return BirFalhaX0001(c, c->f->pos, (const char *const[3]){"IR_FUNCAO", variante, BirNomeDoTipo(c->e->tipos, tipo)}, MARCO_ASSINATURA);
    return PassagemDe(c, tipo, saida);
}

static void GravarTamanhoDoParametro(BirCtx *c, uint32_t nparams, uint32_t k, uint32_t tam)
{
    if (tam == 0)
        return;
    if (c->f->param_tam == NULL) {
        c->f->param_tam = ArenaAlocar(&c->p->arena, nparams * sizeof *c->f->param_tam);
        memset(c->f->param_tam, 0, nparams * sizeof *c->f->param_tam);
    }
    c->f->param_tam[k] = tam;
}

/* Parametros viram os vregs 0..n-1 (agregado: o endereco, classe BC_I64); o retorno vazio e BC_NADA. */
bool BirPrepararAssinatura(BirCtx *c, const IrFuncao *irf)
{
    Passagem p = {0};

    c->f->retorno = BC_NADA;
    if (irf->retorno != TIPO_VAZIO) {
        if (!PassagemDaAssinatura(c, "retorno", irf->retorno, &p))
            return false;
        c->f->retorno = p.classe;
        c->f->retorno_tam = p.tam;
    }
    for (uint32_t k = 0; k < irf->nparams; k++) {
        if (!PassagemDaAssinatura(c, "parametro", irf->params[k].tipo, &p))
            return false;
        BirNovoVreg(c, p.classe);
        GravarTamanhoDoParametro(c, irf->nparams, k, p.tam);
    }
    return true;
}

/* ---- chamada ---- */

/* Agregado por valor (struct, resultado, str): copia para um slot privado (a copia e do chamado) e devolve o endereco dela com o tam. */
bool BirArgumentoAgregado(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida)
{
    BirSlot forma;
    uint32_t origem, dst, slot;

    if (!BirForma(c, v->tipo, &forma) || !BirEnderecoDe(c, i, v, &origem))
        return false;
    slot = BirNovoSlot(c, forma.tam, forma.alinhamento > BIR_SLOT_ALINHAMENTO_MAX ? forma.alinhamento : BIR_SLOT_ALINHAMENTO_MAX);
    dst = BirNovoVreg(c, BC_I64);
    BirEmitirEndSlot(c, slot, dst);
    BirCopiaBloco(c, dst, origem, forma.tam);
    *saida = OpVreg(dst, BC_I64);
    saida->tam = forma.tam;
    return true;
}

/* O foreign de fora (biblioteca de 'link'): nao e proc do programa nem interno da std. NULL para os demais. */
static const IrExterno *ExternoDeFora(const BirCtx *c, int32_t nome)
{
    const IrExterno *externo;

    if (c->simbolo_do_nome[nome] != BIR_SIMBOLO_EXTERNO)
        return NULL;
    externo = BirExternoDe(c, nome);
    return externo != NULL && !externo->interno ? externo : NULL;
}

/* Foreign de fora: bool, i8, i16, u8 e u16 chegam ao C estendidos (sinal para i8/i16, zero para os demais): o C chamado pode contar com os 32 bits. */
BirOperando BirEstenderEstreito(BirOperando op, TipoId tipo)
{
    if (op.classe != BC_I8 && op.classe != BC_I16)
        return op;
    return OpComExtensao(op, TipoEhInteiro(tipo) && !TipoEhSemSinal(tipo) ? BE_SINAL : BE_ZERO);
}

/* estende: o chamado e C (foreign de fora ou chamada indireta) e pode contar com os 32 bits dos estreitos. */
static bool ConstruirArgs(BirCtx *c, const IrInstr *i, bool de_fora, BirOperando **saida)
{
    BirOperando *args = BirArgs(c, i->nargs);

    for (uint32_t k = 0; k < i->nargs; k++) {
        bool agregado = BirEhAgregado(c->e->tipos, i->args[k].tipo);

        if (!(agregado ? BirArgumentoAgregado(c, i, &i->args[k], &args[k]) : BirOperandoDe(c, i, &i->args[k], &args[k])))
            return false;
        if (de_fora && !agregado)
            args[k] = BirEstenderEstreito(args[k], i->args[k].tipo);
    }
    *saida = args;
    return true;
}

static bool ProcConhecido(BirCtx *c, const IrInstr *i)
{
    char causa[CAUSA_CHAMADA_MAX];

    if (i->nome >= 0 && (uint32_t)i->nome < c->n_nomes && c->simbolo_do_nome[i->nome] != BIR_SEM_SIMBOLO)
        return true;
    snprintf(causa, sizeof causa, "chamada a proc desconhecido (nome %d)", i->nome);
    return BirFalhaInconsistente(c, causa);
}

/* O foreign declarado com esse nome (NULL se o nome nao e de foreign). */
const IrExterno *BirExternoDe(const BirCtx *c, int32_t nome)
{
    for (uint32_t k = 0; k < c->e->ir->nexternos; k++) {
        if (c->e->ir->externos[k].nome == nome)
            return &c->e->ir->externos[k];
    }
    return NULL;
}

/* Funcoes do C que o mingw (libmsvcrt.a) liga como thunk para outra, essa sim exportada pela ucrtbase.dll: o oraculo C chama a _time64 e a ucrtbase nao exporta "time". */
typedef struct { const char *simbolo_c, *exportada; } ThunkCrt;
static const ThunkCrt THUNKS_CRT[] = {{"time", "_time64"}};

static bool LinkEhDoCrt(const char *link)
{
    return strcmp(link, "c") == 0 || strcmp(link, "m") == 0 || strcmp(link, "msvcrt") == 0 || strcmp(link, "ucrt") == 0;
}

/* O simbolo que o programa importa: o thunk do CRT troca pelo exportado quando todos os 'link' do programa sao do CRT (ou nao ha 'link'); com DLL de terceiro no 'link' vale o nome C.
 * No alvo linux (c->p->sysv) o thunk nao existe: o libc.a do musl tem "time" e nao tem "_time64". */
static const char *SimboloDoImport(const BirCtx *c, const char *simbolo_c)
{
    const IrPrograma *ir = c->e->ir;

    if (c->p->sysv)
        return simbolo_c;
    for (uint32_t k = 0; k < ir->nbibliotecas; k++) {
        if (!LinkEhDoCrt(InternerTexto(c->e->nomes, ir->bibliotecas[k])))
            return simbolo_c;
    }
    for (size_t k = 0; k < sizeof THUNKS_CRT / sizeof THUNKS_CRT[0]; k++) {
        if (strcmp(simbolo_c, THUNKS_CRT[k].simbolo_c) == 0)
            return THUNKS_CRT[k].exportada;
    }
    return simbolo_c;
}

/* Proc do programa: o simbolo da funcao. Foreign interno da std (Arena e Pool): o simbolo forja_* do proprio runtime. Foreign de fora: o slot
 * __imp_<simbolo C> da IAT (o simbolo C e o alias A6 quando ha; o ligador acha a DLL que o exporta). */
static uint32_t SimboloDaChamada(BirCtx *c, const IrInstr *i)
{
    const IrExterno *externo;

    if (c->simbolo_do_nome[i->nome] != BIR_SIMBOLO_EXTERNO)
        return c->simbolo_do_nome[i->nome];
    externo = BirExternoDe(c, i->nome);
    if (externo->interno)
        return BirSimboloRuntime(c, InternerTexto(c->e->nomes, externo->simbolo));
    return BirSimboloImport(c, SimboloDoImport(c, InternerTexto(c->e->nomes, externo->simbolo)), i->pos);
}

static BirInstr *EmitirChama(BirCtx *c, const IrInstr *i, BirOperando *args, uint8_t retorno)
{
    BirInstr *chama = BirEmitir(c, BIR_CHAMA, retorno);

    chama->simbolo = SimboloDaChamada(c, i);
    chama->args = args;
    chama->nargs = i->nargs;
    return chama;
}

/* Onde o agregado devolvido vai parar: o slot do temporario de destino ou, sem destino, um slot anonimo. */
static bool DestinoAgregado(BirCtx *c, const IrInstr *i, uint32_t *endereco)
{
    BirPedidoSlot pedido = {.temporario = true, .id = i->destino};
    uint32_t slot;

    if (!BirForma(c, i->tipo, &pedido.forma))
        return false;
    if (i->destino >= 0)
        return BirDefinirSlot(c, &pedido, endereco);
    slot = BirNovoSlot(c, pedido.forma.tam, pedido.forma.alinhamento);
    *endereco = BirNovoVreg(c, BC_I64);
    BirEmitirEndSlot(c, slot, *endereco);
    return true;
}

/* M5b P37: no alvo linux, o agregado por valor de foreign de fora leva o descritor SysV (folhas) em cada argumento; sem descritor = X0001. */
static bool AnexarDescritoresSysv(BirCtx *c, const IrInstr *i, BirOperando *args)
{
    for (uint32_t k = 0; k < i->nargs; k++) {
        args[k].sysv = NULL;
        if (!BirEhAgregado(c->e->tipos, i->args[k].tipo))
            continue;
        args[k].sysv = BirDescritorSysv(c, i->args[k].tipo);
        if (args[k].sysv == NULL)
            return BirFalhaX0001(c, i->pos, (const char *const[3]){"CHAMA", "agregado por valor de foreign no alvo linux", BirNomeDoTipo(c->e->tipos, i->args[k].tipo)}, MARCO_SYSV);
    }
    return true;
}

static bool ChamarComRetornoAgregado(BirCtx *c, const IrInstr *i, BirOperando *args, uint32_t tam)
{
    uint32_t endereco;
    BirInstr *chama;

    if (!DestinoAgregado(c, i, &endereco))
        return false;
    chama = EmitirChama(c, i, args, BC_NADA);
    chama->a = OpVreg(endereco, BC_I64);
    chama->a.tam = tam;
    if (c->p->sysv && ExternoDeFora(c, i->nome) != NULL) {
        chama->a.sysv = BirDescritorSysv(c, i->tipo);
        if (chama->a.sysv == NULL)
            return BirFalhaX0001(c, i->pos, (const char *const[3]){"CHAMA", "retorno agregado de foreign no alvo linux", BirNomeDoTipo(c->e->tipos, i->tipo)}, MARCO_SYSV);
    }
    return true;
}

/* Dev: ^T e cstr devolvidos por foreign de fora passam pela checagem de NULL (R0005, forja_falha_nulo). ?^T e ?cstr nao. */
static bool RetornoNaoNulo(const BirCtx *c, const IrExterno *externo)
{
    TipoClasse classe = TipoClasseDe(c->e->tipos, externo->retorno);

    return BirConfereLimites(c) && (classe == TC_PONTEIRO || classe == TC_CSTR);
}

/* Arquivo (trecho da Fonte) que declarou o foreign: o da raiz (0) ou o do trecho k. */
const char *BirArquivoDoTrecho(const BirCtx *c, uint32_t arquivo)
{
    const Fonte *f = c->e->fonte;

    return arquivo == 0 || f->ntrechos <= 1 ? f->caminho : f->trechos[arquivo].caminho;
}

/* valor == 0 -> forja_falha_nulo(oque, arquivo, linha) (R0005); segue num bloco novo. P21: o trampolim fjt_ reusa. */
void BirFalhaSeNulo(BirCtx *c, uint32_t valor, const BirOrigemNulo *origem)
{
    uint32_t nulo = BirComparar(c, BCOND_EQ, OpVreg(valor, BC_I64), OpImed(0, BC_I64));
    uint32_t frio = BirNovoBloco(c, true);
    uint32_t segue = BirNovoBloco(c, false);
    BirOperando args[3];

    BirDesvia(c, OpVreg(nulo, BC_I8), frio, segue);
    c->cur = frio;
    args[0] = OpVreg(BirVregDoTexto(c, origem->oque, (uint32_t)strlen(origem->oque)), BC_I64);
    args[1] = OpVreg(BirVregDoTexto(c, origem->arquivo, (uint32_t)strlen(origem->arquivo)), BC_I64);
    args[2] = OpImed(origem->linha, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, "forja_falha_nulo"), args, 3);
    c->cur = segue;
}

/* resultado == 0 -> forja_falha_nulo("retorno do proc foreign '<nome>'", arquivo do modulo, linha do 'proc') (R0005). */
static void GuardaRetornoForeign(BirCtx *c, const IrExterno *externo, uint32_t resultado)
{
    char oque[TEXTO_FOREIGN_MAX];
    int n = snprintf(oque, sizeof oque, TEXTO_RETORNO_FOREIGN, NomeModuloExibicao(InternerTexto(c->e->nomes, externo->nome))); /* IA1-L8: nome puro do foreign de modulo */

    if (n < 0 || (size_t)n >= sizeof oque)
        FalhaInterna("bir: texto do foreign '%s' passou de %zu bytes", InternerTexto(c->e->nomes, externo->nome), sizeof oque);
    BirFalhaSeNulo(c, resultado, &(BirOrigemNulo){oque, BirArquivoDoTrecho(c, externo->arquivo), externo->linha});
}

/* O retorno passavel da chamada (vazio nao tem passagem); *retorno fica zerado no vazio. */
static bool RetornoDaChamada(BirCtx *c, const IrInstr *i, Passagem *retorno)
{
    if (i->tipo == TIPO_VAZIO)
        return true;
    if (!BirTipoPassavel(c->e->tipos, i->tipo))
        return BirFalhaTipo(c, i, "proc", i->tipo);
    return PassagemDe(c, i->tipo, retorno);
}

/* *dst = o destino da instrucao na classe do retorno, ou BIR_SEM_VREG quando ela nao tem destino. */
static bool DestinoDoRetorno(BirCtx *c, const IrInstr *i, uint8_t classe, uint32_t *dst)
{
    *dst = BIR_SEM_VREG;
    return i->destino < 0 || BirDefinirDestino(c, classe, dst);
}

/* Retorno escalar (ou vazio). Foreign de fora com retorno ^T/cstr em dev guarda o NULL mesmo sem destino (o involucro do C sempre confere). */
static bool ChamarEscalar(BirCtx *c, const IrInstr *i, BirOperando *args, uint8_t classe)
{
    const IrExterno *externo = ExternoDeFora(c, i->nome);
    bool guarda = externo != NULL && RetornoNaoNulo(c, externo);
    uint32_t dst;

    if (!DestinoDoRetorno(c, i, classe, &dst))
        return false;
    if (guarda && dst == BIR_SEM_VREG)
        dst = BirNovoVreg(c, BC_I64);
    EmitirChama(c, i, args, classe)->dst = dst;
    if (guarda)
        GuardaRetornoForeign(c, externo, dst);
    return true;
}

/* M6 P71a (D-M6-8): chamada SOMEM em release. Nenhuma chamada e emitida; o destino nasce ZERADO (o IR o copia depois: o usuario le MemSnapshot{} com 0 em todo campo, nunca lixo da pilha). */
static bool OmitirChamadaSomem(BirCtx *c, const IrInstr *i)
{
    Passagem retorno = {0};
    uint32_t destino;

    if (!RetornoDaChamada(c, i, &retorno))
        return false;
    if (i->destino < 0)
        return true;
    if (retorno.tam != 0) {
        if (!DestinoAgregado(c, i, &destino))
            return false;
        BirZeraBloco(c, destino, retorno.tam);
        return true;
    }
    if (!DestinoDoRetorno(c, i, retorno.classe, &destino))
        return false;
    if (destino != BIR_SEM_VREG)
        BirMover(c, destino, OpImed(0, retorno.classe));
    return true;
}

bool BirRegraChamada(BirCtx *c, const IrInstr *i)
{
    Passagem retorno = {0};
    BirOperando *args;

    if (c->p->modo == BM_RELEASE && i->nome >= 0 && (uint32_t)i->nome < c->n_nomes && BirNomeSomem(InternerTexto(c->e->nomes, i->nome)))
        return OmitirChamadaSomem(c, i); /* antes do ProcConhecido: o proc de std:mem foi podado e nao tem simbolo */
    if (!ProcConhecido(c, i))
        return false;
    if (!RetornoDaChamada(c, i, &retorno))
        return false;
    if (!ConstruirArgs(c, i, ExternoDeFora(c, i->nome) != NULL, &args))
        return false;
    if (c->p->sysv && ExternoDeFora(c, i->nome) != NULL && !AnexarDescritoresSysv(c, i, args))
        return false;
    if (retorno.tam != 0)
        return ChamarComRetornoAgregado(c, i, args, retorno.tam);
    return ChamarEscalar(c, i, args, retorno.classe);
}

/* ---- chamada indireta (IR_CHAMADA_IND, P21v-c) ---- */

#define TEXTO_PROC_NULO "chamada por proc nulo"

typedef struct { uint32_t alvo; BirOperando *args; } ChamadaInd;

/* CHAMA_IND: b = o vreg com o endereco; o resto como o CHAMA. */
static BirInstr *EmitirChamaInd(BirCtx *c, const IrInstr *i, const ChamadaInd *ch, uint8_t classe)
{
    BirInstr *chama = BirEmitir(c, BIR_CHAMA_IND, classe);

    chama->b = OpVreg(ch->alvo, BC_I64);
    chama->args = ch->args;
    chama->nargs = i->nargs;
    return chama;
}

static bool ChamarIndEscalar(BirCtx *c, const IrInstr *i, const ChamadaInd *ch, uint8_t classe)
{
    uint32_t dst = BIR_SEM_VREG;

    if (i->destino >= 0 && !BirDefinirDestino(c, classe, &dst))
        return false;
    EmitirChamaInd(c, i, ch, classe)->dst = dst;
    return true;
}

static bool ChamarIndAgregado(BirCtx *c, const IrInstr *i, const ChamadaInd *ch, uint32_t tam)
{
    uint32_t endereco;
    BirInstr *chama;

    if (!DestinoAgregado(c, i, &endereco))
        return false;
    chama = EmitirChamaInd(c, i, ch, BC_NADA);
    chama->a = OpVreg(endereco, BC_I64);
    chama->a.tam = tam;
    if (c->p->sysv)
        chama->a.sysv = BirDescritorSysv(c, i->tipo);
    return true;
}

/* M5b P38c: no alvo linux o CHAMA_IND de proc (o stub fjt_ SysV) leva o descritor de cada agregado por valor; NULL = a assinatura nao e atendida e o
 * CHAMA_IND segue Win64, como o trampolim da mesma assinatura (AssinaturaSysv em bir_construir_fronteira.c). */
static void AnexarDescritoresSysvDoValor(BirCtx *c, const IrInstr *i, BirOperando *args)
{
    for (uint32_t k = 0; k < i->nargs; k++)
        args[k].sysv = BirEhAgregado(c->e->tipos, i->args[k].tipo) ? BirDescritorSysv(c, i->args[k].tipo) : NULL;
}

/* O proc chamado (local, temporario ou V_PROC) num vreg I64; dev: nulo = R0005 "chamada por proc nulo". */
static bool AlvoDaChamadaInd(BirCtx *c, const IrInstr *i, uint32_t *alvo)
{
    BirOperando op;
    const char *arquivo;
    uint32_t linha;

    if (!BirOperandoDe(c, i, &i->a, &op) || !BirOperandoVreg(c, op, &op))
        return false;
    *alvo = op.id;
    if (!BirConfereLimites(c))
        return true;
    BirArquivoELinha(c, i->pos, &arquivo, &linha);
    BirFalhaSeNulo(c, *alvo, &(BirOrigemNulo){TEXTO_PROC_NULO, arquivo, linha});
    return true;
}

bool BirRegraChamadaInd(BirCtx *c, const IrInstr *i)
{
    Passagem retorno = {0};
    ChamadaInd ch;

    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_PROC)
        return BirFalhaInconsistente(c, "chamada indireta sobre valor que nao e proc");
    if (!RetornoDaChamada(c, i, &retorno))
        return false;
    if (!AlvoDaChamadaInd(c, i, &ch.alvo) || !ConstruirArgs(c, i, true, &ch.args))
        return false;
    if (c->p->sysv)
        AnexarDescritoresSysvDoValor(c, i, ch.args);
    if (retorno.tam != 0)
        return ChamarIndAgregado(c, i, &ch, retorno.tam);
    return ChamarIndEscalar(c, i, &ch, retorno.classe);
}

/* ---- retorno ---- */

static bool RetornaAgregado(BirCtx *c, const IrInstr *i)
{
    BirSlot forma;
    BirOperando valor;
    uint32_t endereco;

    if (!BirForma(c, i->a.tipo, &forma) || !BirEnderecoDe(c, i, &i->a, &endereco))
        return false;
    valor = OpVreg(endereco, BC_I64);
    valor.tam = forma.tam;
    BirRetorna(c, &valor);
    return true;
}

bool BirRegraRetorna(BirCtx *c, const IrInstr *i)
{
    BirOperando valor;

    if (i->a.tipo_no == V_NENHUM) {
        BirRetorna(c, NULL);
        return true;
    }
    if (BirEhAgregado(c->e->tipos, i->a.tipo))
        return RetornaAgregado(c, i);
    if (!BirOperandoDe(c, i, &i->a, &valor))
        return false;
    BirRetorna(c, &valor);
    return true;
}
