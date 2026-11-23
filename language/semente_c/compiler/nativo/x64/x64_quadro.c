/* x64_quadro.c - quadro de pilha: tamanho, prologo, epilogo unico e ordem dos blocos.
 * Versao: 0.11.0
 * Changelog: 0.11.0 - F1 etapa 2: parametro inteiro que o alocador pos em registrador e uma posicao de registrador (0..3) vai direto do registrador de chegada
 *                    ao alocado (MoverParametrosInteiros, paralelo; r11 so no ciclo r8/r9). Sai da home. Real, agregado e posicao de pilha seguem como antes.
 *                    So no alvo Windows (c->p->sysv = alvo ELF fica igual).
 *            0.10.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.10.0 - M7a F4: xmm6..15 do alocador (Quadro.regs_salvos_xmm/salvos_xmm): movups no prologo/epilogo (MoverXmmDoAlocador), sem UWOP_SAVE_XMM128 (ver o comentario); parametro real
 *                    em xmm carrega da home por movss/movsd.
 *            0.9.0 - M7a F3: callee-saved do alocador: EmpurrarSalvos (push depois do mov rbp,rsp), DesempilharSalvos e lea rsp,[rbp-empurrados] no epilogo, CarregarParametrosEmRegistrador,
 *                    QuadroAlocacao (o sub rsp,N e a sondagem descontam os pushes; Quadro.tam segue sendo o quadro inteiro abaixo do rbp). Sem registradores salvos os bytes nao mudam.
 *            0.8.0 - M7a F2: Quadro.regs_salvos (AlocRegsSalvos, hoje 0); AlocOperandoDe no lugar de AlocSlot.
 *            0.7.0 - M6 P65b: X64DbgLocais (deslocamento em rbp dos locais nomeados, para o DbgPrograma).
 *            0.6.0 - M5b P37: QuadroCalcular(programa, funcao) reserva a area de saida da chamada SysV (PilhaDeSaida) e, na funcao com chamada SysV de
 *                    foreign, 176 B para guardar rsi, rdi e xmm6..15 (SalvarRegistradoresSysv no prologo, Restaurar no epilogo). Sem chamada SysV nada muda.
 *            0.5.2 - M3 F9b-1: decisoes extraidas de GuardarHome e QuadroPrologo.
 *            0.5.1 - M3 P21v-c: MaxArgsDoBloco conta o CHAMA_IND (area de argumentos da chamada indireta).
 *            0.5.0 - M2 P11b: o ponteiro oculto do retorno agregado ocupa a posicao 0 da home area (GuardarHome); PonteirosDosBytes (lea do endereco dos bytes do parametro agregado de 1/2/4/8
 *                    bytes no slot do vreg); os argumentos de pilha contam o ponteiro oculto (MaxArgsDoBloco).
 *            0.4.0 - M2 P11a: slots de memoria (BirFuncao.slots) abaixo dos slots de vreg, cada um alinhado ao seu alinhamento (QuadroPosicionarSlots); N = align16(vregs + memoria + 32 + pilha).
 *            0.3.0 - M2 P10b: o prologo grava na home area o xmm<k> (movss/movsd) do parametro real k < 4, no lugar de rcx/rdx/r8/r9.
 *            0.2.2 - portao M1: o comentario do laco da sondagem volta para SondarLaco (estava colado em EmitirLista).
 *            0.2.1 - backend x64 P8b: a sondagem marca X64Ctx.origem (so a listagem do --debug-code le).
 *            0.2.0 - backend x64 P8: sondagem em laco de tamanho fixo (r10 = rsp; r11d = N/4096; {sub r10,4096; test [r10],r10; sub r11d,1; jnz} e
 *                    um test [rsp-N],rax final); PrologoMarcas (fim do push, do mov rbp,rsp e do sub rsp,N) para o UNWIND_INFO.
 *            0.1.0 - backend x64 P7a: N = align16(slots + 32 + 8*max(0, maxargs-4)); prologo push rbp / mov rbp,rsp / [sondagem] / sub rsp,N /
 *                    guarda rcx,rdx,r8,r9 na home area; epilogo lea rsp,[rbp+0] / pop rbp / ret; ordem quentes, epilogo, frios.
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 *
 * Alinhamento: na entrada rsp = 8 mod 16; o push rbp leva a 0 mod 16 e N e multiplo de 16, entao rsp e multiplo de 16 em todo call.
 *
 * Sondagem de pilha (N >= 4096): logo depois de "mov rbp,rsp" e ANTES de "sub rsp,N", o laco de SondarLaco (r10/r11, volateis) e um
 * "test qword [rsp - N], rax" final. Cada test so le a pagina (nao grava nem muda rsp), na ordem em que a guard page cresce a pilha.
 * O unwind nao descreve essas instrucoes (nao mexem em rsp): o UNWIND_INFO marca o push, o mov rbp,rsp e o sub rsp,N por offset.
 * O formato de P7a (um test por pagina) crescia 8 bytes por pagina e estouraria o limite de 255 bytes do SizeOfProlog em frames grandes.
 *
 * Epilogo: o unwinder do Windows so reconhece "lea rsp,[rbp+disp]" ou "add rsp,N" seguido de pops e ret; "mov rsp,rbp" e "leave" nao valem.
 */
