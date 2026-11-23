/* teste_analises.c - verificacoes das analises da BIR (nativo\optimizer\analises.h, M7a F1) em CFGs montados a mao: diamante, laco simples, aninhado,
 * com break, bloco inalcancavel e *_OV com bloco frio. Chamada pelo teste_nativo.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7a F1: primeira versao.
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/analises.h"

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (analises) linha %d: %s\n", linha, texto);
}

#define VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* ---- montagem: cada bloco e uma lista de instrucoes; vreg e so numero ---- */

static void Acrescentar(BirFuncao *f, uint32_t bloco, BirInstr instr)
{
    BirBloco *b = &f->blocos[bloco];

    if (b->n == b->cap) {
        b->cap = b->cap == 0 ? 4 : b->cap * 2;
        b->instrs = realloc(b->instrs, b->cap * sizeof *b->instrs);
    }
    b->instrs[b->n++] = instr;
}

static BirInstr InstrBase(uint8_t op)
{
    BirInstr i;

    memset(&i, 0, sizeof i);
    i.op = op;
    i.dst = BIR_SEM_VREG;
    i.classe = BC_I64;
    return i;
}

static BirOperando Vreg(uint32_t id)
{
    BirOperando o;

    memset(&o, 0, sizeof o);
    o.tipo = BO_VREG;
    o.classe = BC_I64;
    o.id = id;
    return o;
}

/* dst = constante. */
static void Def(BirFuncao *f, uint32_t bloco, uint32_t dst)
{
    BirInstr i = InstrBase(BIR_CONST);

    i.dst = dst;
    Acrescentar(f, bloco, i);
}

/* dst = a + b (op generico de 2 usos); op pode ser ADD, CMP ou ADD_OV (frio = alvo[0] do OV). */
static void Conta(BirFuncao *f, uint32_t bloco, uint8_t op, uint32_t dst, uint32_t a, uint32_t b)
{
    BirInstr i = InstrBase(op);

    i.dst = dst;
    i.a = Vreg(a);
    i.b = Vreg(b);
    Acrescentar(f, bloco, i);
}

static void Salta(BirFuncao *f, uint32_t bloco, uint32_t alvo)
{
    BirInstr i = InstrBase(BIR_SALTA);

    i.alvo[0] = alvo;
    Acrescentar(f, bloco, i);
}

static void Desvia(BirFuncao *f, uint32_t bloco, uint32_t cond, uint32_t verdade, uint32_t falso)
{
    BirInstr i = InstrBase(BIR_DESVIA);

    i.a = Vreg(cond);
    i.alvo[0] = verdade;
    i.alvo[1] = falso;
    Acrescentar(f, bloco, i);
}

static void Retorna(BirFuncao *f, uint32_t bloco, uint32_t vreg)
{
    BirInstr i = InstrBase(BIR_RETORNA);

    i.a = Vreg(vreg);
    Acrescentar(f, bloco, i);
}

static BirFuncao NovaFuncao(uint32_t nblocos, uint32_t nvregs, uint32_t nparams)
{
    BirFuncao f;

    memset(&f, 0, sizeof f);
    f.nblocos = nblocos;
    f.nvregs = nvregs;
    f.nparams = nparams;
    f.blocos = calloc(nblocos, sizeof *f.blocos);
    return f;
}

static void LiberarFuncao(BirFuncao *f)
{
    for (uint32_t b = 0; b < f->nblocos; b++)
        free(f->blocos[b].instrs);
    free(f->blocos);
}

/* Conjunto de vregs esperado como mascara (bit v = vreg v; ate 64 vregs nas fixtures). */
static uint64_t MascaraDoConjunto(const ConjuntoVivo *s)
{
    uint64_t m = 0;

    for (uint32_t k = 0; k < s->n; k++)
        m |= s->v[k] < 64u ? (uint64_t)1 << s->v[k] : 0;
    return m;
}

