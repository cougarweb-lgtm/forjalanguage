/* teste_nativo.c - testes do backend nativo (M1 P3): objeto, secoes, .idata, .reloc e o PE32+ com autoconferencia.
 * Versao: 0.9.23
 *            0.9.23 - M8 P2: os 2 testes do Alcance passam o Objeto vazio tambem no 16o slot (perfil, ALCANCE_QTD_OBJETOS 16); sem isso obj[15] ficava NULL.
 *            0.9.22 - M7a F1: +verificacoes de tests\nativo\unidade\teste_analises.c (AnalisesVerificacoes: preds, RPO, idom, lacos, profundidade e vivacidade em CFGs de mao).
 *            0.9.21 - M5b P39-B2: os 2 testes do Alcance passam o objeto semente_elf (crt1.o, 15 objetos: ALCANCE_QTD_OBJETOS 15); sem ele obj[14] ficava NULL.
 * Changelog: 0.9.20 - M5b P39: os 2 testes do Alcance passam o objeto de entrada (entrada_elf, 14 objetos); TestaRtNativoLinux: o _start sai das variantes (D-R3); os de ELF passam LigarPedido.entrada.
 *            0.9.19 - M5 P53: os 2 testes do Alcance passam tambem o Objeto do std:net (net, rt_win_net.o): AlcanceEntrada leva 13 objetos (ALCANCE_QTD_OBJETOS 13).
 *            0.9.18 - M5 P52: os 2 testes do Alcance passam tambem o Objeto do std:mem VirtualArena (varena, rt_win_varena.o): AlcanceEntrada leva 12 objetos (ALCANCE_QTD_OBJETOS 12).
 *            0.9.17 - M5 P50: os 2 testes do Alcance passam tambem o Objeto do std:atomic (atomic, rt_win_atomic.o): AlcanceEntrada leva 11 objetos (ALCANCE_QTD_OBJETOS 11).
 *            0.9.16 - M5 P49: os 2 testes do Alcance passam tambem o Objeto do std:sync (sync, rt_win_sync.o): AlcanceEntrada leva 10 objetos (ALCANCE_QTD_OBJETOS 10).
 *            0.9.15 - M5 P47: os 2 testes do Alcance passam tambem o Objeto do std:thread (thread, rt_win_thread.o): AlcanceEntrada leva 9 objetos (ALCANCE_QTD_OBJETOS 9); sem isso obj[8] ficava NULL.
 *            0.9.14 - S3 P5b-1: os 2 testes do Alcance passam tambem a libm (math) e o handle do std:file (es): AlcanceEntrada leva 8 objetos (ALCANCE_QTD_OBJETOS 8); sem isso o teste lia obj[6] NULL.
 *            0.9.13 - IA1-L5a: AlcanceEntrada leva 6 objetos (curto, rt_win_curto.o); os 2 testes do Alcance passam tambem um Objeto curto vazio.
 *            0.9.12 - M5 P27b: +20 verificacoes em tests\nativo\unidade\teste_elf.c (ElfVerificacoes: escritor ELF64, segmentos, so .bss, simbolo, recusas com X0905).
 *            0.9.11 - M5 P27a: +2 funcoes de teste do ligador neutro (TestaLigarLinux: 8 verificacoes - IMAGEM_ELF sem .pdata/.xdata/.idata/DIR64, .rdata antes do .text, entrada _start, simbolos vivos, ADDR64 com a base 0x400000; TestaLigarLinuxRecusas: 2 verificacoes - DLL e tabelas
 *                    de imports = X0903; +10 no total) e o modo "x64-linux <arq.fj> [--release]" (gerar_x64.c 0.2.0: a Imagem do 01_ola para o Linux).
 *            0.9.10 - M5 P26d: +2 verificacoes (TestaRtNativoLinux: os tres objetos do runtime Linux embutidos sao COFF aceitos pelo CoffLer, sem .pdata/.xdata/__imp_, com _start, sys_heap_alocar e o Ryu).
 *            0.9.9 - M4 P24p: +4 verificacoes em tests\nativo\bir\gerar_bir_alcance.c (AlcanceVerificacoes: poda do inalcancavel com ciclo, recursao, endereco tomado e export).
 *            0.9.8 - M3b P23f: AlcanceEntrada leva 5 objetos (legado); os 2 testes do Alcance passam tambem um Objeto legado vazio; +2 verificacoes de teste_terceiros.c (construtores, X0107).
 *            0.9.7 - M3b P23e: +5 verificacoes em tests\nativo\unidade\teste_terceiros.c (TerceirosVerificacoes: COMDAT, secoes com '$', WEAK_EXTERNAL, simbolo comum, .drectve e relocacoes);
 *                    AlcanceEntrada leva 4 objetos: os 2 testes do Alcance passam tambem um Objeto de terceiros vazio.
 *            0.9.6 - M3 F5: AlcanceEntrada leva 3 objetos; os 2 testes do Alcance passam um Objeto de Ryu vazio.
 *            0.9.5 - M3b P23d: +6 verificacoes em tests\nativo\unidade\teste_estatica.c (EstaticaVerificacoes: indice GNU, 2o indice MS, ar sem indice/SYM64, ponto fixo entre 2 estaticas, X0106, indice e membro tortos)
 *                    e o modo "estatica <lib.a>... -- <simbolo>..." (EstaticaModo: imprime os membros puxados, para o oraculo do mapa do ld em tests\nativo\verificar_estatica.py).
 *            0.9.4 - M3 P22b: modo "dll64 <saida.dll>" (DLL de teste com 2 campos ADDR64 reais em paginas diferentes e 4 exports que os leem; carregada fora da base por libs\fx_realoca.exe). Nenhuma verificacao nova na unidade.
 *            0.9.3 - M3 P22a: TestaExportacoes (+2 verificacoes: .edata em ordem ASCII com a EAT acompanhando e nome repetido recusado; ordinal base 1, EOT, Name, IMAGE_FILE_DLL, ImageBase da DLL e diretorio 0).
 *            0.9.2 - M3 P19b: +5 verificacoes em tests\nativo\unidade\teste_implib.c (ImplibVerificacoes: MSVC NAME/NOPREFIX/UNDECORATE/EXPORTAS, DATA/ORDINAL/i386, GNU head/tail, ar "//" e nomes longos, import library em arquivo).
 *            0.9.1 - M3 P19c: TestaBibliotecasDoLink (+2 verificacoes: "msvcrt" e "ucrt" viram ucrtbase.dll; fflush de um programa que liga "msvcrt" cai na tabela ucrtbase.dll).
 *            0.9.0 - M3 P19: TestaBibliotecasDoLink (+4 verificacoes: a ULTIMA biblioteca do 'link' que exporta o simbolo vence, como o ld; forwarder entra pelo nome; __imp_ achado na tabela da DLL; ordem dos descritores) e
 *                    imports_usados do Alcance vira bitmap (AlcanceImportUsado nas 2 verificacoes do P17c).
 *            0.8.0 - M3 P19a: TestaPeLerEBiblioteca (+6 verificacoes: PeLerExportacoes em kernel32.dll e ucrtbase.dll reais, forwarder, PE32 = X0105; BibliotecaAchar:
 *                    ordem do ld, link "m"/"c", tipo pelo conteudo) e modo "pe2 <saida.exe>" (o exe de teste com .idata de 2 DLLs: KERNEL32.dll e ucrtbase.dll);
 *                    TestaLigarSoOAlcancavel confere as DLLs pela Imagem (LigarImportacoes saiu).
 *            0.7.1 -P18a: TestaNumeracaoIr (+2 verificacoes: IrNumerarFuncao num if com else e num switch; IrIndiceDe de instrucao alheia).
 *            0.7.0 - M2 P17c: TestaAlcance (descarte de secao inalcancavel, cadeia de relocacoes, .pdata/.xdata do que ficou, import so alcancavel, erro de simbolo vivo
 *                    indefinido), TestaAcharSimboloMuitos (hash, 100 mil simbolos), TestaCoffGrupoDeSecao (.text$f do runtime real); TestaReloc64Sintetica cita o .data pelo .text.
 *            0.6.2 - M2 P9a fase 2: ImagemSecao com Diag; caso de secao sem bytes (X0905 "secao '.vazia' sem bytes", devolve 0).
 *            0.6.1 - M2 P9a: ImagemSecao com PeSecaoPedido (a assinatura nova de pe.h).
 *            0.6.0 - backend x64 P8: TestaDebugItens (+6 verificacoes da lista do --debug-code).
 *            0.5.0 - backend x64 P7a: modo "x64 <arq.fj> <saida.exe> [--release]" (tests\nativo\x64\gerar_x64.c).
 *            0.4.0 - backend x64 P6: modos "bir <arq.fj> [--release]" e "bir-tempo <arq.fj>" e BirVerificacoes na unidade (tests\nativo\bir\gerar_bir.c).
 *            0.3.0 - backend x64 P5: modo "obj <pasta>" (forja_programa ligado ao runtime, caso1.exe e caso2.exe) e unidade do
 *                    leitor COFF (CoffLer) e do ligador (Ligar, ADDR64 sintetica no .reloc).
 *            0.2.0 - backend x64 P4b: modo "enc <pasta>" (casos E01..E18 do codificador para o oraculo NASM, com casos.asm, nossos.bin
 *                    e casos.idx) e verificacoes de unidade do codificador (encode, recusas e formatacao).
 *            0.1.0 - backend x64 P3: TestaObjeto, TestaSecoesImagem, TestaImportacoes, TestaPe, TestaReloc; modos
 *                    "pe <saida.exe>" (monta o exe de teste a mao) e "corrompido <saida.exe>" (X0905 de proposito).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe   -> "OK: N verificacoes"
 *              python -I ..\tests\nativo\pe\verificar_pe.py
 */
#include <stdio.h>
#include <stdlib.h>

#include "ir/ir_numerar.h"
#include "nativo/fmt/pe.h"
#include "nativo/nativo_debug.h"
#include "nativo/obj/biblioteca.h"
#include "nativo/obj/bibliotecas.h"
#include "nativo/obj/coff.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/ligar_alcance.h"
#include "nativo/obj/objeto.h"
#include "nativo/obj/pe_ler.h"
#include "nativo/rt_embutido.h"
#include "nativo/x64/x64.h"
#include "../enc/operandos_enc.h"
#include <stdint.h>
#include <string.h>
#include <time.h>

/* Gerador de casos do modo "enc": tests\nativo\enc\gerar_casos.c (entra no teste_nativo.exe pelo construir.bat). */
int ModoEnc(const char *pasta);

/* Gerador do modo "obj" do P5: tests\nativo\obj\gerar_obj.c (entra no teste_nativo.exe pelo construir.bat). */
int ModoObj(const char *pasta);

/* Modos e verificacoes da BIR (P6): tests\nativo\bir\gerar_bir.c (entra no teste_nativo.exe pelo construir.bat). */
int ModoBir(const char *arquivo, bool release);
int ModoBirTempo(const char *arquivo);
void BirVerificacoes(int *total, int *falhas);

/* Verificacoes da poda do inalcancavel (P24p): tests\nativo\bir\gerar_bir_alcance.c (entra no teste_nativo.exe pelo construir.bat). */
void AlcanceVerificacoes(int *total, int *falhas);

/* Verificacoes das analises da BIR (M7a F1): tests\nativo\unidade\teste_analises.c (entra no teste_nativo.exe pelo construir.bat). */
void AnalisesVerificacoes(int *total, int *falhas);
/* Verificacoes da classe BC_V128 (S1b): tests\nativo\unidade\teste_vetor.c (entra no teste_nativo.exe pelo construir.bat). */
void VetorVerificacoes(int *total, int *falhas);

/* Gerador do modo "x64" do P7a: tests\nativo\x64\gerar_x64.c (entra no teste_nativo.exe pelo construir.bat). */
int ModoX64(const char *arquivo, const char *saida, bool release);

