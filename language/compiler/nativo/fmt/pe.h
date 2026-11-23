/* pe.h - imagem do executavel em memoria (neutra de formato desde o M5 P27a; o escritor PE mora aqui): secoes, importacoes de kernel32 e a escrita do arquivo com autoconferencia.
 * Versao: 0.15.0
 * Changelog: 0.15.0 - M6b P75: Imagem.pdb_caminho e cv_guid (PE com .pdb: secao ".cvinfo" com a IMAGE_DEBUG_DIRECTORY e o RSDS; DataDirectory[DEBUG]) e PeCodeViewSecao/
 *                    PeCodeViewPonteiro/PeCodeViewGuid (pe_secoes.c). Sem pdb_caminho nenhum byte do PE muda.
 *            0.14.0 - M6 P68: IMAGEM_MAX_SECOES 9 -> 13 (as secoes de debug do PE, em Imagem.dbg, entram no cabecalho sem apertar o teto do programa);
 *                    PeNomeSecaoDebug, PeNomeSecaoLongo e PeTabelaFechar (nomes de mais de 8 bytes pela tabela de strings COFF, em pe_secoes.c).
 *            0.13.0 - M6 P68 (extracao): PE_ALINHAMENTO_SECAO e PE_TAM_NOME (macros), PeAlinhar (inline) e PeNomeSecao (declarada; definida em pe_secoes.c)
 *                    saem de pe_escrever.c para ca; nenhum byte muda.
 *            0.12.0 - M6 P66: Imagem.dbg (4 Buf: .debug_abbrev, .debug_info, .debug_line e .debug_frame do programa, na ordem de ImagemDbgIndice); so o ELF preenche
 *                    (secoes nao alocadas, fora dos PT_LOAD: D-M6-4). Tamanho 0 = secao ausente (--simbolos nao, ou PE, que entra no P68).
 *            0.11.0 - M5c-b P61: Imagem.tls, tls_secao e tls_alinhamento (o PT_TLS do ELF); o PE nao preenche (sem TLS, nenhum byte muda).
 *            0.10.0 - M5b P41: Imagem.pie (ELF static-pie, --pie: base 0 e relocacao RELATIVE) e Imagem.relativos (ImagemRelativa: RVA do campo e valor de 64 bits de cada ADDR64 vivo); PeBaseDaImagem devolve 0 nesse caso.
 *            0.9.2 - M5 P53: IMAGEM_MAX_DLLS de 17 para 18 (a WS2_32 do std:net e a 3a tabela fixa, so de quem usa).
 *            0.9.1 - POS-Z A52: IMAGEM_MAX_DLLS de 16 para 17 (a ADVAPI32 do runtime e a 2a DLL fixa; o teto de DLLs de 'link' do usuario segue 15) e IMAGEM_MAX_IMPORTS de 256 para 320.
 *            0.9.0 - M5 P27a: Imagem neutra: ImagemFormato (IMAGEM_PE | IMAGEM_ELF), Imagem.formato, IMAGEM_BASE_ELF (0x400000), ImagemSimbolo e Imagem.simbolos (so o ligador de ELF preenche);
 *                    PeBaseDaImagem devolve a base do formato. O PE (dll, ImageBase, .reloc, .idata) segue como estava, byte a byte.
 *            0.8.0 - M3 P22a: DLL. Imagem.dll (IMAGE_FILE_DLL, ImageBase PE_IMAGE_BASE_DLL = 0x180000000 como o ld; PeBaseDaImagem), IMAGEM_MAX_SECOES 8 -> 9 (a .edata) e
 *                    PeReservarExportacoes (pe_exportar.c): export directory com nomes em ordem ASCII, ordinais base 1, EAT/ENPT/EOT numa secao ".edata".
 *            0.7.0 - M3 P19a: N DLLs no .idata: PeImportDll e PeReservarImportacoesDlls (PeReservarImportacoes vira o caso de uma DLL); ImagemImportacao.dlls
 *                    (nome, primeiro e quantidade de cada DLL, na ordem dos descritores); IMAGEM_MAX_IMPORTS de 16 para 256 e IMAGEM_MAX_DLLS 16.
 *            0.6.0 -M2 P17c: ImagemImportacao.nomes (os nomes passados a PeReservarImportacoes, para o --debug-code listar so o que o exe importa) e
 *                    Imagem.runtime_texto_rva (o runtime ja nao e o fim inteiro do .text: so as funcoes alcancadas entram).
 *            0.5.0 - M2 P9a fase 2: ImagemSecao recebe Diag e recusa secao sem bytes (X0905 "secao '<nome>' sem bytes", devolve 0).
 *            0.4.0 - M2 P9a: ImagemSecao recebe PeSecaoPedido (4 campos em vez de 5 parametros; regra de <= 4 parametros).
 *            0.3.1 - portao M1: PE_SECAO_* (caracteristicas de secao) passam para ca, fonte unica de pe_escrever.c e ligar.c.
 *            0.3.0 - backend x64 P7b: Imagem.linker_maior/linker_menor (MajorLinkerVersion/MinorLinkerVersion: a versao do forja, nunca a do ld) e
 *                    o diretorio de recursos (indice 2) aponta para a secao ".rsrc" quando ela existe.
 *            0.2.0 - backend x64 P5: PE_IMAGE_BASE sai de pe_escrever.c para o ligador usar.
 *            0.1.0 - backend x64 P3: ImagemIniciar/ImagemLiberar, ImagemSecao (RVA sequencial a 0x1000), PeReservarImportacoes
 *                    (.idata) e PeEscrever (cabecalhos copiados do exe do ld; X0905 quando a autoconferencia recusa).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\pe\verificar_pe.py
 */
