/* nome_modulo.h - chave por modulo dos simbolos de topo: "<L><tag>_<nome>" (IA1-L8, D0592).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L8 P1: NomeModuloTags (tag unica por modulo), NomeModuloChave (monta a chave) e NomeModuloExibicao (devolve o nome puro).
 * Verificacao: construir.bat && teste_common.exe
 *
 * Nomes de topo de modulos importados nao podem colidir entre si nem com a raiz. A chave de um simbolo de modulo e o texto "<L><tag>_<nome>", com tag = ultimo segmento do
 * caminho canonico do modulo ("vendor:db" -> "db"; caracteres fora de [A-Za-z0-9_] viram '_') e L = tamanho da tag em decimal. O codigo C do simbolo e "fj_" + chave.
 * Injetivo: o segmento de caminho comeca por letra (E1001), entao a tag nunca comeca por digito e o L e o prefixo de digitos; o nome do usuario nunca comeca por digito, entao
 * a chave de um modulo nunca colide com o nome sem prefixo da raiz. Tag repetida entre dois modulos ganha o sufixo "_<k>" (k = indice do modulo na tabela de modulos).
 */
#ifndef FORJA_NOME_MODULO_H
#define FORJA_NOME_MODULO_H

#include "common/base.h"

#define NOME_MODULO_TAG_MAX 96 /* bytes da tag, com o NUL e o sufixo de desempate */

/* Tag unica de cada modulo: caminhos[i] (texto terminado em NUL) vira tags[i]. A ordem do vetor decide o desempate (o 1o fica com a tag natural, o seguinte ganha "_<i>"). */
void NomeModuloTags(const char *const *caminhos, size_t n, char (*tags)[NOME_MODULO_TAG_MAX]);

/* Escreve "<L><tag>_<nome>" em saida; devolve o tamanho, ou 0 se nao couber em cap (com o NUL). */
size_t NomeModuloChave(const char *tag, const char *nome, size_t tam_nome, char *saida, size_t cap);

/* Nome puro de uma chave: depois de "<L><tag>_" (o que a mensagem e o rastro mostram). Texto que nao e chave (nao comeca por 1-9, ou o prefixo nao fecha) volta inteiro. */
const char *NomeModuloExibicao(const char *chave);

#endif