/* Modo "x64-linux" do P27a (mesmo arquivo): a Imagem neutra do alvo x86_64-linux, conferida e sem escritor. */
int ModoX64Linux(const char *arquivo, bool release);

/* Verificacoes do leitor ar e da import library (P19b): tests\nativo\unidade\teste_implib.c (entra no teste_nativo.exe pelo construir.bat). */
void ImplibVerificacoes(int *total, int *falhas);

/* Verificacoes e modo de linha de comando da selecao de membros de estaticas (P23d): tests\nativo\unidade\teste_estatica.c (entra no teste_nativo.exe pelo construir.bat). */
void EstaticaVerificacoes(int *total, int *falhas);
int EstaticaModo(int argc, char **argv);

/* Verificacoes da uniao de membros COFF de terceiros (P23e): tests\nativo\unidade\teste_terceiros.c (entra no teste_nativo.exe pelo construir.bat). */
void TerceirosVerificacoes(int *total, int *falhas);

/* Verificacoes do escritor ELF64 (P27b): tests\nativo\unidade\teste_elf.c (entra no teste_nativo.exe pelo construir.bat). */
void ElfVerificacoes(int *total, int *falhas);
void LigarElfVerificacoes(int *total, int *falhas);
/* M5b P37d: censo de leitura dos membros ELF das 4 libs do Linux (tests\nativo\unidade\teste_elf_membros.c). */
void ElfMembrosVerificacoes(int *total, int *falhas);

static int g_total, g_falhas;

#define VERIFICA(cond) VerificaCondicao((cond), #cond, __LINE__)

/* Entrada fora de qualquer secao: a autoconferencia tem de recusar (X0905). */
#define TESTE_ENTRADA_CORROMPIDA 0x7FFF0000u
#define TESTE_TAM_MAXIMO_EXE 3072u
#define TESTE_CARAC_TEXTO 0x60000020u
#define TESTE_RVA_TEXTO 0x1000u
#define TESTE_RVA_IDATA 0x1000u
#define TESTE_RVA_IAT 0x1048u
#define TESTE_RVA_HINT 0x1068u
#define TESTE_RVA_NOME_DLL 0x1092u
#define TESTE_ALINHAMENTO_ARQUIVO 0x200u

/* Posicoes dos disp32 dos "call qword [rip+disp32]" no codigo abaixo (o disp32 ocupa 4 bytes depois do FF 15). */
#define TESTE_POS_GETSTD 11
#define TESTE_POS_WRITEFILE 47
#define TESTE_POS_EXITPROCESS 58

static void VerificaCondicao(bool ok, const char *texto, int linha)
{
    g_total++;
    if (ok) return;
    g_falhas++;
    fprintf(stderr, "FALHOU linha %d: %s\n", linha, texto);
}

/* Codigo do teste, montado uma vez com nasm -f bin (asm no scratchpad da execucao):
 *   sub rsp,56 / mov ecx,-11 / call [rip+GetStdHandle] / mov rcx,rax / lea rdx,[rip+msg] / mov r8d,5
 *   lea r9,[rsp+40] / mov qword [rsp+32],0 / call [rip+WriteFile] / mov ecx,7 / call [rip+ExitProcess]
 *   msg: db "ola", 13, 10
 * Os disp32 dos calls vao zerados aqui; PatchearDisp os corrige a partir de iat_rva. O lea ja sai com o disp fixo. */
static const uint8_t CodigoTeste[] = {
    0x48, 0x83, 0xEC, 0x38,                                     /* sub rsp,56 */
    0xB9, 0xF5, 0xFF, 0xFF, 0xFF,                               /* mov ecx,-11 */
    0xFF, 0x15, 0x00, 0x00, 0x00, 0x00,                         /* call [rip+GetStdHandle] */
    0x48, 0x89, 0xC1,                                           /* mov rcx,rax */
    0x48, 0x8D, 0x15, 0x25, 0x00, 0x00, 0x00,                   /* lea rdx,[rip+msg] */
    0x41, 0xB8, 0x05, 0x00, 0x00, 0x00,                         /* mov r8d,5 */
    0x4C, 0x8D, 0x4C, 0x24, 0x28,                               /* lea r9,[rsp+40] */
    0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00,       /* mov qword [rsp+32],0 */
    0xFF, 0x15, 0x00, 0x00, 0x00, 0x00,                         /* call [rip+WriteFile] */
    0xB9, 0x07, 0x00, 0x00, 0x00,                               /* mov ecx,7 */
    0xFF, 0x15, 0x00, 0x00, 0x00, 0x00,                         /* call [rip+ExitProcess] */
    'o', 'l', 'a', 0x0D, 0x0A,                                  /* msg: "ola\r\n" */
};

static uint64_t LeBytes(const uint8_t *p, uint32_t n)
{
    uint64_t r = 0;
    for (uint32_t k = 0; k < n; k++)
        r |= (uint64_t)p[k] << (8 * k);
    return r;
}

static bool TemTexto(const Buf *b, const char *texto)
{
    size_t n = strlen(texto);
    for (size_t i = 0; i + n <= b->tam; i++) {
        if (memcmp(b->dados + i, texto, n) == 0) return true;
    }
    return false;
}

static ObjSimbolo *Simbolo(Objeto *o, size_t i)
{
    return VecEm(&o->simbolos, i);
}

static void TestaObjeto(void)
{
    Objeto o;
    ObjIniciar(&o);
    VERIFICA(ObjSecaoAbrir(&o, ".text", OS_TEXTO, 16) == 0);
    VERIFICA(ObjSecaoAbrir(&o, ".bss", OS_BSS, 32) == 1);
    VERIFICA(((ObjSecao *)VecEm(&o.secoes, 1))->alinhamento == 32);
    VERIFICA(ObjSimboloReferir(&o, "ExitProcess") == 0);
    VERIFICA(ObjSimboloDefinir(&o, "main", 0, 4) == 1);
    VERIFICA(ObjSimboloDefinir(&o, ".L1", 0, 8) == 2);
    VERIFICA(ObjSimboloReferir(&o, "main") == 1);
    VERIFICA(Simbolo(&o, 0)->secao == OBJ_SECAO_INDEFINIDA);
    VERIFICA(Simbolo(&o, 1)->global && !Simbolo(&o, 2)->global);
    ObjRelocar(&o, (ObjAlvoReloc){0, 8}, (ObjCorrecao){OR_REL32, 0, -4});
    VERIFICA(o.relocs.n == 1);
    VERIFICA(((ObjReloc *)VecEm(&o.relocs, 0))->correcao.adendo == -4);
    ObjLiberar(&o);
}

static void TestaSecoesImagem(void)
{
    Imagem im;
    Buf b = {0};
    Diag erro = {0};
    BufAnexar(&b, "\x90\xc3");
    ImagemIniciar(&im);
    VERIFICA(ImagemSecao(&im, &(PeSecaoPedido){".text", TESTE_CARAC_TEXTO, &b, 0}, &erro) == TESTE_RVA_TEXTO);
    VERIFICA(ImagemSecao(&im, &(PeSecaoPedido){".rdata", 0x40000040u, NULL, 0x20u}, &erro) == 0x2000u);
    VERIFICA(ImagemProximoRva(&im) == 0x3000u);
    VERIFICA(im.secoes[0].tam_virtual == 2 && im.secoes[1].tam_virtual == 0x20u);
    VERIFICA(memcmp(im.secoes[0].nome, ".text\0\0\0", 8) == 0);
    VERIFICA(ImagemSecao(&im, &(PeSecaoPedido){".vazia", 0x40000040u, NULL, 0}, &erro) == 0 && im.qtd_secoes == 2 && erro.tem &&
             erro.codigo == X0905 && strcmp(erro.causa, "secao '.vazia' sem bytes") == 0);
    VERIFICA(!PeReservarImportacoes(&im, "K.dll", NULL, 0));
    BufLiberar(&b);
    ImagemLiberar(&im);
}

static void TestaImportacoes(void)
{
    static const char *const nomes[] = {"GetStdHandle", "WriteFile", "ExitProcess"};
    Imagem im;
    ImagemIniciar(&im);
    VERIFICA(PeReservarImportacoes(&im, "KERNEL32.dll", nomes, 3));
    const ImagemImportacao *imp = &im.importacao;
    VERIFICA(imp->dir_import_rva == TESTE_RVA_IDATA && imp->dir_import_tam == 40u);
    VERIFICA(imp->dir_iat_rva == TESTE_RVA_IAT && imp->dir_iat_tam == 32u);
    VERIFICA(imp->iat_rva[0] == TESTE_RVA_IAT && imp->iat_rva[2] == TESTE_RVA_IAT + 16u);
    const Buf *idata = &im.secoes[0].bytes;
    const uint8_t *d = (const uint8_t *)idata->dados;
    VERIFICA(LeBytes(d + 40, 8) == TESTE_RVA_HINT);
    VERIFICA(LeBytes(d + 72, 8) == TESTE_RVA_HINT);
    VERIFICA(LeBytes(d + 12, 4) == TESTE_RVA_NOME_DLL);
    VERIFICA(TemTexto(idata, "KERNEL32.dll") && TemTexto(idata, "ExitProcess"));
    ImagemLiberar(&im);
}

/* Grava o disp32 de um "call [rip+disp32]" que termina em pos+4: disp = alvo - proxima instrucao (RVA do .text + pos + 4). */
static void PatchearDisp(uint8_t *codigo, uint32_t pos, uint32_t alvo, uint32_t text_rva)
{
    uint32_t disp = alvo - (text_rva + pos + 4u);
    for (uint32_t k = 0; k < 4; k++)
        codigo[pos + k] = (uint8_t)(disp >> (8 * k));
}

/* Reserva o .idata do exe de teste: so KERNEL32, ou (duas_dlls, P19a) KERNEL32 e depois ucrtbase.dll com abs e strlen que o codigo nao chama. */
static bool ReservarImportacoesTeste(Imagem *im, bool duas_dlls)
{
    static const char *const nomes[] = {"GetStdHandle", "WriteFile", "ExitProcess"};
    static const char *const nomes_ucrt[] = {"abs", "strlen"};
    const PeImportDll dlls[] = {{"KERNEL32.dll", nomes, 3}, {"ucrtbase.dll", nomes_ucrt, 2}};
    return PeReservarImportacoesDlls(im, dlls, duas_dlls ? 2 : 1);
}

/* Monta a imagem do exe de teste: .idata com KERNEL32 primeiro (o RVA do .text sai dele) e o .text com os disp32 corrigidos.
 * Devolve false so se as importacoes forem recusadas. */
static bool MontarImagemTeste(Imagem *im, bool duas_dlls)
{
    uint8_t codigo[sizeof CodigoTeste];
    Buf texto = {0};
    Diag erro = {0};
    ImagemIniciar(im);
    if (!ReservarImportacoesTeste(im, duas_dlls))
        return false;
    uint32_t text_rva = ImagemProximoRva(im);
    memcpy(codigo, CodigoTeste, sizeof codigo);
    PatchearDisp(codigo, TESTE_POS_GETSTD, im->importacao.iat_rva[0], text_rva);
    PatchearDisp(codigo, TESTE_POS_WRITEFILE, im->importacao.iat_rva[1], text_rva);
    PatchearDisp(codigo, TESTE_POS_EXITPROCESS, im->importacao.iat_rva[2], text_rva);
    BufAnexarN(&texto, (const char *)codigo, sizeof codigo);
    im->entrada_rva = ImagemSecao(im, &(PeSecaoPedido){".text", TESTE_CARAC_TEXTO, &texto, (uint32_t)texto.tam}, &erro);
    BufLiberar(&texto);
    return im->entrada_rva == text_rva;
}

