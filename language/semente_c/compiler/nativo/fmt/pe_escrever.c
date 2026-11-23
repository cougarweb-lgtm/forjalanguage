/* pe_escrever.c - PE32+ do exe Windows x64: secoes, .idata (importacoes), .reloc (ADDR64) e autoconferencia que devolve X0905.
 * Versao: 0.7.0
 * Changelog: 0.7.0 - M6b P75: secao ".cvinfo" (IMAGE_DEBUG_DIRECTORY + RSDS, so com Imagem.pdb_caminho) depois do .reloc, DataDirectory[DEBUG] e o GUID (hash do exe); sem pdb_caminho nenhum byte muda.
 *            0.6.0 - M6 P68: as secoes de debug do programa (Imagem.dbg) vao ao fim da imagem, depois do .reloc, com RVA proprio e sem .reloc (D-M6-4);
 *                    nome de mais de 8 bytes pela tabela de strings COFF (PointerToSymbolTable; sem nome longo, nenhum byte muda).
 *            0.5.4 - M6 P68 (extracao): imagem e secoes (ImagemIniciar/Liberar/ProximoRva/Secao, PeNomeSecao, PeAlinhar, PE_ALINHAMENTO_SECAO, PE_TAM_NOME) foram
 *                    para pe_secoes.c; nenhum byte do PE muda.
 *            0.5.3 - M6 P66: ImagemLiberar libera Imagem.dbg (secoes de debug do ELF); nenhum byte do PE muda. 0.5.2 - M5b P41: ImagemIniciar/ImagemLiberar tratam Imagem.relativos (vazio fora do ELF pie); nenhum byte do PE muda.
 *            0.5.1 - M5 P27a: ImagemIniciar/ImagemLiberar tratam Imagem.simbolos (vazio no PE); nenhum byte do PE muda.
 *            0.5.0 - M3 P22a: DLL (Imagem.dll: IMAGE_FILE_DLL no COFF, ImageBase PeBaseDaImagem) e o diretorio de exportacao (indice 0) aponta para a ".edata" quando ela existe.
 *            0.4.0 - M3 P19a: .idata com N DLLs (PeReservarImportacoesDlls; PeReservarImportacoes e o caso de uma DLL, com os mesmos bytes de antes): um descritor
 *                    por DLL, ILT e IAT de cada uma em sequencia, uma tabela de hint/name; ImagemImportacao.dlls guarda nome, primeiro e quantidade.
 *            0.3.1 -M2 P17c: PeGuardarImportacoes guarda tambem os nomes (ImagemImportacao.nomes).
 *            0.3.0 - M2 P9a fase 2: .reloc sempre presente (sem ADDR64, bloco na pagina do .text com entrada ABSOLUTE de 12 B, como o ld);
 *                    ImagemSecao recusa secao sem bytes com X0905 "secao '<nome>' sem bytes" (recebe Diag).
 *            0.2.2 - M2 P9a: ImagemSecao recebe PeSecaoPedido e PeMontarIdata recebe PeIdataPedido (regra de <= 4 parametros).
 *            0.2.1 - portao M1: PE_SECAO_* passam para pe.h (o ligador as repetia).
 *            0.2.0 - backend x64 P7b: versao do linker vem da Imagem (a do forja; o 2.46 do ld saiu, nao era verdade) e o diretorio de recursos
 *                    aponta para a secao .rsrc.
 *            0.1.1 - backend x64 P5: PE_IMAGE_BASE movido para pe.h.
 *            0.1.0 - backend x64 P3: ImagemSecao, PeReservarImportacoes e PeEscrever; cabecalhos copiados do exe do ld.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\pe\verificar_pe.py
 */
#include "nativo/fmt/pe.h"

