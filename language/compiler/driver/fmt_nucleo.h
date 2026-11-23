/* fmt_nucleo.h - o motor do 'forja fmt': formata o texto de UM arquivo .fj pela forma canonica (F1, plano f1/f2) trocando so os VAOS entre os tokens (nenhum token e criado,
 * apagado ou movido). Cada vao fora da forma vira uma Edicao do motor de edicoes (driver\edicoes.h) com uma regra.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - F1 P1a: FmtFormatar e FmtResultado (so o nucleo; o 'forja fmt --stdin' e o cmd_fmt.c do P1b chamam).
 * Verificacao: construir.bat && python tests\fmt\verificar_fmt.py --forja compiler\forja.exe
 *
 * Forma canonica (R1..R7): UTF-8 sem BOM e LF; termina em exatamente um '\n', sem linha em branco no comeco nem no fim; no maximo 1 linha em branco seguida; sem espaco no fim da
 * linha; indentacao de 4 espacos por nivel ('case' e 'default' no nivel do 'switch'); espacos entre tokens por tabela fechada de pares (fmt_espaco.c); 1 espaco antes do '//' final.
 * Regras (FmtDesvio.regra e Edicoes): "bom", "fim_de_linha" (CRLF), "fim_de_arquivo", "linha_em_branco", "espaco_final", "indentacao", "espaco", "comentario".
 */
#ifndef FORJA_DRIVER_FMT_NUCLEO_H
#define FORJA_DRIVER_FMT_NUCLEO_H

#include "common/arena.h"
#include "common/buf.h"
#include "common/vec.h"
#include "driver/edicoes.h"

typedef enum { FMT_CANONICO, FMT_MUDARIA, FMT_NAO_ANALISADO } FmtEstado;

/* O 1o desvio do arquivo (linha e coluna em bytes a partir de 1, no texto SEM BOM e SEM CR). */
typedef struct { uint32_t linha, coluna; const char *regra; } FmtDesvio;

/* Tudo vive no resultado; libere com FmtResultadoLiberar. Zere a struct antes (FmtResultado r = {0}).
 * normalizado: o texto sem BOM e com LF (a base das posicoes das Edicoes); formatado: o texto canonico (so se estado != nao_analisado).
 * edicoes: Vec de Edicao e regras: Vec de const char * (uma por Edicao, mesma ordem), em ordem de posicao; BOM e CRLF NAO sao Edicoes (tinha_bom, tinha_crlf).
 * desvios = Edicoes + (BOM ? 1 : 0) + (CRLF ? 1 : 0). nao_analisado: codigo = o do 1o diagnostico ("E0101"; "" se nao e erro de sintaxe), linha/coluna dele, motivo = a frase. */
typedef struct {
    FmtEstado estado;
    bool tinha_bom, tinha_crlf;
    uint32_t desvios;
    FmtDesvio primeiro;
    Buf normalizado, formatado;
    Vec edicoes, regras;
    char codigo[8];
    uint32_t diag_linha, diag_coluna;
    char motivo[512];
    Arena arena;
} FmtResultado;

/* Formata fonte[0..tam) (como lido do arquivo: pode ter BOM e CRLF). So lexer e parser (sem imports, sem semantica): arquivo com erro de sintaxe, ou com token de 65535 bytes ou mais,
 * fica nao_analisado. */
void FmtFormatar(const char *fonte, size_t tam, FmtResultado *r);

void FmtResultadoLiberar(FmtResultado *r);

#endif
