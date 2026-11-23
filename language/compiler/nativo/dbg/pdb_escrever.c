/* pdb_escrever.c - fluxos do PDB (M6b P75, D-M6-9): 1 info (versao, GUID, idade, mapa de nomes), 2 TPI, 3 DBI, 4 IPI e 5 /names (com os arquivos .fj das linhas),
 * e PdbEscrever, que monta tudo na ordem dos fluxos.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M6b P77b: o TPI (fluxo 2) leva os registros LF_* que o modulo pediu (PdbTipos: TypeIndexEnd e TypeRecordBytes do que foi gravado); o IPI segue vazio.
 *                    TPI com registros leva o fluxo 8 (hash u32 por tipo, zerado, e IndexOffsetBuffer de 8 em 8 KiB): sem ele o dbghelp recusa o PDB (SymLoadModuleEx = 0), provado com
 *                    1 LF_ARRAY (sem fluxo, ou so com hashes: falha; hashes + (0x1000, 0): carrega). O hash real (resolucao de fwdref pelo nome) fica para depois.
 *            0.3.3 - M6b P76d: a busca de linha pela dica foi para DbgPosicaoComDica (dbg_coletar.c), compartilhada com o DWARF; PDB igual.
 *            0.3.2 - M6b P76c: a linha de cada DbgLinha sai da dica (a linha anterior ou a seguinte, em inicio_linha) antes da busca binaria da Fonte (colher 10 -> 5,5 ms
 *                    no corpus_200000; PDB byte a byte igual).
 *            0.3.1 - M6b P76c: PdbArquivosColher guarda PdbPosLinha por DbgLinha (a Fonte e consultada uma vez por linha; o C13 nao busca de novo).
 *            0.3.0 - M6b P76c: PdbArquivosColher (os caminhos das linhas, por ponteiro, antes do /names) e o /names com a tabela de baldes dos arquivos (HashV1,
 *                    sondagem linear, contagem no fim); PdbEscrever passa a lista ao modulo e o tamanho do C13 ao DBI.
 *            0.2.0 - M6b P76a: o DBI saiu para pdb_dbi.c; PdbEscrever monta tambem o fluxo 6 (modulo, pdb_modulo.c) e o 7 (copia dos cabecalhos de secao do exe).
 *            0.1.0 - M6b P75: primeira versao. TPI e IPI sem registros, DBI sem modulos e sem fluxos de simbolos (todos os indices invalidos): o PDB so casa com o exe.
 *                    Modulo, procs e linhas entram no P76; tipos e locais no P77.
 * Verificacao: python ..\tests\nativo\m6b\verificar_pdb.py --modo modulo --exe <exe do forja> --exigir-pdb  e  python ..\tests\nativo\m6b\ler_pdb.py <arquivo.pdb>
 */
#include "nativo/dbg/pdb.h"

enum { FLUXO_NOMES = 5 };                         /* 0 diretorio antigo, 1 info, 2 TPI, 3 DBI, 4 IPI, 5 /names (a ordem de PdbEscrever) */

#define PDB_VERSAO_INFO 20000404u         /* VC70 */
#define PDB_CARACTERISTICA_VC140 20140508u
#define PDB_VERSAO_TPI 20040203u          /* V80 */
#define PDB_TAM_CABECALHO_TPI 56u
#define PDB_FLUXO_NULO 0xFFFFu
#define PDB_TPI_TAM_CHAVE 4u
#define PDB_TPI_BUCKETS 0x3FFFFu
#define PDB_TPI_PASSO_INDICE 8192u        /* IndexOffsetBuffer: um par (tipo, offset) a cada 8 KiB de registros, como o LLVM */
#define PDB_NOMES_ASSINATURA 0xEFFEEFFEu
#define PDB_NOMES_HASH_V1 1u
#define PDB_NOMES_CAPACIDADE 8u           /* buckets do mapa de fluxos nomeados; o entry cai em hash % capacidade */
#define PDB_NOMES_BALDES_POR_ARQUIVO 2u   /* /names: 2 baldes por nome + 1 (sobra balde livre: a sondagem linear sempre acha vaga) */
#define PDB_NOME_DO_FLUXO_NOMES "/names"

