/* x64_isel_fluxo.c - selecao de instrucoes do fluxo: CHAMA, ARMADILHA, SALTA, DESVIA e RETORNA (as regras de valor estao em x64_isel.c).
 * Versao: 0.9.0
 * Changelog: 0.9.0 - perigo exato (D0737): so conta como perigo o escritor posto ANTES da leitura da origem (EscritorAntesDaLeitura: argumentos por posicao, depois o
 *                    ponteiro oculto, depois o alvo de CHAMA_IND); escritor depois da leitura nao pisa nada e deixa de forcar o desvio pelo slot.
 *            0.8.0 - F1 etapa 1: movimento paralelo dos argumentos. Se uma origem em registrador seria pisada por outro argumento de registrador (ou pelo
 *                    ponteiro oculto / alvo de CHAMA_IND), todas as origens em registrador da chamada vao ao slot do proprio vreg antes e voltam dali (PlanoChamada.perigo).
 *                    Sem perigo, o codigo sai igual.
 *            0.7.2 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.7.1 - M7a F2: IselDesvia le o vreg por AlocCarregar (AlocOperandoDe).
 *            0.7.0 - M5b P37: CHAMA a BS_IMPORT no alvo linux (SysvEhChamadaDoPrograma) vai para IselChamaSysv (ABI SysV, x64_chamada_sysv.c).
 *            0.6.1 - M3 F9b-1: decisoes extraidas de EmitirChamada.
 *            0.6.0 - M3 P21v-c: CHAMA_IND (IselChama serve aos dois): mov rax,[slot do alvo] ; call rax (FX_CALL_R64), depois dos argumentos; mesma classificacao Win64, sombra e retorno agregado.
 *            0.5.0 - M3 P19: CHAMA a um BS_IMPORT (foreign de DLL) = lea rax,[rip+__imp_x] ; call qword [rax] (FX_CALL_M64); os argumentos e a sombra de 32 B sao os mesmos da chamada ao runtime.
 *            0.4.0 - M2 P18b: IselRetorna chama IselPlantio (so teste: o retorno de forja_programa soma 1 com FORJA_TESTE_PLANTAR=3).
 *            0.3.0 - M2 P11b: agregado por valor: argumento de 1/2/4/8 bytes lido do endereco para o registrador/slot da posicao (outro tam: o ponteiro da copia segue como valor), retorno
 *                    agregado por RAX (1/2/4/8) ou ponteiro oculto em RCX (os visiveis andam uma posicao; o chamado copia ao destino e devolve-o em RAX).
 *            0.2.0 - M2 P10b: argumento real na pilha por movss/movsd (via xmm0); real em registrador, no retorno e no resultado da chamada passam pelo IselCarregar/IselGuardar.
 *            0.1.0 - backend x64 P7a: chamada ABI Windows (pilha primeiro, depois RCX/RDX/R8/R9, call rel32 com OR_REL32), ARMADILHA = call + ud2,
 *                    SALTA/DESVIA que omitem o jmp para o bloco seguinte, RETORNA em rax com salto ao epilogo unico.
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 *
 * Os argumentos de pilha saem antes dos de registrador: a carga de cada um passa por rax. Os de registrador leem origem em registrador
 * (vreg no R8/R9 ou xmm2..3): sem perigo, a ordem por posicao basta; com perigo (PlanoChamada) a origem sai do slot do proprio vreg.
 */
#include "nativo/x64/x64_interno.h"

bool IselSalto(X64Ctx *c, uint32_t bloco)
{
    if (bloco == QuadroProximoBloco(c))
        return true;
    return X64Emitir(c, MI1(FX_JMP_REL32, MBloco(bloco)));
}

/* ---- chamada ---- */

/* O argumento ja carregado em rax (ou xmm0, se real) vai para [rsp+desloc]. */
static bool GuardarArgumentoDePilha(X64Ctx *c, const BirOperando *arg, int32_t desloc)
{
    if (BirClasseEhReal(arg->classe))
        return IselGuardarXmm(c, arg->classe, MMem(X64_RSP, desloc, AlocBits(arg->classe)), 0);
    return X64Emitir(c, MI2(FX_MOV64_MR, MMem(X64_RSP, desloc, 64), MReg(X64_RAX, 64)));
}