static void TestaPe(void)
{
    Imagem im, ruim;
    Buf a = {0}, c = {0}, d = {0};
    Diag erro = {0};
    VERIFICA(MontarImagemTeste(&im, false));
    VERIFICA(PeEscrever(&im, &a, &erro));
    VERIFICA(a.tam > 0 && a.tam <= TESTE_TAM_MAXIMO_EXE && a.tam % TESTE_ALINHAMENTO_ARQUIVO == 0);
    VERIFICA(memcmp(a.dados, "MZ", 2) == 0);
    VERIFICA(PeEscrever(&im, &c, &erro));
    VERIFICA(a.tam == c.tam && memcmp(a.dados, c.dados, a.tam) == 0);
    VERIFICA(!erro.tem);
    MontarImagemTeste(&ruim, false);
    ruim.entrada_rva = TESTE_ENTRADA_CORROMPIDA;
    VERIFICA(!PeEscrever(&ruim, &d, &erro));
    VERIFICA(erro.tem && erro.codigo == X0905);
    BufLiberar(&a);
    BufLiberar(&c);
    BufLiberar(&d);
    ImagemLiberar(&im);
    ImagemLiberar(&ruim);
}

static void TestaReloc(void)
{
    Imagem im;
    Buf exe = {0};
    Diag erro = {0};
    MontarImagemTeste(&im, false);
    *(uint32_t *)VecEmpurrar(&im.dir64) = TESTE_RVA_TEXTO + 8u;
    VERIFICA(PeEscrever(&im, &exe, &erro));
    VERIFICA(TemTexto(&exe, ".reloc"));
    BufLiberar(&exe);
    ImagemLiberar(&im);
}

/* ---- M3 P22a: o export directory (PeReservarExportacoes, pe_exportar.c). A secao .edata e lida pelos RVAs (rva - base da secao = offset nos bytes). ---- */

#define TESTE_OFF_EDATA_NOME_DLL 12
#define TESTE_OFF_EDATA_BASE 16
#define TESTE_OFF_EDATA_NFUNCOES 20
#define TESTE_OFF_EDATA_NNOMES 24
#define TESTE_OFF_EDATA_EAT 28
#define TESTE_OFF_EDATA_ENPT 32
#define TESTE_OFF_EDATA_EOT 36
#define TESTE_OFF_CARAC_COFF 0x96      /* e_lfanew 0x80 + assinatura 4 + 22 */
#define TESTE_OFF_IMAGE_BASE 0xB0
#define TESTE_OFF_DIR_EXPORT 0x108
#define TESTE_CARAC_DLL 0x2026u
#define TESTE_IMAGE_BASE_DLL 0x180000000ull

/* O campo de 32 bits do diretorio de exportacao no offset 'off'. */
static uint32_t CampoEdata(const Imagem *im, uint32_t off)
{
    return (uint32_t)LeBytes((const uint8_t *)im->secoes[im->qtd_secoes - 1].bytes.dados + off, 4);
}

/* O texto que o RVA 'rva' aponta dentro da .edata (a ultima secao da imagem). */
static const char *TextoEdata(const Imagem *im, uint32_t rva)
{
    const PeSecao *s = &im->secoes[im->qtd_secoes - 1];
    return (const char *)s->bytes.dados + (rva - s->rva);
}

/* Nomes fora de ordem, com maiuscula e prefixo comum: a tabela sai em ordem ASCII (strcmp), a EAT acompanha os nomes e um nome repetido e recusado. */
static bool ExportaEmOrdemAscii(void)
{
    static const PeExporta pedido[] = {{"zeta", 0x1010}, {"alfa", 0x1000}, {"Beta", 0x1020}, {"alfa2", 0x1030}};
    static const char *const nomes[] = {"Beta", "alfa", "alfa2", "zeta"};
    static const uint32_t rvas[] = {0x1020, 0x1000, 0x1030, 0x1010};
    static const PeExporta repetido[] = {{"a", 1}, {"a", 2}};
    Imagem im, dup;
    bool ok;

    ImagemIniciar(&im);
    ImagemIniciar(&dup);
    ok = PeReservarExportacoes(&im, "t.dll", pedido, 4) && !PeReservarExportacoes(&dup, "t.dll", repetido, 2) && dup.qtd_secoes == 0;
    for (uint32_t k = 0; ok && k < 4; k++) {
        const PeExporta *x = VecEm(&im.exportacoes, k);
        uint32_t nome_rva = (uint32_t)LeBytes((const uint8_t *)TextoEdata(&im, CampoEdata(&im, TESTE_OFF_EDATA_ENPT)) + 4 * k, 4);
        uint32_t eat = (uint32_t)LeBytes((const uint8_t *)TextoEdata(&im, CampoEdata(&im, TESTE_OFF_EDATA_EAT)) + 4 * k, 4);

        ok = strcmp(x->nome, nomes[k]) == 0 && x->rva == rvas[k] && strcmp(TextoEdata(&im, nome_rva), nomes[k]) == 0 && eat == rvas[k];
    }
    ImagemLiberar(&im);
    ImagemLiberar(&dup);
    return ok;
}

/* Ordinal base 1: Base = 1, tantas funcoes quantos nomes, EOT = 0, 1, 2 (o ordinal do k-esimo nome e k + 1), Name = o arquivo da DLL; a imagem sai com IMAGE_FILE_DLL,
 * ImageBase 0x180000000 e o diretorio 0 apontando para a .edata. */
static bool ExportaOrdinalBase1(void)
{
    static const PeExporta pedido[] = {{"b", 0x1010}, {"a", 0x1000}, {"c", 0x1020}};
    Imagem im;
    Buf exe = {0};
    Diag erro = {0};
    bool ok = MontarImagemTeste(&im, false) && PeReservarExportacoes(&im, "t.dll", pedido, 3) && PeEscrever(&im, &exe, &erro);
    const uint8_t *eot;

    if (!ok) {
        BufLiberar(&exe);
        ImagemLiberar(&im);
        return false;
    }
    eot = (const uint8_t *)TextoEdata(&im, CampoEdata(&im, TESTE_OFF_EDATA_EOT));
    ok = CampoEdata(&im, TESTE_OFF_EDATA_BASE) == 1 && CampoEdata(&im, TESTE_OFF_EDATA_NFUNCOES) == 3 && CampoEdata(&im, TESTE_OFF_EDATA_NNOMES) == 3 &&
         LeBytes(eot, 2) == 0 && LeBytes(eot + 2, 2) == 1 && LeBytes(eot + 4, 2) == 2 &&
         strcmp(TextoEdata(&im, CampoEdata(&im, TESTE_OFF_EDATA_NOME_DLL)), "t.dll") == 0 &&
         LeBytes((const uint8_t *)exe.dados + TESTE_OFF_CARAC_COFF, 2) == TESTE_CARAC_DLL &&
         LeBytes((const uint8_t *)exe.dados + TESTE_OFF_IMAGE_BASE, 8) == TESTE_IMAGE_BASE_DLL &&
         LeBytes((const uint8_t *)exe.dados + TESTE_OFF_DIR_EXPORT, 4) == im.secoes[im.qtd_secoes - 1].rva;
    BufLiberar(&exe);
    ImagemLiberar(&im);
    return ok;
}

static void TestaExportacoes(void)
{
    VERIFICA(ExportaEmOrdemAscii());
    VERIFICA(ExportaOrdinalBase1());
}

static int GravarArquivo(const char *caminho, const Buf *exe)
{
    FILE *f = fopen(caminho, "wb");
    if (f == NULL) {
        DiagImprimirJsonSemFonte(X0302, "nao consegui abrir o arquivo de saida para gravar", stderr);
        return 1;
    }
    size_t escritos = fwrite(exe->dados, 1, exe->tam, f);
    if (fclose(f) != 0 || escritos != exe->tam) {
        DiagImprimirJsonSemFonte(X0302, "falha ao gravar o arquivo de saida", stderr);
        return 1;
    }
    printf("pe: %zu bytes em %s\n", exe->tam, caminho);
    return 0;
}

/* Modo "pe" grava o exe de teste; "pe2" grava o mesmo exe com .idata de 2 DLLs (P19a); "corrompido" tenta gravar com a entrada fora do .text e tem
 * de sair com X0905. */
static int ModoPe(const char *caminho, bool corrompe, bool duas_dlls)
{
    Imagem im;
    Buf exe = {0};
    Diag erro = {0};
    int codigo = 0;
    if (!MontarImagemTeste(&im, duas_dlls)) {
        ImagemLiberar(&im);
        return 2;
    }
    if (corrompe)
        im.entrada_rva = TESTE_ENTRADA_CORROMPIDA;
    if (!PeEscrever(&im, &exe, &erro)) {
        DiagImprimirJsonSemFonte(erro.codigo, erro.causa, stderr);
        codigo = 1;
    } else if (corrompe) {
        fprintf(stderr, "teste_nativo: a imagem corrompida foi aceita\n");
        codigo = 2;
    } else {
        codigo = GravarArquivo(caminho, &exe);
    }
    BufLiberar(&exe);
    ImagemLiberar(&im);
    return codigo;
}

/* ---- M3 P22b: modo "dll64 <saida.dll>" - DLL de teste com ADDR64 REAIS no .reloc (os programas Forja nao geram nenhum: o backend poe todo endereco por REL32/RVA).
 * Dois campos de 8 bytes no .data guardam a VA absoluta do proprio endereco (ImageBase + RVA), um em cada pagina de 4 KiB (dois blocos no .reloc). Quatro exports leem cada campo
 * e devolvem o endereco real do campo: numa carga FORA da base preferida o loader so acerta o campo se o .reloc estiver certo (libs\fx_realoca.exe confere). ---- */

#define TESTE_DLL64_PAGINA 0x1000u
#define TESTE_DLL64_TAM_DADOS 8192u
#define TESTE_DLL64_CAMPO_A 0x10u
#define TESTE_DLL64_CAMPO_B 0x1010u
#define TESTE_DLL64_TAM_FUNCAO 16u
#define TESTE_DLL64_TAM_INSTR 7u
#define TESTE_DLL64_RET 0xC3

enum { DLL64_MAIN, DLL64_LER_A, DLL64_LER_B, DLL64_END_A, DLL64_END_B, DLL64_FUNCOES };

/* mov rax,[rip+disp] (endereco = false) ou lea rax,[rip+disp] (endereco = true) seguido de ret, na funcao 'funcao' do .text (que esta em text_rva). */
static void Dll64Funcao(uint8_t *codigo, uint32_t funcao, bool endereco, int64_t alvo_menos_texto)
{
    uint8_t *p = codigo + funcao * TESTE_DLL64_TAM_FUNCAO;
    int32_t disp = (int32_t)(alvo_menos_texto - (int64_t)(funcao * TESTE_DLL64_TAM_FUNCAO + TESTE_DLL64_TAM_INSTR));

    p[0] = 0x48;
    p[1] = endereco ? 0x8D : 0x8B;
    p[2] = 0x05;
    memcpy(p + 3, &disp, sizeof disp);
    p[TESTE_DLL64_TAM_INSTR] = TESTE_DLL64_RET;
}

static bool Dll64Texto(Imagem *im, uint32_t dados_rva, Diag *erro)
{
    uint8_t codigo[DLL64_FUNCOES * TESTE_DLL64_TAM_FUNCAO] = {0xB8, 0x01, 0x00, 0x00, 0x00, TESTE_DLL64_RET};
    uint32_t texto_rva = ImagemProximoRva(im);
    Buf texto = {0};
    int64_t a = (int64_t)dados_rva + TESTE_DLL64_CAMPO_A - (int64_t)texto_rva;
    int64_t b = (int64_t)dados_rva + TESTE_DLL64_CAMPO_B - (int64_t)texto_rva;

    Dll64Funcao(codigo, DLL64_LER_A, false, a);
    Dll64Funcao(codigo, DLL64_LER_B, false, b);
    Dll64Funcao(codigo, DLL64_END_A, true, a);
    Dll64Funcao(codigo, DLL64_END_B, true, b);
    BufAnexarN(&texto, (const char *)codigo, sizeof codigo);
    im->entrada_rva = ImagemSecao(im, &(PeSecaoPedido){".text", TESTE_CARAC_TEXTO, &texto, (uint32_t)texto.tam}, erro);
    BufLiberar(&texto);
    return im->entrada_rva == texto_rva;
}