#include "nativo/x64/x64_interno.h"

#define QUADRO_ALINHAMENTO 16u
#define QUADRO_SALVOS_BYTES 176u /* M5b P37: rsi e rdi (8 B cada) e xmm6..xmm15 (16 B cada) */
#define QUADRO_SALVOS_XMM 10u
#define QUADRO_XMM_BYTES 16u /* M7a F4: movups de um xmm6..15 do alocador */
#define QUADRO_XMM_PRIMEIRO 6u
#define QUADRO_XMM_QTD 16u

static const X64Reg REGS_HOME[ABI_WIN_REGS_ARG] = {X64_RCX, X64_RDX, X64_R8, X64_R9};
static const X64Reg REGS_SALVOS_GP[2] = {X64_RSI, X64_RDI};

static uint32_t MaxArgsDoBloco(const BirBloco *b)
{
    uint32_t maximo = 0;

    for (uint32_t k = 0; k < b->n; k++) {
        const BirInstr *i = &b->instrs[k];
        if ((i->op == BIR_CHAMA || i->op == BIR_CHAMA_IND || i->op == BIR_ARMADILHA) && i->nargs + AbiWinOcultoDaChamada(i) > maximo)
            maximo = i->nargs + AbiWinOcultoDaChamada(i);
    }
    return maximo;
}

uint32_t QuadroPosicionarSlots(const BirFuncao *f, uint32_t *saida)
{
    uint32_t fim = AlocBytesSlots(f);

    for (uint32_t k = 0; k < f->nslots; k++) {
        uint32_t ali = f->slots[k].alinhamento;

        fim = (fim + f->slots[k].tam + ali - 1u) / ali * ali;
        if (saida != NULL)
            saida[k] = fim;
    }
    return fim;
}

/* Local do usuario -> variavel do DbgPrograma: slot >= 0 vence (o dado mora no slot, [rbp - slot_off]); senao o home do vreg; indireto vem da casa. */
static DbgVar VariavelDaCasa(const X64Ctx *c, const BirLocalCasa *k)
{
    DbgVar v = {.nome = k->nome, .tipo_nome = k->tipo_nome, .tipo = k->tipo, .local_id = k->local_id, .parametro = k->local_id < c->f->nparams};

    v.loc.tipo = DBG_LOC_RBP;
    if (k->slot >= 0) {
        v.loc.rbp_off = -(int32_t)c->slot_off[k->slot];
    } else {
        v.loc.rbp_off = AlocOperandoDe(c->f, (uint32_t)k->vreg, 64).desloc;
        v.loc.indireto = k->indireto;
    }
    return v;
}

void X64DbgLocais(const X64Ctx *c)
{
    for (uint32_t j = 0; j < c->f->ncasas; j++) {
        DbgVar v = VariavelDaCasa(c, &c->f->casas[j]);

        DbgVariavel(c->dbg, &v);
    }
}

/* Uma chamada SysV de foreign de fora: se a funcao tem alguma, e quanta pilha a maior delas pede (M5b P37). */
typedef struct { bool chama; uint32_t pilha; } VarreduraSysv;

static void VarrerBlocoSysv(const BirPrograma *p, const BirBloco *b, VarreduraSysv *v)
{
    for (uint32_t k = 0; k < b->n; k++) {
        const BirInstr *i = &b->instrs[k];
        uint32_t pilha;

        if (!SysvEhChamadaDoPrograma(p, i))
            continue;
        pilha = SysvBytesDePilha(i);
        v->chama = true;
        if (pilha > v->pilha)
            v->pilha = pilha;
    }
}

