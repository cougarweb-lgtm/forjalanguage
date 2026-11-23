/* x64_chamada_sysv.c - CHAMA a foreign de fora no alvo x86_64-linux pela ABI System V AMD64 (M5b P37).
 * Argumentos e retorno seguem a classe de cada eightbyte (x64_abi_sysv.c); pilha em [rsp+0] (sem sombra de 32 B); al = quantos xmm; a chamada
 * vai pelo slot __imp_<C> do ligador (lea r11,[rip+slot] ; call [r11]), e r11 e a ponte porque nao leva argumento. Forja interna segue Win64.
 * Versao: 0.4.1
 * Changelog: 0.4.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.4.0 - M5b P38c: CHAMA_IND com agregado por valor e retorno agregado sai SysV quando traz o descritor (ChamadaIndSysv); mesmo predicado do trampolim.
 *            0.3.0 - M5b P38b: CHAMA_IND de assinatura escalar tambem sai SysV no alvo linux (o valor de proc e o stub fjt_ SysV): SysvEhChamadaDoPrograma a aceita e o alvo vai
 *                    por IselCarregar(r11) + call r11. A chamada pelo slot e os byte emitidos de antes nao mudam.
 *            0.2.0 - M5b P38a: o planejamento puro (classes, registradores, pilha, AL, retorno) saiu para AbiSysvPlanejar (x64_abi_sysv.c);
 *                    este arquivo so monta as entradas do BIR e emite. Nenhum byte emitido mudou.
 *            0.1.0 - M5b P37: IselChamaSysv (argumentos, pilha, retorno em registrador ou por ponteiro oculto em RDI), SysvEhChamadaDoPrograma
 *                    (o quadro reserva a pilha e os registradores que o chamado destroi) e SysvBytesDePilha.
 * Verificacao: tests\nativo\linux\abi_sysv (60 assinaturas, dev e release) e tests\nativo\linux\interop\ms (prova_ms).
 */
#include "nativo/bir/x64_abi_sysv.h"
#include "nativo/x64/x64_interno.h"

#define SYSV_MAX_FOLHAS 16u
#define SYSV_BYTES_OITO 8u

/* Descritores SysV dos argumentos e do retorno, no formato do classificador: os campos moram aqui, o plano (AbiSysvPlano) so guarda o destino. */
typedef struct {
    AbiSysvArg arg[ABI_SYSV_MAX_ARGS];
    AbiSysvCampo campos[ABI_SYSV_MAX_ARGS][SYSV_MAX_FOLHAS];
    AbiSysvArg ret;
    AbiSysvCampo campos_ret[SYSV_MAX_FOLHAS];
} EntradaSysv;

static X64Reg RegistradorDoSysv(AbiSysvReg reg)
{
    static const X64Reg REGS[] = {X64_RAX, X64_RDX, X64_RDI, X64_RSI, X64_RCX, X64_R8, X64_R9};

    return REGS[reg];
}

static uint32_t TamDoEscalarSysv(uint8_t classe)
{
    switch (classe) {
    case BC_I8: return 1;
    case BC_I16: return 2;
    case BC_I32: case BC_F32: return 4;
    default: return 8;
    }
}

static void PreencherEscalar(EntradaSysv *e, uint32_t k, uint8_t classe)
{
    uint32_t tam = TamDoEscalarSysv(classe);

    e->campos[k][0] = (AbiSysvCampo){.classe = BirClasseEhReal(classe) ? ABI_SYSV_SSE : ABI_SYSV_INTEIRO, .deslocamento = 0, .alinhamento = tam};
    e->arg[k] = (AbiSysvArg){.tam = tam, .campos = e->campos[k], .n_campos = 1};
}

/* Folhas do descritor do BIR (bir_construir_sysv.c) no formato do classificador. */
static void PreencherAgregado(AbiSysvArg *arg, AbiSysvCampo *campos, const BirOperando *o)
{
    for (uint32_t f = 0; f < o->sysv->n; f++) {
        const BirFolhaSysv *folha = &o->sysv->folhas[f];

        campos[f] = (AbiSysvCampo){.classe = folha->sse ? ABI_SYSV_SSE : ABI_SYSV_INTEIRO, .deslocamento = folha->deslocamento,
                                   .alinhamento = folha->alinhamento};
    }
    *arg = (AbiSysvArg){.tam = o->sysv->tam, .campos = campos, .n_campos = o->sysv->n};
}