static bool VivoEntraIgual(const AnalisesBir *a, uint32_t bloco, uint64_t esperado)
{
    return MascaraDoConjunto(&a->vivo_entra[bloco]) == esperado;
}

static bool VivoSaiIgual(const AnalisesBir *a, uint32_t bloco, uint64_t esperado)
{
    return MascaraDoConjunto(&a->vivo_sai[bloco]) == esperado;
}

static bool PredsSao(const AnalisesBir *a, uint32_t bloco, uint32_t n, const uint32_t *esperado)
{
    if (a->pred_ini[bloco + 1] - a->pred_ini[bloco] != n)
        return false;
    return n == 0 || memcmp(a->pred + a->pred_ini[bloco], esperado, n * sizeof *esperado) == 0;
}

static bool ProfundidadesSao(const AnalisesBir *a, const uint32_t *esperado)
{
    return memcmp(a->prof_laco, esperado, a->nblocos * sizeof *esperado) == 0;
}

static bool Consistente(const BirFuncao *f, AnalisesBir *a)
{
    char causa[200];
    bool ok = AnalisesConferir(f, a, causa, sizeof causa);

    if (!ok)
        fprintf(stderr, "AnalisesConferir: %s\n", causa);
    return ok;
}

/* ---- fixtures ---- */

/* b0 -> b1 | b2 -> b3 ; v0 = parametro. */
static void TestaDiamante(const Contagem *c)
{
    BirFuncao f = NovaFuncao(4, 3, 1);
    AnalisesBir a;
    const uint32_t p3[] = {1, 2}, p1[] = {0}, p0[] = {0};
    const uint32_t prof[] = {0, 0, 0, 0};

    Def(&f, 0, 1);
    Desvia(&f, 0, 1, 1, 2);
    Conta(&f, 1, BIR_ADD, 2, 0, 0);
    Salta(&f, 1, 3);
    Def(&f, 2, 2);
    Salta(&f, 2, 3);
    Retorna(&f, 3, 2);
    AnalisesCalcular(&f, &a);
    VERIFICA(c, PredsSao(&a, 3, 2, p3) && PredsSao(&a, 1, 1, p1) && PredsSao(&a, 2, 1, p0) && PredsSao(&a, 0, 0, NULL));
    VERIFICA(c, a.nrpo == 4 && a.rpo[0] == 0 && a.rpo[3] == 3);
    VERIFICA(c, a.idom[0] == 0 && a.idom[1] == 0 && a.idom[2] == 0 && a.idom[3] == 0);
    VERIFICA(c, a.nlacos == 0 && ProfundidadesSao(&a, prof));
    VERIFICA(c, VivoEntraIgual(&a, 0, 1u << 0) && VivoEntraIgual(&a, 1, 1u << 0) && VivoEntraIgual(&a, 2, 0) && VivoEntraIgual(&a, 3, 1u << 2));
    VERIFICA(c, VivoSaiIgual(&a, 0, 1u << 0) && VivoSaiIgual(&a, 1, 1u << 2) && AnalisesVivoSai(&a, 2, 2) && !AnalisesVivoSai(&a, 3, 2));
    VERIFICA(c, Consistente(&f, &a));
    AnalisesLiberar(&a);
    LiberarFuncao(&f);
}

