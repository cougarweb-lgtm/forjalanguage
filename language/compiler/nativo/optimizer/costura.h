/* costura.h - passe `costura` (jump threading com faixa por aresta): bloco pequeno e puro que desvia num CMP ja decidido pela aresta de entrada e copiado nessa aresta.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - primeira versao (costura.c, costura_faixa.c). Roda no release, depois do `cfg` e antes da `numeracao`; desligavel com --sem-otim costura.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --passe costura
 */
#ifndef FORJA_NATIVO_OPTIMIZER_COSTURA_H
#define FORJA_NATIVO_OPTIMIZER_COSTURA_H

#include "nativo/bir/bir.h"

/* Passe por funcao (passes.c). So age se cfg.h diz que a funcao e elegivel; estouro de orcamento deixa a funcao como esta. */
void OtimCosturaAplicar(const BirPrograma *programa, BirFuncao *funcao);

#endif