/* ---- perigo de registrador: origem em registrador pisada por outro argumento de registrador ---- */

/* Plano da chamada: perigo = alguma origem em registrador seria lida depois de um argumento de registrador ter escrito o mesmo registrador. */
typedef struct {
    X64Ctx *c;
    const BirInstr *i;
    uint32_t oculto;
    bool perigo;
} PlanoChamada;

static bool EmRegistrador(const BirFuncao *f, const BirOperando *o)
{
    return o->tipo == BO_VREG && o->id < f->nvregs && AlocOperandoDe(f, o->id, 64).em_reg;
}

/* Perigo: o valor em registrador vai ao slot do proprio vreg (inteiro: 64 bits; real: a largura da classe) antes de qualquer argumento ser posto. */
static bool GuardarNoSlot(X64Ctx *c, const BirOperando *o)
{
    AlocOperando origem = AlocOperandoDe(c->f, o->id, 64);
    AlocOperando slot;

    if (o->classe == BC_V128)
        return X64SemRegra(c, "chama", "argumento V128 em registrador com perigo de registrador");
    slot = AlocOperandoDe(c->f, o->id, BirClasseEhReal(o->classe) ? AlocBits(o->classe) : 64);
    slot.em_reg = false;
    if (BirClasseEhReal(o->classe))
        return AlocGravar(c, o->classe == BC_F64 ? FX_MOVSD_MX : FX_MOVSS_MX, &slot, MXmm((uint8_t)origem.reg));
    return AlocGravar(c, FX_MOV64_MR, &slot, MReg(origem.reg, 64));
}

/* Le de volta do slot o valor que GuardarNoSlot guardou; a extensao segue a de CarregarVreg. */
static bool CarregarDoSlot(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext)
{
    AlocOperando slot = AlocOperandoDe(c->f, o->id, AlocBits(o->classe));

    slot.em_reg = false;
    if (BirClasseEhReal(o->classe))
        return AlocCarregar(c, o->classe == BC_F64 ? FX_MOVSD_XM : FX_MOVSS_XM, MXmm((uint8_t)reg), &slot);
    switch (o->classe) {
    case BC_I64:
        return AlocCarregar(c, FX_MOV64_RM, MReg(reg, 64), &slot);
    case BC_I32:
        return AlocCarregar(c, FX_MOV32_RM, MReg(reg, 32), &slot) && (ext != BE_SINAL || X64Emitir(c, MI2(FX_MOVSXD64_RR32, MReg(reg, 64), MReg(reg, 32))));
    case BC_I8:
        return ext == BE_SINAL ? AlocCarregar(c, FX_MOVSX64_RM8, MReg(reg, 64), &slot) : AlocCarregar(c, FX_MOVZX32_RM8, MReg(reg, 32), &slot);
    case BC_I16:
        return ext == BE_SINAL ? AlocCarregar(c, FX_MOVSX64_RM16, MReg(reg, 64), &slot) : AlocCarregar(c, FX_MOVZX32_RM16, MReg(reg, 32), &slot);
    default:
        return X64SemRegra(c, "vreg", "operando de classe fora do M2 (so i8, i16, i32 e i64)");
    }
}

/* Operando da chamada: com perigo, o que estiver em registrador volta do slot (ja guardado); o resto sai como sempre. */
static bool LerOrigem(const PlanoChamada *p, X64Reg reg, const BirOperando *o, uint8_t ext)
{
    if (p->perigo && EmRegistrador(p->c->f, o))
        return CarregarDoSlot(p->c, reg, o, ext);
    return IselCarregar(p->c, reg, o, ext);
}

