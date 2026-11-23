/* dbg.h - DbgPrograma: registro neutro de depuracao colhido pelo gerador x64 (D-M6-2). Fonte unica da tabela FRT1 do --rastreio e do dump --debug-code linhas
 * (e, nos passos seguintes, do DWARF e do PDB): nenhum formato le a BIR nem a MInstr direto.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6b P76d: DbgPosicaoComDica.
 *            0.1.0 - M6 P65b: primeira versao (DbgProc, DbgLinha, DbgVar com localizacao em variante, DbgChamada, DbgMain).
 * Verificacao: construir.bat && python ..\tests\nativo\m6\verificar_linhas.py --forja .\forja.exe
 *
 * Ponteiros (nome, simbolo, tipo_nome) apontam para a BIR (BirPrograma), que vive ate o fim da montagem: nada e copiado aqui. Posicoes (pos) sao offsets globais
 * na fonte (BIR_SEM_POS = sem); quem formata resolve arquivo, linha e coluna pela Fonte. Offsets de codigo sao em .text do PROGRAMA.
 */
#ifndef FORJA_NATIVO_DBG_H
#define FORJA_NATIVO_DBG_H

#include "common/base.h"
#include "common/vec.h"
#include "nativo/bir/bir.h"

/* Onde mora o valor de uma variavel. Hoje so "rbp+off"; o M7a acrescenta a variante "lista" sem mexer em produtor nem consumidor. */
typedef enum { DBG_LOC_RBP } DbgLocTipo;

/* indireto: o home guarda um PONTEIRO para o dado (parametro agregado); senao o dado mora em [rbp + rbp_off]. */
typedef struct { uint8_t tipo; int32_t rbp_off; bool indireto; } DbgLoc;

typedef struct { const char *nome, *tipo_nome; TipoId tipo; uint32_t local_id; bool parametro; DbgLoc loc; } DbgVar;

/* Par (offset em .text, pos na fonte), gravado so quando a pos muda dentro do proc. */
typedef struct { uint32_t off, pos; } DbgLinha;

/* Endereco de retorno de um call de proc .fj e a posicao dele (alimenta a FRT1). */
typedef struct { uint32_t ra, pos; } DbgChamada;

/* nome = nome Forja (sem "fj_" e sem prefixo de modulo); simbolo = nome do simbolo do objeto. As faixas de linhas e variaveis indexam DbgPrograma.linhas/vars. */
typedef struct {
    const char *nome, *simbolo;
    uint32_t pos, inicio, fim;
    uint32_t linha_ini, nlinhas;
    uint32_t var_ini, nvars, nparams;
    uint32_t rbp_quadro;        /* bytes do quadro (sub rsp,N): o rbp fica N acima do rsp */
} DbgProc;

typedef struct {
    Vec procs;                  /* DbgProc, na ordem de geracao (o forja_programa sintetico fica de fora) */
    Vec linhas;                 /* DbgLinha */
    Vec vars;                   /* DbgVar */
    Vec chamadas;               /* DbgChamada */
    uint32_t main_pos;          /* BirFuncao.pos do main (BIR_SEM_POS = sem) */
    uint32_t ultima_pos;        /* ultima pos gravada no proc em curso */
    bool aberto;                /* ha proc em curso (false com o sintetico: os ganchos viram no-op) */
} DbgPrograma;

void DbgIniciar(DbgPrograma *d);
void DbgLiberar(DbgPrograma *d);

/* Onde o proc comeca: nome do simbolo, offset em .text e Quadro.tam. */
typedef struct { const char *simbolo; uint32_t inicio, quadro; } DbgAbertura;

/* Abre o proc da BirFuncao f; f->sintetica nao entra. */
void DbgProcAbrir(DbgPrograma *d, const BirFuncao *f, const DbgAbertura *a);

/* Acrescenta a variavel ao proc em curso (parametros primeiro, em ordem de local_id). */
void DbgVariavel(DbgPrograma *d, const DbgVar *v);

/* Instrucao em off com a posicao pos: grava o par so se pos != BIR_SEM_POS e difere da ultima gravada no proc. */
void DbgLinhaVista(DbgPrograma *d, uint32_t off, uint32_t pos);

/* Fim do proc em curso (sem o padding). */
void DbgProcFechar(DbgPrograma *d, uint32_t fim);

void DbgChamadaVista(DbgPrograma *d, uint32_t ra, uint32_t pos);

/* Arquivo (nunca NULL: "" se a fonte nao tem caminho) e linha de pos; a dica (linha anterior, 0-based) poupa a busca binaria quando as posicoes andam em ordem. */
FontePosicao DbgPosicaoComDica(const Fonte *f, uint32_t pos, uint32_t *dica);

#endif