/* Valores do PE32+ copiados do exe que o ld gera (objdump -p examples\teste.exe). */
#define PE_LFANEW 0x80u
#define PE_OFF_LFANEW 0x3Cu
#define PE_TAM_ASSINATURA 4u
#define PE_OFF_COFF (PE_LFANEW + PE_TAM_ASSINATURA)
#define PE_TAM_COFF 20u
#define PE_COFF_NSECOES 2u
#define PE_COFF_TAM_OPCIONAL 16u
#define PE_OFF_OPCIONAL (PE_OFF_COFF + PE_TAM_COFF)
#define PE_OPC_ENTRADA 16u
#define PE_TAM_OPCIONAL_FIXO 112u
#define PE_DIRETORIOS 16u
#define PE_TAM_DIRETORIO 8u
#define PE_TAM_OPCIONAL (PE_TAM_OPCIONAL_FIXO + PE_DIRETORIOS * PE_TAM_DIRETORIO)
#define PE_OFF_DIRETORIOS (PE_OFF_OPCIONAL + PE_TAM_OPCIONAL_FIXO)
#define PE_OFF_SECOES (PE_OFF_OPCIONAL + PE_TAM_OPCIONAL)
#define PE_TAM_SECAO_TAB 40u
#define PE_NOME_TEXTO ".text\0\0\0"
#define PE_MAQUINA_AMD64 0x8664u
#define PE_CARAC_EXE 0x26u        /* EXECUTABLE_IMAGE | LINE_NUMS_STRIPPED | LARGE_ADDRESS_AWARE */
#define PE_CARAC_DLL 0x2026u      /* o do exe | IMAGE_FILE_DLL */
#define PE_MAGIC_PE32_MAIS 0x20Bu
#define PE_VERSAO_SO_MAIOR 4u
#define PE_VERSAO_SUBSISTEMA_MAIOR 5u
#define PE_VERSAO_SUBSISTEMA_MENOR 2u
#define PE_SUBSISTEMA_CUI 3u
#define PE_DLL_CARAC 0x160u       /* HIGH_ENTROPY_VA | DYNAMIC_BASE | NX_COMPAT */
#define PE_PILHA_RESERVA 0x200000ull
#define PE_PILHA_COMMIT 0x1000ull
#define PE_HEAP_RESERVA 0x100000ull
#define PE_HEAP_COMMIT 0x1000ull
#define PE_ALINHAMENTO_ARQUIVO 0x200u
#define PE_QTD_SECOES_MAX (IMAGEM_MAX_SECOES + 1 + IMAGEM_QTD_DEBUG)  /* as da Imagem, o .reloc e as de debug */
#define PE_DIR_EXPORT 0u
#define PE_DIR_IMPORT 1u
#define PE_DIR_RECURSOS 2u
#define PE_DIR_EXCECAO 3u
#define PE_DIR_BASERELOC 5u
#define PE_DIR_DEBUG 6u
#define PE_DIR_IAT 12u
#define PE_DESCRITORES_FIM 1u     /* o descritor nulo que fecha a tabela, depois do descritor de cada DLL */
#define PE_TAM_DESCRITOR 20u
#define PE_TAM_THUNK 8u
#define PE_ALINHAMENTO_HINT 2u
#define PE_TAM_BLOCO_RELOC 8u
#define PE_TAM_ENTRADA_RELOC 2u
#define PE_ALINHAMENTO_BLOCO_RELOC 4u
#define PE_RELOC_DIR64 10u
#define PE_BITS_TIPO_RELOC 12u
#define PE_BITS_BYTE 8u

/* DOS stub copiado do exe do ld: MZ, e_lfanew = 0x80 em 0x3C e a mensagem padrao. */
static const uint8_t PeDosPadrao[PE_LFANEW] = {
    0x4d, 0x5a, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd, 0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, 0x54, 0x68,
    0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f,
    0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6e, 0x20, 0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20,
    0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x0d, 0x0d, 0x0a, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Layout calculado antes de escrever: as secoes da Imagem mais o .reloc (se houver) e os ponteiros do arquivo. */
typedef struct {
    PeSecao secs[PE_QTD_SECOES_MAX];
    uint32_t qtd;
    uint32_t ptr[PE_QTD_SECOES_MAX];        /* PointerToRawData (0 sem bytes) */
    uint32_t bruto[PE_QTD_SECOES_MAX];      /* SizeOfRawData: bytes alinhados a 0x200 */
    uint32_t tam_cabecalhos;                /* SizeOfHeaders */
    uint32_t tam_imagem;                    /* SizeOfImage */
    Buf tab_nomes;                          /* tabela de strings COFF (nomes longos); vazia = sem tabela */
    uint32_t ptr_tab_nomes;                 /* PointerToSymbolTable da tabela de strings (0 sem tabela) */
    uint32_t cv;                            /* indice em secs da ".cvinfo" (0 = sem CodeView: o 0 e sempre .text) */
} PeLayout;

/* Visao somente-leitura do arquivo gravado, para a autoconferencia. */
typedef struct { const uint8_t *d; uint32_t n; } PeVisao;

/* ---------- utilitarios de bytes ---------- */

static void PeAnexarLe(Buf *b, uint64_t v, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        char byte = (char)(v >> (k * PE_BITS_BYTE));
        BufAnexarN(b, &byte, 1);
    }
}

static void PeU8(Buf *b, uint64_t v) { PeAnexarLe(b, v, sizeof(uint8_t)); }
static void PeU16(Buf *b, uint64_t v) { PeAnexarLe(b, v, sizeof(uint16_t)); }
static void PeU32(Buf *b, uint64_t v) { PeAnexarLe(b, v, sizeof(uint32_t)); }
static void PeU64(Buf *b, uint64_t v) { PeAnexarLe(b, v, sizeof(uint64_t)); }

static void PeZeros(Buf *b, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++)
        PeU8(b, 0);
}

static void PeAlinharBuf(Buf *b, uint32_t alinhamento)
{
    uint32_t resto = (uint32_t)(b->tam % alinhamento);
    if (resto != 0)
        PeZeros(b, alinhamento - resto);
}

/* ---------- importacoes (.idata) ---------- */

static uint32_t PeTamThunks(uint32_t n)
{
    return PE_TAM_THUNK * (n + 1);
}

static uint32_t PeTotalNomes(const PeImportDll *dlls, uint32_t n_dlls)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < n_dlls; i++)
        total += dlls[i].n;
    return total;
}