/* Hash V1 do LLVM/MS (hashStringV1): o mapa de nomes do info acha "/names" por ele; a tabela do /names acha cada arquivo. */
static uint32_t PdbHashV1(const char *s)
{
    size_t n = strlen(s);
    uint32_t r = 0;
    size_t k = 0;

    for (; k + 4 <= n; k += 4)
        r ^= (uint32_t)(uint8_t)s[k] | (uint32_t)(uint8_t)s[k + 1] << 8 | (uint32_t)(uint8_t)s[k + 2] << 16 | (uint32_t)(uint8_t)s[k + 3] << 24;
    if (n - k >= 2) {
        r ^= (uint32_t)(uint8_t)s[k] | (uint32_t)(uint8_t)s[k + 1] << 8;
        k += 2;
    }
    if (k < n)
        r ^= (uint32_t)(uint8_t)s[k];
    r |= 0x20202020u;
    r ^= r >> 11;
    return r ^ (r >> 16);
}

/* Tabela hash do mapa de fluxos nomeados com uma entrada (nome -> indice de fluxo): Size, Capacity, bits presentes, bits removidos, pares chave/valor. */
static void PdbMapaNomeado(Buf *b)
{
    uint32_t bucket = PdbHashV1(PDB_NOME_DO_FLUXO_NOMES) % PDB_NOMES_CAPACIDADE;

    PdbU32(b, sizeof PDB_NOME_DO_FLUXO_NOMES);        /* tamanho do bloco de nomes, com o NUL */
    BufAnexarN(b, PDB_NOME_DO_FLUXO_NOMES, sizeof PDB_NOME_DO_FLUXO_NOMES);
    PdbU32(b, 1);                                     /* Size: entradas presentes */
    PdbU32(b, PDB_NOMES_CAPACIDADE);
    PdbU32(b, 1);                                     /* presentes: 1 palavra */
    PdbU32(b, 1u << bucket);
    PdbU32(b, 0);                                     /* removidos: nenhuma palavra */
    PdbU32(b, 0);                                     /* chave: deslocamento do nome no bloco de nomes */
    PdbU32(b, FLUXO_NOMES);                           /* valor: o fluxo */
}

static void PdbFluxoInfo(Buf *b, const PdbParametros *p)
{
    PdbU32(b, PDB_VERSAO_INFO);
    PdbU32(b, 0);                                     /* Signature (carimbo de tempo): 0, o PDB tem de ser reproduzivel */
    PdbU32(b, p->idade);
    BufAnexarN(b, (const char *)p->guid, sizeof p->guid);
    PdbMapaNomeado(b);
    PdbU32(b, PDB_CARACTERISTICA_VC140);
}

/* Pares (TypeIndex, offset do registro) de um em um passo de 8 KiB de registros, o 1o no offset 0 (o dbghelp so carrega o TPI com eles). Devolve quantos;
 * saida NULL so conta. */
static uint32_t PdbIndicesTpi(const PdbTipos *t, Buf *saida)
{
    uint32_t n = 0, proximo_corte = 0, indice = PDB_PRIMEIRO_TIPO;
    size_t pos = 0;

    while (pos < t->reg.tam) {
        if (pos >= proximo_corte) {
            if (saida != NULL) {
                PdbU32(saida, indice);
                PdbU32(saida, (uint32_t)pos);
            }
            n++;
            proximo_corte = (uint32_t)pos + PDB_TPI_PASSO_INDICE;
        }
        pos += 2 + ((uint8_t)t->reg.dados[pos] | (uint32_t)(uint8_t)t->reg.dados[pos + 1] << 8);
        indice++;
    }
    return n;
}

/* Fluxo de hash do TPI: um hash u32 por tipo (0: nada consulta o hash), depois os pares de PdbIndicesTpi. */
static void PdbFluxoHashTpi(Buf *b, const PdbTipos *t)
{
    for (uint32_t k = PDB_PRIMEIRO_TIPO; k < t->proximo; k++)
        PdbU32(b, 0);
    PdbIndicesTpi(t, b);
}

/* TPI (t = os registros do P77b) e IPI (t = NULL, sem registros): o cabecalho de 56 bytes, TypeIndexEnd e TypeRecordBytes do que t gravou, os registros logo depois;
 * com registros, o HashStreamIndex = hash_fluxo (PdbFluxoHashTpi). */