#ifndef FORJA_NATIVO_FMT_PE_H
#define FORJA_NATIVO_FMT_PE_H

#include "common/buf.h"
#include "common/diag.h"
#include "common/vec.h"

#define IMAGEM_MAX_SECOES 13
#define IMAGEM_MAX_IMPORTS 320
#define IMAGEM_MAX_DLLS 17
/* ImageBase preferida do exe (0x140000000): o ligador grava VA absoluta nos campos ADDR64 com ela. A da DLL e 0x180000000 (a do ld). */
#define PE_IMAGE_BASE 0x140000000ull
#define PE_IMAGE_BASE_DLL 0x180000000ull
/* Base do ELF ET_EXEC estatico (D-M5-1): as secoes ficam em RVA a partir de 0x1000 (a pagina 0 e dos cabecalhos) e o ELF soma esta base. */
#define IMAGEM_BASE_ELF 0x400000ull
/* Secoes do PE/ELF: alinhamento de RVA (0x1000) e tamanho do nome de 8 bytes (o nome sem NUL conta os 8). */
#define PE_ALINHAMENTO_SECAO 0x1000u
#define PE_TAM_NOME 8u

/* O formato do executavel que a Imagem vira: so o escritor (PeEscrever, ou o do ELF) sabe os cabecalhos; o ligador so escolhe base, secoes e o que descarta. */
typedef enum { IMAGEM_PE, IMAGEM_ELF } ImagemFormato;

/* Caracteristicas de secao (IMAGE_SCN_*): os bits de conteudo (CODIGO, DADOS_INIT, NAO_INIT) somam o SizeOfCode/Initialized/Uninitialized do
 * cabecalho; o resto sao as combinacoes que o ligador e o PeEscrever gravam. Fonte unica (ligar.c e pe_escrever.c). */
#define PE_SECAO_CODIGO 0x00000020u
#define PE_SECAO_DADOS_INIT 0x00000040u
#define PE_SECAO_NAO_INIT 0x00000080u
#define PE_SECAO_TEXTO 0x60000020u    /* CODE | EXECUTE | READ */
#define PE_SECAO_LEITURA 0x40000040u  /* INITIALIZED_DATA | READ */
#define PE_SECAO_DADOS 0xC0000040u    /* INITIALIZED_DATA | READ | WRITE */
#define PE_SECAO_BSS 0xC0000080u      /* UNINITIALIZED_DATA | READ | WRITE */
#define PE_SECAO_IDATA PE_SECAO_DADOS
#define PE_SECAO_RELOC 0x42000040u    /* INITIALIZED_DATA | DISCARDABLE | READ */

/* Um simbolo que a DLL exporta: nome (ponteiro do chamador, vive ate o fim da Imagem) e o RVA da funcao. */
typedef struct {
    const char *nome;
    uint32_t rva;
} PeExporta;

typedef struct {
    char nome[8];              /* ate 8 caracteres; zero a direita quando o nome e menor */
    uint32_t caracteristicas;
    uint32_t rva;
    uint32_t tam_virtual;      /* nunca menor que bytes.tam */
    Buf bytes;                 /* conteudo gravado no arquivo; vazio em .bss */
} PeSecao;

/* Pedido de uma secao nova para ImagemSecao. bytes NULL = secao sem conteudo (como .bss). */
typedef struct {
    const char *nome;          /* ate 8 caracteres (NUL opcional) */
    uint32_t caracteristicas;
    const Buf *bytes;
    uint32_t tam_virtual;     /* tamanho em memoria; nunca menor que o de bytes */
} PeSecaoPedido;

/* Uma DLL importada: os nomes dela sao os de iat_rva/nomes de primeiro ate primeiro + qtd - 1 (ponteiro do chamador, que vive ate o fim da Imagem). */
typedef struct {
    const char *nome;
    uint32_t primeiro, qtd;
} ImagemDll;

