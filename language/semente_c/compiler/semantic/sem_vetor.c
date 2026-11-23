/* sem_vetor.c - conta elemento a elemento em [N]T (v1.11 L12a, A04, E0331): a + b, a - b, a * b, a / b, a * k, k * a, a / k, -a e as
 * formas += e -=. A regra do escalar vale por elemento: o tipo do elemento do resultado sai da mesma SemAritmetica (unidade, E0412, E0301).
 * v1.11 L12b (E0332): swizzle v.x, v.xy, v.zyx, c.bgra em [2..4]T. Uma letra vira o EX_INDICE v[k] (lvalor, '&', += e o indice constante provado
 * saem do caminho do indice); varias letras ficam EX_CAMPO com ref = REF_COMPONENTE (rvalue [k]T, lido no lowering).
 * Interface: SemContaVetor (binario, chamado por SemAritmetica quando algum operando e colecao), SemNegarVetor (-a), SemCompostaVetor (+=, -=),
 *            SemEhSwizzle e SemSwizzle (campo pedido em [N]T de numero).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.11 L12b: SemEhSwizzle e SemSwizzle (E0332).
 *            0.1.0 - Linguagem v1.11 L12a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

static bool EhVetor(const Semantica *s, TipoId t)
{
    return TipoClasseDe(&s->tipos, t) == TC_ARRAY;
}

/* O elemento de [N]T; um escalar e o proprio elemento. */
static TipoId ElementoOuProprio(const Semantica *s, TipoId t)
{
    return EhVetor(s, t) ? (TipoId)TipoDe(&s->tipos, t)->a : t;
}

/* Operando da conta: numero, ou [N]T de numero. Fatia, lista, ponteiro, bool, str, struct... = E0331. */
static bool OperandoValido(Semantica *s, const Expr *e, const Expr *x)
{
    char texto[96];
    if (TipoEhNumerico(&s->tipos, ElementoOuProprio(s, x->tipo))) return true;
    SemDescrever(s, x->tipo, texto, sizeof texto);
    return SemErro(s, E0331, e->pos_op, "%s nao faz conta elemento a elemento: so [N]T de numero (inteiro, real ou unidade)", texto);
}

/* Soma e subtracao so existem entre dois arrays. */
static bool SomaOuSubSemDoisArrays(const Semantica *s, const Expr *e)
{
    if (e->op != OP_SOMA && e->op != OP_SUB) return false;
    return !(EhVetor(s, e->a->tipo) && EhVetor(s, e->b->tipo));
}

/* As formas que existem: array x array em + - * /; array x k e k x array em *; array / k. */
static bool FormaValida(Semantica *s, const Expr *e)
{
    if (e->op == OP_RESTO) return SemErro(s, E0331, e->pos_op, "'%%' nao existe em conta de array: use for");
    if (SomaOuSubSemDoisArrays(s, e)) {
        return SemErro(s, E0331, e->pos_op, "soma e subtracao pedem dois arrays [N]T do mesmo tipo (array com escalar so em * e /)");
    }
    if (e->op == OP_DIV && !EhVetor(s, e->a->tipo)) return SemErro(s, E0331, e->pos_op, "escalar / array nao existe: so array / escalar");
    return true;
}

/* Dois arrays: o mesmo N. */
static bool MesmoTamanho(Semantica *s, const Expr *e)
{
    char texto_a[96], texto_b[96];
    if (!EhVetor(s, e->a->tipo) || !EhVetor(s, e->b->tipo)) return true;
    if (TipoDe(&s->tipos, e->a->tipo)->b == TipoDe(&s->tipos, e->b->tipo)->b) return true;
    SemDescrever(s, e->a->tipo, texto_a, sizeof texto_a);
    SemDescrever(s, e->b->tipo, texto_b, sizeof texto_b);
    return SemErro(s, E0331, e->pos_op, "%s e %s tem N diferente: a conta pede o mesmo N", texto_a, texto_b);
}

