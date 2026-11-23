/* x64_unwind.c - tabelas de unwind do Windows x64: UNWIND_INFO v1 em .xdata e RUNTIME_FUNCTION em .pdata, uma de cada por funcao.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M7a F3: UWOP_PUSH_NONVOL dos callee-saved do alocador (AnexarSalvos), empurrados depois do SET_FPREG e antes do sub rsp,N; o ALLOC descreve QuadroAlocacao. UNWIND_MAX_CODIGOS 6 -> 14.
 *            0.1.0 - backend x64 P8: X64UnwindRegistrar (PUSH_NONVOL rbp, SET_FPREG rbp+0, ALLOC_SMALL | ALLOC_LARGE, contagem par).
 * Verificacao: construir.bat && python -I ..\tests\nativo\unwind\conferir_unwind.py
 *
 * Toda funcao gerada tem o mesmo prologo (push rbp / mov rbp,rsp / [sondagem] / sub rsp,N), entao o UNWIND_INFO so varia no N e nos offsets.
 * UNWIND_CODE = {CodeOffset, OpInfo << 4 | UnwindOp}; CodeOffset e o fim da instrucao que fez a operacao, os codigos vao em ordem inversa
 * (a ultima operacao do prologo primeiro) e o array tem sempre numero par de slots (padding zerado, fora do CountOfCodes) para o
 * UNWIND_INFO ficar com tamanho multiplo de 4. Desvio do contrato: la "contagem par"; aqui o par e o array, nao o campo CountOfCodes.
 * Funcoes do runtime (rt_win.o) trazem os proprios .pdata/.xdata, que o ligador junta depois dos do programa.
 */
#include "nativo/x64/x64_interno.h"

#define UNWIND_VERSAO 1u
#define UNWIND_CABECALHO 4u
#define UNWIND_TAM_CODIGO 2u
#define UNWIND_MAX_CODIGOS 14u /* alloc grande (2) + 7 pushes de callee-saved + set_fpreg + push rbp, com folga para o padding par */
#define UNWIND_REGS 16u
#define UNWIND_TAM_MAX (UNWIND_CABECALHO + UNWIND_MAX_CODIGOS * UNWIND_TAM_CODIGO)
#define UNWIND_RBP 5u
#define UNWIND_PEQUENO_MAX 128u
#define UNWIND_GRANDE16_MAX (512u * 1024u - 8u)
#define UNWIND_ENTRADA_PDATA 12u
#define UNWIND_BYTES_PALAVRA 8u

typedef enum { UWOP_PUSH_NONVOL = 0, UWOP_ALLOC_LARGE = 1, UWOP_ALLOC_SMALL = 2, UWOP_SET_FPREG = 3 } UnwindOp;

typedef struct {
    uint8_t bytes[UNWIND_TAM_MAX];
    uint32_t tam;
    uint32_t codigos;
} UnwindInfo;

static void AnexarCodigo(UnwindInfo *u, uint8_t offset, uint8_t op_info, UnwindOp op)
{
    u->bytes[u->tam++] = offset;
    u->bytes[u->tam++] = (uint8_t)(op_info << 4 | op);
    u->codigos++;
}

/* Operando de 16 ou 32 bits que segue o UNWIND_CODE, em little-endian, ocupando slots de 2 bytes. */
static void AnexarOperando(UnwindInfo *u, uint32_t valor, uint32_t bytes)
{
    for (uint32_t k = 0; k < bytes; k++)
        u->bytes[u->tam++] = (uint8_t)(valor >> (k * UNWIND_BYTES_PALAVRA));
    u->codigos += bytes / UNWIND_TAM_CODIGO;
}

/* sub rsp,N: pequeno (8..128, info = N/8 - 1), grande de 16 bits (N/8 no operando, ate 512K-8) ou grande de 32 bits (N no operando). */
static void AnexarAlocacao(UnwindInfo *u, uint32_t tam, uint8_t offset)
{
    if (tam <= UNWIND_PEQUENO_MAX) {
        AnexarCodigo(u, offset, (uint8_t)((tam - UNWIND_BYTES_PALAVRA) / UNWIND_BYTES_PALAVRA), UWOP_ALLOC_SMALL);
    } else if (tam <= UNWIND_GRANDE16_MAX) {
        AnexarCodigo(u, offset, 0, UWOP_ALLOC_LARGE);
        AnexarOperando(u, tam / UNWIND_BYTES_PALAVRA, 2);
    } else {
        AnexarCodigo(u, offset, 1, UWOP_ALLOC_LARGE);
        AnexarOperando(u, tam, 4);
    }
}

