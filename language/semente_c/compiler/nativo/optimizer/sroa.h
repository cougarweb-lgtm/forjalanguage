/* sroa.h - passe `sroa` (M7b F3): scalar replacement of aggregates. O slot de pilha pequeno cujo endereco nunca escapa vira um vreg por campo (registrador), na BIR do release x64.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - S4b: OtimSroaVeredito.
 * Changelog: 0.1.0 - M7b F3: primeira versao (sroa.c decide: raizes, escape, particao em campos, grupos por copia_mem, orcamento; sroa_reescrever.c troca as instrucoes).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe sroa
 *
 * Remarks do --debug-code otim (uma linha por slot de funcao com candidato): "sroa fj_<funcao> k<N>: <tam> B, <campos> campos" e
 * "sroa recusa fj_<funcao> k<N>: <motivo>", motivo = escapa | endereco_multiplo | indice_dinamico | classe | sobreposicao | tamanho | campos | grupo | orcamento.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_SROA_H
#define FORJA_NATIVO_OPTIMIZER_SROA_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

/* Roda o passe no programa inteiro (so release). relato != NULL recebe as linhas "sroa ..." do --debug-code otim. */
void OtimSroaAplicar(BirPrograma *programa, Buf *relato);

/* So a decisao, sem tocar na BIR: escalariza[k] (f->nslots posicoes, do chamador) = o sroa escalarizaria o slot k de f agora. Funcao que o sroa nao olha: tudo false. */
void OtimSroaVeredito(const BirPrograma *p, BirFuncao *f, bool *escalariza);

#endif