static bool Dll64Dados(Imagem *im, uint32_t dados_rva, Diag *erro)
{
    uint8_t dados[TESTE_DLL64_TAM_DADOS] = {0};
    const uint32_t campos[] = {TESTE_DLL64_CAMPO_A, TESTE_DLL64_CAMPO_B};
    Buf area = {0};
    bool ok;

    for (uint32_t k = 0; k < 2; k++) {
        uint64_t va = PeBaseDaImagem(im) + dados_rva + campos[k];

        memcpy(dados + campos[k], &va, sizeof va);
        *(uint32_t *)VecEmpurrar(&im->dir64) = dados_rva + campos[k];
    }
    BufAnexarN(&area, (const char *)dados, sizeof dados);
    ok = ImagemSecao(im, &(PeSecaoPedido){".data", PE_SECAO_DADOS, &area, (uint32_t)area.tam}, erro) == dados_rva;
    BufLiberar(&area);
    return ok;
}

/* Imagem da DLL: .text (DllMain e os 4 exports) em RVA, .data logo depois (uma pagina de .text), .edata e .reloc que o PeEscrever monta dos dir64. */
static bool MontarImagemDll64(Imagem *im, Diag *erro)
{
    static const char *const nomes[] = {"fx_dir64_ler_a", "fx_dir64_ler_b", "fx_dir64_end_a", "fx_dir64_end_b"};
    PeExporta pedido[4];
    uint32_t texto_rva;
    uint32_t dados_rva;

    ImagemIniciar(im);
    im->dll = true;
    texto_rva = ImagemProximoRva(im);
    dados_rva = texto_rva + TESTE_DLL64_PAGINA;
    if (!Dll64Texto(im, dados_rva, erro) || !Dll64Dados(im, dados_rva, erro))
        return false;
    for (uint32_t k = 0; k < 4; k++)
        pedido[k] = (PeExporta){nomes[k], texto_rva + (DLL64_LER_A + k) * TESTE_DLL64_TAM_FUNCAO};
    return PeReservarExportacoes(im, "fx_dir64.dll", pedido, 4);
}

static int ModoDll64(const char *caminho)
{
    Imagem im;
    Buf dll = {0};
    Diag erro = {0};
    int codigo = 1;

    if (MontarImagemDll64(&im, &erro) && PeEscrever(&im, &dll, &erro))
        codigo = GravarArquivo(caminho, &dll);
    else
        DiagImprimirJsonSemFonte(erro.tem ? erro.codigo : X0905, erro.tem ? erro.causa : "montagem da DLL de teste falhou", stderr);
    BufLiberar(&dll);
    ImagemLiberar(&im);
    return codigo;
}

/* ---------- codificador x64 (P4b): verificacoes de unidade ---------- */

static bool CodificaIgual(const MInstr *mi, const uint8_t *esperado, uint32_t n)
{
    uint8_t saida[16];
    uint32_t tam = 0;
    Diag erro = {0};
    if (!X64CodificarInstr(mi, saida, &tam, &erro) || tam != n)
        return false;
    return memcmp(saida, esperado, n) == 0;
}

static bool CodificaRecusa(const MInstr *mi)
{
    uint8_t saida[16];
    uint32_t tam = 0;
    Diag erro = {0};
    bool recusou = !X64CodificarInstr(mi, saida, &tam, &erro);
    return recusou && erro.tem && erro.codigo == X0902;
}

static bool FormataIgual(const MInstr *mi, const char *esperado)
{
    Buf b = {0};
    X64Formatar(mi, &b);
    bool ok = strcmp(b.dados != NULL ? b.dados : "", esperado) == 0;
    BufLiberar(&b);
    return ok;
}

static void TestaCodificarBasico(void)
{
    MInstr mi;
    mi = MiNova(FX_MOV64_RR, 2);
    mi.op[0] = OpReg(X64_RAX, 64);
    mi.op[1] = OpReg(X64_RCX, 64);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0x48, 0x89, 0xC8}, 3));
    mi = MiNova(FX_SUB32_RI, 2);
    mi.op[0] = OpReg(X64_RCX, 32);
    mi.op[1] = OpImed(1);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0x83, 0xE9, 0x01}, 3));
    mi = MiNova(FX_JMP_REL32, 1);
    mi.op[0] = OpRel(MO_BLOCO, 0);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0xE9, 0x00, 0x00, 0x00, 0x00}, 5));
    mi = MiNova(FX_JCC_REL32, 1);
    mi.cond = X64_CC_E;
    mi.op[0] = OpRel(MO_BLOCO, 16);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0x0F, 0x84, 0x10, 0x00, 0x00, 0x00}, 6));
    mi = MiNova(FX_CALL_REL32, 1);
    mi.op[0] = OpRel(MO_SIMBOLO, -5);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0xE8, 0xFB, 0xFF, 0xFF, 0xFF}, 5));
}

static void TestaCodificarRipEFlags(void)
{
    MInstr mi;
    mi = MiNova(FX_LEA64_RM, 2);
    mi.op[0] = OpReg(X64_RAX, 64);
    mi.op[1] = OpMem(X64_RIP, 0x1234, 0);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00}, 7));
    mi = MiNova(FX_LEA64_RM, 2);
    mi.op[0] = OpReg(X64_RSP, 64);
    mi.op[1] = OpMem(X64_RBP, 0, 0);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0x48, 0x8D, 0x65, 0x00}, 4));
    mi = MiNova(FX_SETCC8_R, 1);
    mi.cond = X64_CC_E;
    mi.op[0] = OpReg(X64_RSP, 8);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0x40, 0x0F, 0x94, 0xC4}, 4));
    mi = MiNova(FX_PUSH64_R, 1);
    mi.op[0] = OpReg(X64_R12, 64);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0x41, 0x54}, 2));
    mi = MiNova(FX_RET, 0);
    VERIFICA(CodificaIgual(&mi, (const uint8_t[]){0xC3}, 1));
}

static void TestaRecusas(void)
{
    MInstr mi;
    mi = MiNova(FX_ADD64_RI, 2);
    mi.op[0] = OpReg(X64_RAX, 64);
    mi.op[1] = OpImed(0x100000000LL);
    VERIFICA(CodificaRecusa(&mi));
    mi = MiNova(FX_MOV64_RM, 2);
    mi.op[0] = OpReg(X64_RAX, 64);
    mi.op[1] = OpMem(X64_RIP, 0, 64);
    VERIFICA(CodificaRecusa(&mi));
    mi = MiNova(FX_RET, 1);
    VERIFICA(CodificaRecusa(&mi));
    mi = MiNova((uint16_t)FX_QTD, 0);
    VERIFICA(CodificaRecusa(&mi));
}

static void TestaFormatar(void)
{
    MInstr mi;
    bool tabela_ok = true;
    for (uint32_t k = 0; k < FX_QTD; k++) {
        if (FORMAS_X64[k].id != (X64Forma)k)
            tabela_ok = false;
    }
    VERIFICA(tabela_ok);
    mi = MiNova(FX_MOV64_RR, 2);
    mi.op[0] = OpReg(X64_RAX, 64);
    mi.op[1] = OpReg(X64_RCX, 64);
    VERIFICA(FormataIgual(&mi, "mov rax, rcx"));
    mi = MiNova(FX_LEA64_RM, 2);
    mi.op[0] = OpReg(X64_RAX, 64);
    mi.op[1] = OpMem(X64_RBP, -8, 0);
    VERIFICA(FormataIgual(&mi, "lea rax, [rbp-8]"));
    mi = MiNova(FX_MOV32_MR, 2);
    mi.op[0] = OpMem(X64_RSP, 8, 32);
    mi.op[1] = OpReg(X64_RAX, 32);
    VERIFICA(FormataIgual(&mi, "mov dword [rsp+8], eax"));
    mi = MiNova(FX_MOVZX32_RM8, 2);
    mi.op[0] = OpReg(X64_RAX, 32);
    mi.op[1] = OpMem(X64_RSP, 0, 8);
    VERIFICA(FormataIgual(&mi, "movzx eax, byte [rsp]"));
}

static void TestaFormatarCondicoes(void)
{
    MInstr mi;
    mi = MiNova(FX_JCC_REL32, 1);
    mi.cond = X64_CC_E;
    mi.op[0] = OpRel(MO_BLOCO, 0);
    mi.op[0].imed = 3;
    VERIFICA(FormataIgual(&mi, "je b3"));
    mi = MiNova(FX_SETCC8_R, 1);
    mi.cond = X64_CC_NE;
    mi.op[0] = OpReg(X64_RSI, 8);
    VERIFICA(FormataIgual(&mi, "setne sil"));
    VERIFICA(strcmp(X64NomeRegistro(X64_R12, 32), "r12d") == 0);
}

/* ---------- ligador do P5: COFF, ligacao e ADDR64 sintetica no .reloc ---------- */

#define TESTE_TAM_COFF_CABECALHO 20u
#define TESTE_MAQUINA_AMD64_LO 0x64u
#define TESTE_MAQUINA_AMD64_HI 0x86u
#define TESTE_MAQUINA_I386_LO 0x4Cu
#define TESTE_MAQUINA_I386_HI 0x01u

static void TestaCoffCabecalho(void)
{
    uint8_t cab[TESTE_TAM_COFF_CABECALHO] = {0};
    uint8_t ruim[TESTE_TAM_COFF_CABECALHO] = {0};
    Objeto o;
    Diag erro = {0};
    Diag erro_ruim = {0};
    cab[0] = TESTE_MAQUINA_AMD64_LO;
    cab[1] = TESTE_MAQUINA_AMD64_HI;
    ruim[0] = TESTE_MAQUINA_I386_LO;
    ruim[1] = TESTE_MAQUINA_I386_HI;
    ObjIniciar(&o);
    VERIFICA(CoffLer(cab, sizeof cab, &o, &erro));
    ObjLiberar(&o);
    ObjIniciar(&o);
    VERIFICA(!CoffLer(ruim, sizeof ruim, &o, &erro_ruim) && erro_ruim.tem && erro_ruim.codigo == X0903);
    ObjLiberar(&o);
}

/* Objeto de runtime com forja_entrada e um campo ADDR64 em .data: o .reloc tem de sair com a entrada DIR64. O .text cita o .data (REL32): sem isso o
 * ligador descarta o .data por inalcancavel. */
static void TestaReloc64Sintetica(void)
{
    Objeto programa, runtime;
    Imagem im;
    Buf saida = {0};
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    uint32_t texto = ObjSecaoAbrir(&runtime, ".text", OS_TEXTO, 16);
    uint32_t dados = ObjSecaoAbrir(&runtime, ".data", OS_DATA, 8);
    uint32_t entrada = ObjSimboloDefinir(&runtime, "forja_entrada", (int32_t)texto, 0);
    uint32_t campo = ObjSimboloDefinir(&runtime, ".campo", (int32_t)dados, 0);
    BufAnexarN(&((ObjSecao *)VecEm(&runtime.secoes, texto))->bytes, "\xC3\0\0\0\0", 5);
    BufAnexarN(&((ObjSecao *)VecEm(&runtime.secoes, dados))->bytes, "\0\0\0\0\0\0\0\0", 8);
    ObjRelocar(&runtime, (ObjAlvoReloc){texto, 1}, (ObjCorrecao){OR_REL32, campo, 0});
    ObjRelocar(&runtime, (ObjAlvoReloc){dados, 0}, (ObjCorrecao){OR_ADDR64, entrada, 0});
    VERIFICA(Ligar(&programa, &runtime, &im, &erro));
    VERIFICA(im.dir64.n == 1);
    VERIFICA(PeEscrever(&im, &saida, &erro));
    VERIFICA(TemTexto(&saida, ".reloc"));
    BufLiberar(&saida);
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
}

static void TestaLigacao(void)
{
    TestaCoffCabecalho();
    TestaReloc64Sintetica();
}

/* ---------- P17c: alcancabilidade (descarte de secao e de import), grupo de secao do COFF e ObjAcharSimbolo ---------- */

