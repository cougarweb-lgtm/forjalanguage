/* bir_alcance.h - poda do inalcancavel (B07, M4 P24p): quais procs da IR o backend x64 precisa construir.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6 P71b: a lista fechada de BirNomeSomem ganha track_handle e untrack_handle.
 *            0.2.0 - M6 P71a: BirNomeSomem (lista fechada das chamadas de std:mem que somem em release: snapshot e report_since) e BirAlcance.omite_somem: em release a chamada nao marca o proc (o wrapper de std:mem e podado).
 *            0.1.0 - M4 P24p: primeira versao. Marca, a partir das raizes (main, 'pub export proc' em --modo dll, procs 'test' em forja test), os procs alcancaveis
 *                    por IR_CHAMADA direta e por V_PROC (endereco tomado: proc como valor e callback); cada V_PROC alcancado marca tambem o trampolim fjt_ dele.
 * Verificacao: construir.bat && teste_nativo.exe alcance && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m4/poda
 *
 * A semantica nao passa por aqui: so o BirConstruir (BirEntrada.poda). Deterministico e sem estado global: o resultado vive em BirAlcance.
 */
#ifndef FORJA_NATIVO_BIR_BIR_ALCANCE_H
#define FORJA_NATIVO_BIR_BIR_ALCANCE_H

#include "nativo/bir/bir.h"

typedef struct {
    const IrPrograma *ir;
    bool teste, dll;
    bool omite_somem;              /* release: as chamadas SOMEM nao entram no alcance (BirNomeSomem) */
    const Interner *nomes;
    uint32_t nnomes;
    uint8_t *funcao;               /* por IrFuncao: 1 = alcancavel */
    uint8_t *trampolim;            /* por IrTrampolim: 1 = algum proc alcancado toma o endereco dele */
    int32_t *funcao_do_nome;       /* nome internado -> indice da IrFuncao candidata; -1 = nao e proc do programa */
    int32_t *trampolim_do_nome;    /* nome internado -> indice do IrTrampolim; -1 = sem trampolim */
    uint32_t *fila;                /* indices das IrFuncao alcancadas, na ordem em que foram marcadas */
    uint32_t total;                /* procs candidatos: todos, menos os 'test' fora do modo teste */
    uint32_t gerados;              /* procs alcancaveis (tamanho usado da fila) */
} BirAlcance;

/* Calcula o alcance de entrada->ir. Memoria esgotada e FalhaInterna. Libere com BirAlcanceLiberar. */
void BirAlcanceCalcular(const BirEntrada *entrada, BirAlcance *alcance);

/* Lista fechada SOMEM (D-M6-8): a chave interna do proc ("<tam><tag>_<nome>", nome_modulo.h) e uma chamada de std:mem que o release tira? Hoje: snapshot e report_since. */
bool BirNomeSomem(const char *chave);

/* A IrFuncao k / o IrTrampolim k entra na BIR? */
bool BirAlcanceFuncao(const BirAlcance *alcance, uint32_t k);
bool BirAlcanceTrampolim(const BirAlcance *alcance, uint32_t k);

void BirAlcanceLiberar(BirAlcance *alcance);

#endif
