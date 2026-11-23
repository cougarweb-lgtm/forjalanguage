/* pe_recursos.c - secao .rsrc do exe nativo: RT_VERSION (VS_VERSIONINFO) e RT_MANIFEST, id 1, lingua 0x0409, em arvore de 3 niveis.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - M2 P9a fase 2: PeAcrescentarRecursos recebe Diag e devolve bool (ImagemSecao recusa secao sem bytes).
 *            0.2.0 - M2 P9a: RsNo recebe RsNoPedido e ImagemSecao recebe PeSecaoPedido (regra de <= 4 parametros).
 *            0.1.0 - backend x64 P7b: primeira versao (ProductName "Forja Native", FileVersion, FileDescription; manifesto asInvoker + Win 10/11).
 * Verificacao: construir.bat && python -I ..\tests\nativo\pe\verificar_pe.py
 *
 * Formato (winnt.h / "Resource Directory"): raiz (tipos 16 e 24) -> diretorio de nomes (id 1) -> diretorio de linguas (0x0409) -> entrada de
 * dados (RVA, tamanho, codepage 0). Depois da arvore vem o blob do VERSIONINFO e o do manifesto, cada um alinhado a 4.
 */
#include <stdio.h>

#include "nativo/fmt/pe_recursos.h"

#define RS_SECAO 0x40000040u        /* INITIALIZED_DATA | MEM_READ */
#define RS_TIPO_VERSAO 16u
#define RS_TIPO_MANIFESTO 24u
#define RS_ID_UNICO 1u
#define RS_LINGUA 0x0409u
#define RS_SUBDIR 0x80000000u
#define RS_TAM_DIR 16u
#define RS_TAM_ENTRADA 8u
#define RS_TAM_DIR_1 (RS_TAM_DIR + RS_TAM_ENTRADA)
#define RS_TAM_DADOS 16u
#define RS_QTD 2u
#define RS_OFF_NOMES 32u            /* depois da raiz (16 + 2 entradas) */
#define RS_PASSO_RECURSO (2u * RS_TAM_DIR_1)
#define RS_OFF_DADOS (RS_OFF_NOMES + RS_QTD * RS_PASSO_RECURSO)
#define RS_OFF_BLOBS (RS_OFF_DADOS + RS_QTD * RS_TAM_DADOS)
#define RS_TIPO_TEXTO 1u
#define RS_TIPO_BINARIO 0u
#define RS_TAM_FIXO 52u
#define RS_ASSINATURA_FIXA 0xFEEF04BDu
#define RS_ESTRUTURA_FIXA 0x00010000u
#define RS_MASCARA_FLAGS 0x3Fu
#define RS_SO_NT_WIN32 0x00040004u
#define RS_TIPO_APP 1u
#define RS_TRADUCAO 0x04B00409u     /* lingua 0x0409, charset Unicode 0x04B0 */

