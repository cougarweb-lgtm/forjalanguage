/* x64_div_magica.h - planejador puro de divisao por constante (magic number) do backend x64: escolhe a sequencia de operacoes e a emula em 64 bits, sem emitir nada.
 * Versao: 1.1.0
 * Changelog: 1.1.0 - DivMagicaEmular reproduz tambem as formas de resto com sinal do emissor (SINAL_MUL, SINAL_64, POT2_SINAL), sem dividir nem negar o produto.
 *            1.0.0 - DivMagicaPlanejar (u8..u64 e i8..i64) e DivMagicaEmular (mesma sequencia que o emissor fara, sem __int128).
 * Verificacao: gcc -std=c11 -O2 -Wall -Wextra -Werror -I compiler tests\unidade\teste_x64_div_magica.c compiler\nativo\x64\x64_div_magica.c (ver o cabecalho de teste_x64_div_magica.c).
 * Invariante: quociente e resto saem zero-estendidos a N bits; uma divisao com sinal de N < 64 bits deve ser truncada/zero-estendida ao final pelo emissor.
 */
#ifndef X64_DIV_MAGICA_H
#define X64_DIV_MAGICA_H

#include <stdbool.h>
#include <stdint.h>

/* Estrategia do plano. a = dividendo zero-estendido; n = a estendido com sinal a 64 bits; M = magic; s = shift.
 *   NAO_OTIMIZAR    : sem plano (d = 0, ou d = -1 com sinal: o lowering trata). DivMagicaPlanejar devolve false.
 *   UM              : q = a (d = 1).
 *   POT2            : sem sinal, d = 2^s: q = a shr s.
 *   POT2_SINAL      : com sinal, |d| = 2^s: t = (n sar 63) shr (64 - s); q = (n + t) sar s; negar se d < 0.
 *   LEMIRE          : sem sinal, N <= 32, d nao potencia de 2: q = hi64(a * M), M = floor((2^64 - 1) / d) + 1.
 *   SEM_SINAL_64    : sem sinal u64, d nao potencia de 2: M = 2^64 + magic (65 bits); t = hi64(a * magic);
 *                     q = (((a - t) shr 1) + t) shr (s - 1).
 *   MAIOR_SEM_SINAL : sem sinal u64, d >= 2^63 (nao potencia de 2): q = (a >= d).
 *   SINAL_MUL       : com sinal, N <= 32, |d| nao potencia de 2: p = n * M (imul de 32 bits; 64 se produto_64),
 *                     q = (p sar s) + (n < 0); negar se d < 0.
 *   SINAL_64        : com sinal u64, |d| nao potencia de 2: t = hi64_sinal(n, M) (+ n se soma_a); q = (t sar s) + (n < 0); negar se d < 0.
 *   SINAL_MIN       : com sinal, d = MIN: q = (a == MIN).
 */
typedef enum {
    DIV_MAGICA_NAO_OTIMIZAR,
    DIV_MAGICA_UM,
    DIV_MAGICA_POT2,
    DIV_MAGICA_POT2_SINAL,
    DIV_MAGICA_LEMIRE,
    DIV_MAGICA_SEM_SINAL_64,
    DIV_MAGICA_MAIOR_SEM_SINAL,
    DIV_MAGICA_SINAL_MUL,
    DIV_MAGICA_SINAL_64,
    DIV_MAGICA_SINAL_MIN
} DivMagicaEstrategia;

/* Plano de uma divisao por constante. Campos que a estrategia nao usa ficam zerados. */
typedef struct {
    DivMagicaEstrategia estrategia;
    uint32_t bits;          /* largura N: 8, 16, 32 ou 64 */
    bool com_sinal;
    uint64_t divisor;       /* d nos N bits baixos (com sinal, complemento de dois) */
    uint64_t magic;         /* M (LEMIRE, SINAL_MUL, SINAL_64) ou os 64 bits baixos de M (SEM_SINAL_64) */
    uint32_t shift;         /* s: deslocamento final; em SEM_SINAL_64 e SINAL_64 e o shift depois do hi64 */
    bool produto_64;        /* SINAL_MUL com N = 32: imul de 64 bits (operandos ja estendidos) */
    bool soma_a;            /* SINAL_64: M como int64 e negativo, entao soma-se n ao hi64 com sinal */
    bool soma_bit_sinal;    /* SINAL_MUL e SINAL_64: +1 quando o dividendo e negativo (arredonda para zero) */
    bool negar;             /* com sinal: d < 0, o quociente sai negado */
} DivMagicaPlano;

/* Planeja a divisao de um dividendo de `bits` (8, 16, 32 ou 64) por `divisor` (N bits baixos).
 * Entrada: com_sinal diz se e divisao com sinal. Saida: plano preenchido; retorna false (estrategia NAO_OTIMIZAR) so para d = 0 ou d = -1 com sinal, ou bits invalido. */
bool DivMagicaPlanejar(uint32_t bits, bool com_sinal, uint64_t divisor, DivMagicaPlano *saida);

/* Emula a sequencia do plano sobre o dividendo `a` (N bits, zero-estendido). Devolve o quociente (N bits, zero-estendido);
 * se resto != NULL, grava nele o resto (a - q * d, N bits). Nao faz E/S nem altera o plano. */
uint64_t DivMagicaEmular(const DivMagicaPlano *plano, uint64_t a, uint64_t *resto);

#endif
