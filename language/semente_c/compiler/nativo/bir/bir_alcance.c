/* bir_alcance.c - poda do inalcancavel (B07): fila de procs a partir das raizes, sobre a IR.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6 P71b: BirNomeSomem tambem some track_handle e untrack_handle de std:mem (como assert com NDEBUG: em release o efeito do rastreio nao existe; a preparacao dos argumentos, que a IR ja calculou antes da chamada, pode sobrar).
 *            0.2.0 - M6 P71a: BirNomeSomem e omite_somem (release: a chamada SOMEM nao marca o proc de std:mem).
 *            0.1.0 - M4 P24p: primeira versao. Arestas: IR_CHAMADA direta (nome), V_PROC em qualquer operando (a, b, args: endereco tomado, chamada indireta, callback) e,
 *                    junto com o V_PROC, o trampolim fjt_ do proc. Raizes: main, exportados (so em dll) e 'test' (so em forja test).
 * Verificacao: construir.bat && teste_nativo.exe alcance
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/bir_alcance.h"

static void VarrerBloco(BirAlcance *a, const IrBloco *bloco);

static void *AlocarTabela(size_t n, size_t tam)
{
    void *p = calloc(n != 0 ? n : 1, tam);

    if (p == NULL)
        FalhaInterna("bir: memoria esgotada no alcance dos procs");
    return p;
}

static int32_t IndiceDoNome(const BirAlcance *a, const int32_t *tabela, int32_t nome)
{
    return nome >= 0 && (uint32_t)nome < a->nnomes ? tabela[nome] : -1;
}

/* Entra na fila uma vez so (a fila nunca passa do numero de procs). */
static void MarcarFuncao(BirAlcance *a, int32_t indice)
{
    if (indice < 0 || a->funcao[indice] != 0)
        return;
    a->funcao[indice] = 1;
    a->fila[a->gerados++] = (uint32_t)indice;
}

/* V_PROC: o endereco do proc vira o do trampolim fjt_, que CHAMA o proc. */
static void MarcarEndereco(BirAlcance *a, int32_t nome)
{
    int32_t trampolim = IndiceDoNome(a, a->trampolim_do_nome, nome);

    if (trampolim >= 0)
        a->trampolim[trampolim] = 1;
    MarcarFuncao(a, IndiceDoNome(a, a->funcao_do_nome, nome));
}

static void VarrerValor(BirAlcance *a, const Valor *v)
{
    if (v->tipo_no == V_PROC)
        MarcarEndereco(a, v->id);
}

/* Chaves dos procs de std:mem que somem em release (tag "mem" de std:mem). Lista fechada: nome novo entra aqui e no teste do P71. */
bool BirNomeSomem(const char *chave)
{
    return strcmp(chave, "3mem_snapshot") == 0 || strcmp(chave, "3mem_report_since") == 0
        || strcmp(chave, "3mem_track_handle") == 0 || strcmp(chave, "3mem_untrack_handle") == 0;
}

static void VarrerInstr(BirAlcance *a, const IrInstr *i)
{
    if (i->op == IR_CHAMADA && !(a->omite_somem && i->nome >= 0 && (uint32_t)i->nome < a->nnomes && BirNomeSomem(InternerTexto(a->nomes, i->nome))))
        MarcarFuncao(a, IndiceDoNome(a, a->funcao_do_nome, i->nome));
    VarrerValor(a, &i->a);
    VarrerValor(a, &i->b);
    for (uint32_t k = 0; k < i->nargs; k++)
        VarrerValor(a, &i->args[k]);
    VarrerBloco(a, &i->cond);
    VarrerBloco(a, &i->corpo);
    VarrerBloco(a, &i->senao);
    for (uint32_t k = 0; k < i->ncasos; k++)
        VarrerBloco(a, &i->casos[k].corpo);
}

static void VarrerBloco(BirAlcance *a, const IrBloco *bloco)
{
    for (const IrInstr *i = bloco->primeira; i != NULL; i = i->prox)
        VarrerInstr(a, i);
}

static bool EhRaiz(const BirAlcance *a, const IrFuncao *f)
{
    return f->e_main || (a->teste && f->e_teste) || (a->dll && f->exportado);
}

/* nome -> indice dos procs candidatos, a contagem e as raizes na fila. */
static void IndexarFuncoes(BirAlcance *a)
{
    for (uint32_t k = 0; k < a->ir->nfuncoes; k++) {
        const IrFuncao *f = &a->ir->funcoes[k];

        if (f->e_teste && !a->teste)
            continue;
        a->total++;
        a->funcao_do_nome[f->nome] = (int32_t)k;
        if (EhRaiz(a, f))
            MarcarFuncao(a, (int32_t)k);
    }
}

static void Alocar(BirAlcance *a)
{
    a->funcao = AlocarTabela(a->ir->nfuncoes, sizeof *a->funcao);
    a->trampolim = AlocarTabela(a->ir->ntrampolins, sizeof *a->trampolim);
    a->fila = AlocarTabela(a->ir->nfuncoes, sizeof *a->fila);
    a->funcao_do_nome = AlocarTabela(a->nnomes, sizeof *a->funcao_do_nome);
    a->trampolim_do_nome = AlocarTabela(a->nnomes, sizeof *a->trampolim_do_nome);
    memset(a->funcao_do_nome, 0xFF, a->nnomes * sizeof *a->funcao_do_nome);
    memset(a->trampolim_do_nome, 0xFF, a->nnomes * sizeof *a->trampolim_do_nome);
    for (uint32_t k = 0; k < a->ir->ntrampolins; k++)
        a->trampolim_do_nome[a->ir->trampolins[k].nome] = (int32_t)k;
}

void BirAlcanceCalcular(const BirEntrada *entrada, BirAlcance *alcance)
{
    *alcance = (BirAlcance){.ir = entrada->ir, .teste = entrada->teste, .dll = entrada->dll, .omite_somem = entrada->modo == BM_RELEASE, .nomes = entrada->nomes, .nnomes = (uint32_t)entrada->nomes->nomes.n};
    Alocar(alcance);
    IndexarFuncoes(alcance);
    for (uint32_t k = 0; k < alcance->gerados; k++)
        VarrerBloco(alcance, &alcance->ir->funcoes[alcance->fila[k]].corpo);
}

bool BirAlcanceFuncao(const BirAlcance *alcance, uint32_t k)
{
    return alcance->funcao[k] != 0;
}

bool BirAlcanceTrampolim(const BirAlcance *alcance, uint32_t k)
{
    return alcance->trampolim[k] != 0;
}

void BirAlcanceLiberar(BirAlcance *alcance)
{
    free(alcance->funcao);
    free(alcance->trampolim);
    free(alcance->fila);
    free(alcance->funcao_do_nome);
    free(alcance->trampolim_do_nome);
    *alcance = (BirAlcance){0};
}