/* ILT e IAT de todas as DLLs: cada uma tem os seus n + 1 thunks (o ultimo e nulo). */
static uint32_t PeTamThunksDlls(const PeImportDll *dlls, uint32_t n_dlls)
{
    uint32_t tam = 0;
    for (uint32_t i = 0; i < n_dlls; i++)
        tam += PeTamThunks(dlls[i].n);
    return tam;
}

/* Onde cada parte do .idata cai (RVA): ILT de todas as DLLs, IAT de todas e a tabela de hint/name. A ILT e a IAT tem o mesmo tamanho. */
typedef struct {
    uint32_t ilt, iat, hint;
    uint32_t tam_thunks;
} PeIdataPlano;

static PeIdataPlano PePlanejarIdata(uint32_t base, const PeImportDll *dlls, uint32_t n_dlls)
{
    PeIdataPlano pl;
    pl.tam_thunks = PeTamThunksDlls(dlls, n_dlls);
    pl.ilt = base + (n_dlls + PE_DESCRITORES_FIM) * PE_TAM_DESCRITOR;
    pl.iat = pl.ilt + pl.tam_thunks;
    pl.hint = pl.iat + pl.tam_thunks;
    return pl;
}

/* Hint/name: hint 0 (sem tabela de ordinais), nome e NUL, com padding para a entrada cair em par. */
static void PeMontarHintNames(const char *const *nomes, uint32_t n, Buf *tab, uint32_t *offs)
{
    for (uint32_t k = 0; k < n; k++) {
        offs[k] = (uint32_t)tab->tam;
        PeU16(tab, 0);
        BufAnexar(tab, nomes[k]);
        PeU8(tab, 0);
        if (tab->tam % PE_ALINHAMENTO_HINT != 0)
            PeU8(tab, 0);
    }
}

static void PeMontarDescritor(Buf *b, uint32_t ilt, uint32_t nome, uint32_t iat)
{
    PeU32(b, ilt);
    PeU32(b, 0);
    PeU32(b, 0);
    PeU32(b, nome);
    PeU32(b, iat);
}

static void PeMontarThunks(Buf *b, const uint32_t *rvas, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++)
        PeU64(b, rvas[k]);
    PeU64(b, 0);
}

/* Entrada de PeMontarIdata: RVA da secao (base) e as DLLs importadas, na ordem dos descritores. */
typedef struct {
    uint32_t base;
    const PeImportDll *dlls;
    uint32_t n_dlls;
} PeIdataPedido;

/* Hint/name de todas as DLLs numa tabela so; offs[k] = offset do k-esimo nome (DLL por DLL) dentro dela. */
static void PeMontarTabelaNomes(const PeIdataPedido *p, Buf *tab, uint32_t *offs)
{
    uint32_t primeiro = 0;
    for (uint32_t i = 0; i < p->n_dlls; i++) {
        PeMontarHintNames(p->dlls[i].nomes, p->dlls[i].n, tab, offs + primeiro);
        primeiro += p->dlls[i].n;
    }
}

/* RVA do texto do nome de cada DLL: os nomes vem depois da tabela de hint/name, um atras do outro. */
static void PeRvasNomesDll(const PeIdataPedido *p, uint32_t rva, uint32_t *rvas)
{
    for (uint32_t i = 0; i < p->n_dlls; i++) {
        rvas[i] = rva;
        rva += (uint32_t)strlen(p->dlls[i].dll) + 1;
    }
}

/* Um descritor por DLL (ILT, nome, IAT) e o descritor nulo que fecha a tabela. */
static void PeMontarDescritores(const PeIdataPedido *p, const PeIdataPlano *pl, const uint32_t *nomes_dll, Buf *idata)
{
    uint32_t ilt = pl->ilt, iat = pl->iat;
    for (uint32_t i = 0; i < p->n_dlls; i++) {
        PeMontarDescritor(idata, ilt, nomes_dll[i], iat);
        ilt += PeTamThunks(p->dlls[i].n);
        iat += PeTamThunks(p->dlls[i].n);
    }
    PeZeros(idata, PE_TAM_DESCRITOR);
}

static void PeMontarThunksDlls(const PeIdataPedido *p, const uint32_t *rvas, Buf *idata)
{
    uint32_t primeiro = 0;
    for (uint32_t i = 0; i < p->n_dlls; i++) {
        PeMontarThunks(idata, rvas + primeiro, p->dlls[i].n);
        primeiro += p->dlls[i].n;
    }
}

/* Secao .idata inteira, com enderecos ja absolutos (base = RVA da secao): descritores, ILT e IAT de cada DLL, hint/name e os nomes das DLLs. */
static void PeMontarIdata(const PeIdataPedido *p, Buf *idata)
{
    PeIdataPlano pl = PePlanejarIdata(p->base, p->dlls, p->n_dlls);
    uint32_t offs[IMAGEM_MAX_IMPORTS], rvas[IMAGEM_MAX_IMPORTS], nomes_dll[IMAGEM_MAX_DLLS];
    uint32_t total = PeTotalNomes(p->dlls, p->n_dlls);
    Buf tab = {0};
    PeMontarTabelaNomes(p, &tab, offs);
    for (uint32_t k = 0; k < total; k++)
        rvas[k] = pl.hint + offs[k];
    PeRvasNomesDll(p, pl.hint + (uint32_t)tab.tam, nomes_dll);
    PeMontarDescritores(p, &pl, nomes_dll, idata);
    PeMontarThunksDlls(p, rvas, idata);
    PeMontarThunksDlls(p, rvas, idata);
    BufAnexarN(idata, tab.dados, tab.tam);
    for (uint32_t i = 0; i < p->n_dlls; i++) {
        BufAnexar(idata, p->dlls[i].dll);
        PeU8(idata, 0);
    }
    BufLiberar(&tab);
}