/* O argumento em reg: o valor do vreg ou, agregado de 1/2/4/8 bytes, os bytes lidos do endereco que o vreg guarda (o ponteiro de outro agregado segue como valor). */
static bool CarregarArgumento(const PlanoChamada *p, X64Reg reg, const BirOperando *arg, const AbiLocal *onde)
{
    if (!LerOrigem(p, reg, arg, arg->extensao))
        return false;
    return onde->bytes == 0 || IselLerBytes(p->c, reg, reg, onde->bytes);
}

/* oculto = posicoes ocupadas pelo ponteiro de retorno: o argumento k mora na posicao oculto + k. */
static bool ColocarArgumentosDePilha(const PlanoChamada *p)
{
    for (uint32_t k = 0; k < p->i->nargs; k++) {
        const AbiLocal *onde = &p->c->abi[p->oculto + k];

        if (!onde->na_pilha)
            continue;
        if (!CarregarArgumento(p, X64_RAX, &p->i->args[k], onde))
            return false;
        if (!GuardarArgumentoDePilha(p->c, &p->i->args[k], onde->desloc))
            return false;
    }
    return true;
}

static bool ColocarArgumentosDeRegistrador(const PlanoChamada *p)
{
    for (uint32_t k = 0; k < p->i->nargs; k++) {
        const AbiLocal *onde = &p->c->abi[p->oculto + k];

        if (onde->na_pilha)
            continue;
        if (!CarregarArgumento(p, onde->reg, &p->i->args[k], onde))
            return false;
    }
    return true;
}

/* A posicao k e posta (escreve o registrador) antes de a origem da posicao pos ser lida? Ordem de emissao: argumentos de registrador por posicao
 * (de oculto em diante), depois o ponteiro oculto (posicao 0, so quando oculto != 0), depois o alvo de CHAMA_IND (pos = UINT32_MAX). */
static bool EscritorAntesDaLeitura(uint32_t k, uint32_t pos, uint32_t oculto)
{
    if (pos == UINT32_MAX || (oculto != 0 && pos == 0))
        return k != pos;
    return k >= oculto && k < pos;
}

/* Um argumento de registrador posto ANTES da leitura desta origem escreve o registrador dela, no mesmo banco (escritor depois da leitura nao pisa nada). */
static bool OrigemPisada(const PlanoChamada *p, const BirOperando *o, uint32_t pos)
{
    X64Reg origem = (X64Reg)AlocOperandoDe(p->c->f, o->id, 64).reg;
    uint32_t total = p->i->nargs + p->oculto;

    for (uint32_t k = 0; k < total; k++) {
        const AbiLocal *onde = &p->c->abi[k];
        bool mesmo_banco = BirClasseEhReal(p->c->cls[k].classe) == BirClasseEhReal(o->classe);

        if (EscritorAntesDaLeitura(k, pos, p->oculto) && !onde->na_pilha && mesmo_banco && onde->reg == origem)
            return true;
    }
    return false;
}

/* Perigo = um operando de registrador da chamada (argumento de registrador, ponteiro oculto ou alvo de CHAMA_IND) tem o registrador escrito por outro. */
static bool HaPerigoDeRegistrador(const PlanoChamada *p)
{
    for (uint32_t k = 0; k < p->i->nargs; k++) {
        const BirOperando *o = &p->i->args[k];

        if (!p->c->abi[p->oculto + k].na_pilha && EmRegistrador(p->c->f, o) && OrigemPisada(p, o, p->oculto + k))
            return true;
    }
    if (p->oculto != 0 && EmRegistrador(p->c->f, &p->i->a) && OrigemPisada(p, &p->i->a, 0))
        return true;
    return p->i->op == BIR_CHAMA_IND && EmRegistrador(p->c->f, &p->i->b) && OrigemPisada(p, &p->i->b, UINT32_MAX);
}

