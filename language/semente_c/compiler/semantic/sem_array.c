/* sem_array.c - semantica de [N]T e []T: forma da colecao, indexar a[i] e literal de array '{a, b, c}'.
 * Versao: 0.8.0
 * Changelog: 0.8.0 - IA1-L5a: str indexavel por byte (s[i] -> u8, IndiceDoTexto) e fatiavel (s[lo..<hi] -> str emprestado); sai o parametro causa_str do ErroDeBase.
 *            0.7.1 - IA1-b07: o E0301 de indice e de limite de fatia nao inteiros leva a forma da ocorrencia (SemErroDoInteiro).
 *            0.7.0 - Linguagem v1.12 M3 P21v-a: SemTipoSemZero(proc) = true; '{}' de [N]S cujo S tem campo proc = E0911.
 *            0.6.0 -Linguagem v1.11 L15a: [dynamic, N]T (struct oculta) conta como colecao: SemElementoDe devolve FORMA_ARRAY / FORMA_PONTEIRO_ARRAY, SemTamanhoDe devolve a capacidade e '{}' vai para SemLiteralListaFixa.
 *            0.5.0 - Linguagem v1.9 P2: SemIndice despacha o indice duplo m[i, j] (nargs != 0) para SemIndiceDuplo (provisorio).
 *            0.5.0 (P3) - FORMA_NENHUMA com tipo v1.9 vai para SemIndiceV19; literal de classe >= TC_MAPA vai para SemLiteralV19.
 *            0.4.1 - Linguagem v1.8 P7: elemento de literal de array e ponto de posse (guarda).
 *            0.4.0 - Linguagem v1.7 P10: versao de fechamento da v1.7 (sem mudanca de logica).
 *            0.3.2 - Linguagem v1.7 P3: SemTipoSemZero(union) = true; texto do E0309 de '{}' cita union.
 *            0.3.1- Linguagem v1.7 P2: elemento do literal coage (SemCoagir); SemLiteralArray desembrulha o contexto ?T.
 *            0.3.0 - Linguagem v1.6 P4: listas [dynamic]T e ^[dynamic]T (forma, indice/limite sem tamanho fixo, fatia, literal E0309, movimento).
 *            0.2.0 - Linguagem v1.5 P3b: SemFatia (a[lo..<hi], a[lo..=hi], a[..]).
 *            0.1.0 - Linguagem v1.5 P3a: SemElementoDe, SemTamanhoDe, SemTipoSemZero, SemIndice, SemLiteralArray.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

/* Elemento de [N]T, ^[N]T ou []T e a forma encontrada; FORMA_NENHUMA devolve TIPO_NAO_RESOLVIDO. */
TipoId SemElementoDe(const Semantica *s, TipoId colecao, ColecaoForma *forma)
{
    const TipoInfo *k = TipoDe(&s->tipos, colecao);
    *forma = FORMA_NENHUMA;
    if (k->classe == TC_STRUCT && k->b != 0) { /* v1.11 L15a: [dynamic, N]T se indexa, fatia e percorre como o [N]T de itens */
        *forma = FORMA_ARRAY;
        return TipoListaFixaElemento(&s->tipos, colecao);
    }
    if (k->classe == TC_PONTEIRO && TipoEhListaFixa(&s->tipos, k->a)) {
        *forma = FORMA_PONTEIRO_ARRAY;
        return TipoListaFixaElemento(&s->tipos, k->a);
    }
    if (k->classe == TC_PONTEIRO && TipoClasseDe(&s->tipos, k->a) == TC_ARRAY) {
        *forma = FORMA_PONTEIRO_ARRAY;
        return TipoDe(&s->tipos, k->a)->a;
    }
    if (k->classe == TC_PONTEIRO && TipoClasseDe(&s->tipos, k->a) == TC_DINAMICO) {
        *forma = FORMA_PONTEIRO_DINAMICO;
        return TipoDe(&s->tipos, k->a)->a;
    }
    if (k->classe == TC_ARRAY) *forma = FORMA_ARRAY;
    if (k->classe == TC_FATIA) *forma = FORMA_FATIA;
    if (k->classe == TC_DINAMICO) *forma = FORMA_DINAMICO;
    return *forma == FORMA_NENHUMA ? TIPO_NAO_RESOLVIDO : k->a;
}

