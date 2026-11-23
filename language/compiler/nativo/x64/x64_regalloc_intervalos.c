/* x64_regalloc_intervalos.c - intervalos de vida dos vregs sobre o RPO, com o peso de cada um (M7a F3). Ver x64_regalloc_interno.h.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - F1 etapa 2: cruza_chamada so por chamada ESTRITAMENTE dentro do intervalo (o argumento que morre na chamada nao cruza); alvo de
 *                    CHAMA_IND, destino de agregado de CHAMA e funcao com chamada SysV mantem a regra antiga (IntervaloVreg.regra_antiga).
 *            0.3.2 - S1b (D0703): BC_V128 e elegivel e disputa o banco xmm (BirClasseEhXmm).
 *            0.3.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.3.0 - M7b R3: marca def_em_laco e carregada (vreg definido em laco e vivo no cabecalho de laco).
 *            0.2.1- M7b F2: EstenderPorConjunto le o ConjuntoVivo esparso da vivacidade (analises.h 0.2.0).
 *            0.2.0 - M7a F4: vreg real tambem e elegivel (IntervaloVreg.real separa os bancos).
 *            0.1.0 - M7a F3: primeira versao.
 * Verificacao: construir_rapido.py && teste_nativo.exe
 */
#include "nativo/x64/x64_interno.h"
#include "nativo/x64/x64_regalloc_interno.h"

#define PROFUNDIDADE_MAX 6u /* acima disso o peso nao cresce mais (10^6 cabe folgado em uint64 mesmo somado) */
static const uint64_t PESO_DA_PROFUNDIDADE[PROFUNDIDADE_MAX + 1u] = {1, 10, 100, 1000, 10000, 100000, 1000000};

typedef struct {
    const BirPrograma *programa;
    const BirFuncao *f;
    const AnalisesBir *analises;
    Intervalos *saida;
    uint32_t *pos_ini, *pos_fim;   /* por bloco: primeira e ultima posicao (so os alcancaveis) */
} Contexto;

static void Estender(IntervaloVreg *iv, uint32_t pos)
{
    if (iv->inicio == REGALLOC_SEM_POS || pos < iv->inicio)
        iv->inicio = pos;
    if (pos > iv->fim)
        iv->fim = pos;
}

static void Tocar(Intervalos *s, uint32_t vreg, uint32_t pos, uint64_t peso)
{
    Estender(&s->iv[vreg], pos);
    s->iv[vreg].peso += peso;
}

static void TocarOperando(Intervalos *s, const BirOperando *o, uint32_t pos, uint64_t peso)
{
    if (o->tipo == BO_VREG && o->id < s->nvregs)
        Tocar(s, o->id, pos, peso);
}

/* Chamada ao runtime, a proc, indireta ou a armadilha: destroi r8..r10 e le os argumentos em ordem (um argumento pode cair num registrador que outro ainda le). */
static bool EhChamada(uint8_t op)
{
    return op == BIR_CHAMA || op == BIR_CHAMA_IND || op == BIR_ARMADILHA;
}

static void VisitarInstrucao(Contexto *c, const BirInstr *i, uint32_t pos, uint64_t peso)
{
    Intervalos *s = c->saida;

    if (i->dst != BIR_SEM_VREG && i->dst < s->nvregs) {
        Tocar(s, i->dst, pos, peso);
        if (peso > 1u) /* peso = 10^profundidade: > 1 so dentro de laco */
            s->iv[i->dst].def_em_laco = true;
    }
    TocarOperando(s, &i->a, pos, peso);
    TocarOperando(s, &i->b, pos, peso);
    for (uint32_t k = 0; k < i->nargs; k++)
        TocarOperando(s, &i->args[k], pos, peso);
    if (i->op == BIR_CHAMA_IND && i->b.tipo == BO_VREG && i->b.id < s->nvregs)
        s->iv[i->b.id].regra_antiga = true;
    if (i->op == BIR_CHAMA && i->a.tipo == BO_VREG && i->a.id < s->nvregs) /* destino de agregado: GuardarRetornoAgregado o le depois da chamada */
        s->iv[i->a.id].regra_antiga = true;
    if (EhChamada(i->op))
        s->chamadas_ate[pos] = 1;
    if (SysvEhChamadaDoPrograma(c->programa, i))
        s->tem_sysv = true;
}

/* Numera as instrucoes dos blocos alcancaveis na ordem do RPO e acumula definicoes, usos e pesos. */
static void PosicionarBlocos(Contexto *c)
{
    const AnalisesBir *a = c->analises;
    uint32_t pos = 0;

    for (uint32_t r = 0; r < a->nrpo; r++) {
        uint32_t b = a->rpo[r];
        const BirBloco *bloco = &c->f->blocos[b];
        uint32_t prof = a->prof_laco[b] < PROFUNDIDADE_MAX ? a->prof_laco[b] : PROFUNDIDADE_MAX;

        c->pos_ini[b] = pos + 1;
        for (uint32_t k = 0; k < bloco->n; k++)
            VisitarInstrucao(c, &bloco->instrs[k], ++pos, PESO_DA_PROFUNDIDADE[prof]);
        c->pos_fim[b] = pos;
    }
}

