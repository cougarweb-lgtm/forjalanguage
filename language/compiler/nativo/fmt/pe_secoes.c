/* pe_secoes.c - secoes da Imagem (PE32+ e ELF): ciclo de vida da Imagem, nome de secao de 8 bytes e cadastro de secao no proximo RVA.
 * Versao: 0.3.1
 * Changelog: 0.3.1 - M6b P76 (main, 2a passada de desempenho): o GUID do RSDS passa de FNV-1a byte a byte (~15 ms em 13 MB) para hash de 4 pistas (PeHashExe); so o valor do GUID muda (D-M6-9).
 *            0.3.0 - M6b P75: PeCodeViewSecao, PeCodeViewPonteiro e PeCodeViewGuid (IMAGE_DEBUG_DIRECTORY CODEVIEW + RSDS do .pdb; GUID deterministico, D-M6-9).
 *            0.2.0 - M6 P68: nomes de secao de debug (PeNomeSecaoDebug), nome longo pela tabela de strings COFF (PeNomeSecaoLongo, "/<offset>") e PeTabelaFechar.
 *            0.1.0 - M6 P68 (extracao): ImagemIniciar, ImagemLiberar, ImagemProximoRva, ImagemSecao e PeNomeSecao saem de pe_escrever.c (0.5.4);
 *                    so muda o arquivo onde o codigo mora, nenhum byte do PE muda.
 * Verificacao: construir_rapido.py --selftest e python tests\nativo\pe\verificar_pe.py
 */
#include <stdio.h>

#include "common/hash.h"
#include "nativo/fmt/pe.h"

#define PE_DEBUG_TIPO_CODEVIEW 2u
#define PE_CV_OFF_PONTEIRO 24u
#define PE_CV_OFF_GUID 4u        /* dentro do RSDS: depois da assinatura "RSDS" */
#define PE_CV_TAM_GUID 16u

/* Secoes de debug do programa, na ordem de ImagemDbgIndice (D-M6-3): o PE nao leva .debug_frame. */
static const char *const PE_NOMES_DEBUG[IMAGEM_QTD_DEBUG] = {".debug_abbrev", ".debug_info", ".debug_line", ".debug_frame"};

const char *PeNomeSecaoDebug(uint32_t k)
{
    return PE_NOMES_DEBUG[k];
}

/* Os nomes de debug passam de 8 bytes: a secao leva "/<offset>" (offset em decimal a partir do inicio da tabela, que comeca com o tamanho dela). */
void PeNomeSecaoLongo(Buf *tab, const char *nome, char dst[PE_TAM_NOME])
{
    if (tab->tam == 0)
        BufAnexarN(tab, "\0\0\0\0", 4);
    memset(dst, 0, PE_TAM_NOME);
    snprintf(dst, PE_TAM_NOME, "/%u", (unsigned)tab->tam);
    BufAnexarN(tab, nome, strlen(nome) + 1);
}

void PeTabelaFechar(Buf *tab)
{
    uint32_t tam = (uint32_t)tab->tam;

    if (tab->tam == 0)
        return;
    for (uint32_t k = 0; k < 4; k++)
        tab->dados[k] = (char)(tam >> (8 * k));
}

/* ---------- CodeView (P75) ---------- */

static void PeCvGravar32(char *dst, uint32_t v)
{
    for (uint32_t k = 0; k < 4; k++)
        dst[k] = (char)(v >> (8 * k));
}

static void PeCvAnexar32(Buf *b, uint32_t v)
{
    char c[4];

    PeCvGravar32(c, v);
    BufAnexarN(b, c, sizeof c);
}

void PeCodeViewSecao(PeSecao *s, uint32_t rva, const char *pdb)
{
    uint32_t tam_rsds = 4 + PE_CV_TAM_GUID + 4 + (uint32_t)strlen(pdb) + 1;

    PeNomeSecao(s->nome, ".cvinfo");
    s->caracteristicas = PE_SECAO_LEITURA;
    s->rva = rva;
    PeCvAnexar32(&s->bytes, 0);                      /* Characteristics */
    PeCvAnexar32(&s->bytes, 0);                      /* TimeDateStamp: 0, o exe tem de ser reproduzivel */
    PeCvAnexar32(&s->bytes, 0);                      /* MajorVersion e MinorVersion */
    PeCvAnexar32(&s->bytes, PE_DEBUG_TIPO_CODEVIEW);
    PeCvAnexar32(&s->bytes, tam_rsds);
    PeCvAnexar32(&s->bytes, rva + PE_CV_TAM_DIRETORIO);
    PeCvAnexar32(&s->bytes, 0);                      /* PointerToRawData: PeCodeViewPonteiro, quando o layout do arquivo sai */
    BufAnexarN(&s->bytes, "RSDS", 4);
    for (uint32_t k = 0; k < PE_CV_TAM_GUID; k++)
        BufAnexarN(&s->bytes, "\0", 1);
    PeCvAnexar32(&s->bytes, PE_CV_IDADE);
    BufAnexarN(&s->bytes, pdb, strlen(pdb) + 1);
    s->tam_virtual = (uint32_t)s->bytes.tam;
}

void PeCodeViewPonteiro(Buf *bytes, uint32_t ptr)
{
    PeCvGravar32(bytes->dados + PE_CV_OFF_PONTEIRO, ptr + PE_CV_TAM_DIRETORIO);
}

