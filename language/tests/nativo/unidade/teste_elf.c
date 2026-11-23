/* teste_elf.c - verificacoes de unidade do escritor ELF64 (ElfEscrever e ElfPlanejar, chamadas pelo teste_nativo): cabecalhos, segmentos, determinismo e as recusas com X0905.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M5b P41 (--pie): TestaPie (ET_DYN, 7 cabecalhos de programa, PT_DYNAMIC com DT_RELA/DT_RELASZ e as RELATIVE gravadas).
 *            0.3.1 - M5b P39-B1 (D0654): TestaCurtoNoElf usa RtNativoLinuxCurto (rt_linux_curto.o), o curto que LerCurto escolhe no ELF.
 *            0.3.0 - M5b P39: TestaCurtoNoElf liga com LigarPedido.entrada (rt_linux_entrada_cru.o, o _start do ELF agora fica fora do runtime).
 *            0.2.0 - IA1-LX: TestaCurtoNoElf (+4 verificacoes: o rt_win_curto.o liga no ELF com o runtime Linux sem simbolo indefinido, sem .pdata/.xdata/.idata/.reloc, com os simbolos do curto vivos e duas ligacoes iguais).
 *            0.1.0 - M5 P27b: 20 verificacoes (4 secoes R/RX/RW; so .bss no RW; simbolo no .symtab; ordem trocada, formato PE e entrada fora do RX = X0905 com o campo).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdio.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/fmt/elf.h"
#include "nativo/obj/coff.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/objeto.h"
#include "nativo/rt_embutido.h"

#define ELF_TESTE_TEXTO_TAM 0x40u
#define ELF_TESTE_DADOS_TAM 0x30u
#define ELF_TESTE_BSS_TAM 0x2000u
#define ELF_TESTE_OFF_MACHINE 18u
#define ELF_TESTE_OFF_ENTRY 24u
#define ELF_TESTE_OFF_PHNUM 56u
#define ELF_TESTE_OFF_SHNUM 60u
#define ELF_TESTE_TAM_PHDR 56u
#define ELF_TESTE_OFF_PH_OFFSET 8u
#define ELF_TESTE_OFF_PH_FILESZ 32u
#define ELF_TESTE_OFF_PH_MEMSZ 40u
#define ELF_TESTE_OFF_PH_FLAGS 4u
#define ELF_TESTE_PH0 64u
#define ELF_TESTE_TAM_SIMBOLO 24u      /* Elf64_Sym */
#define ELF_TESTE_TAM_NOME_START 7u    /* "_start" com o NUL na .strtab */

typedef struct {
    int *total;
    int *falhas;
} ElfContagem;

static void ElfConfere(const ElfContagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (elf) linha %d: %s\n", linha, texto);
}

#define ELF_VERIFICA(c, cond) ElfConfere((c), (cond), #cond, __LINE__)

static uint64_t LeLe(const Buf *b, uint32_t off, uint32_t n)
{
    uint64_t v = 0;

    for (uint32_t k = n; k > 0; k--)
        v = (v << 8) | (uint8_t)b->dados[off + k - 1];
    return v;
}

/* .rdata, .text, [.data,] .bss; ordem_certa = false poe o .text antes do .rdata. */
static void MontaImagem(Imagem *im, bool com_data, bool ordem_certa)
{
    Buf leitura = {0}, texto = {0}, dados = {0};
    Diag erro = {0};
    PeSecaoPedido rdata = {".rdata", PE_SECAO_LEITURA, &leitura, 0}, text = {".text", PE_SECAO_TEXTO, &texto, 0};

    ImagemIniciar(im);
    im->formato = IMAGEM_ELF;
    BufAnexarN(&leitura, "Forja Native", 12);
    for (uint32_t k = 0; k < ELF_TESTE_TEXTO_TAM; k++)
        BufAnexarN(&texto, "\x90", 1);
    for (uint32_t k = 0; k < ELF_TESTE_DADOS_TAM; k++)
        BufAnexarN(&dados, "\x07", 1);
    if (ordem_certa)
        ImagemSecao(im, &rdata, &erro);
    im->entrada_rva = ImagemSecao(im, &text, &erro);
    if (!ordem_certa)
        ImagemSecao(im, &rdata, &erro);
    if (com_data)
        ImagemSecao(im, &(PeSecaoPedido){".data", PE_SECAO_DADOS, &dados, 0}, &erro);
    ImagemSecao(im, &(PeSecaoPedido){".bss", PE_SECAO_BSS, NULL, ELF_TESTE_BSS_TAM}, &erro);
    BufLiberar(&leitura);
    BufLiberar(&texto);
    BufLiberar(&dados);
}