#define TESTE_TAM_FUNCAO 17u
#define TESTE_QTD_IMPORTS 3u
#define TESTE_QTD_SIMBOLOS_HASH 100000u
#define TESTE_LIMITE_HASH_SEGUNDOS 2.0

/* Indices das secoes do runtime de teste (cada funcao e cada dado numa secao "base$sufixo", como o gcc com -ffunction-sections). */
typedef struct {
    uint32_t entrada, a, b, morta, rd_x, rd_y;
    uint32_t pd_a, pd_b, pd_morta, xd_a, xd_b, xd_morta;
} SecoesAlcance;

static uint32_t SecaoComBytes(Objeto *o, const char *nome, ObjTipoSecao tipo, size_t tam)
{
    static const char zeros[TESTE_TAM_FUNCAO] = {0};
    uint32_t s = ObjSecaoAbrir(o, nome, tipo, 4);
    BufAnexarN(&((ObjSecao *)VecEm(&o->secoes, s))->bytes, zeros, tam);
    return s;
}

/* Funcao de teste: 17 bytes com o simbolo global no inicio; Citar grava um REL32 no byte 'off' para o simbolo alvo (criado indefinido se preciso). */
static uint32_t FuncaoDeTeste(Objeto *o, const char *secao, const char *simbolo)
{
    uint32_t s = SecaoComBytes(o, secao, OS_TEXTO, TESTE_TAM_FUNCAO);
    ObjSimboloDefinir(o, simbolo, (int32_t)s, 0);
    return s;
}

static void Citar(Objeto *o, uint32_t secao, uint32_t off, const char *alvo)
{
    ObjRelocar(o, (ObjAlvoReloc){secao, off}, (ObjCorrecao){OR_REL32, ObjSimboloReferir(o, alvo), 0});
}

/* .pdata$sufixo (12 bytes: inicio e fim do codigo por ADDR32NB e o unwind) e o .xdata$sufixo que ele cita. Devolve {pdata, xdata}. */
static void DiretorioDeTeste(Objeto *o, const char *sufixo, const char *codigo, uint32_t saida[2])
{
    char nome[40], simbolo[40];
    snprintf(nome, sizeof nome, ".xdata$%s", sufixo);
    saida[1] = SecaoComBytes(o, nome, OS_XDATA, 4);
    snprintf(simbolo, sizeof simbolo, ".unwind_%s", sufixo);
    uint32_t unwind = ObjSimboloDefinir(o, simbolo, (int32_t)saida[1], 0);
    snprintf(nome, sizeof nome, ".pdata$%s", sufixo);
    saida[0] = SecaoComBytes(o, nome, OS_PDATA, 12);
    uint32_t cod = ObjSimboloReferir(o, codigo);
    ObjRelocar(o, (ObjAlvoReloc){saida[0], 0}, (ObjCorrecao){OR_ADDR32NB, cod, 0});
    ObjRelocar(o, (ObjAlvoReloc){saida[0], 4}, (ObjCorrecao){OR_ADDR32NB, cod, 1});
    ObjRelocar(o, (ObjAlvoReloc){saida[0], 8}, (ObjCorrecao){OR_ADDR32NB, unwind, 0});
}

/* Dado de teste: .rdata$sufixo de 8 bytes com um simbolo local ".<sufixo>". */
static uint32_t DadoDeTeste(Objeto *o, const char *sufixo)
{
    char nome[40], simbolo[40];
    snprintf(nome, sizeof nome, ".rdata$%s", sufixo);
    uint32_t s = SecaoComBytes(o, nome, OS_RDATA, 8);
    snprintf(simbolo, sizeof simbolo, ".%s", sufixo);
    ObjSimboloDefinir(o, simbolo, (int32_t)s, 0);
    return s;
}

/* Runtime de teste. forja_entrada chama rt_a e importa ExitProcess; rt_a chama rt_b e le .x. rt_morta (que importa WriteFile e um __imp_ fora da tabela,
 * le .y e cita um simbolo que ninguem define) nao e chamada por ninguem. Cada funcao tem .pdata/.xdata proprios. */
static SecoesAlcance MontarRuntimeAlcance(Objeto *rt)
{
    SecoesAlcance s = {0};
    uint32_t dir[2];
    s.entrada = FuncaoDeTeste(rt, ".text", "forja_entrada");
    s.a = FuncaoDeTeste(rt, ".text$a", "rt_a");
    s.b = FuncaoDeTeste(rt, ".text$b", "rt_b");
    s.morta = FuncaoDeTeste(rt, ".text$morta", "rt_morta");
    s.rd_x = DadoDeTeste(rt, "x");
    s.rd_y = DadoDeTeste(rt, "y");
    Citar(rt, s.entrada, 1, "rt_a");
    Citar(rt, s.entrada, 5, "__imp_ExitProcess");
    Citar(rt, s.a, 1, "rt_b");
    Citar(rt, s.a, 5, ".x");
    Citar(rt, s.morta, 1, "__imp_WriteFile");
    Citar(rt, s.morta, 5, ".y");
    Citar(rt, s.morta, 9, "__imp_Desconhecido");
    Citar(rt, s.morta, 13, "ninguem_define");
    DiretorioDeTeste(rt, "a", "rt_a", dir);
    s.pd_a = dir[0];
    s.xd_a = dir[1];
    DiretorioDeTeste(rt, "b", "rt_b", dir);
    s.pd_b = dir[0];
    s.xd_b = dir[1];
    DiretorioDeTeste(rt, "morta", "rt_morta", dir);
    s.pd_morta = dir[0];
    s.xd_morta = dir[1];
    return s;
}

static const char *const TESTE_IMPORTS[TESTE_QTD_IMPORTS] = {"ExitProcess", "WriteFile", "HeapAlloc"};
static const AlcanceRaiz TESTE_RAIZES[] = {{0, NULL}, {1, "forja_entrada"}};

/* O que o ligador descarta: so entra o que a forja_entrada alcanca por relocacao; o .pdata/.xdata sobe com o codigo vivo e desce com o morto. */
static void TestaAlcanceDescarta(void)
{
    Objeto programa, runtime;
    Alcance a;
    Objeto ryu, terceiros, legado, curto, math, es, thread, sync, atomic, varena, net, entrada_elf, semente_elf;
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ObjIniciar(&ryu);
    ObjIniciar(&terceiros);
    ObjIniciar(&legado);
    ObjIniciar(&curto);
    ObjIniciar(&math);
    ObjIniciar(&es);
    ObjIniciar(&thread);
    ObjIniciar(&sync);
    ObjIniciar(&atomic);
    ObjIniciar(&varena);
    ObjIniciar(&net);
    ObjIniciar(&entrada_elf);
    ObjIniciar(&semente_elf);
    SecoesAlcance s = MontarRuntimeAlcance(&runtime);
    AlcanceEntrada entrada = {{&programa, &runtime, &ryu, &terceiros, &legado, &curto, &math, &es, &thread, &sync, &atomic, &varena, &net, &entrada_elf, &semente_elf, &semente_elf}, TESTE_RAIZES, 2, TESTE_IMPORTS, TESTE_QTD_IMPORTS};
    AlcanceCalcular(&entrada, &a);
    VERIFICA(AlcanceVivo(&a, 1, s.entrada) && AlcanceVivo(&a, 1, s.a) && AlcanceVivo(&a, 1, s.b));
    VERIFICA(AlcanceVivo(&a, 1, s.rd_x) && AlcanceVivo(&a, 1, s.pd_a) && AlcanceVivo(&a, 1, s.xd_a));
    VERIFICA(AlcanceVivo(&a, 1, s.pd_b) && AlcanceVivo(&a, 1, s.xd_b));
    VERIFICA(!AlcanceVivo(&a, 1, s.morta) && !AlcanceVivo(&a, 1, s.rd_y));
    VERIFICA(!AlcanceVivo(&a, 1, s.pd_morta) && !AlcanceVivo(&a, 1, s.xd_morta));
    VERIFICA(AlcanceImportUsado(&a, 0) && !AlcanceImportUsado(&a, 1) && !AlcanceImportUsado(&a, 2));
    VERIFICA(a.indefinido == NULL && a.import_fora == NULL);
    AlcanceLiberar(&a);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
    ObjLiberar(&ryu);
    ObjLiberar(&terceiros);
    ObjLiberar(&legado);
    ObjLiberar(&curto);
    ObjLiberar(&math);
    ObjLiberar(&es);
    ObjLiberar(&thread);
    ObjLiberar(&sync);
    ObjLiberar(&atomic);
    ObjLiberar(&varena);
    ObjLiberar(&net);
    ObjLiberar(&entrada_elf);
    ObjLiberar(&semente_elf);
}

/* O programa e raiz inteiro: a funcao dele que chama rt_morta acorda a rt_morta (e o que ela cita, .pdata incluso); o erro de simbolo
 * indefinido e de __imp_ fora da tabela aparece so agora, com a secao viva. */
static void TestaAlcancePeloPrograma(void)
{
    Objeto programa, runtime;
    Alcance a;
    Objeto ryu, terceiros, legado, curto, math, es, thread, sync, atomic, varena, net, entrada_elf, semente_elf;
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ObjIniciar(&ryu);
    ObjIniciar(&terceiros);
    ObjIniciar(&legado);
    ObjIniciar(&curto);
    ObjIniciar(&math);
    ObjIniciar(&es);
    ObjIniciar(&thread);
    ObjIniciar(&sync);
    ObjIniciar(&atomic);
    ObjIniciar(&varena);
    ObjIniciar(&net);
    ObjIniciar(&entrada_elf);
    ObjIniciar(&semente_elf);
    SecoesAlcance s = MontarRuntimeAlcance(&runtime);
    uint32_t chamador = FuncaoDeTeste(&programa, ".text", "forja_programa");
    Citar(&programa, chamador, 1, "rt_morta");
    AlcanceEntrada entrada = {{&programa, &runtime, &ryu, &terceiros, &legado, &curto, &math, &es, &thread, &sync, &atomic, &varena, &net, &entrada_elf, &semente_elf, &semente_elf}, TESTE_RAIZES, 2, TESTE_IMPORTS, TESTE_QTD_IMPORTS};
    AlcanceCalcular(&entrada, &a);
    VERIFICA(AlcanceVivo(&a, 1, s.morta) && AlcanceVivo(&a, 1, s.rd_y) && AlcanceVivo(&a, 1, s.pd_morta) && AlcanceVivo(&a, 1, s.xd_morta));
    VERIFICA(AlcanceImportUsado(&a, 0) && AlcanceImportUsado(&a, 1) && !AlcanceImportUsado(&a, 2));
    VERIFICA(a.import_fora != NULL && strcmp(a.import_fora, "__imp_Desconhecido") == 0);
    VERIFICA(a.indefinido != NULL && strcmp(a.indefinido, "ninguem_define") == 0);
    AlcanceLiberar(&a);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
    ObjLiberar(&ryu);
    ObjLiberar(&terceiros);
    ObjLiberar(&legado);
    ObjLiberar(&curto);
    ObjLiberar(&math);
    ObjLiberar(&es);
    ObjLiberar(&thread);
    ObjLiberar(&sync);
    ObjLiberar(&atomic);
    ObjLiberar(&varena);
    ObjLiberar(&net);
    ObjLiberar(&entrada_elf);
    ObjLiberar(&semente_elf);
}

static const PeSecao *SecaoDaImagem(const Imagem *im, const char *nome)
{
    for (uint32_t k = 0; k < im->qtd_secoes; k++) {
        if (strncmp(im->secoes[k].nome, nome, sizeof im->secoes[k].nome) == 0)
            return &im->secoes[k];
    }
    return NULL;
}

