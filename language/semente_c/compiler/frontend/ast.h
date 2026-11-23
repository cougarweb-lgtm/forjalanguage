/* ast.h - arvore sintatica da Forja (contrato entre parser, semantica e lowering).
 * Versao: 0.43.0
 * Changelog: 0.43.0 - M6d P92b: POSSE_C_ENCERRA (clausula 'ends'; posse_param = -1; sizeof(Decl) segue 240).
 *0.42.0 - M6c P85: Decl.posse_c/posse_param/pos_posse (clausula creates/destroys/takes do proc foreign, no padding) e _Static_assert(sizeof(Decl) == 240).
 *0.41.0 - M6 P65a (D-M6-13): LocalNome e Decl.nomes_locais (tabela densa {nome, tipo, pos} por local_id, na arena; so a semantica grava).
 *            0.40.0 - B12: Expr.carregar (bool no padding do fim; sizeof segue 56): EX_TEXTO de #load("arquivo"), com nome = caminho ate o driver trocar pelo conteudo (ResolverLoads).
 *            0.39.0 - K15: EMB_SET_UNION, EMB_SET_INTERSECT e EMB_SET_DIFF (3 ids no fim de EmbutidoId, depois de EMB_COUNT_BY; sem_lista.c, lower_lista.c, sem_call.c, lower_call.c); 3 _Static_assert intactos.
 *            0.38.0 - K09: EMB_INDEX_BY, EMB_FIND_BY, EMB_ANY_BY, EMB_ALL_BY e EMB_COUNT_BY (5 ids no fim de EmbutidoId, depois de EMB_HEAP_POP_BY; sem_lista.c, lower_lista.c, sem_call.c, lower_call.c); 3 _Static_assert intactos.
 *            0.37.0 - K17: EMB_HEAP_PUSH, EMB_HEAP_POP, EMB_HEAP_PUSH_BY e EMB_HEAP_POP_BY (4 ids no fim de EmbutidoId, depois de EMB_DEDUP; sem_lista.c, lower_lista.c, sem_call.c, lower_call.c); 3 _Static_assert intactos.
 *            0.36.0 - K08c: EMB_DEDUP (1 id no fim de EmbutidoId, depois de EMB_CHOICE; sem_lista.c, sem_call.c, lower_lista.c, lower_call.c); 3 _Static_assert intactos.
 *            0.35.0 - S2b: EMB_SHUFFLE e EMB_CHOICE (2 ids no fim de EmbutidoId, depois de EMB_BINARY_SEARCH; sem_call.c, sem_sorteio.c, lower_sorteio.c); 3 _Static_assert intactos.
 *            0.34.0 - K08b: EMB_SORT_BY_KEY e EMB_BINARY_SEARCH (2 ids depois de EMB_SORT_BY); 3 _Static_assert intactos.
 *            0.33.0 - K08a: EMB_SORT_BY (1 id no fim de EmbutidoId, depois de EMB_TO_STR); 3 _Static_assert intactos.
 *            0.32.0 - B01d: Arg.pos_fim (no preenchimento do Arg, sem crescer o Expr: o forja migrar insere ", alloc = heap" depois do ultimo argumento da chamada).
 *            0.31.0 - B01a: ALOC_PARAM (6, depois de ALOC_POOL; ORIGEM_LITERAL e ORIGEM_C de escopo.h sobem para 7 e 8).
 *            0.30.0 - IA1-L9f: EMB_TO_STR (1 id no fim de EmbutidoId; sem_texto_builder.c, lower_texto_builder.c); 3 _Static_assert intactos.
 *            0.29.0- IA1-L9e: EMB_MIN_OF..EMB_AS_BYTES (8 ids no fim de EmbutidoId; faixa propria, sem_estat.c, sem_mapa_vistas.c, sem_texto_bytes.c, lower_estat.c, lower_mapa_vistas.c, lower_texto_bytes.c); 3 _Static_assert intactos.
 *            0.28.0 - IA1-L9c/L9d: EMB_CAP..EMB_REPLACE_VALUE (19 ids no fim de EmbutidoId; faixa propria, sem_lista.c, sem_lista_busca.c, lower_lista.c e lower_busca.c); 3 _Static_assert intactos.
 *            0.27.0 - IA1-L9a/L9b: EMB_WRAPPING_ADD..EMB_MUL_HI_U64 (19 ids no fim de EmbutidoId; faixa propria, sem_aritmetica.c e lower_aritmetica.c) e OP_SOMA_VOLTA, OP_SUB_VOLTA, OP_MUL_VOLTA (no fim de Operador; so em IR_BINARIO); 3 _Static_assert intactos.
 *            0.26.0 - IA1-L5b: EMB_RESIZE e EMB_SORT (no fim de EmbutidoId); 3 _Static_assert intactos.
 *            0.25.0 - IA1-L5a: EMB_EPRINT (no fim de EmbutidoId) e FMT_CURTO (bit 16 do formato da parte de f"..."); 3 _Static_assert intactos.
 *            0.24.0 - IA1-L8: Decl.chave e Decl.homonimo [sem] (chave por modulo dos simbolos de topo); 3 _Static_assert intactos.
 *            0.23.0 - IA1-L3c2: Caso.pos_membro (o E0110 da forma antiga de membro de enum no 'case'); 3 _Static_assert intactos.
 *            0.22.0 - Linguagem v1.12 M3 P21v-a: EX_CHAMADA_IND (no fim de ExprTipo) e os nomes dos parametros do tipo proc (alvo[nome + k] de TR_PROC); 3 _Static_assert intactos.
 *            0.21.0 -Linguagem v1.11 L15a: TR_LISTA_FIXA (no fim de TipoRefForma; '[dynamic, N]T' como TR_ARRAY: nome = const do N ou -1, pos_tamanho = token do N).
 *            0.20.0 - Linguagem v1.11 L14a: EMB_CONJ (no fim de EmbutidoId).
 *            0.19.0 -Linguagem v1.11 L13: EMB_MIN, EMB_MAX, EMB_ABS, EMB_CLAMP, EMB_SQRT (no fim de EmbutidoId) e OP_RAIZ (no fim de Operador; so em IR_UNARIO, a semantica nao o escreve em Expr).
 *            0.18.0 -Linguagem v1.11 L12b: REF_COMPONENTE no fim de RefTipo (swizzle de varias letras em EX_CAMPO, ref_id = k << 8 | indices 2 bits cada; literal de indice criado pela semantica em EX_INT, ref_id = o valor).
 *            0.17.0 - Linguagem v1.11 L11: FMT_DEPURAR ({expr:?}; a semantica grava so esse bit em Arg.indice; o lowering expande a parte).
 *            0.16.0 -Linguagem v1.11 L9: D_ASSERT (static_assert de topo) e EMB_SIZE_OF, EMB_ALIGN_OF, EMB_OFFSET_OF, EMB_STATIC_ASSERT (no fim dos enums; 3 _Static_assert intactos).
 *            0.15.0 - Linguagem v1.11 L7: Decl.com_erro [sem] (a declaracao ja guardou o seu erro na lista; os passes seguintes a pulam).
 *            0.14.0 - Linguagem v1.11 L3: Decl.fuga_relevante [sem] (proc/test com parametro, retorno ou local com ponteiro: so esses entram na fuga de endereco).
 *            0.13.0 -Linguagem v1.10 P1: Decl.simbolo/pos_simbolo (alias de simbolo C no foreign, A6); 3 _Static_assert intactos.
 *            0.12.0 -Linguagem v1.9 P2: TR_MAPA, TR_BIT_SET, TR_SOA, TR_MATRIZ; EMB_INSERT..EMB_TRANSPOSE; m[i, j] em EX_INDICE (args[0], nargs = 1); os 3 _Static_assert intactos.
 *            0.11.0 - Linguagem v1.8b P2: TR_PROC (callback de foreign), Membro.valor/pos_valor/tem_valor, Decl.op = 1 em D_ENUM com valor (3 _Static_assert intactos).
 *            0.10.0 - Linguagem v1.8 P2a: modulos (Expr/TipoRef/Caso/Decl.modulo, TipoRef.modulo_erro, Expr.alocacao, ALOC_*, D_IMPORT) (3 _Static_assert intactos).
 *            0.9.0 - Linguagem v1.7 P1: ?T, union, retorno multiplo, f"..." (D24): TR_OPCIONAL/TR_TUPLA, EX_EMBRULHA/EX_OPC_TESTE/EX_TUPLA/EX_FTEXTO,
 *                    FMT_*, ST_LET_MULTIPLO + NomeLet, Caso.tipo_escrito, D_UNIAO, Decl.retornos, EMB_FORMAT/EMB_FORMAT_C (3 _Static_assert intactos).
 *            0.8.0 - Linguagem v1.6 P1a: TR_DINAMICO, EX_TIPO, EMB_NEW..EMB_FRAME_RESET, Decl.contem_dono (os 3 _Static_assert intactos).
 *            0.7.0 - Linguagem v1.5 P1a: TR_ARRAY/TR_FATIA + TipoRef.pos_tamanho, EX_INDICE/EX_FATIA/EX_LITERAL_ARRAY + FATIA_*,
 *                    Stmt.item_ref/nome_indice/local_indice (for de colecao); sizeof de TipoRef/Expr/Stmt travado por _Static_assert.
 *            0.6.0 - Linguagem v1.4 P1: D_LINK ('link "lib"' dentro de foreign: nome = texto, pos = offset da string) e Decl.externo.
 *            0.5.0 - Linguagem v1.3: TipoRef recursivo (TR_NOME/TR_PONTEIRO/TR_ANULAVEL + alvo; sai 'bool ponteiro'), EX_NONE, OP_DEREF,
 *                    EMB_LEN e uniao args/alvo em Expr (o 'as' guarda o TipoRef*; Expr continua em 56 bytes).
 *            0.4.0 - Linguagem v1.2: operadores de bits (OP_BIT_E..OP_BIT_NAO), EMB_TRANSMUTE, Campo.pos_min/pos_max (E0420 por
 *                    largura aponta o limite) e Expr.op de EX_INT = quantidade de digitos do literal (caminho rapido da faixa).
 *            0.3.0 - Linguagem v1.1: Decl.publico e Decl.exportado ('pub', 'pub export proc').
 *            0.2.0 - Fase 5c: campos de resolucao da semantica. RefTipo/ref/ref_id em Expr (nome -> local/param/const/enum/
 *                    membro/campo/proc/struct/embutido), Arg.indice, Decl.indice, Decl.nlocais, Stmt.local_id, Caso.local_id/
 *                    tipo/indice_membro e o enum EmbutidoId. Numeracao documentada em semantic/semantic.h.
 *            0.1.0 - Fase 5a: contrato do subconjunto da Fase 5 (subconjunto v0 + enum, T ! E, try, or_else,
 *                    switch, defer de 1 comando, break/continue, test, expect). O parser (5b) pode refinar campos;
 *                    mudanca de campo = bump aqui + nota no MEM-LJ-ADR-38.
 * Verificacao: construir.bat (compila com -Werror); a partir da 5b: forja ast <arq.fj> imprime esta arvore em JSON.
 *
 * Convencoes: posicao = offset em bytes na Fonte (linha/coluna so ao reportar); nome = id do Interner;
 *             tudo vive na Arena do compilador (libera-se a arena inteira, nunca no a no); "Tipo" = id da tabela de tipos de semantic/
 *             (0 = ainda nao resolvido). Campos marcados [sem] so a semantica preenche.
 */