/* N de [N]T ou ^[N]T; 0 nos demais. */
uint32_t SemTamanhoDe(const Semantica *s, TipoId colecao)
{
    const TipoInfo *k = TipoDe(&s->tipos, colecao);
    if (k->classe == TC_PONTEIRO) k = TipoDe(&s->tipos, k->a);
    if (k->classe == TC_STRUCT && k->b != 0) k = TipoDe(&s->tipos, k->b); /* v1.11 L15a: o N da capacidade (indice constante >= N nunca vale) */
    return k->classe == TC_ARRAY ? k->b : 0;
}

/* ^T, union e proc (v1.12) nao tem valor zero (?T tem: none); [N]X tambem nao quando X nao tem. */
bool SemTipoSemZero(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe == TC_PONTEIRO || k->classe == TC_UNIAO || k->classe == TC_PROC) return true;
    return k->classe == TC_ARRAY && SemTipoSemZero(s, k->a);
}

/* ---------- a[i] ---------- */

/* Base de a[i] e a[lo..<hi]: so [N]T, []T, ^[N]T e str; os outros tipos dizem o que fazer. 'acao' = "indexar" ou "fatiar". */
static bool ErroDeBase(Semantica *s, const Expr *e, const char *acao)
{
    char tipo[96];
    const TipoInfo *k = TipoDe(&s->tipos, e->a->tipo);
    if (k->classe == TC_ANULAVEL && TipoClasseDe(&s->tipos, k->a) == TC_ARRAY) {
        SemDescrever(s, k->a, tipo, sizeof tipo);
        return SemErro(s, E0306, e->pos_op, "%s '?%s' pode estar em none: converta com 'p as ^%s' antes", acao, tipo, tipo);
    }
    SemDescrever(s, e->a->tipo, tipo, sizeof tipo);
    return SemErro(s, E0308, e->pos_op, "%s pede array, fatia, lista, str ou ponteiro para eles (tipo %s)", acao, tipo);
}

/* Indice constante: negativo nunca vale; em [N]T e ^[N]T, >= N tambem nao (E0421). */
static bool ConferirIndiceConstante(Semantica *s, const Expr *e, ColecaoForma forma)
{
    bool negativo;
    uint64_t mag;
    char tipo[96];
    const Expr *indice = e->b;
    if (!SemInteiroConstante(s, indice, &negativo, &mag)) return true;
    if (negativo && mag != 0) return SemErro(s, E0421, indice->pos, "indice negativo (-%llu)", (unsigned long long)mag);
    if (!FormaTemTamanhoFixo(forma) || mag <SemTamanhoDe(s, e->a->tipo)) return true;
    SemDescrever(s, e->a->tipo, tipo, sizeof tipo);
    return SemErro(s, E0421, indice->pos, "indice %llu fora de %s (0..<%u)", (unsigned long long)mag, tipo,
                   (unsigned)SemTamanhoDe(s, e->a->tipo));
}

/* L-02: s[i] e o byte (u8) do str; indice fora de 0..<len(s) e R0006 em execucao (como na fatia). */
static bool IndiceDoTexto(Semantica *s, Expr *e)
{
    if (!SemChecar(s, e->b, TIPO_INT)) return false;
    if (!TipoEhInteiro(e->b->tipo)) return SemErroDoInteiro(s, e->b, "o indice");
    if (!ConferirIndiceConstante(s, e, FORMA_FATIA)) return false;
    e->tipo = TIPO_U8;
    return true;
}

