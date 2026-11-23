/* numeracao_interno.h - tipos que numeracao.c (percurso e decisao), numeracao_fatos.c (tabela de fatos com desfazer), numeracao_alias.c (sobreposicao de memoria) e
 * numeracao_morto.c (limpeza das copias e contas sem uso) dividem. Nao incluir fora de numeracao*.c.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M7b R3 rodada 2: memoria de trabalho unica reaproveitada (NumCtx.temp, NumTemp, NumPreparar); NumRemoverMortos recebe o contexto.
 *            0.2.0- M7b R3 rodada 2: NumCtx.ultimo_uso e pos_bloco (janela do copy-prop de origem com varias definicoes).
 *            0.1.0 - M7b R3: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Modelo: a BIR nao e SSA. Um FATO diz "o vreg res guarda o valor da chave" (conta pura ou carga) ou "o vreg d e copia do vreg s". Cada fato guarda a VERSAO de cada vreg de que depende
 * (versao = quantas vezes o vreg foi definido no caminho percorrido); redefinir um vreg sobe a versao e invalida, sem varrer nada, todo fato que o cita. Percorre-se a arvore de
 * dominadores em profundidade: ao sair de um bloco o que ele criou (fatos, versoes, mortes de carga) e desfeito na ordem inversa.
 * Escopo: fato GLOBAL (todo vreg citado tem definicao unica que domina o ponto do fato) vale na subarvore dominada, e e a prova de SSA; fato nao global vale so dentro da EBB (cadeia
 * de blocos de predecessor unico) onde nasceu. Carga nunca e global (a memoria muda por caminhos que a dominancia nao ve).
 */
#ifndef FORJA_NATIVO_OPTIMIZER_NUMERACAO_INTERNO_H
#define FORJA_NATIVO_OPTIMIZER_NUMERACAO_INTERNO_H

#include <stdbool.h>
#include <stdint.h>

#include "nativo/bir/bir.h"
#include "nativo/bir/analises.h"

#define NUM_SEM UINT32_MAX
#define NUM_JANELA_EXPR 96u    /* distancia maxima (instrucoes percorridas) entre o fato e o reaproveitamento de uma conta: limita o alongamento do intervalo do vreg */
#define NUM_JANELA_CARGA 64u   /* idem, para carga */
#define NUM_CARGAS_EBB 32u     /* cargas lembradas por EBB (cada gravacao as varre) */
#define NUM_CADEIA_MAX 16u     /* nos de uma cadeia da tabela que a busca visita */

typedef struct { uint8_t op, aux, classe, ta, tb, ca, cb; int64_t va, vb; int32_t desloc; } NumChave;

typedef struct {
    NumChave chave;
    uint32_t res;              /* vreg que guarda o valor (copia: o vreg de origem) */
    uint32_t dep[3], ver[3];   /* vregs citados (res, a, b; NUM_SEM = nenhum) e a versao de cada um quando o fato nasceu */
    uint32_t ebb, seq, prox, hash;
    uint8_t global, morta, copia;
} NumFato;

typedef struct { uint32_t nfatos, ncargas, nverlog, nmortos; } NumMarca;
typedef struct { uint32_t ebb, dom; } NumPar;
typedef struct { NumPar copia, expr, carga; uint32_t mortos; } NumContagem;
typedef struct { void *v; uint32_t n, cap; } NumVetor;
/* Fotografia das dependencias da instrucao ANTES de ela definir o dst: dep[0] e preenchido depois da definicao. */
typedef struct { uint32_t dep[3], ver[3]; } NumSnap;
typedef struct { uint8_t slot; uint32_t id; } NumBase; /* slot = 1: id e o indice do slot (END_SLOT de definicao unica); 0: id e o vreg */

typedef struct {
    const BirFuncao *f;
    const AnalisesBir *a;
    uint32_t nvregs;
    uint32_t *ndefs, *def_b, *def_k;     /* por vreg: definicoes, bloco e indice da (ultima) definicao */
    uint32_t *ultimo_uso, *pos_bloco;    /* posicao linear (blocos na ordem do RPO, uma por instrucao: a do alocador) do ultimo uso de cada vreg e do inicio de cada bloco */
    uint32_t *ver, *cp;                  /* por vreg: versao atual e o fato de copia mais novo (NUM_SEM = nenhum) */
    NumFato *fatos; uint32_t nfatos, capfatos;
    uint32_t *balde, mascara;            /* tabela: balde[h & mascara] = fato mais novo da cadeia */
    NumVetor cargas, verlog, mortos;     /* uint32_t: fatos de carga da pilha, vregs com versao subida, fatos de carga mortos */
    uint32_t seq;
    NumContagem n;
    uint32_t *temp; size_t temp_cap, temp_usado; /* memoria de trabalho da funcao: UM bloco reaproveitado entre as funcoes (malloc por funcao custava ~40% do passe) */
} NumCtx;

/* numeracao_fatos.c: fatia n palavras zeradas (preenchimento 0) ou com bytes 0xFF (preenchimento 0xFF) de ctx->temp; a capacidade foi garantida por NumPreparar. */
uint32_t *NumTemp(NumCtx *c, size_t palavras, int preenchimento);
/* Garante palavras de memoria de trabalho, zera o escopo (temp_usado, contagens, pilhas de desfazer, fatos) para uma funcao nova. */
void NumPreparar(NumCtx *c, size_t palavras);

/* ---- numeracao_fatos.c ---- */
void NumVetorAcrescentar(NumVetor *vetor, uint32_t valor);
void NumFatosIniciar(NumCtx *c, uint32_t ninstrs);
void NumFatosLiberar(NumCtx *c);
NumMarca NumMarcar(const NumCtx *c);
void NumRestaurar(NumCtx *c, NumMarca marca, bool versoes);
void NumDefinir(NumCtx *c, uint32_t vreg);
bool NumFatoValido(const NumCtx *c, const NumFato *fato, uint32_t ebb);
uint32_t NumBuscar(const NumCtx *c, const NumChave *chave, uint32_t ebb);
void NumInserir(NumCtx *c, NumFato fato);
void NumMatar(NumCtx *c, uint32_t fato);

/* ---- numeracao_alias.c ---- */
bool NumPodeSobrepor(const NumCtx *c, const NumChave *carga, const NumChave *gravacao);
NumBase NumBaseDe(const NumCtx *c, uint32_t vreg);
uint32_t NumTamanhoDaClasse(uint8_t classe);

/* ---- numeracao_morto.c ---- */
/* Tira as instrucoes puras cujo dst nunca e lido (cascata). Devolve quantas. */
uint32_t NumRemoverMortos(BirFuncao *f, NumCtx *c);

#endif