static bool PrepararArgumentos(const BirInstr *i, EntradaSysv *e)
{
    for (uint32_t k = 0; k < i->nargs; k++) {
        const BirOperando *o = &i->args[k];

        if (o->tam == 0) {
            PreencherEscalar(e, k, o->classe);
            continue;
        }
        if (o->sysv == NULL || o->sysv->n == 0 || o->sysv->n > SYSV_MAX_FOLHAS)
            return false;
        PreencherAgregado(&e->arg[k], e->campos[k], o);
    }
    return true;
}

/* Retorno void vira descritor vazio (tam 0): a classificacao da ABI o trata como sem registrador. */
static bool PrepararRetorno(const BirInstr *i, EntradaSysv *e)
{
    if (i->a.tam == 0) {
        e->ret = (AbiSysvArg){0};
        return true;
    }
    if (i->a.sysv == NULL || i->a.sysv->n == 0 || i->a.sysv->n > SYSV_MAX_FOLHAS)
        return false;
    PreencherAgregado(&e->ret, e->campos_ret, &i->a);
    return true;
}

/* Monta as entradas do BIR e planeja a chamada com a parte pura da ABI (AbiSysvPlanejar). */
static bool PlanejarSysv(const BirInstr *i, EntradaSysv *e, AbiSysvPlano *p)
{
    if (i->nargs > ABI_SYSV_MAX_ARGS || !PrepararArgumentos(i, e) || !PrepararRetorno(i, e))
        return false;
    return AbiSysvPlanejar(e->arg, i->nargs, &e->ret, p);
}

/* CHAMA_IND no alvo linux: SysV quando cada agregado de argumento e o retorno agregado trazem descritor (anexado em bir_construir_chamada.c). Sem descritor
 * segue Win64, como o trampolim da mesma assinatura (bir_construir_fronteira.c AssinaturaSysv). */
static bool ChamadaIndSysv(const BirInstr *i)
{
    if (i->nargs > ABI_SYSV_MAX_ARGS)
        return false;
    for (uint32_t k = 0; k < i->nargs; k++) {
        if (i->args[k].tam != 0 && i->args[k].sysv == NULL)
            return false;
    }
    return i->a.tipo == BO_NADA || i->a.sysv != NULL;
}

bool SysvEhChamadaDoPrograma(const BirPrograma *p, const BirInstr *i)
{
    if (!p->sysv)
        return false;
    if (i->op == BIR_CHAMA_IND)
        return ChamadaIndSysv(i);
    return i->op == BIR_CHAMA && p->simbolos[i->simbolo].tipo == BS_IMPORT;
}

/* Bytes de pilha que a chamada SysV pede (o quadro reserva); 0 se o plano nao fecha (o gerador recusa depois). */
uint32_t SysvBytesDePilha(const BirInstr *i)
{
    EntradaSysv entrada;
    AbiSysvPlano plano;

    return PlanejarSysv(i, &entrada, &plano) ? plano.bytes_pilha : 0;
}

/* ---- argumentos ---- */

/* Escalar na pilha: 8 bytes (real pelo xmm0, que nao e registrador de argumento ainda nao posto). */
static bool EmpilharEscalar(X64Ctx *c, const BirOperando *o, uint32_t off)
{
    if (BirClasseEhReal(o->classe))
        return IselCarregarReal(c, 0, o) && IselGuardarXmm(c, o->classe, MMem(X64_RSP, (int32_t)off, 64), 0);
    return IselCarregar(c, X64_RAX, o, o->extensao) && X64Emitir(c, MI2(FX_MOV64_MR, MMem(X64_RSP, (int32_t)off, 64), MReg(X64_RAX, 64)));
}

/* Agregado na pilha: a copia sai de 8 em 8 pelo rax (r11 e o endereco da origem). A origem pode ser lida ate 7 bytes alem do fim (o slot e do quadro). */
static bool CopiarAgregadoParaPilha(X64Ctx *c, const BirOperando *o, uint32_t off, uint32_t tam)
{
    if (!IselCarregar(c, X64_R11, o, BE_NENHUMA))
        return false;
    for (uint32_t j = 0; j < tam; j += SYSV_BYTES_OITO) {
        if (!X64Emitir(c, MI2(FX_MOV64_RM, MReg(X64_RAX, 64), MMem(X64_R11, (int32_t)j, 64))))
            return false;
        if (!X64Emitir(c, MI2(FX_MOV64_MR, MMem(X64_RSP, (int32_t)(off + j), 64), MReg(X64_RAX, 64))))
            return false;
    }
    return true;
}

