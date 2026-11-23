/* lower_lista.h - pecas de IR dos embutidos de lista, array e fatia (lower_lista.c, lower_busca.c; IA1-L9c, IA1-L9d).
 * Nao existe IrOp nova: tudo e IR_ELEMENTO, IR_ENDERECO_ELEMENTO, IR_GRAVA_PONTEIRO, IR_TAMANHO_TEXTO, IR_PARA, IR_SE, IR_REDIMENSIONA, IR_ANEXA, IR_ANEXA_FATIA e IR_EMBRULHA,
 * entao os dois backends (C e x64) so precisam do que ja sabem, e dev e release diferem so pelas checagens de sempre (R0006 de indice, R0008 de epoca).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - K17: TipoDeOrdenacao (lower_memoria.c, deixou de ser static) e LwHeapPush/LwHeapPushBy/LwHeapPop/LwHeapPopBy (lower_lista.c).
 *            0.3.0 - IA1-L9f: LwToStr e LwFormatoInto (lower_texto_builder.c).
 *            0.2.0- IA1-L9e: LwEstat, LwVistasDoMapa, LwClearMapa, LwTextoCopia e LwAsBytes (lower_estat.c, lower_mapa_vistas.c, lower_texto_bytes.c).
 *            0.1.0 - IA1-L9c/L9d: primeira versao (lower_interno.h, congelado, ficou como estava).
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#ifndef FORJA_LOWER_LISTA_H
#define FORJA_LOWER_LISTA_H

#include "ir/lower_ops.h"

/* A colecao sobre a qual um embutido trabalha: valor = a fatia []T ou o ^[dynamic]T; lista = e ^[dynamic]T (a epoca se confere em dev, o indice nao se da por provado). */
typedef struct { Valor valor; TipoId elemento; bool lista; } LlBase;

const Expr *LlArgumento(const Expr *e, int32_t indice);                 /* o argumento do parametro `indice` (NULL se o fonte nao o escreveu) */
LlBase LlBaseDe(const LowerCtx *c, Valor valor);                        /* fatia []T ou ^[dynamic]T */
Valor LlIndiceInt(const LoCtx *k, const Expr *e);                       /* o inteiro de e (indice, contagem) como int */
Valor LlTamanho(const LoCtx *k, Valor colecao);                         /* IR_TAMANHO_TEXTO: len da fatia ou da lista */
Valor LlLe(const LoCtx *k, const LlBase *b, Valor indice, bool provado);       /* IR_ELEMENTO; provado = o indice ja esta em 0..<len (sem R0006 em fatia) */
Valor LlEnd(const LoCtx *k, const LlBase *b, Valor indice, bool provado);      /* IR_ENDERECO_ELEMENTO (^T) */
void LlGrava(const LoCtx *k, Valor endereco, TipoId elemento, Valor valor);    /* IR_GRAVA_PONTEIRO: *endereco = valor */
Valor LlEmbrulha(const LoCtx *k, TipoId opcional, Valor valor);                /* IR_EMBRULHA: valor como ?T */
Valor LlEntraLaco(const LoCtx *k, Valor inicio, Valor fim, IrBloco **externo); /* IR_PARA inicio ..< fim: entra no corpo; devolve o contador (int) */
void LlSaiDoLaco(const LoCtx *k, IrBloco *externo);
void LlPare(const LoCtx *k);                                                   /* IR_PARE */
void LlRedimensiona(const LoCtx *k, Valor lista, TipoId elemento, Valor novo_len); /* IR_REDIMENSIONA (encolher ou crescer com zeros) */
Valor LlMais(const LoCtx *k, Valor a, Valor b);                                /* a + b em int */
Valor LlMenos(const LoCtx *k, Valor a, Valor b);                               /* a - b em int */
Valor LlIgual(const LoCtx *k, Valor a, Valor b);                               /* a == b (bool; str por conteudo) */

uint8_t TipoDeOrdenacao(const LowerCtx *c, TipoId elemento); /* ORDENA_* do elemento (lower_memoria.c, K17: deixou de ser static) */

Valor LwLista(LowerCtx *c, const Expr *e);   /* EMB_CAP..EMB_REMOVE_VALUE (lower_lista.c); EMB_INDEX_OF em diante vai para LwBusca */
Valor LwBusca(LowerCtx *c, const Expr *e);   /* EMB_INDEX_OF..EMB_REPLACE_VALUE (lower_busca.c) */

/* IA1-L9e: estatistica, vistas de map e bytes de texto (tambem sem IrOp nova) */
Valor LwEstat(LowerCtx *c, const Expr *e);          /* EMB_MIN_OF..EMB_AS_BYTES (lower_estat.c) */
Valor LwVistasDoMapa(LowerCtx *c, const Expr *e);   /* keys e values (lower_mapa_vistas.c) */
Valor LwClearMapa(LowerCtx *c, const Expr *e);      /* clear(&m) de map (lower_mapa_vistas.c) */
Valor LwTextoCopia(LowerCtx *c, const Expr *e);     /* clone(str) e from_bytes: o IR_FORMATA do f"{x}" que a semantica montou no 1o argumento (lower_texto_bytes.c) */
Valor LwAsBytes(LowerCtx *c, const Expr *e);        /* as_bytes(s): IR_TRANSMUTA de str para []u8 (lower_texto_bytes.c) */

/* IA1-L9f: builder de texto (lower_texto_builder.c) */
Valor LwToStr(LowerCtx *c, const Expr *e);          /* to_str(&buf): IR_REDIMENSIONA REDIM_PARA_STR */
Valor LwFormatoInto(LowerCtx *c, const Expr *e);    /* format(f"...", into = &buf): IR_FORMATA str + IR_ANEXA_FATIA + IR_LIBERA */

#endif
