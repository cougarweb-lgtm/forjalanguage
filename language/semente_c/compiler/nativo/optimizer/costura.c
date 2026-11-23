/* costura.c - passe `costura` (jump threading com faixa por aresta; ver costura.h e costura_interno.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - primeira versao: a faixa de cada parametro que nunca e regravado (raiz) e a forma afim (raiz + c) de cada vreg viajam pelas arestas;
 *                    bloco pequeno e puro que termina em DESVIA de um CMP decidido pela aresta de entrada e copiado nessa aresta, que salta direto ao sucessor.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --passe costura (e o fuzz dirigido, recurso j, do gerador)
 *
 * Analise (um passo, blocos em ordem de id; aresta de bloco ainda nao analisado = desconhecido, faixa total e sem forma): a entrada de cada bloco e a
 * juncao das arestas que chegam (faixa = envelope; forma so se igual em todas). A decisao olha cada aresta P->B: roda as instrucoes de B sobre o estado
 * de P, e se o DESVIA de B fica decidido, B e copiado naquela aresta com SALTA ao sucessor. O bloco original fica para as outras entradas; o que ficar
 * sem entrada e tirado (CfgAplicarOrdem). So copia bloco sem carga, gravacao, chamada, armadilha, *_OV, divisao ou deslocamento; ARMADILHA nunca some.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/cfg.h"
#include "nativo/optimizer/costura.h"
#include "nativo/optimizer/costura_interno.h"

typedef struct {
    BirFuncao *f;
    uint32_t nvregs, nblocos, nraizes; /* nblocos = o numero antes do passe (as copias novas nao sao analisadas) */
    int32_t *raiz_de;                  /* nvregs: indice da raiz, ou -1 */
    CosForma *saida;                   /* nblocos x nvregs: forma de cada vreg no fim do bloco */
    CosFaixa *entrada;                 /* nblocos x nraizes: faixa de cada raiz na entrada do bloco */
    CosFaixa *ramo;                    /* nblocos x 2 x nraizes: faixa por ramo do DESVIA (SALTA: a da entrada) */
    uint8_t *feito, *ov_alvo;          /* nblocos: 1 = saida calculada / alvo frio de *_OV (entrada desconhecida) */
    CosForma *tab;                     /* nvregs: formas de trabalho */
    CosFaixa *faixas;                  /* nraizes: faixas de trabalho da aresta em avaliacao */
    uint32_t dup_blocos, dup_instrs;
    bool mudou;
} CosCtx;

static void *Alocar(size_t n, size_t tam)
{
    void *p = calloc(n > 0 ? n : 1u, tam > 0 ? tam : 1u);

    if (p == NULL)
        FalhaInterna("otim: memoria esgotada");
    return p;
}

static bool EhContaOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

static BirInstr *Terminador(const BirFuncao *f, uint32_t b)
{
    return &f->blocos[b].instrs[f->blocos[b].n - 1];
}

/* A instrucao escreve o vreg dst? (SALTA, DESVIA, RETORNA, ARMADILHA e as gravacoes de memoria nao escrevem vreg.) */
static bool EhDefinicao(uint8_t op)
{
    return op != BIR_SALTA && op != BIR_DESVIA && op != BIR_RETORNA && op != BIR_ARMADILHA && op != BIR_GRAVA && op != BIR_COPIA_MEM && op != BIR_ZERA_MEM;
}

/* Instrucao que pode sair do bloco copiado: sem trap, sem memoria, sem chamada e sem estouro. */
static bool EhPuro(uint8_t op)
{
    switch (op) {
    case BIR_CONST: case BIR_COPIA: case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_AND: case BIR_OR: case BIR_XOR:
    case BIR_NEG: case BIR_NOT: case BIR_CMP: case BIR_EXT_S: case BIR_EXT_Z: case BIR_TRUNC:
        return true;
    default:
        return false;
    }
}

