/* cfg.c - passe `cfg` (M7b R4b): simplificacao do grafo de fluxo da BIR (ver cfg.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7b R4b: primeira versao (cadeia jmp->jmp, fusao de bloco com sucessor unico, bloco inalcancavel).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Um round: (1) redireciona as arestas SALTA/DESVIA para o destino final da cadeia de blocos que so tem SALTA (ciclo: fica como esta); (2) marca o alcancavel a partir do bloco 0 e dos blocos que
 * terminam em ARMADILHA (esses nunca somem); (3) funde B com C quando B termina em SALTA C, C so tem essa aresta de entrada, os dois sao quentes ou os dois frios e C nao termina em ARMADILHA;
 * (4) compacta os ids. A aresta frio de *_OV nunca e redirecionada (o validador exige alvo frio).
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/cfg.h"

typedef struct {
    BirFuncao *f;
    uint32_t n;
    uint32_t *fim, *estado, *pilha, *entradas; /* destino final da cadeia, marca do percurso, pilha de trabalho, arestas de entrada por bloco */
    uint8_t *vivo;                              /* 1 = o bloco continua na funcao */
    uint32_t topo;                              /* topo da pilha na marcacao do alcancavel */
} CfgCtx;

static bool EhContaOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

static BirInstr *Terminador(const BirFuncao *f, uint32_t b)
{
    return &f->blocos[b].instrs[f->blocos[b].n - 1];
}

static bool TerminaEmArmadilha(const BirFuncao *f, uint32_t b)
{
    return Terminador(f, b)->op == BIR_ARMADILHA;
}

/* Bloco que so tem SALTA (e nao e a entrada nem frio). */
static bool EhPassagem(const BirFuncao *f, uint32_t b)
{
    const BirBloco *k = &f->blocos[b];

    return b != 0 && !k->frio && k->n == 1 && k->instrs[0].op == BIR_SALTA;
}

/* Destino final da cadeia de passagens que comeca em b; cadeia que cai num ciclo de passagens fica sem mudar. Memoriza o percurso inteiro. */
static uint32_t SeguirPassagens(CfgCtx *c, uint32_t b)
{
    uint32_t topo = 0, atual = b, resultado = BIR_SEM_BLOCO;

    for (;;) {
        if (c->estado[atual] == 2) {
            resultado = c->fim[atual];
            break;
        }
        if (!EhPassagem(c->f, atual)) {
            resultado = atual;
            break;
        }
        if (c->estado[atual] == 1)
            break;
        c->estado[atual] = 1;
        c->pilha[topo++] = atual;
        atual = c->f->blocos[atual].instrs[0].alvo[0];
    }
    for (uint32_t k = 0; k < topo; k++) {
        c->fim[c->pilha[k]] = resultado == BIR_SEM_BLOCO ? c->pilha[k] : resultado;
        c->estado[c->pilha[k]] = 2;
    }
    return c->fim[b];
}

static void RedirecionarArestas(CfgCtx *c)
{
    for (uint32_t b = 0; b < c->n; b++) {
        BirInstr *t = Terminador(c->f, b);

        if (t->op == BIR_SALTA || t->op == BIR_DESVIA)
            t->alvo[0] = SeguirPassagens(c, t->alvo[0]);
        if (t->op == BIR_DESVIA)
            t->alvo[1] = SeguirPassagens(c, t->alvo[1]);
    }
}

/* Chama fn(ctx, alvo) para cada aresta que sai do bloco (SALTA, DESVIA e *_OV). */
static void PercorrerArestas(const BirBloco *blk, void (*fn)(CfgCtx *, uint32_t), CfgCtx *c)
{
    for (uint32_t k = 0; k < blk->n; k++) {
        const BirInstr *i = &blk->instrs[k];

        if (i->op == BIR_SALTA || i->op == BIR_DESVIA || EhContaOv(i->op))
            fn(c, i->alvo[0]);
        if (i->op == BIR_DESVIA)
            fn(c, i->alvo[1]);
    }
}

static void ContarEntrada(CfgCtx *c, uint32_t alvo)
{
    c->entradas[alvo]++;
}

