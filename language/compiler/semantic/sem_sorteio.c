/* sem_sorteio.c - shuffle(s, rng = &r) e choice(s, rng = &r) -> ?T (S2b): embutidos polimorficos de colecao com o gerador de std:rand.
 * O rng e OBRIGATORIO e tem de ser ^Rng do modulo std:rand: falta, Rng por valor ou Rng de outro modulo = E0338. Colecao errada = E0310;
 * elemento dono no choice = E0311 (o sorteado sai como copia); shuffle de elemento dono e permitido (so troca de lugar, como sort e reverse).
 * A aleatoriedade vem so do proc below de std:rand (lower_sorteio.c o chama por IR_CHAMADA); nao ha RNG global (ADR-39).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - S2b: SemBelowDoRng recusa o Rng por valor (so ^Rng vale; antes a struct passava e o E0338 nao saia).
 *            0.1.0 - S2b: primeira versao (SemChamarShuffle, SemChamarChoice e SemBelowDoRng).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

#include <string.h>

/* O nome interno e exatamente o texto dado (sem depender de o texto vir terminado em NUL). */
static bool NomeEhTexto(const Semantica *s, int32_t nome, const char *texto)
{
    size_t tam = strlen(texto);
    return InternerTam(s->nomes, nome) == tam && memcmp(InternerTexto(s->nomes, nome), texto, tam) == 0;
}

/* A struct Rng pub do modulo de caminho "std:rand" (origem MOD_STD). O apelido do import nao importa. */
static bool EhRngDoRand(const Semantica *s, const Decl *d)
{
    const Modulo *m;
    if (d == NULL || d->tipo_no != D_STRUCT || d->modulo == 0 || s->modulos == NULL) return false;
    if (!NomeEhTexto(s, d->nome, "Rng")) return false;
    m = &s->modulos->itens[d->modulo];
    return m->origem == MOD_STD && NomeEhTexto(s, m->caminho, "std:rand");
}

/* O proc below pub do mesmo modulo do Rng, ou NULL se o tipo nao e ^Rng de std:rand. */
const Decl *SemBelowDoRng(const Semantica *s, TipoId ponteiro)
{
    const Decl *rng;
    if (TipoClasseDe(&s->tipos, ponteiro) != TC_PONTEIRO) return NULL; /* Rng por valor nao vale: tem de ser ^Rng (E0338) */
    rng = SemDeclDaStruct(s, ponteiro);
    if (!EhRngDoRand(s, rng)) return NULL;
    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_PROC && d->modulo == rng->modulo && d->publico && NomeEhTexto(s, d->nome, "below")) return d;
    }
    return NULL;
}

/* E0338: o rng nao e ^rand.Rng (falta, valor sem &, ou Rng de outro modulo). */
static bool ErroRng(Semantica *s, const Expr *e, const char *motivo)
{
    return SemErro(s, E0338, e->pos_op, "%s: o rng nao e ^rand.Rng de std:rand (%s)", SemNomeTexto(s, e->nome), motivo);
}

/* O rng e o argumento 1 (rng = &r): ausente, por valor ou de outro tipo = E0338. */
static bool ConferirRng(Semantica *s, Expr *e)
{
    Arg *rng = ArgumentoDoParametro(e, 1);
    if (rng == NULL) return ErroRng(s, e, "falta o argumento rng = &r");
    if (!SemChecar(s, rng->valor, TIPO_VAZIO)) return false;
    if (SemBelowDoRng(s, rng->valor->tipo) != NULL) return true;
    return ErroRng(s, e, "passe o endereco de um Rng: rng = &r, com r de rand.seed");
}

/* A colecao (argumento 0) ja checada: lista, array ou fatia; shuffle altera, choice so le. */
static bool ColecaoDoSorteio(Semantica *s, Expr *e, bool escolha, TipoId *elemento)
{
    Expr *v = ArgumentoDoParametro(e, 0)->valor;
    return SemChecar(s, v, TIPO_VAZIO) && SemColecaoJaChecada(s, e, 0, !escolha, elemento);
}

/* shuffle (escolha false) e choice (true): colecao, depois rng; choice confere o dono e embrulha o sorteado. */
static bool ChamarSorteio(Semantica *s, Expr *e, bool escolha)
{
    TipoId elemento;
    bool ponteiro;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_SORTEIO], 2) || !ColecaoDoSorteio(s, e, escolha, &elemento)) return false;
    if (!ConferirRng(s, e)) return false;
    if (!escolha) {
        e->tipo = TIPO_VAZIO;
        return true;
    }
    if (!SemListaSemDono(s, e, elemento, "sortearia")) return false;
    ponteiro = TipoClasseDe(&s->tipos, elemento) == TC_PONTEIRO;
    e->tipo = ponteiro ? TipoAnulavel(&s->tipos, (TipoId)TipoDe(&s->tipos, elemento)->a) : TipoOpcional(&s->tipos, elemento);
    return true;
}

bool SemChamarShuffle(Semantica *s, Expr *e)
{
    return ChamarSorteio(s, e, false);
}

bool SemChamarChoice(Semantica *s, Expr *e)
{
    return ChamarSorteio(s, e, true);
}
