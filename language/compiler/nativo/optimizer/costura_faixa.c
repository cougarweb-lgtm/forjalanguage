/* costura_faixa.c - aritmetica de forma e faixa do passe `costura` (ver costura_interno.h): so calcula, nao muda a BIR.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - primeira versao: soma de constante na forma, conjunto de valores que satisfaz o CMP com sinal, decisao e refino da faixa.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --selftest
 *
 * Tudo com inteiro de 64 bits e sem estouro: a faixa so e usada quando nenhum valor de raiz + pmin ou raiz + pmax sai da faixa de 64 bits
 * (o estouro da BIR da a volta; um valor que deu a volta nao segue a conta). Conjunto vazio = faixa com lo > hi (1, 0).
 */
#include <stdint.h>

#include "nativo/optimizer/costura_interno.h"

static int64_t Menor(int64_t a, int64_t b)
{
    return a < b ? a : b;
}

static int64_t Maior(int64_t a, int64_t b)
{
    return a > b ? a : b;
}

/* u = k - c sem estouro (c de COSTURA_C_MAX). false se a conta da a volta. */
static bool CosUmbral(int64_t k, int64_t c, int64_t *u)
{
    if (c >= 0) {
        if (k < INT64_MIN + c)
            return false;
        *u = k - c;
        return true;
    }
    if (k > INT64_MAX + c)
        return false;
    *u = k - c;
    return true;
}

CosForma CosSemForma(void)
{
    return (CosForma){.raiz = -1};
}

CosForma CosFormaDaRaiz(int32_t raiz)
{
    return (CosForma){.raiz = raiz};
}

/* Forma do vreg v: a raiz se v e um parametro que serve de raiz (raiz_de >= 0), senao a forma que ele tem em tab. */
CosForma CosFormaDe(const int32_t *raiz_de, const CosForma *tab, uint32_t v)
{
    if (raiz_de[v] >= 0)
        return CosFormaDaRaiz(raiz_de[v]);
    return tab[v];
}

/* Forma + d (constante). false se nao tem forma, se estoura ou se |c| passa de COSTURA_C_MAX. */
bool CosFormaSoma(CosForma f, int64_t d, CosForma *out)
{
    int64_t novo;

    if (f.raiz < 0)
        return false;
    if ((d > 0 && f.c > INT64_MAX - d) || (d < 0 && f.c < INT64_MIN - d))
        return false;
    novo = f.c + d;
    if (novo > COSTURA_C_MAX || novo < -COSTURA_C_MAX)
        return false;
    *out = (CosForma){.raiz = f.raiz, .c = novo, .pmin = Menor(f.pmin, novo), .pmax = Maior(f.pmax, novo)};
    return true;
}

/* Juncao de duas formas: so sobrevive se as duas dizem o mesmo (raiz e c); a cadeia passa a cobrir os dois caminhos. */
CosForma CosMesclaForma(CosForma a, CosForma b)
{
    if (a.raiz < 0 || a.raiz != b.raiz || a.c != b.c)
        return CosSemForma();
    return (CosForma){.raiz = a.raiz, .c = a.c, .pmin = Menor(a.pmin, b.pmin), .pmax = Maior(a.pmax, b.pmax)};
}

/* Com a faixa inteira, raiz + pmin e raiz + pmax cabem em 64 bits (pmin <= 0 <= pmax): so entao a forma vale como raiz + c. */
bool CosFaixaSemVolta(CosFaixa iv, CosForma f)
{
    if (f.raiz < 0)
        return false;
    return iv.lo >= INT64_MIN - f.pmin && iv.hi <= INT64_MAX - f.pmax;
}

