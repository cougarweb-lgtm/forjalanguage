/* analises.h - analises da BIR para o otimizador (M7a F1): predecessores, RPO, dominadores, lacos naturais e vivacidade. So leem; nenhuma transforma a BIR.
 * Versao: 0.2.2
 * Changelog: 0.2.2 - G3 G0b: quebra de ciclo x64->optimizer (movido de nativo/optimizer/ para nativo/bir/).
 *            0.2.1 - M7b F2 rodada 3: AnalisesCalcularLacosApenas (sem vivacidade; o passe `inline` so precisa dos lacos).
 *            0.2.0 - M7b F2: vivacidade em conjuntos esparsos (ConjuntoVivo) com fila de trabalho (o bitset blocos x vregs custava 650 ms numa funcao de 23 mil blocos e 109 mil vregs, o chamador com muito inline) e AnalisesDomina em O(1) (dom_pre/dom_tam; a subida pelo idom custava 320 ms na mesma funcao).
 *            0.1.0 - M7a F1: primeira versao (Cooper-Harvey-Kennedy, lacos naturais por back-edge, vivacidade em bitset por bloco).
 * Verificacao: construir_rapido.py && teste_nativo.exe (em language\compiler); FORJA_BIR_ANALISES=1 forja build <fj> --release
 *
 * Grafo: bloco = BirBloco; aresta = SALTA, DESVIA e o alvo frio de cada *_OV (a aresta nasce na instrucao, no meio do bloco). Sem duplicatas.
 * Determinismo: todas as ordens saem de (id do bloco, ordem da instrucao); nenhum ponteiro e comparado. Bloco inalcancavel da entrada (bloco 0) fica de fora
 * do RPO, dos dominadores, dos lacos e da vivacidade (pos_rpo/idom = BIR_SEM_BLOCO, conjuntos vazios).
 * Vivacidade: sobre os vregs (cada vreg e um bit; o slot de pilha e memoria e nao entra). vivo_entra(b) = vivos antes da 1a instrucao; vivo_sai(b) = uniao dos
 * vivo_entra dos sucessores do TERMINADOR; a aresta fria de um *_OV e tratada dentro do bloco, no ponto da instrucao (dst do *_OV so e definido sem estouro).
 * Lacos: um por cabecalho (back-edges de mesmo cabecalho se fundem); aresta de retorno em grafo irredutivel (cabecalho nao domina a origem) nao vira laco.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_ANALISES_H
#define FORJA_NATIVO_OPTIMIZER_ANALISES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/arena.h"
#include "nativo/bir/bir.h"

/* Laco natural: corpo = blocos do laco (cabecalho incluso) em ordem crescente de id. */
typedef struct { uint32_t cabecalho, ncorpo; uint32_t *corpo; } LacoBir;

/* Conjunto esparso de vregs vivos: v[0..n) em ordem crescente (na arena das analises). */
typedef struct { uint32_t n; uint32_t *v; } ConjuntoVivo;

typedef struct {
    Arena arena;                        /* dona de todos os vetores abaixo */
    uint32_t nblocos, nvregs;
    uint32_t *suc_ini, *suc;             /* sucessores em CSR: suc[suc_ini[b] .. suc_ini[b+1]) */
    uint32_t *pred_ini, *pred;           /* predecessores em CSR, em ordem crescente de id */
    uint32_t *rpo; uint32_t nrpo;        /* alcancaveis em ordem pos-ordem reversa (rpo[0] = 0) */
    uint32_t *pos_rpo;                   /* por bloco: indice em rpo ou BIR_SEM_BLOCO */
    uint32_t *idom;                      /* por bloco: dominador imediato; idom[0] = 0; BIR_SEM_BLOCO se inalcancavel */
    uint32_t *dom_pre, *dom_tam;         /* por bloco alcancavel: numero em pre-ordem na arvore de dominadores e tamanho da subarvore (AnalisesDomina em O(1)) */
    LacoBir *lacos; uint32_t nlacos;     /* ordenados pelo id do cabecalho */
    uint32_t *prof_laco;                 /* por bloco: quantos lacos o contem (0 = fora de laco) */
    ConjuntoVivo *vivo_entra, *vivo_sai; /* por bloco: vregs vivos na entrada e na saida */
} AnalisesBir;

/* Calcula tudo para a funcao. Funcao sem blocos da resultado vazio. */
void AnalisesCalcular(const BirFuncao *funcao, AnalisesBir *saida);

/* So o grafo: arestas, RPO, dominadores e lacos (vivo_entra/vivo_sai ficam NULL). Libere com AnalisesLiberar. */
void AnalisesCalcularLacosApenas(const BirFuncao *funcao, AnalisesBir *saida);

/* Acrescenta a vivacidade a uma AnalisesBir que veio de AnalisesCalcularLacosApenas (mesmo resultado de AnalisesCalcular). */
void AnalisesCompletarVivo(const BirFuncao *funcao, AnalisesBir *saida);

void AnalisesLiberar(AnalisesBir *a);

/* x domina y? (todo bloco alcancavel se domina; inalcancavel nao e dominado por ninguem) */
bool AnalisesDomina(const AnalisesBir *a, uint32_t x, uint32_t y);

/* O vreg esta vivo na entrada / na saida do bloco? */
bool AnalisesVivoEntra(const AnalisesBir *a, uint32_t bloco, uint32_t vreg);
bool AnalisesVivoSai(const AnalisesBir *a, uint32_t bloco, uint32_t vreg);

/* Confere as invariantes (RPO cobre os alcancaveis, idom domina o bloco e os predecessores, cabecalho domina o laco, vivacidade e ponto fixo, vivo_entra da
 * entrada so tem parametros). false com a causa em causa[n]. Usa a arena para rascunho (por isso nao e const). */
bool AnalisesConferir(const BirFuncao *funcao, AnalisesBir *a, char *causa, size_t n);

#endif
