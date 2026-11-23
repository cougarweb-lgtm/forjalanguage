/* sem_matematica.c - min, max, abs, clamp e sqrt embutidos (v1.11 L13, A13, E0334).
 * min(a, b = c) e max(a, b = c): dois valores do MESMO tipo numerico (inteiro, real, unidade; [N]T por elemento). clamp(x, lo = a, hi = b): tres do mesmo
 * tipo, lo > hi constante e erro. abs(x): inteiro com sinal ou real (unidade vale). sqrt(x): f32 ou f64 sem unidade. O resultado tem o tipo do argumento.
 * Um literal sozinho adota o tipo do outro argumento (min(a, b = 3.5) com a: f32). O abaixamento mora em ir\lower_matematica.c.
 * Interface (sem_interno.h): SemChamarMatematica (chamado por ChamarEmbutido em sem_call.c para EMB_MIN em diante).
 * Versao: 0.1.3
 * Changelog: 0.1.3 - IA1-L9a: TiparArgumentos vira SemTiparArgumentos (publico; sem_aritmetica.c a usa).
 *            0.1.2 - Linguagem v1.11 L14b: abs(q) desvia para SemAbsDeQuat (a norma, f32).
 *            0.1.1 - Linguagem v1.11 L14a: abs(q) = E0333 (a norma do quat chega no L14b).
 *            0.1.0 - Linguagem v1.11 L13: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include <stdlib.h>

/* Numero escalar, ou [N]T de numero. */
static bool EhNumeroOuVetor(const Semantica *s, TipoId t)
{
    if (TipoClasseDe(&s->tipos, t) == TC_ARRAY) t = (TipoId)TipoDe(&s->tipos, t)->a;
    return TipoEhNumerico(&s->tipos, t);
}

/* Indice do 1o argumento que nao e literal (0 se todos forem literais). */
static uint32_t IndiceDoQueManda(const Expr *e)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (!e->args[i].valor->flexivel) return i;
    }
    return 0;
}

/* Tipa um argumento: o literal adota o tipo 'adotado'; os demais se tipam sozinhos. */
static bool TiparUm(Semantica *s, Expr *valor, TipoId adotado)
{
    return SemChecar(s, valor, valor->flexivel ? adotado : TIPO_VAZIO);
}

/* Tipa os argumentos (ja ligados, um por parametro, na ordem): o 1o que nao e literal manda; os literais adotam o tipo dele. */
bool SemTiparArgumentos(Semantica *s, Expr *e, TipoId contexto_literal)
{
    uint32_t base = IndiceDoQueManda(e);
    if (!TiparUm(s, e->args[base].valor, contexto_literal)) return false;
    TipoId manda = e->args[base].valor->tipo;
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (i != base && !TiparUm(s, e->args[i].valor, manda)) return false;
    }
    return true;
}

/* min, max e clamp: todos os argumentos do mesmo tipo, e esse tipo e numero ou [N]T de numero. */
static bool ConferirMesmoTipoNumerico(Semantica *s, const Expr *e)
{
    char a[96], b[96];
    TipoId tipo = e->args[0].valor->tipo;
    for (uint32_t i = 1; i < e->nargs; i++) {
        const Expr *v = e->args[i].valor;
        if (v->tipo == tipo) continue;
        SemDescrever(s, tipo, a, sizeof a);
        SemDescrever(s, v->tipo, b, sizeof b);
        return SemErro(s, E0334, v->pos, "%s pede valores do mesmo tipo: %s e %s", SemNomeTexto(s, e->nome), a, b);
    }
    if (EhNumeroOuVetor(s, tipo)) return true;
    SemDescrever(s, tipo, a, sizeof a);
    return SemErro(s, E0334, e->args[0].valor->pos, "%s pede numero (inteiro, real, unidade ou [N]T deles), nao %s", SemNomeTexto(s, e->nome), a);
}