static const char RS_MANIFESTO[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
    "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
    "<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\"><security><requestedPrivileges>"
    "<requestedExecutionLevel level=\"asInvoker\" uiAccess=\"false\"/></requestedPrivileges></security></trustInfo>"
    "<compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>"
    "<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/>"
    "</application></compatibility></assembly>\r\n";

static void RsU16(Buf *b, uint32_t v)
{
    char dois[2] = {(char)v, (char)(v >> 8)};
    BufAnexarN(b, dois, sizeof dois);
}

static void RsU32(Buf *b, uint32_t v)
{
    RsU16(b, v & 0xFFFFu);
    RsU16(b, v >> 16);
}

static void RsPad4(Buf *b)
{
    const char zero = 0;

    while (b->tam % 4 != 0)
        BufAnexarN(b, &zero, 1);
}

/* Texto ASCII como UTF-16LE com o NUL final. Devolve a quantidade de caracteres de 16 bits gravados (NUL incluido). */
static uint32_t RsWide(Buf *b, const char *texto)
{
    uint32_t n = 0;

    for (; texto[n] != '\0'; n++)
        RsU16(b, (unsigned char)texto[n]);
    RsU16(b, 0);
    return n + 1;
}

/* Pedido de um no de VERSIONINFO; valor e filhos podem ser NULL. */
typedef struct {
    const char *chave;
    uint32_t tipo;
    uint32_t tam_valor;
    const Buf *valor;
    const Buf *filhos;
} RsNoPedido;

/* No de VERSIONINFO: wLength, wValueLength, wType, chave UTF-16, valor e filhos, cada parte alinhada a 4. wLength fecha no fim. */
static void RsNo(Buf *dst, const RsNoPedido *no)
{
    size_t inicio = dst->tam;

    RsU16(dst, 0);
    RsU16(dst, no->tam_valor);
    RsU16(dst, no->tipo);
    RsWide(dst, no->chave);
    RsPad4(dst);
    if (no->valor != NULL)
        BufAnexarN(dst, no->valor->dados, no->valor->tam);
    if (no->filhos != NULL && no->filhos->tam != 0) {
        RsPad4(dst);
        BufAnexarN(dst, no->filhos->dados, no->filhos->tam);
    }
    dst->dados[inicio] = (char)(dst->tam - inicio);
    dst->dados[inicio + 1] = (char)((dst->tam - inicio) >> 8);
}

/* "a.b.c" -> a, b, c (o que faltar fica 0). */
static void RsPartesVersao(const char *versao, uint32_t partes[3])
{
    partes[0] = partes[1] = partes[2] = 0;
    sscanf(versao, "%u.%u.%u", &partes[0], &partes[1], &partes[2]);
}

static void RsFixo(Buf *b, const char *versao)
{
    uint32_t p[3];
    uint32_t ms, ls;

    RsPartesVersao(versao, p);
    ms = (p[0] << 16) | p[1];
    ls = p[2] << 16;
    RsU32(b, RS_ASSINATURA_FIXA);
    RsU32(b, RS_ESTRUTURA_FIXA);
    RsU32(b, ms), RsU32(b, ls);
    RsU32(b, ms), RsU32(b, ls);
    RsU32(b, RS_MASCARA_FLAGS);
    RsU32(b, 0);
    RsU32(b, RS_SO_NT_WIN32);
    RsU32(b, RS_TIPO_APP);
    RsU32(b, 0);
    RsU32(b, 0), RsU32(b, 0);
}

/* Um par chave/valor do StringTable, ja alinhado a 4 para o proximo. */
static void RsString(Buf *tabela, const char *chave, const char *valor)
{
    Buf v = {0};
    uint32_t n = RsWide(&v, valor);

    RsNo(tabela, &(RsNoPedido){chave, RS_TIPO_TEXTO, n, &v, NULL});
    RsPad4(tabela);
    BufLiberar(&v);
}

static void RsStringFileInfo(Buf *dst, const char *versao)
{
    Buf tabela = {0}, st = {0};

    RsString(&tabela, "ProductName", "Forja Native");
    RsString(&tabela, "ProductVersion", versao);
    RsString(&tabela, "FileVersion", versao);
    RsString(&tabela, "FileDescription", "Executavel gerado pelo backend nativo da Forja");
    RsNo(&st, &(RsNoPedido){"040904B0", RS_TIPO_TEXTO, 0, NULL, &tabela});
    RsNo(dst, &(RsNoPedido){"StringFileInfo", RS_TIPO_TEXTO, 0, NULL, &st});
    BufLiberar(&tabela);
    BufLiberar(&st);
}

static void RsVarFileInfo(Buf *dst)
{
    Buf traducao = {0}, var = {0};

    RsU32(&traducao, RS_TRADUCAO);
    RsNo(&var, &(RsNoPedido){"Translation", RS_TIPO_BINARIO, sizeof(uint32_t), &traducao, NULL});
    RsNo(dst, &(RsNoPedido){"VarFileInfo", RS_TIPO_TEXTO, 0, NULL, &var});
    BufLiberar(&traducao);
    BufLiberar(&var);
}

/* Blob do RT_VERSION: VS_VERSION_INFO com o VS_FIXEDFILEINFO e os filhos StringFileInfo e VarFileInfo. */
static void RsMontarVersao(Buf *blob, const char *versao)
{
    Buf fixo = {0}, filhos = {0};

    RsFixo(&fixo, versao);
    RsStringFileInfo(&filhos, versao);
    RsPad4(&filhos);
    RsVarFileInfo(&filhos);
    RsNo(blob, &(RsNoPedido){"VS_VERSION_INFO", RS_TIPO_BINARIO, RS_TAM_FIXO, &fixo, &filhos});
    BufLiberar(&fixo);
    BufLiberar(&filhos);
}

static void RsDir(Buf *b, uint32_t qtd_ids)
{
    RsU32(b, 0);
    RsU32(b, 0);
    RsU16(b, 0);
    RsU16(b, 0);
    RsU16(b, 0);
    RsU16(b, qtd_ids);
}

static void RsEntrada(Buf *b, uint32_t id, uint32_t destino)
{
    RsU32(b, id);
    RsU32(b, destino);
}

/* Raiz, depois (nomes, linguas) de cada recurso, depois as duas entradas de dados. Os blobs ficam em rva_blob/tam_blob. */
static void RsArvore(Buf *b, const uint32_t rva_blob[RS_QTD], const uint32_t tam_blob[RS_QTD])
{
    static const uint32_t TIPOS[RS_QTD] = {RS_TIPO_VERSAO, RS_TIPO_MANIFESTO};

    RsDir(b, RS_QTD);
    for (uint32_t k = 0; k < RS_QTD; k++)
        RsEntrada(b, TIPOS[k], (RS_OFF_NOMES + k * RS_PASSO_RECURSO) | RS_SUBDIR);
    for (uint32_t k = 0; k < RS_QTD; k++) {
        uint32_t nomes = RS_OFF_NOMES + k * RS_PASSO_RECURSO;
        RsDir(b, 1);
        RsEntrada(b, RS_ID_UNICO, (nomes + RS_TAM_DIR_1) | RS_SUBDIR);
        RsDir(b, 1);
        RsEntrada(b, RS_LINGUA, RS_OFF_DADOS + k * RS_TAM_DADOS);
    }
    for (uint32_t k = 0; k < RS_QTD; k++) {
        RsU32(b, rva_blob[k]);
        RsU32(b, tam_blob[k]);
        RsU32(b, 0);
        RsU32(b, 0);
    }
}

bool PeAcrescentarRecursos(Imagem *im, const char *versao, Diag *erro)
{
    Buf blobs[RS_QTD] = {{0}, {0}}, secao = {0};
    uint32_t rva = ImagemProximoRva(im), rva_blob[RS_QTD], tam_blob[RS_QTD];

    RsMontarVersao(&blobs[0], versao);
    BufAnexarN(&blobs[1], RS_MANIFESTO, sizeof RS_MANIFESTO - 1);
    rva_blob[0] = rva + RS_OFF_BLOBS;
    tam_blob[0] = (uint32_t)blobs[0].tam;
    rva_blob[1] = rva_blob[0] + ((tam_blob[0] + 3u) & ~3u);
    tam_blob[1] = (uint32_t)blobs[1].tam;
    RsArvore(&secao, rva_blob, tam_blob);
    for (uint32_t k = 0; k < RS_QTD; k++) {
        BufAnexarN(&secao, blobs[k].dados, blobs[k].tam);
        RsPad4(&secao);
        BufLiberar(&blobs[k]);
    }
    uint32_t rva_rsrc = ImagemSecao(im, &(PeSecaoPedido){".rsrc", RS_SECAO, &secao, (uint32_t)secao.tam}, erro);
    BufLiberar(&secao);
    return rva_rsrc != 0;
}
