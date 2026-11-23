/* x64_abi_sysv.h - ABI System V AMD64 (Linux): classe de cada eightbyte (INTEGER, SSE, MEMORY) para argumentos e retorno.
 * Versao: 0.3.1
 * Changelog: 0.3.1 - G3 G0b: quebra de ciclo bir->x64 (movido de nativo/x64/ para nativo/bir/).
 *            0.3.0 - M5b P38b: layout do bloco de entrada do callback (ABI_SYSV_BLOCO_*, AbiSysvBlocoArg, AbiSysvBlocoRet).
 *            0.2.0 - M5b P38a: AbiSysvPlano e AbiSysvPlanejar (parte pura do plano de chamada, antes em x64_chamada_sysv.c); ABI_SYSV_MAX_ARGS.
 *            0.1.0 - M5 P34: AbiSysvClassificar, AbiSysvRetorno, AbiSysvAlVariadica e AbiSysvNomeReg. Tipos locais (nao importam BIR nem X64Reg): a entrada e o campo folha de cada agregado.
 * Verificacao: gcc -std=c11 -Wall -Wextra -I compiler tests\unidade\teste_abi_sysv.c compiler\nativo\x64\x64_abi_sysv.c (ver o cabecalho de teste_abi_sysv.c).
 */
#ifndef X64_ABI_SYSV_H
#define X64_ABI_SYSV_H

#include <stdbool.h>
#include <stdint.h>

#define ABI_SYSV_REGS_INT 6u     /* RDI, RSI, RDX, RCX, R8, R9 */
#define ABI_SYSV_REGS_SSE 8u     /* XMM0 ate XMM7 */
#define ABI_SYSV_TAM_REG 16u     /* agregado acima disso e MEMORY */
#define ABI_SYSV_BYTES_EIGHTBYTE 8u
#define ABI_SYSV_MAX_ARGS 16u    /* teto de argumentos de uma chamada (o plano guarda um slot por argumento) */

/* Classe SysV de um campo folha ou de um eightbyte. */
typedef enum { ABI_SYSV_INTEIRO, ABI_SYSV_SSE, ABI_SYSV_MEMORIA } AbiSysvClasse;

/* Registrador de um eightbyte. ABI_SYSV_NENHUM = sem registrador (o valor esta na memoria). */
typedef enum {
    ABI_SYSV_RAX, ABI_SYSV_RDX, ABI_SYSV_RDI, ABI_SYSV_RSI, ABI_SYSV_RCX, ABI_SYSV_R8, ABI_SYSV_R9,
    ABI_SYSV_XMM0, ABI_SYSV_XMM1, ABI_SYSV_XMM2, ABI_SYSV_XMM3, ABI_SYSV_XMM4, ABI_SYSV_XMM5, ABI_SYSV_XMM6, ABI_SYSV_XMM7,
    ABI_SYSV_NENHUM
} AbiSysvReg;

/* Campo folha de um agregado (ou o proprio escalar, em deslocamento 0). alinhamento 0 conta como desalinhado. */
typedef struct {
    AbiSysvClasse classe;   /* INTEIRO para int, long, ponteiro e char; SSE para float e double */
    uint32_t deslocamento;  /* em bytes, a partir do inicio do agregado */
    uint32_t alinhamento;   /* exigido pelo tipo do campo; desalinhado = MEMORY */
} AbiSysvCampo;

/* Argumento ou retorno. tam = tamanho total em bytes, com o padding final (como o sizeof do C). */
typedef struct {
    uint32_t tam;
    const AbiSysvCampo *campos;
    uint32_t n_campos;
} AbiSysvArg;

/* Onde fica um argumento ou um retorno: ate 2 eightbytes, cada um com classe e registrador.
 * na_memoria: argumento vai para a pilha; retorno MEMORY, o chamador passa o ponteiro oculto em RDI e recebe-o em RAX (reg[0]). */
typedef struct {
    bool na_memoria;
    uint32_t n_eightbytes;
    AbiSysvClasse classe[2];
    AbiSysvReg reg[2];
} AbiSysvLocal;