/* Valor de um literal numerico (inteiro, real, '-' na frente, const inteira): so para ver se lo > hi. */
static bool ValorConstante(const Semantica *s, const Expr *e, double *valor)
{
    bool negativo;
    uint64_t magnitude;
    if (SemInteiroConstante(s, e, &negativo, &magnitude)) {
        *valor = negativo ? -(double)magnitude : (double)magnitude;
        return true;
    }
    const Expr *numero = e->tipo_no == EX_UNARIO && e->op == OP_NEGAR ? e->a : e;
    if (numero->tipo_no != EX_REAL) return false;
    *valor = strtod(s->fonte->texto + numero->pos_op, NULL);
    if (numero != e) *valor = -*valor;
    return true;
}

/* clamp(x, lo = a, hi = b) com lo e hi constantes e lo > hi nunca daria um intervalo. */
static bool ConferirIntervalo(Semantica *s, const Expr *e)
{
    double lo, hi;
    const Expr *vlo = e->args[1].valor, *vhi = e->args[2].valor;
    if (!ValorConstante(s, vlo, &lo) || !ValorConstante(s, vhi, &hi) || lo <= hi) return true;
    return SemErro(s, E0334, vlo->pos, "clamp com lo maior que hi: o intervalo [lo, hi] fica vazio");
}

static bool ChamarMinMax(Semantica *s, Expr *e)
{
    if (!SemLigarEmbutido(s, e, s->params_minmax, 2) || !SemTiparArgumentos(s, e, TIPO_VAZIO)) return false;
    if (!ConferirMesmoTipoNumerico(s, e)) return false;
    e->tipo = e->args[0].valor->tipo;
    return true;
}

static bool ChamarClamp(Semantica *s, Expr *e)
{
    if (!SemLigarEmbutido(s, e, s->params_clamp, 3) || !SemTiparArgumentos(s, e, TIPO_VAZIO)) return false;
    if (!ConferirMesmoTipoNumerico(s, e) || !ConferirIntervalo(s, e)) return false;
    e->tipo = e->args[0].valor->tipo;
    return true;
}

/* abs(x): inteiro com sinal ou real (a unidade fica); sem sinal e array nao tem. */
static bool ChamarAbs(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, s->params_um, 1) || !SemTiparArgumentos(s, e, TIPO_VAZIO)) return false;
    TipoId tipo = e->args[0].valor->tipo;
    if (SemEhQuat(s, tipo)) return SemAbsDeQuat(s, e); /* v1.11 L14b: abs(q) = a norma */
    bool com_sinal = TipoEhInteiro(tipo) && !TipoEhSemSinal(tipo);
    if (!com_sinal && !TipoEhReal(&s->tipos, tipo)) {
        SemDescrever(s, tipo, texto, sizeof texto);
        return SemErro(s, E0334, e->args[0].valor->pos, "abs pede inteiro com sinal ou real, nao %s (sem sinal nao tem abs)", texto);
    }
    e->tipo = tipo;
    return true;
}

/* sqrt(x): so f32 ou f64 sem unidade (um literal sozinho vira f32). */
static bool ChamarSqrt(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, s->params_um, 1) || !SemTiparArgumentos(s, e, TIPO_F32)) return false;
    TipoId tipo = e->args[0].valor->tipo;
    if (tipo != TIPO_F32 && tipo != TIPO_F64) {
        SemDescrever(s, tipo, texto, sizeof texto);
        return SemErro(s, E0334, e->args[0].valor->pos, "sqrt pede f32 ou f64 sem unidade, nao %s", texto);
    }
    e->tipo = tipo;
    return true;
}

bool SemChamarMatematica(Semantica *s, Expr *e, int32_t embutido)
{
    if (embutido == EMB_MIN || embutido == EMB_MAX) return ChamarMinMax(s, e);
    if (embutido == EMB_CLAMP) return ChamarClamp(s, e);
    if (embutido == EMB_ABS) return ChamarAbs(s, e);
    return ChamarSqrt(s, e);
}