static void EmpilharSeNovo(CfgCtx *c, uint32_t alvo)
{
    if (!c->vivo[alvo]) {
        c->vivo[alvo] = 1;
        c->pilha[c->topo++] = alvo;
    }
}

/* Alcancavel a partir do bloco 0 e dos blocos de armadilha (que ficam sempre, com o que eles alcancam). */
static void MarcarAlcancavel(CfgCtx *c)
{
    c->topo = 0;
    EmpilharSeNovo(c, 0);
    for (uint32_t b = 1; b < c->n; b++) {
        if (TerminaEmArmadilha(c->f, b))
            EmpilharSeNovo(c, b);
    }
    while (c->topo > 0)
        PercorrerArestas(&c->f->blocos[c->pilha[--c->topo]], EmpilharSeNovo, c);
}

/* B termina em SALTA C e C so tem essa entrada: B passa a ter as instrucoes de C no lugar do SALTA. */
static bool PodeAbsorver(const CfgCtx *c, uint32_t b)
{
    const BirInstr *t = Terminador(c->f, b);
    uint32_t alvo;

    if (t->op != BIR_SALTA)
        return false;
    alvo = t->alvo[0];
    return alvo != b && alvo != 0 && c->entradas[alvo] == 1 && c->f->blocos[alvo].frio == c->f->blocos[b].frio && !TerminaEmArmadilha(c->f, alvo);
}

static void Absorver(CfgCtx *c, uint32_t b)
{
    BirBloco *dst = &c->f->blocos[b];
    uint32_t alvo = Terminador(c->f, b)->alvo[0];
    BirBloco *src = &c->f->blocos[alvo];
    uint32_t novo_n = dst->n - 1u + src->n;

    if (novo_n > dst->cap) {
        BirInstr *novo = realloc(dst->instrs, (size_t)novo_n * sizeof *novo);

        if (novo == NULL)
            FalhaInterna("otim: memoria esgotada");
        dst->instrs = novo;
        dst->cap = novo_n;
    }
    memcpy(dst->instrs + dst->n - 1u, src->instrs, (size_t)src->n * sizeof *src->instrs);
    dst->n = novo_n;
    free(src->instrs);
    *src = (BirBloco){.frio = src->frio};
    c->vivo[alvo] = 0;
}

static uint32_t FundirBlocos(CfgCtx *c)
{
    uint32_t fusoes = 0;

    for (uint32_t b = 0; b < c->n; b++) {
        while (c->vivo[b] && PodeAbsorver(c, b)) {
            Absorver(c, b);
            fusoes++;
        }
    }
    return fusoes;
}

static void AlocarContexto(CfgCtx *c, BirFuncao *f)
{
    uint32_t *w = calloc((size_t)f->nblocos * 5u, sizeof *w);

    if (w == NULL)
        FalhaInterna("otim: memoria esgotada");
    c->f = f;
    c->n = f->nblocos;
    c->fim = w;
    c->estado = w + c->n;
    c->pilha = w + 2u * c->n;
    c->entradas = w + 3u * c->n;
    c->vivo = (uint8_t *)(w + 4u * c->n);
    for (uint32_t b = 0; b < c->n; b++)
        c->fim[b] = b;
}

/* Fica com os blocos vivos na ordem de id; so mexe na funcao se algum sumiu. */
static void Compactar(CfgCtx *c)
{
    uint32_t *ordem = c->pilha, n = 0;

    for (uint32_t b = 0; b < c->n; b++) {
        if (c->vivo[b])
            ordem[n++] = b;
    }
    if (n != c->n)
        CfgAplicarOrdem(c->f, ordem, n);
}

void OtimCfgAplicar(const BirPrograma *programa, BirFuncao *funcao)
{
    CfgCtx c;

    if (!CfgElegivel(programa, funcao))
        return;
    AlocarContexto(&c, funcao);
    RedirecionarArestas(&c);
    MarcarAlcancavel(&c);
    for (uint32_t b = 0; b < c.n; b++) {
        if (c.vivo[b])
            PercorrerArestas(&funcao->blocos[b], ContarEntrada, &c);
    }
    FundirBlocos(&c);
    Compactar(&c);
    free(c.fim);
}
