/* bir_construir_fronteira.c - IR -> BIR: a fronteira C -> Forja. Um trampolim fjt_<proc> por IrTrampolim (proc da Forja passado a foreign) e o V_PROC que o nomeia;
 * na DLL, o involucro de nome limpo de cada 'pub export proc' e o forja_liberar.
 * Versao: 0.6.2
 * Changelog: 0.6.2 - Linguagem v1.13 L1a-ir: BirPonte.arquivo (trecho da Fonte, uint32_t) no lugar de modulo; BirArquivoDoTrecho.
 *            0.6.1 - G3 G0b: quebra de ciclo bir->nativo e bir->x64 (sem nativo_interface.h: DLL_INFO_* vieram de bir.h; x64_abi_sysv.[ch] mudou para nativo/bir).
 *            0.6.0 - M8 H1 (D-M8-2): BirConstruirExports acrescenta a BirFuncao forja_dll_info (devolve o endereco da struct que o nativo_interface.c grava no .rdata) e a lista de exportados.
 *            0.5.2 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.5.1 - O0 P13: GuardaParametro (nulo de ^T/cstr da fronteira) tambem no release com --checagem sim (BirConfereLimites).
 *            0.5.0 - M5b P38c: assinatura SysV (AssinaturaSysv) aceita agregado com descritor (BirDescritorSysv): o wrapper copia os eightbytes do bloco para um slot
 *                    privado, passa o endereco da pilha do chamador C (MEMORY) e devolve o retorno agregado por RAX/RDX/XMM0/XMM1 ou pelo ponteiro oculto em RDI.
 *            0.4.0 - M5b P38b: BirComparadorDoRuntime (sort_by e heap_*_by chamam o comparador em Win64): no alvo linux o comparador e o valor do proc (fjt_ SysV) menos
 *                    ABI_SYSV_ENTRADA_WIN64_ANTES, onde o stub tem uma entrada Win64 (jmp fjtm_<proc>); com algum comparador no programa (BirCtx.comparador_runtime) cada trampolim
 *                    escalar ganha tambem a ponte Win64 fjtm_<proc> (BirEntradaSysv.win64). Vale para literal e para proc em variavel.
 *            0.3.0 - M5b P38b: no alvo linux, o trampolim de assinatura so de escalares vira o par SysV: o stub fjt_<proc> (x64_entrada_sysv.c, listado em BirPrograma.entradas_sysv)
 *                    e o wrapper fjtw_<proc> (BirFuncao Win64 com 2 parametros, bloco* e pilha*) que le cada argumento da posicao da ABI (AbiSysvPlanejar), chama fj_<proc> e grava o
 *                    retorno no bloco. Assinatura com agregado (ou mais de 16 argumentos) segue Win64 ate o P38c. Windows: nenhuma instrucao muda.
 *            0.2.2 - M5 P46c: o prologo da ponte chama forja_fio_da_ponte (runtime) antes de converter os argumentos: o fio da thread de fora nasce so aqui.
 *            0.2.1 - M4 P24p: BirConstruirTrampolins pula o trampolim que o alcance nao marcou (nenhum proc alcancado toma o endereco do proc).
 *            0.2.0 - M3 P22a: BirPonte (o trampolim e o involucro de export sao a mesma BirFuncao, ConstruirPonte) e BirConstruirExports: uma BirFuncao com o nome exato do proc por 'pub export proc'
 *                    (estreitos re-estendidos; dev: ^T e cstr NULL entrando = R0005 "parametro '<p>' do proc exportado '<proc>'", arquivo raiz e linha do 'proc', o texto do FORJA_EXPORT do emit_c)
 *                    e forja_liberar (forja_mem_liberar(p, arquivo raiz, 0)) quando algum export devolve cstr/?cstr. BirPrograma.exportados lista os nomes, em ordem do fonte.
 *            0.1.0 - M3 P21: primeira versao. O trampolim e uma BirFuncao com a assinatura do proc (Win64 entrando do C): estreitos (bool, i8, i16, u8, u16) seguem RE-estendidos ao proc
 *                    (o C pode deixar lixo nos bits altos), dev: ^T e cstr NULL = R0005 "parametro '<p>' do callback '<proc>'" (o texto do fjt_ do emit_c), depois CHAMA fj_<proc> com os mesmos
 *                    argumentos e devolve o retorno (agregado por valor incluso). V_PROC = END_SIMBOLO (lea rax,[rip+fjt_<proc>]). Como toda BirFuncao, o trampolim ganha .pdata/.xdata do x64_unwind.c.
 * Verificacao: construir.bat && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m3/callback
 */