#ifndef FORJA_AST_H
#define FORJA_AST_H

#include "common/base.h"

typedef uint32_t TipoId;
enum { TIPO_NAO_RESOLVIDO = 0 };

/* [sem] A que o nome de uma Expr foi ligado (ref_id depende do tipo; ver semantic/semantic.h). */
typedef enum {
    REF_NENHUM, REF_LOCAL, REF_PARAM, REF_CONST, REF_ENUM, REF_MEMBRO, REF_CAMPO, REF_PROC, REF_STRUCT, REF_EMBUTIDO,
    REF_COMPONENTE /* v1.11 L12b: v.xy (EX_CAMPO, varias letras) e o EX_INT de indice que a semantica cria para v.x (vira EX_INDICE) */
} RefTipo;

/* ref_id de REF_COMPONENTE (swizzle de varias letras): (k << 8) | posicao de cada letra em 2 bits (a 1a nos 2 bits de baixo). */
enum { SWIZZLE_LETRAS_MAX = 4, SWIZZLE_BITS_POR_LETRA = 2, SWIZZLE_MASCARA_LETRA = 3, SWIZZLE_DESLOCA_CONTA = 8 };

/* ref_id de REF_EMBUTIDO. */
typedef enum { EMB_PRINT, EMB_ASSERT, EMB_PANIC, EMB_TRANSMUTE, EMB_LEN,
                 EMB_NEW, EMB_MAKE, EMB_APPEND, EMB_DELETE, EMB_FRAME_RESET,
                 EMB_FORMAT, EMB_FORMAT_C /* v1.7 */,
                 EMB_INSERT, EMB_GET, EMB_GET_PTR, EMB_HAS, EMB_REMOVE, EMB_TRANSPOSE /* v1.9 */,
                 EMB_SLICE_FROM, /* v1.11: fatia emprestada de ponteiro C */
                 EMB_SIZE_OF, EMB_ALIGN_OF, EMB_OFFSET_OF, EMB_STATIC_ASSERT /* v1.11 L9: layout (valor no lowering) */,
                 EMB_MIN, EMB_MAX, EMB_ABS, EMB_CLAMP, EMB_SQRT /* v1.11 L13: matematica embutida (sem_matematica.c, lower_matematica.c) */,
                 EMB_CONJ, /* v1.11 L14a: conj(q) do quat (sem_quat.c, lower_quat.c); fora da tabela de matematica */
                 EMB_EPRINT, /* IA1-L5a: eprint(x) = print para stderr (tratador exato; fora da faixa da matematica) */
                 EMB_RESIZE, EMB_SORT /* IA1-L5b: resize(&l, len = n) e sort(fatia) (tratadores exatos) */,
                 /* IA1-L9a: aritmetica com volta, saturada, checada e com par (valor, estourou); 4 familias x (add, sub, mul), nessa ordem */
                 EMB_WRAPPING_ADD, EMB_WRAPPING_SUB, EMB_WRAPPING_MUL, EMB_SATURATING_ADD, EMB_SATURATING_SUB, EMB_SATURATING_MUL,
                 EMB_CHECKED_ADD, EMB_CHECKED_SUB, EMB_CHECKED_MUL, EMB_OVERFLOWING_ADD, EMB_OVERFLOWING_SUB, EMB_OVERFLOWING_MUL,
                 /* IA1-L9b: bits e a metade alta do produto de 128 bits */
                 EMB_ROTATE_LEFT, EMB_ROTATE_RIGHT, EMB_COUNT_ONES, EMB_LEADING_ZEROS, EMB_TRAILING_ZEROS, EMB_BYTE_SWAP, EMB_MUL_HI_U64,
                 /* IA1-L9c: lista dona (sem_lista.c, lower_lista.c); EMB_CAP a EMB_REMOVE_VALUE, nessa ordem */
                 EMB_CAP, EMB_CLEAR, EMB_RESERVE, EMB_TRUNCATE, EMB_POP, EMB_INSERT_AT, EMB_ORDERED_REMOVE, EMB_UNORDERED_REMOVE, EMB_REMOVE_VALUE,
                 /* IA1-L9d: busca e movimento sobre lista, array e fatia (sem_lista_busca.c, lower_busca.c); EMB_INDEX_OF a EMB_REPLACE_VALUE, nessa ordem */
                 EMB_INDEX_OF, EMB_LAST_INDEX_OF, EMB_CONTAINS, EMB_COUNT, EMB_COPY, EMB_FILL, EMB_CLONE, EMB_SWAP, EMB_REVERSE, EMB_REPLACE_VALUE,
                 /* IA1-L9e: estatistica de lista (sem_estat.c), vistas de map (sem_mapa_vistas.c) e bytes de texto (sem_texto_bytes.c); EMB_MIN_OF a EMB_AS_BYTES, nessa ordem (clear de map e clone de str reusam EMB_CLEAR e EMB_CLONE) */
                 EMB_MIN_OF, EMB_MAX_OF, EMB_SUM, EMB_AVG, EMB_KEYS, EMB_VALUES, EMB_FROM_BYTES, EMB_AS_BYTES,
                 /* IA1-L9f: builder de texto (sem_texto_builder.c, lower_texto_builder.c): to_str(&buf); append de str e format(into =) reusam EMB_APPEND e EMB_FORMAT */
                 EMB_TO_STR,
                 /* K08a: sort_by(s, less = p) (tratador exato; sem_memoria.c, lower_memoria.c); fora da faixa de EMB_MIN_OF..EMB_TO_STR */
                 EMB_SORT_BY,
                 /* K08b: sort_by_key(s, key = Struct.campo) e binary_search(s, value = v) (sem_memoria.c, lower_memoria.c); depois de EMB_SORT_BY */
                 EMB_SORT_BY_KEY, EMB_BINARY_SEARCH,
                 /* S2b: shuffle(s, rng = &r) e choice(s, rng = &r) -> ?T (sem_sorteio.c, lower_sorteio.c); depois de EMB_BINARY_SEARCH */
                 EMB_SHUFFLE, EMB_CHOICE,
                 /* K08c: dedup(&l) remove duplicatas consecutivas de lista (sem_lista.c, lower_lista.c); depois de EMB_CHOICE */
                 EMB_DEDUP,
                 /* K17: heap_push(&l, v), heap_pop(&l) e as versoes _by(&l, less = p) (sem_lista.c, lower_lista.c); depois de EMB_DEDUP */
                 EMB_HEAP_PUSH, EMB_HEAP_POP, EMB_HEAP_PUSH_BY, EMB_HEAP_POP_BY,
                 /* K09: index_by, find_by, any_by, all_by e count_by(s, pred = p) (sem_lista.c, lower_lista.c); depois de EMB_HEAP_POP_BY */
                 EMB_INDEX_BY, EMB_FIND_BY, EMB_ANY_BY, EMB_ALL_BY, EMB_COUNT_BY,
                 /* K15: set_union, set_intersect e set_diff(&a, b) sobre map[K]bool (presenca = membro; sem_lista.c, lower_lista.c); depois de EMB_COUNT_BY */
                 EMB_SET_UNION, EMB_SET_INTERSECT, EMB_SET_DIFF } EmbutidoId;