/* DLL por DLL: iat_rva e nomes seguem a ordem do pedido (a IAT de cada DLL e contigua, com o thunk nulo no fim). */
static void PeGuardarDlls(ImagemImportacao *imp, const PeIdataPedido *p, uint32_t iat)
{
    imp->qtd = 0;
    imp->qtd_dlls = p->n_dlls;
    for (uint32_t i = 0; i < p->n_dlls; i++) {
        const PeImportDll *d = &p->dlls[i];
        imp->dlls[i] = (ImagemDll){d->dll, imp->qtd, d->n};
        for (uint32_t k = 0; k < d->n; k++, imp->qtd++) {
            imp->iat_rva[imp->qtd] = iat + k * PE_TAM_THUNK;
            imp->nomes[imp->qtd] = d->nomes[k];
        }
        iat += PeTamThunks(d->n);
    }
}

static void PeGuardarImportacoes(Imagem *im, const PeIdataPedido *p)
{
    ImagemImportacao *imp = &im->importacao;
    PeIdataPlano pl = PePlanejarIdata(p->base, p->dlls, p->n_dlls);
    imp->dir_import_rva = p->base;
    imp->dir_import_tam = (p->n_dlls + PE_DESCRITORES_FIM) * PE_TAM_DESCRITOR;
    imp->dir_iat_rva = pl.iat;
    imp->dir_iat_tam = pl.tam_thunks;
    PeGuardarDlls(imp, p, pl.iat);
}

/* 1 a IMAGEM_MAX_DLLS DLLs, cada uma com pelo menos 1 nome, e no maximo IMAGEM_MAX_IMPORTS nomes no total. */
static bool PeImportacoesValidas(const PeImportDll *dlls, uint32_t n_dlls)
{
    if (n_dlls == 0 || n_dlls > IMAGEM_MAX_DLLS)
        return false;
    for (uint32_t i = 0; i < n_dlls; i++) {
        if (dlls[i].n == 0)
            return false;
    }
    return PeTotalNomes(dlls, n_dlls) <= IMAGEM_MAX_IMPORTS;
}

bool PeReservarImportacoesDlls(Imagem *im, const PeImportDll *dlls, uint32_t n_dlls)
{
    if (!PeImportacoesValidas(dlls, n_dlls))
        return false;
    uint32_t base = ImagemProximoRva(im);
    Buf idata = {0};
    Diag erro = {0};
    PeIdataPedido pedido = {base, dlls, n_dlls};
    PeMontarIdata(&pedido, &idata);
    PeSecaoPedido secao = {".idata", PE_SECAO_IDATA, &idata, (uint32_t)idata.tam};
    uint32_t rva = ImagemSecao(im, &secao, &erro);
    BufLiberar(&idata);
    if (rva == 0)
        FalhaInterna("pe: .idata recusado: %s", erro.causa);
    if (rva != base)
        FalhaInterna("pe: .idata em 0x%x, previsto 0x%x", rva, base);
    PeGuardarImportacoes(im, &pedido);
    return true;
}

bool PeReservarImportacoes(Imagem *im, const char *dll, const char *const *nomes, uint32_t n)
{
    PeImportDll unica = {dll, nomes, n};
    return PeReservarImportacoesDlls(im, &unica, 1);
}

/* ---------- .reloc (campos ADDR64 da Imagem) ---------- */

static uint32_t PeRvaDir64(const Imagem *im, uint32_t i)
{
    return *(const uint32_t *)VecEm(&im->dir64, i);
}

/* Um bloco por pagina de 4 KiB: cabecalho (pagina, tamanho), uma entrada DIR64 por campo e padding de 4 bytes. */
static void PeAnexarBlocoReloc(const Imagem *im, uint32_t *i, Buf *b)
{
    uint32_t ini = *i;
    uint32_t mascara = ~(PE_ALINHAMENTO_SECAO - 1u);
    uint32_t pagina = PeRvaDir64(im, ini) & mascara;
    while (*i < im->dir64.n && (PeRvaDir64(im, *i) & mascara) == pagina)
        (*i)++;
    uint32_t qtd = *i - ini;
    PeU32(b, pagina);
    PeU32(b, PeAlinhar(PE_TAM_BLOCO_RELOC + qtd * PE_TAM_ENTRADA_RELOC, PE_ALINHAMENTO_BLOCO_RELOC));
    for (uint32_t k = ini; k < *i; k++)
        PeU16(b, ((uint64_t)PE_RELOC_DIR64 << PE_BITS_TIPO_RELOC) | (PeRvaDir64(im, k) & (PE_ALINHAMENTO_SECAO - 1u)));
    if (qtd % 2 != 0)
        PeU16(b, 0);
}