#include <stdio.h>
#include <string.h>

#include "common/nome_modulo.h"
#include "nativo/bir/bir_interno.h"
#include "nativo/bir/x64_abi_sysv.h"

#define PREFIXO_TRAMPOLIM "fjt_"
#define PREFIXO_WRAPPER "fjtw_"
#define PREFIXO_TRAMPOLIM_MS "fjtm_"  /* alvo linux: a ponte Win64 (o runtime chama o comparador assim); fjt_ la e a entrada SysV */
#define PARAM_BLOCO 0u  /* vreg 0 do wrapper SysV: o bloco que o stub preencheu */
#define PARAM_PILHA 1u  /* vreg 1: o inicio dos argumentos de pilha do chamador C */
#define TEXTO_PARAMETRO_CALLBACK "parametro '%s' do callback '%s'"
#define TEXTO_PARAMETRO_EXPORT "parametro '%s' do proc exportado '%s'"
#define TEXTO_CALLBACK_MAX 320
#define NOME_LIBERAR "forja_liberar"

static const IrFuncao *FuncaoDoProc(const BirCtx *c, int32_t nome)
{
    for (uint32_t k = 0; k < c->e->ir->nfuncoes; k++) {
        if (c->e->ir->funcoes[k].nome == nome)
            return &c->e->ir->funcoes[k];
    }
    return NULL;
}

/* "<prefixo><proc>" copiado na arena; o mesmo texto devolve o mesmo simbolo (a tabela de simbolos deduplica por nome). */
static uint32_t SimboloComPrefixo(BirCtx *c, const char *prefixo, int32_t nome)
{
    const char *proc = InternerTexto(c->e->nomes, nome);
    size_t n = strlen(proc);
    size_t np = strlen(prefixo);
    char *texto = ArenaAlocar(&c->p->arena, np + n + 1);

    memcpy(texto, prefixo, np);
    memcpy(texto + np, proc, n + 1);
    return BirSimboloGlobal(c, texto);
}

static uint32_t SimboloDoTrampolim(BirCtx *c, int32_t nome)
{
    return SimboloComPrefixo(c, PREFIXO_TRAMPOLIM, nome);
}

/* Indice do IrTrampolim do proc; UINT32_MAX = nao ha. */
static uint32_t IndiceDoTrampolim(const BirCtx *c, int32_t nome)
{
    for (uint32_t k = 0; k < c->e->ir->ntrampolins; k++) {
        if (c->e->ir->trampolins[k].nome == nome)
            return k;
    }
    return UINT32_MAX;
}

bool BirOperandoProc(BirCtx *c, const Valor *v, BirOperando *saida)
{
    uint32_t endereco;

    if (IndiceDoTrampolim(c, v->id) == UINT32_MAX) {
        char causa[TEXTO_CALLBACK_MAX];

        snprintf(causa, sizeof causa, "V_PROC '%s' sem IrTrampolim", InternerTexto(c->e->nomes, v->id));
        return BirFalhaInconsistente(c, causa);
    }
    endereco = BirNovoVreg(c, BC_I64);
    BirEmitirPedido(c, (BirPedido){.op = BIR_END_SIMBOLO, .dst = endereco, .a = OpSimbolo(SimboloDoTrampolim(c, v->id))});
    *saida = OpVreg(endereco, BC_I64);
    return true;
}

/* O runtime (forja_ordenar_estavel, heap) chama o comparador em Win64. No alvo linux o valor do proc (literal ou em variavel) e o fjt_ SysV, e a entrada Win64 do stub
 * (um jmp ao fjtm_) mora ABI_SYSV_ENTRADA_WIN64_ANTES bytes antes dele: o comparador e fjt_ - 16. Em Windows e BirOperandoDe + BirOperandoVreg, como sempre. */
bool BirComparadorDoRuntime(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida)
{
    BirOperando proc;

    if (!BirOperandoDe(c, i, v, &proc) || !BirOperandoVreg(c, proc, saida))
        return false;
    if (!c->p->sysv)
        return true;
    c->comparador_runtime = true;
    *saida = OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = *saida, .b = OpImed(-(int64_t)ABI_SYSV_ENTRADA_WIN64_ANTES, BC_I64)}), BC_I64);
    return true;
}