static void EstenderPorConjunto(Intervalos *s, const ConjuntoVivo *conjunto, uint32_t pos)
{
    for (uint32_t k = 0; k < conjunto->n; k++)
        Estender(&s->iv[conjunto->v[k]], pos);
}

/* Vivo na entrada ou na saida de um bloco = vivo na primeira ou na ultima posicao dele; o fecho convexo cobre o resto (laco incluso). */
static void EstenderPelaVivacidade(Contexto *c)
{
    const AnalisesBir *a = c->analises;

    for (uint32_t r = 0; r < a->nrpo; r++) {
        uint32_t b = a->rpo[r];

        if (c->f->blocos[b].n == 0)
            continue;
        EstenderPorConjunto(c->saida, &a->vivo_entra[b], c->pos_ini[b]);
        EstenderPorConjunto(c->saida, &a->vivo_sai[b], c->pos_fim[b]);
    }
}

/* Carregada pelo laco: definida dentro de algum laco e viva na entrada do cabecalho de algum laco (o valor atravessa a volta). */
static void MarcarCarregadasPorLaco(Contexto *c)
{
    const AnalisesBir *a = c->analises;

    for (uint32_t k = 0; k < a->nlacos; k++) {
        const ConjuntoVivo *vivo = &a->vivo_entra[a->lacos[k].cabecalho];

        for (uint32_t j = 0; j < vivo->n; j++) {
            if (c->saida->iv[vivo->v[j]].def_em_laco)
                c->saida->iv[vivo->v[j]].carregada = true;
        }
    }
}

/* Chamada que cruza = chamada estritamente dentro de (inicio, fim): o argumento que morre na chamada le antes dela e o dst nasce depois.
 * Alvo de CHAMA_IND, destino de agregado de CHAMA (lido depois), funcao com chamada SysV e alvo linux ficam com a regra antiga (inclui a chamada em fim). */
static bool CruzaChamada(const Intervalos *s, const IntervaloVreg *iv, bool alvo_linux)
{
    if (iv->regra_antiga || s->tem_sysv || alvo_linux)
        return s->chamadas_ate[iv->fim] - s->chamadas_ate[iv->inicio] > 0;
    if (iv->fim <= iv->inicio + 1u)
        return false;
    return s->chamadas_ate[iv->fim - 1u] - s->chamadas_ate[iv->inicio] > 0;
}

/* Parametro nasce na posicao 0; elegivel = inteiro ou real que aparece e nao e agregado por bytes; cruza_chamada pelo prefixo das chamadas. */
static void FinalizarIntervalos(Contexto *c)
{
    Intervalos *s = c->saida;

    for (uint32_t pos = 1; pos <= s->npos; pos++)
        s->chamadas_ate[pos] += s->chamadas_ate[pos - 1u];
    for (uint32_t v = 0; v < s->nvregs; v++) {
        IntervaloVreg *iv = &s->iv[v];
        uint8_t classe = c->f->classe_vreg[v];

        if (iv->inicio == REGALLOC_SEM_POS)
            continue;
        if (v < c->f->nparams)
            iv->inicio = 0;
        iv->real = BirClasseEhXmm(classe); /* "real" = banco xmm: F32, F64 e o vetor V128 */
        iv->elegivel = classe >= BC_I8 && classe <= BC_V128 && !(v < c->f->nparams && AbiWinParamEmBytes(c->f, v));
        iv->cruza_chamada = CruzaChamada(s, iv, c->programa->sysv);
    }
}

static uint32_t ContarPosicoes(const BirFuncao *f, const AnalisesBir *a)
{
    uint32_t n = 0;

    for (uint32_t r = 0; r < a->nrpo; r++)
        n += f->blocos[a->rpo[r]].n;
    return n;
}

void RegallocIntervalos(const BirPrograma *programa, const BirFuncao *f, const AnalisesBir *a, Intervalos *saida)
{
    Contexto c = {.programa = programa, .f = f, .analises = a, .saida = saida};
    Arena *arena = saida->arena;

    *saida = (Intervalos){.arena = arena, .nvregs = f->nvregs, .npos = ContarPosicoes(f, a)};
    saida->iv = ArenaAlocar(arena, sizeof(IntervaloVreg) * (f->nvregs + 1u));
    saida->chamadas_ate = ArenaAlocar(arena, sizeof(uint32_t) * (saida->npos + 2u));
    c.pos_ini = ArenaAlocar(arena, sizeof(uint32_t) * (f->nblocos + 1u));
    c.pos_fim = ArenaAlocar(arena, sizeof(uint32_t) * (f->nblocos + 1u));
    for (uint32_t v = 0; v < f->nvregs; v++)
        saida->iv[v].inicio = REGALLOC_SEM_POS;
    PosicionarBlocos(&c);
    EstenderPelaVivacidade(&c);
    MarcarCarregadasPorLaco(&c);
    FinalizarIntervalos(&c);
}