static VarreduraSysv VarrerSysv(const BirPrograma *p, const BirFuncao *f)
{
    VarreduraSysv v = {0};

    for (uint32_t b = 0; b < f->nblocos; b++)
        VarrerBlocoSysv(p, &f->blocos[b], &v);
    return v;
}

/* Bytes de pilha da area de saida: a SysV nao usa a sombra de 32 B, entao o que passa de 32 B aumenta a area (M5b P37). */
static uint32_t PilhaDeSaida(const Quadro *q, const VarreduraSysv *sysv)
{
    uint32_t pilha = q->maxargs > ABI_WIN_REGS_ARG ? ABI_WIN_BYTES_ARG * (q->maxargs - ABI_WIN_REGS_ARG) : 0;

    if (sysv->pilha > ABI_WIN_SOMBRA && sysv->pilha - ABI_WIN_SOMBRA > pilha)
        pilha = sysv->pilha - ABI_WIN_SOMBRA;
    return pilha;
}

Quadro QuadroCalcular(const BirPrograma *p, const BirFuncao *f)
{
    Quadro q = {.slots = AlocBytesSlots(f)};
    VarreduraSysv sysv = VarrerSysv(p, f);
    uint32_t pilha;

    q.agregados = QuadroPosicionarSlots(f, NULL) - q.slots;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        uint32_t n = MaxArgsDoBloco(&f->blocos[b]);
        if (n > q.maxargs)
            q.maxargs = n;
    }
    pilha = PilhaDeSaida(&q, &sysv);
    q.salva = sysv.chama;
    q.regs_salvos = AlocRegsSalvos(f);
    q.empurrados = AlocBytesEmpurrados(f);
    q.salvos = q.salva ? QUADRO_SALVOS_BYTES : 0;
    q.regs_salvos_xmm = AlocRegsSalvosXmm(f);
    q.salvos_xmm = QUADRO_XMM_BYTES * (uint32_t)__builtin_popcount(q.regs_salvos_xmm);
    q.tam = (q.slots + q.agregados + q.salvos + q.salvos_xmm + ABI_WIN_SOMBRA + pilha + QUADRO_ALINHAMENTO - 1u) / QUADRO_ALINHAMENTO * QUADRO_ALINHAMENTO;
    q.sondar = QuadroAlocacao(&q) >= X64_PAGINA;
    return q;
}

uint32_t QuadroAlocacao(const Quadro *q)
{
    return q->tam - q->empurrados;
}

void QuadroOrdenar(const BirFuncao *f, uint32_t *ordem)
{
    uint32_t n = 0;

    for (uint32_t b = 0; b < f->nblocos; b++) {
        if (!f->blocos[b].frio)
            ordem[n++] = b;
    }
    ordem[n++] = f->nblocos; /* o epilogo ocupa o lugar de um bloco de indice nblocos */
    for (uint32_t b = 0; b < f->nblocos; b++) {
        if (f->blocos[b].frio)
            ordem[n++] = b;
    }
}

uint32_t QuadroProximoBloco(const X64Ctx *c)
{
    return c->pos_ordem + 1 < c->nordem ? c->ordem[c->pos_ordem + 1] : BIR_SEM_BLOCO;
}

static bool EmitirLista(X64Ctx *c, const MInstr *lista, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        if (!X64Emitir(c, lista[k]))
            return false;
    }
    return true;
}

/* Laco da sondagem: r10 desce uma pagina por volta e r11d conta as voltas; o rsp nao muda. Tamanho fixo (37 bytes), por isso o prologo
 * cabe nos 255 bytes que o SizeOfProlog e os CodeOffset do UNWIND_INFO aceitam, qualquer que seja N. */
static bool SondarLaco(X64Ctx *c)
{
    uint32_t laco = c->f->nblocos + 1;
    const MInstr inicio[] = {
        MI2(FX_MOV64_RR, MReg(X64_R10, 64), MReg(X64_RSP, 64)),
        MI2(FX_MOV32_RI, MReg(X64_R11, 32), MImed(QuadroAlocacao(&c->quadro) / X64_PAGINA)),
    };
    const MInstr volta[] = {
        MI2(FX_SUB64_RI, MReg(X64_R10, 64), MImed(X64_PAGINA)),
        MI2(FX_TEST64_MR, MMem(X64_R10, 0, 64), MReg(X64_R10, 64)),
        MI2(FX_SUB32_RI, MReg(X64_R11, 32), MImed(1)),
        MICc(FX_JCC_REL32, X64_CC_NE, MBloco(laco)),
    };

    if (!EmitirLista(c, inicio, 2))
        return false;
    c->bloco_off[laco] = X64Offset(c);
    return EmitirLista(c, volta, 4);
}

