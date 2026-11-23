/* gerar_obj.c - modo "obj" do teste_nativo: monta o forja_programa com o codificador x64, liga com o runtime lido por CoffLer e grava os .exe.
 * Versao: 0.1.3
 * Changelog: 0.1.3 - M6e P93d: a FRT1 vazia passa a 12 palavras.
 *            0.1.2 - M6 P69: o programa leva uma tabela FRT1 vazia (o runtime dev passou a ter o rastreio).
 *            0.1.1- M2 P10b: o runtime lido e o dev (RtNativoWin recebe o modo).
 *            0.1.0 - backend x64 P5: caso1.exe (sub rsp,40; mov rcx,42; call forja_print_i64 -> "42") e caso2.exe (forja_falha_estouro
 *                    com 5 argumentos, o 5o em [rsp+32], strings no .rdata -> R0002 e exit 101).
 * Verificacao: python -I ..\tests\nativo\obj\verificar_obj.py (exige construir.bat antes)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/fmt/pe.h"
#include "nativo/obj/coff.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/objeto.h"
#include "nativo/rt_embutido.h"
#include "nativo/x64/x64.h"
#include "../enc/operandos_enc.h"

#define GO_TAM_INSTR 15u
#define GO_ALINHAMENTO_CODIGO 16u
#define GO_TAM_CAMINHO 512
#define GO_LINHA_ESTOURO 7
#define GO_PARAMETRO_PILHA_32 32
#define GO_QUADRO_CASO2 56
#define GO_QUADRO_CASO1 40
#define GO_TAM_CAMPO_REL 4u

/* Strings do caso 2 no .rdata: a operacao e o nome do arquivo, cada um com NUL. */
static const char GO_OPERACAO[] = "soma i64";
static const char GO_ARQUIVO[] = "teste_rt.fj";
static const char GO_SIMBOLO_S0[] = ".s0";
static const char GO_SIMBOLO_S1[] = ".s1";

/* Codifica a instrucao, anexa os bytes na secao e devolve o offset do fim (para o REL32 de quem tem simbolo). */
static uint32_t GoEmitir(Objeto *o, uint32_t secao, MInstr mi)
{
    uint8_t bytes[GO_TAM_INSTR];
    uint32_t tam = 0;
    Diag erro = {0};
    if (!X64CodificarInstr(&mi, bytes, &tam, &erro))
        FalhaInterna("gerar_obj: codificador recusou a instrucao: %s", erro.causa);
    ObjSecao *s = VecEm(&o->secoes, secao);
    BufAnexarN(&s->bytes, (const char *)bytes, tam);
    return (uint32_t)s->bytes.tam;
}

/* Instrucao com registrador e imediato: mov, sub, add. */
static MInstr GoRegImed(uint16_t forma, X64Reg reg, uint8_t largura, int64_t imed)
{
    MInstr mi = MiNova(forma, 2);
    mi.op[0] = OpReg(reg, largura);
    mi.op[1] = OpImed(imed);
    return mi;
}

static MInstr GoRegReg(uint16_t forma, X64Reg a, uint8_t largura_a, X64Reg b, uint8_t largura_b)
{
    MInstr mi = MiNova(forma, 2);
    mi.op[0] = OpReg(a, largura_a);
    mi.op[1] = OpReg(b, largura_b);
    return mi;
}

/* lea reg,[rip+d32]: o deslocamento sai zero e o REL32 para o simbolo entra depois (GoRelocarFim). */
static MInstr GoLeaRip(X64Reg reg)
{
    MInstr mi = MiNova(FX_LEA64_RM, 2);
    mi.op[0] = OpReg(reg, 64);
    mi.op[1] = OpMem(X64_RIP, 0, 64);
    return mi;
}

/* mov dword [base+desloc], reg: a forma MOV32_MR leva o registrador de origem em op[1]. */
static MInstr GoMovMemReg(X64Reg base, int32_t desloc, X64Reg origem)
{
    MInstr mi = MiNova(FX_MOV32_MR, 2);
    mi.op[0] = OpMem(base, desloc, 32);
    mi.op[1] = OpReg(origem, 32);
    return mi;
}

static MInstr GoCallSimbolo(uint32_t simbolo)
{
    MInstr mi = MiNova(FX_CALL_REL32, 1);
    mi.op[0] = OpRel(MO_SIMBOLO, 0);
    mi.op[0].simbolo = simbolo;
    return mi;
}

/* Registra o REL32 de uma instrucao que termina em fim (o campo rel32 e os 4 ultimos bytes dela). */
static void GoRelocarFim(Objeto *o, uint32_t secao, uint32_t fim, uint32_t simbolo)
{
    ObjRelocar(o, (ObjAlvoReloc){secao, fim - GO_TAM_CAMPO_REL}, (ObjCorrecao){OR_REL32, simbolo, 0});
}

/* Caso 1: sub rsp,40; mov rcx,42; call forja_print_i64; xor eax,eax; add rsp,40; ret. */
static void GoMontarCaso1(Objeto *o)
{
    uint32_t texto = ObjSecaoAbrir(o, ".text", OS_TEXTO, GO_ALINHAMENTO_CODIGO);
    uint32_t print = ObjSimboloReferir(o, "forja_print_i64");
    ObjSimboloDefinir(o, "forja_programa", (int32_t)texto, 0);
    GoEmitir(o, texto, GoRegImed(FX_SUB64_RI, X64_RSP, 64, GO_QUADRO_CASO1));
    GoEmitir(o, texto, GoRegImed(FX_MOV64_RI, X64_RCX, 64, 42));
    GoRelocarFim(o, texto, GoEmitir(o, texto, GoCallSimbolo(print)), print);
    GoEmitir(o, texto, GoRegReg(FX_XOR32_RR, X64_RAX, 32, X64_RAX, 32));
    GoEmitir(o, texto, GoRegImed(FX_ADD64_RI, X64_RSP, 64, GO_QUADRO_CASO1));
    GoEmitir(o, texto, MiNova(FX_RET, 0));
}