/* O tipo de cada elemento do resultado: a conta dos dois escalares, pela regra do escalar (copia rasa de e, so com os tipos dos operandos). */
static bool TipoDoElemento(Semantica *s, const Expr *e, TipoId *resultado)
{
    Expr elemento_a = {.tipo_no = EX_NOME, .tipo = ElementoOuProprio(s, e->a->tipo)};
    Expr elemento_b = {.tipo_no = EX_NOME, .tipo = ElementoOuProprio(s, e->b->tipo)};
    Expr conta = *e;
    conta.a = &elemento_a;
    conta.b = &elemento_b;
    if (!SemAritmetica(s, &conta)) return false;
    *resultado = conta.tipo;
    return true;
}

/* + - * / com algum operando colecao (e->a e e->b ja tipados). Grava e->tipo = [N]R. */
bool SemContaVetor(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!OperandoValido(s, e, e->a) || !OperandoValido(s, e, e->b)) return false;
    if (!FormaValida(s, e) || !MesmoTamanho(s, e) || !TipoDoElemento(s, e, &elemento)) return false;
    const Expr *vetor = EhVetor(s, e->a->tipo) ? e->a : e->b;
    e->tipo = TipoArray(&s->tipos, elemento, TipoDe(&s->tipos, vetor->tipo)->b);
    return true;
}

/* -a em [N]T: elemento numero com sinal. */
bool SemNegarVetor(Semantica *s, Expr *e)
{
    TipoId elemento = ElementoOuProprio(s, e->a->tipo);
    if (!TipoEhNumerico(&s->tipos, elemento)) {
        return SemErro(s, E0331, e->pos, "'-' em array pede [N]T de numero (inteiro, real ou unidade)");
    }
    if (TipoEhSemSinal(elemento)) return SemErro(s, E0305, e->pos, "'-' em inteiro sem sinal (%s)", TipoNomeFixo(elemento));
    e->tipo = e->a->tipo;
    return true;
}

/* alvo += expr e alvo -= expr com alvo [N]T: a conta alvo + expr (ou -) tem de dar o tipo do alvo. */
bool SemCompostaVetor(Semantica *s, Stmt *st)
{
    Expr conta = {.tipo_no = EX_BINARIO, .op = st->op == ATRIB_SOMA ? OP_SOMA : OP_SUB, .a = st->alvo, .b = st->expr};
    conta.pos = st->alvo->pos;
    conta.pos_op = st->expr->pos;
    if (!SemContaVetor(s, &conta)) return false;
    if (conta.tipo == st->alvo->tipo) return true;
    return SemErro(s, E0301, st->expr->pos, "a conta nao tem o tipo do alvo: o '+=' e o '-=' pedem o mesmo [N]T");
}

/* ---------- swizzle (v1.11 L12b, E0332) ---------- */

/* O ref_id empacotado de varias letras (SWIZZLE_* em frontend\ast.h) e montado em LerLetras e lido em LwSwizzle (ir\lower_vetor.c). */

/* Posicao da letra em "xyzw" (grupo 0) ou "rgba" (grupo 1); -1 se nao e letra de swizzle. */
static int PosicaoDaLetra(char letra, int *grupo)
{
    static const char *const GRUPOS[2] = {"xyzw", "rgba"};
    for (int g = 0; g < 2; g++) {
        const char *achada = strchr(GRUPOS[g], letra);
        if (achada == NULL) continue;
        *grupo = g;
        return (int)(achada - GRUPOS[g]);
    }
    return -1;
}

/* O nome inteiro tem 1 a 4 letras de swizzle (de qualquer grupo): so entao o campo pedido em [N]T e um swizzle. */
static bool NomeDeSwizzle(const char *nome)
{
    int grupo;
    size_t letras = strlen(nome);
    if (letras == 0 || letras > SWIZZLE_LETRAS_MAX) return false;
    for (size_t j = 0; j < letras; j++) {
        if (PosicaoDaLetra(nome[j], &grupo) < 0) return false;
    }
    return true;
}

