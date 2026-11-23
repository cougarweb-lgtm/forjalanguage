/* perfil.c - passe `perfil` (M8 P2, D-M8-5): chamadas BIR a forja_perfil_entrar/sair em cada proc de usuario (ver perfil.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M8 P2: primeira versao.
 * Verificacao: construir_rapido.py && forja build --perfil fib.fj -o fib.exe && fib.exe (o fib.exe.perfil.txt diz fib com 21891 chamadas para fib(20))
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "common/nome_modulo.h"
#include "nativo/optimizer/perfil.h"

#define PERFIL_PREFIXO_FUNCAO "fj_"
#define PERFIL_SIMBOLO_ENTRAR "forja_perfil_entrar"
#define PERFIL_SIMBOLO_SAIR "forja_perfil_sair"
#define PERFIL_SIMBOLO_FINALIZAR "forja_perfil_finalizar"
#define PERFIL_SIMBOLO_PROC_SAIR "forja_proc_sair"
#define PERFIL_ID_LIMITE 0x100000u /* a FPF1 e o runtime aceitam ids 0..0xFFFFF (1.048.575 procs) */

/* O que o passe sabe enquanto percorre o programa: os simbolos do runtime e, por funcao, se ela e proc de usuario e qual o id dela. */
typedef struct {
    BirPrograma *programa;
    uint32_t entrar, sair, finalizar; /* indices em BirPrograma.simbolos */
    uint32_t proc_sair;               /* forja_proc_sair, ou BIR_SEM_SIMBOLO se o programa nao a chama */
    bool instrumenta;                 /* a funcao em curso e proc de usuario */
    uint32_t id;                      /* id da funcao em curso (so vale com instrumenta) */
} PerfilCtx;

bool OtimPerfilProc(const BirPrograma *programa, const BirFuncao *funcao)
{
    return funcao->nblocos > 0 && !funcao->sintetica && strncmp(programa->simbolos[funcao->simbolo].nome, PERFIL_PREFIXO_FUNCAO, strlen(PERFIL_PREFIXO_FUNCAO)) == 0;
}

/* A chave de modulo "<L><tag>_<nome>" (nome_modulo.h) vira "tag.nome"; o nome da raiz (sem chave de modulo) fica como esta. */
void OtimPerfilNome(const BirPrograma *programa, const BirFuncao *funcao, char *saida, size_t cap)
{
    const char *chave = programa->simbolos[funcao->simbolo].nome + strlen(PERFIL_PREFIXO_FUNCAO);
    const char *puro = NomeModuloExibicao(chave);
    size_t digitos = 0;

    if (puro == chave) {
        snprintf(saida, cap, "%s", chave);
        return;
    }
    while (chave[digitos] >= '0' && chave[digitos] <= '9')
        digitos++;
    snprintf(saida, cap, "%.*s.%s", (int)(puro - chave - digitos - 1), chave + digitos, puro);
}

/* O simbolo BS_RUNTIME com este nome (literal do codigo, sem copia); cria se ainda nao existe. */
static uint32_t SimboloRuntime(BirPrograma *p, const char *nome)
{
    for (uint32_t k = 0; k < p->nsimbolos; k++) {
        if (p->simbolos[k].tipo == BS_RUNTIME && strcmp(p->simbolos[k].nome, nome) == 0)
            return k;
    }
    if (p->nsimbolos == p->cap_simbolos) {
        p->cap_simbolos = p->cap_simbolos == 0 ? 16u : p->cap_simbolos * 2u;
        p->simbolos = realloc(p->simbolos, (size_t)p->cap_simbolos * sizeof *p->simbolos);
        if (p->simbolos == NULL)
            FalhaInterna("perfil: memoria esgotada");
    }
    p->simbolos[p->nsimbolos] = (BirSimbolo){.nome = nome, .tipo = BS_RUNTIME, .tam = (uint32_t)strlen(nome)};
    return p->nsimbolos++;
}