static void PeMontarReloc(const Imagem *im, Buf *b)
{
    uint32_t i = 0;
    while (i < im->dir64.n)
        PeAnexarBlocoReloc(im, &i, b);
}

/* ---------- layout ---------- */

static void PeCopiarSecoes(const Imagem *im, PeLayout *L)
{
    memcpy(L->secs, im->secoes, im->qtd_secoes * sizeof im->secoes[0]);
    L->qtd = im->qtd_secoes;
}

static uint32_t PeBaseCodigo(const PeLayout *L)
{
    for (uint32_t i = 0; i < L->qtd; i++) {
        if (L->secs[i].caracteristicas & PE_SECAO_CODIGO)
            return L->secs[i].rva;
    }
    return 0;
}

/* Sem ADDR64 o .reloc existe mesmo assim (como no ld): um bloco na pagina do .text, uma entrada ABSOLUTE (tipo 0, nao faz nada) e padding a 4 B. */
static void PeMontarRelocVazio(uint32_t pagina, Buf *b)
{
    PeU32(b, pagina);
    PeU32(b, PeAlinhar(PE_TAM_BLOCO_RELOC + PE_TAM_ENTRADA_RELOC, PE_ALINHAMENTO_BLOCO_RELOC));
    PeU16(b, 0);
    PeU16(b, 0);
}

static void PeAcrescentarReloc(const Imagem *im, Buf *reloc, PeLayout *L)
{
    if (im->dir64.n == 0)
        PeMontarRelocVazio(PeBaseCodigo(L), reloc);
    else
        PeMontarReloc(im, reloc);
    PeSecao *s = &L->secs[L->qtd++];
    memset(s, 0, sizeof *s);
    PeNomeSecao(s->nome, ".reloc");
    s->caracteristicas = PE_SECAO_RELOC;
    s->rva = ImagemProximoRva(im);
    s->tam_virtual = (uint32_t)reloc->tam;
    s->bytes = *reloc;
}

/* RVA logo depois da ultima secao ja no layout (o .reloc, quando as de debug vem depois dele). */
static uint32_t PeProximoRvaLayout(const PeLayout *L)
{
    const PeSecao *ult = &L->secs[L->qtd - 1];
    return PeAlinhar(ult->rva + ult->tam_virtual, PE_ALINHAMENTO_SECAO);
}

/* CodeView do .pdb (P75): secao propria (nao descartavel: o minidump le o RSDS da memoria) depois do .reloc; os bytes sao do layout (PeEscrever libera). */
static void PeAcrescentarCodeView(const Imagem *im, PeLayout *L)
{
    if (im->pdb_caminho == NULL)
        return;
    uint32_t rva = PeProximoRvaLayout(L);

    L->cv = L->qtd++;
    memset(&L->secs[L->cv], 0, sizeof(PeSecao));
    PeCodeViewSecao(&L->secs[L->cv], rva, im->pdb_caminho);
}

/* Secoes de debug do programa (D-M6-4): no fim da imagem, depois do .reloc, sem entrada nele (a relocacao de debug e absoluta, feita pelo ligador).
 * Os bytes sao emprestados de Imagem.dbg (ImagemLiberar os libera); o nome longo vai para a tabela de strings COFF. */
static void PeAcrescentarDebug(const Imagem *im, PeLayout *L)
{
    for (uint32_t k = 0; k < IMAGEM_QTD_DEBUG; k++) {
        if (im->dbg[k].tam == 0)
            continue;
        uint32_t rva = PeProximoRvaLayout(L);
        PeSecao *s = &L->secs[L->qtd++];
        memset(s, 0, sizeof *s);
        s->caracteristicas = PE_SECAO_RELOC;   /* DISCARDABLE | READ | INITIALIZED_DATA, como o ld */
        s->rva = rva;
        s->tam_virtual = (uint32_t)im->dbg[k].tam;
        s->bytes = im->dbg[k];
        PeNomeSecaoLongo(&L->tab_nomes, PeNomeSecaoDebug(k), s->nome);
    }
    PeTabelaFechar(&L->tab_nomes);
}

static void PeCalcularArquivo(PeLayout *L)
{
    uint32_t ptr = PeAlinhar(PE_OFF_SECOES + L->qtd * PE_TAM_SECAO_TAB, PE_ALINHAMENTO_ARQUIVO);
    L->tam_cabecalhos = ptr;
    for (uint32_t i = 0; i < L->qtd; i++) {
        const PeSecao *s = &L->secs[i];
        uint32_t tam = (uint32_t)s->bytes.tam;
        L->bruto[i] = PeAlinhar(tam, PE_ALINHAMENTO_ARQUIVO);
        L->ptr[i] = tam != 0 ? ptr : 0;
        ptr += L->bruto[i];
        L->tam_imagem = PeAlinhar(s->rva + s->tam_virtual, PE_ALINHAMENTO_SECAO);
    }
    L->ptr_tab_nomes = L->tab_nomes.tam > 0 ? ptr : 0;
    if (L->cv != 0)
        PeCodeViewPonteiro(&L->secs[L->cv].bytes, L->ptr[L->cv]);
}

