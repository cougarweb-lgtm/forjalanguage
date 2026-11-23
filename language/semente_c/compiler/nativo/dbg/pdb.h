/* pdb.h - PDB proprio (M6b P75, D-M6-9): container MSF 7.0 (bloco de 4096 bytes) e os streams minimos (info, TPI, DBI, IPI e /names).
 * Versao: 0.5.0
 * Changelog: 0.5.0 - M6b P77b: PdbTipos (o TPI sob demanda: registros LF_* dos tipos nao simples), PdbTipoIndice, PdbTiposIniciar/Liberar; PdbParametros ganha ir, nomes e tpi.
 *            0.4.0 - M6b P77a: PdbParametros ganha tipos (TypeIndex simples de cada variavel); PdbTipoSimples (pdb_tipos.c); o modulo grava S_REGREL32 (RBP) de cada variavel simples.
 *            0.3.1 - M6b P76c: PdbTamanhosModulo leva os arquivos do modulo (ArquivosFonte do ModInfo e FileInfo do DBI: o dbghelp so acha a linha com a lista).
 *            0.3.0 - M6b P76c: PdbParametros ganha fonte (arquivo:linha de cada DbgLinha); PdbArquivos (os .fj que as linhas citam, colhidos antes do /names);
 *                    PdbTamanhosModulo (SymByteSize e C13ByteSize) e PdbPatchU32 (tamanhos que so se sabem depois). O C13 (FILECHKSMS + DEBUG_S_LINES) fica no modulo.
 *            0.2.0 - M6b P76a: PdbParametros ganha dbg, exe, modulo e codigo_tam; PdbSecoes e as funcoes de pdb_dbi.c (DBI com 1 modulo, SectionContribution, SectionMap,
 *                    OptionalDbgHeader) e pdb_modulo.c (S_GPROC32/S_FRAMEPROC/S_END). Linhas (C13) ficam para o P76c.
 *            0.1.0 - M6b P75: primeira versao (PdbMsf: fluxos em memoria e serializacao; PdbEscrever: o PDB vazio-valido, casado com o RSDS do exe pelo GUID e pela idade).
 * Verificacao: python compiler\construir_rapido.py && python ..\tests\nativo\m6b\verificar_pdb.py --modo modulo --exe <exe do forja> --exigir-pdb
 *              python ..\tests\nativo\m6b\ler_pdb.py <arquivo.pdb>
 *
 * O MSF e a lista de fluxos (streams) numerados; o leitor acha cada um pelo diretorio. pdb_msf.c so conhece bytes: o significado de cada fluxo e de pdb_escrever.c.
 */
#ifndef FORJA_NATIVO_DBG_PDB_H
#define FORJA_NATIVO_DBG_PDB_H

#include "common/arena.h"
#include "common/buf.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "common/vec.h"
#include "ir/ir.h"
#include "ir/layout.h"
#include "nativo/dbg/dbg.h"

#define PDB_TAM_BLOCO 4096u
#define PDB_MAX_FLUXOS 16u
#define PDB_PRIMEIRO_TIPO 0x1000u   /* TypeIndexBegin do TPI: os indices abaixo sao os tipos simples (T_*) */

/* Os fluxos vivem em memoria ate a serializacao; o indice do fluxo e a ordem de PdbMsfNovoFluxo (o 0 e o diretorio antigo do MSF: o chamador o deixa vazio). */
typedef struct {
    Buf fluxos[PDB_MAX_FLUXOS];
    uint32_t qtd;
} PdbMsf;

void PdbMsfIniciar(PdbMsf *m);
void PdbMsfLiberar(PdbMsf *m);

/* Acrescenta um fluxo vazio e devolve o buffer dele; o indice e m->qtd - 1. Passar de PDB_MAX_FLUXOS e erro interno. */
Buf *PdbMsfNovoFluxo(PdbMsf *m);

/* Monta o arquivo MSF em saida (que tem de vir vazio): superbloco, mapa de blocos livres, fluxos, diretorio e mapa do diretorio.
 * Devolve false se o arquivo passaria de 32768 blocos (o limite de um bloco de mapa livre; o PDB de um programa da Forja nao chega perto). */
bool PdbMsfSerializar(const PdbMsf *m, Buf *saida);

/* Inteiros little-endian no fim do buffer. PdbPatchU32 troca o u32 ja gravado na posicao pos (tamanhos que so se sabem depois). */
void PdbU16(Buf *b, uint32_t v);
void PdbU32(Buf *b, uint32_t v);
void PdbPatchU32(Buf *b, size_t pos, uint32_t v);

/* O TPI em construcao: reg = os registros LF_* ja gravados (cada um multiplo de 4); indice = 2 * ntipos u32 por TipoId (a 1a metade o TypeIndex, 0 = ainda sem registro,
 * 1 = em curso; a 2a o fwdref de struct em ciclo), alocado na 1a variavel nao simples; lay = LayoutCalcular, so no 1o tipo composto; proximo = o proximo TypeIndex. */
typedef struct {
    const Tipos *tipos;
    const IrPrograma *ir;
    const Interner *nomes;
    Buf reg;
    Arena arena;
    uint32_t *indice;
    uint32_t ntipos, proximo;
    Layouts lay;
    bool lay_pronto;
} PdbTipos;

