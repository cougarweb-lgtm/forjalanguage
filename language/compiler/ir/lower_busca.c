/* lower_busca.c - lowering dos embutidos de busca e movimento sobre fatia (IA1-L9d): index_of, last_index_of, contains, count, copy, fill, clone, swap, reverse,
 * replace_value. A semantica ja trocou lista e array por s[..], entao o 1o argumento chega aqui como fatia []T (um IR_FATIA, avaliado uma vez). Sem IrOp nova:
 * lacos IR_PARA com IR_ELEMENTO / IR_ENDERECO_ELEMENTO + IR_GRAVA_PONTEIRO (indice provado, sem R0006 dentro dos lacos; a unica checagem de indice e a do swap).
 *   index_of      laco 0..<n, 1o igual: resultado = ?int embrulhado e PARE.        last_index_of  o mesmo com o indice n - 1 - volta.
 *   contains      bool; count  int (soma).     igualdade = IR_BINARIO == (str compara bytes, enum e inteiro por valor, real por IEEE).
 *   copy          m = min(len dst, len src); sobreposicao como memmove: se o endereco de dst <= o de src copia para frente, senao de tras para frente (enderecos comparados
 *                 como uint por IR_TRANSMUTA); devolve m.
 *   fill          laco gravando v.     swap  le os dois (R0006 em dev se i ou j fora) e grava trocado.     reverse  n / 2 trocas.
 *   replace_value laco: igual a old e quantidade restante > 0 grava new e desconta (sem n, a quantidade e o maior int).
 *   clone         IR_LISTA_CRIA com cap = n no alocador do argumento, IR_ANEXA_FATIA (so se n > 0) e a lista nova e o resultado.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - S2a: com str no 1o argumento, index_of, last_index_of, contains, count e reverse desviam para LwBuscaTexto (lower_texto_busca.c).
 *            0.2.0 - IA1-L9e: clone de str (resultado TIPO_STR) desvia para LwTextoCopia (lower_texto_bytes.c).
 *            0.1.0 - IA1-L9d: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_lista.h"

#define INT_VALOR(n) LwValorInt((n), TIPO_INT)

/* A fatia do argumento 0, a base dela e o len. */
typedef struct { LlBase base; Valor n; } Fatia;

static Fatia FatiaDoArgumento(const LoCtx *k, const Expr *arg)
{
    Valor fatia = LwExpr(k->c, arg);
    return (Fatia){LlBaseDe(k->c, fatia), LlTamanho(k, fatia)};
}

/* Laco 0..<n sobre a fatia: entra no corpo e devolve o contador. */
static Valor Percorre(const LoCtx *k, const Fatia *f, IrBloco **externo)
{
    return LlEntraLaco(k, INT_VALOR(0), f->n, externo);
}

/* index_of e last_index_of -> ?int: o resultado nasce none; o 1o igual (de frente ou de tras) o embrulha e PARE. */
static Valor LwIndexOf(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo, *fora;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Valor valor = LwExpr(c, LlArgumento(e, 1));
    int32_t resultado = LoLocal(&k, e->tipo, LwValorNenhum());
    Valor volta = Percorre(&k, &f, &externo);
    Valor posicao = e->ref_id == EMB_LAST_INDEX_OF ? LlMenos(&k, LlMenos(&k, f.n, INT_VALOR(1)), volta) : volta;
    fora = LoEntraSe(&k, LlIgual(&k, LlLe(&k, &f.base, posicao, true), valor));
    LoAtribui(&k, resultado, e->tipo, LlEmbrulha(&k, e->tipo, posicao));
    LlPare(&k);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    return LwValorLocal(resultado, e->tipo);
}

/* contains -> bool e count -> int. */
static Valor LwContains(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo, *fora;
    bool contagem = e->ref_id == EMB_COUNT;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Valor valor = LwExpr(c, LlArgumento(e, 1));
    int32_t resultado = LoLocal(&k, e->tipo, LwValorNenhum());
    Valor volta = Percorre(&k, &f, &externo);
    fora = LoEntraSe(&k, LlIgual(&k, LlLe(&k, &f.base, volta, true), valor));
    if (contagem) {
        LoAtribui(&k, resultado, TIPO_INT, LlMais(&k, LwValorLocal(resultado, TIPO_INT), INT_VALOR(1)));
    } else {
        LoAtribui(&k, resultado, TIPO_BOOL, LwValorBool(true));
        LlPare(&k);
    }
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    return LwValorLocal(resultado, e->tipo);
}