enum { EMB_ARIT_FAMILIA_TAM = 3, EMB_ARIT_FAMILIAS = 4 }; /* IA1-L9a: (id - EMB_WRAPPING_ADD) / 3 = familia (0 wrapping, 1 saturating, 2 checked, 3 overflowing); % 3 = add, sub, mul */

/* IA1-L5b: indices (Arg.indice) dos parametros de make e append que o lowering le: make(tipo, alloc, cap, len) e append(lista, item, items). */
enum { MAKE_PARAM_CAP = 2, MAKE_PARAM_LEN = 3, APPEND_PARAM_ITEM = 1, APPEND_PARAM_ITEMS = 2 };

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Decl Decl;

/* v1.8: onde o new/make/format aloca (Expr.alocacao) e a origem de um local (mesmos valores). */
enum { ALOC_NENHUM, ALOC_HEAP, ALOC_FRAME, ALOC_ESCOPO, ALOC_ARENA, ALOC_POOL, ALOC_PARAM }; /* B01a: ALOC_PARAM = valor vindo do parametro 'alloc: Allocator' */

/* ---------- tipos escritos no codigo ---------- */
/* "i32", "^Grifo", "?^Grifo", "^^i32", "Malha ! ErroCarga" ou "! ErroCarga" (TR_NOME com nome = -1 quando so ha erro).
 * '^T' e '?^T' sao UM no com 'alvo' = T (recursivo, arena); '?^T' nao tem no '^' dentro. */
