/* sem_chave.c - chave por modulo dos simbolos de topo (IA1-L8): preenche Decl.chave de toda declaracao do programa.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L8 P1: SemChavesPreencher (primeira versao; ninguem le Decl.chave ainda).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 *
 * Nomes de topo de modulos importados deixam de ser globais: a chave de um simbolo de modulo e "<L><tag>_<nome>" (common\nome_modulo.h), internada UMA vez aqui, antes de
 * SemanticaIniciar fixar qtd_nomes (as tabelas por id de nome tem esse tamanho; internar depois estoura). Ficam SEM prefixo (chave = nome): o modulo raiz, o main, o proc
 * `export` (a colisao de ABI continua E0202 de proposito), o foreign interno da std (forja_*, simbolo do runtime) e tudo que nao e simbolo de topo (test, link, import, assert).
 */
#include "semantic/sem_interno.h"

#include "common/nome_modulo.h"

#include <string.h>

/* Declaracao que vira simbolo de topo e, num modulo importado, ganha prefixo. */
static bool DeclEhSimboloDeTopo(const Decl *d)
{
    switch (d->tipo_no) {
    case D_UNIDADE_BASE: case D_UNIDADE_DERIV: case D_CONST: case D_STRUCT: case D_ENUM: case D_PROC: case D_UNIAO: return d->nome >= 0;
    default: return false;
    }
}

/* Foreign da std com nome forja_*: o simbolo C ja existe no runtime (o lowering o trata como interno). */
static bool EhExternoInternoDaStd(const Semantica *s, const Decl *d)
{
    return d->externo && s->modulos->itens[d->modulo].origem == MOD_STD && strncmp(InternerTexto(s->nomes, d->nome), "forja_", 6) == 0;
}

static bool DeclTemPrefixo(const Semantica *s, const Decl *d)
{
    if (d->modulo == 0 || !DeclEhSimboloDeTopo(d)) return false;
    if (d->tipo_no == D_PROC && (d->exportado || EhExternoInternoDaStd(s, d))) return false;
    return true;
}

static int32_t InternarChave(Semantica *s, const char *tag, const Decl *d)
{
    char texto[NOME_MODULO_TAG_MAX + 280];
    size_t tam = NomeModuloChave(tag, InternerTexto(s->nomes, d->nome), InternerTam(s->nomes, d->nome), texto, sizeof texto);
    if (tam == 0) FalhaInterna("nome de simbolo longo demais para a chave por modulo: %s", InternerTexto(s->nomes, d->nome));
    return Internar(s->nomes, texto, (uint32_t)tam);
}

/* Toda Decl sai com chave: o proprio nome, ou o nome com o prefixo do modulo. Sem modulos importados nada muda. */
void SemChavesPreencher(Semantica *s)
{
    char tags[MODULOS_MAX][NOME_MODULO_TAG_MAX];
    const char *caminhos[MODULOS_MAX];
    bool ha_modulos = s->modulos != NULL && s->qtd_modulos > 1;

    if (ha_modulos) {
        for (uint32_t k = 0; k < s->qtd_modulos; k++) caminhos[k] = InternerTexto(s->nomes, s->modulos->itens[k].caminho);
        NomeModuloTags(caminhos, s->qtd_modulos, tags);
    }
    s->chave_ini = (uint32_t)s->nomes->nomes.n;
    for (Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        d->chave = ha_modulos && DeclTemPrefixo(s, d) ? InternarChave(s, tags[d->modulo], d) : d->nome;
    }
    s->chave_fim = (uint32_t)s->nomes->nomes.n;
}

/* Cadeia de homonimos: por id de nome, as declaracoes de topo dos modulos importados (a raiz mora em globais[nome]). Roda depois de qtd_nomes fixada. */
void SemHomonimosIniciar(Semantica *s)
{
    if (s->modulos == NULL || s->qtd_modulos <= 1) return;
    s->homonimos = ArenaAlocar(s->arena, (size_t)s->qtd_nomes * sizeof *s->homonimos);
    for (Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo == 0 || !DeclEhSimboloDeTopo(d)) continue;
        d->homonimo = s->homonimos[d->nome];
        s->homonimos[d->nome] = d;
    }
}
