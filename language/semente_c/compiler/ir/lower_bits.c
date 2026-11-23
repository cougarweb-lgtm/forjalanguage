/* lower_bits.c - lowering de rotate_left, rotate_right, count_ones, leading_zeros, trailing_zeros e byte_swap (IA1-L9b). Sem IrOp nova:
 * tudo na versao sem sinal do tipo (IR_TRANSMUTA de ida e volta nos com sinal) com IR_BINARIO/IR_UNARIO que ja existem.
 *   rotate_left(x, k) = (x << k) | (x >> (N - k)), com k = n & (N - 1); deslocar >= N da 0 (regra do IR), entao k = 0 nao precisa de caso.
 *   count_ones = popcount por SWAR (pares, nibbles, bytes e a soma dos bytes por um produto com volta, OP_MUL_VOLTA).
 *   leading_zeros = count_ones(~y), y = x com todos os bits abaixo do mais alto ligados.  trailing_zeros = count_ones(~x & (x - 1)), x - 1 com volta (0 da N).
 *   byte_swap = cada byte deslocado para a posicao espelhada, juntados por OU.
 * count_ones, leading_zeros e trailing_zeros devolvem int (a contagem de 0 a 64 e convertida do sem sinal).
 * Interface (lower_ops.h): LwBits (chamado por LwAritmetica para EMB_ROTATE_LEFT..EMB_BYTE_SWAP).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9b: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_ops.h"

enum { BITS_POR_BYTE = 8, PADRAO_PARES = 0x55, PADRAO_QUARTETOS = 0x33, PADRAO_NIBBLE_BAIXO = 0x0F, PADRAO_BYTE = 0xFF, PADRAO_UM = 0x01 };

/* O byte 'padrao' repetido por toda a largura de 'tipo' (0x55 -> 0x5555...). */
static Valor Repetido(const LoCtx *k, TipoId tipo, uint64_t padrao)
{
    uint64_t bits = 0;
    for (uint32_t b = 0; b < LoLargura(k, tipo) / BITS_POR_BYTE; b++) bits |= padrao << (BITS_POR_BYTE * b);
    return LwValorInt((int64_t)bits, tipo);
}

static Valor Deslocar(const LoCtx *k, uint8_t sentido, Valor x, uint32_t quanto)
{
    return LoBinario(k, sentido, x.tipo, x, LwValorInt(quanto, x.tipo));
}

static Valor Operar(const LoCtx *k, uint8_t operador, Valor a, Valor b)
{
    return LoBinario(k, operador, a.tipo, a, b);
}

/* popcount de x (sem sinal) na propria largura; o resultado tem o mesmo tipo de x. */
static Valor Unidades(const LoCtx *k, Valor x)
{
    TipoId t = x.tipo;
    Valor pares = Operar(k, OP_SUB, x, Operar(k, OP_BIT_E, Deslocar(k, OP_DESLOCA_DIR, x, 1), Repetido(k, t, PADRAO_PARES)));
    Valor q = Repetido(k, t, PADRAO_QUARTETOS);
    Valor quartetos = Operar(k, OP_SOMA, Operar(k, OP_BIT_E, pares, q), Operar(k, OP_BIT_E, Deslocar(k, OP_DESLOCA_DIR, pares, 2), q));
    Valor bytes = Operar(k, OP_BIT_E, Operar(k, OP_SOMA, quartetos, Deslocar(k, OP_DESLOCA_DIR, quartetos, 4)),
                         Repetido(k, t, PADRAO_NIBBLE_BAIXO));
    uint32_t largura = LoLargura(k, t);
    if (largura == BITS_POR_BYTE) return bytes;
    return Deslocar(k, OP_DESLOCA_DIR, Operar(k, OP_MUL_VOLTA, bytes, Repetido(k, t, PADRAO_UM)), largura - BITS_POR_BYTE);
}

/* Liga todos os bits abaixo do mais alto ligado de x. */
static Valor Espalhar(const LoCtx *k, Valor x)
{
    for (uint32_t passo = 1; passo < LoLargura(k, x.tipo); passo *= 2) x = Operar(k, OP_BIT_OU, x, Deslocar(k, OP_DESLOCA_DIR, x, passo));
    return x;
}

