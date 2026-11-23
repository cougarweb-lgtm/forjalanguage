/* pdb_dbi.c - fluxo DBI do PDB (M6b P76a, D-M6-9): cabecalho de 64 bytes e os substreams ModInfo, SectionContribution, SectionMap, FileInfo, TypeServer, EC e OptionalDbgHeader,
 * com 1 modulo (o programa Forja) e a copia dos cabecalhos de secao do exe.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6b P76c: FileInfo com os arquivos do modulo (FileNameOffsets + NamesBuffer = o bloco do /names) e ArquivosFonte do ModInfo.
 *            0.2.0 - M6b P76c: ModInfo com TamanhoSimbolos e C13ByteSize do modulo (PdbTamanhosModulo, vindo de PdbFluxoModulo).
 *            0.1.0 - M6b P76a: primeira versao (o DBI vazio saiu de pdb_escrever.c). Sem GSI/publicos (fluxos invalidos): se o dbghelp pedir, e o P76b.
 * Verificacao: python ..\tests\nativo\m6b\ler_pdb.py <arquivo.pdb> --exigir-modulo
 */
#include "nativo/dbg/pdb.h"

#define PDB_FLUXO_NULO 0xFFFFu
#define PDB_DBI_VERSAO_70 19990903u
#define PDB_DBI_CONSTRUCAO 0x8E00u        /* novo formato, versao 14.0 */
#define PDB_MAQUINA_AMD64 0x8664u
#define PDB_FLUXO_MODULO 6u
#define PDB_FLUXO_SECOES 7u
#define PDB_SC_VERSAO 0xF12EBA2Du
#define PDB_SC_TAM 28u
#define PDB_SC_CARACTERISTICAS 0x60000020u
#define PDB_MODINFO_FIXO 64u
#define PDB_SECAO_LEITURA 0x40000000u     /* IMAGE_SCN_MEM_READ, WRITE e EXECUTE */
#define PDB_SECAO_ESCRITA 0x80000000u
#define PDB_SECAO_EXECUCAO 0x20000000u
#define PDB_MAPA_BASE 0x0108u             /* fSel | f32Bit; somam-se R=1, W=2, X=4 */
#define PDB_MAPA_ABSOLUTA 0x0208u
#define PDB_MAPA_SEM_NOME 0xFFFFu
#define PDB_OPCIONAL_QTD 11u
#define PDB_OPCIONAL_SECTION_HDR 5u

static uint32_t Le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void PdbSecoesLer(const Buf *exe, PdbSecoes *s)
{
    const uint8_t *d = (const uint8_t *)exe->dados;
    uint32_t pe, tam_opcional;

    s->cabecalhos = NULL;
    s->qtd = 0;
    s->texto = 0;
    if (exe->tam < 0x40)
        FalhaInterna("pdb: exe de %zu bytes sem cabecalho PE", exe->tam);
    pe = Le32(d + 0x3C);
    if (pe + 24 > exe->tam)
        FalhaInterna("pdb: e_lfanew %u fora do exe", pe);
    s->qtd = (uint32_t)d[pe + 6] | (uint32_t)d[pe + 7] << 8;
    tam_opcional = (uint32_t)d[pe + 20] | (uint32_t)d[pe + 21] << 8;
    if (pe + 24 + tam_opcional + (size_t)PDB_TAM_CABECALHO_SECAO * s->qtd > exe->tam)
        FalhaInterna("pdb: a tabela de secoes do exe passa do arquivo");
    s->cabecalhos = d + pe + 24 + tam_opcional;
    for (uint32_t k = 0; k < s->qtd && s->texto == 0; k++)
        if (memcmp(s->cabecalhos + PDB_TAM_CABECALHO_SECAO * k, ".text\0\0\0", 8) == 0)
            s->texto = k + 1;
    if (s->texto == 0)
        FalhaInterna("pdb: o exe nao tem secao .text");
}