static void TestaGravacao(const ElfContagem *c)
{
    Imagem im;
    Buf a = {0}, b = {0};
    Diag erro = {0};

    MontaImagem(&im, true, true);
    ELF_VERIFICA(c, ElfEscrever(&im, &a, &erro));
    ELF_VERIFICA(c, ElfEscrever(&im, &b, &erro) && a.tam == b.tam && memcmp(a.dados, b.dados, a.tam) == 0);
    ELF_VERIFICA(c, memcmp(a.dados, "\x7f" "ELF\x02\x01\x01", 7) == 0 && LeLe(&a, ELF_TESTE_OFF_MACHINE, 2) == 62);
    ELF_VERIFICA(c, LeLe(&a, ELF_TESTE_OFF_ENTRY, 8) == IMAGEM_BASE_ELF + im.entrada_rva);
    ELF_VERIFICA(c, LeLe(&a, ELF_TESTE_OFF_PHNUM, 2) == 4 && LeLe(&a, ELF_TESTE_OFF_SHNUM, 2) == 8);
    ELF_VERIFICA(c, !erro.tem);
    BufLiberar(&a);
    BufLiberar(&b);
    ImagemLiberar(&im);
}

static void TestaSegmentos(const ElfContagem *c)
{
    Imagem im;
    Buf a = {0};
    Diag erro = {0};
    uint32_t rx = ELF_TESTE_PH0 + ELF_TESTE_TAM_PHDR, rw = rx + ELF_TESTE_TAM_PHDR;

    MontaImagem(&im, true, true);
    ELF_VERIFICA(c, ElfEscrever(&im, &a, &erro));
    ELF_VERIFICA(c, LeLe(&a, ELF_TESTE_PH0 + ELF_TESTE_OFF_PH_OFFSET, 8) == 0 && LeLe(&a, ELF_TESTE_PH0 + ELF_TESTE_OFF_PH_FLAGS, 4) == ELF_PF_R);
    ELF_VERIFICA(c, LeLe(&a, rx + ELF_TESTE_OFF_PH_FLAGS, 4) == (ELF_PF_R | ELF_PF_X) && LeLe(&a, rx + ELF_TESTE_OFF_PH_FILESZ, 8) == ELF_TESTE_TEXTO_TAM);
    ELF_VERIFICA(c, LeLe(&a, rw + ELF_TESTE_OFF_PH_FLAGS, 4) == (ELF_PF_R | ELF_PF_W) && LeLe(&a, rw + ELF_TESTE_OFF_PH_FILESZ, 8) == ELF_TESTE_DADOS_TAM);
    ELF_VERIFICA(c, LeLe(&a, rw + ELF_TESTE_OFF_PH_MEMSZ, 8) > LeLe(&a, rw + ELF_TESTE_OFF_PH_FILESZ, 8));
    ELF_VERIFICA(c, LeLe(&a, rw + ELF_TESTE_OFF_PH_OFFSET, 8) % 0x1000u == 0 && LeLe(&a, rx + ELF_TESTE_OFF_PH_OFFSET, 8) % 0x1000u == 0);
    BufLiberar(&a);
    ImagemLiberar(&im);
}

/* Sem .data: o RW so tem o .bss (filesz 0, memsz = o .bss) e o ELF sai valido. */
static void TestaSoBss(const ElfContagem *c)
{
    Imagem im;
    Buf a = {0};
    Diag erro = {0};
    uint32_t rw = ELF_TESTE_PH0 + 2 * ELF_TESTE_TAM_PHDR;

    MontaImagem(&im, false, true);
    ELF_VERIFICA(c, ElfEscrever(&im, &a, &erro));
    ELF_VERIFICA(c, LeLe(&a, rw + ELF_TESTE_OFF_PH_FILESZ, 8) == 0 && LeLe(&a, rw + ELF_TESTE_OFF_PH_MEMSZ, 8) == ELF_TESTE_BSS_TAM);
    ELF_VERIFICA(c, LeLe(&a, rw + ELF_TESTE_OFF_PH_OFFSET, 8) <= a.tam);
    BufLiberar(&a);
    ImagemLiberar(&im);
}

static void TestaSimbolo(const ElfContagem *c)
{
    Imagem im;
    Buf com = {0}, sem = {0};
    Diag erro = {0};

    MontaImagem(&im, true, true);
    ELF_VERIFICA(c, ElfEscrever(&im, &sem, &erro));
    *(ImagemSimbolo *)VecEmpurrar(&im.simbolos) = (ImagemSimbolo){"_start", im.entrada_rva, 1};
    ELF_VERIFICA(c, ElfEscrever(&im, &com, &erro) && com.tam >= sem.tam + ELF_TESTE_TAM_SIMBOLO + ELF_TESTE_TAM_NOME_START);
    BufLiberar(&com);
    BufLiberar(&sem);
    ImagemLiberar(&im);
}