/* N >= 4096: sonda as paginas de rsp-4096 ate rsp-4096*floor(N/4096) em ordem (a guard page cresce a pilha uma de cada vez) e
 * por fim o proprio rsp-N, que pode cair numa pagina a mais. Tudo ANTES do sub rsp,N e so com r10, r11 e leitura de memoria. */
static bool Sondar(X64Ctx *c)
{
    MInstr ultima = MI2(FX_TEST64_MR, MMem(X64_RSP, -(int32_t)QuadroAlocacao(&c->quadro), 64), MReg(X64_RAX, 64));
    bool ok;

    c->origem = ORIGEM_SONDAGEM;
    ok = SondarLaco(c) && X64Emitir(c, ultima);
    c->origem = ORIGEM_PROLOGO;
    return ok;
}

/* A posicao k na home area: real = o xmm da mesma posicao (movss/movsd); senao o registrador inteiro. oculto = posicoes do ponteiro de retorno. */
static bool GuardarPosicaoNaHome(X64Ctx *c, uint32_t k, uint32_t oculto)
{
    uint8_t classe = k < oculto ? (uint8_t)BC_I64 : c->f->classe_vreg[k - oculto];
    MInstr guarda = MI2(FX_MOV64_MR, MMem(X64_RBP, AbiWinParamRbp(k), 64), MReg(REGS_HOME[k], 64));

    if (BirClasseEhReal(classe))
        return IselGuardarXmm(c, classe, MMem(X64_RBP, AbiWinParamRbp(k), AlocBits(classe)), (uint8_t)k);
    return X64Emitir(c, guarda);
}

/* Parametro inteiro que o alocador pos em registrador e chega numa posicao de registrador (0..3): vai direto, sem passar pela home. */
/* So no alvo Windows: no alvo Linux (ELF) o prologo fica como antes, byte a byte. */
static bool ParametroDiretoEmRegistrador(const X64Ctx *c, uint32_t k)
{
    const BirFuncao *f = c->f;
    uint32_t oculto = AbiWinOculto(f);

    if (c->p->sysv || k >= f->nparams || k + oculto >= ABI_WIN_REGS_ARG || BirClasseEhReal(f->classe_vreg[k]))
        return false;
    return AlocOperandoDe(f, k, 64).em_reg;
}

/* A posicao k (o ponteiro oculto ou um parametro sem o caminho direto) e guardada na home. */
static bool PosicaoVaiParaHome(const X64Ctx *c, uint32_t k, uint32_t oculto)
{
    return k < oculto || !ParametroDiretoEmRegistrador(c, k - oculto);
}

/* Posicoes 0..3 na home area; a posicao 0 e o ponteiro oculto do retorno agregado, se houver (os parametros andam uma posicao). */
static bool GuardarHome(X64Ctx *c)
{
    uint32_t oculto = AbiWinOculto(c->f);
    uint32_t n = c->f->nparams + oculto < ABI_WIN_REGS_ARG ? c->f->nparams + oculto : ABI_WIN_REGS_ARG;

    for (uint32_t k = 0; k < n; k++) {
        if (PosicaoVaiParaHome(c, k, oculto) && !GuardarPosicaoNaHome(c, k, oculto))
            return false;
    }
    return true;
}

/* Parametro agregado de 1/2/4/8 bytes: os bytes ja estao na home area/pilha da posicao; o vreg (slot proprio) recebe o endereco dela. */
static bool PonteirosDosBytes(X64Ctx *c)
{
    uint32_t oculto = AbiWinOculto(c->f);

    for (uint32_t k = 0; k < c->f->nparams; k++) {
        if (!AbiWinParamEmBytes(c->f, k))
            continue;
        if (!X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RAX, 64), MMem(X64_RBP, AbiWinParamRbp(k + oculto), 64))) || !IselGuardar(c, k, X64_RAX))
            return false;
    }
    return true;
}

/* Distancia desde o inicio da funcao ate o fim da instrucao que acabou de sair. */
static uint32_t FimDaInstrucao(const X64Ctx *c)
{
    return X64Offset(c) - c->inicio;
}

