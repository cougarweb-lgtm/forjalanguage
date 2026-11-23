/* gerar_bir.h - ponto de entrada dos testes que precisam da BIR de um arquivo .fj (front-end completo, BIR construida e validada).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - backend x64 P7a: BirUsoFn e BirComPrograma.
 * Verificacao: construir.bat (compila com -Werror)
 */
#ifndef FORJA_TESTE_NATIVO_BIR_GERAR_BIR_H
#define FORJA_TESTE_NATIVO_BIR_GERAR_BIR_H

#include <stdint.h>

#include "nativo/bir/bir.h"

/* Recebe a BIR validada; devolve o codigo de saida do modo. */
typedef int (*BirUsoFn)(const BirPrograma *programa, void *ctx);

/* Roda o front-end em arquivo, constroi e valida a BIR (modo = BirModo) e chama uso. Codigos: 0 ok | 1 diagnostico JSON em stdout
 * (front-end ou X0001) | 2 arquivo ilegivel | 3 BIR invalida (X0904) ou memoria; senao o que uso devolver. */
int BirComPrograma(const char *arquivo, uint8_t modo, BirUsoFn uso, void *ctx);

#endif