/* Ligar de ponta a ponta: so ExitProcess no .idata, so rt_a/rt_b e o dado .x no exe, e o .pdata sem entrada da funcao descartada. */
static void TestaLigarSoOAlcancavel(void)
{
    Objeto programa, runtime;
    Imagem im;
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    MontarRuntimeAlcance(&runtime);
    VERIFICA(Ligar(&programa, &runtime, &im, &erro));
    VERIFICA(im.importacao.qtd == 1 && strcmp(im.importacao.nomes[0], "ExitProcess") == 0);
    VERIFICA(im.importacao.qtd_dlls == 1 && strcmp(im.importacao.dlls[0].nome, "KERNEL32.dll") == 0 && im.importacao.dlls[0].qtd == 1);
    VERIFICA(SecaoDaImagem(&im, ".text") != NULL && SecaoDaImagem(&im, ".text")->tam_virtual == 3 * TESTE_TAM_FUNCAO + 6);
    VERIFICA(SecaoDaImagem(&im, ".rdata") != NULL && SecaoDaImagem(&im, ".rdata")->tam_virtual == 8);
    VERIFICA(SecaoDaImagem(&im, ".pdata") != NULL && SecaoDaImagem(&im, ".pdata")->bytes.tam == 2 * 12);
    VERIFICA(SecaoDaImagem(&im, ".xdata") != NULL && SecaoDaImagem(&im, ".xdata")->bytes.tam == 2 * 4);
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
}

/* Simbolo citado por secao viva e indefinido: X0903 (antes era erro por qualquer simbolo da tabela; agora so o que o exe usaria). */
static void TestaLigarSimboloVivoIndefinido(void)
{
    Objeto programa, runtime;
    Imagem im;
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    uint32_t entrada = FuncaoDeTeste(&runtime, ".text", "forja_entrada");
    Citar(&runtime, entrada, 1, "ninguem_define");
    VERIFICA(!Ligar(&programa, &runtime, &im, &erro) && erro.tem && erro.codigo == X0903 && strstr(erro.causa, "ninguem_define") != NULL);
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
}

static const ImagemSimbolo *SimboloDaImagem(const Imagem *im, const char *nome)
{
    for (size_t k = 0; k < im->simbolos.n; k++) {
        const ImagemSimbolo *s = VecEm(&im->simbolos, k);
        if (strcmp(s->nome, nome) == 0)
            return s;
    }
    return NULL;
}

/* Programa sintetico do alvo linux: forja_programa com o .pdata/.xdata do Win64 e um .data com um ADDR64 para ela; o runtime tem _start (a entrada), rt_a, o dado .x e rt_morta, cada um com unwind. */
static void MontarParLinux(Objeto *programa, Objeto *runtime)
{
    uint32_t dir[2];
    uint32_t dados = SecaoComBytes(programa, ".data", OS_DATA, 8);
    uint32_t inicio = FuncaoDeTeste(runtime, ".text", "_start");
    uint32_t a = FuncaoDeTeste(runtime, ".text$a", "rt_a");

    FuncaoDeTeste(programa, ".text", "forja_programa");
    DiretorioDeTeste(programa, "p", "forja_programa", dir);
    ObjRelocar(programa, (ObjAlvoReloc){dados, 0}, (ObjCorrecao){OR_ADDR64, ObjSimboloReferir(programa, "forja_programa"), 0});
    DadoDeTeste(runtime, "x");
    FuncaoDeTeste(runtime, ".text$morta", "rt_morta");
    Citar(runtime, inicio, 1, "rt_a");
    Citar(runtime, inicio, 5, "forja_programa");
    Citar(runtime, a, 1, ".x");
    DiretorioDeTeste(runtime, "a", "rt_a", dir);
    DiretorioDeTeste(runtime, "morta", "rt_morta", dir);
}

/* M5 P27a: a mesma ligacao em IMAGEM_ELF: base 0x400000, .rdata antes do .text, sem .pdata/.xdata/.idata (nem as ADDR32NB deles), sem DIR64, entrada = _start e os simbolos vivos na Imagem. */
static void TestaLigarLinux(void)
{
    Objeto programa, runtime;
    Imagem im;
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    MontarParLinux(&programa, &runtime);
    VERIFICA(LigarComTabelas(&(LigarPedido){.programa = &programa, .runtime = &runtime, .formato = IMAGEM_ELF}, &im, &erro));
    VERIFICA(im.formato == IMAGEM_ELF && PeBaseDaImagem(&im) == IMAGEM_BASE_ELF && im.dir64.n == 0 && im.importacao.qtd == 0 && im.importacao.qtd_dlls == 0);
    VERIFICA(SecaoDaImagem(&im, ".pdata") == NULL && SecaoDaImagem(&im, ".xdata") == NULL && SecaoDaImagem(&im, ".idata") == NULL && SecaoDaImagem(&im, ".reloc") == NULL);
    VERIFICA(SecaoDaImagem(&im, ".rdata") != NULL && SecaoDaImagem(&im, ".text") != NULL && SecaoDaImagem(&im, ".rdata") < SecaoDaImagem(&im, ".text"));
    const ImagemSimbolo *inicio = SimboloDaImagem(&im, "_start");
    VERIFICA(inicio != NULL && inicio->rva == im.entrada_rva && im.entrada_rva >= SecaoDaImagem(&im, ".text")->rva);
    VERIFICA(SimboloDaImagem(&im, "forja_programa") != NULL && SimboloDaImagem(&im, "rt_a") != NULL && SimboloDaImagem(&im, "rt_morta") == NULL);
    VERIFICA(SimboloDaImagem(&im, ".unwind_p") == NULL && SimboloDaImagem(&im, ".x") == NULL);
    /* o ADDR64 do .data leva a base do ELF mais o RVA de forja_programa */
    const PeSecao *dados = SecaoDaImagem(&im, ".data");
    VERIFICA(dados != NULL && LeBytes((const uint8_t *)dados->bytes.dados, 8) == IMAGEM_BASE_ELF + SimboloDaImagem(&im, "forja_programa")->rva);
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
}

/* O ELF estatico nao liga DLL nem tabelas de imports: o ligador recusa com X0903 em vez de montar uma imagem sem sentido. */
static void TestaLigarLinuxRecusas(void)
{
    static const char *const SIMBOLOS[] = {"Sleep"};
    static const ImportTabela TABELA[] = {{"KERNEL32.dll", SIMBOLOS, 1}};
    Objeto programa, runtime;
    Imagem im;
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    MontarParLinux(&programa, &runtime);
    VERIFICA(!LigarComTabelas(&(LigarPedido){.programa = &programa, .runtime = &runtime, .formato = IMAGEM_ELF, .dll = true}, &im, &erro) && erro.codigo == X0903);
    erro = (Diag){0};
    VERIFICA(!LigarComTabelas(&(LigarPedido){.programa = &programa, .runtime = &runtime, .formato = IMAGEM_ELF, .tabelas = TABELA, .qtd_tabelas = 1}, &im, &erro) && erro.codigo == X0903);
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
}

/* ObjAcharSimbolo por hash: 100 mil simbolos, achar todos e nao achar o que nao existe, sem o custo quadratico da busca linear (que levaria dezenas de segundos). */
static void TestaAcharSimboloMuitos(void)
{
    Objeto o;
    char nome[32];
    uint32_t erros = 0;
    clock_t inicio = clock();
    ObjIniciar(&o);
    uint32_t secao = ObjSecaoAbrir(&o, ".text", OS_TEXTO, 16);
    for (uint32_t i = 0; i < TESTE_QTD_SIMBOLOS_HASH; i++) {
        snprintf(nome, sizeof nome, "sim_%u", i);
        erros += ObjSimboloReferir(&o, nome) != i;
    }
    for (uint32_t i = 0; i < TESTE_QTD_SIMBOLOS_HASH; i++) {
        snprintf(nome, sizeof nome, "sim_%u", i);
        erros += ObjAcharSimbolo(&o, nome) != (int64_t)i;
        erros += ObjSimboloReferir(&o, nome) != i;
    }
    VERIFICA(erros == 0 && o.simbolos.n == TESTE_QTD_SIMBOLOS_HASH);
    VERIFICA(ObjAcharSimbolo(&o, "sim_") == -1 && ObjAcharSimbolo(&o, "sim_100000") == -1 && ObjAcharSimbolo(&o, "") == -1);
    VERIFICA(ObjSimboloDefinir(&o, "sim_77", (int32_t)secao, 4) == 77 && Simbolo(&o, 77)->secao == (int32_t)secao && Simbolo(&o, 77)->off == 4);
    VERIFICA(strcmp(Simbolo(&o, 99999)->nome, "sim_99999") == 0);
    VERIFICA((double)(clock() - inicio) / CLOCKS_PER_SEC < TESTE_LIMITE_HASH_SEGUNDOS);
    ObjLiberar(&o);
}

static uint32_t ContaSecoes(const Objeto *o, const char *prefixo, ObjTipoSecao tipo)
{
    uint32_t n = 0;
    for (size_t k = 0; k < o->secoes.n; k++) {
        const ObjSecao *s = VecEm(&o->secoes, k);
        n += strncmp(s->nome, prefixo, strlen(prefixo)) == 0 && s->tipo == tipo;
    }
    return n;
}

/* O runtime real vem com -ffunction-sections/-fdata-sections: o leitor COFF tem de aceitar o '$' e dar o tipo certo a .pdata$f e .xdata$f. */
static void TestaCoffGrupoDeSecao(void)
{
    Objeto rt;
    Diag erro = {0};
    size_t n = 0;
    const uint8_t *bytes = RtNativoWin(BM_DEV, &n);
    ObjIniciar(&rt);
    VERIFICA(CoffLer(bytes, n, &rt, &erro));
    VERIFICA(ContaSecoes(&rt, ".text$", OS_TEXTO) > 50 && ContaSecoes(&rt, ".data$", OS_DATA) > 0 && ContaSecoes(&rt, ".rdata$", OS_RDATA) > 0);
    VERIFICA(ContaSecoes(&rt, ".pdata$", OS_PDATA) == ContaSecoes(&rt, ".text$", OS_TEXTO));
    VERIFICA(ContaSecoes(&rt, ".xdata$", OS_XDATA) == ContaSecoes(&rt, ".text$", OS_TEXTO));
    ObjLiberar(&rt);
}

/* O objeto Linux (M5 P26d) e COFF que o CoffLer aceita, sem .pdata/.xdata (D-M5-7) e sem __imp_; devolve o Objeto lido em *o (quem chama libera). */
static bool RtLinuxLeSemWindows(const uint8_t *bytes, size_t n, Objeto *o)
{
    Diag erro = {0};
    bool ok;

    ObjIniciar(o);
    ok = CoffLer(bytes, n, o, &erro) && ContaSecoes(o, ".pdata", OS_PDATA) == 0 && ContaSecoes(o, ".xdata", OS_XDATA) == 0;
    for (size_t k = 0; ok && k < o->simbolos.n; k++)
        ok = strncmp(((const ObjSimbolo *)o->simbolos.dados)[k].nome, "__imp_", 6) != 0;
    return ok;
}

static void TestaRtNativoLinux(void)
{
    size_t n_dev = 0, n_rel = 0, n_comum = 0;
    const uint8_t *dev = RtNativoLinux(BM_DEV, &n_dev);
    const uint8_t *rel = RtNativoLinux(BM_RELEASE, &n_rel);
    const uint8_t *comum = RtNativoLinuxComum(&n_comum);
    Objeto od, orl, oc;

    VERIFICA(RtLinuxLeSemWindows(dev, n_dev, &od) && RtLinuxLeSemWindows(rel, n_rel, &orl) && RtLinuxLeSemWindows(comum, n_comum, &oc));
    /* M5b P39 (D-R3): o _start saiu das variantes (rt_linux_entrada_cru.o); o forja_entrada fica nelas; o heap e o Ryu, no comum. */
    VERIFICA(ObjAcharSimbolo(&od, "_start") < 0 && ObjAcharSimbolo(&orl, "_start") < 0 && ObjAcharSimbolo(&oc, "sys_heap_alocar") >= 0 && ObjAcharSimbolo(&oc, "rt_fmt_fixo") >= 0);
    ObjLiberar(&od);
    ObjLiberar(&orl);
    ObjLiberar(&oc);
}