/* Bloco candidato: nao frio, nao o bloco 0, curto, so instrucao pura e DESVIA com alvos diferentes. */
static bool EhCandidato(const BirFuncao *f, uint32_t b)
{
    const BirBloco *k = &f->blocos[b];
    const BirInstr *t;

    if (b == 0 || k->frio || k->n < 2u || k->n > COSTURA_BLOCO_INSTR_MAX)
        return false;
    t = &k->instrs[k->n - 1u];
    if (t->op != BIR_DESVIA || t->alvo[0] == t->alvo[1])
        return false;
    for (uint32_t j = 0; j + 1u < k->n; j++) {
        if (!EhPuro(k->instrs[j].op))
            return false;
    }
    return true;
}

/* A aresta r do bloco p vai para b? (SALTA usa so o ramo 0; DESVIA, o ramo 0 ou 1.) */
static bool ArestaRamo(const BirFuncao *f, uint32_t p, uint32_t r, uint32_t b)
{
    const BirInstr *t = Terminador(f, p);

    if (t->op == BIR_SALTA)
        return r == 0 && t->alvo[0] == b;
    return t->op == BIR_DESVIA && t->alvo[r] == b;
}

static bool VregI64(const CosCtx *c, uint32_t v)
{
    return v < c->nvregs && c->f->classe_vreg[v] == BC_I64;
}

/* Forma de a + b (ou a - b) quando um lado e vreg com forma e o outro e imediato. */
static CosForma FormaAfim(const CosCtx *c, const CosForma *tab, const BirInstr *i, bool subtrai)
{
    const BirOperando *v;
    CosForma base, saida;
    int64_t d;

    if (i->a.tipo == BO_VREG && i->b.tipo == BO_IMED) {
        v = &i->a;
        d = i->b.imed;
    } else if (!subtrai && i->a.tipo == BO_IMED && i->b.tipo == BO_VREG) {
        v = &i->b;
        d = i->a.imed;
    } else {
        return CosSemForma();
    }
    if (!VregI64(c, v->id))
        return CosSemForma();
    base = CosFormaDe(c->raiz_de, tab, v->id);
    if (subtrai) {
        if (d == INT64_MIN)
            return CosSemForma();
        d = -d;
    }
    return CosFormaSoma(base, d, &saida) ? saida : CosSemForma();
}

/* Forma que o vreg dst recebe com a instrucao (COPIA e soma/subtracao de constante em I64; o resto perde a forma). */
static CosForma FormaDaInstr(const CosCtx *c, const CosForma *tab, const BirInstr *i)
{
    if (!VregI64(c, i->dst))
        return CosSemForma();
    switch (i->op) {
    case BIR_COPIA:
        if (i->a.tipo == BO_VREG && VregI64(c, i->a.id))
            return CosFormaDe(c->raiz_de, tab, i->a.id);
        return CosSemForma();
    case BIR_ADD:
        return FormaAfim(c, tab, i, false);
    case BIR_SUB:
        return FormaAfim(c, tab, i, true);
    default:
        return CosSemForma();
    }
}

/* Condicao com o imediato na esquerda (K op x) reescrita com a variavel na esquerda (x op' K): so o sentido de LT e GT (e LE e GE) troca. */
static uint8_t Espelho(uint8_t cond)
{
    switch (cond) {
    case BCOND_LT_S: return BCOND_GT_S;
    case BCOND_GT_S: return BCOND_LT_S;
    case BCOND_LE_S: return BCOND_GE_S;
    case BCOND_GE_S: return BCOND_LE_S;
    default: return cond;
    }
}

static bool CondSinalizada(uint8_t cond)
{
    return cond == BCOND_EQ || cond == BCOND_NE || cond == BCOND_LT_S || cond == BCOND_LE_S || cond == BCOND_GT_S || cond == BCOND_GE_S;
}

