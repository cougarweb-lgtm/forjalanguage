/* x64_abi_sysv.c - ABI System V AMD64 (Linux): classe de cada eightbyte de argumentos e retorno, mesma forma de x64_abi_win.c.
 * Versao: 0.3.1
 * Changelog: 0.3.1 - G3 G0b: quebra de ciclo bir->x64 (movido de nativo/x64/ para nativo/bir/).
 *            0.3.0 - M5b P38b: AbiSysvBlocoArg e AbiSysvBlocoRet (layout do bloco de entrada do callback).
 *            0.2.0 - M5b P38a: AbiSysvPlanejar (plano da chamada: pilha, AL e destino do retorno), movido de x64_chamada_sysv.c; sem BIR.
 *            0.1.0 - M5 P34: AbiSysvClassificar (RDI, RSI, RDX, RCX, R8, R9 e XMM0..7; agregado inteiro vai para a pilha se faltar registrador),
 *                    AbiSysvRetorno (RAX/RDX, XMM0/XMM1 ou ponteiro oculto), AbiSysvAlVariadica e AbiSysvNomeReg.
 * Verificacao: tests\unidade\teste_abi_sysv.c contra as 60 assinaturas do gcc (tests\nativo\linux\abi_sysv\abi_sysv.json).
 */
#include "nativo/bir/x64_abi_sysv.h"

static const AbiSysvReg REGS_INT_ARG[ABI_SYSV_REGS_INT] = {ABI_SYSV_RDI, ABI_SYSV_RSI, ABI_SYSV_RDX, ABI_SYSV_RCX, ABI_SYSV_R8, ABI_SYSV_R9};
static const AbiSysvReg REGS_SSE_ARG[ABI_SYSV_REGS_SSE] = {ABI_SYSV_XMM0, ABI_SYSV_XMM1, ABI_SYSV_XMM2, ABI_SYSV_XMM3,
                                                          ABI_SYSV_XMM4, ABI_SYSV_XMM5, ABI_SYSV_XMM6, ABI_SYSV_XMM7};
static const AbiSysvReg REGS_INT_RET[2] = {ABI_SYSV_RAX, ABI_SYSV_RDX};
static const AbiSysvReg REGS_SSE_RET[2] = {ABI_SYSV_XMM0, ABI_SYSV_XMM1};
static const char *NOMES_REG[ABI_SYSV_NENHUM + 1] = {"rax", "rdx", "rdi", "rsi", "rcx", "r8", "r9",
                                                    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "-"};

/* Registradores ja tomados na ordem da chamada (INTEGER e SSE tem sequencias separadas). */
typedef struct {
    uint32_t ints;
    uint32_t sses;
} AbiSysvUso;

static AbiSysvLocal LocalVazio(void)
{
    return (AbiSysvLocal){.na_memoria = false, .n_eightbytes = 0,
                          .classe = {ABI_SYSV_MEMORIA, ABI_SYSV_MEMORIA}, .reg = {ABI_SYSV_NENHUM, ABI_SYSV_NENHUM}};
}

/* Campo com alinhamento 0 nao existe no C; tratar como desalinhado acusa a entrada errada em vez de passar em silencio. */
static bool CampoAlinhado(const AbiSysvCampo *campo)
{
    return campo->alinhamento != 0 && campo->deslocamento % campo->alinhamento == 0;
}

/* MEMORY: agregado maior que 16 bytes ou campo fora do alinhamento natural. */
static bool EhMemoria(const AbiSysvArg *arg)
{
    for (uint32_t i = 0; i < arg->n_campos; i++) {
        if (!CampoAlinhado(&arg->campos[i]))
            return true;
    }
    return arg->tam > ABI_SYSV_TAM_REG;
}

static bool ArgValido(const AbiSysvArg *arg)
{
    return arg->tam > 0 && arg->n_campos > 0;
}

static uint32_t NumeroDeEightbytes(uint32_t tam)
{
    return (tam + ABI_SYSV_BYTES_EIGHTBYTE - 1u) / ABI_SYSV_BYTES_EIGHTBYTE;
}