/* b0 -> b1 ; b1 -> b2 | b3 ; b2 -> b1 ; b3 retorna. v0 = limite, v1 = contador. */
static void TestaLacoSimples(const Contagem *c)
{
    BirFuncao f = NovaFuncao(4, 3, 1);
    AnalisesBir a;
    const uint32_t prof[] = {0, 1, 1, 0}, p1[] = {0, 2};

    Def(&f, 0, 1);
    Salta(&f, 0, 1);
    Conta(&f, 1, BIR_CMP, 2, 1, 0);
    Desvia(&f, 1, 2, 2, 3);
    Conta(&f, 2, BIR_ADD, 1, 1, 1);
    Salta(&f, 2, 1);
    Retorna(&f, 3, 1);
    AnalisesCalcular(&f, &a);
    VERIFICA(c, PredsSao(&a, 1, 2, p1));
    VERIFICA(c, a.nrpo == 4 && a.rpo[0] == 0 && a.rpo[1] == 1);
    VERIFICA(c, a.idom[1] == 0 && a.idom[2] == 1 && a.idom[3] == 1);
    VERIFICA(c, a.nlacos == 1 && a.lacos[0].cabecalho == 1 && a.lacos[0].ncorpo == 2 && a.lacos[0].corpo[0] == 1 && a.lacos[0].corpo[1] == 2);
    VERIFICA(c, ProfundidadesSao(&a, prof));
    VERIFICA(c, VivoEntraIgual(&a, 1, 3) && VivoSaiIgual(&a, 2, 3) && VivoEntraIgual(&a, 3, 2) && VivoEntraIgual(&a, 0, 1));
    VERIFICA(c, Consistente(&f, &a));
    AnalisesLiberar(&a);
    LiberarFuncao(&f);
}

/* Externo: b1 (cabecalho) ; interno: b3 (cabecalho) ; b6 incrementa o externo ; b5 retorna. v0 = limite, v1 externo, v3 interno. */
static void TestaLacoAninhado(const Contagem *c)
{
    BirFuncao f = NovaFuncao(7, 5, 1);
    AnalisesBir a;
    const uint32_t prof[] = {0, 1, 1, 2, 2, 0, 1};

    Def(&f, 0, 1);
    Salta(&f, 0, 1);
    Conta(&f, 1, BIR_CMP, 2, 1, 0);
    Desvia(&f, 1, 2, 2, 5);
    Def(&f, 2, 3);
    Salta(&f, 2, 3);
    Conta(&f, 3, BIR_CMP, 4, 3, 0);
    Desvia(&f, 3, 4, 4, 6);
    Conta(&f, 4, BIR_ADD, 3, 3, 1);
    Salta(&f, 4, 3);
    Retorna(&f, 5, 1);
    Conta(&f, 6, BIR_ADD, 1, 1, 1);
    Salta(&f, 6, 1);
    AnalisesCalcular(&f, &a);
    VERIFICA(c, a.nrpo == 7 && a.nlacos == 2);
    VERIFICA(c, a.lacos[0].cabecalho == 1 && a.lacos[0].ncorpo == 5 && a.lacos[1].cabecalho == 3 && a.lacos[1].ncorpo == 2);
    VERIFICA(c, ProfundidadesSao(&a, prof));
    VERIFICA(c, a.idom[3] == 2 && a.idom[6] == 3 && a.idom[5] == 1 && a.idom[4] == 3);
    VERIFICA(c, AnalisesDomina(&a, 1, 4) && !AnalisesDomina(&a, 4, 1) && AnalisesDomina(&a, 0, 6));
    VERIFICA(c, VivoEntraIgual(&a, 3, (1u << 0) | (1u << 1) | (1u << 3)) && VivoEntraIgual(&a, 6, (1u << 0) | (1u << 1)));
    VERIFICA(c, Consistente(&f, &a));
    AnalisesLiberar(&a);
    LiberarFuncao(&f);
}