static void PeMontar(const Imagem *im, Buf *reloc, PeLayout *L)
{
    memset(L, 0, sizeof *L);
    PeCopiarSecoes(im, L);
    PeAcrescentarReloc(im, reloc, L);
    PeAcrescentarCodeView(im, L);
    PeAcrescentarDebug(im, L);
    PeCalcularArquivo(L);
}

/* Soma das secoes com a mascara; bruto = tamanho no arquivo, senao o tamanho em memoria. */
static uint32_t PeSomar(const PeLayout *L, uint32_t mascara, bool bruto)
{
    uint32_t soma = 0;
    for (uint32_t i = 0; i < L->qtd; i++) {
        if (L->secs[i].caracteristicas & mascara)
            soma += bruto ? L->bruto[i] : L->secs[i].tam_virtual;
    }
    return soma;
}

/* ---------- escrita ---------- */

static void PeEscreverDos(Buf *b)
{
    BufAnexarN(b, (const char *)PeDosPadrao, PE_LFANEW);
}

static const PeSecao *PeAcharSecao(const PeLayout *L, const char *nome)
{
    char alvo[PE_TAM_NOME];
    PeNomeSecao(alvo, nome);
    for (uint32_t i = 0; i < L->qtd; i++) {
        if (memcmp(L->secs[i].nome, alvo, PE_TAM_NOME) == 0)
            return &L->secs[i];
    }
    return NULL;
}

static void PeDefinirDiretorios(const PeLayout *L, const Imagem *im, uint32_t dir[PE_DIRETORIOS][2])
{
    const PeSecao *pdata = PeAcharSecao(L, ".pdata");
    const PeSecao *reloc = PeAcharSecao(L, ".reloc");
    const PeSecao *rsrc = PeAcharSecao(L, ".rsrc");
    const PeSecao *edata = PeAcharSecao(L, ".edata");
    const ImagemImportacao *imp = &im->importacao;
    if (L->cv != 0) {
        dir[PE_DIR_DEBUG][0] = L->secs[L->cv].rva;
        dir[PE_DIR_DEBUG][1] = PE_CV_TAM_DIRETORIO;
    }
    if (edata != NULL) {
        dir[PE_DIR_EXPORT][0] = edata->rva;
        dir[PE_DIR_EXPORT][1] = edata->tam_virtual;
    }
    dir[PE_DIR_IMPORT][0] = imp->dir_import_rva;
    dir[PE_DIR_IMPORT][1] = imp->dir_import_tam;
    if (rsrc != NULL) {
        dir[PE_DIR_RECURSOS][0] = rsrc->rva;
        dir[PE_DIR_RECURSOS][1] = rsrc->tam_virtual;
    }
    dir[PE_DIR_IAT][0] = imp->dir_iat_rva;
    dir[PE_DIR_IAT][1] = imp->dir_iat_tam;
    if (pdata != NULL) {
        dir[PE_DIR_EXCECAO][0] = pdata->rva;
        dir[PE_DIR_EXCECAO][1] = pdata->tam_virtual;
    }
    if (reloc != NULL) {
        dir[PE_DIR_BASERELOC][0] = reloc->rva;
        dir[PE_DIR_BASERELOC][1] = reloc->tam_virtual;
    }
}

static void PeEscreverDiretorios(const PeLayout *L, const Imagem *im, Buf *b)
{
    uint32_t dir[PE_DIRETORIOS][2];
    memset(dir, 0, sizeof dir);
    PeDefinirDiretorios(L, im, dir);
    for (uint32_t i = 0; i < PE_DIRETORIOS; i++) {
        PeU32(b, dir[i][0]);
        PeU32(b, dir[i][1]);
    }
}

/* Campos do opcional ate ImageBase e alinhamentos (magic, linker, tamanhos de codigo e dados, entrada, base). */
static void PeEscreverOpcionalCampos(const PeLayout *L, const Imagem *im, Buf *b)
{
    PeU16(b, PE_MAGIC_PE32_MAIS);
    PeU8(b, im->linker_maior);
    PeU8(b, im->linker_menor);
    PeU32(b, PeSomar(L, PE_SECAO_CODIGO, true));
    PeU32(b, PeSomar(L, PE_SECAO_DADOS_INIT, true));
    PeU32(b, PeSomar(L, PE_SECAO_NAO_INIT, false));
    PeU32(b, im->entrada_rva);
    PeU32(b, PeBaseCodigo(L));
    PeU64(b, PeBaseDaImagem(im));
    PeU32(b, PE_ALINHAMENTO_SECAO);
    PeU32(b, PE_ALINHAMENTO_ARQUIVO);
}

