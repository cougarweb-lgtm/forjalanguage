/* rcode_runtime.h - R-code enriquecido pelo driver (contrato IA1, D-IA1-14): o forja run/test le o stderr do programa e acrescenta explicacao, forma_certa e spec do catalogo
 * a cada linha JSON de R-code que o runtime escreveu. Zero byte novo no exe do usuario: o runtime segue escrevendo codigo, tipo, arquivo, linha e causa.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b10: primeira versao (RcodeEnriquecerLinha e o filtro por bytes RcodeFiltro).
 * Verificacao: construir.bat && python -I ..\tests\ia_first\verificar_limites_b10.py --forja .\forja.exe
 *
 * Linha enriquecida: a original sem o '}' final, mais , "explicacao": ..., "forma_certa": ..., "spec": ... e o '}' com o mesmo fim de linha (LF ou CRLF).
 * Linha que nao e de R-code do catalogo, ou que ja tem forma_certa, passa intacta (byte a byte).
 */
#ifndef FORJA_DRIVER_RCODE_RUNTIME_H
#define FORJA_DRIVER_RCODE_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

#include "common/buf.h"

enum { RCODE_LINHA_MAX = 4096 };

/* A linha (com o fim de linha, se tiver) em 'saida': enriquecida se for JSON de R-code do catalogo, senao igual. true = enriqueceu. */
bool RcodeEnriquecerLinha(const char *linha, size_t tam, Buf *saida);

/* Filtro por fluxo: cada linha que comeca com {"codigo": "R vira candidata e so sai quando a linha fecha; o resto passa direto, na hora. */
typedef struct {
    char linha[RCODE_LINHA_MAX];
    size_t n;
    bool passando;      /* a linha atual nao e candidata: os bytes saem direto ate o fim dela */
} RcodeFiltro;

void RcodeFiltroIniciar(RcodeFiltro *f);

/* Alimenta o filtro com dados[0..n); o que ja pode sair e anexado a 'saida'. */
void RcodeFiltroAlimentar(RcodeFiltro *f, const char *dados, size_t n, Buf *saida);

/* Fim do fluxo: o que ficou retido (linha candidata sem fim de linha) sai como veio. */
void RcodeFiltroFechar(RcodeFiltro *f, Buf *saida);

#endif