typedef enum { TR_NOME, TR_PONTEIRO, TR_ANULAVEL, TR_ARRAY, TR_FATIA, TR_DINAMICO,
               TR_OPCIONAL,  /* v1.7: '?T' (alvo = T, pos = o '?'); '?^T' continua TR_ANULAVEL */
               TR_TUPLA      /* v1.7: so em Decl.retorno ('-> (q: int, r: int)'); elementos em Decl.retornos, pos = '(', pos_tamanho = '!' (erro >= 0) */
               , TR_PROC     /* v1.8b: 'proc(nome: T, ...) -> R' (callback de foreign): alvo = array de 2 * nome + 1 TipoRef na arena (alvo[0] = retorno,
                              * valido so se pos_tamanho == 1; alvo[1..nome] = parametros; v1.12 P21v-a: alvo[nome + k] (k = 1..nome) guarda o NOME
                              * do parametro k em .nome/.pos), nome = quantidade de parametros (0..16), erro = -1,
                              * pos = o 'proc', pos_tamanho = 1 se tem '-> R' */
               , TR_MAPA     /* v1.9: map[K]V; alvo = 2 TipoRef na arena (alvo[0] = K, alvo[1] = V); pos = 'map'; nome = -1 */
               , TR_BIT_SET  /* v1.9: bit_set[E]; alvo = 1 TipoRef (o enum); pos = 'bit_set'; nome = -1 */
               , TR_SOA      /* v1.9: soa[N]S / soa[dynamic]S; alvo = 1 TipoRef de forma TR_ARRAY, TR_DINAMICO ou TR_FATIA (pos = o '[');
                              * pos = 'soa'; nome = -1 */
               , TR_MATRIZ   /* v1.9: matrix[R, C]T; alvo = 1 TipoRef (elemento); nome = (R << 4) | C; pos_tamanho = offset do R; pos = 'matrix' */
               , TR_LISTA_FIXA /* v1.11: [dynamic, N]T; alvo = o elemento; nome = id da const do N ou -1 (literal); pos_tamanho = offset do token do N; pos = o '[' */
} TipoRefForma;
typedef struct TipoRef TipoRef;
struct TipoRef {
    TipoRef *alvo;        /* TR_PONTEIRO/TR_ANULAVEL/TR_ARRAY/TR_FATIA: o tipo apontado ou o elemento; NULL em TR_NOME */
    int32_t nome, erro;   /* TR_NOME: nome (-1 so em '! E'); TR_ARRAY: id da const do N ou -1 (literal); erro: enum depois de '!' (-1 sem), so no no de fora */
    uint32_t pos;         /* offset do 1o token do tipo escrito ('?', '^', '[' ou o nome) */
    uint32_t pos_tamanho; /* TR_ARRAY: offset do token do N (o N mora na tabela de tipos, nao no no); TR_DINAMICO: offset do 'dynamic' */
    TipoId tipo;          /* [sem] */
    uint8_t forma;        /* TipoRefForma */
    uint8_t modulo, modulo_erro; /* v1.8: 0 = sem qualificador; k = 'apelido.' do modulo k (TR_NOME: pos = apelido, pos_tamanho = nome); modulo_erro: 'T ! apelido.E' */
};
_Static_assert(sizeof(TipoRef) == 32, "TipoRef nao pode crescer (v1.5)");