/* Campos do opcional depois do ImageBase: versoes, tamanhos de imagem e cabecalho, pilha e heap. */
static void PeEscreverOpcionalVersoes(const PeLayout *L, Buf *b)
{
    PeU16(b, PE_VERSAO_SO_MAIOR);
    PeU16(b, 0);
    PeU16(b, 0);
    PeU16(b, 0);
    PeU16(b, PE_VERSAO_SUBSISTEMA_MAIOR);
    PeU16(b, PE_VERSAO_SUBSISTEMA_MENOR);
    PeU32(b, 0);
    PeU32(b, L->tam_imagem);
    PeU32(b, L->tam_cabecalhos);
    PeU32(b, 0);
    PeU16(b, PE_SUBSISTEMA_CUI);
    PeU16(b, PE_DLL_CARAC);
    PeU64(b, PE_PILHA_RESERVA);
    PeU64(b, PE_PILHA_COMMIT);
    PeU64(b, PE_HEAP_RESERVA);
    PeU64(b, PE_HEAP_COMMIT);
    PeU32(b, 0);
    PeU32(b, PE_DIRETORIOS);
}

static void PeEscreverOpcional(const PeLayout *L, const Imagem *im, Buf *b)
{
    PeEscreverOpcionalCampos(L, im, b);
    PeEscreverOpcionalVersoes(L, b);
    PeEscreverDiretorios(L, im, b);
}

static void PeEscreverCabecalho(const PeLayout *L, const Imagem *im, Buf *b)
{
    BufAnexarN(b, "PE\0\0", PE_TAM_ASSINATURA);
    PeU16(b, PE_MAQUINA_AMD64);
    PeU16(b, L->qtd);
    PeU32(b, 0);
    PeU32(b, L->ptr_tab_nomes);
    PeU32(b, 0);
    PeU16(b, PE_TAM_OPCIONAL);
    PeU16(b, im->dll ? PE_CARAC_DLL : PE_CARAC_EXE);
    PeEscreverOpcional(L, im, b);
}

static void PeEscreverSecaoTab(const PeSecao *s, uint32_t ptr, uint32_t bruto, Buf *b)
{
    BufAnexarN(b, s->nome, PE_TAM_NOME);
    PeU32(b, s->tam_virtual);
    PeU32(b, s->rva);
    PeU32(b, bruto);
    PeU32(b, ptr);
    PeU32(b, 0);
    PeU32(b, 0);
    PeU32(b, 0);
    PeU32(b, s->caracteristicas);
}

static void PeEscreverDados(const PeSecao *s, uint32_t bruto, Buf *b)
{
    if (bruto == 0)
        return;
    BufAnexarN(b, s->bytes.dados, s->bytes.tam);
    PeAlinharBuf(b, PE_ALINHAMENTO_ARQUIVO);
}

static void PeEscreverSecoes(const PeLayout *L, Buf *b)
{
    for (uint32_t i = 0; i < L->qtd; i++)
        PeEscreverSecaoTab(&L->secs[i], L->ptr[i], L->bruto[i], b);
    PeAlinharBuf(b, PE_ALINHAMENTO_ARQUIVO);
    for (uint32_t i = 0; i < L->qtd; i++)
        PeEscreverDados(&L->secs[i], L->bruto[i], b);
}

/* ---------- autoconferencia (X0905) ---------- */

static bool PeCabe(const PeVisao *v, uint32_t off, uint32_t tam)
{
    return off <= v->n && tam <= v->n - off;
}

/* Le n bytes (n <= 4) little-endian; quem chama confere PeCabe antes. */
static uint32_t PeLerLe(const PeVisao *v, uint32_t off, uint32_t n)
{
    uint32_t r = 0;
    for (uint32_t k = 0; k < n; k++)
        r |= (uint32_t)v->d[off + k] << (k * PE_BITS_BYTE);
    return r;
}

static uint32_t PeLer16(const PeVisao *v, uint32_t off)
{
    return PeLerLe(v, off, sizeof(uint16_t));
}

static uint32_t PeLer32(const PeVisao *v, uint32_t off)
{
    return PeLerLe(v, off, sizeof(uint32_t));
}

static bool PeDentroDe(uint32_t valor, uint32_t ini, uint32_t tam)
{
    return valor >= ini && valor - ini < tam;
}

static const char *PeChecarDos(const PeVisao *v)
{
    if (!PeCabe(v, 0, PE_LFANEW))
        return "arquivo menor que o cabecalho DOS";
    if (v->d[0] != 'M' || v->d[1] != 'Z')
        return "assinatura MZ ausente";
    if (PeLer32(v, PE_OFF_LFANEW) != PE_LFANEW)
        return "e_lfanew diferente de 0x80";
    return NULL;
}

static const char *PeChecarCoff(const PeVisao *v)
{
    if (!PeCabe(v, PE_LFANEW, PE_OFF_SECOES - PE_LFANEW))
        return "cabecalho PE truncado";
    if (memcmp(v->d + PE_LFANEW, "PE\0\0", PE_TAM_ASSINATURA) != 0)
        return "assinatura PE ausente";
    if (PeLer16(v, PE_OFF_COFF) != PE_MAQUINA_AMD64)
        return "maquina diferente de AMD64";
    if (PeLer16(v, PE_OFF_COFF + PE_COFF_TAM_OPCIONAL) != PE_TAM_OPCIONAL)
        return "SizeOfOptionalHeader diferente de 240";
    if (PeLer16(v, PE_OFF_OPCIONAL) != PE_MAGIC_PE32_MAIS)
        return "magic diferente de PE32+";
    return NULL;
}