bool SemIndice(Semantica *s, Expr *e)
{
    ColecaoForma forma;
    if (__builtin_expect(e->nargs != 0, 0)) return SemIndiceDuplo(s, e);
    if (!SemChecar(s, e->a, TIPO_VAZIO)) return false;
    TipoId elemento = SemElementoDe(s, e->a->tipo, &forma);
    if (forma == FORMA_NENHUMA) {
        if (e->a->tipo == TIPO_TEXTO) return IndiceDoTexto(s, e);
        if (__builtin_expect(TiposTemV19(&s->tipos), 0) && SemEhTipoV19(s, e->a->tipo)) return SemIndiceV19(s, e);
        return ErroDeBase(s, e, "indexar");
    }
    if (!SemChecar(s, e->b, TIPO_INT)) return false;
    if (!TipoEhInteiro(e->b->tipo)) return SemErroDoInteiro(s, e->b, "o indice");
    if (!ConferirIndiceConstante(s, e, forma)) return false;
    e->tipo = elemento;
    return true;
}

/* ---------- a[lo..<hi], a[lo..=hi], a[..] ---------- */

typedef struct { bool conhecido; uint64_t valor; } LimiteConst;

/* Um limite: inteiro; constante negativa nunca vale (E0421). */
static bool LerLimite(Semantica *s, Expr *limite, LimiteConst *c)
{
    bool negativo;
    uint64_t mag;
    if (!SemChecar(s, limite, TIPO_INT)) return false;
    if (!TipoEhInteiro(limite->tipo)) return SemErroDoInteiro(s, limite, "o limite da fatia");
    c->conhecido = SemInteiroConstante(s, limite, &negativo, &mag);
    if (!c->conhecido) return true;
    if (negativo && mag != 0) return SemErro(s, E0421, limite->pos, "limite negativo (-%llu)", (unsigned long long)mag);
    c->valor = mag;
    return true;
}

/* O limite constante passa do fim do array? '..<' aceita ate N; '..=' aceita ate N-1. */
static bool LimiteForaDoArray(const Expr *e, uint64_t valor, uint64_t n)
{
    return e->op == FATIA_INCL ? valor >= n : valor > n;
}

/* Inicio depois do fim (os dois constantes); '..=' com inicio = fim + 1 e a fatia vazia. */
static bool InicioDepoisDoFim(const Expr *e, uint64_t inicio, uint64_t fim)
{
    return e->op == FATIA_INCL ? inicio > 0 && inicio - 1 > fim : inicio > fim;
}

static bool ConferirLimitesConstantes(Semantica *s, const Expr *e, ColecaoForma forma, const LimiteConst lim[2])
{
    char tipo[96];
    uint64_t n = SemTamanhoDe(s, e->a->tipo);
    const char *sinal = e->op == FATIA_INCL ? "..=" : "..<";
    SemDescrever(s, e->a->tipo, tipo, sizeof tipo);
    for (int k = 0; k < 2 && FormaTemTamanhoFixo(forma); k++) {
        if (lim[k].conhecido && (k == 0 ? lim[k].valor > n : LimiteForaDoArray(e, lim[k].valor, n))) {
            return SemErro(s, E0421, e->args[k].valor->pos, "limite %llu da fatia fora de %s (0..=%u)", (unsigned long long)lim[k].valor,
                           tipo, (unsigned)n);
        }
    }
    if (lim[0].conhecido && lim[1].conhecido && InicioDepoisDoFim(e, lim[0].valor, lim[1].valor)) {
        return SemErro(s, E0421, e->args[0].valor->pos, "fatia %llu%s%llu: o inicio passa do fim", (unsigned long long)lim[0].valor,
                       sinal, (unsigned long long)lim[1].valor);
    }
    return true;
}

static bool LimitesDaFatia(Semantica *s, Expr *e, ColecaoForma forma)
{
    LimiteConst lim[2] = {{false, 0}, {false, 0}};
    if (!LerLimite(s, e->args[0].valor, &lim[0]) || !LerLimite(s, e->args[1].valor, &lim[1])) return false;
    return ConferirLimitesConstantes(s, e, forma, lim);
}

