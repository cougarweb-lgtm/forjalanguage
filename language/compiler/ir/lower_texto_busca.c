/* lower_texto_busca.c - lowering dos embutidos de busca e reverse com str no 1o argumento (S2a): index_of, last_index_of, contains, count e reverse(str, alloc = A).
 * Sem IrOp nova: IR_TRANSMUTA (str -> []u8, mesmo tamanho), IR_PARA, IR_SE, IR_ELEMENTO e IR_ENDERECO_ELEMENTO + IR_GRAVA_PONTEIRO, IR_LISTA_CRIA, IR_REDIMENSIONA
 * (REDIM_RESIZE e REDIM_PARA_STR, o mesmo do to_str). Os 5 embutidos de lista (lower_busca.c) so cuidam de fatia; aqui o str vira []u8 com os bytes de sempre.
 *   Busca (bytes, igual a text.str_*): para i em 0..<(n - m + 1), dentro de m <= n. Compara os m bytes com PARE na 1a diferenca. Indices provados (i + j < n).
 *     Agulha vazia (m = 0) sai sozinha: o 1o i casa e os resultados batem com a convencao (contains true, index_of 0, last_index_of n, count n + 1).
 *     count so conta do proximo_livre em diante (nao sobreposicao): "aaaa"/"aa" = 2.
 *   reverse(str): cada runa (tamanho pela regra do decode de utf8.fj) vai para o lugar espelhado: a que ocupa [j, j+tam) grava em [n - j - tam, n - j).
 *     Byte invalido = runa de 1 byte, preservado, igual a utf8.str_reverse. Os bytes entram na lista do alocador (cap n) e o REDIM_PARA_STR os move para a str dona.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - S2a: primeira versao (LwBuscaTexto, desviado por LwBusca em lower_busca.c 0.3.0).
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py --forja compiler\forja.exe
 */
#include "ir/lower_lista.h"

#define INT_VALOR(n) LwValorInt((n), TIPO_INT)
#define BYTE_VALOR(n) LwValorInt((n), TIPO_U8)

/* Bytes de UTF-8 usados no decode de utf8.fj (decode, ler_sequencia): os limites de cada faixa. */
enum {
    BYTE_C2 = 0xC2, BYTE_DF = 0xDF,
    BYTE_E0 = 0xE0, BYTE_ED = 0xED, BYTE_EF = 0xEF,
    BYTE_F0 = 0xF0, BYTE_F4 = 0xF4,
    BYTE_CONT_MIN = 0x80, BYTE_CONT_MAX = 0xBF,
    BYTE_A0 = 0xA0, BYTE_9F = 0x9F, BYTE_90 = 0x90, BYTE_8F = 0x8F
};

/* O str como bytes ([]u8, avaliado uma vez) e o len dele. */
typedef struct { LlBase base; Valor n; } Texto;
/* reverse: os bytes de origem e a lista de destino ([dynamic]u8, ja com len n). */
typedef struct { Texto s; LlBase saida; } Reverso;

/* O argumento str como []u8 (IR_TRANSMUTA: str e fatia tem o mesmo tamanho) e o len em bytes. */
static Texto TextoDoArgumento(const LoCtx *k, const Expr *arg)
{
    Valor bytes = LoTransmuta(k, TipoFatia(&k->c->sem->tipos, TIPO_U8), LwExpr(k->c, arg));
    return (Texto){(LlBase){bytes, TIPO_U8, false}, LlTamanho(k, bytes)};
}

/* Os m bytes de sub batem com s a partir de i? Laco interno com PARE na 1a diferenca; devolve o local 'iguais'. */
static int32_t BatemEm(const LoCtx *k, const Texto *s, const Texto *sub, Valor i)
{
    int32_t iguais = LoLocal(k, TIPO_BOOL, LwValorBool(true));
    IrBloco *externo;
    Valor j = LlEntraLaco(k, INT_VALOR(0), sub->n, &externo);
    Valor do_texto = LlLe(k, &s->base, LlMais(k, i, j), true);
    Valor da_agulha = LlLe(k, &sub->base, j, true);
    IrBloco *fora = LoEntraSe(k, LoBinario(k, OP_DIFERENTE, TIPO_BOOL, do_texto, da_agulha));
    LoAtribui(k, iguais, TIPO_BOOL, LwValorBool(false));
    LlPare(k);
    LoSaiDoSe(k, fora);
    LlSaiDoLaco(k, externo);
    return iguais;
}