/* ---------- expressoes ---------- */
typedef enum {
    EX_INT, EX_REAL, EX_BOOL, EX_TEXTO, EX_NOME, EX_CAMPO, EX_CHAMADA, EX_LITERAL_STRUCT,
    EX_UNARIO, EX_BINARIO, EX_CONVERSAO, EX_TRY, EX_OR_ELSE, EX_NONE,
    /* v1.5 */
    EX_INDICE,        /* a = base, b = indice, pos_op = '['; v1.9: m[i, j] guarda o 2o indice em args[0].valor com nargs = 1 (nargs = 0 no indice simples) */
    EX_FATIA,         /* a = base, args[0].valor = inicio, args[1].valor = fim (nargs 2; 0 em 'a[..]'), op = FATIA_*, pos_op = '[' */
    EX_LITERAL_ARRAY, /* args = elementos (nome = -1), nargs (0 em '{}'), pos = pos_op = '{' */
    /* v1.6 */
    EX_TIPO,          /* alvo = TipoRef escrito (nargs 0); pos = pos_op = alvo->pos; so existe como 1o argumento de new e make */
    /* v1.7 */
    EX_EMBRULHA,      /* criado SO pela semantica (D3): a = valor original; tipo = ?T ou union; op = tag da variante (union) ou 0 */
    EX_OPC_TESTE,     /* criado SO pela semantica (D5): a = valor ?T; op = 0 'tem valor' (!= none), 1 'e none' (== none); tipo bool */
    EX_TUPLA,         /* 'return a, b': args = valores (nome -1), nargs 2..; pos = pos_op = pos do 1o valor */
    EX_FTEXTO,        /* f"...": args = partes. Parte texto: valor EX_TEXTO, nome -1, indice -1. Parte {expr}: valor = expr, nome = texto do
                         formato internado ou -1, pos_nome = o ':' (ou valor->pos sem formato), indice = 0 no parser; a semantica grava
                         ali o formato empacotado (FMT_*) */
    /* v1.12 P21v-a */
    EX_CHAMADA_IND    /* chamada sobre expressao de tipo proc: a = o chamado (nome, campo, chamada...), args/nargs como em EX_CHAMADA, pos_op = o '('
                         (ou o nome, quando a semantica converte 'f(x)' de local proc); a semantica grava o retorno em tipo */
} ExprTipo;