/* Classifica os argumentos na ordem da chamada. oculto = 1 quando o retorno e MEMORY (RDI ja vai para o ponteiro).
 * Retorna false se algum argumento e invalido (tam 0 ou sem campos). saida tem n posicoes. */
bool AbiSysvClassificar(const AbiSysvArg *args, uint32_t n, uint32_t oculto, AbiSysvLocal *saida);

/* Retorno: tam 0 e sem campos = void (saida sem registrador); MEMORY = na_memoria; senao RAX/RDX (INTEGER) e XMM0/XMM1 (SSE) por eightbyte.
 * Retorna false se o retorno e invalido. */
bool AbiSysvRetorno(const AbiSysvArg *ret, AbiSysvLocal *saida);

/* AL de uma chamada variadica: quantos XMM aparecem nos n argumentos ja classificados (argumentos nomeados e variadicos). */
uint32_t AbiSysvAlVariadica(const AbiSysvLocal *saida, uint32_t n);

/* Plano completo de uma chamada SysV: onde mora cada argumento e o retorno, sem emitir nada. */
typedef struct {
    AbiSysvLocal arg[ABI_SYSV_MAX_ARGS];  /* classe e registrador de cada eightbyte, ou na_memoria (vai para a pilha) */
    uint32_t off[ABI_SYSV_MAX_ARGS];      /* posicao na pilha, em bytes a partir de [rsp], so dos argumentos na_memoria */
    AbiSysvLocal ret;                     /* destino do retorno: RAX/RDX ou XMM0/XMM1; na_memoria = MEMORY */
    bool oculto;                          /* retorno MEMORY: o chamador passa o ponteiro oculto em RDI */
    uint32_t bytes_pilha;                 /* soma do tamanho de cada argumento na pilha, arredondado a 8 */
    uint32_t al;                          /* quantos XMM os argumentos usam (AL de uma chamada variadica) */
} AbiSysvPlano;

/* Planeja a chamada: classifica argumentos e retorno e calcula a pilha e o AL. n <= ABI_SYSV_MAX_ARGS.
 * Retorna false se algum argumento ou o retorno e invalido (ver AbiSysvClassificar e AbiSysvRetorno). ret tam 0 e sem campos = void. */
bool AbiSysvPlanejar(const AbiSysvArg *args, uint32_t n, const AbiSysvArg *ret, AbiSysvPlano *saida);

/* Bloco de entrada de um callback SysV (P38b): o stub fjt_ guarda os registradores aqui e o wrapper fjtw_ le os argumentos e grava o retorno.
 * Layout (bytes): 0..47 rdi, rsi, rdx, rcx, r8, r9 | 48..111 xmm0..xmm7 (8 B cada) | 112 rax, 120 rdx, 128 xmm0, 136 xmm1 (retorno). */
#define ABI_SYSV_BLOCO_XMM 48u
#define ABI_SYSV_BLOCO_RET_INT 112u
#define ABI_SYSV_BLOCO_RET_SSE 128u
#define ABI_SYSV_BLOCO_BYTES 144u
/* A entrada Win64 do stub fjt_ (jmp fjtm_<proc>, usada quando o runtime, que fala Win64, chama o proc) fica este numero de bytes ANTES do fjt_. */
#define ABI_SYSV_ENTRADA_WIN64_ANTES 16u

/* Onde o registrador de ARGUMENTO (rdi..r9, xmm0..7) fica no bloco; ABI_SYSV_NENHUM e rax/rdx de retorno valem 0 (nao sao argumento). */
uint32_t AbiSysvBlocoArg(AbiSysvReg reg);
/* Onde o registrador de RETORNO (rax, rdx, xmm0, xmm1) fica no bloco. */
uint32_t AbiSysvBlocoRet(AbiSysvReg reg);

/* Nome do registrador em minusculas ("rdi", "xmm0"); "-" para ABI_SYSV_NENHUM. */
const char *AbiSysvNomeReg(AbiSysvReg reg);

#endif
