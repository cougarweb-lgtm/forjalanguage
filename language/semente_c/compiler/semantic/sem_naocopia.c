/* sem_naocopia.c - tipo que nao se copia (Linguagem v1.12 M5 P48, S18, E0339): Mutex, Cond e os Atomic* de std:sync e std:atomic.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M5 P50: comentario da lista atualizado (std:atomic existe desde o P50); sem mudanca de comportamento.
 *            0.1.0 - M5 P48: lista fechada da std (como Arena/Pool, D-R10/Q-R4); copia de lvalor, parametro por valor, retorno, variante de union, elemento de [dynamic] e valor de map; struct com campo marcado herda a marca.
 */
#include "semantic/sem_interno.h"

#include <stdio.h>
#include <string.h>

enum { PROFUNDIDADE_NAO_COPIAVEL_MAX = 32 };

/* Lista fechada: so estes nomes da std (std:sync e std:atomic) sao marcados. */
static const char *const NOMES_NAO_COPIAVEIS[] = {"Mutex", "Cond", "AtomicI32", "AtomicI64", "AtomicU64", "AtomicPtr"};

/* Struct declarada na std (origem MOD_STD) com um dos nomes marcados. */
static bool EhStructMarcadaDaStd(const Semantica *s, const Decl *d)
{
    const char *nome;
    if (s->modulos->itens[d->modulo].origem != MOD_STD) return false;
    nome = SemNomeTexto(s, d->nome);
    for (size_t k = 0; k < sizeof NOMES_NAO_COPIAVEIS / sizeof NOMES_NAO_COPIAVEIS[0]; k++) {
        if (strcmp(nome, NOMES_NAO_COPIAVEIS[k]) == 0) return true;
    }
    return false;
}

static bool TipoNaoCopiavelAte(const Semantica *s, TipoId t, int profundidade);

/* Struct marcada, ou com algum campo que nao se copia: a marca passa para a struct inteira. */
static bool StructNaoCopiavel(const Semantica *s, TipoId t, int profundidade)
{
    const Decl *d = s->globais[TipoDe(&s->tipos, t)->a].decl;
    if (EhStructMarcadaDaStd(s, d)) return true;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (TipoNaoCopiavelAte(s, d->campos[i].tipo.tipo, profundidade + 1)) return true;
    }
    return false;
}

static bool TipoNaoCopiavelAte(const Semantica *s, TipoId t, int profundidade)
{
    TipoClasse c;
    if (profundidade > PROFUNDIDADE_NAO_COPIAVEL_MAX) return false;
    c = TipoClasseDe(&s->tipos, t);
    if (c == TC_STRUCT) return StructNaoCopiavel(s, t, profundidade);
    if (c == TC_ARRAY || c == TC_RESULTADO || c == TC_OPCIONAL) {
        return TipoNaoCopiavelAte(s, (TipoId)TipoDe(&s->tipos, t)->a, profundidade + 1);
    }
    return false;
}

/* Registro da struct (RegistrarAlocador): a marca liga a conferencia de copia, que fica desligada sem std:sync/std:atomic. */
void SemRegistrarNaoCopiavel(Semantica *s, const Decl *d)
{
    if (EhStructMarcadaDaStd(s, d)) s->tem_nao_copiavel = true;
}

bool SemTipoEhNaoCopiavel(const Semantica *s, TipoId t)
{
    return s->tem_nao_copiavel && TipoNaoCopiavelAte(s, t, 0);
}

/* E0339 com a forma 'passe ^T' (o nome do tipo marcado sai no texto). */
static bool ErroNaoCopiavel(Semantica *s, TipoId t, uint32_t pos, const char *motivo)
{
    char texto[96], forma[128];
    SemDescrever(s, t, texto, sizeof texto);
    SemErro(s, E0339, pos, "%s nao se copia: %s", texto, motivo);
    if (s->diag->tem && s->diag->codigo == E0339) {
        snprintf(forma, sizeof forma, "passe ^%s", texto);
        DiagForma(s->diag, NULL, forma);
    }
    return false;
}

/* Copia de lvalor (nome, campo, elemento, p^) de tipo que nao se copia = E0339; valor novo (chamada, literal) se move. */
bool SemConferirNaoCopiavel(Semantica *s, const Expr *e)
{
    if (e->tipo_no == EX_EMBRULHA) return true;
    if (!SemTipoEhNaoCopiavel(s, e->tipo)) return true;
    if (e->tipo_no == EX_OR_ELSE) return SemConferirNaoCopiavel(s, e->b);
    if (e->tipo_no == EX_NOME || e->tipo_no == EX_CAMPO || e->tipo_no == EX_INDICE ||
        (e->tipo_no == EX_UNARIO && e->op == OP_DEREF)) {
        return ErroNaoCopiavel(s, e->tipo, e->pos, "copiar um lvalor cria outro dono do mesmo estado (use ^T e &x)");
    }
    return true;
}

/* Lugar que nao aceita o tipo por valor (parametro, retorno, variante de union, elemento de [dynamic], valor de map). */
bool SemExigeCopiavel(Semantica *s, TipoId t, uint32_t pos, const char *motivo)
{
    if (!SemTipoEhNaoCopiavel(s, t)) return true;
    return ErroNaoCopiavel(s, t, pos, motivo);
}