static void PdbFluxoTipos(Buf *b, const PdbTipos *t, uint32_t hash_fluxo)
{
    PdbU32(b, PDB_VERSAO_TPI);
    PdbU32(b, PDB_TAM_CABECALHO_TPI);
    PdbU32(b, PDB_PRIMEIRO_TIPO);
    uint32_t tipos = t != NULL ? t->proximo - PDB_PRIMEIRO_TIPO : 0;
    uint32_t indices = tipos != 0 ? PdbIndicesTpi(t, NULL) : 0;

    PdbU32(b, t != NULL ? t->proximo : PDB_PRIMEIRO_TIPO);
    PdbU32(b, t != NULL ? (uint32_t)t->reg.tam : 0);  /* TypeRecordBytes */
    PdbU16(b, tipos != 0 ? hash_fluxo : PDB_FLUXO_NULO);   /* HashStreamIndex: o dbghelp nao carrega TPI com registros sem ele */
    PdbU16(b, PDB_FLUXO_NULO);                        /* HashAuxStreamIndex */
    PdbU32(b, PDB_TPI_TAM_CHAVE);
    PdbU32(b, PDB_TPI_BUCKETS);
    PdbU32(b, 0);                                     /* HashValueBuffer: deslocamento e tamanho dentro do fluxo de hash */
    PdbU32(b, 4 * tipos);
    PdbU32(b, 4 * tipos);                             /* IndexOffsetBuffer */
    PdbU32(b, 8 * indices);
    PdbU32(b, 4 * tipos + 8 * indices);               /* HashAdjBuffer: vazio */
    PdbU32(b, 0);
    if (t != NULL)
        BufAnexarN(b, t->reg.dados, t->reg.tam);
}

/* Procura o caminho pelo ponteiro (a Fonte devolve o mesmo ponteiro para o mesmo trecho). O cache do ultimo achado poupa a varredura na maioria das linhas. */
static bool PdbArquivoAchar(PdbArquivos *a, const char *caminho, uint32_t *indice)
{
    if (a->ultimo < a->lista.n && ((const PdbArquivo *)VecEm(&a->lista, a->ultimo))->caminho == caminho) {
        *indice = (uint32_t)a->ultimo;
        return true;
    }
    for (size_t k = 0; k < a->lista.n; k++) {
        if (((const PdbArquivo *)VecEm(&a->lista, k))->caminho == caminho) {
            a->ultimo = k;
            *indice = (uint32_t)k;
            return true;
        }
    }
    return false;
}

static void PdbArquivoAcrescentar(PdbArquivos *a, const char *caminho)
{
    PdbArquivo *arq = VecEmpurrar(&a->lista);

    arq->caminho = caminho;
    arq->nome_off = (uint32_t)a->nomes.tam;
    BufAnexarN(&a->nomes, caminho, strlen(caminho) + 1);     /* o NUL entra: o /names guarda cadeias terminadas */
    a->ultimo = a->lista.n - 1;
}

/* Uma passada pelas DbgLinha: a Fonte e consultada aqui, uma vez por linha; o C13 le PdbPosLinha, sem nova busca. */
void PdbArquivosColher(const PdbParametros *p, PdbArquivos *a)
{
    uint32_t indice, dica = 0;

    VecIniciar(&a->lista, sizeof(PdbArquivo));
    VecIniciar(&a->linhas, sizeof(PdbPosLinha));
    VecReservar(&a->linhas, p->dbg->linhas.n);
    BufAnexarN(&a->nomes, "\0", 1);                  /* offset 0 = cadeia vazia */
    a->ultimo = 0;
    for (size_t k = 0; k < p->dbg->linhas.n; k++) {
        const DbgLinha *l = VecEm(&p->dbg->linhas, k);
        FontePosicao fp = DbgPosicaoComDica(p->fonte, l->pos, &dica);
        PdbPosLinha *pl = VecEmpurrar(&a->linhas);

        if (!PdbArquivoAchar(a, fp.caminho, &indice)) {
            PdbArquivoAcrescentar(a, fp.caminho);
            indice = (uint32_t)a->lista.n - 1;
        }
        pl->arquivo = indice;
        pl->linha = fp.linha;
    }
}

void PdbArquivosLiberar(PdbArquivos *a)
{
    VecLiberar(&a->lista);
    VecLiberar(&a->linhas);
    BufLiberar(&a->nomes);
}

