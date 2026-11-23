/* layout.c - passe `layout` (M7b R4b): ordem dos blocos da BIR para o x64 (ver cfg.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7b R4b: primeira versao (tracos guiados pela profundidade de laco, frio e armadilha por ultimo, rotacao do cabecalho de laco com teste).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Tracos: do bloco atual segue o sucessor de maior nota (nota = 0 se frio ou termina em ARMADILHA, senao 1 + profundidade de laco; empate: o bloco com menos instrucoes, depois o primeiro alvo); o outro vai para a fila.
 * Sem sucessor livre, sai da fila o bloco de maior nota (empate: o descoberto antes). Blocos frios e de armadilha so entram quando mais nada resta, e nunca como fall-through.
 * Rotacao: laco contiguo cujo cabecalho termina em DESVIA com um alvo dentro e outro fora leva o cabecalho para o fim do corpo; o SALTA de volta cai direto nele e o jcc de volta e o
 * unico desvio por iteracao. O alocador anda pelo RPO (os sucessores por ordem de instrucao, nao por id), entao renumerar os blocos nao muda a alocacao.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/bir/analises.h"
#include "nativo/optimizer/cfg.h"

typedef struct {
    const BirFuncao *f;
    const AnalisesBir *a;
    uint32_t *ordem, nordem; /* ordem final dos blocos e quantos ja entraram */
    uint32_t *fila, nfila;   /* blocos descobertos e ainda nao colocados */
    uint32_t *pos;           /* por bloco: posicao em ordem (so depois de montada) */
    uint8_t *colocado, *emlaco;
} Layout;

static bool EhFrioOuArmadilha(const BirFuncao *f, uint32_t b)
{
    const BirBloco *k = &f->blocos[b];

    return k->frio || k->instrs[k->n - 1].op == BIR_ARMADILHA;
}

static uint32_t Nota(const Layout *L, uint32_t b)
{
    return EhFrioOuArmadilha(L->f, b) ? 0u : 1u + L->a->prof_laco[b];
}

/* a e melhor que b como fall-through: nota maior; empate, o bloco menor (o desvio raro costuma ser o trecho grande: tratamento de erro, reflexao, rebote). */
static bool Melhor(const Layout *L, uint32_t a, uint32_t b)
{
    uint32_t na = Nota(L, a), nb = Nota(L, b);

    return na > nb || (na == nb && L->f->blocos[a].n < L->f->blocos[b].n);
}

static void Enfileirar(Layout *L, uint32_t b)
{
    if (!L->colocado[b])
        L->fila[L->nfila++] = b;
}

/* Tira da fila o bloco ainda livre de maior nota (primeiro descoberto no empate); BIR_SEM_BLOCO se acabou. A fila perde os ja colocados. */
static uint32_t RetirarMelhor(Layout *L)
{
    uint32_t melhor = BIR_SEM_BLOCO, restam = 0;

    for (uint32_t k = 0; k < L->nfila; k++) {
        uint32_t b = L->fila[k];

        if (L->colocado[b])
            continue;
        L->fila[restam++] = b;
        if (melhor == BIR_SEM_BLOCO || Nota(L, b) > Nota(L, melhor))
            melhor = b;
    }
    L->nfila = restam;
    return melhor;
}

/* Poe b na ordem e empilha na fila o alvo frio de cada *_OV do bloco. */
static void Colocar(Layout *L, uint32_t b)
{
    const BirBloco *blk = &L->f->blocos[b];

    L->colocado[b] = 1;
    L->ordem[L->nordem++] = b;
    for (uint32_t k = 0; k < blk->n; k++) {
        uint8_t op = blk->instrs[k].op;

        if (op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV)
            Enfileirar(L, blk->instrs[k].alvo[0]);
    }
}

/* Confronta o candidato c com o melhor ate aqui: o pior vai para a fila; devolve o melhor. */
static uint32_t Confrontar(Layout *L, uint32_t melhor, uint32_t c)
{
    if (c == BIR_SEM_BLOCO || L->colocado[c])
        return melhor;
    if (melhor == BIR_SEM_BLOCO || Melhor(L, c, melhor)) {
        if (melhor != BIR_SEM_BLOCO)
            Enfileirar(L, melhor);
        return c;
    }
    Enfileirar(L, c);
    return melhor;
}

/* Proximo bloco do traco depois de b: o sucessor livre de maior nota que nao e frio nem armadilha; o resto vai para a fila. BIR_SEM_BLOCO se nao ha. */
static uint32_t EscolherProximo(Layout *L, uint32_t b)
{
    const BirBloco *blk = &L->f->blocos[b];
    const BirInstr *t = &blk->instrs[blk->n - 1];
    uint32_t melhor = BIR_SEM_BLOCO;

    if (t->op == BIR_SALTA || t->op == BIR_DESVIA)
        melhor = Confrontar(L, melhor, t->alvo[0]);
    if (t->op == BIR_DESVIA)
        melhor = Confrontar(L, melhor, t->alvo[1]);
    if (melhor != BIR_SEM_BLOCO && Nota(L, melhor) == 0) {
        Enfileirar(L, melhor);
        melhor = BIR_SEM_BLOCO;
    }
    return melhor;
}