/* Uma ponte C -> Forja: o trampolim fjt_<proc> (callback de foreign) e o involucro de nome limpo de um 'pub export proc' (DLL) sao a mesma BirFuncao, com a
 * assinatura do proc, e so diferem no simbolo, no texto do R0005 (formato com o parametro e o proc) e no arquivo/linha dele. */
typedef struct {
    uint32_t simbolo;
    const char *formato;
    uint32_t arquivo; /* trecho da Fonte do proc (0 = raiz) */
    uint32_t linha;
} BirPonte;

/* O parametro k do proc e o vreg que o guarda na ponte: o proprio k na ponte Win64; um vreg carregado do bloco na ponte SysV (wrapper fjtw_). */
typedef struct {
    uint32_t k, vreg;
} ParamDaPonte;

/* Dev: ^T e cstr vindos do C passam pela mesma checagem de NULL da ponte (forja_falha_nulo, R0005); ?^T nao. */
static void GuardaParametro(BirCtx *c, const BirPonte *ponte, const IrFuncao *irf, const ParamDaPonte *param)
{
    char oque[TEXTO_CALLBACK_MAX];
    const char *proc = NomeModuloExibicao(InternerTexto(c->e->nomes, irf->nome)); /* IA1-L8: o texto mostra o nome puro do proc de modulo */
    int n = snprintf(oque, sizeof oque, ponte->formato, InternerTexto(c->e->nomes, irf->params[param->k].nome), proc);

    if (n < 0 || (size_t)n >= sizeof oque)
        FalhaInterna("bir: texto da ponte '%s' passou de %zu bytes", proc, sizeof oque);
    BirFalhaSeNulo(c, param->vreg, &(BirOrigemNulo){oque, BirArquivoDoTrecho(c, ponte->arquivo), ponte->linha});
}

/* O parametro da ponte como argumento do proc: agregado = o endereco da copia que o C entregou, com o tamanho; estreito re-estendido. */
static BirOperando ArgumentoDaPonte(BirCtx *c, const BirPonte *ponte, const IrFuncao *irf, const ParamDaPonte *param)
{
    uint32_t k = param->k;
    BirOperando arg = OpVreg(param->vreg, c->f->classe_vreg[param->vreg]);
    TipoClasse classe = TipoClasseDe(c->e->tipos, irf->params[k].tipo);

    if (c->f->param_tam != NULL && c->f->param_tam[k] != 0) {
        arg.tam = c->f->param_tam[k];
        return arg;
    }
    if (BirConfereLimites(c) && (classe == TC_PONTEIRO || classe == TC_CSTR))
        GuardaParametro(c, ponte, irf, param);
    return BirEstenderEstreito(arg, irf->params[k].tipo);
}

static BirInstr *EmitirChamada(BirCtx *c, const IrFuncao *irf, BirOperando *args, uint8_t retorno)
{
    BirInstr *chama = BirEmitir(c, BIR_CHAMA, retorno);

    chama->simbolo = c->simbolo_do_nome[irf->nome];
    chama->args = args;
    chama->nargs = irf->nparams;
    return chama;
}

/* Retorno por valor de struct: o proc grava num slot privado (o endereco viaja em CHAMA.a) e o trampolim devolve esse endereco em RETORNA.a. */
static bool ChamarComRetornoAgregado(BirCtx *c, const IrFuncao *irf, BirOperando *args)
{
    BirSlot forma;
    BirOperando destino;
    uint32_t endereco;

    if (!BirForma(c, irf->retorno, &forma))
        return false;
    endereco = BirNovoVreg(c, BC_I64);
    BirEmitirEndSlot(c, BirNovoSlot(c, forma.tam, forma.alinhamento), endereco);
    destino = OpVreg(endereco, BC_I64);
    destino.tam = c->f->retorno_tam;
    EmitirChamada(c, irf, args, BC_NADA)->a = destino;
    BirRetorna(c, &destino);
    return true;
}