/* index_of, last_index_of e contains: o 1o i que casa (de frente, ou de tras no last_index_of) grava o resultado e PARE. */
static Valor LwTextoAcha(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Texto s = TextoDoArgumento(&k, LlArgumento(e, 0));
    Texto sub = TextoDoArgumento(&k, LlArgumento(e, 1));
    int32_t resultado = LoLocal(&k, e->tipo, LwValorNenhum());
    IrBloco *fora_se = LoEntraSe(&k, LoBinario(&k, OP_MENOR_IGUAL, TIPO_BOOL, sub.n, s.n));
    IrBloco *externo;
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), LlMais(&k, LlMenos(&k, s.n, sub.n), INT_VALOR(1)), &externo);
    Valor posicao = e->ref_id == EMB_LAST_INDEX_OF ? LlMenos(&k, LlMenos(&k, s.n, sub.n), volta) : volta;
    int32_t iguais = BatemEm(&k, &s, &sub, posicao);
    IrBloco *fora = LoEntraSe(&k, LwValorLocal(iguais, TIPO_BOOL));
    if (e->ref_id == EMB_CONTAINS) LoAtribui(&k, resultado, TIPO_BOOL, LwValorBool(true));
    else LoAtribui(&k, resultado, e->tipo, LlEmbrulha(&k, e->tipo, posicao));
    LlPare(&k);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    LoSaiDoSe(&k, fora_se);
    return LwValorLocal(resultado, e->tipo);
}

/* count: conta os casamentos que nao se sobrepoem (i >= proximo_livre); o proximo livre e i + m. */
static Valor LwTextoConta(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Texto s = TextoDoArgumento(&k, LlArgumento(e, 0));
    Texto sub = TextoDoArgumento(&k, LlArgumento(e, 1));
    int32_t total = LoLocal(&k, TIPO_INT, LwValorNenhum());
    int32_t proximo = LoLocal(&k, TIPO_INT, LwValorNenhum());
    IrBloco *fora_se = LoEntraSe(&k, LoBinario(&k, OP_MENOR_IGUAL, TIPO_BOOL, sub.n, s.n));
    IrBloco *externo;
    Valor i = LlEntraLaco(&k, INT_VALOR(0), LlMais(&k, LlMenos(&k, s.n, sub.n), INT_VALOR(1)), &externo);
    int32_t iguais = BatemEm(&k, &s, &sub, i);
    IrBloco *fora = LoEntraSe(&k, LwValorLocal(iguais, TIPO_BOOL));
    IrBloco *livre = LoEntraSe(&k, LoBinario(&k, OP_MAIOR_IGUAL, TIPO_BOOL, i, LwValorLocal(proximo, TIPO_INT)));
    LoAtribui(&k, total, TIPO_INT, LlMais(&k, LwValorLocal(total, TIPO_INT), INT_VALOR(1)));
    LoAtribui(&k, proximo, TIPO_INT, LlMais(&k, i, sub.n));
    LoSaiDoSe(&k, livre);
    LoSaiDoSe(&k, fora);
    LlSaiDoLaco(&k, externo);
    LoSaiDoSe(&k, fora_se);
    return LwValorLocal(total, TIPO_INT);
}

/* Byte de s no deslocamento d a partir de j: so chamado dentro do IR_SE que garante j + d < n (indice provado). */
static Valor ByteEm(const LoCtx *k, const Texto *s, Valor j, int64_t d)
{
    return LlLe(k, &s->base, LlMais(k, j, INT_VALOR(d)), true);
}

/* Faixa fechada: lo <= b <= hi (bytes). */
static Valor Entre(const LoCtx *k, Valor b, int64_t lo, int64_t hi)
{
    Valor acima = LoBinario(k, OP_MAIOR_IGUAL, TIPO_BOOL, b, BYTE_VALOR(lo));
    Valor abaixo = LoBinario(k, OP_MENOR_IGUAL, TIPO_BOOL, b, BYTE_VALOR(hi));
    return LoBinario(k, OP_E, TIPO_BOOL, acima, abaixo);
}

/* Byte de continuacao (10xxxxxx). */
static Valor Continua(const LoCtx *k, Valor b)
{
    return Entre(k, b, BYTE_CONT_MIN, BYTE_CONT_MAX);
}

/* b != v (bytes). */
static Valor DiferenteDe(const LoCtx *k, Valor b, int64_t v)
{
    return LoBinario(k, OP_DIFERENTE, TIPO_BOOL, b, BYTE_VALOR(v));
}

