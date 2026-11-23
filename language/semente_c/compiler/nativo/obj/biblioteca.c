/* biblioteca.c - achar a biblioteca de um 'link' do foreign e dizer o tipo dela pelo conteudo (P19a do M3).
 * Versao: 0.5.0
 * Changelog: 0.5.0 - M5b P39-B2 (D0654): BibAcharSemente: no alvo ELF com semente, "c", "m", "msvcrt" e "ucrt" acham <semente>\libc.a (o musl, a mesma libc do crt1.o).
 *            0.4.0 - M5b P36: alvo ELF (BibliotecaBusca.elf): busca em <pasta>\vendor\linux (lib<x>.a, depois lib<x>.so); ELF ET_DYN = BIB_ELF_DINAMICO; sem System32.
 *            0.3.1 - M3 F9b-1: decisoes extraidas de BibClasseMembro e BibTipoDoMembro.
 *            0.3.0 - M3 P19b: o tipo de um ar vem dos primeiros membros lidos do ARQUIVO com salto de corpo (BibTipoArArquivo), nao dos primeiros 64 KiB: o indice de simbolos do MSVC
 *                    (python313.lib) passa de 88 KB e escondia os membros; DLL e EXE seguem pelos 64 KiB. A ordem de busca nao mudou.
 *            0.2.0 - M3 P19c: link "msvcrt" e "ucrt" tambem valem ucrtbase.dll (como "c" e "m"): e o que o gcc UCRT do oraculo C liga, e o stdout do CRT passa a ser o mesmo do runtime.
 *            0.1.0 - M3 P19a: BibliotecaAchar. Ordem por pasta = a do ld 2.46 para -l<x> no PE, conferida com gcc -Wl,--trace sobre os 7 candidatos
 *                    presentes ao mesmo tempo (sai o 1o, depois tira-se o vencedor e repete). Tipo pelos primeiros 64 KiB (cabecalho PE; cabecalho dos
 *                    primeiros membros do ar): extensao nao entra na decisao.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nativo/obj/biblioteca.h"

#define BIB_TAM_PREFIXO 65536u
#define BIB_ASSINATURA_AR "!<arch>\n"
#define BIB_TAM_ASSINATURA_AR 8u
#define BIB_TAM_CABECALHO_MEMBRO 60u
#define BIB_OFF_MEMBRO_TAMANHO 48u
#define BIB_TAM_CAMPO_TAMANHO 10u
#define BIB_MAX_MEMBROS 8u
#define BIB_TAM_AMOSTRA_MEMBRO 1024u
#define BIB_TAM_COFF 20u
#define BIB_TAM_SECAO_COFF 40u
#define BIB_PREFIXO_IDATA ".idata$"
#define BIB_OFF_PE_CARACTERISTICAS 22u
#define BIB_CARAC_DLL 0x2000u
#define BIB_OFF_LFANEW 0x3Cu
#define BIB_TAM_CABECALHO_IMPORT 20u
#define BIB_ASSINATURA_ANONIMA 0xFFFFu
#define BIB_VERSAO_IMPORT_CURTO 0u
#define BIB_VERSAO_OBJETO_GL 1u
#define BIB_NOME_SISTEMA "ucrtbase"
#define BIB_PASTA_SISTEMA_PADRAO "C:\\Windows"

/* Pre e sufixo de cada candidato, na ordem do ld. */
typedef struct {
    const char *prefixo;
    const char *sufixo;
} BibCandidato;

static const BibCandidato BIB_CANDIDATOS[] = {
    {"lib", ".dll.a"}, {"", ".dll.a"}, {"lib", ".a"}, {"", ".lib"}, {"lib", ".lib"}, {"lib", ".dll"}, {"", ".dll"},
};

/* Alvo Linux (M5b P36): a estatica do vendor\linux vem antes; o .so e achado so para ser recusado (X0003). */
static const BibCandidato BIB_CANDIDATOS_LINUX[] = {{"lib", ".a"}, {"lib", ".so"}};
#define BIB_PASTA_LINUX "\\vendor\\linux"
#define BIB_TAM_MAGICA_ELF 4u
#define BIB_MAGICA_ELF "\x7f" "ELF"
#define BIB_ELF_OFF_TIPO 16u
#define BIB_ELF_ET_DYN 3u

/* O que o primeiro membro (ou um membro seguinte) de um ar diz sobre a biblioteca. */
typedef enum { MEMBRO_OBJETO, MEMBRO_IDATA, MEMBRO_IMPORT_CURTO, MEMBRO_OBJETO_GL } BibMembro;