/* Classe do eightbyte e: INTEGER se algum campo inteiro comeca nele (INTEGER vence SSE); senao SSE. */
static AbiSysvClasse ClasseDoEightbyte(const AbiSysvArg *arg, uint32_t eightbyte)
{
    for (uint32_t i = 0; i < arg->n_campos; i++) {
        const AbiSysvCampo *campo = &arg->campos[i];
        if (campo->deslocamento / ABI_SYSV_BYTES_EIGHTBYTE == eightbyte && campo->classe == ABI_SYSV_INTEIRO)
            return ABI_SYSV_INTEIRO;
    }
    return ABI_SYSV_SSE;
}

/* Preenche n_eightbytes e classe[] do argumento que cabe em registrador (sem atribuir nenhum). */
static void ClassificarEightbytes(const AbiSysvArg *arg, AbiSysvLocal *saida)
{
    saida->n_eightbytes = NumeroDeEightbytes(arg->tam);
    for (uint32_t e = 0; e < saida->n_eightbytes; e++)
        saida->classe[e] = ClasseDoEightbyte(arg, e);
}

/* Conta quantos registradores de cada classe o argumento pede (sem gastar nenhum ainda). */
static void ContarPedidos(const AbiSysvLocal *saida, uint32_t *pedido_int, uint32_t *pedido_sse)
{
    *pedido_int = 0;
    *pedido_sse = 0;
    for (uint32_t e = 0; e < saida->n_eightbytes; e++) {
        if (saida->classe[e] == ABI_SYSV_INTEIRO)
            (*pedido_int)++;
        else
            (*pedido_sse)++;
    }
}

/* Argumento inteiro ou nada: se faltar registrador de alguma classe, ele vai inteiro para a pilha e nao gasta nenhum. */
static void AtribuirArgumento(const AbiSysvArg *arg, AbiSysvUso *uso, AbiSysvLocal *saida)
{
    uint32_t pedido_int = 0, pedido_sse = 0;

    if (EhMemoria(arg)) {
        saida->na_memoria = true;
        return;
    }
    ClassificarEightbytes(arg, saida);
    ContarPedidos(saida, &pedido_int, &pedido_sse);
    if (uso->ints + pedido_int > ABI_SYSV_REGS_INT || uso->sses + pedido_sse > ABI_SYSV_REGS_SSE) {
        saida->na_memoria = true;
        saida->n_eightbytes = 0;
        return;
    }
    for (uint32_t e = 0; e < saida->n_eightbytes; e++) {
        if (saida->classe[e] == ABI_SYSV_INTEIRO)
            saida->reg[e] = REGS_INT_ARG[uso->ints++];
        else
            saida->reg[e] = REGS_SSE_ARG[uso->sses++];
    }
}

bool AbiSysvClassificar(const AbiSysvArg *args, uint32_t n, uint32_t oculto, AbiSysvLocal *saida)
{
    AbiSysvUso uso = {.ints = oculto ? 1u : 0u, .sses = 0};

    for (uint32_t k = 0; k < n; k++) {
        if (!ArgValido(&args[k]))
            return false;
        saida[k] = LocalVazio();
        AtribuirArgumento(&args[k], &uso, &saida[k]);
    }
    return true;
}

/* Retorno de 1 ou 2 eightbytes: INTEGER pega RAX e depois RDX; SSE pega XMM0 e depois XMM1. */
static void AtribuirRetornoRegistrado(const AbiSysvArg *ret, AbiSysvLocal *saida)
{
    AbiSysvUso uso = {.ints = 0, .sses = 0};

    ClassificarEightbytes(ret, saida);
    for (uint32_t e = 0; e < saida->n_eightbytes; e++) {
        if (saida->classe[e] == ABI_SYSV_INTEIRO)
            saida->reg[e] = REGS_INT_RET[uso.ints++];
        else
            saida->reg[e] = REGS_SSE_RET[uso.sses++];
    }
}