/* 2 bytes: C2..DF + continuacao; so com j + 1 < n. */
static void TamanhoDe2(const LoCtx *k, const Texto *s, Valor j, int32_t tam)
{
    Valor b0 = ByteEm(k, s, j, 0);
    IrBloco *fora = LoEntraSe(k, Entre(k, b0, BYTE_C2, BYTE_DF));
    IrBloco *cabe = LoEntraSe(k, LoBinario(k, OP_MENOR, TIPO_BOOL, LlMais(k, j, INT_VALOR(1)), s->n));
    IrBloco *ok = LoEntraSe(k, Continua(k, ByteEm(k, s, j, 1)));
    LoAtribui(k, tam, TIPO_INT, INT_VALOR(2));
    LoSaiDoSe(k, ok);
    LoSaiDoSe(k, cabe);
    LoSaiDoSe(k, fora);
}

/* Segundo byte de 3 bytes: faixa 80..BF; A0..BF se E0 (sem overlong); 80..9F se ED (sem surrogate). */
static Valor SegundoDe3(const LoCtx *k, Valor b0, Valor b1)
{
    Valor sem_overlong = LoBinario(k, OP_OU, TIPO_BOOL, DiferenteDe(k, b0, BYTE_E0), LoBinario(k, OP_MAIOR_IGUAL, TIPO_BOOL, b1, BYTE_VALOR(BYTE_A0)));
    Valor sem_surrogate = LoBinario(k, OP_OU, TIPO_BOOL, DiferenteDe(k, b0, BYTE_ED), LoBinario(k, OP_MENOR_IGUAL, TIPO_BOOL, b1, BYTE_VALOR(BYTE_9F)));
    return LoBinario(k, OP_E, TIPO_BOOL, Continua(k, b1), LoBinario(k, OP_E, TIPO_BOOL, sem_overlong, sem_surrogate));
}

/* 3 bytes: E0..EF; o 2o pela faixa de SegundoDe3 e o 3o continuacao; so com j + 2 < n. */
static void TamanhoDe3(const LoCtx *k, const Texto *s, Valor j, int32_t tam)
{
    Valor b0 = ByteEm(k, s, j, 0);
    IrBloco *fora = LoEntraSe(k, Entre(k, b0, BYTE_E0, BYTE_EF));
    IrBloco *cabe = LoEntraSe(k, LoBinario(k, OP_MENOR, TIPO_BOOL, LlMais(k, j, INT_VALOR(2)), s->n));
    Valor b1 = ByteEm(k, s, j, 1);
    Valor b2 = ByteEm(k, s, j, 2);
    IrBloco *ok = LoEntraSe(k, LoBinario(k, OP_E, TIPO_BOOL, SegundoDe3(k, b0, b1), Continua(k, b2)));
    LoAtribui(k, tam, TIPO_INT, INT_VALOR(3));
    LoSaiDoSe(k, ok);
    LoSaiDoSe(k, cabe);
    LoSaiDoSe(k, fora);
}

/* Segundo byte de 4 bytes: 80..BF; 90..BF se F0 (sem overlong); 80..8F se F4 (ate U+10FFFF). */
static Valor SegundoDe4(const LoCtx *k, Valor b0, Valor b1)
{
    Valor sem_overlong = LoBinario(k, OP_OU, TIPO_BOOL, DiferenteDe(k, b0, BYTE_F0), LoBinario(k, OP_MAIOR_IGUAL, TIPO_BOOL, b1, BYTE_VALOR(BYTE_90)));
    Valor ate_fim = LoBinario(k, OP_OU, TIPO_BOOL, DiferenteDe(k, b0, BYTE_F4), LoBinario(k, OP_MENOR_IGUAL, TIPO_BOOL, b1, BYTE_VALOR(BYTE_8F)));
    return LoBinario(k, OP_E, TIPO_BOOL, Continua(k, b1), LoBinario(k, OP_E, TIPO_BOOL, sem_overlong, ate_fim));
}

/* 4 bytes: F0..F4; o 2o pela faixa de SegundoDe4 e o 3o e o 4o continuacao; so com j + 3 < n. */
static void TamanhoDe4(const LoCtx *k, const Texto *s, Valor j, int32_t tam)
{
    Valor b0 = ByteEm(k, s, j, 0);
    IrBloco *fora = LoEntraSe(k, Entre(k, b0, BYTE_F0, BYTE_F4));
    IrBloco *cabe = LoEntraSe(k, LoBinario(k, OP_MENOR, TIPO_BOOL, LlMais(k, j, INT_VALOR(3)), s->n));
    Valor b1 = ByteEm(k, s, j, 1);
    Valor b2 = ByteEm(k, s, j, 2);
    Valor b3 = ByteEm(k, s, j, 3);
    Valor resto = LoBinario(k, OP_E, TIPO_BOOL, Continua(k, b2), Continua(k, b3));
    IrBloco *ok = LoEntraSe(k, LoBinario(k, OP_E, TIPO_BOOL, SegundoDe4(k, b0, b1), resto));
    LoAtribui(k, tam, TIPO_INT, INT_VALOR(4));
    LoSaiDoSe(k, ok);
    LoSaiDoSe(k, cabe);
    LoSaiDoSe(k, fora);
}