static uint16_t BibLe16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t BibLe32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* ---------- tipo pelo conteudo ---------- */

static BibliotecaTipo BibTipoPe(const uint8_t *b, size_t n)
{
    if (n < BIB_OFF_LFANEW + sizeof(uint32_t))
        return BIB_DESCONHECIDO;
    size_t pe = BibLe32(b + BIB_OFF_LFANEW);
    if (pe > n || n - pe < BIB_OFF_PE_CARACTERISTICAS + sizeof(uint16_t) || memcmp(b + pe, "PE\0\0", 4) != 0)
        return BIB_DESCONHECIDO;
    return (BibLe16(b + pe + BIB_OFF_PE_CARACTERISTICAS) & BIB_CARAC_DLL) != 0 ? BIB_DLL : BIB_EXE;
}

/* Objeto COFF com alguma secao ".idata$..." (cabecalho e tabela de secoes inteiros em b[0..n)). */
static bool BibTemIdata(const uint8_t *b, size_t n)
{
    if (n < BIB_TAM_COFF)
        return false;
    size_t secoes = BIB_TAM_COFF + BibLe16(b + 16), qtd = BibLe16(b + 2);
    for (size_t i = 0; i < qtd && secoes + (i + 1) * BIB_TAM_SECAO_COFF <= n; i++) {
        if (memcmp(b + secoes + i * BIB_TAM_SECAO_COFF, BIB_PREFIXO_IDATA, sizeof BIB_PREFIXO_IDATA - 1) == 0)
            return true;
    }
    return false;
}

/* Os bytes comecam com o cabecalho de objeto anonimo (Sig1 0, Sig2 0xFFFF): import curto ou objeto /GL. */
static bool BibTemCabecalhoAnonimo(const uint8_t *b, size_t n)
{
    return n >= BIB_TAM_CABECALHO_IMPORT && BibLe16(b) == 0 && BibLe16(b + 2) == BIB_ASSINATURA_ANONIMA;
}

/* Classe de um membro de cabecalho anonimo pela Version: 0 import curto, 1 objeto /GL. */
static BibMembro BibClasseAnonima(const uint8_t *b)
{
    uint16_t versao = BibLe16(b + 4);
    if (versao == BIB_VERSAO_IMPORT_CURTO)
        return MEMBRO_IMPORT_CURTO;
    return versao == BIB_VERSAO_OBJETO_GL ? MEMBRO_OBJETO_GL : MEMBRO_OBJETO;
}

/* Classe de um membro pelos primeiros bytes: cabecalho de import curto (Sig1 0, Sig2 0xFFFF, Version 0), objeto anonimo /GL (Version 1) ou COFF. */
static BibMembro BibClasseMembro(const uint8_t *b, size_t n)
{
    if (BibTemCabecalhoAnonimo(b, n))
        return BibClasseAnonima(b);
    return BibTemIdata(b, n) ? MEMBRO_IDATA : MEMBRO_OBJETO;
}

/* Tamanho do membro (10 digitos decimais, a direita com espacos). */
static size_t BibTamanhoMembro(const uint8_t *cab)
{
    size_t tam = 0;
    for (size_t i = 0; i < BIB_TAM_CAMPO_TAMANHO && cab[BIB_OFF_MEMBRO_TAMANHO + i] >= '0' && cab[BIB_OFF_MEMBRO_TAMANHO + i] <= '9'; i++)
        tam = tam * 10 + (size_t)(cab[BIB_OFF_MEMBRO_TAMANHO + i] - '0');
    return tam;
}

/* "/" (indice de simbolos) e "//" (nomes longos) nao sao objetos; "/123" e membro comum de nome longo. */
static bool BibMembroEspecial(const uint8_t *cab)
{
    return cab[0] == '/' && (cab[1] == ' ' || (cab[1] == '/' && cab[2] == ' '));
}

/* Classe do 1o membro de objeto e se algum dos primeiros membros e import curto (MSVC poe os imports curtos logo depois do descritor). */
typedef struct {
    bool visto;
    BibMembro primeiro;
    bool tem_curto;
} BibMembros;

static void BibAnotarMembro(BibMembros *m, BibMembro classe)
{
    if (!m->visto) {
        m->visto = true;
        m->primeiro = classe;
    }
    m->tem_curto = m->tem_curto || classe == MEMBRO_IMPORT_CURTO;
}

static bool BibEhImportMsvc(const BibMembros *m)
{
    return m->primeiro == MEMBRO_IMPORT_CURTO || (m->primeiro == MEMBRO_IDATA && m->tem_curto);
}