bool SemFatia(Semantica *s, Expr *e)
{
    ColecaoForma forma;
    if (!SemChecar(s, e->a, TIPO_VAZIO)) return false;
    TipoId elemento = SemElementoDe(s, e->a->tipo, &forma);
    if (forma == FORMA_NENHUMA && e->a->tipo != TIPO_TEXTO) return ErroDeBase(s, e, "fatiar");
    if (forma == FORMA_NENHUMA) { /* L-02: s[lo..<hi] e um str emprestado do mesmo texto (sem copia); limite fora de 0..=len(s) e R0006 em execucao */
        if (e->op != FATIA_INTEIRA && !LimitesDaFatia(s, e, FORMA_FATIA)) return false;
        e->tipo = TIPO_TEXTO;
        return true;
    }
    if (forma == FORMA_ARRAY && !SemLvalorMutavelDe(s, e->a, "fatiar um array pede 'var': a fatia altera os elementos")) return false;
    if (forma == FORMA_DINAMICO && !SemLvalorMutavelDe(s, e->a, "fatiar uma lista pede 'var': a fatia altera os elementos")) return false;
    if (e->op != FATIA_INTEIRA && !LimitesDaFatia(s, e, forma)) return false;
    e->tipo = TipoFatia(&s->tipos, elemento);
    return true;
}

/* ---------- literal de array ---------- */

/* '{}' em [N]T e o valor zero, se o elemento tem zero. */
static bool LiteralVazio(Semantica *s, const Expr *e, TipoId ctx, TipoId elemento)
{
    char tipo[96];
    bool com_proc = SemProcSemZero(s, elemento);
    if (!com_proc && !SemTipoSemZero(s, elemento)) return true;
    SemDescrever(s, ctx, tipo, sizeof tipo);
    if (com_proc) {
        return SemErro(s, E0911, e->pos, "'{}' de %s: o elemento tem campo proc, que nao tem valor zero; escreva os %u elementos com o proc em cada um", tipo,
                       (unsigned)TipoDe(&s->tipos, ctx)->b);
    }
    return SemErro(s, E0309, e->pos, "'{}' de %s: o elemento nao tem valor zero (^T e union nao tem); escreva os %u elementos", tipo,
                   (unsigned)TipoDe(&s->tipos, ctx)->b);
}

static bool ElementosDoLiteral(Semantica *s, Expr *e, TipoId elemento)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        Expr *v = e->args[i].valor;
        if (!SemChecar(s, v, elemento) || !SemCoagir(s, v, elemento, v->pos)) return false;
        if (!SemConferirMovimento(s, v) || !SemConferirPosse(s, v, PONTO_GUARDA)) return false;
    }
    return true;
}

bool SemLiteralArray(Semantica *s, Expr *e, TipoId ctx)
{
    char tipo[96];
    ctx = SemCtxSemOpcional(s, ctx);
    const TipoInfo *k = TipoDe(&s->tipos, ctx);
    if (k->classe == TC_STRUCT && k->b != 0) return SemLiteralListaFixa(s, e, ctx); /* v1.11 L15a */
    if (k->classe == TC_FATIA) {
        if (e->nargs != 0) return SemErro(s, E0309, e->pos, "fatia nao tem literal com elementos: declare o array e fatie (a[..])");
        e->tipo = ctx;
        return true;
    }
    if (k->classe == TC_DINAMICO) {
        return SemErro(s, E0309, e->pos, "lista dinamica nasce de make: make([dynamic]T, alloc = heap); '{}' nao cria lista");
    }
    if (k->classe != TC_ARRAY) {
        if (k->classe >= TC_MAPA) return SemLiteralV19(s, e, ctx);
        return SemErro(s, E0309, e->pos, "o literal de array precisa do tipo pelo contexto: let a: [3]int = {1, 2, 3}");
    }
    if (e->nargs == 0 && !LiteralVazio(s, e, ctx, k->a)) return false;
    if (e->nargs != 0 && e->nargs != k->b) {
        SemDescrever(s, ctx, tipo, sizeof tipo);
        return SemErro(s, E0309, e->pos, "o literal tem %u elementos e o tipo %s pede %u", (unsigned)e->nargs, tipo, (unsigned)k->b);
    }
    if (!ElementosDoLiteral(s, e, k->a)) return false;
    e->tipo = ctx;
    return true;
}
