/* editar_imports.c - os imports que mover_decl e remover_decl do 'forja editar' tem de acrescentar ou apagar (ver editar_interno.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b (modulo de pasta): o import vale por ARQUIVO (trecho da Fonte): a conta e as celulas sao [arquivo x modulo], o apelido se confere no arquivo do uso (antes o indice do MODULO ia no lugar do arquivo)
 *            e os imports apagados e novos caem no arquivo certo (EditarTrocarNoArquivo).
 *            0.1.0 - IA1-I5: primeira versao. Para cada par (modulo x, modulo k) conta as referencias qualificadas "apelido.nome" que o indice acha (base) e o quanto a operacao as muda (delta).
 *            Import cujo total chega a 0 e apagado (senao seria E1005); modulo novo na conta ganha import (apelido padrao, ou o que a operacao pediu). Um import apagado e um novo no mesmo
 *            modulo trocam de lugar na mesma linha; o modulo raiz nao se importa (E1002).
 * Verificacao: construir.bat && python tests\editar\verificar_editar_i5.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/editar_interno.h"

enum { NOVOS_MAX = 32, APAGAR_MAX = 64 };

typedef struct { uint32_t x; uint8_t k; const char *apelido, *linha; } NovoImport; /* x = o arquivo que ganha o import (trecho da Fonte), k = o modulo importado */

struct Imports {
    Plano *p;
    uint32_t n;                 /* quantos modulos */
    uint32_t narquivos;         /* quantos arquivos (trechos da Fonte; 1 sem trechos) */
    int32_t *base, *delta;      /* [x * n + k]: referencias do arquivo x ao modulo k hoje, e o quanto a operacao as muda */
    NovoImport novos[NOVOS_MAX];
    uint32_t nnovos;
};

bool EditarUsoEhDeDecl(const IndUso *u)
{
    switch ((IndAlvo)u->alvo_tipo) {
    case IND_PROC:
    case IND_STRUCT:
    case IND_ENUM:
    case IND_UNIAO:
    case IND_CONST:
    case IND_UNIDADE:
        return u->alvo != NULL;
    default:
        return false;
    }
}

bool EditarUsoQualificado(const Consulta *q, const IndUso *u, uint32_t *token_nome)
{
    uint32_t i = EditarUsoEhDeDecl(u) ? EditarTokenDoNome(q, u) : TOKEN_AUSENTE;
    uint8_t x = EditarModuloDoOffset(q, u->pos_nome);

    if (i == TOKEN_AUSENTE || i < 2) return false;
    if (EditarToken(q, i - 1)->tipo != TK_DOT || EditarToken(q, i - 2)->tipo != TK_IDENT || u->alvo->modulo == x) return false;
    if (ModulosApelido(&q->c->modulos, EditarArquivoDoOffset(q, u->pos_nome), EditarToken(q, i - 2)->sym) != u->alvo->modulo) return false; /* o apelido vale no arquivo do uso */
    *token_nome = i;
    return true;
}

static size_t Celula(const Imports *imp, uint32_t x, uint8_t k)
{
    return (size_t)x * imp->n + k;
}

Imports *ImportsIniciar(Plano *p)
{
    Imports *imp = calloc(1, sizeof *imp);
    const Consulta *q = p->q;

    if (imp == NULL) FalhaInterna("memoria esgotada contando os imports");
    imp->p = p;
    imp->n = q->c->modulos.n;
    imp->narquivos = q->c->fonte.ntrechos > 0 ? q->c->fonte.ntrechos : 1;
    imp->base = calloc((size_t)imp->narquivos * imp->n, sizeof *imp->base);
    imp->delta = calloc((size_t)imp->narquivos * imp->n, sizeof *imp->delta);
    if (imp->base == NULL || imp->delta == NULL) FalhaInterna("memoria esgotada contando os imports");
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        uint32_t token;

        if (EditarUsoQualificado(q, u, &token)) imp->base[Celula(imp, EditarArquivoDoOffset(q, u->pos_nome), u->alvo->modulo)]++;
    }
    return imp;
}

void ImportsLiberar(Imports *imp)
{
    if (imp == NULL) return;
    free(imp->base);
    free(imp->delta);
    free(imp);
}

void ImportsContar(Imports *imp, uint32_t x, uint8_t k, int delta)
{
    imp->delta[Celula(imp, x, k)] += delta;
}