/* Grava o offset do nome no 1o balde livre a partir de HashV1 % baldes (sondagem linear); tabela = u32 por balde, 0 = livre. */
static void PdbBaldeInserir(Vec *tabela, const PdbArquivo *arq, const char *nomes)
{
    size_t baldes = tabela->n;
    size_t pos = PdbHashV1(nomes + arq->nome_off) % baldes;

    while (*(uint32_t *)VecEm(tabela, pos) != 0)
        pos = (pos + 1) % baldes;
    *(uint32_t *)VecEm(tabela, pos) = arq->nome_off;
}

/* /names: cabecalho 0xEFFEEFFE/1, bloco de strings (cadeia vazia no 0 e um nome por arquivo), baldes (offset do nome; 0 = vazio) e contagem de nomes. */
void PdbFluxoNomes(Buf *b, const PdbArquivos *a)
{
    size_t baldes = a->lista.n * PDB_NOMES_BALDES_POR_ARQUIVO + 1;
    Vec tabela;

    PdbU32(b, PDB_NOMES_ASSINATURA);
    PdbU32(b, PDB_NOMES_HASH_V1);
    PdbU32(b, (uint32_t)a->nomes.tam);
    BufAnexarN(b, a->nomes.dados, a->nomes.tam);
    PdbU32(b, (uint32_t)baldes);
    VecIniciar(&tabela, sizeof(uint32_t));
    for (size_t k = 0; k < baldes; k++)
        *(uint32_t *)VecEmpurrar(&tabela) = 0;
    for (size_t k = 0; k < a->lista.n; k++)
        PdbBaldeInserir(&tabela, VecEm(&a->lista, k), a->nomes.dados);
    for (size_t k = 0; k < baldes; k++)
        PdbU32(b, *(uint32_t *)VecEm(&tabela, k));
    PdbU32(b, (uint32_t)a->lista.n);                  /* contagem: os nomes da tabela */
    VecLiberar(&tabela);
}

bool PdbEscrever(const PdbParametros *entrada, Buf *saida)
{
    PdbParametros pm = *entrada;                      /* copia: o modulo pede os TypeIndex ao pm.tpi */
    const PdbParametros *p = &pm;
    PdbTipos tipos;
    PdbMsf m;
    PdbSecoes secoes;
    PdbArquivos arquivos = {0};
    PdbTamanhosModulo tam;
    Buf *dbi, *tpi;
    bool ok;

    PdbTiposIniciar(&tipos, p->tipos, p->ir, p->nomes);
    pm.tpi = &tipos;
    PdbSecoesLer(p->exe, &secoes);
    PdbArquivosColher(p, &arquivos);
    PdbMsfIniciar(&m);
    PdbMsfNovoFluxo(&m);                              /* 0: o diretorio antigo do MSF, vazio */
    PdbFluxoInfo(PdbMsfNovoFluxo(&m), p);
    tpi = PdbMsfNovoFluxo(&m);                        /* 2: preenchido depois do modulo, que e quem pede os tipos */
    dbi = PdbMsfNovoFluxo(&m);                        /* 3: preenchido por ultimo, depende dos tamanhos do modulo */
    PdbFluxoTipos(PdbMsfNovoFluxo(&m), NULL, PDB_FLUXO_NULO);
    PdbFluxoNomes(PdbMsfNovoFluxo(&m), &arquivos);
    tam = PdbFluxoModulo(PdbMsfNovoFluxo(&m), p, &arquivos, secoes.texto);   /* 6 */
    BufAnexarN(PdbMsfNovoFluxo(&m), (const char *)secoes.cabecalhos, (size_t)PDB_TAM_CABECALHO_SECAO * secoes.qtd);   /* 7 */
    if (tipos.proximo > PDB_PRIMEIRO_TIPO)
        PdbFluxoHashTpi(PdbMsfNovoFluxo(&m), &tipos);   /* 8: so com registros no TPI */
    PdbFluxoTipos(tpi, &tipos, m.qtd - 1);
    PdbFluxoDbi(dbi, p, &secoes, &tam);
    ok = PdbMsfSerializar(&m, saida);
    PdbMsfLiberar(&m);
    PdbArquivosLiberar(&arquivos);
    PdbTiposLiberar(&tipos);
    return ok;
}