/* Com perigo, todas as origens em registrador da chamada vao ao slot antes de qualquer argumento ser posto. */
static bool GuardarOrigensEmRegistrador(const PlanoChamada *p)
{
    const BirInstr *i = p->i;

    for (uint32_t k = 0; k < i->nargs; k++) {
        if (EmRegistrador(p->c->f, &i->args[k]) && !GuardarNoSlot(p->c, &i->args[k]))
            return false;
    }
    if (p->oculto != 0 && EmRegistrador(p->c->f, &i->a) && !GuardarNoSlot(p->c, &i->a))
        return false;
    return i->op != BIR_CHAMA_IND || !EmRegistrador(p->c->f, &i->b) || GuardarNoSlot(p->c, &i->b);
}

/* Foreign de DLL (BS_IMPORT): lea rax,[rip+__imp_x] ; call qword [rax]. RAX nao leva argumento (RCX, RDX, R8, R9 e pilha), entao serve de ponte. */
static bool ChamarPeloSlot(X64Ctx *c, const BirInstr *i)
{
    MOperando slot = MMem(X64_RIP, 0, 64);

    slot.simbolo = c->obj_sim[i->simbolo];
    return X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RAX, 64), slot)) && X64Emitir(c, MI1(FX_CALL_M64, MMem(X64_RAX, 0, 64)));
}

static bool ChamarSimbolo(X64Ctx *c, const BirInstr *i)
{
    if (c->p->simbolos[i->simbolo].tipo == BS_IMPORT)
        return ChamarPeloSlot(c, i);
    return X64Emitir(c, MI1(FX_CALL_REL32, MSimbolo(c->obj_sim[i->simbolo])));
}

/* CHAMA_IND: o endereco do proc (b) vai para rax, que nao leva argumento, depois dos argumentos e do ponteiro oculto; call rax. */
static bool ChamarPeloEndereco(const PlanoChamada *p)
{
    return LerOrigem(p, X64_RAX, &p->i->b, BE_NENHUMA) && X64Emitir(p->c, MI1(FX_CALL_R64, MReg(X64_RAX, 64)));
}

/* Monta as classes dos argumentos (o ponteiro oculto, se houver, ocupa a posicao 0) e as posicoes na ABI. */
static bool ClassificarArgumentosDaChamada(X64Ctx *c, const BirInstr *i, uint32_t oculto)
{
    if (oculto != 0)
        c->cls[0] = (AbiArg){.classe = BC_I64};
    for (uint32_t k = 0; k < i->nargs; k++)
        c->cls[oculto + k] = (AbiArg){.classe = (BirClasse)i->args[k].classe, .tam = i->args[k].tam};
    if (!AbiWinClassificar(c->cls, i->nargs + oculto, c->abi))
        return X64SemRegra(c, "chama", "argumento de classe fora do M1");
    return true;
}

/* Pilha primeiro, depois os registradores e, com retorno agregado por ponteiro oculto, RCX = destino do retorno. */
static bool ColocarArgumentosDaChamada(const PlanoChamada *p)
{
    if (p->perigo && !GuardarOrigensEmRegistrador(p))
        return false;
    if (!ColocarArgumentosDePilha(p) || !ColocarArgumentosDeRegistrador(p))
        return false;
    return p->oculto == 0 || LerOrigem(p, p->c->abi[0].reg, &p->i->a, BE_NENHUMA);
}

static bool EmitirChamada(X64Ctx *c, const BirInstr *i)
{
    PlanoChamada p = {.c = c, .i = i, .oculto = AbiWinOcultoDaChamada(i)};

    if (!ClassificarArgumentosDaChamada(c, i, p.oculto))
        return false;
    p.perigo = HaPerigoDeRegistrador(&p);
    if (!ColocarArgumentosDaChamada(&p))
        return false;
    return i->op == BIR_CHAMA_IND ? ChamarPeloEndereco(&p) : ChamarSimbolo(c, i);
}

/* Retorno agregado: de 1/2/4/8 bytes volta em RAX e vai para [CHAMA.a]; os demais o chamado ja escreveu no destino do ponteiro oculto. */
static bool GuardarRetornoAgregado(X64Ctx *c, const BirInstr *i)
{
    if (AbiWinOcultoDaChamada(i) != 0)
        return true;
    return IselCarregar(c, X64_RCX, &i->a, BE_NENHUMA) && IselGravarBytes(c, X64_RCX, X64_RAX, i->a.tam);
}