/* Campo pedido em [N]T de numero (inteiro, real ou unidade) com nome de letras xyzw/rgba; o resto segue o erro de sempre (E0308). */
bool SemEhSwizzle(const Semantica *s, const Expr *e)
{
    if (!EhVetor(s, e->a->tipo)) return false;
    if (!TipoEhNumerico(&s->tipos, ElementoOuProprio(s, e->a->tipo))) return false;
    return NomeDeSwizzle(SemNomeTexto(s, e->nome));
}

/* As letras como ref_id empacotado; E0332 se o grupo mistura ou a letra passa do N. */
static bool LerLetras(Semantica *s, const Expr *e, uint32_t n, int32_t *empacotado)
{
    const char *nome = SemNomeTexto(s, e->nome);
    size_t letras = strlen(nome);
    int primeiro_grupo = -1;
    int32_t pacote = (int32_t)letras << SWIZZLE_DESLOCA_CONTA;
    for (size_t j = 0; j < letras; j++) {
        int grupo = 0, posicao = PosicaoDaLetra(nome[j], &grupo);
        if (primeiro_grupo >= 0 && grupo != primeiro_grupo) {
            return SemErro(s, E0332, e->pos_op, "'.%s' mistura xyzw com rgba: use as letras de um grupo so", nome);
        }
        primeiro_grupo = grupo;
        if ((uint32_t)posicao >= n) {
            return SemErro(s, E0332, e->pos_op, "a letra '%c' passa do tamanho: [%u]T so tem as letras ate '%c'", nome[j], (unsigned)n,
                           "xyzwrgba"[(grupo * 4) + (int)n - 1]);
        }
        pacote |= posicao << (SWIZZLE_BITS_POR_LETRA * (int)j);
    }
    *empacotado = pacote;
    return true;
}

/* O indice que a semantica cria para v.x: EX_INT de 1 digito cujo valor mora em ref_id (nao existe no fonte; o lowering e SemInteiroConstante o leem dali). */
static Expr *LiteralDeIndice(Semantica *s, const Expr *e, int32_t valor)
{
    Expr *literal = ArenaAlocar(s->arena, sizeof *literal);
    literal->tipo_no = EX_INT;
    literal->op = 1;
    literal->ref = REF_COMPONENTE;
    literal->ref_id = valor;
    literal->tipo = TIPO_INT;
    literal->pos = literal->pos_op = e->pos_op;
    return literal;
}

/* v.letras com v ja tipado [N]T de numero: uma letra vira v[k]; 2 a 4 letras viram o rvalue [k]T. */
bool SemSwizzle(Semantica *s, Expr *e)
{
    const TipoInfo *vetor = TipoDe(&s->tipos, e->a->tipo);
    uint32_t n = vetor->b;
    TipoId elemento = (TipoId)vetor->a;
    int32_t pacote = 0;
    if (n < 2 || n > SWIZZLE_LETRAS_MAX) {
        return SemErro(s, E0332, e->pos_op, "as letras x y z w (ou r g b a) so existem em [2]T, [3]T e [4]T; em [%u]T use o indice", (unsigned)n);
    }
    if (!LerLetras(s, e, n, &pacote)) return false;
    uint32_t letras = (uint32_t)pacote >> SWIZZLE_DESLOCA_CONTA;
    if (letras == 1) {
        e->tipo_no = EX_INDICE;
        e->b = LiteralDeIndice(s, e, pacote & SWIZZLE_MASCARA_LETRA);
        e->tipo = elemento;
        return true;
    }
    e->ref = REF_COMPONENTE;
    e->ref_id = pacote;
    e->tipo = TipoArray(&s->tipos, elemento, letras);
    return true;
}