/* push rbp / mov rbp,rsp, com as marcas do fim de cada um para o UNWIND_INFO. */
static bool SalvarRbp(X64Ctx *c)
{
    if (!X64Emitir(c, MI1(FX_PUSH64_R, MReg(X64_RBP, 64))))
        return false;
    c->prologo.push = FimDaInstrucao(c);
    if (!X64Emitir(c, MI2(FX_MOV64_RR, MReg(X64_RBP, 64), MReg(X64_RSP, 64))))
        return false;
    c->prologo.fpreg = FimDaInstrucao(c);
    return true;
}

/* M5b P37: rsi, rdi e xmm6..15 sao callee-saved no Win64 (quem chama Forja conta com eles) e o chamado SysV os destroi. A funcao que
 * tem chamada SysV guarda-os no quadro (depois do sub rsp, antes do corpo) e restaura-os antes do lea rsp do epilogo. */
static int32_t DeslocSalvoGP(const Quadro *q, uint32_t k)
{
    return -(int32_t)(q->slots + q->agregados + X64_SLOT_BYTES * (k + 1u));
}

static int32_t DeslocSalvoXmm(const Quadro *q, uint32_t j)
{
    return -(int32_t)(q->slots + q->agregados + 2u * X64_SLOT_BYTES + 16u * (j + 1u));
}

static bool SalvarRegistradoresSysv(X64Ctx *c)
{
    for (uint32_t k = 0; k < 2; k++) {
        if (!X64Emitir(c, MI2(FX_MOV64_MR, MMem(X64_RBP, DeslocSalvoGP(&c->quadro, k), 64), MReg(REGS_SALVOS_GP[k], 64))))
            return false;
    }
    for (uint32_t j = 0; j < QUADRO_SALVOS_XMM; j++) {
        if (!X64Emitir(c, MI2(FX_MOVUPS_MX, MMem(X64_RBP, DeslocSalvoXmm(&c->quadro, j), 128), MXmm((uint8_t)(6u + j)))))
            return false;
    }
    return true;
}

static bool RestaurarRegistradoresSysv(X64Ctx *c)
{
    for (uint32_t k = 0; k < 2; k++) {
        if (!X64Emitir(c, MI2(FX_MOV64_RM, MReg(REGS_SALVOS_GP[k], 64), MMem(X64_RBP, DeslocSalvoGP(&c->quadro, k), 64))))
            return false;
    }
    for (uint32_t j = 0; j < QUADRO_SALVOS_XMM; j++) {
        if (!X64Emitir(c, MI2(FX_MOVUPS_XM, MXmm((uint8_t)(6u + j)), MMem(X64_RBP, DeslocSalvoXmm(&c->quadro, j), 128))))
            return false;
    }
    return true;
}

/* M7a F3: os callee-saved que o alocador usa (mascara, bit = numero do registrador) sao empurrados em ordem crescente logo depois do mov rbp,rsp, abaixo do rbp (os slots
 * de vreg comecam depois deles); salvo[k] marca o fim de cada push para o UNWIND_INFO (UWOP_PUSH_NONVOL). */
static bool EmpurrarSalvos(X64Ctx *c)
{
    uint32_t k = 0;

    for (uint32_t reg = 0; reg < 16; reg++) {
        if (((c->quadro.regs_salvos >> reg) & 1u) == 0)
            continue;
        if (!X64Emitir(c, MI1(FX_PUSH64_R, MReg((X64Reg)reg, 64))))
            return false;
        c->prologo.salvo[k++] = FimDaInstrucao(c);
    }
    return true;
}

/* M7a F4: os xmm6..15 que o alocador usa (mascara, bit = numero do xmm) ficam no quadro, depois da area SysV: movups no prologo (antes de os parametros chegarem a eles) e no
 * epilogo. O UNWIND_INFO nao os descreve: UWOP_SAVE_XMM128 e relativo a rbp - FrameOffset*16 (deslocamento sem sinal) e o quadro desta Forja mora abaixo do rbp com FrameOffset 0
 * (mesmo estado do salvamento SysV acima); o unwinder so os perderia ao desenrolar para um frame que sobrevive, e a Forja nao tem handler que retome. */
static int32_t DeslocSalvoXmmAlocador(const Quadro *q, uint32_t k)
{
    return -(int32_t)(q->slots + q->agregados + q->salvos + QUADRO_XMM_BYTES * (k + 1u));
}