typedef struct {
    uint32_t dir_import_rva, dir_import_tam;
    uint32_t dir_iat_rva, dir_iat_tam;
    uint32_t iat_rva[IMAGEM_MAX_IMPORTS];  /* RVA da entrada IAT de cada nome, DLL por DLL, na ordem passada a PeReservarImportacoes* */
    const char *nomes[IMAGEM_MAX_IMPORTS]; /* os mesmos nomes (ponteiros do chamador, que tem de viver ate o fim da Imagem) */
    uint32_t qtd;
    ImagemDll dlls[IMAGEM_MAX_DLLS];       /* na ordem dos descritores do .idata */
    uint32_t qtd_dlls;
} ImagemImportacao;

/* Simbolo definido e vivo da imagem (so no ELF, para o .symtab): o nome vive no Objeto de origem (ate o fim da montagem) e secao e o indice em Imagem.secoes. */
typedef struct {
    const char *nome;
    uint32_t rva;
    uint32_t secao;
} ImagemSimbolo;

/* Um campo ADDR64 vivo de um ELF static-pie (P41): o RVA do campo e o valor de 64 bits gravado (com base 0, e o adendo do R_X86_64_RELATIVE). */
typedef struct {
    uint32_t rva;
    uint64_t valor;
} ImagemRelativa;

/* Secoes de debug do ELF (M6 P66): o indice e o do tipo OS_DEBUG_* menos OS_DEBUG_ABBREV. */
#define IMAGEM_QTD_DEBUG 4

typedef struct {
    PeSecao secoes[IMAGEM_MAX_SECOES];
    Buf dbg[IMAGEM_QTD_DEBUG]; /* bytes das secoes de debug (ELF, M6 P66); tam 0 = ausente */
    uint32_t qtd_secoes;
    uint32_t entrada_rva;      /* AddressOfEntryPoint: tem de cair dentro de .text */
    uint32_t runtime_texto_rva;  /* RVA onde comeca o que o runtime pos no .text (o ligador grava; so o --debug-code le) */
    uint8_t linker_maior, linker_menor;  /* quem gerou o exe: versao do forja (0 = nao informado) */
    ImagemImportacao importacao;
    Vec dir64;                 /* uint32_t: RVAs dos campos ADDR64; viram o .reloc na escrita */
    bool dll;                  /* DLL: IMAGE_FILE_DLL e ImageBase PE_IMAGE_BASE_DLL (entrada_rva = o DllMain) */
    Vec exportacoes;           /* PeExporta em ordem ASCII (o ordinal do k-esimo e k + 1); preenchido por PeReservarExportacoes; os nomes sao do chamador */
    ImagemFormato formato;     /* IMAGEM_PE (0, o padrao de ImagemIniciar) ou IMAGEM_ELF: o ligador marca antes de aplicar as relocacoes */
    Vec simbolos;              /* ImagemSimbolo: so com IMAGEM_ELF (o PE nao grava simbolos), na ordem dos objetos */
    bool pie;                  /* ELF static-pie (--pie, P41): base 0, PT_DYNAMIC e .rela.dyn; so com IMAGEM_ELF */
    Vec relativos;             /* ImagemRelativa: os ADDR64 vivos (e as celulas .got) de um pie; vazio sem pie */
    bool tls;                  /* ELF com bloco TLS (M5c-b P61): PT_TLS aponta para a secao tls_secao (.tdata com o .tbss depois, em memsz) */
    uint32_t tls_secao;        /* indice em secoes da secao TLS (so com tls) */
    uint32_t tls_alinhamento;  /* p_align do PT_TLS: o maior alinhamento dos membros TLS (so com tls) */
    const char *pdb_caminho;   /* PE com .pdb (M6b P75, D-M6-9): caminho absoluto do .pdb, gravado no RSDS; NULL = sem CodeView (vive ate o fim da Imagem) */
    uint8_t *cv_guid;          /* NULL ou 16 bytes do chamador: PeEscrever grava aqui o GUID (hash do exe com o campo zerado) que pos no RSDS */
} Imagem;

/* Base da imagem: o ligador grava VA absoluta nos ADDR64 com ela e o escritor a poe no cabecalho (ImageBase do PE, endereco do 1o PT_LOAD do ELF). O pie do ELF e 0. */
static inline uint64_t PeBaseDaImagem(const Imagem *im)
{
    if (im->formato == IMAGEM_ELF)
        return im->pie ? 0 : IMAGEM_BASE_ELF;
    return im->dll ? PE_IMAGE_BASE_DLL : PE_IMAGE_BASE;
}

void ImagemIniciar(Imagem *im);
void ImagemLiberar(Imagem *im);