static bool RecusaCom(const Imagem *im, const char *trecho)
{
    Buf saida = {0};
    Diag erro = {0};
    bool recusou = !ElfEscrever(im, &saida, &erro) && erro.tem && erro.codigo == X0905 && strstr(erro.causa, trecho) != NULL;

    BufLiberar(&saida);
    return recusou;
}

static void TestaRecusas(const ElfContagem *c)
{
    Imagem fora_de_ordem, pe, sem_exec;

    MontaImagem(&fora_de_ordem, true, false);
    ELF_VERIFICA(c, RecusaCom(&fora_de_ordem, "ordem R, RX, RW"));
    MontaImagem(&pe, true, true);
    pe.formato = IMAGEM_PE;
    ELF_VERIFICA(c, RecusaCom(&pe, "nao e do formato ELF"));
    MontaImagem(&sem_exec, true, true);
    sem_exec.entrada_rva = sem_exec.secoes[0].rva;
    ELF_VERIFICA(c, RecusaCom(&sem_exec, "e_entry"));
    ImagemLiberar(&fora_de_ordem);
    ImagemLiberar(&pe);
    ImagemLiberar(&sem_exec);
}

/* Programa minimo do alvo linux que chama as duas portas do curto (print curto de f64 e eprint): forja_programa com 2 REL32 para simbolos indefinidos. */
static void MontaProgramaComCurto(Objeto *programa)
{
    static const char zeros[16] = {0};
    uint32_t texto;

    ObjIniciar(programa);
    texto = ObjSecaoAbrir(programa, ".text", OS_TEXTO, 16);
    BufAnexarN(&((ObjSecao *)VecEm(&programa->secoes, texto))->bytes, zeros, sizeof zeros);
    ObjSimboloDefinir(programa, "forja_programa", (int32_t)texto, 0);
    ObjRelocar(programa, (ObjAlvoReloc){texto, 1}, (ObjCorrecao){OR_REL32, ObjSimboloReferir(programa, "forja_fmt_f64_curto"), 0});
    ObjRelocar(programa, (ObjAlvoReloc){texto, 6}, (ObjCorrecao){OR_REL32, ObjSimboloReferir(programa, "forja_fmt_imprimir_erro"), 0});
}

static bool ImagemTemSecao(const Imagem *im, const char *nome)
{
    for (uint32_t k = 0; k < im->qtd_secoes; k++)
        if (strcmp(im->secoes[k].nome, nome) == 0)
            return true;
    return false;
}

static bool ImagemTemSimbolo(const Imagem *im, const char *nome)
{
    for (size_t k = 0; k < im->simbolos.n; k++)
        if (strcmp(((const ImagemSimbolo *)VecEm(&im->simbolos, k))->nome, nome) == 0)
            return true;
    return false;
}

/* IA1-LX: o rt_win_curto.o (COFF) liga no ELF junto do runtime Linux: sem simbolo indefinido (o ligador recusaria com X0903), sem .pdata/.xdata/.idata, com os simbolos do curto vivos e a mesma imagem nas duas ligacoes. */
static void TestaCurtoNoElf(const ElfContagem *c)
{
    Objeto programa, runtime, comum, curto, entrada;
    Imagem im, outra;
    Diag erro = {0};
    size_t n = 0;
    const uint8_t *bytes = RtNativoLinux(BM_RELEASE, &n);
    bool lidos;

    ObjIniciar(&runtime);
    lidos = CoffLer(bytes, n, &runtime, &erro);
    ObjIniciar(&comum);
    ObjIniciar(&curto);
    bytes = RtNativoLinuxComum(&n);
    lidos = lidos && CoffLer(bytes, n, &comum, &erro);
    bytes = RtNativoLinuxCurto(&n); /* P39-B1 (D0654): o curto do ELF e o rt_linux_curto.o, como em nativo.c LerCurto */
    lidos = lidos && CoffLer(bytes, n, &curto, &erro);
    /* M5b P39: o _start e do objeto de entrada (rt_linux_entrada_cru.o), nao da variante. */
    ObjIniciar(&entrada);
    bytes = RtNativoLinuxEntrada(&n);
    lidos = lidos && CoffLer(bytes, n, &entrada, &erro);
    ELF_VERIFICA(c, lidos);
    MontaProgramaComCurto(&programa);
    ImagemIniciar(&im);
    ImagemIniciar(&outra);
    LigarPedido pedido = {.programa = &programa, .runtime = &runtime, .ryu = &comum, .curto = &curto, .entrada = &entrada, .formato = IMAGEM_ELF};
    ELF_VERIFICA(c, LigarComTabelas(&pedido, &im, &erro) && !erro.tem);
    ELF_VERIFICA(c, !ImagemTemSecao(&im, ".pdata") && !ImagemTemSecao(&im, ".xdata") && !ImagemTemSecao(&im, ".idata") && !ImagemTemSecao(&im, ".reloc"));
    ELF_VERIFICA(c, ImagemTemSimbolo(&im, "forja_fmt_f64_curto") && ImagemTemSimbolo(&im, "forja_fmt_imprimir_erro") && ImagemTemSimbolo(&im, "rt_saida_stderr"));
    ELF_VERIFICA(c, LigarComTabelas(&pedido, &outra, &erro) && im.qtd_secoes == outra.qtd_secoes && im.simbolos.n == outra.simbolos.n);
    ImagemLiberar(&im);
    ImagemLiberar(&outra);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
    ObjLiberar(&comum);
    ObjLiberar(&curto);
    ObjLiberar(&entrada);
}