static void RegistrarCond(const CosCtx *c, const CosForma *tab, CosCond *cond, const BirInstr *i)
{
    CosForma f;
    int64_t k;
    uint8_t op;

    cond->valida = false;
    if (!CondSinalizada(i->aux))
        return;
    if (i->a.tipo == BO_VREG && i->b.tipo == BO_IMED && VregI64(c, i->a.id)) {
        f = CosFormaDe(c->raiz_de, tab, i->a.id);
        k = i->b.imed;
        op = i->aux;
    } else if (i->a.tipo == BO_IMED && i->b.tipo == BO_VREG && VregI64(c, i->b.id)) {
        f = CosFormaDe(c->raiz_de, tab, i->b.id);
        k = i->a.imed;
        op = Espelho(i->aux);
    } else {
        return;
    }
    if (f.raiz < 0)
        return;
    *cond = (CosCond){.valida = true, .vreg = i->dst, .forma = f, .k = k, .cond = op};
}

/* Passo de uma instrucao (que nao e desvio) sobre as formas e sobre o CMP do bloco. */
static void PassoInstrucao(const CosCtx *c, CosForma *tab, CosCond *cond, const BirInstr *i)
{
    if (!EhDefinicao(i->op) || i->dst >= c->nvregs)
        return;
    if (cond->valida && i->dst == cond->vreg)
        cond->valida = false;
    if (i->op == BIR_CMP) {
        RegistrarCond(c, tab, cond, i);
        tab[i->dst] = CosSemForma();
        return;
    }
    tab[i->dst] = FormaDaInstr(c, tab, i);
}

static bool EntradaConhecida(const CosCtx *c, uint32_t b)
{
    uint32_t arestas = 0;

    if (b == 0 || c->ov_alvo[b])
        return false;
    for (uint32_t p = 0; p < c->nblocos; p++) {
        for (uint32_t r = 0; r < 2u; r++) {
            if (!ArestaRamo(c->f, p, r, b))
                continue;
            if (!c->feito[p])
                return false;
            arestas++;
        }
    }
    return arestas > 0;
}

/* Mistura uma aresta (aresta = bloco origem * 2 + ramo) na entrada: a 1a (primeira) copia a faixa e a forma; as outras alargam a faixa e cortam a forma. */
static void MesclarAresta(CosCtx *c, uint32_t aresta, bool primeira, CosFaixa *ent)
{
    const CosFaixa *ram = c->ramo + (size_t)aresta * c->nraizes;
    const CosForma *sai = c->saida + (size_t)(aresta / 2u) * c->nvregs;

    for (uint32_t k = 0; k < c->nraizes; k++) {
        if (primeira || ram[k].lo < ent[k].lo)
            ent[k].lo = ram[k].lo;
        if (primeira || ram[k].hi > ent[k].hi)
            ent[k].hi = ram[k].hi;
    }
    for (uint32_t v = 0; v < c->nvregs; v++)
        c->tab[v] = primeira ? sai[v] : CosMesclaForma(c->tab[v], sai[v]);
}

/* Juncao das arestas que chegam em b: faixa = envelope, forma = a que todas dizem. */
static void MesclarEntrada(CosCtx *c, uint32_t b)
{
    bool primeira = true;
    CosFaixa *ent = c->entrada + (size_t)b * c->nraizes;

    for (uint32_t p = 0; p < c->nblocos; p++) {
        for (uint32_t r = 0; r < 2u; r++) {
            if (!ArestaRamo(c->f, p, r, b))
                continue;
            MesclarAresta(c, p * 2u + r, primeira, ent);
            primeira = false;
        }
    }
}

/* Entrada desconhecida: faixa total e sem forma (sempre seguro). */
static void SemInformacao(CosCtx *c, uint32_t b)
{
    CosFaixa *ent = c->entrada + (size_t)b * c->nraizes;

    for (uint32_t k = 0; k < c->nraizes; k++)
        ent[k] = (CosFaixa){.lo = INT64_MIN, .hi = INT64_MAX};
    for (uint32_t v = 0; v < c->nvregs; v++)
        c->tab[v] = CosSemForma();
}

static void CalcularEntrada(CosCtx *c, uint32_t b)
{
    if (EntradaConhecida(c, b))
        MesclarEntrada(c, b);
    else
        SemInformacao(c, b);
}