static BibliotecaTipo BibTipoDoMembro(const BibMembros *m)
{
    if (!m->visto)
        return BIB_ESTATICA_AR;
    if (m->primeiro == MEMBRO_OBJETO_GL)
        return BIB_OBJETO_GL;
    if (BibEhImportMsvc(m))
        return BIB_IMPORT_MSVC;
    return m->primeiro == MEMBRO_IDATA ? BIB_IMPORT_GNU : BIB_ESTATICA_AR;
}

/* Os primeiros membros do ar, lidos do arquivo com salto de corpo: o indice de simbolos do MSVC passa de 64 KiB (python313.lib: 88 KB), entao o prefixo nao basta.
 * De cada membro de objeto so os primeiros BIB_TAM_AMOSTRA_MEMBRO bytes (cabecalho COFF, tabela de secoes e o cabecalho de import curto). */
static BibliotecaTipo BibTipoArArquivo(FILE *arq)
{
    BibMembros m = {0};
    int64_t pos = BIB_TAM_ASSINATURA_AR;
    uint8_t cab[BIB_TAM_CABECALHO_MEMBRO], amostra[BIB_TAM_AMOSTRA_MEMBRO];
    for (uint32_t k = 0; k < BIB_MAX_MEMBROS; k++) {
        if (_fseeki64(arq, pos, SEEK_SET) != 0 || fread(cab, 1, sizeof cab, arq) != sizeof cab)
            break;
        size_t tam = BibTamanhoMembro(cab);
        if (!BibMembroEspecial(cab))
            BibAnotarMembro(&m, BibClasseMembro(amostra, fread(amostra, 1, tam < sizeof amostra ? tam : sizeof amostra, arq)));
        pos += (int64_t)(BIB_TAM_CABECALHO_MEMBRO + tam + (tam & 1u));
    }
    return BibTipoDoMembro(&m);
}

/* ELF (M5b P36): ET_DYN (e_type 3 no offset 16) e .so, que o backend nao liga; o resto (objeto, executavel) nao e biblioteca aceita. */
static BibliotecaTipo BibTipoElf(const uint8_t *b, size_t n)
{
    if (n < BIB_ELF_OFF_TIPO + sizeof(uint16_t))
        return BIB_DESCONHECIDO;
    return BibLe16(b + BIB_ELF_OFF_TIPO) == BIB_ELF_ET_DYN ? BIB_ELF_DINAMICO : BIB_DESCONHECIDO;
}

static BibliotecaTipo BibTipoDeBytes(const uint8_t *b, size_t n)
{
    if (n >= 2 && b[0] == 'M' && b[1] == 'Z')
        return BibTipoPe(b, n);
    if (n >= BIB_TAM_MAGICA_ELF && memcmp(b, BIB_MAGICA_ELF, BIB_TAM_MAGICA_ELF) == 0)
        return BibTipoElf(b, n);
    return BIB_DESCONHECIDO;
}

static bool BibEhAr(const uint8_t *b, size_t n)
{
    return n >= BIB_TAM_ASSINATURA_AR && memcmp(b, BIB_ASSINATURA_AR, BIB_TAM_ASSINATURA_AR) == 0;
}

/* ---------- busca ---------- */

/* Tenta o candidato: registra em procurei_em; se o arquivo abre, guarda caminho e tipo (lidos os primeiros 64 KiB). */
static bool BibTentar(const char *candidato, BibliotecaAchada *achada)
{
    uint8_t prefixo[BIB_TAM_PREFIXO];
    BufAnexar(&achada->procurei_em, candidato);
    BufAnexar(&achada->procurei_em, "\n");
    FILE *arq = fopen(candidato, "rb");
    if (arq == NULL)
        return false;
    size_t n = fread(prefixo, 1, sizeof prefixo, arq);
    snprintf(achada->caminho, sizeof achada->caminho, "%s", candidato);
    achada->tipo = BibEhAr(prefixo, n) ? BibTipoArArquivo(arq) : BibTipoDeBytes(prefixo, n);
    fclose(arq);
    return true;
}

/* pasta + separador (se a pasta ainda nao termina em um) + prefixo + link + sufixo; false se nao couber. */
static bool BibMontar(char *dest, const char *pasta, const char *link, const BibCandidato *c)
{
    size_t tam = strlen(pasta);
    const char *sep = tam > 0 && pasta[tam - 1] != '\\' && pasta[tam - 1] != '/' ? "\\" : "";
    int escritos = snprintf(dest, BIBLIOTECA_TAM_CAMINHO, "%s%s%s%s%s", pasta, sep, c->prefixo, link, c->sufixo);
    return escritos > 0 && escritos < BIBLIOTECA_TAM_CAMINHO;
}

