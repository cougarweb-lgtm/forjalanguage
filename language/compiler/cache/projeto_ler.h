/* projeto_ler.h - o manifesto forja.proj do orquestrador de projeto (subconjunto 0.19.0): produtos, dependencias e pasta de saida (P22-O1b do M3).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22-O1b: Projeto, Produto, ProjetoLer (B0005) e ProjetoLiberar.
 * Verificacao: construir.bat && teste_common.exe  (e python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe)
 */
#ifndef FORJA_CACHE_PROJETO_LER_H
#define FORJA_CACHE_PROJETO_LER_H

#include "common/diag.h"
#include "common/vec.h"

#define PROJETO_NOME_MAX 64         /* nome do produto, com o NUL */
#define PROJETO_CAMINHO_MAX 1100
#define PROJETO_DEPENDE_MAX 32      /* dependencias diretas por produto */

typedef enum { PRODUTO_EXE, PRODUTO_DLL } ProdutoTipo;

/* Um produto ja resolvido: caminhos absolutos; dep[] = indices em Projeto.produtos, em ordem crescente (= ordem de nome, porque os produtos ficam ordenados por nome). */
typedef struct {
    char nome[PROJETO_NOME_MAX];
    ProdutoTipo tipo;
    char entrada[PROJETO_CAMINHO_MAX];      /* o .fj raiz */
    char saida[PROJETO_CAMINHO_MAX];        /* <saida>\<nome>.exe ou .dll */
    char cabecalho[PROJETO_CAMINHO_MAX];    /* <saida>\<nome>.h da dll; vazio no exe */
    char nomes_dep[PROJETO_DEPENDE_MAX][PROJETO_NOME_MAX];  /* os nomes como o manifesto os escreveu (so ate ProjetoLer resolver) */
    uint32_t dep[PROJETO_DEPENDE_MAX];
    uint32_t ndep;
    uint32_t linha;                         /* linha do 'produto' no manifesto */
} Produto;

typedef struct {
    char pasta[PROJETO_CAMINHO_MAX];        /* pasta do forja.proj, absoluta */
    char saida[PROJETO_CAMINHO_MAX];        /* pasta de saida, absoluta */
    Vec produtos;                           /* Produto, ordenados por nome */
} Projeto;

/* Le o manifesto. Formato (UTF-8, uma declaracao por linha, '#' comenta ate o fim da linha):
 *   saida = <pasta>                                        (opcional, uma vez; padrao "bin"; relativa a pasta do forja.proj)
 *   produto <nome> <exe|dll> <entrada.fj> [depende <nome> ...]   (nome = [a-z0-9_]+ unico; entrada relativa a pasta do manifesto e existente)
 * Devolve true e o Projeto preenchido (libere com ProjetoLiberar). false com erro->tem = true: B0005 (causa traz o caminho dado, a linha e o motivo) e o chamador
 * imprime. false com erro->tem = false: o arquivo nao abriu e FonteLer ja escreveu a mensagem. */
bool ProjetoLer(const char *caminho, Projeto *saida, Diag *erro);

void ProjetoLiberar(Projeto *p);

/* O k-esimo produto (k < p->produtos.n). */
const Produto *ProjetoProduto(const Projeto *p, uint32_t k);

#endif