static bool MoverXmmDoAlocador(X64Ctx *c, bool salvar)
{
    uint32_t k = 0;

    for (uint32_t reg = QUADRO_XMM_PRIMEIRO; reg < QUADRO_XMM_QTD; reg++) {
        MOperando memoria = MMem(X64_RBP, DeslocSalvoXmmAlocador(&c->quadro, k), 128);
        MInstr mover = salvar ? MI2(FX_MOVUPS_MX, memoria, MXmm((uint8_t)reg)) : MI2(FX_MOVUPS_XM, MXmm((uint8_t)reg), memoria);

        if (((c->quadro.regs_salvos_xmm >> reg) & 1u) == 0)
            continue;
        if (!X64Emitir(c, mover))
            return false;
        k++;
    }
    return true;
}

/* Os pops, em ordem inversa dos pushes, depois do lea rsp,[rbp - empurrados]. */
static bool DesempilharSalvos(X64Ctx *c)
{
    for (uint32_t reg = 16; reg-- > 0;) {
        if (((c->quadro.regs_salvos >> reg) & 1u) != 0 && !X64Emitir(c, MI1(FX_POP64_R, MReg((X64Reg)reg, 64))))
            return false;
    }
    return true;
}

/* Parametro que o alocador pos em registrador: le da home area (GuardarHome ja gravou) na largura da classe; I8 e I16 com zero a esquerda (invariante do alocador). */
static bool CarregarParametroEmRegistrador(X64Ctx *c, uint32_t k, X64Reg reg)
{
    uint8_t classe = c->f->classe_vreg[k];

    if (BirClasseEhReal(classe)) /* real: reg e o numero do xmm; movss/movsd da memoria zeram o alto */
        return X64Emitir(c, MI2(classe == BC_F64 ? FX_MOVSD_XM : FX_MOVSS_XM, MXmm((uint8_t)reg), AlocHomeDoParametro(c->f, k, AlocBits(classe))));
    switch (AlocBits(classe)) {
    case 64:
        return X64Emitir(c, MI2(FX_MOV64_RM, MReg(reg, 64), AlocHomeDoParametro(c->f, k, 64)));
    case 32:
        return X64Emitir(c, MI2(FX_MOV32_RM, MReg(reg, 32), AlocHomeDoParametro(c->f, k, 32)));
    case 16:
        return X64Emitir(c, MI2(FX_MOVZX32_RM16, MReg(reg, 32), AlocHomeDoParametro(c->f, k, 16)));
    default:
        return X64Emitir(c, MI2(FX_MOVZX32_RM8, MReg(reg, 32), AlocHomeDoParametro(c->f, k, 8)));
    }
}

/* Parametro em registrador que nao tem o caminho direto: le da home (real, agregado ou posicao de pilha). */
static bool CarregarParametrosEmRegistrador(X64Ctx *c)
{
    for (uint32_t k = 0; k < c->f->nparams; k++) {
        AlocOperando vreg = AlocOperandoDe(c->f, k, 64);

        if (vreg.em_reg && !ParametroDiretoEmRegistrador(c, k) && !CarregarParametroEmRegistrador(c, k, vreg.reg))
            return false;
    }
    return true;
}

/* Um movimento do prologo: origem = registrador de chegada (REGS_HOME), destino = registrador do alocador. */
typedef struct {
    X64Reg origem, destino;
    uint8_t classe;
    bool feito;
} MovParametro;

static uint32_t ListarMovimentosDeParametros(const X64Ctx *c, MovParametro *mov)
{
    uint32_t oculto = AbiWinOculto(c->f);
    uint32_t n = 0;

    for (uint32_t k = 0; k < c->f->nparams; k++) {
        if (!ParametroDiretoEmRegistrador(c, k))
            continue;
        mov[n++] = (MovParametro){.origem = REGS_HOME[k + oculto], .destino = AlocOperandoDe(c->f, k, 64).reg, .classe = c->f->classe_vreg[k]};
    }
    return n;
}

/* Copia um inteiro do registrador de origem ao destino na largura da classe: I8 e I16 com zero a esquerda (invariante do alocador). */
static bool EmitirMovParametro(X64Ctx *c, const MovParametro *m)
{
    switch (AlocBits(m->classe)) {
    case 64:
        return X64Emitir(c, MI2(FX_MOV64_RR, MReg(m->destino, 64), MReg(m->origem, 64)));
    case 32:
        return X64Emitir(c, MI2(FX_MOV32_RR, MReg(m->destino, 32), MReg(m->origem, 32)));
    case 16:
        return X64Emitir(c, MI2(FX_MOVZX32_RR16, MReg(m->destino, 32), MReg(m->origem, 16)));
    default:
        return X64Emitir(c, MI2(FX_MOVZX32_RR8, MReg(m->destino, 32), MReg(m->origem, 8)));
    }
}

