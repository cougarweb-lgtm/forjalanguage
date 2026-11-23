/* projeto_grafo.h - camadas topologicas dos produtos do forja.proj e deteccao de ciclo (P22-O1b do M3).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22-O1b: ProjetoCamadas (B0007).
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_CACHE_PROJETO_GRAFO_H
#define FORJA_CACHE_PROJETO_GRAFO_H

#include "cache/projeto_ler.h"

/* Camadas por Kahn: camada[k] = 0 para quem nao depende de ninguem, senao 1 + a maior camada das dependencias. ordem[] recebe os indices dos produtos por (camada, nome):
 * dentro da camada, a ordem de nome (os produtos ja vem ordenados por nome). camada e ordem tem projeto->produtos.n posicoes, do chamador. Ciclo (inclusive um produto que
 * depende de si): false e B0007 em erro, com o ciclo na ordem ("a -> b -> a"); camada e ordem ficam sem valor. */
bool ProjetoCamadas(const Projeto *p, uint32_t *camada, uint32_t *ordem, Diag *erro);

#endif