void ImportsDescontarDecl(Imports *imp, const Decl *d)
{
    const Consulta *q = imp->p->q;
    uint32_t ini = ConsultaInicioDecl(q, d), fim = AstFimDecl(&q->fim, d);

    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        uint32_t token;

        if (u->pos_nome >= ini && u->pos_nome < fim && EditarUsoQualificado(q, u, &token)) ImportsContar(imp, EditarArquivoDoOffset(q, d->pos), u->alvo->modulo, -1);
    }
}

/* ---------- o apelido de um modulo ---------- */

static const char *TextoDoNome(Plano *p, int32_t nome)
{
    return ArenaCopiarTexto(p->arena, InternerTexto(&p->q->c->nomes, nome), InternerTam(&p->q->c->nomes, nome));
}

/* O apelido com que x ja chama k (import que existe ou agendado); NULL = nenhum. */
static const char *ApelidoJaUsado(Imports *imp, uint32_t x, uint8_t k)
{
    const Modulos *mods = &imp->p->q->c->modulos;

    for (size_t i = 0; i < mods->apelidos.n; i++) {
        const ApelidoDeArquivo *a = VecEm(&mods->apelidos, i);

        if (a->arquivo == x && a->modulo == k) return TextoDoNome(imp->p, a->nome);
    }
    for (uint32_t i = 0; i < imp->nnovos; i++) {
        if (imp->novos[i].x == x && imp->novos[i].k == k) return imp->novos[i].apelido;
    }
    return NULL;
}

/* O apelido ja designa OUTRO modulo em x (import que existe ou agendado)? */
static bool ApelidoOcupado(Imports *imp, uint32_t x, int32_t nome)
{
    const Modulos *mods = &imp->p->q->c->modulos;
    const char *texto = TextoDoNome(imp->p, nome);

    for (size_t i = 0; i < mods->apelidos.n; i++) {
        const ApelidoDeArquivo *a = VecEm(&mods->apelidos, i);

        if (a->arquivo == x && a->nome == nome) return true;
    }
    for (uint32_t i = 0; i < imp->nnovos; i++) {
        if (imp->novos[i].x == x && strcmp(imp->novos[i].apelido, texto) == 0) return true;
    }
    return false;
}

static const char *AgendarImport(Imports *imp, uint32_t x, uint8_t k, int32_t preferido)
{
    const Modulos *mods = &imp->p->q->c->modulos;
    const Modulo *alvo = &mods->itens[k];
    int32_t nome = preferido >= 0 ? preferido : alvo->apelido;
    Buf linha = {0};
    NovoImport *novo;

    if (k == 0) {
        PlanoErro(imp->p, "o arquivo raiz nao pode ser importado por outro modulo (E1002): a declaracao e o que ela usa teriam de ficar no mesmo modulo");
        return NULL;
    }
    if (ApelidoOcupado(imp, x, nome)) {
        PlanoErro(imp->p, "o apelido '%s' ja designa outro modulo em \"%s\": ajuste os imports a mao", TextoDoNome(imp->p, nome), TextoDoNome(imp->p, mods->itens[ModulosPacoteDoArquivo(mods, x)].caminho));
        return NULL;
    }
    if (imp->nnovos == NOVOS_MAX) {
        PlanoErro(imp->p, "imports demais para acrescentar numa operacao so");
        return NULL;
    }
    BufFormatar(&linha, "import \"%s\"", TextoDoNome(imp->p, alvo->caminho));
    if (nome != alvo->apelido) BufFormatar(&linha, " as %s", TextoDoNome(imp->p, nome));
    novo = &imp->novos[imp->nnovos++];
    *novo = (NovoImport){.x = x, .k = k, .apelido = TextoDoNome(imp->p, nome), .linha = ArenaCopiarTexto(imp->p->arena, linha.dados, linha.tam)};
    BufLiberar(&linha);
    return novo->apelido;
}

const char *ImportsApelido(Imports *imp, uint32_t x, uint8_t k, int32_t preferido)
{
    const char *existente = ApelidoJaUsado(imp, x, k);

    return existente != NULL ? existente : AgendarImport(imp, x, k, preferido);
}

/* ---------- emitir as edicoes ---------- */