static bool PassarNaPilha(X64Ctx *c, const BirInstr *i, const AbiSysvPlano *p)
{
    for (uint32_t k = 0; k < i->nargs; k++) {
        const BirOperando *o = &i->args[k];

        if (!p->arg[k].na_memoria)
            continue;
        if (o->tam == 0 && !EmpilharEscalar(c, o, p->off[k]))
            return false;
        if (o->tam != 0 && !CopiarAgregadoParaPilha(c, o, p->off[k], o->tam))
            return false;
    }
    return true;
}

static bool CarregarEightbyte(X64Ctx *c, AbiSysvReg reg, uint32_t desloc)
{
    if (reg >= ABI_SYSV_XMM0)
        return X64Emitir(c, MI2(FX_MOVSD_XM, MXmm((uint8_t)(reg - ABI_SYSV_XMM0)), MMem(X64_R11, (int32_t)desloc, 64)));
    return X64Emitir(c, MI2(FX_MOV64_RM, MReg(RegistradorDoSysv(reg), 64), MMem(X64_R11, (int32_t)desloc, 64)));
}

static bool CarregarAgregadoNosRegistradores(X64Ctx *c, const BirOperando *o, const AbiSysvLocal *loc)
{
    if (!IselCarregar(c, X64_R11, o, BE_NENHUMA))
        return false;
    for (uint32_t e = 0; e < loc->n_eightbytes; e++) {
        if (!CarregarEightbyte(c, loc->reg[e], SYSV_BYTES_OITO * e))
            return false;
    }
    return true;
}

static bool CarregarEscalarNoRegistrador(X64Ctx *c, const BirOperando *o, AbiSysvReg reg)
{
    if (reg >= ABI_SYSV_XMM0)
        return IselCarregarReal(c, (uint8_t)(reg - ABI_SYSV_XMM0), o);
    return IselCarregar(c, RegistradorDoSysv(reg), o, o->extensao);
}

/* Registradores: o ponteiro oculto do retorno vai em RDI antes dos argumentos (que so usam o que sobra). */
static bool PassarNosRegistradores(X64Ctx *c, const BirInstr *i, const AbiSysvPlano *p)
{
    if (p->oculto && !IselCarregar(c, X64_RDI, &i->a, BE_NENHUMA))
        return false;
    for (uint32_t k = 0; k < i->nargs; k++) {
        const BirOperando *o = &i->args[k];

        if (p->arg[k].na_memoria)
            continue;
        if (o->tam == 0 && !CarregarEscalarNoRegistrador(c, o, p->arg[k].reg[0]))
            return false;
        if (o->tam != 0 && !CarregarAgregadoNosRegistradores(c, o, &p->arg[k]))
            return false;
    }
    return true;
}

/* ---- chamada e retorno ---- */

static bool ChamarSysvPeloSlot(X64Ctx *c, const BirInstr *i)
{
    MOperando slot = MMem(X64_RIP, 0, 64);

    if (i->op == BIR_CHAMA_IND) /* o valor de um proc (o stub fjt_ ou um ponteiro de funcao C): r11 nao leva argumento */
        return IselCarregar(c, X64_R11, &i->b, BE_NENHUMA) && X64Emitir(c, MI1(FX_CALL_R64, MReg(X64_R11, 64)));
    slot.simbolo = c->obj_sim[i->simbolo];
    return X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_R11, 64), slot)) && X64Emitir(c, MI1(FX_CALL_M64, MMem(X64_R11, 0, 64)));
}