#define PE_HASH_PISTAS 4u
#define PE_HASH_MULT UINT64_C(0x9E3779B97F4A7C15)
#define PE_HASH_GIRO 29u

/* Hash de 64 bits do exe inteiro em 4 pistas independentes de 8 bytes (xor, giro, multiplicacao): o FNV-1a byte a byte custava ~15 ms num exe de 13 MB
 * (cadeia de dependencia de 1 byte por volta); aqui sao ~2 ms. A sobra (< 32 bytes) entra byte a byte na pista 0 e o tamanho fecha a mistura. Nao e criptografico. */
static uint64_t PeHashExe(const uint8_t *dados, size_t n)
{
    uint64_t h[PE_HASH_PISTAS] = {HASH64_SEMENTE, HASH64_SEMENTE + 1, HASH64_SEMENTE + 2, HASH64_SEMENTE + 3};
    size_t i = 0;

    for (; i + 8 * PE_HASH_PISTAS <= n; i += 8 * PE_HASH_PISTAS) {
        for (uint32_t k = 0; k < PE_HASH_PISTAS; k++) {
            uint64_t w;

            memcpy(&w, dados + i + 8 * k, sizeof w);
            w ^= h[k];
            h[k] = ((w << PE_HASH_GIRO) | (w >> (64 - PE_HASH_GIRO))) * PE_HASH_MULT;
        }
    }
    for (; i < n; i++)
        h[0] = (h[0] ^ dados[i]) * UINT64_C(0x100000001b3);
    h[0] ^= (uint64_t)n;
    for (uint32_t k = 1; k < PE_HASH_PISTAS; k++)
        h[0] = (((h[0] ^ h[k]) << PE_HASH_GIRO) | ((h[0] ^ h[k]) >> (64 - PE_HASH_GIRO))) * PE_HASH_MULT;
    return h[0] ^ (h[0] >> 32);
}

/* 128 bits = hash de 4 pistas do exe inteiro (uma passada so) e a 2a metade, o FNV-1a dos 8 bytes da 1a: deterministico, sem relogio e sem nome de maquina. */
void PeCodeViewGuid(Buf *exe, uint32_t ptr, uint8_t *saida)
{
    uint64_t h1 = PeHashExe((const uint8_t *)exe->dados, exe->tam);
    uint64_t h2 = Hash64(&h1, sizeof h1, ~h1);
    uint8_t *guid = (uint8_t *)exe->dados + ptr + PE_CV_TAM_DIRETORIO + PE_CV_OFF_GUID;

    for (uint32_t k = 0; k < 8; k++) {
        guid[k] = (uint8_t)(h1 >> (8 * k));
        guid[8 + k] = (uint8_t)(h2 >> (8 * k));
    }
    if (saida != NULL)
        memcpy(saida, guid, PE_CV_TAM_GUID);
}

/* ---------- imagem e secoes ---------- */

void ImagemIniciar(Imagem *im)
{
    memset(im, 0, sizeof *im);
    VecIniciar(&im->dir64, sizeof(uint32_t));
    VecIniciar(&im->exportacoes, sizeof(PeExporta));
    VecIniciar(&im->simbolos, sizeof(ImagemSimbolo));
    VecIniciar(&im->relativos, sizeof(ImagemRelativa));
}

void ImagemLiberar(Imagem *im)
{
    for (uint32_t i = 0; i < im->qtd_secoes + IMAGEM_QTD_DEBUG; i++)
        BufLiberar(i < im->qtd_secoes ? &im->secoes[i].bytes : &im->dbg[i - im->qtd_secoes]);
    VecLiberar(&im->dir64);
    VecLiberar(&im->exportacoes);
    VecLiberar(&im->simbolos);
    VecLiberar(&im->relativos);
}

void PeNomeSecao(char dst[PE_TAM_NOME], const char *src)
{
    memset(dst, 0, PE_TAM_NOME);
    for (uint32_t k = 0; k < PE_TAM_NOME && src[k] != '\0'; k++)
        dst[k] = src[k];
}

uint32_t ImagemProximoRva(const Imagem *im)
{
    if (im->qtd_secoes == 0)
        return PE_ALINHAMENTO_SECAO;
    const PeSecao *ult = &im->secoes[im->qtd_secoes - 1];
    return PeAlinhar(ult->rva + ult->tam_virtual, PE_ALINHAMENTO_SECAO);
}

uint32_t ImagemSecao(Imagem *im, const PeSecaoPedido *pedido, Diag *erro)
{
    if (im->qtd_secoes >= IMAGEM_MAX_SECOES)
        FalhaInterna("pe: a imagem passou de %d secoes", IMAGEM_MAX_SECOES);
    uint32_t rva = ImagemProximoRva(im);
    uint32_t tam_dados = pedido->bytes != NULL ? (uint32_t)pedido->bytes->tam : 0;
    uint32_t tam_virtual = pedido->tam_virtual > tam_dados ? pedido->tam_virtual : tam_dados;
    if (tam_virtual == 0) {
        DiagRelatar(erro, X0905, 0, "secao '%.8s' sem bytes", pedido->nome);
        return 0;
    }
    PeSecao *s = &im->secoes[im->qtd_secoes++];
    PeNomeSecao(s->nome, pedido->nome);
    s->caracteristicas = pedido->caracteristicas;
    s->rva = rva;
    s->tam_virtual = tam_virtual;
    if (tam_dados > 0)
        BufAnexarN(&s->bytes, pedido->bytes->dados, pedido->bytes->tam);
    return rva;
}