/* CHAMA fj_<proc>(args) e RETORNA o resultado (escalar no dst; vazio sem valor). */
static bool ChamarEDevolver(BirCtx *c, const IrFuncao *irf, BirOperando *args)
{
    uint8_t retorno = c->f->retorno;
    BirOperando valor;
    uint32_t dst;

    if (c->f->retorno_tam != 0)
        return ChamarComRetornoAgregado(c, irf, args);
    if (retorno == BC_NADA) {
        EmitirChamada(c, irf, args, BC_NADA);
        BirRetorna(c, NULL);
        return true;
    }
    dst = BirNovoVreg(c, retorno);
    EmitirChamada(c, irf, args, retorno)->dst = dst;
    valor = OpVreg(dst, retorno);
    BirRetorna(c, &valor);
    return true;
}

static bool ConstruirPonte(BirCtx *c, const IrFuncao *irf, const BirPonte *ponte)
{
    BirFuncao *f = &c->p->funcoes[c->p->nfuncoes++];
    BirOperando *args;

    *f = (BirFuncao){.simbolo = ponte->simbolo, .nparams = irf->nparams, .pos = BIR_SEM_POS};
    c->f = f;
    c->cap_vregs = 0;
    c->atual = NULL;
    if (!BirPrepararAssinatura(c, irf))
        return false;
    c->cur = BirNovoBloco(c, false);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_fio_da_ponte", BC_NADA, 0}, NULL); /* P46c: thread de fora ganha o fio antes de tudo */
    args = BirArgs(c, irf->nparams);
    for (uint32_t k = 0; k < irf->nparams; k++)
        args[k] = ArgumentoDaPonte(c, ponte, irf, &(ParamDaPonte){k, k});
    return ChamarEDevolver(c, irf, args);
}

/* ---- callback SysV (M5b P38b): o stub fjt_ guarda os registradores no bloco e chama este wrapper (Win64: rcx = bloco, rdx = pilha do chamador C) ---- */

static uint32_t TamEscalarSysv(uint8_t classe)
{
    switch (classe) {
    case BC_I8: return 1;
    case BC_I16: return 2;
    case BC_I32: case BC_F32: return 4;
    default: return 8;
    }
}

/* Tipo que o par SysV atende: escalar, ou agregado com descritor (folhas em BirDescritorSysv). Agregado sem descritor (str, lista, ?T...) segue Win64. */
static bool TipoSysvAtendido(BirCtx *c, TipoId tipo)
{
    uint8_t classe;

    if (BirClasseDoTipo(c->e->tipos, tipo, &classe))
        return true;
    return BirEhAgregado(c->e->tipos, tipo) && BirDescritorSysv(c, tipo) != NULL;
}

/* Ate ABI_SYSV_MAX_ARGS argumentos, todos atendidos: a mesma conta que CHAMA_IND faz (x64_chamada_sysv.c ChamadaIndSysv) pelos descritores anexados. */
static bool AssinaturaSysv(BirCtx *c, const IrFuncao *irf)
{
    if (irf->nparams > ABI_SYSV_MAX_ARGS)
        return false;
    if (irf->retorno != TIPO_VAZIO && !TipoSysvAtendido(c, irf->retorno))
        return false;
    for (uint32_t k = 0; k < irf->nparams; k++) {
        if (!TipoSysvAtendido(c, irf->params[k].tipo))
            return false;
    }
    return true;
}

static AbiSysvArg DescritorEscalarSysv(AbiSysvCampo *campo, uint8_t classe)
{
    uint32_t tam = TamEscalarSysv(classe);

    *campo = (AbiSysvCampo){.classe = BirClasseEhReal(classe) ? ABI_SYSV_SSE : ABI_SYSV_INTEIRO, .deslocamento = 0, .alinhamento = tam};
    return (AbiSysvArg){.tam = tam, .campos = campo, .n_campos = 1};
}

/* Descritores SysV da assinatura: os campos moram aqui; o plano (AbiSysvPlano) so guarda o destino de cada um. */
#define FOLHAS_SYSV_MAX 16u /* um agregado de ate 16 bytes tem no maximo 16 escalares (bir_construir_sysv.c) */

typedef struct {
    AbiSysvArg arg[ABI_SYSV_MAX_ARGS];
    AbiSysvCampo campos[ABI_SYSV_MAX_ARGS][FOLHAS_SYSV_MAX];
    AbiSysvArg ret;
    AbiSysvCampo campos_ret[FOLHAS_SYSV_MAX];
} DescritoresDaPonte;