bool AbiSysvRetorno(const AbiSysvArg *ret, AbiSysvLocal *saida)
{
    *saida = LocalVazio();
    if (ret->tam == 0 && ret->n_campos == 0)
        return true;
    if (!ArgValido(ret))
        return false;
    if (EhMemoria(ret)) {
        saida->na_memoria = true;
        saida->reg[0] = ABI_SYSV_RAX;
        return true;
    }
    AtribuirRetornoRegistrado(ret, saida);
    return true;
}

uint32_t AbiSysvAlVariadica(const AbiSysvLocal *saida, uint32_t n)
{
    uint32_t al = 0;

    for (uint32_t k = 0; k < n; k++) {
        for (uint32_t e = 0; e < saida[k].n_eightbytes; e++) {
            if (saida[k].reg[e] >= ABI_SYSV_XMM0 && saida[k].reg[e] <= ABI_SYSV_XMM7)
                al++;
        }
    }
    return al;
}

/* Bytes que um argumento ocupa na pilha: tamanho arredondado para cima a 8 (cada um entra alinhado a 8). */
static uint32_t BytesNaPilha(uint32_t tam)
{
    return (tam + ABI_SYSV_BYTES_EIGHTBYTE - 1u) / ABI_SYSV_BYTES_EIGHTBYTE * ABI_SYSV_BYTES_EIGHTBYTE;
}

/* Posicao na pilha dos argumentos que foram para a memoria, em ordem; devolve o total de bytes da pilha. */
static uint32_t PosicionarNaPilha(const AbiSysvArg *args, uint32_t n, AbiSysvPlano *saida)
{
    uint32_t pilha = 0;

    for (uint32_t k = 0; k < n; k++) {
        if (!saida->arg[k].na_memoria)
            continue;
        saida->off[k] = pilha;
        pilha += BytesNaPilha(args[k].tam);
    }
    return pilha;
}

/* Planeja a chamada: o retorno vem primeiro (MEMORY decide o ponteiro oculto em RDI), depois os argumentos, a pilha e o AL. */
bool AbiSysvPlanejar(const AbiSysvArg *args, uint32_t n, const AbiSysvArg *ret, AbiSysvPlano *saida)
{
    if (n > ABI_SYSV_MAX_ARGS || !AbiSysvRetorno(ret, &saida->ret))
        return false;
    saida->oculto = saida->ret.na_memoria;
    if (!AbiSysvClassificar(args, n, saida->oculto ? 1u : 0u, saida->arg))
        return false;
    saida->bytes_pilha = PosicionarNaPilha(args, n, saida);
    saida->al = AbiSysvAlVariadica(saida->arg, n);
    return true;
}

/* Posicao de cada registrador de argumento no bloco: os 6 inteiros na ordem da ABI e os xmm em ABI_SYSV_BLOCO_XMM + 8*n. */
uint32_t AbiSysvBlocoArg(AbiSysvReg reg)
{
    for (uint32_t k = 0; k < ABI_SYSV_REGS_INT; k++) {
        if (REGS_INT_ARG[k] == reg)
            return k * ABI_SYSV_BYTES_EIGHTBYTE;
    }
    for (uint32_t k = 0; k < ABI_SYSV_REGS_SSE; k++) {
        if (REGS_SSE_ARG[k] == reg)
            return ABI_SYSV_BLOCO_XMM + k * ABI_SYSV_BYTES_EIGHTBYTE;
    }
    return 0;
}

/* Retorno: rax e rdx em ABI_SYSV_BLOCO_RET_INT (+8), xmm0 e xmm1 em ABI_SYSV_BLOCO_RET_SSE (+8). */
uint32_t AbiSysvBlocoRet(AbiSysvReg reg)
{
    for (uint32_t k = 0; k < 2; k++) {
        if (REGS_INT_RET[k] == reg)
            return ABI_SYSV_BLOCO_RET_INT + k * ABI_SYSV_BYTES_EIGHTBYTE;
        if (REGS_SSE_RET[k] == reg)
            return ABI_SYSV_BLOCO_RET_SSE + k * ABI_SYSV_BYTES_EIGHTBYTE;
    }
    return 0;
}

const char *AbiSysvNomeReg(AbiSysvReg reg)
{
    return reg <= ABI_SYSV_NENHUM ? NOMES_REG[reg] : "?";
}