/* Faixa dos t com `t cond u` verdadeiro (EQ, LT_S, LE_S, GT_S, GE_S); vazia (1, 0) quando u esta no limite e nao sobra t. */
static bool ConjuntoVerdadeiro(uint8_t cond, int64_t u, CosFaixa *out)
{
    *out = (CosFaixa){.lo = 1, .hi = 0};
    switch (cond) {
    case BCOND_EQ: *out = (CosFaixa){.lo = u, .hi = u}; return true;
    case BCOND_LT_S: if (u != INT64_MIN) *out = (CosFaixa){.lo = INT64_MIN, .hi = u - 1}; return true;
    case BCOND_LE_S: *out = (CosFaixa){.lo = INT64_MIN, .hi = u}; return true;
    case BCOND_GT_S: if (u != INT64_MAX) *out = (CosFaixa){.lo = u + 1, .hi = INT64_MAX}; return true;
    case BCOND_GE_S: *out = (CosFaixa){.lo = u, .hi = INT64_MAX}; return true;
    default: return false;
    }
}

/* Faixa dos t com `t cond u` falso (NE, LT_S, LE_S, GT_S, GE_S); vazia no limite, como acima. */
static bool ConjuntoFalso(uint8_t cond, int64_t u, CosFaixa *out)
{
    *out = (CosFaixa){.lo = 1, .hi = 0};
    switch (cond) {
    case BCOND_NE: *out = (CosFaixa){.lo = u, .hi = u}; return true;
    case BCOND_LT_S: *out = (CosFaixa){.lo = u, .hi = INT64_MAX}; return true;
    case BCOND_LE_S: if (u != INT64_MAX) *out = (CosFaixa){.lo = u + 1, .hi = INT64_MAX}; return true;
    case BCOND_GT_S: *out = (CosFaixa){.lo = INT64_MIN, .hi = u}; return true;
    case BCOND_GE_S: if (u != INT64_MIN) *out = (CosFaixa){.lo = INT64_MIN, .hi = u - 1}; return true;
    default: return false;
    }
}

/* Valores de t (raiz) para os quais `t cond u` vale (verdade) ou nao vale. false = nao e um intervalo (NE verdadeiro, EQ falso). */
bool CosConjunto(uint8_t cond, bool verdade, int64_t u, CosFaixa *out)
{
    return verdade ? ConjuntoVerdadeiro(cond, u, out) : ConjuntoFalso(cond, u, out);
}

/* Decide o CMP sobre a faixa da raiz: 1 = sempre verdadeiro, 0 = sempre falso, -1 = depende do valor (ou a conta daria a volta). */
int CosDecide(const CosCond *r, const CosFaixa *faixas)
{
    CosFaixa iv = faixas[r->forma.raiz], t;
    int64_t u;

    if (!CosFaixaSemVolta(iv, r->forma) || !CosUmbral(r->k, r->forma.c, &u))
        return -1;
    if (r->cond == BCOND_NE) {
        if (u < iv.lo || u > iv.hi)
            return 1;
        return iv.lo == iv.hi ? 0 : -1;
    }
    if (!CosConjunto(r->cond, true, u, &t))
        return -1;
    if (t.lo <= iv.lo && iv.hi <= t.hi)
        return 1;
    if (Maior(t.lo, iv.lo) > Menor(t.hi, iv.hi))
        return 0;
    return -1;
}

/* Estreita a faixa da raiz ao ramo do CMP (verdade = ramo verdadeiro do DESVIA). Sem refino se a conta daria a volta ou se a faixa ficaria vazia (mantem a faixa maior, que e segura). */
void CosRefinar(const CosCond *r, bool verdade, CosFaixa *faixas)
{
    CosFaixa *iv = &faixas[r->forma.raiz], t;
    int64_t u, nlo, nhi;

    if (!CosFaixaSemVolta(*iv, r->forma) || !CosUmbral(r->k, r->forma.c, &u) || !CosConjunto(r->cond, verdade, u, &t))
        return;
    nlo = Maior(iv->lo, t.lo);
    nhi = Menor(iv->hi, t.hi);
    if (nlo <= nhi) {
        iv->lo = nlo;
        iv->hi = nhi;
    }
}