/* Analisa o bloco b na ordem: entrada, instrucoes, saida e faixa por ramo do DESVIA (o CMP refina a raiz no ramo verdadeiro e no falso). */
static void AnalisarBloco(CosCtx *c, uint32_t b)
{
    const BirBloco *blk = &c->f->blocos[b];
    const BirInstr *t = &blk->instrs[blk->n - 1u];
    CosCond cond = {0};

    CalcularEntrada(c, b);
    for (uint32_t j = 0; j + 1u < blk->n; j++)
        PassoInstrucao(c, c->tab, &cond, &blk->instrs[j]);
    memcpy(c->saida + (size_t)b * c->nvregs, c->tab, (size_t)c->nvregs * sizeof *c->tab);
    for (uint32_t r = 0; r < 2u; r++) {
        CosFaixa *ram = c->ramo + ((size_t)b * 2u + r) * c->nraizes;

        memcpy(ram, c->entrada + (size_t)b * c->nraizes, (size_t)c->nraizes * sizeof *ram);
        if (t->op == BIR_DESVIA && cond.valida && cond.vreg == t->a.id)
            CosRefinar(&cond, r == 0, ram);
    }
    c->feito[b] = 1;
}

/* Alvo que a aresta r de p tomaria para o bloco b (B copiado com o estado de p): -1 se o DESVIA de B nao se decide. */
static int64_t AlvoDecidido(CosCtx *c, uint32_t b, uint32_t p, uint32_t r)
{
    const BirBloco *k = &c->f->blocos[b];
    const BirInstr *t = &k->instrs[k->n - 1u];
    CosCond cond = {0};
    int decisao;

    memcpy(c->tab, c->saida + (size_t)p * c->nvregs, (size_t)c->nvregs * sizeof *c->tab);
    memcpy(c->faixas, c->ramo + ((size_t)p * 2u + r) * c->nraizes, (size_t)c->nraizes * sizeof *c->faixas);
    for (uint32_t j = 0; j + 1u < k->n; j++)
        PassoInstrucao(c, c->tab, &cond, &k->instrs[j]);
    if (!cond.valida || cond.vreg != t->a.id)
        return -1;
    decisao = CosDecide(&cond, c->faixas);
    if (decisao < 0)
        return -1;
    return decisao == 1 ? (int64_t)t->alvo[0] : (int64_t)t->alvo[1];
}

/* Troca o alvo b por nova nas arestas de p (SALTA ou DESVIA; um DESVIA com os dois alvos iguais nao entra aqui). */
static void RedirecionarAresta(BirFuncao *f, uint32_t p, uint32_t b, uint32_t nova)
{
    BirInstr *t = Terminador(f, p);

    if (t->op == BIR_SALTA && t->alvo[0] == b)
        t->alvo[0] = nova;
    if (t->op == BIR_DESVIA) {
        if (t->alvo[0] == b)
            t->alvo[0] = nova;
        if (t->alvo[1] == b)
            t->alvo[1] = nova;
    }
}

/* Copia o bloco b (sem o DESVIA) num bloco novo com SALTA ao sucessor e redireciona a aresta p->b para ele. */
static void DuplicarNaAresta(CosCtx *c, uint32_t b, uint32_t p, uint32_t sucessor)
{
    BirFuncao *f = c->f;
    uint32_t nova = f->nblocos, n = f->blocos[b].n - 1u;
    BirInstr termina = f->blocos[b].instrs[n];
    BirInstr *copia;

    copia = Alocar(n + 1u, sizeof *copia);
    memcpy(copia, f->blocos[b].instrs, (size_t)n * sizeof *copia);
    copia[n] = (BirInstr){.op = BIR_SALTA, .classe = BC_NADA, .dst = BIR_SEM_VREG, .simbolo = BIR_SEM_SIMBOLO,
                          .alvo = {sucessor, BIR_SEM_BLOCO}, .pos = termina.pos, .ir = termina.ir};
    f->blocos = realloc(f->blocos, (size_t)(nova + 1u) * sizeof *f->blocos);
    if (f->blocos == NULL)
        FalhaInterna("otim: memoria esgotada");
    f->blocos[nova] = (BirBloco){.instrs = copia, .n = n + 1u, .cap = n + 1u, .frio = false};
    f->nblocos = f->cap_blocos = nova + 1u;
    RedirecionarAresta(f, p, b, nova);
    c->dup_blocos++;
    c->dup_instrs += n;
    c->mudou = true;
}