/* Cabecalho (versao, SizeOfProlog, CountOfCodes, FrameRegister rbp + FrameOffset 0). O array tem de ter numero par de slots, mas o
 * CountOfCodes NAO conta o slot de padding (se contasse, o unwinder leria um "push rax" no offset 0). */
static void FecharCabecalho(UnwindInfo *u, uint8_t tam_prologo)
{
    uint32_t reais = u->codigos;

    if (reais % 2 != 0) {
        u->bytes[u->tam++] = 0;
        u->bytes[u->tam++] = 0;
    }
    u->bytes[0] = UNWIND_VERSAO;
    u->bytes[1] = tam_prologo;
    u->bytes[2] = (uint8_t)reais;
    u->bytes[3] = UNWIND_RBP;
}

/* Um UWOP_PUSH_NONVOL por callee-saved empurrado (M7a F3), do ultimo push para o primeiro: a ordem inversa do prologo. */
static void AnexarSalvos(UnwindInfo *u, const X64Ctx *c)
{
    uint32_t k = 0;

    for (uint32_t reg = 0; reg < UNWIND_REGS; reg++) {
        if (((c->quadro.regs_salvos >> reg) & 1u) != 0)
            k++;
    }
    for (uint32_t reg = UNWIND_REGS; reg-- > 0;) {
        if (((c->quadro.regs_salvos >> reg) & 1u) != 0)
            AnexarCodigo(u, (uint8_t)c->prologo.salvo[--k], (uint8_t)reg, UWOP_PUSH_NONVOL);
    }
}

static UnwindInfo MontarUnwind(const X64Ctx *c)
{
    UnwindInfo u = {.tam = UNWIND_CABECALHO};

    AnexarAlocacao(&u, QuadroAlocacao(&c->quadro), (uint8_t)c->prologo.alocacao);
    AnexarSalvos(&u, c);
    AnexarCodigo(&u, (uint8_t)c->prologo.fpreg, 0, UWOP_SET_FPREG);
    AnexarCodigo(&u, (uint8_t)c->prologo.push, UNWIND_RBP, UWOP_PUSH_NONVOL);
    FecharCabecalho(&u, (uint8_t)c->prologo.alocacao);
    return u;
}

/* RUNTIME_FUNCTION {BeginAddress, EndAddress, UnwindData}: tres campos de 32 bits, cada um uma relocacao ADDR32NB (RVA). */
static void RegistrarFuncao(X64Ctx *c, uint32_t fim, uint32_t offset_xdata)
{
    ObjSecao *pdata = VecEm(&c->o->secoes, c->pdata);
    static const char ZEROS[UNWIND_ENTRADA_PDATA] = {0};
    uint32_t base = (uint32_t)pdata->bytes.tam;

    BufAnexarN(&pdata->bytes, ZEROS, UNWIND_ENTRADA_PDATA);
    ObjRelocar(c->o, (ObjAlvoReloc){c->pdata, base}, (ObjCorrecao){OR_ADDR32NB, c->func_sim, 0});
    ObjRelocar(c->o, (ObjAlvoReloc){c->pdata, base + 4}, (ObjCorrecao){OR_ADDR32NB, c->func_sim, (int64_t)(fim - c->inicio)});
    ObjRelocar(c->o, (ObjAlvoReloc){c->pdata, base + 8}, (ObjCorrecao){OR_ADDR32NB, c->sim_xdata, offset_xdata});
}

void X64UnwindRegistrar(X64Ctx *c)
{
    ObjSecao *xdata = VecEm(&c->o->secoes, c->xdata);
    uint32_t fim = X64Offset(c);
    UnwindInfo u = MontarUnwind(c);
    uint32_t offset_xdata = (uint32_t)xdata->bytes.tam;

    BufAnexarN(&xdata->bytes, (const char *)u.bytes, u.tam);
    RegistrarFuncao(c, fim, offset_xdata);
    if (c->lista != NULL)
        X64ListarQuadro(c, fim, u.bytes, u.tam);
}