static bool BibAcharNaPasta(const char *pasta, const char *link, BibliotecaAchada *achada)
{
    char candidato[BIBLIOTECA_TAM_CAMINHO];
    for (size_t k = 0; k < sizeof BIB_CANDIDATOS / sizeof BIB_CANDIDATOS[0]; k++) {
        if (BibMontar(candidato, pasta, link, &BIB_CANDIDATOS[k]) && BibTentar(candidato, achada))
            return true;
    }
    return false;
}

/* %SystemRoot%\System32\<link>.dll (so a .dll: o sistema nao traz import library). */
static bool BibAcharNoSistema(const char *link, BibliotecaAchada *achada)
{
    char pasta[BIBLIOTECA_TAM_CAMINHO], candidato[BIBLIOTECA_TAM_CAMINHO];
    const char *raiz = getenv("SystemRoot");
    snprintf(pasta, sizeof pasta, "%s\\System32", raiz != NULL && raiz[0] != '\0' ? raiz : BIB_PASTA_SISTEMA_PADRAO);
    const BibCandidato dll = {"", ".dll"};
    return BibMontar(candidato, pasta, link, &dll) && BibTentar(candidato, achada);
}

/* "c", "m", "msvcrt" e "ucrt" sao a mesma UCRT: o gcc daqui e UCRT (libmsvcrt.a importa api-ms-win-crt-*, que a ucrtbase.dll atende), entao no oraculo C "msvcrt" e a ucrtbase
 * e compartilha o FILE stdout com o print; a msvcrt.dll real do System32 e outro CRT, com outro buffer. */
static bool BibLinkEhCrt(const char *link)
{
    return strcmp(link, "m") == 0 || strcmp(link, "c") == 0 || strcmp(link, "msvcrt") == 0 || strcmp(link, "ucrt") == 0;
}

static bool BibAcharLinuxNaPasta(const char *pasta, const char *link, BibliotecaAchada *achada)
{
    char candidato[BIBLIOTECA_TAM_CAMINHO];
    for (size_t k = 0; k < sizeof BIB_CANDIDATOS_LINUX / sizeof BIB_CANDIDATOS_LINUX[0]; k++) {
        if (BibMontar(candidato, pasta, link, &BIB_CANDIDATOS_LINUX[k]) && BibTentar(candidato, achada))
            return true;
    }
    return false;
}

/* Alvo Linux: cada pasta de busca cede <pasta>\vendor\linux; nada de System32 (o link do Linux nao procura DLL). */
static bool BibAcharLinux(const BibliotecaBusca *busca, const char *link, BibliotecaAchada *achada)
{
    char pasta[BIBLIOTECA_TAM_CAMINHO];
    for (uint32_t i = 0; i < busca->qtd_pastas; i++) {
        snprintf(pasta, sizeof pasta, "%s" BIB_PASTA_LINUX, busca->pastas[i]);
        if (BibAcharLinuxNaPasta(pasta, link, achada))
            return true;
    }
    return false;
}

/* M5b P39-B2: a libc do alvo linux e a semente musl (libc.a ao lado de crt1.o); a libm e a msvcrt estao dentro dela. */
#define BIB_SEMENTE_LIBC "\\libc.a"

static bool BibAcharSemente(const char *pasta, BibliotecaAchada *achada)
{
    char candidato[BIBLIOTECA_TAM_CAMINHO];

    snprintf(candidato, sizeof candidato, "%s" BIB_SEMENTE_LIBC, pasta);
    return BibTentar(candidato, achada);
}

bool BibliotecaAchar(const BibliotecaBusca *busca, const char *link, BibliotecaAchada *achada)
{
    memset(achada, 0, sizeof *achada);
    achada->tipo = BIB_DESCONHECIDO;
    if (busca->elf && busca->semente != NULL && BibLinkEhCrt(link))
        return BibAcharSemente(busca->semente, achada);
    if (busca->elf)
        return BibAcharLinux(busca, link, achada);
    if (BibLinkEhCrt(link))
        return BibAcharNoSistema(BIB_NOME_SISTEMA, achada);
    for (uint32_t i = 0; i < busca->qtd_pastas; i++) {
        if (BibAcharNaPasta(busca->pastas[i], link, achada))
            return true;
    }
    return BibAcharNoSistema(link, achada);
}

void BibliotecaLiberar(BibliotecaAchada *achada)
{
    BufLiberar(&achada->procurei_em);
}