/* Parte do retorno que cabe em bytes (1..8) de uma origem: 8 de uma vez; resto em pedacos de 4, 2 e 1, deslocando a origem (so o ultimo eightbyte pode ser parcial). */
static bool EmitirGravacaoRetorno(X64Ctx *c, X64Reg origem, uint32_t desloc, uint32_t parte)
{
    switch (parte) {
    case 8: return X64Emitir(c, MI2(FX_MOV64_MR, MMem(X64_R11, (int32_t)desloc, 64), MReg(origem, 64)));
    case 4: return X64Emitir(c, MI2(FX_MOV32_MR, MMem(X64_R11, (int32_t)desloc, 32), MReg(origem, 32)));
    case 2: return X64Emitir(c, MI2(FX_MOV16_MR, MMem(X64_R11, (int32_t)desloc, 16), MReg(origem, 16)));
    default: return X64Emitir(c, MI2(FX_MOV8_MR, MMem(X64_R11, (int32_t)desloc, 8), MReg(origem, 8)));
    }
}

static uint32_t ParteDoRetorno(uint32_t resto)
{
    if (resto >= 8)
        return 8;
    return resto >= 4 ? 4 : (resto >= 2 ? 2 : 1);
}

static bool GravarParteDoRetorno(X64Ctx *c, X64Reg origem, uint32_t desloc, uint32_t bytes)
{
    uint32_t pos = 0;

    while (pos < bytes) {
        uint32_t parte = ParteDoRetorno(bytes - pos);

        if (!EmitirGravacaoRetorno(c, origem, desloc + pos, parte))
            return false;
        pos += parte;
        if (pos < bytes && !X64Emitir(c, MI2(FX_SHR64_RI, MReg(origem, 64), MImed((int64_t)parte * 8))))
            return false;
    }
    return true;
}

/* Retorno SSE sai do xmm por r10 (a origem do retorno, rax/rdx, nao e tocada). */
static bool OrigemDoRetorno(X64Ctx *c, AbiSysvReg reg, X64Reg *origem)
{
    if (reg < ABI_SYSV_XMM0) {
        *origem = RegistradorDoSysv(reg);
        return true;
    }
    *origem = X64_R10;
    return X64Emitir(c, MI2(FX_MOVQ_RX, MReg(X64_R10, 64), MXmm((uint8_t)(reg - ABI_SYSV_XMM0))));
}

/* O agregado devolvido em registradores vai para o endereco de destino (CHAMA.a), eightbyte a eightbyte. */
static bool GravarRetornoAgregadoSysv(X64Ctx *c, const BirInstr *i, const AbiSysvPlano *p)
{
    if (!IselCarregar(c, X64_R11, &i->a, BE_NENHUMA))
        return false;
    for (uint32_t e = 0; e < p->ret.n_eightbytes; e++) {
        uint32_t resto = i->a.tam - SYSV_BYTES_OITO * e;
        X64Reg origem;

        if (!OrigemDoRetorno(c, p->ret.reg[e], &origem))
            return false;
        if (!GravarParteDoRetorno(c, origem, SYSV_BYTES_OITO * e, resto < SYSV_BYTES_OITO ? resto : SYSV_BYTES_OITO))
            return false;
    }
    return true;
}

static bool ResultadoSysv(X64Ctx *c, const BirInstr *i, const AbiSysvPlano *p)
{
    if (i->a.tam != 0)
        return p->oculto || GravarRetornoAgregadoSysv(c, i, p);
    if (i->dst == BIR_SEM_VREG)
        return true;
    if (BirClasseEhReal(c->f->classe_vreg[i->dst]))
        return IselGuardarReal(c, i->dst, 0);
    return IselGuardar(c, i->dst, X64_RAX);
}

/* CHAMA a foreign de fora no alvo linux: planeja, passa a pilha, os registradores, al, chama e recolhe o retorno. */
bool IselChamaSysv(X64Ctx *c, const BirInstr *i)
{
    EntradaSysv entrada;
    AbiSysvPlano plano;

    if (!PlanejarSysv(i, &entrada, &plano))
        return X64SemRegra(c, "chama", "foreign SysV: argumento ou retorno agregado sem descritor (ou mais de 16 argumentos)");
    if (!PassarNaPilha(c, i, &plano) || !PassarNosRegistradores(c, i, &plano))
        return false;
    if (!X64Emitir(c, MI2(FX_MOV32_RI, MReg(X64_RAX, 32), MImed((int64_t)plano.al))))
        return false;
    if (!ChamarSysvPeloSlot(c, i))
        return false;
    return ResultadoSysv(c, i, &plano);
}