/* O destino de mov[j] ainda e origem de outro movimento pendente: mov[j] espera. */
static bool DestinoAindaLido(const MovParametro *mov, uint32_t n, uint32_t j)
{
    for (uint32_t k = 0; k < n; k++) {
        if (k != j && !mov[k].feito && mov[k].origem == mov[j].destino)
            return true;
    }
    return false;
}

/* Ciclo (so r8 e r9 podem formar): o destino do primeiro pendente ainda tem o valor de chegada; vai para r11 e os pendentes que o leem passam a ler r11. */
static bool SalvarOrigemEmR11(X64Ctx *c, MovParametro *mov, uint32_t n)
{
    X64Reg presa = X64_R11;

    for (uint32_t k = 0; k < n; k++) {
        if (!mov[k].feito) {
            presa = mov[k].destino;
            break;
        }
    }
    if (!X64Emitir(c, MI2(FX_MOV64_RR, MReg(X64_R11, 64), MReg(presa, 64))))
        return false;
    for (uint32_t k = 0; k < n; k++) {
        if (!mov[k].feito && mov[k].origem == presa)
            mov[k].origem = X64_R11;
    }
    return true;
}

/* Emite os movimentos cujo destino nao e mais origem de outro pendente; *emitidos conta quantos sairam. */
static bool EmitirMovimentosProntos(X64Ctx *c, MovParametro *mov, uint32_t n, uint32_t *emitidos)
{
    *emitidos = 0;
    for (uint32_t j = 0; j < n; j++) {
        if (mov[j].feito || DestinoAindaLido(mov, n, j))
            continue;
        if (!EmitirMovParametro(c, &mov[j]))
            return false;
        mov[j].feito = true;
        (*emitidos)++;
    }
    return true;
}

/* Os inteiros que vao direto: ordem que nao pisa origem ainda lida; so um ciclo cabe, por isso um unico r11. */
static bool MoverParametrosInteiros(X64Ctx *c)
{
    MovParametro mov[ABI_WIN_REGS_ARG];
    uint32_t n = ListarMovimentosDeParametros(c, mov);
    uint32_t pendentes = n;
    bool r11_usado = false;

    while (pendentes > 0) {
        uint32_t emitidos = 0;

        if (!EmitirMovimentosProntos(c, mov, n, &emitidos))
            return false;
        pendentes -= emitidos;
        if (emitidos > 0)
            continue;
        if (r11_usado)
            return X64SemRegra(c, "prologo", "dois ciclos de parametros em registrador (fora do degrau do alocador)");
        if (!SalvarOrigemEmR11(c, mov, n))
            return false;
        r11_usado = true;
    }
    return true;
}

bool QuadroPrologo(X64Ctx *c)
{
    if (!SalvarRbp(c) || !EmpurrarSalvos(c))
        return false;
    if (c->quadro.sondar && !Sondar(c))
        return false;
    if (!X64Emitir(c, MI2(FX_SUB64_RI, MReg(X64_RSP, 64), MImed(QuadroAlocacao(&c->quadro)))))
        return false;
    c->prologo.alocacao = FimDaInstrucao(c);
    return GuardarHome(c) && PonteirosDosBytes(c) && MoverXmmDoAlocador(c, true) && MoverParametrosInteiros(c) && CarregarParametrosEmRegistrador(c) && (!c->quadro.salva || SalvarRegistradoresSysv(c));
}

bool QuadroEpilogo(X64Ctx *c)
{
    if (c->quadro.salva && !RestaurarRegistradoresSysv(c))
        return false;
    if (!MoverXmmDoAlocador(c, false))
        return false;
    if (!X64Emitir(c, MI2(FX_LEA64_RM, MReg(X64_RSP, 64), MMem(X64_RBP, -(int32_t)c->quadro.empurrados, 64))))
        return false;
    if (!DesempilharSalvos(c) || !X64Emitir(c, MI1(FX_POP64_R, MReg(X64_RBP, 64))))
        return false;
    return X64Emitir(c, MI0(FX_RET));
}