static bool CabeNoOrcamento(const CosCtx *c, uint32_t b)
{
    return c->dup_blocos < COSTURA_DUP_BLOCOS_MAX && c->dup_instrs + (c->f->blocos[b].n - 1u) <= COSTURA_DUP_INSTR_MAX;
}

/* Aresta r de p para o candidato b: se o DESVIA de b se decide nesse estado, b e copiado nela. */
static void TentarAresta(CosCtx *c, uint32_t b, uint32_t p, uint32_t r)
{
    const BirInstr *t;
    int64_t sucessor;

    if (p == b || !c->feito[p] || c->f->blocos[p].frio || !ArestaRamo(c->f, p, r, b) || !CabeNoOrcamento(c, b))
        return;
    t = Terminador(c->f, p);
    if (t->op == BIR_DESVIA && t->alvo[0] == t->alvo[1])
        return;
    sucessor = AlvoDecidido(c, b, p, r);
    if (sucessor < 0 || sucessor == (int64_t)b)
        return;
    DuplicarNaAresta(c, b, p, (uint32_t)sucessor);
}

/* Para cada bloco candidato, cada aresta de bloco ja analisado que chega nele. */
static void DecidirArestas(CosCtx *c)
{
    for (uint32_t b = 1; b < c->nblocos; b++) {
        if (!EhCandidato(c->f, b))
            continue;
        for (uint32_t p = 0; p < c->nblocos; p++) {
            TentarAresta(c, b, p, 0u);
            TentarAresta(c, b, p, 1u);
        }
    }
}

/* Parametro I64 sem agregado vira candidato a raiz (-2); a 1a regravacao no corpo o tira (-1). */
static void MarcarCandidatosRaiz(CosCtx *c)
{
    const BirFuncao *f = c->f;

    for (uint32_t v = 0; v < c->nvregs; v++)
        c->raiz_de[v] = -1;
    for (uint32_t k = 0; k < f->nparams && k < c->nvregs; k++) {
        if (f->classe_vreg[k] == BC_I64 && (f->param_tam == NULL || f->param_tam[k] == 0))
            c->raiz_de[k] = -2;
    }
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t j = 0; j < f->blocos[b].n; j++) {
            const BirInstr *i = &f->blocos[b].instrs[j];

            if (EhDefinicao(i->op) && i->dst < c->nvregs && c->raiz_de[i->dst] == -2)
                c->raiz_de[i->dst] = -1;
        }
    }
}

/* Numera as raizes restantes (ate COSTURA_RAIZES_MAX); o que sobra de -2 vira -1. */
static void NumerarRaizes(CosCtx *c)
{
    c->nraizes = 0;
    for (uint32_t v = 0; v < c->nvregs; v++) {
        if (c->raiz_de[v] != -2)
            continue;
        c->raiz_de[v] = c->nraizes < COSTURA_RAIZES_MAX ? (int32_t)c->nraizes++ : -1;
    }
}

static void AcharRaizes(CosCtx *c)
{
    MarcarCandidatosRaiz(c);
    NumerarRaizes(c);
}

static bool TemCandidato(const BirFuncao *f)
{
    for (uint32_t b = 1; b < f->nblocos; b++) {
        if (EhCandidato(f, b))
            return true;
    }
    return false;
}

static void MarcarOvAlvos(CosCtx *c)
{
    for (uint32_t b = 0; b < c->nblocos; b++) {
        for (uint32_t j = 0; j < c->f->blocos[b].n; j++) {
            const BirInstr *i = &c->f->blocos[b].instrs[j];

            if (EhContaOv(i->op))
                c->ov_alvo[i->alvo[0]] = 1;
        }
    }
}