/* Descritor de um tipo da assinatura: escalar (uma folha) ou agregado (as folhas do BirDescritorSysv; o MEMORY ja vem com a folha falsa). */
static void DescreverTipoSysv(BirCtx *c, TipoId tipo, AbiSysvArg *arg, AbiSysvCampo *campos)
{
    uint8_t classe;
    const BirSysv *desc;

    if (BirClasseDoTipo(c->e->tipos, tipo, &classe)) {
        *arg = DescritorEscalarSysv(campos, classe);
        return;
    }
    desc = BirDescritorSysv(c, tipo);
    *arg = (AbiSysvArg){.tam = desc->tam, .campos = campos, .n_campos = desc->n};
    for (uint32_t f = 0; f < desc->n; f++) {
        campos[f] = (AbiSysvCampo){.classe = desc->folhas[f].sse ? ABI_SYSV_SSE : ABI_SYSV_INTEIRO,
                                   .deslocamento = desc->folhas[f].deslocamento, .alinhamento = desc->folhas[f].alinhamento};
    }
}

/* Plano da ABI SysV para a assinatura (ja atendida por AssinaturaSysv): onde cada argumento chega e em que registrador o retorno sai. */
static bool PlanejarEntradaSysv(BirCtx *c, const IrFuncao *irf, AbiSysvPlano *plano)
{
    DescritoresDaPonte d;

    for (uint32_t k = 0; k < irf->nparams; k++)
        DescreverTipoSysv(c, irf->params[k].tipo, &d.arg[k], d.campos[k]);
    d.ret = (AbiSysvArg){0};
    if (irf->retorno != TIPO_VAZIO)
        DescreverTipoSysv(c, irf->retorno, &d.ret, d.campos_ret);
    return AbiSysvPlanejar(d.arg, irf->nparams, &d.ret, plano) || BirFalhaInconsistente(c, "callback SysV: plano da ABI nao fecha");
}

/* O argumento escalar k: do bloco (registrador) ou da pilha do chamador C, na largura da classe; a posicao vem do plano. */
static uint32_t CarregarParametroSysv(BirCtx *c, const AbiSysvPlano *plano, uint32_t k, uint8_t classe)
{
    uint32_t dst = BirNovoVreg(c, classe);
    bool na_pilha = plano->arg[k].na_memoria;
    uint32_t desloc = na_pilha ? plano->off[k] : AbiSysvBlocoArg(plano->arg[k].reg[0]);

    BirEmitirCarrega(c, dst, na_pilha ? PARAM_PILHA : PARAM_BLOCO, (int32_t)desloc);
    return dst;
}

/* Tamanho do slot privado de um agregado: arredondado a 8, para os eightbytes entrarem inteiros. */
static uint32_t TamDoSlotSysv(uint32_t tam)
{
    return (tam + ABI_SYSV_BYTES_EIGHTBYTE - 1u) / ABI_SYSV_BYTES_EIGHTBYTE * ABI_SYSV_BYTES_EIGHTBYTE;
}

/* Agregado nos registradores: cada eightbyte (INTEGER ou SSE) sai do bloco para um slot privado; o endereco do slot e o argumento. */
static uint32_t CopiarAgregadoDoBloco(BirCtx *c, const AbiSysvPlano *plano, uint32_t k, uint32_t tam)
{
    uint32_t endereco = BirEmitirEndSlot(c, BirNovoSlot(c, TamDoSlotSysv(tam), ABI_SYSV_BYTES_EIGHTBYTE), BirNovoVreg(c, BC_I64));

    for (uint32_t e = 0; e < plano->arg[k].n_eightbytes; e++) {
        uint32_t valor = BirNovoVreg(c, BC_I64);

        BirEmitirCarrega(c, valor, PARAM_BLOCO, (int32_t)AbiSysvBlocoArg(plano->arg[k].reg[e]));
        BirEmitirGrava(c, endereco, (int32_t)(e * ABI_SYSV_BYTES_EIGHTBYTE), OpVreg(valor, BC_I64));
    }
    return endereco;
}

/* Agregado na pilha (MEMORY ou sem registradores bastantes): o endereco e a propria area de argumentos do chamador C, na posicao do plano. */
static uint32_t EnderecoDaPilhaSysv(BirCtx *c, const AbiSysvPlano *plano, uint32_t k)
{
    return BirContaNova(c, BC_I64, (BirPedido){.op = BIR_ADD, .a = OpVreg(PARAM_PILHA, BC_I64), .b = OpImed((int64_t)plano->off[k], BC_I64)});
}

