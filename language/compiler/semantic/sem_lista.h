/* sem_lista.h - o que sem_lista.c e sem_lista_busca.c compartilham: onde cada assinatura mora em Semantica.params_lista e as conferencias comuns.
 * Versao: 0.7.0
 * Changelog: 0.7.0 - K17: ASS_HEAP_PUSH, ASS_HEAP_PUSH_BY, ASS_HEAP_POP_BY; SemChamarHeapPush/PushBy/Pop/PopBy (sem_lista.c) e SemEhOrdenavelDoRuntime (sem_memoria.c, deixou de ser static).
 *            0.6.0 - K08c: SemChamarDedup (dedup(&l), em sem_lista.c).
 *            0.5.0 - S2b: ASS_SORTEIO (shuffle e choice: colecao e rng, com padrao so para nao dar E0503), SemChamarShuffle, SemChamarChoice e SemBelowDoRng (sem_sorteio.c).
 *            0.4.0 - S2a: ASS_COLECAO_ALLOC (reverse, com alloc opcional so para str) e SemChamarBuscaTexto / SemChamarReverseTexto (sem_texto_busca.c).
 *            0.3.1 - to_str aceita builder de qualquer alocador (so comentario de SemChamarToStr).
 *            0.3.0 - IA1-L9f: SemChamarToStr, SemFormatoTemInto, SemChamarFormatoInto e SemTextoNoBuilder (sem_texto_builder.c).
 *            0.2.0- IA1-L9e: ASS_MAPA, ASS_BYTES e ASS_TEXTO (keys/values, from_bytes, as_bytes); SemColecaoJaChecada, SemChamarEstatistica, SemChamarClearMapa, SemChamarVistasDoMapa,
 *                      SemChamarCloneTexto, SemChamarFromBytes e SemChamarAsBytes (sem_estat.c, sem_mapa_vistas.c, sem_texto_bytes.c).
 *            0.1.0 - IA1-L9c/L9d: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#ifndef FORJA_SEM_LISTA_H
#define FORJA_SEM_LISTA_H

#include "semantic/sem_interno.h"

/* Posicao (em params_lista) e nomes de cada assinatura; o tamanho de cada uma e o n que o tratador passa a SemLigarEmbutido. */
enum {
    ASS_LISTA = 0,         /* (lista)                            cap, clear, pop */
    ASS_RESERVE = 1,       /* (lista, cap)                       reserve */
    ASS_INDICE = 3,        /* (lista, i)                         ordered_remove, unordered_remove */
    ASS_INSERT_AT = 5,     /* (lista, i, item)                   insert_at */
    ASS_LISTA_VALOR = 8,   /* (lista, value)                     remove_value */
    ASS_COLECAO = 10,      /* (colecao)                          reverse */
    ASS_COLECAO_VALOR = 11, /* (colecao, value)                  index_of, last_index_of, contains, count, fill */
    ASS_COPY = 13,         /* (dst, src)                         copy */
    ASS_CLONE = 15,        /* (colecao, em)                      clone */
    ASS_SWAP = 17,         /* (colecao, i, j)                    swap */
    ASS_REPLACE = 20,      /* (colecao, old, new, n)             replace_value (n opcional) */
    ASS_MAPA = 24,         /* (m, alloc)                         keys, values (L9e) */
    ASS_BYTES = 26,        /* (bytes, alloc)                     from_bytes (L9e) */
    ASS_TEXTO = 28,        /* (s)                                as_bytes (L9e) */
    ASS_COLECAO_ALLOC = 29, /* (colecao, alloc)                  reverse (alloc so vale com str: S2a) */
    ASS_SORTEIO = 31,       /* (colecao, rng)                    shuffle, choice (rng opcional na tabela; a falta vira E0338: S2b) */
    ASS_HEAP_PUSH = 33,     /* (lista, v)                        heap_push (K17); heap_pop usa ASS_LISTA */
    ASS_HEAP_PUSH_BY = 35,  /* (lista, v, less)                  heap_push_by (K17) */
    ASS_HEAP_POP_BY = 38,   /* (lista, less)                     heap_pop_by (K17) */
    ASS_FIM = 40
};
_Static_assert((int)ASS_FIM == (int)SEM_LISTA_PARAMS, "params_lista precisa ter o tamanho das assinaturas");

