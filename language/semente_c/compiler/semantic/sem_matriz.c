/* sem_matriz.c - matrix[R, C]T (Linguagem v1.9): tipo, literal, m[i, j], + - * entre matrizes, escalar e vetor, -m e transpose (E0321).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - IA1-b07: o E0301 de indice nao inteiro leva a forma da ocorrencia (SemErroDoInteiro).
 *            0.1.0 - Linguagem v1.9 P5: ResolverMatriz, SemLiteralMatriz, SemIndiceMatriz, SemContaMatriz e SemChamarTranspose (3.6).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

/* matrix[R, C]T: T e f32 ou f64 (f32 com unidade e o inteiro caem no erro); R e C ja vieram validados (2 a 4) do parser. */
bool ResolverMatriz(Semantica *s, TipoRef *t, TipoId *base)
{
    char tipo[96];
    if (!SemResolverTipoRef(s, t->alvo)) return false;
    TipoId alvo = t->alvo->tipo;
    if (alvo != TIPO_F32 && alvo != TIPO_F64) {
        SemDescrever(s, alvo, tipo, sizeof tipo);
        return SemErro(s, E0321, t->alvo->pos, "matrix guarda f32 ou f64 (recebeu %s)", tipo);
    }
    *base = TipoMatriz(&s->tipos, alvo, (uint32_t)t->nome >> 4, (uint32_t)t->nome & 15u);
    return true;
}

/* {} = zero; senao R*C elementos escritos por linhas, cada um coagido para T. */
bool SemLiteralMatriz(Semantica *s, Expr *e, TipoId ctx)
{
    char tipo[96];
    TipoId elemento = (TipoId)TipoDe(&s->tipos, ctx)->a;
    uint32_t esperado = TipoMatrizLinhas(&s->tipos, ctx) * TipoMatrizColunas(&s->tipos, ctx);
    if (e->nargs != 0 && e->nargs != esperado) {
        SemDescrever(s, ctx, tipo, sizeof tipo);
        return SemErro(s, E0321, e->pos, "o literal tem %u elementos e %s pede %u (escritos por linhas) ou {} para zero",
                       (unsigned)e->nargs, tipo, (unsigned)esperado);
    }
    for (uint32_t i = 0; i < e->nargs; i++) {
        Expr *v = e->args[i].valor;
        if (!SemChecar(s, v, elemento) || !SemCoagir(s, v, elemento, v->pos)) return false;
    }
    e->tipo = ctx;
    return true;
}

/* Um dos dois indices: inteiro e, se constante, dentro de 0..<limite. */
static bool ConferirIndiceDaMatriz(Semantica *s, Expr *x, uint32_t limite, TipoId matriz)
{
    bool negativo;
    uint64_t mag;
    char tipo[96];
    if (!SemChecar(s, x, TIPO_INT)) return false;
    if (!TipoEhInteiro(x->tipo)) return SemErroDoInteiro(s, x, "o indice");
    if (!SemInteiroConstante(s, x, &negativo, &mag)) return true;
    if ((negativo && mag != 0) || mag >= limite) {
        SemDescrever(s, matriz, tipo, sizeof tipo);
        return SemErro(s, E0321, x->pos, "indice constante fora de 0..<%u em %s", (unsigned)limite, tipo);
    }
    return true;
}

/* m[i, j] (linha, coluna): so em matrix. */
bool SemIndiceMatriz(Semantica *s, Expr *e)
{
    if (!SemChecar(s, e->a, TIPO_VAZIO)) return false;
    TipoId matriz = e->a->tipo;
    if (TipoClasseDe(&s->tipos, matriz) != TC_MATRIZ) {
        return SemErro(s, E0308, e->pos_op, "indice duplo a[i, j] so existe em matrix");
    }
    if (!ConferirIndiceDaMatriz(s, e->b, TipoMatrizLinhas(&s->tipos, matriz), matriz)) return false;
    if (!ConferirIndiceDaMatriz(s, e->args[0].valor, TipoMatrizColunas(&s->tipos, matriz), matriz)) return false;
    e->tipo = (TipoId)TipoDe(&s->tipos, matriz)->a;
    return true;
}

/* matriz x matriz: o elemento e igual e as colunas da 1a sao as linhas da 2a. */
static bool ProdutoDeMatrizes(Semantica *s, Expr *e, TipoId ta, TipoId tb)
{
    char texto_a[96], texto_b[96];
    uint32_t colunas_a = TipoMatrizColunas(&s->tipos, ta), linhas_b = TipoMatrizLinhas(&s->tipos, tb);
    SemDescrever(s, ta, texto_a, sizeof texto_a);
    SemDescrever(s, tb, texto_b, sizeof texto_b);
    if (TipoDe(&s->tipos, ta)->a != TipoDe(&s->tipos, tb)->a) {
        return SemErro(s, E0321, e->pos_op, "produto de %s por %s: o elemento precisa ser o mesmo", texto_a, texto_b);
    }
    if (colunas_a != linhas_b) {
        return SemErro(s, E0321, e->pos_op, "produto de %s por %s: as colunas da 1a (%u) precisam ser as linhas da 2a (%u)",
                       texto_a, texto_b, (unsigned)colunas_a, (unsigned)linhas_b);
    }
    e->tipo = TipoMatriz(&s->tipos, (TipoId)TipoDe(&s->tipos, ta)->a, TipoMatrizLinhas(&s->tipos, ta), TipoMatrizColunas(&s->tipos, tb));
    return true;
}

