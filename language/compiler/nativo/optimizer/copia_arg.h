/* copia_arg.h - passe `copia_arg` (M7a): tira a copia do argumento agregado quando o chamado so le o parametro.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7a: primeira versao (recomendacao do Fable sobre aabb_raio: 3 copia_mem n=48 por volta antes de `chama`).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <abs> && python tests\nativo\otim\verificar_otim.py
 *
 * O construtor da BIR copia todo argumento agregado para um slot privado (`copia_mem [temp], [origem]`) porque o chamado pode alterar o parametro. Este passe passa
 * a ENDERECO DA ORIGEM no `chama` (e apaga a copia e o end_slot do temp) quando, juntas: (1) o chamado e proc do programa (chamada direta, nunca foreign, indireta
 * ou com descritor SysV) e so LE o parametro (so `carrega [p+d]` e fonte de `copia_mem`; nao grava, nao redefine, nao passa adiante nem toma o endereco);
 * (2) o temp so aparece como destino dessa copia e como argumento desse `chama`, no mesmo bloco; (3) a origem e um slot local (por end_slot, direto ou
 * somado a indice) cujo endereco nunca escapa na funcao (E2); (4) nenhuma gravacao no slot de origem entre a copia e o `chama`. Origem global, `p^` ou slot
 * com `&` escapado: nada muda. Roda em ponto fixo (a copia k5->k6->origem encadeia). Nunca move trap: so apaga copia e end_slot.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_COPIA_ARG_H
#define FORJA_NATIVO_OPTIMIZER_COPIA_ARG_H

#include "nativo/bir/bir.h"

/* Passe da tabela (passes.c): transforma a funcao no lugar. */
void OtimCopiaArgAplicar(const BirPrograma *programa, BirFuncao *funcao);

#endif