/* v1.7: formato empacotado de {expr:FMT} (Arg.indice de EX_FTEXTO). Largura 0 = sem; o campo de precisao guarda precisao + 1 (0 = sem). */
enum { FMT_LARGURA_MASCARA = 0xFF, FMT_PRECISAO_DESLOC = 8, FMT_PRECISAO_MASCARA = 0x1F, FMT_ZERO = 1 << 13, FMT_HEX = 1 << 14,
       FMT_DEPURAR = 1 << 15, /* v1.11: {expr:?}, sozinho (nao combina com largura, precisao, zeros nem hex) */
       FMT_CURTO = 1 << 16 /* IA1-L5a: real no formato curto que volta igual (print/eprint de f32/f64; a semantica grava, o usuario nao escreve) */ };

enum { FATIA_EXCL, FATIA_INCL, FATIA_INTEIRA }; /* Expr.op de EX_FATIA: '..<', '..=', '..' */

typedef enum {
    OP_OU, OP_E, OP_IGUAL, OP_DIFERENTE, OP_MENOR, OP_MENOR_IGUAL, OP_MAIOR, OP_MAIOR_IGUAL,
    OP_SOMA, OP_SUB, OP_MUL, OP_DIV, OP_RESTO, OP_NEGAR, OP_NAO, OP_ENDERECO,
    /* v1.2: so em inteiro sem sinal. '~' binario e o ou-exclusivo ('^' ja e ponteiro); OP_BIT_NAO e o '~' unario. */
    OP_BIT_E, OP_BIT_OU, OP_BIT_XOU, OP_DESLOCA_ESQ, OP_DESLOCA_DIR, OP_BIT_NAO,
    OP_DEREF, /* v1.3: 'p^' posfixo (EX_UNARIO: a = operando, pos_op = o '^') */
    OP_RAIZ,  /* v1.11 L13: raiz quadrada de f32/f64; so em IR_UNARIO (sqrt(x) do fonte vira a chamada EMB_SQRT) */
    OP_SOMA_VOLTA, OP_SUB_VOLTA, OP_MUL_VOLTA /* IA1-L9a: + - * de inteiro com volta (modulo 2^N, sem R0002 nos dois modos); so em IR_BINARIO */
} Operador;

/* Argumento de chamada ou campo de literal de struct: o 1o de uma chamada e posicional (nome = -1). */
typedef struct {
    int32_t nome;
    uint32_t pos_nome;
    Expr *valor;
    int32_t indice; /* [sem] posicao do parametro (chamada) ou do campo (literal de struct) ligado */
    uint32_t pos_fim; /* B01d: offset logo depois do ultimo token do argumento de chamada ('forja migrar' insere ", alloc = heap" aqui); 0 em literal e no 1o argumento de new/make (o tipo) */
} Arg;

struct Expr {
    uint8_t tipo_no; /* ExprTipo */
    uint8_t op;      /* Operador (unario/binario); valor do bool (EX_BOOL); quantidade de digitos (EX_INT, satura em 255) */
    bool flexivel;   /* [sem] arvore so de literais numericos: o tipo vem do contexto */
    uint8_t ref;     /* [sem] RefTipo */
    TipoId tipo;     /* [sem] */
    int32_t ref_id;  /* [sem] indice do que o nome liga (depende de 'ref'); so vale se ref != REF_NENHUM */
    uint32_t pos;    /* inicio da expressao */
    uint32_t pos_op; /* operador, nome (variavel, campo, proc, struct), tipo do cast ou literal */
    int32_t nome;    /* variavel, campo, proc ou struct; tipo-alvo do 'as'; texto internado do EX_TEXTO */
    Expr *a, *b;     /* operandos; EX_CAMPO/EX_CONVERSAO/EX_TRY usam so 'a'; EX_OR_ELSE: a or_else b */
    union {
        Arg *args;     /* EX_CHAMADA, EX_LITERAL_STRUCT, EX_FATIA e EX_LITERAL_ARRAY */
        TipoRef *alvo; /* EX_CONVERSAO: o tipo depois do 'as' (nargs = 0); nome = alvo->nome se TR_NOME, senao -1; EX_TIPO: o tipo escrito */
    };
    uint32_t nargs;
    uint8_t modulo;    /* v1.8: 0 = sem qualificador; k = 'apelido.' do modulo k (pos = apelido, pos_op = nome) */
    uint8_t alocacao;  /* [sem] v1.8: AlocacaoTipo do new/make/format/format_c/delete; 0 nos demais */
    bool carregar;     /* v1.12 B12: EX_TEXTO de #load("arquivo"); nome = caminho cru ate o driver trocar pelo conteudo (antes da semantica) */
};
_Static_assert(sizeof(Expr) == 56, "Expr nao pode crescer (v1.5)");