bool IselChama(X64Ctx *c, const BirInstr *i)
{
    if (SysvEhChamadaDoPrograma(c->p, i))
        return IselChamaSysv(c, i);
    if (!EmitirChamada(c, i))
        return false;
    if (i->a.tipo == BO_VREG)
        return GuardarRetornoAgregado(c, i);
    return i->dst == BIR_SEM_VREG || IselGuardar(c, i->dst, ABI_WIN_REG_RETORNO);
}

bool IselArmadilha(X64Ctx *c, const BirInstr *i)
{
    return EmitirChamada(c, i) && X64Emitir(c, MI0(FX_UD2));
}

/* ---- terminadores ---- */

bool IselSalta(X64Ctx *c, const BirInstr *i)
{
    return IselSalto(c, i->alvo[0]);
}

/* movzx eax,byte [slot] / test eax,eax / jnz alvo0 / jmp alvo1; o jmp some se alvo1 e o proximo, e se alvo0 e o proximo vira jz alvo1. */
bool IselDesvia(X64Ctx *c, const BirInstr *i)
{
    uint32_t proximo = QuadroProximoBloco(c);
    AlocOperando cond = AlocOperandoDe(c->f, i->a.id, 8);

    if (!AlocCarregar(c, FX_MOVZX32_RM8, MReg(X64_RAX, 32), &cond))
        return false;
    if (!X64Emitir(c, MI2(FX_TEST32_RR, MReg(X64_RAX, 32), MReg(X64_RAX, 32))))
        return false;
    if (i->alvo[0] == proximo && i->alvo[1] != proximo)
        return X64Emitir(c, MICc(FX_JCC_REL32, X64_CC_E, MBloco(i->alvo[1])));
    if (!X64Emitir(c, MICc(FX_JCC_REL32, X64_CC_NE, MBloco(i->alvo[0]))))
        return false;
    return IselSalto(c, i->alvo[1]);
}

/* Agregado devolvido: de 1/2/4/8 bytes, os bytes lidos da origem vao para RAX; os demais sao copiados para o destino que o chamador passou em RCX (guardado na home da posicao 0) e
 * o destino volta em RAX. */
static bool RetornarAgregado(X64Ctx *c, const BirInstr *i)
{
    MOperando home = MMem(X64_RBP, AbiWinParamRbp(0), 64);

    if (AbiWinRetorno(BC_I64, i->a.tam) == ABI_RET_RAX)
        return IselCarregar(c, X64_RAX, &i->a, BE_NENHUMA) && IselLerBytes(c, X64_RAX, X64_RAX, i->a.tam);
    if (!IselCarregar(c, X64_RDX, &i->a, BE_NENHUMA) || !X64Emitir(c, MI2(FX_MOV64_RM, MReg(X64_RCX, 64), home)))
        return false;
    return IselBlocoMem(c, i->a.tam) && X64Emitir(c, MI2(FX_MOV64_RM, MReg(X64_RAX, 64), home));
}

static bool CarregarRetorno(X64Ctx *c, const BirInstr *i)
{
    if (i->a.tipo == BO_NADA)
        return true;
    if (i->a.tam != 0)
        return RetornarAgregado(c, i);
    return IselCarregar(c, ABI_WIN_REG_RETORNO, &i->a, BE_NENHUMA);
}

/* O epilogo e o bloco de indice nblocos. */
bool IselRetorna(X64Ctx *c, const BirInstr *i)
{
    if (!CarregarRetorno(c, i))
        return false;
    if (IselPlantio(c, i) == 3 && !X64Emitir(c, MI2(FX_ADD32_RI, MReg(X64_RAX, 32), MImed(1)))) /* so teste: codigo de saida + 1 */
        return false;
    return IselSalto(c, c->f->nblocos);
}