#define ELF_TESTE_TAM_PH 56u
#define ELF_TESTE_PT_DYNAMIC 2u
#define ELF_TESTE_TAM_DT 16u
#define ELF_TESTE_DT_RELA 7u
#define ELF_TESTE_DT_RELASZ 8u
#define ELF_TESTE_RELATIVAS 2u

/* P41 (--pie): ET_DYN, 7 cabecalhos de programa (PT_PHDR, 3 PT_LOAD, o PT_LOAD do .dynamic/.rela.dyn, PT_DYNAMIC, PT_GNU_STACK), o .dynamic no
 * inicio do seu PT_LOAD com DT_RELA logo depois, e cada RELATIVE gravado com offset = RVA do campo, info = 8 e adendo = o valor. */
static void TestaPie(const ElfContagem *c)
{
    Imagem im;
    Buf saida = {0};
    Diag erro = {0};
    uint64_t dyn_off = 0, dyn_va = 0, relasz = 0, rela_va = 0, rela_off;
    bool ok;

    MontaImagem(&im, true, true);
    im.pie = true;
    for (uint32_t k = 0; k < ELF_TESTE_RELATIVAS; k++) {
        ImagemRelativa *r = VecEmpurrar(&im.relativos);
        r->rva = im.secoes[0].rva + 8u * k;
        r->valor = 0x1000u + k;
    }
    ok = ElfEscrever(&im, &saida, &erro);
    ELF_VERIFICA(c, ok);
    ELF_VERIFICA(c, LeLe(&saida, 16, 2) == 3 && LeLe(&saida, 56, 2) == 7);
    for (uint32_t k = 0; k < 7; k++) {
        uint32_t ph = ELF_TESTE_PH0 + k * ELF_TESTE_TAM_PH;
        if (LeLe(&saida, ph, 4) == ELF_TESTE_PT_DYNAMIC) {
            dyn_off = LeLe(&saida, ph + 8, 8);
            dyn_va = LeLe(&saida, ph + 16, 8);
        }
    }
    ELF_VERIFICA(c, dyn_va != 0);
    for (uint32_t k = 0; k < 5; k++) {
        uint32_t d = (uint32_t)dyn_off + k * ELF_TESTE_TAM_DT;
        if (LeLe(&saida, d, 8) == ELF_TESTE_DT_RELASZ)
            relasz = LeLe(&saida, d + 8, 8);
        if (LeLe(&saida, d, 8) == ELF_TESTE_DT_RELA)
            rela_va = LeLe(&saida, d + 8, 8);
    }
    ELF_VERIFICA(c, relasz == ELF_TESTE_RELATIVAS * 24u && rela_va == dyn_va + 5u * ELF_TESTE_TAM_DT);
    rela_off = dyn_off + 5u * ELF_TESTE_TAM_DT;
    ELF_VERIFICA(c, LeLe(&saida, (uint32_t)rela_off, 8) == im.secoes[0].rva && LeLe(&saida, (uint32_t)rela_off + 8, 8) == 8 &&
                        LeLe(&saida, (uint32_t)rela_off + 16, 8) == 0x1000);
    ImagemLiberar(&im);
    BufLiberar(&saida);
}

void ElfVerificacoes(int *total, int *falhas)
{
    ElfContagem c = {total, falhas};

    TestaCurtoNoElf(&c);
    TestaGravacao(&c);
    TestaSegmentos(&c);
    TestaSoBss(&c);
    TestaSimbolo(&c);
    TestaRecusas(&c);
    TestaPie(&c);
}