/* Os D_IMPORT do arquivo x que perdem o ultimo uso, na ordem do arquivo. Devolve quantos. */
static uint32_t ImportsSemUso(const Imports *imp, uint32_t x, const Decl **lista)
{
    uint32_t n = 0;

    for (const Decl *d = imp->p->q->c->sem.programa->primeira; d != NULL && n < APAGAR_MAX; d = d->prox) {
        size_t c;

        if (d->tipo_no != D_IMPORT || EditarArquivoDoOffset(imp->p->q, d->pos) != x) continue;
        c = Celula(imp, x, (uint8_t)d->indice);
        if (imp->delta[c] < 0 && imp->base[c] + imp->delta[c] <= 0) lista[n++] = d;
    }
    return n;
}

/* O ultimo D_IMPORT do arquivo x; NULL se nao tem. */
static const Decl *UltimoImport(const Imports *imp, uint32_t x)
{
    const Decl *ultimo = NULL;

    for (const Decl *d = imp->p->q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_IMPORT && EditarArquivoDoOffset(imp->p->q, d->pos) == x && (ultimo == NULL || d->pos > ultimo->pos)) ultimo = d;
    }
    return ultimo;
}

/* As linhas dos imports agendados para o arquivo x, separadas por LF. */
static void LinhasNovas(const Imports *imp, uint32_t x, Buf *saida)
{
    for (uint32_t i = 0; i < imp->nnovos; i++) {
        if (imp->novos[i].x != x) continue;
        if (saida->tam > 0) BufAnexar(saida, "\n");
        BufAnexar(saida, imp->novos[i].linha);
    }
}

/* A 1a linha apagada passa a ter os imports novos; as outras saem inteiras. */
static bool TrocarApagados(Imports *imp, const Decl **apagar, uint32_t n, const Buf *novos)
{
    const Consulta *q = imp->p->q;

    for (uint32_t j = 0; j < n; j++) {
        uint32_t ini = EditarInicioDaLinha(q, apagar[j]->pos);
        bool trocar = j == 0 && novos->tam > 0;
        uint32_t fim = trocar ? EditarFimDaLinha(q, apagar[j]->pos) : EditarProximaLinha(q, apagar[j]->pos);

        if (!EditarTrocarNoArquivo(imp->p, ini, fim, trocar ? novos->dados : "")) return false;
    }
    return true;
}

/* Sem import para apagar: os novos entram depois do ultimo import, ou no topo do arquivo se ele nao tem nenhum. */
static bool InserirNovos(Imports *imp, uint32_t x, const Buf *novos)
{
    const Consulta *q = imp->p->q;
    const Decl *ultimo = UltimoImport(imp, x);
    Buf texto = {0};
    uint32_t onde = 0, seguinte = 0;
    bool ok, branco = false, cabecalho = false;

    if (ultimo != NULL && EditarLinhaSeguinte(q, ultimo->pos, &seguinte, &branco)) {
        onde = seguinte;
        BufFormatar(&texto, "%s\n", novos->dados);
    } else if (ultimo != NULL) {
        onde = EditarFimDaLinha(q, ultimo->pos);
        BufFormatar(&texto, "\n%s", novos->dados);
    } else if (EditarPontoSemImports(q, x, &onde, &cabecalho)) {
        BufFormatar(&texto, cabecalho ? "\n\n%s" : "%s\n\n", novos->dados);
    } else {
        onde = EditarFimDoConteudo(q, x);
        BufFormatar(&texto, "\n\n%s", novos->dados);
    }
    ok = EditarTrocarNoArquivo(imp->p, onde, onde, texto.dados);
    BufLiberar(&texto);
    return ok;
}

static bool AplicarNoArquivo(Imports *imp, uint32_t x)
{
    const Decl *apagar[APAGAR_MAX];
    uint32_t n = ImportsSemUso(imp, x, apagar);
    uint8_t modulo = ModulosPacoteDoArquivo(&imp->p->q->c->modulos, x);
    Buf novos = {0};
    bool ok = true;

    LinhasNovas(imp, x, &novos);
    if (n > 0) ok = EditarModuloEditavel(imp->p, modulo) && TrocarApagados(imp, apagar, n, &novos);
    else if (novos.tam > 0) ok = EditarModuloEditavel(imp->p, modulo) && InserirNovos(imp, x, &novos);
    BufLiberar(&novos);
    return ok;
}

bool ImportsAplicar(Imports *imp)
{
    for (uint32_t x = 0; x < imp->narquivos; x++) {
        if (!AplicarNoArquivo(imp, x)) return false;
    }
    return true;
}