/* Argumento k agregado: o endereco (copia nos registradores ou pilha do C) e o tamanho do tipo, como o CHAMA de agregado espera. */
static BirOperando ArgumentoAgregadoSysv(BirCtx *c, const AbiSysvPlano *plano, const IrFuncao *irf, uint32_t k)
{
    BirSlot forma;
    BirOperando arg;

    BirForma(c, irf->params[k].tipo, &forma);
    arg = OpVreg(plano->arg[k].na_memoria ? EnderecoDaPilhaSysv(c, plano, k) : CopiarAgregadoDoBloco(c, plano, k, forma.tam), BC_I64);
    arg.tam = forma.tam;
    return arg;
}

static BirOperando *ArgumentosDoBlocoSysv(BirCtx *c, const BirPonte *ponte, const IrFuncao *irf, const AbiSysvPlano *plano)
{
    BirOperando *args = BirArgs(c, irf->nparams);
    uint8_t classe;

    for (uint32_t k = 0; k < irf->nparams; k++) {
        if (BirClasseDoTipo(c->e->tipos, irf->params[k].tipo, &classe))
            args[k] = ArgumentoDaPonte(c, ponte, irf, &(ParamDaPonte){k, CarregarParametroSysv(c, plano, k, classe)});
        else
            args[k] = ArgumentoAgregadoSysv(c, plano, irf, k);
    }
    return args;
}

/* O retorno escalar no registrador da ABI (rax ou xmm0) do bloco; estreito e i32 seguem estendidos a 64 bits (o C pode contar com isso). */
static void GravarRetornoNoBloco(BirCtx *c, const IrFuncao *irf, const AbiSysvPlano *plano, uint32_t vreg)
{
    uint8_t classe = c->f->classe_vreg[vreg];
    BirOperando valor = OpVreg(vreg, classe);

    if (classe >= BC_I8 && classe <= BC_I32)
        valor = OpVreg(BirContaNova(c, BC_I64, (BirPedido){.op = TipoEhSemSinal(irf->retorno) ? BIR_EXT_Z : BIR_EXT_S, .a = valor}), BC_I64);
    BirEmitirGrava(c, PARAM_BLOCO, (int32_t)AbiSysvBlocoRet(plano->ret.reg[0]), valor);
}

/* CHAMA de fj_<proc> com o destino do agregado em CHAMA.a (tam = bytes do retorno, como ChamarComRetornoAgregado). */
static void ChamarComDestinoSysv(BirCtx *c, const IrFuncao *irf, BirOperando *args, uint32_t endereco)
{
    BirSlot forma;
    BirOperando destino = OpVreg(endereco, BC_I64);

    BirForma(c, irf->retorno, &forma);
    destino.tam = forma.tam;
    EmitirChamada(c, irf, args, BC_NADA)->a = destino;
}

/* Eightbytes do agregado devolvido (no slot privado) para RAX/RDX ou XMM0/XMM1 do bloco, conforme o plano do retorno. */
static void GravarEightbytesDoRetorno(BirCtx *c, const AbiSysvPlano *plano, uint32_t endereco)
{
    for (uint32_t e = 0; e < plano->ret.n_eightbytes; e++) {
        uint32_t valor = BirNovoVreg(c, BC_I64);

        BirEmitirCarrega(c, valor, endereco, (int32_t)(e * ABI_SYSV_BYTES_EIGHTBYTE));
        BirEmitirGrava(c, PARAM_BLOCO, (int32_t)AbiSysvBlocoRet(plano->ret.reg[e]), OpVreg(valor, BC_I64));
    }
}

/* Retorno agregado. MEMORY (plano oculto): o destino e o ponteiro que chegou em RDI (bloco[0]), e o stub devolve esse ponteiro em RAX.
 * Senao: o chamado grava num slot privado e os eightbytes saem pelos registradores do plano. */