/* A SectionContribution do modulo: a .text do programa, do offset 0 por codigo_tam bytes. */
static void PdbContribuicao(Buf *b, const PdbParametros *p, const PdbSecoes *s)
{
    PdbU16(b, s->texto);
    PdbU16(b, 0);
    PdbU32(b, 0);                                     /* Offset */
    PdbU32(b, p->codigo_tam);
    PdbU32(b, PDB_SC_CARACTERISTICAS);
    PdbU16(b, 0);                                     /* Imod */
    PdbU16(b, 0);
    PdbU32(b, 0);                                     /* DataCrc */
    PdbU32(b, 0);                                     /* RelocCrc */
}

/* ModInfo: 64 bytes fixos (a SectionContribution embutida entra depois de 4 bytes de Unused1) e ModuleName + ObjFileName, com padding ate multiplo de 4. */
static void PdbModInfo(Buf *b, const PdbParametros *p, const PdbSecoes *s, const PdbTamanhosModulo *t)
{
    size_t inicio = b->tam;
    size_t nome = strlen(p->modulo) + 1;

    PdbU32(b, 0);
    PdbContribuicao(b, p, s);
    PdbU16(b, 0);                                     /* Flags */
    PdbU16(b, PDB_FLUXO_MODULO);
    PdbU32(b, t->simbolos);
    PdbU32(b, 0);                                     /* C11ByteSize */
    PdbU32(b, t->c13);                                /* C13ByteSize: FILECHKSMS e DEBUG_S_LINES do modulo (P76c) */
    PdbU16(b, (uint16_t)t->arquivos->lista.n);       /* arquivos fonte do modulo */
    PdbU16(b, 0);
    PdbU32(b, 0);
    PdbU32(b, 0);                                     /* SourceFileNameIndex */
    PdbU32(b, 0);                                     /* PdbFilePathNameIndex */
    if (b->tam - inicio != PDB_MODINFO_FIXO)
        FalhaInterna("pdb: ModInfo com %zu bytes fixos (esperado %u)", b->tam - inicio, PDB_MODINFO_FIXO);
    BufAnexarN(b, p->modulo, nome);
    BufAnexarN(b, p->modulo, nome);
    BufAnexarN(b, "\0\0\0", (4 - (b->tam & 3)) & 3);
}

static void PdbSectionMap(Buf *b, const PdbSecoes *s)
{
    PdbU16(b, s->qtd + 1);                            /* Count e LogCount: as secoes e a entrada absoluta */
    PdbU16(b, s->qtd + 1);
    for (uint32_t k = 0; k < s->qtd; k++) {
        const uint8_t *h = s->cabecalhos + PDB_TAM_CABECALHO_SECAO * k;
        uint32_t caract = Le32(h + 36);
        uint32_t flags = PDB_MAPA_BASE | ((caract & PDB_SECAO_LEITURA) ? 1u : 0u) | ((caract & PDB_SECAO_ESCRITA) ? 2u : 0u)
                         | ((caract & PDB_SECAO_EXECUCAO) ? 4u : 0u);

        PdbU16(b, flags);
        PdbU16(b, 0);                                 /* Ovl */
        PdbU16(b, 0);                                 /* Group */
        PdbU16(b, k + 1);                             /* Frame */
        PdbU16(b, PDB_MAPA_SEM_NOME);
        PdbU16(b, PDB_MAPA_SEM_NOME);
        PdbU32(b, 0);                                 /* Offset */
        PdbU32(b, Le32(h + 8));                       /* VirtualSize */
    }
    PdbU16(b, PDB_MAPA_ABSOLUTA);
    PdbU16(b, 0);
    PdbU16(b, 0);
    PdbU16(b, s->qtd + 1);
    PdbU16(b, PDB_MAPA_SEM_NOME);
    PdbU16(b, PDB_MAPA_SEM_NOME);
    PdbU32(b, 0);
    PdbU32(b, 0xFFFFFFFFu);
}