/* Acrescenta uma secao no proximo RVA (alinhado a 0x1000) e devolve esse RVA. pedido: ver PeSecaoPedido.
 * Secao com tamanho zero (sem bytes e sem tam_virtual) e recusada: erro recebe X0905 e a funcao devolve 0 (nunca e RVA de secao). */
uint32_t ImagemSecao(Imagem *im, const PeSecaoPedido *pedido, Diag *erro);

/* RVA em que a proxima secao vai ficar: quem monta bytes com enderecos absolutos (call [rip+IAT]) le isto antes. */
uint32_t ImagemProximoRva(const Imagem *im);

/* Copia o nome da secao para dst (8 bytes, zeros a direita); para no NUL ou em 8 caracteres (o nome pode nao ter NUL). */
void PeNomeSecao(char dst[PE_TAM_NOME], const char *src);

/* Arredonda v para cima ao multiplo de alinhamento (alinhamento > 0). */
static inline uint32_t PeAlinhar(uint32_t v, uint32_t alinhamento)
{
    return (v + alinhamento - 1) / alinhamento * alinhamento;
}

/* Nome da secao de debug k (indice em Imagem.dbg, D-M6-4); o PE nao tem .debug_frame (a CFI e do ELF; o PE usa .pdata). */
const char *PeNomeSecaoDebug(uint32_t k);

/* Nome de mais de 8 bytes (COFF): entra em tab (tabela de strings; a 1a vez ganha 4 bytes de tamanho) e dst recebe "/<offset>" em decimal. */
void PeNomeSecaoLongo(Buf *tab, const char *nome, char dst[PE_TAM_NOME]);

/* Grava o tamanho da tabela de strings nos 4 primeiros bytes (tabela vazia nao muda). */
void PeTabelaFechar(Buf *tab);

/* CodeView (P75): preenche s (zerada pelo chamador) com a secao ".cvinfo" em rva: IMAGE_DEBUG_DIRECTORY (28 B, tipo CODEVIEW) e o RSDS (GUID zero, idade 1, caminho pdb).
 * s->bytes passa a ser do chamador (BufLiberar). */
#define PE_CV_TAM_DIRETORIO 28u
#define PE_CV_IDADE 1u            /* idade do RSDS e do PDB: o forja grava os dois juntos, sempre 1 (build novo = GUID novo) */
void PeCodeViewSecao(PeSecao *s, uint32_t rva, const char *pdb);

/* Grava em bytes o PointerToRawData (RSDS) da secao ".cvinfo", que cai em ptr no arquivo. */
void PeCodeViewPonteiro(Buf *bytes, uint32_t ptr);

/* GUID do RSDS = hash de exe inteiro (com o campo zerado), gravado no arquivo (o RSDS comeca em ptr + 28) e copiado para saida (16 bytes, ou NULL). */
void PeCodeViewGuid(Buf *exe, uint32_t ptr, uint8_t *saida);

/* Cria a secao .idata (descritor, ILT, IAT e hint/name) para n nomes de 'dll' e preenche importacao e iat_rva.
 * Devolve false se n for 0 ou passar de IMAGEM_MAX_IMPORTS. */
bool PeReservarImportacoes(Imagem *im, const char *dll, const char *const *nomes, uint32_t n);

/* Uma DLL do pedido de PeReservarImportacoesDlls: o nome dela e os n nomes importados (n >= 1). */
typedef struct {
    const char *dll;
    const char *const *nomes;
    uint32_t n;
} PeImportDll;

/* Como PeReservarImportacoes, para n_dlls DLLs: um descritor por DLL, na ordem do pedido; dentro da DLL, os nomes na ordem do pedido. O .idata de uma DLL
 * so e identico ao de PeReservarImportacoes. Devolve false se n_dlls for 0 ou passar de IMAGEM_MAX_DLLS, se alguma DLL tiver n 0, ou se a soma dos
 * nomes passar de IMAGEM_MAX_IMPORTS. */
bool PeReservarImportacoesDlls(Imagem *im, const PeImportDll *dlls, uint32_t n_dlls);


/* Cria a secao ".edata" (export directory, EAT, ENPT, EOT, nomes) com os n simbolos em ordem ASCII (strcmp) e ordinais a partir de 1; Imagem.exportacoes guarda a lista
 * ordenada. Devolve false se n for 0 ou se houver nome repetido. A Imagem passa a ter dll = true (quem chama grava o resto). */
bool PeReservarExportacoes(Imagem *im, const char *nome_dll, const PeExporta *simbolos, uint32_t n);

/* Escreve o PE32+ em saida (que tem de vir vazio) e rele o que gravou: X0905 se a autoconferencia nao bater. */
bool PeEscrever(const Imagem *im, Buf *saida, Diag *erro);

#endif