static void ChamarEGravarAgregadoSysv(BirCtx *c, const IrFuncao *irf, BirOperando *args, const AbiSysvPlano *plano)
{
    BirSlot forma;
    uint32_t endereco;

    BirForma(c, irf->retorno, &forma);
    if (plano->oculto) {
        endereco = BirNovoVreg(c, BC_I64);
        BirEmitirCarrega(c, endereco, PARAM_BLOCO, (int32_t)AbiSysvBlocoArg(ABI_SYSV_RDI));
        BirEmitirGrava(c, PARAM_BLOCO, (int32_t)AbiSysvBlocoRet(ABI_SYSV_RAX), OpVreg(endereco, BC_I64));
        ChamarComDestinoSysv(c, irf, args, endereco);
        return;
    }
    endereco = BirEmitirEndSlot(c, BirNovoSlot(c, TamDoSlotSysv(forma.tam), ABI_SYSV_BYTES_EIGHTBYTE), BirNovoVreg(c, BC_I64));
    ChamarComDestinoSysv(c, irf, args, endereco);
    GravarEightbytesDoRetorno(c, plano, endereco);
}

static void ChamarEGravarSysv(BirCtx *c, const IrFuncao *irf, BirOperando *args, const AbiSysvPlano *plano)
{
    uint8_t classe = BC_NADA;
    uint32_t dst;

    if (irf->retorno == TIPO_VAZIO) {
        EmitirChamada(c, irf, args, BC_NADA);
    } else if (BirClasseDoTipo(c->e->tipos, irf->retorno, &classe)) {
        dst = BirNovoVreg(c, classe);
        EmitirChamada(c, irf, args, classe)->dst = dst;
        GravarRetornoNoBloco(c, irf, plano, dst);
    } else {
        ChamarEGravarAgregadoSysv(c, irf, args, plano);
    }
    BirRetorna(c, NULL);
}

static bool ConstruirPonteSysv(BirCtx *c, const IrFuncao *irf, const BirPonte *ponte)
{
    BirFuncao *f = &c->p->funcoes[c->p->nfuncoes++];
    uint32_t wrapper = SimboloComPrefixo(c, PREFIXO_WRAPPER, irf->nome);
    AbiSysvPlano plano;

    if (!PlanejarEntradaSysv(c, irf, &plano))
        return false;
    *f = (BirFuncao){.simbolo = wrapper, .nparams = 2, .pos = BIR_SEM_POS};
    c->f = f;
    c->cap_vregs = 0;
    c->atual = NULL;
    BirNovoVreg(c, BC_I64);
    BirNovoVreg(c, BC_I64);
    c->cur = BirNovoBloco(c, false);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_fio_da_ponte", BC_NADA, 0}, NULL);
    ChamarEGravarSysv(c, irf, ArgumentosDoBlocoSysv(c, ponte, irf, &plano), &plano);
    c->p->entradas_sysv[c->p->nentradas_sysv++] = (BirEntradaSysv){.stub = ponte->simbolo, .wrapper = wrapper, .win64 = BIR_SEM_SIMBOLO};
    return true;
}

/* Alvo linux, assinatura atendida por AssinaturaSysv: o par SysV (stub fjt_ + wrapper) e, se algum comparador vai ao runtime, a ponte Win64 fjtm_ que a entrada Win64 do stub alcanca.
 * Windows e assinatura nao atendida (agregado sem descritor, mais de 16 argumentos): o fjt_ Win64 de sempre. */
static bool ConstruirTrampolim(BirCtx *c, const IrTrampolim *t)
{
    const IrFuncao *irf = FuncaoDoProc(c, t->nome);
    BirPonte ponte = {SimboloDoTrampolim(c, t->nome), TEXTO_PARAMETRO_CALLBACK, t->arquivo, t->linha};

    if (irf == NULL)
        return BirFalhaInconsistente(c, "trampolim de proc inexistente");
    if (!c->p->sysv || !AssinaturaSysv(c, irf))
        return ConstruirPonte(c, irf, &ponte);
    if (!ConstruirPonteSysv(c, irf, &ponte))
        return false;
    if (!c->comparador_runtime)
        return true;
    ponte.simbolo = SimboloComPrefixo(c, PREFIXO_TRAMPOLIM_MS, t->nome);
    c->p->entradas_sysv[c->p->nentradas_sysv - 1].win64 = ponte.simbolo;
    return ConstruirPonte(c, irf, &ponte);
}

bool BirConstruirTrampolins(BirCtx *c)
{
    if (c->p->sysv)
        c->p->entradas_sysv = ArenaAlocar(&c->p->arena, (c->e->ir->ntrampolins + 1u) * sizeof *c->p->entradas_sysv);
    for (uint32_t k = 0; k < c->e->ir->ntrampolins; k++) {
        if (c->alcance != NULL && !BirAlcanceTrampolim(c->alcance, k))
            continue;
        if (!ConstruirTrampolim(c, &c->e->ir->trampolins[k]))
            return false;
    }
    return true;
}

