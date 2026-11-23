/* pdb_msf.c - container MSF 7.0 do PDB (M6b P75, D-M6-9): superbloco, mapa de blocos livres, diretorio de fluxos e mapa do diretorio.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M6b P75: primeira versao. Bloco de 4096; o mapa livre fica no bloco 2 (FreeBlockMapBlock = 2, como o lld; o bloco 1 e reservado e zerado);
 *                    fluxos contiguos a partir do bloco 3, depois o diretorio e por ultimo o bloco do mapa do diretorio.
 * Verificacao: python ..\tests\nativo\m6b\ler_pdb.py <arquivo.pdb>
 */
#include "nativo/dbg/pdb.h"

#define MSF_BLOCO_MAPA_LIVRE 2u
#define MSF_PRIMEIRO_BLOCO_LIVRE 3u
#define MSF_MAX_BLOCOS (PDB_TAM_BLOCO * 8u)     /* um bloco de mapa livre cobre 32768 blocos */
#define MSF_MAX_BLOCOS_DIRETORIO (PDB_TAM_BLOCO / 4u)
#define MSF_TAM_MAGICA 32u

static const char MSF_MAGICA[MSF_TAM_MAGICA + 1] = "Microsoft C/C++ MSF 7.00\r\n\x1a" "DS\0\0\0";

void PdbU16(Buf *b, uint32_t v)
{
    char c[2] = {(char)v, (char)(v >> 8)};

    BufAnexarN(b, c, sizeof c);
}

void PdbU32(Buf *b, uint32_t v)
{
    char c[4] = {(char)v, (char)(v >> 8), (char)(v >> 16), (char)(v >> 24)};

    BufAnexarN(b, c, sizeof c);
}

void PdbMsfIniciar(PdbMsf *m)
{
    memset(m, 0, sizeof *m);
}

void PdbMsfLiberar(PdbMsf *m)
{
    for (uint32_t i = 0; i < m->qtd; i++)
        BufLiberar(&m->fluxos[i]);
    m->qtd = 0;
}

Buf *PdbMsfNovoFluxo(PdbMsf *m)
{
    if (m->qtd >= PDB_MAX_FLUXOS)
        FalhaInterna("pdb: o MSF passou de %u fluxos", PDB_MAX_FLUXOS);
    return &m->fluxos[m->qtd++];
}

static uint32_t MsfBlocos(uint64_t bytes)
{
    return (uint32_t)((bytes + PDB_TAM_BLOCO - 1) / PDB_TAM_BLOCO);
}

/* Onde cada coisa cai: primeiro bloco de cada fluxo, do diretorio e do mapa do diretorio, e o total de blocos do arquivo. */
typedef struct {
    uint32_t primeiro[PDB_MAX_FLUXOS];
    uint32_t dir_bytes, dir_primeiro, dir_blocos;
    uint32_t mapa_bloco, total;
} MsfPlano;

static bool MsfPlanejar(const PdbMsf *m, MsfPlano *p)
{
    uint32_t prox = MSF_PRIMEIRO_BLOCO_LIVRE;

    p->dir_bytes = 4 + 4 * m->qtd;
    for (uint32_t i = 0; i < m->qtd; i++) {
        uint32_t n = MsfBlocos(m->fluxos[i].tam);
        p->primeiro[i] = prox;
        prox += n;
        p->dir_bytes += 4 * n;
    }
    p->dir_blocos = MsfBlocos(p->dir_bytes);
    p->dir_primeiro = prox;
    p->mapa_bloco = prox + p->dir_blocos;
    p->total = p->mapa_bloco + 1;
    return p->dir_blocos <= MSF_MAX_BLOCOS_DIRETORIO && p->total <= MSF_MAX_BLOCOS;
}

static void MsfGravar32(char *dst, uint32_t v)
{
    for (uint32_t k = 0; k < 4; k++)
        dst[k] = (char)(v >> (8 * k));
}

static void MsfEscreverSuperbloco(char *arq, const MsfPlano *p)
{
    memcpy(arq, MSF_MAGICA, MSF_TAM_MAGICA);
    MsfGravar32(arq + 32, PDB_TAM_BLOCO);
    MsfGravar32(arq + 36, MSF_BLOCO_MAPA_LIVRE);
    MsfGravar32(arq + 40, p->total);
    MsfGravar32(arq + 44, p->dir_bytes);
    MsfGravar32(arq + 48, 0);
    MsfGravar32(arq + 52, p->mapa_bloco);
}

/* Bit k do mapa = bloco k livre (1) ou em uso (0); todo bloco do arquivo esta em uso, os que passam do fim ficam livres. */
static void MsfEscreverMapaLivre(char *bloco, uint32_t total)
{
    memset(bloco, 0xFF, PDB_TAM_BLOCO);
    for (uint32_t k = 0; k < total; k++)
        bloco[k / 8] &= (char)~(1u << (k % 8));
}

/* Diretorio: NumStreams, tamanho de cada fluxo e a lista de blocos de cada um (os fluxos sao contiguos). */
static void MsfEscreverDiretorio(const PdbMsf *m, const MsfPlano *p, char *dir)
{
    uint32_t k = 0;

    MsfGravar32(dir + 4 * k++, m->qtd);
    for (uint32_t i = 0; i < m->qtd; i++)
        MsfGravar32(dir + 4 * k++, (uint32_t)m->fluxos[i].tam);
    for (uint32_t i = 0; i < m->qtd; i++) {
        for (uint32_t b = 0; b < MsfBlocos(m->fluxos[i].tam); b++)
            MsfGravar32(dir + 4 * k++, p->primeiro[i] + b);
    }
}

bool PdbMsfSerializar(const PdbMsf *m, Buf *saida)
{
    static const char zeros[PDB_TAM_BLOCO];
    MsfPlano p;

    if (saida->tam != 0)
        FalhaInterna("pdb: PdbMsfSerializar recebeu uma saida que nao esta vazia");
    if (!MsfPlanejar(m, &p))
        return false;
    for (uint32_t b = 0; b < p.total; b++)
        BufAnexarN(saida, zeros, sizeof zeros);
    char *arq = saida->dados;
    MsfEscreverSuperbloco(arq, &p);
    MsfEscreverMapaLivre(arq + (size_t)MSF_BLOCO_MAPA_LIVRE * PDB_TAM_BLOCO, p.total);
    for (uint32_t i = 0; i < m->qtd; i++) {
        if (m->fluxos[i].tam > 0)
            memcpy(arq + (size_t)p.primeiro[i] * PDB_TAM_BLOCO, m->fluxos[i].dados, m->fluxos[i].tam);
    }
    MsfEscreverDiretorio(m, &p, arq + (size_t)p.dir_primeiro * PDB_TAM_BLOCO);
    for (uint32_t b = 0; b < p.dir_blocos; b++)
        MsfGravar32(arq + (size_t)p.mapa_bloco * PDB_TAM_BLOCO + 4 * b, p.dir_primeiro + b);
    return true;
}