/* O que casa o .pdb com o exe (o GUID e a idade do RSDS; o dbghelp compara os dois) e o que o modulo descreve: os procs do DbgPrograma, enderecados por
 * secao:offset (o objeto do programa e o 1o da ligacao e comeca no offset 0 da .text de saida, entao CodeOffset = DbgProc.inicio). */
typedef struct {
    uint8_t guid[16];
    uint32_t idade;
    const DbgPrograma *dbg;     /* os procs e as linhas do programa */
    const Tipos *tipos;         /* os tipos da linguagem: o TypeIndex simples de cada variavel (PdbTipoSimples) */
    const IrPrograma *ir;       /* as structs e os enums (membros) dos tipos nao simples */
    const Interner *nomes;      /* os nomes internados de struct, campo e enum */
    PdbTipos *tpi;              /* o TPI em construcao: PdbEscrever o preenche numa copia dos parametros (o chamador deixa NULL) */
    const Fonte *fonte;        /* a fonte do programa: cada DbgLinha.pos vira arquivo:linha por ela (o mesmo caminho do JSON) */
    const Buf *exe;             /* o exe ja montado: dele vem a copia dos cabecalhos de secao */
    const char *modulo;         /* nome do modulo (e do objeto) no DBI */
    uint32_t codigo_tam;        /* bytes do .text que sao do programa (runtime_texto_rva - rva da .text): a SectionContribution do modulo */
} PdbParametros;

/* Um arquivo .fj que as linhas citam: caminho = ponteiro do trecho (o mesmo texto do JSON); nome_off = offset do nome no /names (o NameIndex do FILECHKSMS). */
typedef struct { const char *caminho; uint32_t nome_off; } PdbArquivo;

/* Uma DbgLinha ja resolvida na Fonte: arquivo = indice em PdbArquivos.lista, linha = linha local do arquivo. */
typedef struct { uint32_t arquivo, linha; } PdbPosLinha;

/* lista na ordem da 1a aparicao; nomes = bloco de strings do /names (a cadeia vazia no 0); ultimo = cache do ultimo achado;
 * linhas = PdbPosLinha por DbgLinha (a Fonte e consultada uma vez por linha, na Colher). */
typedef struct { Vec lista; Buf nomes; size_t ultimo; Vec linhas; } PdbArquivos;

/* pdb_escrever.c: colhe os arquivos e as posicoes de todas as linhas (antes do /names e do modulo), libera a colheita e monta o /names com a tabela de baldes. */
void PdbArquivosColher(const PdbParametros *p, PdbArquivos *a);
void PdbArquivosLiberar(PdbArquivos *a);
void PdbFluxoNomes(Buf *b, const PdbArquivos *a);

#define PDB_TAM_CABECALHO_SECAO 40u

/* Os cabecalhos de secao do exe (IMAGE_SECTION_HEADER, 40 bytes cada), apontando para dentro de exe; texto = indice da .text contado de 1. */
typedef struct {
    const uint8_t *cabecalhos;
    uint32_t qtd;
    uint32_t texto;
} PdbSecoes;

/* SymByteSize (com a assinatura) e C13ByteSize do modulo: o DBI os copia para o ModInfo; arquivos = os .fj do modulo (ArquivosFonte e FileInfo do DBI). */
typedef struct { uint32_t simbolos; uint32_t c13; const PdbArquivos *arquivos; } PdbTamanhosModulo;

/* pdb_tipos.c: o TypeIndex CodeView de um tipo simples (escalar, ponteiro de 64 bits para escalar, rawptr/cstr = void*); 0 = nao simples. */
uint32_t PdbTipoSimples(const Tipos *t, TipoId id);

/* O TypeIndex de qualquer tipo: o simples direto, ou o registro LF_* (gravado agora, em ordem topologica, se ainda nao existe). 0 = tipo sem forma (vazio, veneno). */
uint32_t PdbTipoIndice(PdbTipos *t, TipoId id);
void PdbTiposIniciar(PdbTipos *t, const Tipos *tipos, const IrPrograma *ir, const Interner *nomes);
void PdbTiposLiberar(PdbTipos *t);

/* pdb_dbi.c: acha os cabecalhos de secao no exe (PE malformado ou sem .text = FalhaInterna) e monta o fluxo DBI (1 modulo; fluxos 6 = modulo e 7 = cabecalhos). */
void PdbSecoesLer(const Buf *exe, PdbSecoes *s);
void PdbFluxoDbi(Buf *b, const PdbParametros *p, const PdbSecoes *s, const PdbTamanhosModulo *t);

/* pdb_modulo.c: o fluxo do modulo: assinatura C13, por proc S_GPROC32 + S_FRAMEPROC + S_REGREL32 das variaveis simples + S_END, o C13 (FILECHKSMS e DEBUG_S_LINES por proc) e GlobalRefsSize 0.
 * Proc que passa de codigo_tam ou linha fora do proc = FalhaInterna. */
PdbTamanhosModulo PdbFluxoModulo(Buf *b, const PdbParametros *p, const PdbArquivos *a, uint32_t segmento);

/* O PDB (fluxos 1 info, 2 TPI, 3 DBI, 4 IPI, 5 /names, 6 modulo, 7 cabecalhos de secao) em saida, que tem de vir vazia. false = nao coube no MSF. */
bool PdbEscrever(const PdbParametros *p, Buf *saida);

#endif
