/* sroa_interno.h - tipos que sroa.c (decisao) e sroa_reescrever.c (troca das instrucoes) dividem.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7b F3: primeira versao.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_SROA_INTERNO_H
#define FORJA_NATIVO_OPTIMIZER_SROA_INTERNO_H

#include "nativo/bir/bir.h"

#define SROA_SEM UINT32_MAX
#define ORC_SROA_SLOT_BYTES 64u    /* slot maior que isto fica na pilha */
#define ORC_SROA_CAMPOS_SLOT 16u   /* campos de um slot */
#define ORC_SROA_VREGS_FUNCAO 256u /* vregs novos por funcao */
#define SROA_CAMPOS_GRUPO 64u      /* intervalos distintos de um grupo de slots ligados por copia_mem */

/* SM_OK = candidato (no fim da decisao: escalariza). A ordem de SM_* e a de NOME_MOTIVO em sroa.c. SM_PULADO: slot de depuracao (--simbolos), sem remark. */
typedef enum {
    SM_OK, SM_ESCAPA, SM_ENDERECO_MULTIPLO, SM_INDICE_DINAMICO, SM_CLASSE, SM_SOBREPOSICAO, SM_TAMANHO, SM_CAMPOS, SM_GRUPO, SM_ORCAMENTO, SM_PULADO
} SroaMotivo;

/* Um campo do slot: faixa [off, off+tam) em bytes do slot, classe do acesso e o vreg novo que o guarda. */
typedef struct { int32_t off; uint32_t tam; uint8_t classe; uint32_t vreg; } SroaCampo;

typedef struct { uint8_t motivo, nf; SroaCampo campo[ORC_SROA_CAMPOS_SLOT]; } SroaSlot; /* campo[] em ordem crescente de off */

/* De que slot um vreg de endereco e raiz e a quantos bytes do inicio dele (slot = SROA_SEM: nao e endereco de slot). */
typedef struct { uint32_t slot; int32_t off; } SroaRaiz;

/* O que a decisao entrega a reescrita: info[nslots], raiz[nvregs_antigo] e os vregs novos [nvregs_antigo, nvregs_novo) ja numerados em info[].campo[].vreg. */
typedef struct { const SroaSlot *info; const SroaRaiz *raiz; uint32_t nvregs_antigo, nvregs_novo; } SroaPlano;

/* Troca, em f, cada acesso a slot com motivo SM_OK pelos campos dele; apaga END_SLOT, COPIA e ADD de endereco. */
void SroaReescrever(BirFuncao *f, const SroaPlano *plano);

#endif