/* ---------- comandos ---------- */
typedef enum {
    ST_LET, ST_VAR, ST_ATRIBUICAO, ST_IF, ST_WHILE, ST_FOR, ST_RETURN, ST_CHAMADA,
    ST_BREAK, ST_CONTINUE, ST_DEFER, ST_SWITCH, ST_EXPECT,
    ST_LET_MULTIPLO /* v1.7: 'let q, r = f()' / 'var a, _ = f()': op = 0 let / 1 var; nomes/nnomes; expr = valor; pos = 'let'/'var' */
} StmtTipo;

/* v1.7: um nome de ST_LET_MULTIPLO. '_' descarta (local_id -1). tipo e local_id [sem]. */
typedef struct { int32_t nome; uint32_t pos; TipoId tipo; int32_t local_id; } NomeLet;

typedef struct {
    int32_t tipo_nome;  /* "case Malha:" ou "case ErroCarga:" ou "case Direcao.cima:" -> nome do tipo/enum */
    int32_t membro;     /* nome do membro do enum, ou -1 */
    uint32_t pos_membro; /* L3c2: offset do nome do membro (so vale com membro >= 0) */
    uint32_t pos;
    bool padrao;        /* "default:" */
    Stmt *corpo;
    TipoId tipo;        /* [sem] tipo que o case estreita (o da ligacao); no default, o tipo do valor inteiro */
    int32_t indice_membro; /* [sem] indice do membro do enum (-1 se o case e de tipo ou default) */
    int32_t local_id;   /* [sem] local da ligacao 'switch r in ...' neste case (-1 sem ligacao) */
    TipoRef *tipo_escrito; /* v1.7: "case ^Grifo:", "case [3]int:", "case ?i32:" (NULL = usa tipo_nome/membro); 'case none:' = tipo_nome KW_NONE - KW_CONST */
    uint8_t modulo;     /* v1.8: 'case apelido.Enum.membro:' (0 = sem qualificador) */
} Caso;

/* Stmt.op de ST_ATRIBUICAO */
enum { ATRIB_SIMPLES = 0, ATRIB_SOMA = 1, ATRIB_SUB = 2 };

struct Stmt {
    uint8_t tipo_no; /* StmtTipo */
    uint8_t op;      /* ST_ATRIBUICAO: 0 '=', 1 '+=', 2 '-=' */
    bool tem_tipo;   /* let/var com tipo escrito */
    uint32_t pos, pos_fim;
    int32_t nome;    /* variavel declarada; variavel de ligacao do switch */
    TipoRef tipo;    /* tipo escrito (let/var) */
    TipoId tipo_local; /* [sem] tipo do local (let/var/for): o que o 'forja ast --tipos' mostra */
    int32_t local_id;  /* [sem] numero do local declarado (let/var/for) dentro do proc/test (so ST_LET, ST_VAR e ST_FOR) */
    Expr *expr, *expr2, *alvo; /* valor/condicao/inicio; fim do for; lado esquerdo da atribuicao */
    bool faixa_excl;    /* for i in a..<b (true) ou a..=b (false) */
    /* v1.5: ST_FOR de colecao = expr2 == NULL; nome = item; nome_indice/local_indice = -1 sem indice; item_ref = 'for &item' */
    bool item_ref;
    int32_t nome_indice;
    Stmt *corpo, *senao, *prox; /* ST_DEFER: corpo = o unico comando adiado; ST_LET/ST_VAR (v1.8): [sem] ST_DEFER sintetico do 'em = escopo' (o parser deixa NULL) */
    union { Caso *casos; NomeLet *nomes; };          /* ST_SWITCH / ST_LET_MULTIPLO (v1.7) */
    union { uint32_t ncasos; uint32_t nnomes; };
    int32_t local_indice; /* [sem] local do indice do 'for i, item in' (-1 sem indice) */
};
_Static_assert(sizeof(Stmt) == 128, "Stmt nao pode crescer (v1.5)");

/* ---------- declaracoes ---------- */
typedef enum { D_UNIDADE_BASE, D_UNIDADE_DERIV, D_CONST, D_STRUCT, D_ENUM, D_PROC, D_TEST, D_LINK,
               D_UNIAO, /* v1.7: union Nome { variantes }: nome, pos = offset do NOME, variantes em campos/ncampos (Campo.nome -1, padrao NULL) */
               D_IMPORT /* v1.8: import "caminho" [as apelido]: nome = caminho (internado, sem aspas), nome_a = apelido, pos = 'import', pos_a = texto (aspas),
                           pos_b = apelido (o IDENT depois de 'as'; sem 'as' = pos_a), indice = modulo importado */
               , D_ASSERT /* v1.11 L9: static_assert(cond, msg = "...") no topo: init = a chamada (EX_CHAMADA de static_assert), pos = o nome, nome = -1 */
} DeclTipo;