/* Aloca o contexto; false se a funcao e grande demais para a tabela de formas (fica como esta). */
static bool AlocarCtx(CosCtx *c, BirFuncao *f)
{
    c->f = f;
    c->nvregs = f->nvregs;
    c->nblocos = f->nblocos;
    if ((uint64_t)c->nblocos * c->nvregs > COSTURA_CELULAS_MAX)
        return false;
    c->raiz_de = Alocar(c->nvregs, sizeof *c->raiz_de);
    c->saida = Alocar((size_t)c->nblocos * c->nvregs, sizeof *c->saida);
    c->entrada = Alocar((size_t)c->nblocos * COSTURA_RAIZES_MAX, sizeof *c->entrada);
    c->ramo = Alocar((size_t)c->nblocos * 2u * COSTURA_RAIZES_MAX, sizeof *c->ramo);
    c->feito = Alocar(c->nblocos, sizeof *c->feito);
    c->ov_alvo = Alocar(c->nblocos, sizeof *c->ov_alvo);
    c->tab = Alocar(c->nvregs, sizeof *c->tab);
    c->faixas = Alocar(COSTURA_RAIZES_MAX, sizeof *c->faixas);
    return true;
}

static void LiberarCtx(CosCtx *c)
{
    free(c->raiz_de);
    free(c->saida);
    free(c->entrada);
    free(c->ramo);
    free(c->feito);
    free(c->ov_alvo);
    free(c->tab);
    free(c->faixas);
}

/* Empilha os alvos de saida da instrucao que ainda nao estao marcados. */
static void EmpilharAlvos(const BirInstr *i, uint8_t *vivo, uint32_t *pilha, uint32_t *topo)
{
    uint32_t alvos = i->op == BIR_DESVIA ? 2u : (i->op == BIR_SALTA || EhContaOv(i->op) ? 1u : 0u);

    for (uint32_t a = 0; a < alvos; a++) {
        if (!vivo[i->alvo[a]]) {
            vivo[i->alvo[a]] = 1;
            pilha[(*topo)++] = i->alvo[a];
        }
    }
}

/* Marca o alcancavel a partir do bloco 0 e dos blocos que terminam em ARMADILHA (ficam sempre, com o que alcancam, como no cfg). */
static void MarcarAlcancaveis(const BirFuncao *f, uint8_t *vivo, uint32_t *pilha)
{
    uint32_t topo = 0;

    vivo[0] = 1;
    pilha[topo++] = 0;
    for (uint32_t b = 1; b < f->nblocos; b++) {
        if (Terminador(f, b)->op == BIR_ARMADILHA) {
            vivo[b] = 1;
            pilha[topo++] = b;
        }
    }
    while (topo > 0) {
        const BirBloco *k = &f->blocos[pilha[--topo]];

        for (uint32_t j = 0; j < k->n; j++)
            EmpilharAlvos(&k->instrs[j], vivo, pilha, &topo);
    }
}

/* Tira os blocos que nao estao marcados; so mexe na funcao se algum sumiu (CfgAplicarOrdem mantem a ordem de id). */
static void CompactarInalcancaveis(BirFuncao *f)
{
    uint32_t n = f->nblocos, m = 0;
    uint8_t *vivo = Alocar(n, sizeof *vivo);
    uint32_t *ordem = Alocar(n, sizeof *ordem);

    MarcarAlcancaveis(f, vivo, ordem);
    for (uint32_t b = 0; b < n; b++) {
        if (vivo[b])
            ordem[m++] = b;
    }
    if (m != n)
        CfgAplicarOrdem(f, ordem, m);
    free(vivo);
    free(ordem);
}

void OtimCosturaAplicar(const BirPrograma *programa, BirFuncao *funcao)
{
    CosCtx c = {0};

    if (!CfgElegivel(programa, funcao) || !TemCandidato(funcao) || !AlocarCtx(&c, funcao))
        return;
    AcharRaizes(&c);
    if (c.nraizes > 0) {
        MarcarOvAlvos(&c);
        for (uint32_t b = 0; b < c.nblocos; b++)
            AnalisarBloco(&c, b);
        DecidirArestas(&c);
        if (c.mudou)
            CompactarInalcancaveis(funcao);
    }
    LiberarCtx(&c);
}