/* Caso 2: forja_falha_estouro("soma i64", INT64_MAX, 1, "teste_rt.fj", 7), com a linha no 5o argumento ([rsp+32]). */
static void GoMontarCaso2(Objeto *o)
{
    uint32_t texto = ObjSecaoAbrir(o, ".text", OS_TEXTO, GO_ALINHAMENTO_CODIGO);
    uint32_t rdata = ObjSecaoAbrir(o, ".rdata", OS_RDATA, 1);
    uint32_t falha = ObjSimboloReferir(o, "forja_falha_estouro");
    uint32_t s0, s1;
    ObjSecao *dados = VecEm(&o->secoes, rdata);
    BufAnexarN(&dados->bytes, GO_OPERACAO, sizeof GO_OPERACAO);
    BufAnexarN(&dados->bytes, GO_ARQUIVO, sizeof GO_ARQUIVO);
    s0 = ObjSimboloDefinir(o, GO_SIMBOLO_S0, (int32_t)rdata, 0);
    s1 = ObjSimboloDefinir(o, GO_SIMBOLO_S1, (int32_t)rdata, sizeof GO_OPERACAO);
    ObjSimboloDefinir(o, "forja_programa", (int32_t)texto, 0);
    GoEmitir(o, texto, GoRegImed(FX_SUB64_RI, X64_RSP, 64, GO_QUADRO_CASO2));
    GoRelocarFim(o, texto, GoEmitir(o, texto, GoLeaRip(X64_RCX)), s0);
    GoEmitir(o, texto, GoRegImed(FX_MOV64_RI, X64_RDX, 64, INT64_MAX));
    GoEmitir(o, texto, GoRegImed(FX_MOV64_RI, X64_R8, 64, 1));
    GoRelocarFim(o, texto, GoEmitir(o, texto, GoLeaRip(X64_R9)), s1);
    GoEmitir(o, texto, GoRegImed(FX_MOV32_RI, X64_R10, 32, GO_LINHA_ESTOURO));
    GoEmitir(o, texto, GoMovMemReg(X64_RSP, GO_PARAMETRO_PILHA_32, X64_R10));
    GoRelocarFim(o, texto, GoEmitir(o, texto, GoCallSimbolo(falha)), falha);
    GoEmitir(o, texto, MiNova(FX_UD2, 0));
}

/* O runtime dev (M6 P69) referencia a tabela FRT1 do rastreio: o programa montado a mao leva uma vazia (cabecalho de 12 palavras, 0 funcoes, 0 chamadas, sem guarda) e o inicio do .text (secao 0). */
static void GoDefinirRastroVazio(Objeto *o)
{
    static const uint32_t tabela[12] = {0x31545246u, 0, 0, 0, 48, 48, 0, 0, 0, 0, 0, 0};
    uint32_t rdata = ObjSecaoAbrir(o, ".rdata", OS_RDATA, 8);
    ObjSecao *dados = VecEm(&o->secoes, rdata);

    BufAnexarN(&dados->bytes, (const char *)tabela, sizeof tabela);
    ObjSimboloDefinir(o, "forja_rastro_tabela", (int32_t)rdata, 0);
    ObjSimboloDefinir(o, "forja_rastro_texto", 0, 0);
}

static bool GoLerRuntime(Objeto *rt, Diag *erro)
{
    size_t n = 0;
    const uint8_t *b = RtNativoWin(BM_DEV, &n);
    ObjIniciar(rt);
    return CoffLer(b, n, rt, erro);
}

static bool GoGravarArquivo(const char *caminho, const Buf *exe)
{
    FILE *f = fopen(caminho, "wb");
    if (f == NULL)
        return false;
    size_t escritos = fwrite(exe->dados, 1, exe->tam, f);
    fclose(f);
    return escritos == exe->tam;
}

/* Monta o programa do caso, liga com o runtime e grava o exe em pasta/nome. Devolve 0 se deu certo. */
static int GoGravarCaso(const char *pasta, const char *nome, bool caso2)
{
    Objeto programa, runtime;
    Imagem im;
    Buf exe = {0};
    Diag erro = {0};
    char caminho[GO_TAM_CAMINHO];
    bool ok;
    ObjIniciar(&programa);
    ImagemIniciar(&im);
    if (caso2)
        GoMontarCaso2(&programa);
    else
        GoMontarCaso1(&programa);
    GoDefinirRastroVazio(&programa);
    snprintf(caminho, sizeof caminho, "%s/%s", pasta, nome);
    ok = GoLerRuntime(&runtime, &erro) && Ligar(&programa, &runtime, &im, &erro) && PeEscrever(&im, &exe, &erro);
    ok = ok && GoGravarArquivo(caminho, &exe);
    if (!ok)
        fprintf(stderr, "modo obj: %s nao saiu: %s\n", nome, erro.tem ? erro.causa : "falha ao gravar");
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
    ImagemLiberar(&im);
    BufLiberar(&exe);
    return ok ? 0 : 1;
}

int ModoObj(const char *pasta)
{
    int falhas = GoGravarCaso(pasta, "caso1.exe", false);
    falhas += GoGravarCaso(pasta, "caso2.exe", true);
    return falhas == 0 ? 0 : 1;
}
