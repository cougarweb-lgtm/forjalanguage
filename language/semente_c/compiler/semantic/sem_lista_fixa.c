/* sem_lista_fixa.c - [dynamic, N]T, a lista de capacidade fixa dentro do proprio valor (v1.11 L15a, A08, E0335).
 * No IR e uma struct OCULTA {itens: [N]T, n: int}, uma por tipo (Decl fora da lista do programa, registrada na 1a vez que o tipo aparece; o lowering as
 * registra). O TipoId e TC_STRUCT com b = TipoId do [N]T de itens (tipos.h: TipoEhListaFixa), entao IR, layout e backends a veem como struct comum e
 * [N]Lista, ^Lista e campo de struct funcionam sem classe nova. Os nomes "0lista_fixa_<k>" (nao escrevivel no fonte) nascem todos em SemanticaIniciar porque
 * as tabelas por id de nome tem o tamanho de qtd_nomes. Para a semantica a lista e uma colecao de [N]T (SemElementoDe devolve FORMA_ARRAY /
 * FORMA_PONTEIRO_ARRAY): l[i], &l[i], l[..], for, for &item, for i, item e len(l) seguem o caminho do array; o que muda (len = n, l[i] contra n, append)
 * e do lowering. Regras do E0335: N literal ou const de 1 a 65536; elemento nao pode ser dono ([dynamic]T, map...); '{}' vazio; sem make, delete, cap,
 * literal com elementos nem campo.
 * Interface (sem_interno.h): SemListasFixasIniciar, SemResolverListaFixa, SemLiteralListaFixa, SemEhListaFixaOuPonteiro e SemErroDeListaFixa.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.11 L15a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include <stdio.h>
#include <string.h>

enum { LISTA_FIXA_CAMPOS = 2 };

/* Chamado por SemanticaIniciar: os nomes dos campos e os LISTAS_FIXAS_MAX nomes das structs ocultas. */
void SemListasFixasIniciar(Semantica *s)
{
    char texto[32];
    s->nome_itens = Internar(s->nomes, "itens", 5);
    s->nome_n = Internar(s->nomes, "n", 1);
    for (uint32_t k = 0; k < LISTAS_FIXAS_MAX; k++) {
        int n = snprintf(texto, sizeof texto, "0lista_fixa_%u", (unsigned)k);
        s->nomes_listas_fixas[k] = Internar(s->nomes, texto, (uint32_t)n);
    }
}

/* Os campos itens: [N]T e n: int. */
static Campo *CamposDaLista(Semantica *s, TipoId array)
{
    const int32_t nomes[LISTA_FIXA_CAMPOS] = {s->nome_itens, s->nome_n};
    const TipoId tipos[LISTA_FIXA_CAMPOS] = {array, TIPO_INT};
    Campo *campos = ArenaAlocar(s->arena, LISTA_FIXA_CAMPOS * sizeof *campos);
    memset(campos, 0, LISTA_FIXA_CAMPOS * sizeof *campos);
    for (int k = 0; k < LISTA_FIXA_CAMPOS; k++) {
        campos[k].nome = nomes[k];
        campos[k].tipo.forma = TR_NOME;
        campos[k].tipo.nome = -1;
        campos[k].tipo.erro = -1;
        campos[k].tipo.tipo = tipos[k];
    }
    return campos;
}

/* Cria a struct oculta do [N]T 'array' (a k-esima do programa). */
static TipoId NovaListaFixa(Semantica *s, TipoId array)
{
    uint32_t k = s->qtd_listas_fixas++;
    Decl *d = ArenaAlocar(s->arena, sizeof *d);
    memset(d, 0, sizeof *d);
    d->tipo_no = D_STRUCT;
    d->nome = s->nomes_listas_fixas[k];
    d->chave = d->nome;
    d->simbolo = -1;
    d->publico = true;
    d->campos = CamposDaLista(s, array);
    d->ncampos = LISTA_FIXA_CAMPOS;
    d->indice = (int32_t)s->qtd_struct++;
    TipoId tipo = TipoListaFixa(&s->tipos, d->nome, array);
    s->globais[d->nome] = (Simbolo){SIM_STRUCT, d->indice, d, tipo, 0};
    s->decls_listas_fixas[k] = d;
    s->tipos_listas_fixas[k] = tipo;
    return tipo;
}

/* O TipoId da lista fixa de itens 'array', criada na 1a vez; 0 com o teto de tipos estourado (o erro ja foi relatado). */
static TipoId ListaFixaDoArray(Semantica *s, TipoId array, uint32_t pos)
{
    for (uint32_t k = 0; k < s->qtd_listas_fixas; k++) {
        if (TipoListaFixaArray(&s->tipos, s->tipos_listas_fixas[k]) == array) return s->tipos_listas_fixas[k];
    }
    if (s->qtd_listas_fixas == LISTAS_FIXAS_MAX) {
        SemErro(s, E0335, pos, "o programa usa mais de %d tipos [dynamic, N]T diferentes", LISTAS_FIXAS_MAX);
        return 0;
    }
    return NovaListaFixa(s, array);
}

/* O elemento: nao pode ser 'T ! E' e nao pode ser dono (a copia da lista copiaria o dono). */
static bool ConferirElemento(Semantica *s, const TipoRef *alvo)
{
    char texto[96];
    if (TipoClasseDe(&s->tipos, alvo->tipo) == TC_RESULTADO) {
        return SemErro(s, E0308, alvo->pos, "lista de 'T ! E' nao existe: guarde o valor ou o erro");
    }
    if (!SemTipoEhDono(s, alvo->tipo)) return true;
    SemDescrever(s, alvo->tipo, texto, sizeof texto);
    return SemErro(s, E0335, alvo->pos, "[dynamic, N]%s nao existe: a lista fixa e valor e copiaria o dono; use [dynamic]T com make", texto);
}

/* '[dynamic, N]T' escrito: resolve o elemento e N (1..65536) e devolve a struct oculta. */
bool SemResolverListaFixa(Semantica *s, TipoRef *t, TipoId *base)
{
    uint32_t n = 0;
    if (!SemResolverTipoRef(s, t->alvo) || !ConferirElemento(s, t->alvo)) return false;
    if (!SemResolverCapacidade(s, t, &n)) return false;
    *base = ListaFixaDoArray(s, TipoArray(&s->tipos, t->alvo->tipo, n), t->pos);
    return *base != 0;
}

/* '{}' em [dynamic, N]T: a lista vazia; com elementos nao existe. */
bool SemLiteralListaFixa(Semantica *s, Expr *e, TipoId ctx)
{
    if (e->nargs != 0) {
        return SemErro(s, E0335, e->pos, "[dynamic, N]T nao tem literal com elementos: comece com {} e use append(&l, item = x)");
    }
    e->tipo = ctx;
    return true;
}

/* O tipo e [dynamic, N]T ou ^[dynamic, N]T? (caminho de erro e de len/append) */
bool SemEhListaFixaOuPonteiro(const Semantica *s, TipoId t)
{
    bool por_ponteiro;
    return TipoListaFixaDe(&s->tipos, t, &por_ponteiro) != 0;
}

/* Erro E0335 com a causa dada (um atalho para os pontos de entrada que recusam make, delete e campo). */
bool SemErroDeListaFixa(Semantica *s, uint32_t pos, const char *causa)
{
    return SemErro(s, E0335, pos, "%s", causa);
}
