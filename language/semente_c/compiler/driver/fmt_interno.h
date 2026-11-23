/* fmt_interno.h - tipos e funcoes compartilhados entre fmt_nucleo.c, fmt_espaco.c e fmt_indent.c (o forja fmt). Nao e API: quem chama o motor usa fmt_nucleo.h.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - F1 P1a: primeira versao (FmtToken, FmtPapel, tabela de pares, niveis de indentacao).
 * Verificacao: construir.bat && python tests\fmt\verificar_fmt.py --forja compiler\forja.exe
 */
#ifndef FORJA_DRIVER_FMT_INTERNO_H
#define FORJA_DRIVER_FMT_INTERNO_H

#include "common/vec.h"
#include "frontend/token.h"

/* O papel de um token no espacamento. A ordem e a das linhas e colunas da tabela de pares (fmt_espaco.c). */
typedef enum {
    PAPEL_W, PAPEL_K, PAPEL_PR,                         /* palavra/literal (operando), palavra-chave, 'proc' */
    PAPEL_LP, PAPEL_RP, PAPEL_LB, PAPEL_RB,             /* ( ) [ ] */
    PAPEL_LK, PAPEL_LL, PAPEL_RK, PAPEL_RL,             /* '{' e '}' de bloco/declaracao (K) e de literal (L) */
    PAPEL_CM, PAPEL_CO, PAPEL_DT, PAPEL_RG,             /* , : . e .. ..< ..= */
    PAPEL_BI, PAPEL_UN, PAPEL_PO,                       /* binario (1 espaco em volta), unario prefixo, '^' pos-fixo */
    PAPEL_QTD
} FmtPapel;

/* Um token logico (f"..." inteiro e UM token): bytes [ini, fim) do texto; primeiro = abre a linha; nivel = indentacao (em niveis) se for o 1o da linha. */
typedef struct { uint32_t ini, fim, nivel; uint8_t tipo, papel; bool primeiro; } FmtToken;

/* Quantos espacos entre dois tokens da mesma linha: 0, 1 ou -1 (par fora da tabela: quem chama mantem o vao original). */
int FmtEspacoDoPar(FmtPapel antes, FmtPapel depois);

/* Preenche papel de cada token (primeiro ja preenchido). chaves_literal: offsets das '{' de literal, em ordem crescente (Vec de uint32_t). */
void FmtAtribuirPapeis(FmtToken *t, size_t n, const Vec *chaves_literal);

/* Preenche nivel de cada token (so vale para o 1o da linha). */
void FmtCalcularNiveis(FmtToken *t, size_t n);

/* O papel e de fechamento ( ) ] }? */
bool FmtPapelFecha(FmtPapel p);

#endif