/* O endereco do elemento 0 como uint: o que decide o lado da copia. */
static Valor EnderecoInicial(const LoCtx *k, const LlBase *b)
{
    return LoTransmuta(k, TIPO_UINT, LlEnd(k, b, INT_VALOR(0), true));
}

/* Copia m elementos de src para dst: de frente (para = 0..m-1) ou de tras (volta = m-1-c). */
static void CopiaElementos(const LoCtx *k, const Fatia *dst, const Fatia *src, Valor m, bool de_tras)
{
    IrBloco *externo;
    Valor volta = LlEntraLaco(k, INT_VALOR(0), m, &externo);
    Valor i = de_tras ? LlMenos(k, LlMenos(k, m, INT_VALOR(1)), volta) : volta;
    LlGrava(k, LlEnd(k, &dst->base, i, true), dst->base.elemento, LlLe(k, &src->base, i, true));
    LlSaiDoLaco(k, externo);
}

/* copy(dst, src) -> int. */
static Valor LwCopy(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *fora;
    Fatia dst = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Fatia src = FatiaDoArgumento(&k, LlArgumento(e, 1));
    int32_t m = LoLocal(&k, TIPO_INT, dst.n);
    fora = LoEntraSe(&k, LoBinario(&k, OP_MENOR, TIPO_BOOL, src.n, dst.n));
    LoAtribui(&k, m, TIPO_INT, src.n);
    LoSaiDoSe(&k, fora);
    Valor quantos = LwValorLocal(m, TIPO_INT);
    fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR, TIPO_BOOL, quantos, INT_VALOR(0)));
    Valor para_frente = LoBinario(&k, OP_MENOR_IGUAL, TIPO_BOOL, EnderecoInicial(&k, &dst.base), EnderecoInicial(&k, &src.base));
    IrInstr *se = LwNovaInstr(c, IR_SE, e->pos_op);
    se->a = para_frente;
    IrBloco *dentro = LwTrocarBloco(c, &se->corpo);
    CopiaElementos(&k, &dst, &src, quantos, false);
    LwTrocarBloco(c, &se->senao);
    CopiaElementos(&k, &dst, &src, quantos, true);
    LwTrocarBloco(c, dentro);
    LoSaiDoSe(&k, fora);
    return quantos;
}

static Valor LwFill(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Valor valor = LwExpr(c, LlArgumento(e, 1));
    Valor volta = Percorre(&k, &f, &externo);
    LlGrava(&k, LlEnd(&k, &f.base, volta, true), f.base.elemento, valor);
    LlSaiDoLaco(&k, externo);
    return LwValorNenhum();
}

/* swap(s, i = a, j = b): as duas leituras conferem os indices (R0006 em dev); depois grava trocado. */
static Valor LwSwap(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Valor a = LlIndiceInt(&k, LlArgumento(e, 1));
    Valor b = LlIndiceInt(&k, LlArgumento(e, 2));
    Valor em_a = LlLe(&k, &f.base, a, false);
    Valor em_b = LlLe(&k, &f.base, b, false);
    LlGrava(&k, LlEnd(&k, &f.base, a, true), f.base.elemento, em_b);
    LlGrava(&k, LlEnd(&k, &f.base, b, true), f.base.elemento, em_a);
    return LwValorNenhum();
}

/* reverse(s): n / 2 trocas entre c e n - 1 - c. */
static Valor LwReverse(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Valor metade = LoBinario(&k, OP_DIV, TIPO_INT, f.n, INT_VALOR(2));
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), metade, &externo);
    Valor outro = LlMenos(&k, LlMenos(&k, f.n, INT_VALOR(1)), volta);
    Valor da_frente = LlLe(&k, &f.base, volta, true);
    Valor de_tras = LlLe(&k, &f.base, outro, true);
    LlGrava(&k, LlEnd(&k, &f.base, volta, true), f.base.elemento, de_tras);
    LlGrava(&k, LlEnd(&k, &f.base, outro, true), f.base.elemento, da_frente);
    LlSaiDoLaco(&k, externo);
    return LwValorNenhum();
}