/* FileInfo: 1 modulo com os seus arquivos (NumModules, NumSourceFiles, ModIndices[1], ModFileCounts[1], FileNameOffsets[NumSourceFiles] e o NamesBuffer,
 * que e o bloco de nomes do /names: os offsets apontam para ele). Sem arquivo: so o cabecalho de 8 B. */
static void PdbFileInfo(Buf *b, const PdbArquivos *a)
{
    uint16_t qtd = (uint16_t)a->lista.n;

    PdbU16(b, 1);
    PdbU16(b, qtd);
    PdbU16(b, 0);                                     /* ModIndices[0]: o modulo 0 */
    PdbU16(b, qtd);                                   /* ModFileCounts[0] */
    if (qtd == 0)
        return;
    for (uint16_t k = 0; k < qtd; k++)
        PdbU32(b, ((const PdbArquivo *)VecEm(&a->lista, k))->nome_off);
    BufAnexarN(b, a->nomes.dados, a->nomes.tam);
    BufAnexarN(b, "\0\0\0", (4 - (b->tam & 3)) & 3);   /* o FileInfo fecha em multiplo de 4, como os outros substreams do DBI */
}

static void PdbOpcional(Buf *b)
{
    for (uint32_t k = 0; k < PDB_OPCIONAL_QTD; k++)
        PdbU16(b, k == PDB_OPCIONAL_SECTION_HDR ? PDB_FLUXO_SECOES : PDB_FLUXO_NULO);
}

void PdbFluxoDbi(Buf *b, const PdbParametros *p, const PdbSecoes *s, const PdbTamanhosModulo *t)
{
    Buf corpo = {0};
    uint32_t tam[5];                                  /* ModInfo, SectionContribution, SectionMap, FileInfo, OptionalDbgHeader */

    PdbModInfo(&corpo, p, s, t);
    tam[0] = (uint32_t)corpo.tam;
    PdbU32(&corpo, PDB_SC_VERSAO);
    PdbContribuicao(&corpo, p, s);
    tam[1] = (uint32_t)corpo.tam - tam[0];
    PdbSectionMap(&corpo, s);
    tam[2] = (uint32_t)corpo.tam - tam[0] - tam[1];
    PdbFileInfo(&corpo, t->arquivos);
    tam[3] = (uint32_t)corpo.tam - tam[0] - tam[1] - tam[2];
    PdbOpcional(&corpo);
    tam[4] = (uint32_t)corpo.tam - tam[0] - tam[1] - tam[2] - tam[3];

    PdbU32(b, 0xFFFFFFFFu);                           /* VersionSignature */
    PdbU32(b, PDB_DBI_VERSAO_70);
    PdbU32(b, p->idade);
    PdbU16(b, PDB_FLUXO_NULO);                        /* GlobalStreamIndex */
    PdbU16(b, PDB_DBI_CONSTRUCAO);
    PdbU16(b, PDB_FLUXO_NULO);                        /* PublicStreamIndex */
    PdbU16(b, 0);                                     /* PdbDllVersion */
    PdbU16(b, PDB_FLUXO_NULO);                        /* SymRecordStreamIndex */
    PdbU16(b, 0);                                     /* PdbDllRbld */
    PdbU32(b, tam[0]);
    PdbU32(b, tam[1]);
    PdbU32(b, tam[2]);
    PdbU32(b, tam[3]);
    PdbU32(b, 0);                                     /* TypeServerMap */
    PdbU32(b, 0);                                     /* MFCTypeServerIndex */
    PdbU32(b, tam[4]);                                /* OptionalDbgHeader */
    PdbU32(b, 0);                                     /* ECSubstream */
    PdbU16(b, 0);                                     /* Flags */
    PdbU16(b, PDB_MAQUINA_AMD64);
    PdbU32(b, 0);                                     /* Padding */
    BufAnexarN(b, corpo.dados, corpo.tam);
    BufLiberar(&corpo);
}