/* b1 -> b2 | b4 ; b2 -> b4 (break) | b3 ; b3 -> b1 ; b4 retorna. */
static void TestaLacoComBreak(const Contagem *c)
{
    BirFuncao f = NovaFuncao(5, 4, 1);
    AnalisesBir a;
    const uint32_t prof[] = {0, 1, 1, 1, 0}, p4[] = {1, 2};

    Def(&f, 0, 1);
    Salta(&f, 0, 1);
    Conta(&f, 1, BIR_CMP, 2, 1, 0);
    Desvia(&f, 1, 2, 2, 4);
    Conta(&f, 2, BIR_CMP, 3, 1, 0);
    Desvia(&f, 2, 3, 4, 3);
    Conta(&f, 3, BIR_ADD, 1, 1, 1);
    Salta(&f, 3, 1);
    Retorna(&f, 4, 1);
    AnalisesCalcular(&f, &a);
    VERIFICA(c, PredsSao(&a, 4, 2, p4));
    VERIFICA(c, a.idom[4] == 1 && a.idom[3] == 2 && a.nlacos == 1 && a.lacos[0].ncorpo == 3);
    VERIFICA(c, ProfundidadesSao(&a, prof));
    VERIFICA(c, VivoEntraIgual(&a, 4, 1u << 1) && VivoEntraIgual(&a, 2, 3));
    VERIFICA(c, Consistente(&f, &a));
    AnalisesLiberar(&a);
    LiberarFuncao(&f);
}

/* b1 e b2 nao tem caminho da entrada; b1 salta para b0 (predecessor inalcancavel conta em pred, nao em idom). */
static void TestaInalcancavel(const Contagem *c)
{
    BirFuncao f = NovaFuncao(3, 3, 1);
    AnalisesBir a;
    const uint32_t p0[] = {1}, prof[] = {0, 0, 0};

    Retorna(&f, 0, 0);
    Def(&f, 1, 1);
    Salta(&f, 1, 0);
    Def(&f, 2, 2);
    Salta(&f, 2, 1);
    AnalisesCalcular(&f, &a);
    VERIFICA(c, a.nrpo == 1 && a.pos_rpo[1] == BIR_SEM_BLOCO && a.pos_rpo[2] == BIR_SEM_BLOCO);
    VERIFICA(c, PredsSao(&a, 0, 1, p0) && a.idom[0] == 0 && a.idom[1] == BIR_SEM_BLOCO && !AnalisesDomina(&a, 0, 1));
    VERIFICA(c, a.nlacos == 0 && ProfundidadesSao(&a, prof));
    VERIFICA(c, VivoEntraIgual(&a, 0, 1u << 0) && VivoEntraIgual(&a, 1, 0));
    VERIFICA(c, Consistente(&f, &a));
    AnalisesLiberar(&a);
    LiberarFuncao(&f);
}

/* ADD_OV v1 = v0 + v0 com frio b1 (que usa v0 e nao volta): a aresta nasce no meio do bloco e v0 fica vivo pelo frio. */
static void TestaContaComFrio(const Contagem *c)
{
    BirFuncao f = NovaFuncao(2, 3, 1);
    AnalisesBir a;
    BirInstr armadilha = InstrBase(BIR_ARMADILHA);
    BirInstr soma = InstrBase(BIR_ADD_OV);
    const uint32_t p1[] = {0};

    soma.dst = 1;
    soma.a = Vreg(0);
    soma.b = Vreg(0);
    soma.alvo[0] = 1;
    Acrescentar(&f, 0, soma);
    Retorna(&f, 0, 1);
    armadilha.nargs = 1;
    armadilha.args = calloc(1, sizeof *armadilha.args);
    armadilha.args[0] = Vreg(0);
    Acrescentar(&f, 1, armadilha);
    AnalisesCalcular(&f, &a);
    VERIFICA(c, PredsSao(&a, 1, 1, p1) && a.nrpo == 2 && a.idom[1] == 0);
    VERIFICA(c, VivoEntraIgual(&a, 1, 1u << 0) && VivoSaiIgual(&a, 0, 0));
    VERIFICA(c, VivoEntraIgual(&a, 0, 1u << 0) && !AnalisesVivoEntra(&a, 0, 1));
    free(armadilha.args);
    AnalisesLiberar(&a);
    LiberarFuncao(&f);
}

void AnalisesVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    TestaDiamante(&c);
    TestaLacoSimples(&c);
    TestaLacoAninhado(&c);
    TestaLacoComBreak(&c);
    TestaInalcancavel(&c);
    TestaContaComFrio(&c);
}