static void TestaAlcance(void)
{
    TestaRtNativoLinux();
    TestaAlcanceDescarta();
    TestaAlcancePeloPrograma();
    TestaLigarSoOAlcancavel();
    TestaLigarSimboloVivoIndefinido();
    TestaLigarLinux();
    TestaLigarLinuxRecusas();
    TestaAcharSimboloMuitos();
    TestaCoffGrupoDeSecao();
}

/* --debug-code: a lista vira mascara; item desconhecido, 'ir' e item vazio recusam com a causa. */
static void TestaDebugItens(void)
{
    uint32_t mascara = 0xFFu;
    char causa[200];

    VERIFICA(DebugItens("", &mascara, causa, sizeof causa) && mascara == 0);
    VERIFICA(DebugItens("bir,asm", &mascara, causa, sizeof causa) && mascara == (DEBUG_BIR | DEBUG_ASM));
    VERIFICA(DebugItens("quadros,reloc,formato,tempos", &mascara, causa, sizeof causa)
             && mascara == (DEBUG_QUADROS | DEBUG_RELOC | DEBUG_FORMATO | DEBUG_TEMPOS));
    VERIFICA(!DebugItens("ir", &mascara, causa, sizeof causa) && strstr(causa, "'ir'") != NULL);
    VERIFICA(!DebugItens("asm,xyz", &mascara, causa, sizeof causa) && strstr(causa, "'xyz'") != NULL);
    VERIFICA(!DebugItens("asm,,bir", &mascara, causa, sizeof causa));
}

/* Cada itens[k] tem de ter o indice k na numeracao (as instrucoes do teste ficam guardadas na ordem esperada da pre-ordem). */
static bool NumeracaoConfere(const IrNumeracao *num, const IrInstr *itens, uint32_t n)
{
    if (num->n != n)
        return false;
    for (uint32_t k = 0; k < n; k++) {
        if (IrIndiceDe(num, &itens[k]) != k)
            return false;
    }
    return true;
}

/* IrNumerarFuncao (P18a): pre-ordem fixa, a instrucao, depois cond, corpo e senao (if com else) e os casos na ordem (switch). */
static void TestaNumeracaoIr(void)
{
    Arena arena = {0};
    IrNumeracao num;
    IrFuncao f = {0};
    IrInstr estranha = {.op = IR_LOCAL};
    /* if: n[0] local, n[1] if (cond n[2]; corpo n[3] n[4]; senao n[5]), n[6] return */
    IrInstr se[7] = {{.op = IR_LOCAL}, {.op = IR_SE}, {.op = IR_BINARIO}, {.op = IR_LOCAL}, {.op = IR_LOCAL}, {.op = IR_LOCAL}, {.op = IR_RETORNA}};
    /* switch: n[0] chaveia (caso 0: n[1] n[2]; caso 1: n[3]), n[4] return */
    IrInstr sw[5] = {{.op = IR_CHAVEIA}, {.op = IR_LOCAL}, {.op = IR_LOCAL}, {.op = IR_LOCAL}, {.op = IR_RETORNA}};
    IrCaso casos[2] = {0};

    se[0].prox = &se[1];
    se[1].prox = &se[6];
    se[1].cond = (IrBloco){.primeira = &se[2], .ultima = &se[2]};
    se[3].prox = &se[4];
    se[1].corpo = (IrBloco){.primeira = &se[3], .ultima = &se[4]};
    se[1].senao = (IrBloco){.primeira = &se[5], .ultima = &se[5]};
    f.corpo = (IrBloco){.primeira = &se[0], .ultima = &se[6]};
    IrNumerarFuncao(&f, &arena, &num);
    VERIFICA(NumeracaoConfere(&num, se, 7));
    sw[0].prox = &sw[4];
    sw[1].prox = &sw[2];
    casos[0].corpo = (IrBloco){.primeira = &sw[1], .ultima = &sw[2]};
    casos[1].corpo = (IrBloco){.primeira = &sw[3], .ultima = &sw[3]};
    sw[0].casos = casos;
    sw[0].ncasos = 2;
    f.corpo = (IrBloco){.primeira = &sw[0], .ultima = &sw[4]};
    IrNumerarFuncao(&f, &arena, &num);
    VERIFICA(NumeracaoConfere(&num, sw, 5) && IrIndiceDe(&num, &estranha) == IR_SEM_INDICE);
    ArenaLiberar(&arena);
}

/* ---------- M3 P19a: leitor de PE de terceiro (DLLs reais do Windows) e busca de biblioteca ---------- */

#define TESTE_TAM_CAMINHO 600

static bool LerArquivo(const char *caminho, Buf *conteudo)
{
    char bloco[4096];
    size_t n;
    FILE *f = fopen(caminho, "rb");
    if (f == NULL)
        return false;
    while ((n = fread(bloco, 1, sizeof bloco, f)) > 0)
        BufAnexarN(conteudo, bloco, n);
    fclose(f);
    return conteudo->tam > 0;
}

static void CaminhoDoSistema(char *dest, size_t tam, const char *relativo)
{
    const char *raiz = getenv("SystemRoot");
    snprintf(dest, tam, "%s\\%s", raiz != NULL ? raiz : "C:\\Windows", relativo);
}

/* Le as exportacoes de uma DLL do Windows (relativa a %SystemRoot%). exp fica pronta para PeExportacoesLiberar mesmo se devolver false. */
static bool ExportacoesDoSistema(const char *relativo, PeExportacoes *exp, Diag *erro)
{
    char caminho[TESTE_TAM_CAMINHO];
    Buf bytes = {0};
    *exp = (PeExportacoes){0};
    CaminhoDoSistema(caminho, sizeof caminho, relativo);
    bool ok = LerArquivo(caminho, &bytes) && PeLerExportacoes((const uint8_t *)bytes.dados, bytes.tam, exp, erro);
    BufLiberar(&bytes);
    return ok;
}

static const PeExportacao *AcharExportacao(const PeExportacoes *e, const char *nome)
{
    for (size_t i = 0; i < e->itens.n; i++) {
        const PeExportacao *x = VecEm(&e->itens, i);
        if (strcmp(x->nome, nome) == 0)
            return x;
    }
    return NULL;
}

/* A DLL exporta o simbolo pelo nome, com ordinal e RVA, e sem repasse a outra DLL. */
static bool ExportaSemRepasse(const char *dll, const char *simbolo)
{
    PeExportacoes e;
    Diag erro = {0};
    bool lido = ExportacoesDoSistema(dll, &e, &erro);
    const PeExportacao *x = lido ? AcharExportacao(&e, simbolo) : NULL;
    bool ok = x != NULL && x->forwarder == NULL && x->ordinal > 0 && x->rva > 0;
    PeExportacoesLiberar(&e);
    return ok;
}

/* A DLL repassa o simbolo a outra: o forwarder e o texto "dll.simbolo" esperado. */
static bool ExportaComRepasse(const char *dll, const char *simbolo, const char *destino)
{
    PeExportacoes e;
    Diag erro = {0};
    bool lido = ExportacoesDoSistema(dll, &e, &erro);
    const PeExportacao *x = lido ? AcharExportacao(&e, simbolo) : NULL;
    bool ok = x != NULL && x->forwarder != NULL && strcmp(x->forwarder, destino) == 0;
    PeExportacoesLiberar(&e);
    return ok;
}

/* DLL de 32 bits: X0105 "biblioteca de 32 bits". */
static bool RecusaPe32(void)
{
    PeExportacoes e;
    Diag erro = {0};
    bool lido = ExportacoesDoSistema("SysWOW64\\kernel32.dll", &e, &erro);
    PeExportacoesLiberar(&e);
    return !lido && erro.tem && erro.codigo == X0105 && strstr(erro.causa, "32 bits") != NULL;
}

static void CriarArquivoDeTeste(const char *pasta, const char *nome, const Buf *dados)
{
    char caminho[TESTE_TAM_CAMINHO];
    snprintf(caminho, sizeof caminho, "%s\\%s", pasta, nome);
    FILE *f = fopen(caminho, "wb");
    if (f == NULL)
        return;
    fwrite(dados->dados, 1, dados->tam, f);
    fclose(f);
}

static void ApagarArquivoDeTeste(const char *pasta, const char *nome)
{
    char caminho[TESTE_TAM_CAMINHO];
    snprintf(caminho, sizeof caminho, "%s\\%s", pasta, nome);
    remove(caminho);
}

static bool TerminaCom(const char *texto, const char *fim)
{
    size_t n = strlen(texto), m = strlen(fim);
    return n >= m && strcmp(texto + n - m, fim) == 0;
}

/* Na mesma pasta, <x>.dll e lib<x>.a: o ld pega lib<x>.a (a .dll vem por ultimo). */
static bool OrdemDoLdComDoisCandidatos(const char *pasta)
{
    BibliotecaAchada achada;
    const char *pastas[] = {pasta};
    BibliotecaBusca busca = {pastas, 1, false, NULL};
    Buf ar = {0};
    BufAnexar(&ar, "!<arch>\n");
    CriarArquivoDeTeste(pasta, "forja_p19a_o.dll", &ar);
    CriarArquivoDeTeste(pasta, "libforja_p19a_o.a", &ar);
    bool ok = BibliotecaAchar(&busca, "forja_p19a_o", &achada) && TerminaCom(achada.caminho, "libforja_p19a_o.a") && achada.tipo == BIB_ESTATICA_AR &&
              achada.procurei_em.tam > 0 && strstr(achada.procurei_em.dados, "forja_p19a_o.dll\n") == NULL;
    BibliotecaLiberar(&achada);
    ApagarArquivoDeTeste(pasta, "forja_p19a_o.dll");
    ApagarArquivoDeTeste(pasta, "libforja_p19a_o.a");
    BufLiberar(&ar);
    return ok;
}

/* "m" e "c" valem a ucrtbase.dll do System32, que e DLL pelo conteudo. */
static bool LinkMeCViramUcrtbase(void)
{
    BibliotecaAchada m, c;
    BibliotecaBusca busca = {NULL, 0, false, NULL};
    bool ok = BibliotecaAchar(&busca, "m", &m) && BibliotecaAchar(&busca, "c", &c) && TerminaCom(m.caminho, "System32\\ucrtbase.dll") &&
              strcmp(m.caminho, c.caminho) == 0 && m.tipo == BIB_DLL;
    BibliotecaLiberar(&m);
    BibliotecaLiberar(&c);
    return ok;
}

/* Tipo pelo conteudo, com a extensao trocada: a DLL em lib<x>.a e DLL; o ar em <x>.dll e ESTATICA_AR; o exe em lib<x>.dll.a e EXE. */
static bool TipoPeloConteudo(const char *pasta)
{
    char caminho[TESTE_TAM_CAMINHO];
    Buf dll = {0}, exe = {0}, ar = {0};
    BibliotecaAchada achada;
    const char *pastas[] = {pasta};
    BibliotecaBusca busca = {pastas, 1, false, NULL};
    CaminhoDoSistema(caminho, sizeof caminho, "System32\\kernel32.dll");
    bool ok = LerArquivo(caminho, &dll);
    CaminhoDoSistema(caminho, sizeof caminho, "System32\\cmd.exe");
    ok = LerArquivo(caminho, &exe) && ok;
    BufAnexar(&ar, "!<arch>\n");
    CriarArquivoDeTeste(pasta, "libforja_p19a_c.a", &dll);
    CriarArquivoDeTeste(pasta, "forja_p19a_d.dll", &ar);
    CriarArquivoDeTeste(pasta, "libforja_p19a_e.dll.a", &exe);
    ok = ok && BibliotecaAchar(&busca, "forja_p19a_c", &achada) && achada.tipo == BIB_DLL;
    BibliotecaLiberar(&achada);
    ok = ok && BibliotecaAchar(&busca, "forja_p19a_d", &achada) && achada.tipo == BIB_ESTATICA_AR;
    BibliotecaLiberar(&achada);
    ok = ok && BibliotecaAchar(&busca, "forja_p19a_e", &achada) && achada.tipo == BIB_EXE;
    BibliotecaLiberar(&achada);
    ApagarArquivoDeTeste(pasta, "libforja_p19a_c.a");
    ApagarArquivoDeTeste(pasta, "forja_p19a_d.dll");
    ApagarArquivoDeTeste(pasta, "libforja_p19a_e.dll.a");
    BufLiberar(&dll);
    BufLiberar(&exe);
    BufLiberar(&ar);
    return ok;
}