/* replace_value(s, old = a, new = b, n = k): troca ate k ocorrencias (sem n, todas). */
static Valor LwReplaceValue(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo, *fora;
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Valor antigo = LwExpr(c, LlArgumento(e, 1));
    Valor novo = LwExpr(c, LlArgumento(e, 2));
    const Expr *quantas = LlArgumento(e, 3);
    int32_t resta = LoLocal(&k, TIPO_INT, quantas != NULL ? LlIndiceInt(&k, quantas) : INT_VALOR(INT64_MAX));
    Valor restantes = LwValorLocal(resta, TIPO_INT);
    Valor volta = Percorre(&k, &f, &externo);
    Valor igual = LlIgual(&k, LlLe(&k, &f.base, volta, true), antigo);
    fora = LoEntraSe(&k, LoBinario(&k, OP_E, TIPO_BOOL, igual, LoBinario(&k, OP_MAIOR, TIPO_BOOL, restantes, INT_VALOR(0))));
    LlGrava(&k, LlEnd(&k, &f.base, volta, true), f.base.elemento, novo);
    LoAtribui(&k, resta, TIPO_INT, LlMenos(&k, restantes, INT_VALOR(1)));
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    return LwValorNenhum();
}

/* clone(s, alloc = A) -> [dynamic]T: lista nova de capacidade n; os n elementos entram por IR_ANEXA_FATIA (copia rasa). */
static Valor LwClone(LowerCtx *c, const Expr *e)
{
    if (e->tipo == TIPO_STR) return LwTextoCopia(c, e); /* clone(str, alloc = A) (L9e) */
    LoCtx k = {c, e->pos_op};
    Fatia f = FatiaDoArgumento(&k, LlArgumento(e, 0));
    Valor alocador = LwAlocador(c, e);
    IrInstr *cria = LwNovaInstr(c, IR_LISTA_CRIA, e->pos_op);
    cria->destino = LwNovoTemp(c);
    cria->tipo = e->tipo;
    cria->a = alocador;
    cria->b = f.n;
    int32_t lista = LoLocal(&k, e->tipo, LwValorTemp(cria->destino, e->tipo));
    IrBloco *fora = LoEntraSe(&k, LoBinario(&k, OP_MAIOR, TIPO_BOOL, f.n, INT_VALOR(0)));
    IrInstr *anexa = LwNovaInstr(c, IR_ANEXA_FATIA, e->pos_op);
    anexa->tipo = f.base.elemento;
    anexa->a = LwValorLocal(lista, e->tipo);
    anexa->b = f.base.valor;
    LoSaiDoSe(&k, fora);
    return LwValorLocal(lista, e->tipo);
}

/* S2a: index_of, last_index_of, contains, count e reverse com str no 1o argumento (lower_texto_busca.c). O clone de str nao entra aqui (LwClone). */
Valor LwBuscaTexto(LowerCtx *c, const Expr *e);

static bool EhBuscaDeTexto(const Expr *e)
{
    const Expr *alvo = LlArgumento(e, 0);
    switch (e->ref_id) {
    case EMB_INDEX_OF:
    case EMB_LAST_INDEX_OF:
    case EMB_CONTAINS:
    case EMB_COUNT:
    case EMB_REVERSE: return alvo != NULL && alvo->tipo == TIPO_STR;
    default: return false;
    }
}

Valor LwBusca(LowerCtx *c, const Expr *e)
{
    if (EhBuscaDeTexto(e)) return LwBuscaTexto(c, e);
    switch (e->ref_id) {
    case EMB_INDEX_OF:
    case EMB_LAST_INDEX_OF: return LwIndexOf(c, e);
    case EMB_CONTAINS:
    case EMB_COUNT: return LwContains(c, e);
    case EMB_COPY: return LwCopy(c, e);
    case EMB_FILL: return LwFill(c, e);
    case EMB_CLONE: return LwClone(c, e);
    case EMB_SWAP: return LwSwap(c, e);
    case EMB_REVERSE: return LwReverse(c, e);
    case EMB_REPLACE_VALUE: return LwReplaceValue(c, e);
    default: FalhaInterna("lowering: embutido de busca desconhecido (offset %u)", (unsigned)e->pos_op);
    }
}
