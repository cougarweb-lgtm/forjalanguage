/* diag_forma.h - interface interna entre diag.c, diag_forma.c e diag_forma_tab.c: a forma por ocorrencia montada na saida (IA1-b08).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b08: primeira versao (FormaTextos, FormaDaCausa, DiagFormaDaOcorrencia e a tabela FORMAS_DA_CAUSA).
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\ia_first\verificar_forma_b08.py --forja compiler\forja.exe
 *
 * So o diag.c inclui este arquivo; quem imprime diagnostico usa o diag.h. A explicacao e a forma_certa de uma ocorrencia vem, nesta ordem, de:
 * (1) o que o passo da analise escreveu com DiagForma (b07: E0301, E0101 e o que o b08 acrescentou); (2) a regra da tabela cujo trecho aparece na causa
 * (mesmo codigo); (3) a frase geral: a linha do .fj, a causa e a forma_certa do catalogo. Nunca o texto fixo do catalogo sozinho.
 *
 * Marcas nos textos da tabela: $O = "linha N, coluna M de arq.fj"; $L = a linha do .fj (sem recuo nem comentario); $T = o trecho do .fj a partir do
 * offset do erro (ate virgula, fecha-parenteses, '//' ou fim da linha); $C = a causa; $1 e $2 = o 1o e o 2o nome entre aspas simples da causa (sem nome, o $T); $$ = um '$'.
 */
#ifndef FORJA_DIAG_FORMA_H
#define FORJA_DIAG_FORMA_H

#include <stddef.h>

#include "common/diag.h"
#include "common/fonte.h"

enum { FORMA_TEXTO_MAX = 720 };

typedef struct { char explicacao[FORMA_TEXTO_MAX], forma[FORMA_TEXTO_MAX]; } FormaTextos;

/* Uma regra: no diagnostico de 'codigo' cuja causa contem 'trecho_da_causa', valem estes dois textos (com as marcas acima). A 1a regra que casa vence. */
typedef struct { DiagCodigo codigo; const char *trecho_da_causa, *explicacao, *forma; } FormaDaCausa;

extern const FormaDaCausa FORMAS_DA_CAUSA[];
extern const size_t FORMAS_DA_CAUSA_QTD;

/* Monta os dois textos da ocorrencia. 'f' e a fonte onde o offset do diagnostico vale; forma_catalogo e a forma_certa do codigo (a frase geral a usa). */
void DiagFormaDaOcorrencia(const Diag *d, const Fonte *f, const char *forma_catalogo, FormaTextos *saida);

#endif
