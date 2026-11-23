/* analises_interno.h - passos das analises da BIR que o analises.c orquestra (M7a F1). Nao incluir fora de bir\analises*.c.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - G3 G0b: quebra de ciclo x64->optimizer (movido de nativo/optimizer/ para nativo/bir/).
 *            0.2.0 - M7b F2: AnalisesVivoDoBloco sai, entra AnalisesVivoConfere (vivacidade esparsa).
 *            0.1.0 - M7a F1: primeira versao.
 * Verificacao: construir_rapido.py && teste_nativo.exe
 */
#ifndef FORJA_NATIVO_OPTIMIZER_ANALISES_INTERNO_H
#define FORJA_NATIVO_OPTIMIZER_ANALISES_INTERNO_H

#include "nativo/bir/analises.h"

/* n+1 elementos zerados na arena das analises (nunca devolve NULL). */
uint32_t *AnalisesVetor32(AnalisesBir *a, size_t n);
uint64_t *AnalisesVetor64(AnalisesBir *a, size_t n);

void AnalisesMontarArestas(AnalisesBir *a, const BirFuncao *f);
void AnalisesCalcularRpo(AnalisesBir *a);
void AnalisesCalcularIdom(AnalisesBir *a);
void AnalisesCalcularLacos(AnalisesBir *a);
void AnalisesCalcularVivo(AnalisesBir *a, const BirFuncao *f);

/* Recalcula os vivos na entrada e na saida do bloco b a partir dos vivo_entra dos sucessores e diz se batem com os guardados (usa a arena para rascunho). */
bool AnalisesVivoConfere(AnalisesBir *a, const BirFuncao *f, uint32_t b);

#endif
