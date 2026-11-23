/* costura_interno.h - tipos e aritmetica de faixa do passe `costura` (costura.c; ver costura.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - primeira versao: forma afim (raiz + c) de um vreg, faixa de uma raiz (param I64 que nunca e regravado) e decisao de CMP sem estouro.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --selftest
 *
 * Forma: o vreg vale raiz + c na entrada da raiz. pmin e pmax sao o menor e o maior valor parcial da cadeia de somas (sempre pmin <= 0 <= pmax):
 * a faixa da raiz so vale se nenhum passo da cadeia da a volta. Faixa: [lo, hi] de valores com sinal da raiz.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_COSTURA_INTERNO_H
#define FORJA_NATIVO_OPTIMIZER_COSTURA_INTERNO_H

#include <stdbool.h>
#include <stdint.h>

#include "nativo/bir/bir.h"

#define COSTURA_BLOCO_INSTR_MAX 6u   /* bloco candidato: instrucoes, contando o DESVIA */
#define COSTURA_DUP_BLOCOS_MAX 64u   /* por funcao: copias de bloco que o passe acrescenta */
#define COSTURA_DUP_INSTR_MAX 256u   /* por funcao: instrucoes copiadas (sem o DESVIA) */
#define COSTURA_CELULAS_MAX (1u << 18) /* blocos x vregs: acima disso a funcao nao e analisada (memoria limitada) */
#define COSTURA_RAIZES_MAX 8u        /* parametros que servem de raiz (faixa de cada um) */
#define COSTURA_C_MAX ((int64_t)1 << 60) /* |c| de uma forma: mantem pmin e pmax longe do estouro */

/* raiz < 0: o vreg nao tem forma. */
typedef struct {
    int32_t raiz;
    int64_t c, pmin, pmax;
} CosForma;

typedef struct {
    int64_t lo, hi;
} CosFaixa;

/* O CMP que o DESVIA do bloco usa: vreg (o bool) recebe (forma aux k), com a forma na esquerda e cond sinalizado (EQ, NE, LT_S, LE_S, GT_S, GE_S). */
typedef struct {
    bool valida;
    uint32_t vreg;
    CosForma forma;
    int64_t k;
    uint8_t cond;
} CosCond;

/* costura_faixa.c */
CosForma CosSemForma(void);
CosForma CosFormaDaRaiz(int32_t raiz);
CosForma CosFormaDe(const int32_t *raiz_de, const CosForma *tab, uint32_t v);
bool CosFormaSoma(CosForma f, int64_t d, CosForma *out);
CosForma CosMesclaForma(CosForma a, CosForma b);
bool CosFaixaSemVolta(CosFaixa iv, CosForma f);
bool CosConjunto(uint8_t cond, bool verdade, int64_t u, CosFaixa *out);
int CosDecide(const CosCond *r, const CosFaixa *faixas);
void CosRefinar(const CosCond *r, bool verdade, CosFaixa *faixas);

#endif