/* Monta a ordem: entrada primeiro, depois os tracos; o que ninguem alcanca (so bloco de armadilha solto) vai no fim, na ordem dos ids. */
static void MontarOrdem(Layout *L)
{
    uint32_t atual = 0;

    while (atual != BIR_SEM_BLOCO) {
        Colocar(L, atual);
        atual = EscolherProximo(L, atual);
        if (atual == BIR_SEM_BLOCO)
            atual = RetirarMelhor(L);
    }
    for (uint32_t b = 0; b < L->f->nblocos; b++) {
        if (!L->colocado[b])
            L->ordem[L->nordem++] = b;
    }
}

static void AtualizarPosicoes(Layout *L, uint32_t de, uint32_t ate)
{
    for (uint32_t k = de; k < ate; k++)
        L->pos[L->ordem[k]] = k;
}

/* O laco ocupa posicoes seguidas a partir do cabecalho? */
static bool LacoContiguo(const Layout *L, const LacoBir *laco)
{
    uint32_t inicio = L->pos[laco->cabecalho];

    for (uint32_t k = 0; k < laco->ncorpo; k++) {
        uint32_t p = L->pos[laco->corpo[k]];

        if (p < inicio || p >= inicio + laco->ncorpo)
            return false;
    }
    return true;
}

/* O cabecalho termina em DESVIA com um alvo dentro do laco e outro fora? (usa emlaco como rascunho e o deixa zerado) */
static bool CabecalhoComTeste(Layout *L, const LacoBir *laco)
{
    const BirBloco *h = &L->f->blocos[laco->cabecalho];
    const BirInstr *t = &h->instrs[h->n - 1];
    bool dentro0, dentro1;

    if (t->op != BIR_DESVIA)
        return false;
    for (uint32_t k = 0; k < laco->ncorpo; k++)
        L->emlaco[laco->corpo[k]] = 1;
    dentro0 = L->emlaco[t->alvo[0]] != 0;
    dentro1 = L->emlaco[t->alvo[1]] != 0;
    for (uint32_t k = 0; k < laco->ncorpo; k++)
        L->emlaco[laco->corpo[k]] = 0;
    return dentro0 != dentro1;
}

/* Cabecalho para o fim do corpo; devolve se mudou algo. */
static bool GirarLaco(Layout *L, const LacoBir *laco)
{
    uint32_t inicio, cabecalho = laco->cabecalho;

    if (laco->ncorpo < 2 || cabecalho == 0 || L->pos[cabecalho] == BIR_SEM_BLOCO)
        return false;
    inicio = L->pos[cabecalho];
    if (!LacoContiguo(L, laco) || !CabecalhoComTeste(L, laco))
        return false;
    memmove(L->ordem + inicio, L->ordem + inicio + 1u, (size_t)(laco->ncorpo - 1u) * sizeof *L->ordem);
    L->ordem[inicio + laco->ncorpo - 1u] = cabecalho;
    AtualizarPosicoes(L, inicio, inicio + laco->ncorpo);
    return true;
}

/* A fila recebe um bloco por aresta (no maximo 2 por bloco, mais um por *_OV): o vetor tem espaco para todas. */
static void AlocarLayout(Layout *L, const BirFuncao *f, const AnalisesBir *a)
{
    size_t n = f->nblocos, arestas = 2u * n + 1u;
    uint32_t *w;
    uint8_t *m = calloc(n * 2u + 1u, 1);

    for (uint32_t b = 0; b < f->nblocos; b++)
        arestas += f->blocos[b].n;
    w = calloc(2u * n + arestas, sizeof *w);
    if (w == NULL || m == NULL)
        FalhaInterna("otim: memoria esgotada");
    *L = (Layout){.f = f, .a = a, .ordem = w, .pos = w + n, .fila = w + 2u * n, .colocado = m, .emlaco = m + n};
}

static bool OrdemMudou(const Layout *L)
{
    for (uint32_t k = 0; k < L->nordem; k++) {
        if (L->ordem[k] != k)
            return true;
    }
    return false;
}

void OtimLayoutAplicar(const BirPrograma *programa, BirFuncao *funcao)
{
    AnalisesBir a;
    Layout L;
    bool mudou = false;

    if (!CfgElegivel(programa, funcao) || funcao->nblocos < 3)
        return;
    AnalisesCalcularLacosApenas(funcao, &a);
    AlocarLayout(&L, funcao, &a);
    MontarOrdem(&L);
    AtualizarPosicoes(&L, 0, L.nordem);
    for (uint32_t k = 0; k < a.nlacos; k++)
        mudou |= GirarLaco(&L, &a.lacos[k]);
    if (mudou || OrdemMudou(&L))
        CfgAplicarOrdem(funcao, L.ordem, L.nordem);
    free(L.ordem);
    free(L.colocado);
    AnalisesLiberar(&a);
}
