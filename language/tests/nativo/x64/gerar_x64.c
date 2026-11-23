/* gerar_x64.c - modo "x64" do teste_nativo: front-end, BIR, X64Gerar, Ligar (com o runtime de RtNativoWin) e PeEscrever, sem gcc nem ld.
 * Versao: 0.3.2
 * Changelog: 0.3.2 - S2 (D0703): GerarExe deixa de ser static (teste_vetor.c gera o exe de uma BIR sintetica).
 *            0.3.1 - M6 P69: GerarObjeto passa a colheita com rastreio em dev (o runtime dev referencia a tabela FRT1).
 *            0.3.0- M5b P39: ModoX64Linux liga com LigarPedido.entrada (rt_linux_entrada_cru.o, LerEntradaLinux), o _start sai da variante do runtime.
 *            0.2.0 - M5 P27a: ModoX64Linux (teste_nativo.exe x64-linux <arq.fj> [--release]): liga o programa ao runtime Linux (RtNativoLinux + RtNativoLinuxComum no slot ryu) em IMAGEM_ELF e confere a Imagem
 *                    (sem .pdata/.xdata/.idata/.reloc, sem import nem DIR64, .rdata antes do .text, entrada = _start dentro do .text, base 0x400000); nada e gravado (o escritor ELF e o P27b).
 *            0.1.3 - M3 F5: liga tambem o Ryu (rt_win_ryu.o, RtNativoWinRyu) por LigarComTabelas.
 *            0.1.2 - M2 P10b: o runtime lido e o do modo do programa (RtNativoWin(modo)).
 *            0.1.1 - backend x64 P8: Alvo.gerar ganhou o parametro da listagem (aqui NULL).
 *            0.1.0 - backend x64 P7a: ModoX64 (teste_nativo.exe x64 <arq.fj> <saida.exe> [--release]).
 * Verificacao: python -I ..\tests\nativo\obj\verificar_obj.py (exige construir.bat antes)
 *
 * Saida: exit 0 = exe gravado | 1, 2, 3 = os de BirComPrograma | 4 = falha do gerador, do ligador ou da gravacao (causa em stderr).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/x64/alvo.h"
#include "nativo/dbg/dbg.h"
#include "nativo/fmt/pe.h"
#include "nativo/obj/coff.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/objeto.h"
#include "nativo/rt_embutido.h"
#include "../bir/gerar_bir.h"

#define GX_ALVO "x86_64-windows"
#define GX_ALVO_LINUX "x86_64-linux"
#define GX_FALHA 4

/* Gera o Objeto do programa; em dev (M6 P69) com a tabela FRT1 do rastreio, que o runtime dev referencia. */
static bool GerarObjeto(const Alvo *alvo, const BirPrograma *programa, Objeto *codigo, Diag *erro)
{
    DbgPrograma dbg;
    AlvoColheita colheita = {.rastreio = true, .dbg = &dbg};
    bool ok;

    if (alvo == NULL)
        return false;
    DbgIniciar(&dbg);
    ok = alvo->gerar(programa, codigo, erro, programa->modo == BM_DEV ? &colheita : NULL);
    DbgLiberar(&dbg);
    return ok;
}

static bool GravarArquivo(const char *caminho, const Buf *exe)
{
    FILE *f = fopen(caminho, "wb");
    size_t escritos;

    if (f == NULL)
        return false;
    escritos = fwrite(exe->dados, 1, exe->tam, f);
    fclose(f);
    return escritos == exe->tam;
}

static bool LerRuntime(Objeto *runtime, Objeto *ryu, uint8_t modo, Diag *erro)
{
    size_t n = 0;
    const uint8_t *bytes = RtNativoWin(modo, &n);

    if (!CoffLer(bytes, n, runtime, erro))
        return false;
    bytes = RtNativoWinRyu(&n);
    return CoffLer(bytes, n, ryu, erro);
}

/* BIR -> Objeto -> Ligar -> PE -> arquivo (saida = o caminho). Exportada: teste_vetor.c (S2) gera o exe de uma BIR sintetica por aqui. */
int GerarExe(const BirPrograma *programa, void *saida)
{
    const Alvo *alvo = AlvoPorNome(GX_ALVO);
    Objeto codigo, runtime, ryu;
    Imagem imagem;
    Buf exe = {0};
    Diag erro = {0};
    bool ok;

    ObjIniciar(&codigo);
    ObjIniciar(&runtime);
    ObjIniciar(&ryu);
    ImagemIniciar(&imagem);
    ok = GerarObjeto(alvo, programa, &codigo, &erro) && LerRuntime(&runtime, &ryu, programa->modo, &erro);
    ok = ok && LigarComTabelas(&(LigarPedido){.programa = &codigo, .runtime = &runtime, .ryu = &ryu}, &imagem, &erro) && PeEscrever(&imagem, &exe, &erro);
    if (ok && !GravarArquivo((const char *)saida, &exe)) {
        ok = false;
        erro = (Diag){0};
    }
    if (!ok)
        fprintf(stderr, "modo x64: falhou: %s\n", erro.tem ? erro.causa : "nao consegui gravar o executavel");
    ObjLiberar(&codigo);
    ObjLiberar(&runtime);
    ObjLiberar(&ryu);
    ImagemLiberar(&imagem);
    BufLiberar(&exe);
    return ok ? 0 : GX_FALHA;
}

int ModoX64(const char *arquivo, const char *saida, bool release)
{
    return BirComPrograma(arquivo, release ? BM_RELEASE : BM_DEV, GerarExe, (void *)saida);
}