static Valor ZerosNaFrente(const LoCtx *k, Valor x)
{
    return Unidades(k, LoBitNao(k, Espalhar(k, x)));
}

static Valor ZerosAtras(const LoCtx *k, Valor x)
{
    Valor menos_um = Operar(k, OP_SUB_VOLTA, x, LwValorInt(1, x.tipo));
    return Unidades(k, Operar(k, OP_BIT_E, LoBitNao(k, x), menos_um));
}

static Valor TrocarBytes(const LoCtx *k, Valor x)
{
    uint32_t bytes = LoLargura(k, x.tipo) / BITS_POR_BYTE;
    Valor trocado = LwValorNenhum();
    for (uint32_t b = 0; b < bytes; b++) {
        Valor byte = Operar(k, OP_BIT_E, Deslocar(k, OP_DESLOCA_DIR, x, BITS_POR_BYTE * b), LwValorInt(PADRAO_BYTE, x.tipo));
        Valor posto = Deslocar(k, OP_DESLOCA_ESQ, byte, BITS_POR_BYTE * (bytes - 1 - b));
        trocado = b == 0 ? posto : Operar(k, OP_BIT_OU, trocado, posto);
    }
    return trocado;
}

/* k = n & (N - 1) no tipo de x; constante: calculado aqui (sem instrucao). */
static Valor ContagemDaRotacao(const LoCtx *k, TipoId sem_sinal, Valor n)
{
    uint64_t volta = LoLargura(k, sem_sinal) - 1;
    if (n.tipo_no == V_INT) return LwValorInt((int64_t)((uint64_t)n.inteiro & volta), sem_sinal);
    TipoId tipo_n = LoSemSinal(n.tipo);
    Valor mascarado = LoBinario(k, OP_BIT_E, tipo_n, LoTransmuta(k, tipo_n, n), LwValorInt((int64_t)volta, tipo_n));
    return LoConverte(k, sem_sinal, mascarado);
}

static Valor Rotacionar(const LoCtx *k, bool esquerda, Valor x, Valor n)
{
    TipoId tipo = x.tipo, sem_sinal = LoSemSinal(tipo);
    Valor bits = LoTransmuta(k, sem_sinal, x);
    Valor passos = ContagemDaRotacao(k, sem_sinal, n);
    Valor largura = LwValorInt(LoLargura(k, sem_sinal), sem_sinal);
    Valor resto = passos.tipo_no == V_INT ? LwValorInt(largura.inteiro - passos.inteiro, sem_sinal) : Operar(k, OP_SUB, largura, passos);
    Valor um_lado = Operar(k, esquerda ? OP_DESLOCA_ESQ : OP_DESLOCA_DIR, bits, passos);
    Valor outro_lado = Operar(k, esquerda ? OP_DESLOCA_DIR : OP_DESLOCA_ESQ, bits, resto);
    return LoTransmuta(k, tipo, Operar(k, OP_BIT_OU, um_lado, outro_lado));
}

Valor LwBits(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    Valor op[2] = {{0}};
    for (uint32_t i = 0; i < e->nargs; i++) op[e->args[i].indice] = LwExpr(c, e->args[i].valor);
    op[0] = LoEmLocal(&k, op[0]); /* o dado nunca e constante solta: as contas abaixo partem de um registrador */
    if (e->ref_id == EMB_ROTATE_LEFT || e->ref_id == EMB_ROTATE_RIGHT) return Rotacionar(&k, e->ref_id == EMB_ROTATE_LEFT, op[0], op[1]);
    TipoId tipo = op[0].tipo, sem_sinal = LoSemSinal(tipo);
    Valor bits = LoTransmuta(&k, sem_sinal, op[0]);
    if (e->ref_id == EMB_BYTE_SWAP) return LoTransmuta(&k, tipo, TrocarBytes(&k, bits));
    Valor contagem = e->ref_id == EMB_COUNT_ONES ? Unidades(&k, bits) : e->ref_id == EMB_LEADING_ZEROS ? ZerosNaFrente(&k, bits) : ZerosAtras(&k, bits);
    return LoConverte(&k, TIPO_INT, contagem);
}
