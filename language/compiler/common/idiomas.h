/* idiomas.h - tabela dos idiomas de outras linguagens que a Forja recusa com E0110 ("forma de outra linguagem; na Forja e X"), contrato IA1, passo L1.
 * Uma linha por idioma em IDIOMAS (idiomas.c): a causa, a explicacao e a forma_certa. Quem DETECTA o idioma (lexer, parser; no L2 a semantica) chama IdiomaRelatar
 * e acrescenta a sugestao aplicavel; o texto vive so na tabela. Para acrescentar um idioma: um IDI_* no fim do enum, uma linha na tabela, o detector.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - O0 P13: IDI_XOR_CIRCUNFLEXO ('a ^ b' como xor de C, Java e JS; na Forja e 'a ~ b').
 *            0.2.0 - IA1-L2: 10 idiomas semanticos (nil/null/None, True/False, range, string/String, [1, 2], printf/console.log, x.len(), x.push(v), import "os", throw).
 *            0.1.0 - IA1-L1: primeira versao (15 idiomas lexicos e sintaticos).
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\ia_first\verificar_e0110_l1.py --forja compiler\forja.exe && python ..\tests\ia_first\verificar_m_d4.py --forja compiler\forja.exe
 */
#ifndef FORJA_COMMON_IDIOMAS_H
#define FORJA_COMMON_IDIOMAS_H

#include "common/diag.h"
#include "common/diag_forma.h"

typedef enum {
    IDI_PONTO_E_VIRGULA, IDI_FOR_ESTILO_C, IDI_E_LOGICO, IDI_OU_LOGICO, IDI_NEGACAO, IDI_INCREMENTO, IDI_TERNARIO, IDI_TRY_RUST,
    IDI_BLOCO_NA_LINHA, IDI_PALAVRA_DE_PROC, IDI_ATRIBUICAO_CURTA, IDI_ELIF, IDI_COMENTARIO_DE_BLOCO, IDI_TRY_CATCH, IDI_MAIN_COM_RETORNO,
    IDI_NULO, IDI_BOOL_ALHEIO, IDI_RANGE, IDI_TIPO_TEXTO, IDI_ARRAY_COLCHETE, IDI_IMPRIME, IDI_TAMANHO, IDI_ACRESCENTAR, IDI_IMPORT_SEM_STD, IDI_THROW,
    IDI_XOR_CIRCUNFLEXO,
    IDI_QTD
} IdiomaId;

/* causa: formato printf com UM %s (o texto achado, ex. "func"); forma: explicacao e forma_certa com as marcas do diag_forma.h, achadas pelo trecho_da_causa. */
typedef struct { IdiomaId id; const char *causa; FormaDaCausa forma; } Idioma;

extern const Idioma IDIOMAS[IDI_QTD];

/* Relata o E0110 do idioma em 'off' (achado entra no %s da causa). Devolve false, como DiagRelatar. */
bool IdiomaRelatar(Diag *d, IdiomaId id, uint32_t off, const char *achado);

/* IA1-L2: a troca de um idioma semantico ou sintatico: ate DIAG_EDICOES_MAX edicoes de uma linha, montadas por quem detecta e abertas por IdiomaSugerir. */
typedef struct { DiagEdicao itens[DIAG_EDICOES_MAX]; uint32_t n; bool falhou; } IdiomaTroca;

#define IDIOMA_TROCA_VAZIA ((IdiomaTroca){.n = 0, .falhou = false})

/* Acrescenta a edicao [ini, fim) -> novo em 't' (de 'texto', a Fonte). Edicao que nao cabe (varias linhas, muito longa, 3a) marca t->falhou. */
void IdiomaTrocaSomar(IdiomaTroca *t, const char *texto, uint32_t ini, uint32_t fim, const char *novo);

/* Abre a sugestao do E0110 ja relatado: aplicavel na confianca dada se 't' tem edicoes que cabem; senao so_dica (a descricao e a mesma). */
void IdiomaSugerir(Diag *d, DiagConfianca confianca, const char *descricao, const IdiomaTroca *t);

/* Sugestao CERTA de trocar [ini, fim) por 'palavra' no texto da Fonte, com um espaco de cada lado quando o vizinho nao e espaco (para a&&b virar a and b). */
void IdiomaSugerirPalavra(Diag *d, const Fonte *f, uint32_t ini, uint32_t fim, const char *palavra);

#endif