/* M6c P85: clausula de posse de handle depois da assinatura do proc foreign (Decl.posse_c) */
typedef enum { POSSE_C_NENHUMA, POSSE_C_CRIA_RETORNO, POSSE_C_CRIA_SAIDA, POSSE_C_DESTROI, POSSE_C_ENTREGA, POSSE_C_ENCERRA } PosseClausula;

typedef struct {
    int32_t nome;
    uint32_t pos;
    TipoRef tipo;
    Expr *padrao;
    bool tem_faixa, faixa_excl;
    int64_t faixa_min, faixa_max;
    uint32_t pos_min, pos_max; /* offset de cada limite (do '-' se negativo): onde o E0420 por largura aponta */
} Campo;

typedef struct { int32_t nome; uint32_t pos; TipoRef tipo; Expr *padrao; } Param;
/* [sem] M6 P65a: nome e tipo de um local do usuario (a depuracao precisa; o IR so guarda o id) */
typedef struct { int32_t nome; TipoId tipo; uint32_t pos; } LocalNome;
/* enum; valor/pos_valor: o '= v' escrito ou, no enum com valor (Decl.op == 1), o implicito (pos_valor do implicito = pos do nome) */
typedef struct { int32_t nome; uint32_t pos; int64_t valor; uint32_t pos_valor; bool tem_valor; } Membro;

struct Decl {
    uint8_t tipo_no; /* DeclTipo */
    uint8_t op;      /* D_UNIDADE_DERIV: '*' ou '/'; D_ENUM: 1 = enum com valor explicito (v1.8b) */
    uint8_t modulo;  /* v1.8: modulo do arquivo de onde a Decl veio (0 = raiz) */
    int32_t nome, indice; /* indice [sem]: posicao entre as declaracoes da mesma categoria, na ordem do fonte */
    int32_t simbolo;      /* v1.10 (A6): proc foreign com alias 'proc nome = "simbolo_c"(...)': nome internado do simbolo C; -1 = nome e o simbolo */
    uint32_t pos_simbolo; /* v1.10: offset do texto entre aspas do alias */
    uint32_t nlocais;     /* [sem] D_PROC/D_TEST: parametros + locais; local_id vai de 0 a nlocais-1, sem reuso */
    LocalNome *nomes_locais; /* [sem] D_PROC/D_TEST: nlocais entradas, indexadas por local_id (parametros primeiro); NULL sem corpo analisado */
    uint32_t pos, pos_fim;
    int32_t nome_a, nome_b; /* unidade derivada: A op B */
    uint32_t pos_a, pos_b;
    TipoRef tipo;    /* const; unidade base: f32 */
    Expr *init;      /* const */
    Campo *campos; uint32_t ncampos;
    Param *params; uint32_t nparams;
    Membro *membros; uint32_t nmembros;
    TipoRef retorno; bool tem_retorno;
    uint8_t posse_c;      /* M6c P85: proc foreign com clausula de posse (PosseClausula); 0 = sem clausula */
    int16_t posse_param;  /* M6c P85: indice do parametro da clausula; -1 = o retorno (creates sem nome) */
    uint32_t pos_posse;   /* M6c P85: offset do NOME da clausula (ou da palavra, sem nome): onde o E0912 da semantica aponta */
    Param *retornos; uint32_t nretornos; /* v1.7: retorno.forma == TR_TUPLA; Param.padrao sempre NULL */
    Stmt *corpo;
    uint32_t pos_titulo; /* D_TEST: offset do titulo entre aspas */
    bool publico;   /* escrito 'pub' antes da declaracao (sem efeito ate o sistema de pacotes) */
    bool exportado; /* escrito 'export' (valido so em 'pub export proc'; a semantica confere): vira simbolo limpo na DLL */
    bool externo;   /* proc declarado dentro de 'foreign': corpo NULL, nome = simbolo C */
    bool contem_dono; /* [sem] D_STRUCT: algum campo e [dynamic]T ou array dele (D3) */
    bool fuga_relevante; /* [sem] D_PROC/D_TEST: algum parametro, o retorno ou algum local tem ponteiro (SemFugaTipoTemPonteiro); sem isso a fuga nao tem o que conferir */
    bool com_erro;       /* [sem] L7: o erro desta declaracao (o 1o) ja esta na lista de diagnosticos; o resto da analise a pula */
    int32_t chave;       /* [sem] IA1-L8: id internado da chave por modulo ("<L><tag>_<nome>"); = nome na raiz, no proc export e nas demais sem prefixo (sem_chave.c) */
    Decl *homonimo;      /* [sem] IA1-L8: proxima declaracao de topo de OUTRO modulo importado com o mesmo nome (Semantica.homonimos, so com 2+ modulos) */
    Decl *prox;
};
_Static_assert(sizeof(Decl) == 240, "Decl nao pode crescer (M6c P85: a clausula de posse cabe no padding)");

typedef struct { Decl *primeira, *ultima; } Programa;

#endif
