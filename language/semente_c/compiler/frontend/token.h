/* token.h - tokens da Forja e nomes pre-internados (palavras-chave e nomes embutidos).
 * Versao: 0.12.0
 * Changelog: 0.12.0 - B12: TK_HASH ('#', so em #load("arquivo"); antes de TK_QTD, nenhum TK_* antigo muda de id).
 *            0.11.0 - B01a: SYM_ALLOCATOR (no fim; nenhum SYM_* antigo muda de id).
 *            0.10.0 - Linguagem v1.11 L14a: SYM_QUAT (no fim; nenhum SYM_* antigo muda de id).
 *            0.9.0 - Linguagem v1.11 L9: SYM_SIZE_OF, SYM_ALIGN_OF, SYM_OFFSET_OF, SYM_STATIC_ASSERT (no fim; nenhum SYM_* antigo muda de id).
 *            0.8.0 - Linguagem v1.9 P2: SYM_MAP, SYM_BIT_SET, SYM_SOA, SYM_MATRIX (palavras contextuais de tipo).
 *            0.7.0 - Linguagem v1.7 P1: TK_FTEXTO_INICIO, TK_FTEXTO_PEDACO, TK_FTEXTO_CHAVE, TK_FTEXTO_FORMATO, TK_FTEXTO_FIM_CHAVE, TK_FTEXTO_FIM (f"...", D16).
 *            0.6.0 - Linguagem v1.6 P1a: SYM_NEW, SYM_MAKE, SYM_DYNAMIC.
 *            0.5.0 - Linguagem v1.5 P1a: TK_LCOLCHETE e TK_RCOLCHETE.
 *            0.4.0 - Linguagem v1.4 P1: SYM_LINK (nome contextual do bloco 'foreign').
 *            0.3.0 - Linguagem v1.3: TK_INTERROGA ('?') e o sentinela TK_QTD (tamanho das tabelas por token); nomes de tipo str, cstr, rawptr (SYM_*).
 *            0.2.0 - Linguagem v1.2: nomes de tipo i8 i16 i64 u8 u16 u32 u64 uint f64 (SYM_*); tokens TK_PIPE, TK_TIL, TK_SHL, TK_SHR.
 *            0.1.0 - Fase 5b: todas as palavras-chave da spec sao reservadas; TK_BANG ('!') entra para 'T ! E'.
 * Verificacao: construir.bat && forja.exe ast <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 *
 * Convencao: a ordem de KW_* e a ordem das palavras em PALAVRAS_CHAVE (lexer.c). O Interner recebe as palavras
 * nessa ordem antes de ler o arquivo, entao o id de uma palavra-chave e (kind - KW_CONST) e os nomes embutidos
 * (SYM_*) tem ids fixos logo depois delas.
 */
#ifndef FORJA_TOKEN_H
#define FORJA_TOKEN_H

#include "common/base.h"

typedef enum {
    TK_EOF, TK_NL, TK_IDENT, TK_INT, TK_REAL, TK_TEXTO,
    KW_CONST, KW_LET, KW_VAR, KW_PROC, KW_STRUCT, KW_ENUM, KW_UNION, KW_UNIT, KW_PUB, KW_EXPORT,
    KW_IMPORT, KW_AS, KW_FOREIGN, KW_TEST, KW_IF, KW_ELSE, KW_FOR, KW_IN, KW_WHILE, KW_SWITCH,
    KW_CASE, KW_DEFAULT, KW_BREAK, KW_CONTINUE, KW_RETURN, KW_DEFER, KW_TRY, KW_OR_ELSE, KW_AND,
    KW_OR, KW_NOT, KW_TRUE, KW_FALSE, KW_NONE,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_COMMA, TK_COLON, TK_ASSIGN,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT, TK_EQ, TK_NE, TK_LT, TK_LE,
    TK_GT, TK_GE, TK_PLUSEQ, TK_MINUSEQ, TK_ARROW, TK_DOTDOT, TK_DOTDOTLT,
    TK_DOTDOTEQ, TK_DOT, TK_CARET, TK_AMP, TK_BANG,
    TK_PIPE, TK_TIL, TK_SHL, TK_SHR, /* v1.2: '|', '~', '<<', '>>' (bits em inteiro sem sinal) */
    TK_INTERROGA, /* v1.3: '?' (so existe '?^T') */
    TK_LCOLCHETE, TK_RCOLCHETE, /* v1.5: '[' e ']' (array [N]T, fatia []T, indice a[i], fatiamento a[lo..<hi]) */
    /* v1.7: f"..." (D16) */
    TK_FTEXTO_INICIO, TK_FTEXTO_PEDACO, TK_FTEXTO_CHAVE, TK_FTEXTO_FORMATO, TK_FTEXTO_FIM_CHAVE, TK_FTEXTO_FIM,
    TK_HASH, /* v1.12 B12: '#' de #load("arquivo") */
    TK_QTD
} TokenTipo;

/* 12 bytes. sym: id do nome (identificador e palavra-chave) ou do texto cru entre aspas (TK_TEXTO); -1 nos demais.
 * len satura em 65535 (so alimenta mensagens). */
typedef struct { uint32_t off; int32_t sym; uint16_t len; uint8_t tipo; } Token;

enum { KW_QTD = KW_NONE - KW_CONST + 1 };
/* v1.2: os 9 nomes de tipo novos vao no fim, entao nenhum SYM_* antigo muda de id. */
enum {
    SYM_I32 = KW_QTD, SYM_INT, SYM_F32, SYM_BOOL, SYM_PRINT, SYM_MAIN, SYM_EXPECT,
    SYM_I8, SYM_I16, SYM_I64, SYM_U8, SYM_U16, SYM_U32, SYM_U64, SYM_UINT, SYM_F64,
    SYM_STR, SYM_CSTR, SYM_RAWPTR, /* v1.3 */
    SYM_LINK, /* v1.4: 'link' e palavra contextual (so vale como 1o token de linha dentro de 'foreign') */
    SYM_NEW, SYM_MAKE, SYM_DYNAMIC, /* v1.6: o parser precisa deles: 1o argumento de new/make e tipo; [dynamic]T */
    SYM_MAP, SYM_BIT_SET, SYM_SOA, SYM_MATRIX, /* v1.9: palavras contextuais de tipo (so viram tipo antes de '[') */
    SYM_SIZE_OF, SYM_ALIGN_OF, SYM_OFFSET_OF, SYM_STATIC_ASSERT, /* v1.11 L9: embutidos de layout (size_of/align_of levam um TIPO; static_assert tambem e declaracao de topo) */
    SYM_QUAT, /* v1.11 L14a: tipo embutido quat (o parser aceita 'quat{...}' como literal mesmo em minuscula) */
    SYM_ALLOCATOR, SYM_QTD /* B01a: tipo embutido Allocator (parametro 'alloc: Allocator' de proc do usuario) */
};

#endif