/* ---- DLL (M3 P22a): o involucro de cada 'pub export proc' e o forja_liberar ---- */

static uint32_t ContarExports(const IrPrograma *ir)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < ir->nfuncoes; k++) {
        if (ir->funcoes[k].exportado)
            n++;
    }
    return n;
}

/* Algum export devolve cstr ou ?cstr (nicho): a DLL ganha forja_liberar (D20), como o ExportaCstr do emit_h. */
static bool ExportaTexto(const BirCtx *c)
{
    for (uint32_t k = 0; k < c->e->ir->nfuncoes; k++) {
        const IrFuncao *f = &c->e->ir->funcoes[k];

        if (f->exportado && (TipoClasseDe(c->e->tipos, f->retorno) == TC_CSTR || TipoOpcionalNicho(c->e->tipos, f->retorno)))
            return true;
    }
    return false;
}

/* void forja_liberar(const char *p): forja_mem_liberar(p, FORJA_ARQ, 0), o que a DLL exporta para quem recebeu um cstr. */
static bool ConstruirLiberar(BirCtx *c)
{
    BirFuncao *f = &c->p->funcoes[c->p->nfuncoes++];
    const char *arquivo = BirArquivoDoTrecho(c, 0);
    BirOperando *args;

    *f = (BirFuncao){.simbolo = BirSimboloGlobal(c, NOME_LIBERAR), .nparams = 1, .pos = BIR_SEM_POS};
    c->f = f;
    c->cap_vregs = 0;
    c->atual = NULL;
    BirNovoVreg(c, BC_I64);
    c->cur = BirNovoBloco(c, false);
    args = BirArgs(c, 3);
    args[0] = OpVreg(0, BC_I64);
    args[1] = OpVreg(BirVregDoTexto(c, arquivo, (uint32_t)strlen(arquivo)), BC_I64);
    args[2] = OpImed(0, BC_I32);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_mem_liberar", BC_NADA, 3}, args);
    BirRetorna(c, NULL);
    return true;
}

static bool ConstruirExport(BirCtx *c, const IrFuncao *irf)
{
    const char *nome = InternerTexto(c->e->nomes, irf->nome);

    c->p->exportados[c->p->nexportados++] = nome;
    return ConstruirPonte(c, irf, &(BirPonte){BirSimboloGlobal(c, nome), TEXTO_PARAMETRO_EXPORT, 0, irf->linha});
}

/* M8 H1 (D-M8-2): forja_dll_info(): devolve o endereco da struct estatica (magic, formato, hash e texto da interface) que o nativo.c grava no .rdata. */
static void ConstruirInfo(BirCtx *c)
{
    BirFuncao *f = &c->p->funcoes[c->p->nfuncoes++];
    uint32_t vreg;
    BirOperando valor;

    *f = (BirFuncao){.simbolo = BirSimboloGlobal(c, DLL_INFO_NOME), .retorno = BC_I64, .pos = BIR_SEM_POS};
    c->f = f;
    c->cap_vregs = 0;
    c->atual = NULL;
    c->cur = BirNovoBloco(c, false);
    vreg = BirNovoVreg(c, BC_I64);
    BirEmitirPedido(c, (BirPedido){.op = BIR_END_SIMBOLO, .dst = vreg, .a = OpSimbolo(BirSimboloGlobal(c, DLL_INFO_SIMBOLO_DADOS))});
    valor = OpVreg(vreg, BC_I64);
    BirRetorna(c, &valor);
}

bool BirConstruirExports(BirCtx *c)
{
    const IrPrograma *ir = c->e->ir;

    c->p->exportados = ArenaAlocar(&c->p->arena, (ContarExports(ir) + 2) * sizeof *c->p->exportados);
    for (uint32_t k = 0; k < ir->nfuncoes; k++) {
        if (ir->funcoes[k].exportado && !ConstruirExport(c, &ir->funcoes[k]))
            return false;
    }
    c->p->exportados[c->p->nexportados++] = DLL_INFO_NOME;
    ConstruirInfo(c);
    if (!ExportaTexto(c))
        return true;
    c->p->exportados[c->p->nexportados++] = NOME_LIBERAR;
    return ConstruirLiberar(c);
}
