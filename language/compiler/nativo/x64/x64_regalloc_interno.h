/* x64_regalloc_interno.h - intervalos de vida dos vregs do alocador (M7a F3). Nao incluir fora de x64_regalloc*.c.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - F1 etapa 2: IntervaloVreg.regra_antiga (alvo de CHAMA_IND e destino de agregado de CHAMA: mantem a regra antiga de cruzamento).
 *            0.3.0 - M7b R3: IntervaloVreg.def_em_laco e .carregada (base da guarda G1 e do rastro FORJA_ALOC_RASTRO da regra 2).
 *            0.2.0- M7a F4: IntervaloVreg.real (F32/F64 disputam os xmm; elegivel vale para inteiro e real).
 *            0.1.0 - M7a F3: primeira versao.
 * Verificacao: construir_rapido.py && teste_nativo.exe
 *
 * Posicoes: 0 = entrada da funcao (onde os parametros nascem); depois uma por instrucao, blocos na ordem do RPO. O intervalo de um vreg e o fecho convexo de todas as posicoes
 * onde ele e definido, usado ou vivo na entrada/saida de um bloco: como a vivacidade ja cobre o laco inteiro, o intervalo que cruza um laco se estende ate o fim dele.
 * Dois intervalos conflitam se se tocam (inicio <= fim do outro): dst e operando da mesma instrucao nunca dividem registrador.
 */
#ifndef FORJA_NATIVO_X64_X64_REGALLOC_INTERNO_H
#define FORJA_NATIVO_X64_X64_REGALLOC_INTERNO_H

#include "common/arena.h"
#include "nativo/bir/analises.h"

#define REGALLOC_SEM_POS UINT32_MAX

typedef struct {
    uint32_t inicio, fim;     /* REGALLOC_SEM_POS no inicio = vreg que nunca aparece */
    uint64_t peso;            /* usos e definicoes x 10^profundidade de laco */
    bool elegivel;            /* inteiro ou real, nao e parametro agregado por bytes e aparece em alguma instrucao */
    bool real;                /* F32 ou F64: disputa os xmm em vez dos registradores inteiros */
    bool cruza_chamada;       /* alguma chamada estritamente dentro de (inicio, fim): r8..r10 e xmm2..5 morrem nela (F1: o argumento que morre na chamada nao cruza) */
    bool def_em_laco;         /* M7b R3: alguma definicao do vreg esta num bloco de laco (profundidade > 0) */
    bool carregada;           /* M7b R3: def_em_laco e vivo na entrada do cabecalho de algum laco (acumulador, inducao: o valor atravessa a volta) */
    bool regra_antiga;        /* F1 etapa 2: o vreg e lido depois da chamada (destino de agregado de CHAMA) ou e o alvo de CHAMA_IND: vale a regra antiga (inclui a chamada em fim) */
} IntervaloVreg;

typedef struct {
    Arena *arena;             /* entrada: dona dos vetores abaixo */
    uint32_t nvregs, npos;
    IntervaloVreg *iv;        /* nvregs entradas, indexadas pelo vreg */
    uint32_t *chamadas_ate;   /* npos + 1 entradas: chamadas em posicoes <= p */
    bool tem_sysv;            /* a funcao chama foreign SysV (alvo linux) */
} Intervalos;

/* Calcula os intervalos de f sobre as analises a; saida->arena (preenchida por quem chama) da a memoria. */
void RegallocIntervalos(const BirPrograma *programa, const BirFuncao *f, const AnalisesBir *a, Intervalos *saida);

#endif
