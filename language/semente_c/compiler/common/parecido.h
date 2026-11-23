/* parecido.h - "voce quis dizer" (B02): distancia Damerau-Levenshtein entre nomes e os candidatos mais parecidos (ate 3).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-b14: ParecidoNome.pos (onde o candidato foi declarado) e ParecidosVerEm; ParecidosVer segue igual (pos = PARECIDO_SEM_POS).
 *            0.1.0 - Linguagem v1.11 L5: Parecido (OSA, nomes ate 64 bytes), ParecidosIniciar e ParecidosVer (ordem por distancia e nome).
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_PARECIDO_H
#define FORJA_PARECIDO_H

#include "common/base.h"

#define PARECIDOS_MAX 3      /* quantos nomes a sugestao devolve */
#define PARECIDO_TAM 64      /* nomes mais longos que isso nao se comparam */
#define PARECIDO_LONGE 1000u /* distancia devolvida quando um dos nomes passa de PARECIDO_TAM */
#define PARECIDO_SEM_POS 0xFFFFFFFFu /* ParecidoNome.pos de quem nao diz onde o candidato mora */

/* Distancia de Damerau-Levenshtein na forma OSA: insercao, remocao, troca e transposicao de letras vizinhas (ab -> ba custa 1). */
uint32_t Parecido(const char *a, size_t na, const char *b, size_t nb);

typedef struct { const char *texto; size_t tam; uint32_t distancia; uint32_t pos; } ParecidoNome; /* pos = offset da declaracao do candidato (b14) */

/* Candidatos a um nome escrito errado. Entra so quem esta a distancia 1..limite; limite = min(2, max(1, len(alvo)/3)).
 * nomes[] vai ordenado por (distancia, nome). Os textos sao os do chamador (o interner): nao sao copiados. */
typedef struct {
    const char *alvo;
    size_t tam_alvo;
    uint32_t limite;
    uint32_t qtd;
    ParecidoNome nomes[PARECIDOS_MAX];
} Parecidos;

void ParecidosIniciar(Parecidos *p, const char *alvo, size_t tam);

/* Oferece um candidato: fica na lista se for parecido e couber entre os 3 melhores. */
void ParecidosVer(Parecidos *p, const char *nome, size_t tam);

/* Igual a ParecidosVer, guardando 'pos' (o offset onde o candidato foi declarado) para o relacionado "parecido" do diagnostico. */
void ParecidosVerEm(Parecidos *p, const char *nome, size_t tam, uint32_t pos);

#endif