/* matriz x [C]T -> [R]T. */
static bool ProdutoPorVetor(Semantica *s, Expr *e, TipoId matriz, TipoId vetor)
{
    char texto_m[96], texto_v[96], texto_t[96];
    const TipoInfo *k = TipoDe(&s->tipos, matriz), *v = TipoDe(&s->tipos, vetor);
    uint32_t colunas = TipoMatrizColunas(&s->tipos, matriz);
    if (v->a == k->a && v->b == colunas) {
        e->tipo = TipoArray(&s->tipos, (TipoId)k->a, TipoMatrizLinhas(&s->tipos, matriz));
        return true;
    }
    SemDescrever(s, matriz, texto_m, sizeof texto_m);
    SemDescrever(s, vetor, texto_v, sizeof texto_v);
    SemDescrever(s, (TipoId)k->a, texto_t, sizeof texto_t);
    return SemErro(s, E0321, e->pos_op, "produto de %s por %s: o vetor precisa ser [%u]%s", texto_m, texto_v, (unsigned)colunas, texto_t);
}

/* matriz x escalar ou escalar x matriz: o escalar tem o tipo do elemento. */
static bool ProdutoPorEscalar(Semantica *s, Expr *e, TipoId matriz, TipoId escalar)
{
    char texto_m[96], texto_t[96], texto_e[96];
    TipoId elemento = (TipoId)TipoDe(&s->tipos, matriz)->a;
    if (escalar == elemento) {
        e->tipo = matriz;
        return true;
    }
    SemDescrever(s, matriz, texto_m, sizeof texto_m);
    SemDescrever(s, elemento, texto_t, sizeof texto_t);
    SemDescrever(s, escalar, texto_e, sizeof texto_e);
    return SemErro(s, E0321, e->pos_op, "o escalar de %s precisa ser %s (recebeu %s)", texto_m, texto_t, texto_e);
}

/* '*' com pelo menos uma matriz. */
static bool ProdutoDaMatriz(Semantica *s, Expr *e, TipoId ta, TipoId tb)
{
    TipoClasse ca = TipoClasseDe(&s->tipos, ta), cb = TipoClasseDe(&s->tipos, tb);
    if (ca == TC_MATRIZ && cb == TC_MATRIZ) return ProdutoDeMatrizes(s, e, ta, tb);
    if (ca == TC_MATRIZ && cb == TC_ARRAY) return ProdutoPorVetor(s, e, ta, tb);
    if (ca == TC_ARRAY) return SemErro(s, E0321, e->pos_op, "vetor * matrix nao existe: escreva transpose(m) * v");
    if (ca == TC_MATRIZ) return ProdutoPorEscalar(s, e, ta, tb);
    return ProdutoPorEscalar(s, e, tb, ta);
}

/* + - * / % e bits com alguma matriz. */
bool SemContaMatriz(Semantica *s, Expr *e)
{
    char texto_a[96], texto_b[96];
    TipoId ta = e->a->tipo, tb = e->b->tipo;
    if (e->op >= OP_BIT_E && e->op <= OP_DESLOCA_DIR) {
        return SemErro(s, E0321, e->pos_op, "matrix nao tem operador de bits");
    }
    if (e->op == OP_DIV || e->op == OP_RESTO) {
        return SemErro(s, E0321, e->pos_op, "matrix tem + - e *; para dividir multiplique pelo inverso do escalar");
    }
    if (e->op == OP_MUL) return ProdutoDaMatriz(s, e, ta, tb);
    if (ta == tb) {
        e->tipo = ta;
        return true;
    }
    SemDescrever(s, ta, texto_a, sizeof texto_a);
    SemDescrever(s, tb, texto_b, sizeof texto_b);
    return SemErro(s, E0321, e->pos_op, "soma ou subtracao de %s com %s: as duas precisam ser do mesmo tipo", texto_a, texto_b);
}

/* transpose(m): matrix[R, C]T -> matrix[C, R]T. */
bool SemChamarTranspose(Semantica *s, Expr *e)
{
    char tipo[96];
    if (!SemLigarEmbutido(s, e, s->params_transpose, 1)) return false;
    Expr *v = e->args[0].valor;
    if (!SemChecar(s, v, TIPO_VAZIO)) return false;
    if (TipoClasseDe(&s->tipos, v->tipo) != TC_MATRIZ) {
        SemDescrever(s, v->tipo, tipo, sizeof tipo);
        return SemErro(s, E0321, v->pos, "transpose pede matrix (tipo %s)", tipo);
    }
    e->tipo = TipoMatriz(&s->tipos, (TipoId)TipoDe(&s->tipos, v->tipo)->a, TipoMatrizColunas(&s->tipos, v->tipo),
                         TipoMatrizLinhas(&s->tipos, v->tipo));
    return true;
}