/* CHAMA do runtime sem retorno; com id, um argumento imediato u32. A instrucao e sintetica (ir NULL); pos e a da instrucao vizinha. */
static BirInstr Chamada(PerfilCtx *c, uint32_t simbolo, bool com_id, uint32_t pos)
{
    BirInstr i = {.op = BIR_CHAMA, .classe = BC_NADA, .dst = BIR_SEM_VREG, .simbolo = simbolo, .alvo = {BIR_SEM_BLOCO, BIR_SEM_BLOCO}, .pos = pos};

    if (com_id) {
        i.args = ArenaAlocar(&c->programa->arena, sizeof *i.args);
        i.args[0] = (BirOperando){.tipo = BO_IMED, .classe = BC_I32, .imed = (int64_t)c->id};
        i.nargs = 1;
    }
    return i;
}

/* Quantas chamadas entram neste bloco: a de entrada (so no bloco 0 de proc de usuario), uma antes de cada RETORNA e uma antes de cada CHAMA a forja_proc_sair. */
static uint32_t ContarInsercoes(const PerfilCtx *c, const BirBloco *b, bool entrada)
{
    uint32_t n = entrada && c->instrumenta ? 1u : 0u;

    for (uint32_t k = 0; k < b->n; k++) {
        if (c->instrumenta && b->instrs[k].op == BIR_RETORNA)
            n++;
        else if (b->instrs[k].op == BIR_CHAMA && b->instrs[k].simbolo == c->proc_sair)
            n++;
    }
    return n;
}

/* Refaz o vetor de instrucoes do bloco com as chamadas do perfil nos lugares certos. */
static void InserirNoBloco(PerfilCtx *c, BirBloco *b, bool entrada)
{
    uint32_t extras = ContarInsercoes(c, b, entrada);
    BirInstr *novo;
    uint32_t j = 0;

    if (extras == 0)
        return;
    novo = malloc((size_t)(b->n + extras) * sizeof *novo);
    if (novo == NULL)
        FalhaInterna("perfil: memoria esgotada");
    if (entrada && c->instrumenta)
        novo[j++] = Chamada(c, c->entrar, true, b->n > 0 ? b->instrs[0].pos : BIR_SEM_POS);
    for (uint32_t k = 0; k < b->n; k++) {
        const BirInstr *x = &b->instrs[k];

        if (c->instrumenta && x->op == BIR_RETORNA)
            novo[j++] = Chamada(c, c->sair, true, x->pos);
        else if (x->op == BIR_CHAMA && x->simbolo == c->proc_sair)
            novo[j++] = Chamada(c, c->finalizar, false, x->pos);
        novo[j++] = *x;
    }
    free(b->instrs);
    b->instrs = novo;
    b->n = b->cap = j;
}

void OtimPerfilAplicar(BirPrograma *programa, Buf *relato)
{
    PerfilCtx c = {.programa = programa, .proc_sair = BIR_SEM_SIMBOLO};
    uint32_t proximo_id = 0;

    c.entrar = SimboloRuntime(programa, PERFIL_SIMBOLO_ENTRAR);
    c.sair = SimboloRuntime(programa, PERFIL_SIMBOLO_SAIR);
    c.finalizar = SimboloRuntime(programa, PERFIL_SIMBOLO_FINALIZAR);
    for (uint32_t k = 0; k < programa->nsimbolos; k++) {
        if (programa->simbolos[k].tipo == BS_RUNTIME && strcmp(programa->simbolos[k].nome, PERFIL_SIMBOLO_PROC_SAIR) == 0)
            c.proc_sair = k;
    }
    for (uint32_t i = 0; i < programa->nfuncoes; i++) {
        BirFuncao *f = &programa->funcoes[i];

        c.instrumenta = OtimPerfilProc(programa, f);
        if (c.instrumenta && proximo_id >= PERFIL_ID_LIMITE)
            FalhaInterna("perfil: mais de %u procs de usuario (o limite da tabela FPF1)", PERFIL_ID_LIMITE);
        c.id = c.instrumenta ? proximo_id++ : 0;
        if (c.instrumenta && relato != NULL)
            BufFormatar(relato, "perfil %s: id %u\n", programa->simbolos[f->simbolo].nome, (unsigned)c.id);
        for (uint32_t b = 0; b < f->nblocos; b++)
            InserirNoBloco(&c, &f->blocos[b], b == 0);
    }
}