/* P19a: PeLerExportacoes e BibliotecaAchar (6 verificacoes). */
static void TestaPeLerEBiblioteca(void)
{
    const char *pasta_temporaria = getenv("TEMP");
    const char *pasta = pasta_temporaria != NULL ? pasta_temporaria : ".";
    VERIFICA(ExportaSemRepasse("System32\\kernel32.dll", "GetCurrentProcessId") && ExportaSemRepasse("System32\\ucrtbase.dll", "strlen"));
    VERIFICA(ExportaComRepasse("System32\\kernel32.dll", "AcquireSRWLockExclusive", "NTDLL.RtlAcquireSRWLockExclusive"));
    VERIFICA(RecusaPe32());
    VERIFICA(OrdemDoLdComDoisCandidatos(pasta));
    VERIFICA(LinkMeCViramUcrtbase());
    VERIFICA(TipoPeloConteudo(pasta));
}

/* ---------- P19: os 'link' viram tabelas de imports por DLL (4 verificacoes) ---------- */

/* Resolve os simbolos contra os 'link' sem pasta nenhuma na busca: so o System32. */
static bool ResolverNoSistema(const char *const *links, uint32_t qtd_links, const char *const *simbolos, BibliotecasResolvidas *saida)
{
    BibliotecaBusca busca = {NULL, 0, false, NULL};
    BibliotecasPedido pedido = {.busca = &busca, .links = links, .qtd_links = qtd_links, .simbolos = simbolos, .qtd_simbolos = 1};
    Diag erro = {0};

    return BibliotecasResolver(&pedido, saida, &erro);
}

/* O simbolo que duas DLLs do 'link' exportam fica na ULTIMA, nas duas ordens (como o ld: medido com gcc -lfx_basica -lfx_lixo e a ordem trocada). */
static bool UltimaBibliotecaVence(void)
{
    static const char *const k32_depois_kb[] = {"kernel32", "kernelbase"};
    static const char *const kb_depois_k32[] = {"kernelbase", "kernel32"};
    static const char *const simbolo[] = {"GetCurrentProcessId"};
    BibliotecasResolvidas a = {0}, b = {0};
    bool ok = ResolverNoSistema(k32_depois_kb, 2, simbolo, &a) && ResolverNoSistema(kb_depois_k32, 2, simbolo, &b);

    ok = ok && a.qtd_tabelas == 1 && strcmp(a.tabelas[0].dll, "kernelbase.dll") == 0 && b.qtd_tabelas == 1 && strcmp(b.tabelas[0].dll, "kernel32.dll") == 0;
    BibliotecasLiberar(&a);
    BibliotecasLiberar(&b);
    return ok;
}

/* Forwarder (a kernel32 repassa AcquireSRWLockExclusive a NTDLL): entra pelo nome na tabela da DLL que o exporta. */
static bool ForwarderEntraPeloNome(void)
{
    static const char *const links[] = {"kernel32"};
    static const char *const simbolo[] = {"AcquireSRWLockExclusive"};
    BibliotecasResolvidas r = {0};
    bool ok = ResolverNoSistema(links, 1, simbolo, &r) && r.qtd_tabelas == 1 && strcmp(r.tabelas[0].dll, "kernel32.dll") == 0 && r.tabelas[0].qtd == 1 &&
              strcmp(r.tabelas[0].simbolos[0], "AcquireSRWLockExclusive") == 0;

    BibliotecasLiberar(&r);
    return ok;
}

/* O programa cita __imp_<simbolo>: o ligador acha o slot na tabela da DLL do 'link', depois da KERNEL32 do runtime (ExitProcess). */
static bool LigarCitaImpDeUmaDll(void)
{
    static const char *const nomes[] = {"NomeForaDaTabelaFixa"};
    const ImportTabela tabelas[] = {{"kernel32.dll", nomes, 1}};
    Objeto programa, runtime;
    Imagem im;
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    MontarRuntimeAlcance(&runtime);
    Citar(&programa, FuncaoDeTeste(&programa, ".text", "fj_usa"), 1, "__imp_NomeForaDaTabelaFixa");
    bool ok = LigarComTabelas(&(LigarPedido){.programa = &programa, .runtime = &runtime, .tabelas = tabelas, .qtd_tabelas = 1}, &im, &erro) && im.importacao.qtd == 2 && im.importacao.qtd_dlls == 2 &&
              strcmp(im.importacao.dlls[1].nome, "kernel32.dll") == 0 && strcmp(im.importacao.nomes[1], "NomeForaDaTabelaFixa") == 0;
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
    return ok;
}

/* Descritores: KERNEL32.dll (runtime) primeiro e depois as DLLs na ordem das tabelas (a dos 'link'), nao a das referencias. */
static bool DescritoresNaOrdemDosLink(void)
{
    static const char *const em_b[] = {"fb"};
    static const char *const em_a[] = {"fa"};
    const ImportTabela tabelas[] = {{"b.dll", em_b, 1}, {"a.dll", em_a, 1}};
    Objeto programa, runtime;
    Imagem im;
    Diag erro = {0};
    ObjIniciar(&programa);
    ObjIniciar(&runtime);
    ImagemIniciar(&im);
    MontarRuntimeAlcance(&runtime);
    uint32_t usa = FuncaoDeTeste(&programa, ".text", "fj_usa");
    Citar(&programa, usa, 1, "__imp_fa");
    Citar(&programa, usa, 5, "__imp_fb");
    bool ok = LigarComTabelas(&(LigarPedido){.programa = &programa, .runtime = &runtime, .tabelas = tabelas, .qtd_tabelas = 2}, &im, &erro) && im.importacao.qtd_dlls == 3 &&
              strcmp(im.importacao.dlls[0].nome, "KERNEL32.dll") == 0 && strcmp(im.importacao.dlls[1].nome, "b.dll") == 0 && strcmp(im.importacao.dlls[2].nome, "a.dll") == 0;
    ImagemLiberar(&im);
    ObjLiberar(&programa);
    ObjLiberar(&runtime);
    return ok;
}

/* P19c: "msvcrt" e "ucrt" valem a ucrtbase.dll do System32 (a UCRT que o gcc do oraculo liga), nao a msvcrt.dll real. */
static bool MsvcrtEUcrtViramUcrtbase(void)
{
    BibliotecaAchada m, u;
    BibliotecaBusca busca = {NULL, 0, false, NULL};
    bool ok = BibliotecaAchar(&busca, "msvcrt", &m) && BibliotecaAchar(&busca, "ucrt", &u) && TerminaCom(m.caminho, "System32\\ucrtbase.dll") &&
              strcmp(m.caminho, u.caminho) == 0 && m.tipo == BIB_DLL;

    BibliotecaLiberar(&m);
    BibliotecaLiberar(&u);
    return ok;
}

/* P19c: o fflush (import da variante CRT do runtime) de um programa que liga "msvcrt" cai na tabela ucrtbase.dll. */
static bool FflushDeMsvcrtNaTabelaUcrtbase(void)
{
    static const char *const links[] = {"msvcrt"};
    static const char *const simbolo[] = {"fflush"};
    BibliotecasResolvidas r = {0};
    bool ok = ResolverNoSistema(links, 1, simbolo, &r) && r.qtd_tabelas == 1 && strcmp(r.tabelas[0].dll, "ucrtbase.dll") == 0 &&
              strcmp(r.tabelas[0].simbolos[0], "fflush") == 0;

    BibliotecasLiberar(&r);
    return ok;
}

static void TestaBibliotecasDoLink(void)
{
    VERIFICA(MsvcrtEUcrtViramUcrtbase());
    VERIFICA(FflushDeMsvcrtNaTabelaUcrtbase());
    VERIFICA(UltimaBibliotecaVence());
    VERIFICA(ForwarderEntraPeloNome());
    VERIFICA(LigarCitaImpDeUmaDll());
    VERIFICA(DescritoresNaOrdemDosLink());
}

static int RodarUnidade(void)
{
    TestaObjeto();
    TestaSecoesImagem();
    TestaImportacoes();
    TestaPe();
    TestaReloc();
    TestaExportacoes();
    TestaPeLerEBiblioteca();
    TestaBibliotecasDoLink();
    ImplibVerificacoes(&g_total, &g_falhas);
    EstaticaVerificacoes(&g_total, &g_falhas);
    TerceirosVerificacoes(&g_total, &g_falhas);
    ElfVerificacoes(&g_total, &g_falhas);
    LigarElfVerificacoes(&g_total, &g_falhas);
    ElfMembrosVerificacoes(&g_total, &g_falhas);
    TestaCodificarBasico();
    TestaCodificarRipEFlags();
    TestaRecusas();
    TestaFormatar();
    TestaFormatarCondicoes();
    TestaLigacao();
    TestaAlcance();
    TestaDebugItens();
    TestaNumeracaoIr();
    BirVerificacoes(&g_total, &g_falhas);
    AlcanceVerificacoes(&g_total, &g_falhas);
    AnalisesVerificacoes(&g_total, &g_falhas);
    VetorVerificacoes(&g_total, &g_falhas);
    if (g_falhas != 0) {
        fprintf(stderr, "FALHOU: %d de %d verificacoes\n", g_falhas, g_total);
        return 1;
    }
    printf("OK: %d verificacoes\n", g_total);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1)
        return RodarUnidade();
    if (argc == 3 && strcmp(argv[1], "pe") == 0)
        return ModoPe(argv[2], false, false);
    if (argc == 3 && strcmp(argv[1], "pe2") == 0)
        return ModoPe(argv[2], false, true);
    if (argc == 3 && strcmp(argv[1], "corrompido") == 0)
        return ModoPe(argv[2], true, false);
    if (argc == 3 && strcmp(argv[1], "dll64") == 0)
        return ModoDll64(argv[2]);
    if (argc == 3 && strcmp(argv[1], "enc") == 0)
        return ModoEnc(argv[2]);
    if (argc == 3 && strcmp(argv[1], "obj") == 0)
        return ModoObj(argv[2]);
    if ((argc == 3 || (argc == 4 && strcmp(argv[3], "--release") == 0)) && strcmp(argv[1], "bir") == 0)
        return ModoBir(argv[2], argc == 4);
    if (argc == 3 && strcmp(argv[1], "bir-tempo") == 0)
        return ModoBirTempo(argv[2]);
    if (argc >= 4 && strcmp(argv[1], "estatica") == 0)
        return EstaticaModo(argc, argv);
    if ((argc == 4 || (argc == 5 && strcmp(argv[4], "--release") == 0)) && strcmp(argv[1], "x64") == 0)
        return ModoX64(argv[2], argv[3], argc == 5);
    if ((argc == 3 || (argc == 4 && strcmp(argv[3], "--release") == 0)) && strcmp(argv[1], "x64-linux") == 0)
        return ModoX64Linux(argv[2], argc == 4);
    fprintf(stderr, "uso: teste_nativo.exe  |  teste_nativo.exe pe <saida.exe>  |  teste_nativo.exe pe2 <saida.exe>  |  teste_nativo.exe corrompido <saida.exe>  |  teste_nativo.exe dll64 <saida.dll>  |  teste_nativo.exe enc <pasta>  |  teste_nativo.exe obj <pasta>  |  teste_nativo.exe bir <arq.fj> [--release]  |  teste_nativo.exe bir-tempo <arq.fj>  |  teste_nativo.exe estatica <lib.a>... -- <simbolo>...  |  teste_nativo.exe x64 <arq.fj> <saida.exe> [--release]  |  teste_nativo.exe x64-linux <arq.fj> [--release]\n");
    return 2;
}