/* Indice ou contagem inteira (qualquer largura; o literal adota int); constante negativa = E0421. 'papel' diz o que e, para a mensagem. */
bool SemListaInteiro(Semantica *s, Expr *v, const char *papel);
/* O valor do tipo do elemento (o literal adota o tipo; T -> ?T se for o caso). */
bool SemListaValorDoElemento(Semantica *s, Expr *v, TipoId elemento);
/* Elemento com == : inteiro, real, bool, str ou enum (struct nao entra: E0301). 'e' e a chamada, para a mensagem. */
bool SemListaComparavel(Semantica *s, const Expr *e, TipoId elemento);
/* Elemento que e dono de memoria nao se solta, copia nem duplica sem dono: E0311. */
bool SemListaSemDono(Semantica *s, const Expr *e, TipoId elemento, const char *efeito);
/* K08c: dedup(&l): tira as duplicatas consecutivas (lista var, elemento comparavel e sem dono); tratador exato em sem_call.c. */
bool SemChamarDedup(Semantica *s, Expr *e);
/* K17: heap_push(&l, v), heap_pop(&l) e as versoes _by com less = p (sem_lista.c); tratadores exatos em sem_call.c. */
bool SemChamarHeapPush(Semantica *s, Expr *e);
bool SemChamarHeapPushBy(Semantica *s, Expr *e);
bool SemChamarHeapPop(Semantica *s, Expr *e);
bool SemChamarHeapPopBy(Semantica *s, Expr *e);
/* K09: index_by, find_by, any_by, all_by e count_by(s, pred = p) (sem_lista.c); tratadores exatos em sem_call.c. */
bool SemChamarIndexBy(Semantica *s, Expr *e);
bool SemChamarFindBy(Semantica *s, Expr *e);
bool SemChamarAnyBy(Semantica *s, Expr *e);
bool SemChamarAllBy(Semantica *s, Expr *e);
bool SemChamarCountBy(Semantica *s, Expr *e);
/* K15: set_union, set_intersect e set_diff(&a, b) sobre map[K]bool (sem_lista.c); tratadores exatos em sem_call.c. */
bool SemChamarSetUnion(Semantica *s, Expr *e);
bool SemChamarSetIntersect(Semantica *s, Expr *e);
bool SemChamarSetDiff(Semantica *s, Expr *e);
/* K17: int, real (f32, f64) ou str: a ordem que o runtime compara sem proc (sem_memoria.c; a mesma do sort_by_key e do binary_search). */
bool SemEhOrdenavelDoRuntime(TipoId t);

bool SemChamarBusca(Semantica *s, Expr *e, int32_t embutido); /* EMB_INDEX_OF..EMB_REPLACE_VALUE (sem_lista_busca.c) */
bool SemChamarBuscaTexto(Semantica *s, Expr *e, int32_t embutido); /* index_of, last_index_of, contains e count sobre str (sem_texto_busca.c); o 1o argumento ja e str */
bool SemChamarReverseTexto(Semantica *s, Expr *e);                  /* reverse(str, alloc = A) -> str (sem_texto_busca.c); o 1o argumento ja e str */

/* S2b (sem_sorteio.c): shuffle(s, rng = &r) -> (); choice(s, rng = &r) -> ?T. O rng tem de ser ^Rng de std:rand (E0338). */
bool SemChamarShuffle(Semantica *s, Expr *e);                       /* EMB_SHUFFLE: troca no lugar, igual ao reverse de lista */
bool SemChamarChoice(Semantica *s, Expr *e);                        /* EMB_CHOICE: elemento sorteado embrulhado; colecao vazia = none, sem consumir o gerador */
/* O proc below (pub) do modulo std:rand do tipo ^Rng, ou NULL se o tipo nao e esse ponteiro. O lowering (lower_sorteio.c) acha o mesmo proc por aqui. */
const Decl *SemBelowDoRng(const Semantica *s, TipoId ponteiro);

/* IA1-L9e: a colecao (lista, array ou fatia) do argumento k de e, ja checado (SemChecar feito): confere a forma e o var, troca o argumento por uma fatia e devolve o elemento (sem_lista_busca.c). */
bool SemColecaoJaChecada(Semantica *s, Expr *e, int32_t k, bool alteracao, TipoId *elemento);
bool SemChamarEstatistica(Semantica *s, Expr *e, int32_t embutido);   /* min_of, max_of, sum, avg (sem_estat.c) */
bool SemChamarVistasDoMapa(Semantica *s, Expr *e, int32_t embutido);  /* keys, values (sem_mapa_vistas.c) */
bool SemChamarClearMapa(Semantica *s, Expr *e, Expr *alvo);           /* clear(&m): alvo e o 1o argumento, ja checado e do tipo ^map (sem_mapa_vistas.c) */
bool SemChamarCloneTexto(Semantica *s, Expr *e);                      /* clone(str, alloc = A) (sem_texto_bytes.c); o 1o argumento ja foi checado e e str */
bool SemChamarFromBytes(Semantica *s, Expr *e);                       /* from_bytes(b, alloc = A) (sem_texto_bytes.c) */
bool SemChamarAsBytes(Semantica *s, Expr *e);                         /* as_bytes(s) (sem_texto_bytes.c) */

/* IA1-L9f: builder de texto (sem_texto_builder.c) */
bool SemChamarToStr(Semantica *s, Expr *e);                           /* to_str(&buf) -> str: move os bytes do builder ([dynamic]u8, de qualquer alocador); a str herda a origem do builder local */
bool SemFormatoTemInto(Semantica *s, const Expr *e);                  /* a chamada de format tem um argumento chamado into? */
bool SemChamarFormatoInto(Semantica *s, Expr *e);                     /* format(f"...", into = &buf): anexa ao builder, sem valor */
bool SemTextoNoBuilder(Semantica *s, const Expr *lista, Arg *item);   /* append(&buf, item = "texto"): o item str (ja checado) de lista de u8 vira items (indice APPEND_PARAM_ITEMS) */

#endif
