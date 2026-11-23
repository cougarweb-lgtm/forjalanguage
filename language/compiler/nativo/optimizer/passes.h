/* passes.h - gerenciador de passes de otimizacao da BIR (M7a F0): tabela estatica de passes, --sem-otim e o relato do --debug-code otim.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - G0d: OtimRelatos (contadores do item otim + dump da BIR por passe do item passes); OtimExecutar recebe a struct no lugar do Buf.
 *            0.3.1 - M8 P2: so comentario: com BirPrograma.perfil o OtimExecutar roda tambem o passe `perfil` (perfil.h), que nao esta na tabela e roda ate em dev.
 *            0.3.0 - M7b F2: PasseOtim.aplicar_programa (passe que ve o programa inteiro e muda varias funcoes: `inline`); NULL nos demais.
 *            0.2.0 - M7a F3: PasseOtim.aplicar recebe tambem o programa (o alocador precisa saber das chamadas SysV); a tabela ganha `alocador`.
 *            0.1.0 - M7a F0: infraestrutura neutra; a tabela fica vazia (os passes reais chegam nas fatias F1+), com ela vazia a BIR e o exe nao mudam.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --selftest
 *
 * Passe novo = uma linha em PASSES (passes.c), antes da sentinela. So o release roda passe; o dev nunca. Com FORJA_BIR_VALIDAR=1 (modo teste) o
 * BirValidar roda depois de CADA passe e uma BIR invalida aborta o build com X0904 citando o passe.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_PASSES_H
#define FORJA_NATIVO_OPTIMIZER_PASSES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/bir/bir.h"

/* Um passe: transforma uma funcao da BIR no lugar. nome e o que --sem-otim aceita. */
typedef struct {
    const char *nome;
    void (*aplicar)(const BirPrograma *programa, BirFuncao *funcao);
    bool ligado_por_padrao;
    void (*aplicar_programa)(BirPrograma *programa, Buf *relato); /* opcional (M7b F2): se nao NULL, roda UMA vez no lugar de aplicar por funcao */
} PasseOtim;

/* Quantos passes a tabela tem. */
uint32_t OtimQtdPasses(void);

/* Nome do k-esimo passe (k < OtimQtdPasses). */
const char *OtimNomePasse(uint32_t k);

/* Confere o valor de --sem-otim: "tudo" ou nomes de passe separados por virgula. false com a causa (lista dos passos validos) em causa[n]. */
bool OtimConferirLista(const char *lista, char *causa, size_t n);

/* Onde OtimExecutar deixa o que o --debug-code pede; cada Buf pode ser NULL (item nao pedido). */
typedef struct {
    Buf *contadores; /* item otim: cabecalho "otim: N passes" e uma linha por passe e funcao (instrucoes antes e depois) */
    Buf *passes;     /* item passes (G0d): "== entrada" e, depois de cada passe que rodou, "== passe <nome>" seguido da BIR inteira (BirImprimir) */
} OtimRelatos;

/* Roda os passes ligados (padrao menos os de sem_otim; NULL = nenhum desligado) sobre cada funcao, na ordem da tabela, so se programa->modo e BM_RELEASE.
 * relatos pode ser NULL (nada a relatar). false com X0904 se algum passe deixou a BIR invalida (so com FORJA_BIR_VALIDAR=1). */
bool OtimExecutar(BirPrograma *programa, const char *sem_otim, const OtimRelatos *relatos, Diag *erro);

#endif