/* ---------- modo "x64-linux" (M5 P27a): a Imagem NEUTRA do alvo x86_64-linux, sem escritor ---------- */

static bool LerRuntimeLinux(Objeto *runtime, Objeto *comum, uint8_t modo, Diag *erro)
{
    size_t n = 0;
    const uint8_t *bytes = RtNativoLinux(modo, &n);

    if (!CoffLer(bytes, n, runtime, erro))
        return false;
    bytes = RtNativoLinuxComum(&n);
    return CoffLer(bytes, n, comum, erro);
}

/* M5b P39 (D-R3): o _start do Linux e o objeto de entrada, fora da variante do runtime. */
static bool LerEntradaLinux(Objeto *entrada, Diag *erro)
{
    size_t n = 0;
    const uint8_t *bytes = RtNativoLinuxEntrada(&n);

    return CoffLer(bytes, n, entrada, erro);
}

static const PeSecao *AcharSecao(const Imagem *im, const char *nome)
{
    for (uint32_t k = 0; k < im->qtd_secoes; k++) {
        if (strncmp(im->secoes[k].nome, nome, sizeof im->secoes[k].nome) == 0)
            return &im->secoes[k];
    }
    return NULL;
}

static const ImagemSimbolo *AcharSimbolo(const Imagem *im, const char *nome)
{
    for (size_t k = 0; k < im->simbolos.n; k++) {
        const ImagemSimbolo *s = VecEm(&im->simbolos, k);
        if (strcmp(s->nome, nome) == 0)
            return s;
    }
    return NULL;
}

/* A entrada cai dentro do .text e e o _start; devolve o motivo da recusa ou NULL. */
static const char *ConferirEntrada(const Imagem *im)
{
    const PeSecao *texto = AcharSecao(im, ".text");
    const ImagemSimbolo *inicio = AcharSimbolo(im, "_start");

    if (texto == NULL || im->entrada_rva < texto->rva || im->entrada_rva >= texto->rva + texto->tam_virtual)
        return "entrada fora do .text";
    return inicio != NULL && inicio->rva == im->entrada_rva ? NULL : "a entrada nao e o simbolo _start";
}

/* Sem nada que e so do PE: unwind, imports, .reloc e DIR64; a leitura (.rdata) vem antes do codigo (R, RX, RW). Devolve o motivo da recusa ou NULL. */
static const char *ConferirSemPe(const Imagem *im)
{
    static const char *const SO_DO_PE[] = {".pdata", ".xdata", ".idata", ".reloc"};

    for (size_t k = 0; k < sizeof SO_DO_PE / sizeof SO_DO_PE[0]; k++) {
        if (AcharSecao(im, SO_DO_PE[k]) != NULL)
            return "secao so do PE na imagem do linux";
    }
    if (im->importacao.qtd != 0 || im->importacao.qtd_dlls != 0 || im->dir64.n != 0)
        return "import ou DIR64 na imagem do linux";
    return AcharSecao(im, ".rdata") == AcharSecao(im, ".text") || AcharSecao(im, ".rdata") > AcharSecao(im, ".text") ? "ordem das secoes" : NULL;
}

static const char *ConferirImagemLinux(const Imagem *im)
{
    const char *motivo;

    if (im->formato != IMAGEM_ELF || PeBaseDaImagem(im) != IMAGEM_BASE_ELF)
        return "formato ou base da imagem";
    motivo = ConferirSemPe(im);
    return motivo != NULL ? motivo : ConferirEntrada(im);
}

/* BIR -> Objeto -> Ligar (formato ELF) -> confere a Imagem e imprime o resumo; nada e gravado (o escritor ELF e o P27b). */
static int GerarImagemLinux(const BirPrograma *programa, void *ctx)
{
    const Alvo *alvo = AlvoPorNome(GX_ALVO_LINUX);
    Objeto codigo, runtime, comum, entrada;
    Imagem imagem;
    Diag erro = {0};
    const char *motivo = NULL;
    bool ok;

    (void)ctx;
    ObjIniciar(&codigo);
    ObjIniciar(&runtime);
    ObjIniciar(&comum);
    ObjIniciar(&entrada);
    ImagemIniciar(&imagem);
    ok = GerarObjeto(alvo, programa, &codigo, &erro) && LerRuntimeLinux(&runtime, &comum, programa->modo, &erro);
    ok = ok && LerEntradaLinux(&entrada, &erro);
    ok = ok && LigarComTabelas(&(LigarPedido){.programa = &codigo, .runtime = &runtime, .ryu = &comum, .entrada = &entrada, .formato = alvo->formato}, &imagem, &erro);
    if (ok)
        motivo = ConferirImagemLinux(&imagem);
    if (!ok || motivo != NULL)
        fprintf(stderr, "modo x64-linux: falhou: %s\n", ok ? motivo : erro.tem ? erro.causa : "causa desconhecida");
    else
        printf("x64-linux: %u secoes, entrada 0x%x, %zu simbolos, base 0x%llx\n", imagem.qtd_secoes, imagem.entrada_rva, imagem.simbolos.n, (unsigned long long)PeBaseDaImagem(&imagem));
    ObjLiberar(&codigo);
    ObjLiberar(&runtime);
    ObjLiberar(&comum);
    ObjLiberar(&entrada);
    ImagemLiberar(&imagem);
    return ok && motivo == NULL ? 0 : GX_FALHA;
}

int ModoX64Linux(const char *arquivo, bool release)
{
    return BirComPrograma(arquivo, release ? BM_RELEASE : BM_DEV, GerarImagemLinux, NULL);
}
