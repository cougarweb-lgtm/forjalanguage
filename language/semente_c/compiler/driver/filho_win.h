/* filho_win.h - o programa do usuario no forja run/test (Windows): CreateProcess suspenso, stdout/stderr por pipe, Job Object com os limites de tempo, memoria e saida (contrato IA1, D-IA1-14 e b10).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b10: primeira versao (FilhoExecutar).
 * Verificacao: construir.bat && python -I ..\tests\ia_first\verificar_limites_b10.py --forja .\forja.exe
 *
 * O stderr do programa passa por um leitor que acrescenta explicacao, forma_certa e spec a cada linha de R-code (rcode_runtime.c); o stdout so e desviado pelo pipe quando ha limite de
 * saida (sem limite ele e o mesmo handle do forja, byte a byte como antes). O stdin e herdado. Com algum limite o filho e a arvore dele vivem num Job Object (KILL_ON_JOB_CLOSE): estouro
 * mata o job inteiro. Tempo = tempo de parede desde que o filho foi solto; memoria = o pico de memoria confirmada do job, lido a cada 5 ms (e uma vez mais no fim);
 * saida = bytes de stdout + stderr juntos, cortados no limite (o que saiu antes fica; o resto e contado e descartado).
 */
#ifndef FORJA_DRIVER_FILHO_WIN_H
#define FORJA_DRIVER_FILHO_WIN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *exe;                    /* caminho do executavel (vira argv[0]) */
    const char *const *args;            /* argv[1..], terminado por NULL */
    double limite_tempo_s;              /* 0 = sem limite */
    uint64_t limite_mem, limite_saida;  /* bytes; 0 = sem limite */
    bool enriquecer;                    /* acrescenta explicacao, forma_certa e spec as linhas de R-code do stderr */
} FilhoPedido;

typedef enum { FILHO_TERMINOU, FILHO_ESTOUROU_TEMPO, FILHO_ESTOUROU_MEMORIA, FILHO_ESTOUROU_SAIDA } FilhoFim;

typedef struct {
    FilhoFim fim;
    uint32_t codigo;                    /* exit code do filho (so vale em FILHO_TERMINOU) */
    uint64_t descartados;               /* bytes de saida alem do limite que foram descartados */
    uint64_t pico_mem;                  /* pico de memoria do job, em bytes (0 sem limite de memoria) */
} FilhoResultado;

/* Roda o filho ate o fim ou ate um limite estourar. false = nao iniciou (ou nao pode aplicar os limites); *erro = GetLastError(). */
bool FilhoExecutar(const FilhoPedido *pedido, FilhoResultado *resultado, uint32_t *erro);

#endif