static const char *PeChecarSecao(const PeVisao *v, uint32_t off)
{
    uint32_t bruto = PeLer32(v, off + 16), ptr = PeLer32(v, off + 20);
    if (PeLer32(v, off + 12) % PE_ALINHAMENTO_SECAO != 0)
        return "secao com RVA fora do alinhamento de 0x1000";
    if (!PeCabe(v, ptr, bruto))
        return "dados de secao fora do arquivo";
    if (bruto != 0 && ptr % PE_ALINHAMENTO_ARQUIVO != 0)
        return "dados de secao fora do alinhamento de 0x200";
    return NULL;
}

static const char *PeChecarSecoes(const PeVisao *v, uint32_t qtd)
{
    if (PeLer16(v, PE_OFF_COFF + PE_COFF_NSECOES) != qtd)
        return "numero de secoes diferente do montado";
    if (!PeCabe(v, PE_OFF_SECOES, qtd * PE_TAM_SECAO_TAB))
        return "tabela de secoes truncada";
    for (uint32_t i = 0; i < qtd; i++) {
        const char *motivo = PeChecarSecao(v, PE_OFF_SECOES + i * PE_TAM_SECAO_TAB);
        if (motivo != NULL)
            return motivo;
    }
    return NULL;
}

static const char *PeChecarEntrada(const PeVisao *v, uint32_t qtd)
{
    uint32_t entrada = PeLer32(v, PE_OFF_OPCIONAL + PE_OPC_ENTRADA);
    for (uint32_t i = 0; i < qtd; i++) {
        uint32_t off = PE_OFF_SECOES + i * PE_TAM_SECAO_TAB;
        if (memcmp(v->d + off, PE_NOME_TEXTO, PE_TAM_NOME) != 0)
            continue;
        return PeDentroDe(entrada, PeLer32(v, off + 12), PeLer32(v, off + 8)) ? NULL : "entrada fora de .text";
    }
    return "secao .text ausente";
}

/* Diretorio com tamanho precisa cair inteiro dentro de uma secao. */
static bool PeEmSecao(const PeVisao *v, uint32_t qtd, uint32_t rva, uint32_t tam)
{
    for (uint32_t i = 0; i < qtd; i++) {
        uint32_t off = PE_OFF_SECOES + i * PE_TAM_SECAO_TAB;
        uint32_t sec_rva = PeLer32(v, off + 12), sec_tam = PeLer32(v, off + 8);
        if (rva >= sec_rva && rva - sec_rva < sec_tam && tam <= sec_tam - (rva - sec_rva))
            return true;
    }
    return false;
}

static const char *PeChecarDiretorios(const PeVisao *v, uint32_t qtd)
{
    for (uint32_t i = 0; i < PE_DIRETORIOS; i++) {
        uint32_t off = PE_OFF_DIRETORIOS + i * PE_TAM_DIRETORIO;
        uint32_t rva = PeLer32(v, off), tam = PeLer32(v, off + sizeof(uint32_t));
        if (tam != 0 && !PeEmSecao(v, qtd, rva, tam))
            return "diretorio fora de qualquer secao";
    }
    return NULL;
}

static const char *PeConferir(const PeVisao *v, uint32_t qtd)
{
    const char *motivo = PeChecarDos(v);
    if (motivo == NULL)
        motivo = PeChecarCoff(v);
    if (motivo == NULL)
        motivo = PeChecarSecoes(v, qtd);
    if (motivo == NULL)
        motivo = PeChecarEntrada(v, qtd);
    if (motivo == NULL)
        motivo = PeChecarDiretorios(v, qtd);
    return motivo;
}

static bool PeAutoconferir(const Buf *saida, uint32_t qtd, Diag *erro)
{
    PeVisao v = {(const uint8_t *)saida->dados, (uint32_t)saida->tam};
    const char *motivo = PeConferir(&v, qtd);
    if (motivo == NULL)
        return true;
    return DiagRelatar(erro, X0905, 0, "autoconferencia do PE: %s", motivo);
}

/* ---------- API ---------- */

bool PeEscrever(const Imagem *im, Buf *saida, Diag *erro)
{
    if (saida->tam != 0)
        FalhaInterna("pe: PeEscrever recebeu uma saida que nao esta vazia");
    if (im->qtd_secoes == 0)
        return DiagRelatar(erro, X0905, 0, "autoconferencia do PE: imagem sem secoes");
    Buf reloc = {0};
    PeLayout L;
    PeMontar(im, &reloc, &L);
    PeEscreverDos(saida);
    PeEscreverCabecalho(&L, im, saida);
    PeEscreverSecoes(&L, saida);
    if (L.tab_nomes.tam > 0)
        BufAnexarN(saida, L.tab_nomes.dados, L.tab_nomes.tam);
    if (L.cv != 0) {
        PeCodeViewGuid(saida, L.ptr[L.cv], im->cv_guid);
        BufLiberar(&L.secs[L.cv].bytes);
    }
    BufLiberar(&reloc);
    BufLiberar(&L.tab_nomes);
    return PeAutoconferir(saida, L.qtd, erro);
}