/* Tamanho em bytes da runa que comeca em j (regra de utf8.fj): 2, 3 ou 4 quando a sequencia e valida; senao 1 (o byte invalido sai sozinho). */
static int32_t TamanhoRuna(const LoCtx *k, const Texto *s, Valor j)
{
    int32_t tam = LoLocal(k, TIPO_INT, INT_VALOR(1));
    TamanhoDe2(k, s, j, tam);
    TamanhoDe3(k, s, j, tam);
    TamanhoDe4(k, s, j, tam);
    return tam;
}

/* Copia os tam bytes da runa de j para o lugar espelhado: [j, j + tam) vai para [n - j - tam, n - j), na ordem dos bytes. */
static void EscreveRuna(const LoCtx *k, const Reverso *r, Valor j, Valor tam)
{
    IrBloco *externo;
    Valor t = LlEntraLaco(k, INT_VALOR(0), tam, &externo);
    Valor destino = LlMais(k, LlMenos(k, LlMenos(k, r->s.n, j), tam), t);
    LlGrava(k, LlEnd(k, &r->saida, destino, false), TIPO_U8, LlLe(k, &r->s.base, LlMais(k, j, t), true));
    LlSaiDoLaco(k, externo);
}

/* to_str sobre a lista local: REDIM_PARA_STR move os bytes para a str dona (sem copia) e esvazia a lista; o mesmo que LwToStr (lower_texto_builder.c). */
static Valor LwListaParaTexto(LowerCtx *c, const Expr *e, Valor lista)
{
    IrInstr *i = LwNovaInstr(c, IR_REDIMENSIONA, e->pos_op);
    i->op_aux = REDIM_PARA_STR;
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_STR;
    i->a = lista;
    return LwValorTemp(i->destino, TIPO_STR);
}

/* reverse(str, alloc = A) -> str: lista do alocador com len n; cada runa e espelhada; o texto sai pelo REDIM_PARA_STR. O corpo e o do reverse de utf8.str_reverse. */
static Valor LwReverseTexto(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Texto s = TextoDoArgumento(&k, LlArgumento(e, 0));
    TipoId lista_u8 = TipoDinamico(&c->sem->tipos, TIPO_U8);
    Valor alocador = LwAlocador(c, e);
    IrInstr *cria = LwNovaInstr(c, IR_LISTA_CRIA, e->pos_op);
    cria->destino = LwNovoTemp(c);
    cria->tipo = lista_u8;
    cria->a = alocador;
    cria->b = s.n;
    int32_t saida = LoLocal(&k, lista_u8, LwValorTemp(cria->destino, lista_u8));
    Valor valor_saida = LwValorLocal(saida, lista_u8);
    LlRedimensiona(&k, valor_saida, TIPO_U8, s.n);
    Reverso r = {s, (LlBase){valor_saida, TIPO_U8, true}};
    int32_t pular_ate = LoLocal(&k, TIPO_INT, LwValorNenhum());
    IrBloco *externo;
    Valor j = LlEntraLaco(&k, INT_VALOR(0), s.n, &externo);
    IrBloco *inicio = LoEntraSe(&k, LoBinario(&k, OP_MAIOR_IGUAL, TIPO_BOOL, j, LwValorLocal(pular_ate, TIPO_INT)));
    int32_t tam = TamanhoRuna(&k, &s, j);
    Valor tamanho = LwValorLocal(tam, TIPO_INT);
    EscreveRuna(&k, &r, j, tamanho);
    LoAtribui(&k, pular_ate, TIPO_INT, LlMais(&k, j, tamanho));
    LoSaiDoSe(&k, inicio);
    LlSaiDoLaco(&k, externo);
    return LwListaParaTexto(c, e, valor_saida);
}

Valor LwBuscaTexto(LowerCtx *c, const Expr *e)
{
    switch (e->ref_id) {
    case EMB_INDEX_OF:
    case EMB_LAST_INDEX_OF:
    case EMB_CONTAINS: return LwTextoAcha(c, e);
    case EMB_COUNT: return LwTextoConta(c, e);
    case EMB_REVERSE: return LwReverseTexto(c, e);
    default: FalhaInterna("lowering: embutido de texto desconhecido (offset %u)", (unsigned)e->pos_op);
    }
}
