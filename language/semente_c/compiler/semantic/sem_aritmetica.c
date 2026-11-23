/* sem_aritmetica.c - aritmetica inteira com volta, saturada, checada e com par (wrapping_*, saturating_*, checked_*, overflowing_*), bits
 * (rotate_left, rotate_right, count_ones, leading_zeros, trailing_zeros, byte_swap) e mul_hi_u64 (IA1-L9a, IA1-L9b; E0334).
 * wrapping_add(a, b = c), saturating_sub(a, b = c) ...: dois inteiros do MESMO tipo (i8..i64, u8..u64, int, uint); o literal adota o tipo do outro.
 * Resultado: wrapping e saturating = T; checked = ?T (vazio no estouro); overflowing = o par (valor: T, estourou: bool) para 'let v, e = ...'.
 * rotate_left/right(x, n = k): x inteiro, k inteiro de qualquer tipo (vale k modulo a largura de x); resultado = tipo de x.
 * count_ones/leading_zeros/trailing_zeros(x): x inteiro; resultado int (contagem pequena, 0..64; int e o inteiro padrao e o indice dos arrays).
 * byte_swap(x): x inteiro; resultado = tipo de x. mul_hi_u64(a, b = c): dois u64 (uint); resultado u64 = metade alta do produto de 128 bits.
 * Interface (sem_interno.h): SemAritmeticaInternar, SemAritmeticaRegistrar (chamados por semantic.c) e SemChamarAritmetica (ChamarPorFaixa em sem_call.c).
 * O abaixamento mora em ir\lower_aritmetica.c e ir\lower_bits.c.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9a/L9b: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include <string.h>

/* Na ordem de EMB_WRAPPING_ADD..EMB_MUL_HI_U64 (ast.h). */
static const char *const NOMES[] = {
    "wrapping_add", "wrapping_sub", "wrapping_mul", "saturating_add", "saturating_sub", "saturating_mul",
    "checked_add", "checked_sub", "checked_mul", "overflowing_add", "overflowing_sub", "overflowing_mul",
    "rotate_left", "rotate_right", "count_ones", "leading_zeros", "trailing_zeros", "byte_swap", "mul_hi_u64",
};
enum { NOMES_QTD = sizeof NOMES / sizeof NOMES[0] };

_Static_assert(NOMES_QTD == EMB_MUL_HI_U64 - EMB_WRAPPING_ADD + 1, "NOMES precisa seguir EmbutidoId");

void SemAritmeticaInternar(Semantica *s)
{
    for (int k = 0; k < NOMES_QTD; k++) Internar(s->nomes, NOMES[k], (uint32_t)strlen(NOMES[k]));
}

/* O Internar de um nome ja internado devolve o mesmo id (SemAritmeticaInternar o criou antes de fixar qtd_nomes). */
void SemAritmeticaRegistrar(Semantica *s)
{
    for (int k = 0; k < NOMES_QTD; k++) {
        Simbolo *y = &s->globais[Internar(s->nomes, NOMES[k], (uint32_t)strlen(NOMES[k]))];
        y->categoria = (uint8_t)SIM_PROC_EMBUTIDO;
        y->indice = EMB_WRAPPING_ADD + k;
        y->tipo = TIPO_VAZIO;
    }
}

/* O argumento k precisa ser inteiro (a mensagem diz qual embutido pediu). */
static bool ConferirInteiro(Semantica *s, const Expr *e, uint32_t k)
{
    char texto[96];
    const Expr *v = e->args[k].valor;
    if (TipoEhInteiro(v->tipo)) return true;
    SemDescrever(s, v->tipo, texto, sizeof texto);
    return SemErro(s, E0334, v->pos, "%s pede inteiro (i8..i64, u8..u64, int, uint), nao %s", SemNomeTexto(s, e->nome), texto);
}

/* Os dois argumentos inteiros e do mesmo tipo. */
static bool ConferirDoisInteirosIguais(Semantica *s, const Expr *e)
{
    char a[96], b[96];
    const Expr *v = e->args[1].valor;
    if (!ConferirInteiro(s, e, 0) || !ConferirInteiro(s, e, 1)) return false;
    if (v->tipo == e->args[0].valor->tipo) return true;
    SemDescrever(s, e->args[0].valor->tipo, a, sizeof a);
    SemDescrever(s, v->tipo, b, sizeof b);
    return SemErro(s, E0334, v->pos, "%s pede inteiros do mesmo tipo: %s e %s", SemNomeTexto(s, e->nome), a, b);
}

/* Tipo do resultado da familia (EMB_ARIT_FAMILIAS) para o tipo T dos operandos. */
static TipoId ResultadoDaFamilia(Semantica *s, uint32_t familia, TipoId tipo)
{
    TipoId elementos[2] = {tipo, TIPO_BOOL};
    if (familia == 2) return TipoOpcional(&s->tipos, tipo);
    if (familia == 3) return TipoTupla(&s->tipos, elementos, 2);
    return tipo;
}

static bool ChamarOperacao(Semantica *s, Expr *e, int32_t embutido)
{
    if (!SemLigarEmbutido(s, e, s->params_minmax, 2) || !SemTiparArgumentos(s, e, TIPO_VAZIO)) return false;
    if (!ConferirDoisInteirosIguais(s, e)) return false;
    e->tipo = ResultadoDaFamilia(s, (uint32_t)(embutido - EMB_WRAPPING_ADD) / EMB_ARIT_FAMILIA_TAM, e->args[0].valor->tipo);
    return true;
}

static bool ChamarRotacao(Semantica *s, Expr *e)
{
    if (!SemLigarEmbutido(s, e, s->params_rotacao, 2) || !SemTiparArgumentos(s, e, TIPO_VAZIO)) return false;
    if (!ConferirInteiro(s, e, 0) || !ConferirInteiro(s, e, 1)) return false;
    e->tipo = e->args[0].valor->tipo;
    return true;
}

/* count_ones, leading_zeros, trailing_zeros (-> int) e byte_swap (-> o tipo de x). */
static bool ChamarUmInteiro(Semantica *s, Expr *e, int32_t embutido)
{
    if (!SemLigarEmbutido(s, e, s->params_um, 1) || !SemTiparArgumentos(s, e, TIPO_VAZIO)) return false;
    if (!ConferirInteiro(s, e, 0)) return false;
    e->tipo = embutido == EMB_BYTE_SWAP ? e->args[0].valor->tipo : TIPO_INT;
    return true;
}

static bool ChamarMulHi(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, s->params_minmax, 2) || !SemTiparArgumentos(s, e, TIPO_UINT)) return false;
    for (uint32_t k = 0; k < 2; k++) {
        const Expr *v = e->args[k].valor;
        if (v->tipo == TIPO_UINT) continue;
        SemDescrever(s, v->tipo, texto, sizeof texto);
        return SemErro(s, E0334, v->pos, "mul_hi_u64 pede dois u64, nao %s", texto);
    }
    e->tipo = TIPO_UINT;
    return true;
}

bool SemChamarAritmetica(Semantica *s, Expr *e, int32_t embutido)
{
    if (embutido <= EMB_OVERFLOWING_MUL) return ChamarOperacao(s, e, embutido);
    if (embutido <= EMB_ROTATE_RIGHT) return ChamarRotacao(s, e);
    if (embutido == EMB_MUL_HI_U64) return ChamarMulHi(s, e);
    return ChamarUmInteiro(s, e, embutido);
}
