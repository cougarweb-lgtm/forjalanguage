/* semantic.h - analise semantica da Forja: tipos, unidades, nomes, resultado/erro, fluxo, test. Anota a AST para o lowering.
 * Versao: 0.42.0
 * Changelog: 0.42.0 - Linguagem v1.13 L1a: Semantica.arquivo_atual (trecho da Fonte da declaracao em analise); import_usado vira [trecho * qtd_modulos + modulo].
 *            0.41.0 - M6e P93g: corpo_muta_colecao (o corpo em analise chama append/insert/remove/delete/clear/resize; so ele passa pelo E0913).
 *            0.40.0 - M6c P83: usa_clong, nome_clong, nome_culong e clong_campos (E0342, sem_clong.c).
 *            0.39.0 - M6c P82: alvo_lp64 (c_long/c_ulong por alvo, sem_tipo.c).
 *            0.38.0 - M6 P65a: nomes_locais (Vec de LocalNome do proc em analise; vira Decl.nomes_locais).
 *            0.37.0 - M5 P48 (S18): tem_nao_copiavel (marca de std:sync/std:atomic, sem_naocopia.c).
 *            0.36.0 - F4: simbolo_c_decl (o 1o proc foreign de cada simbolo C; o mesmo simbolo em varios modulos exige assinatura identica).
 *            0.35.0 - S4 P1: modulo_testado (o forja test liga os modulos de origem arquivo; so o driver preenche) e titulo unico por modulo (ver sem_decl.c).
 *            0.34.0 - K17: SEM_LISTA_PARAMS 33 -> 40 (ASS_HEAP_PUSH, ASS_HEAP_PUSH_BY e ASS_HEAP_POP_BY em sem_lista.h).
 *            0.33.0 - K08c: nome_dedup (dedup(&l), remove duplicatas consecutivas; sem_lista.c).
 *            0.32.0 - S2b: SEM_LISTA_PARAMS 31 -> 33 (ASS_SORTEIO: shuffle(s, rng = &r) e choice(s, rng = &r); o rng usa o padrao ficticio padrao_cap).
 *            0.31.0 - S2a: SEM_LISTA_PARAMS 29 -> 31 (ASS_COLECAO_ALLOC: reverse(str, alloc = A) e reverse de lista com alloc vazio).
 *            0.30.0 - K08b: nome_sort_by_key, nome_binary_search, params_sort_by_key e params_binary_search (sort_by_key(s, key = ...), binary_search(s, value = ...)).
 *            0.29.0 - K08a: comparador_sort_by (flag do EX_NOME de sort_by que pula o E0909 da fronteira C).
 *            0.28.0 - K08a: nome_sort_by, nome_less e params_sort_by (sort_by(s, less = p)).
 *            0.27.0 - B01c: alocador_lendo (a leitura do nome do parametro Allocator so vale no rotulo alloc =).
 *            0.26.0 - B01a: tipo_allocator, decl_allocator, nome_alloc e alocador_permitido (o tipo embutido Allocator).
 *            0.25.0 - IA1-L9e: SEM_LISTA_PARAMS 24 -> 29 (assinaturas de keys/values, from_bytes e as_bytes).
 *            0.24.0 - IA1-L9c/L9d: params_lista[SEM_LISTA_PARAMS] (assinaturas dos 19 embutidos de lista, montadas por SemListaMontar).
 *            0.23.0 - IA1-L9b: params_rotacao (rotate_left/right).
 *            0.22.0 - IA1-L5b: nome_items, nome_resize, nome_sort, params_resize; params_make[4] (len) e params_append[3] (items).
 *            0.21.0 - IA1-L5a: nome_eprint.
 *            0.20.0 - IA1-L8: globais por chave (Decl.chave), homonimos, chave_ini/chave_fim; simbolo_c_usado por modulo.
 *            0.19.0 - IA1-L3c: nome_to e nome_scope (formas novas de para e escopo; tabela de renomeados).
 *            0.18.0 - IA1-b15: corpo_com_erro (o corpo em analise teve erro guardado e seguiu) e nomes_relatados (nomes sem declaracao ja relatados no corpo: o uso seguinte e cascata calada).
 *            0.17.0 - IA1-b07: ContextoDoTipo e Semantica.ctx_tipo (onde a coercao esta sendo pedida: declaracao, atribuicao, argumento, retorno, campo; alimenta a forma por ocorrencia do E0301).
 *            0.16.0 - IA1-b04: SemHigiene.sug (a sugestao do achado de higiene, copiada para o Diag quando o achado vira erro).
 *            0.15.0 - Linguagem v1.12 M3 P21v-a: usa_proc_valor, pos_proc_valor, valor_para_foreign, adia_assinatura_proc e prof_tipo (proc como valor); EX_CHAMADA_IND (tipo = retorno do proc chamado).
 *            0.14.0 -Linguagem v1.11 L15a: LISTAS_FIXAS_MAX, nomes_listas_fixas, decls_listas_fixas, tipos_listas_fixas, qtd_listas_fixas, nome_itens e nome_n (struct oculta de [dynamic, N]T).
 *            0.13.0 - Linguagem v1.11 L14a: nome_conj, nome_y, nome_z, nome_w, tipo_quat e decl_quat.
 *            0.12.0 -Linguagem v1.11 L13: nomes dos embutidos min, max, abs, clamp e sqrt e dos parametros a, b, x, lo, hi; params_minmax, params_clamp e params_um.
 *            0.11.0 -Linguagem v1.11 L10a: SemHigiene, Semantica.higiene, higienes e import_usado (higiene: var nunca alterada, let nunca lido, retorno descartado, import sem uso).
 *            0.10.0 -Linguagem v1.11 L9: Semantica.usa_layout.
 *            0.9.0 - Linguagem v1.11 L8: Simbolo.envenenado, Semantica.tocou_veneno e pula_passe_b (veneno e cascata calada, sem_veneno.c).
 *            0.8.0- Linguagem v1.11 L7: Semantica.lista (DiagLista): o passe A e o passe B guardam o 1o erro de cada declaracao e seguem; SemanticaAnalisar true = lista vazia.
 *            0.7.0 - Linguagem v1.11 L3: corpo_tem_ponteiro (estado do corpo em analise; vira Decl.fuga_relevante).
 *            0.6.0 -Linguagem v1.9 P3: nomes dos embutidos insert, get, get_ptr, has, remove, transpose e dos parametros key, value, colecao, matriz; params_insert/params_chave/params_transpose; padrao_value; mapas_em_for.
 *            0.5.0 - Linguagem v1.8b P2: campos tem_opaco, sob_ponteiro, tem_struct_aninhada, abi_foreign, tem_externo, tipo_proc_permitido (zerados).
 *            0.4.2 - Linguagem v1.8 P7: let_em_analise, rastreados e marca_laco (escopo e posse estatica).
 *            0.4.1 - Linguagem v1.8 P6a: tipo_arena, tipo_pool, nome_escopo/id/arena/pool, padrao_em e params_delete[2] (em do delete).
 *            0.4.0 - Linguagem v1.8 P3: modulos, qtd_modulos (entrada) e modulo_atual (estado) para a visibilidade entre modulos.
 *            0.3.1- Linguagem v1.7 P5a: params_format e nome_texto (embutidos format/format_c).
 *            0.3.0 - Linguagem v1.7 P2: SIM_UNIAO; nome_format, nome_format_c, nome_descarte e qtd_uniao.
 *            0.2.0 - Linguagem v1.6 P3a: nomes e assinaturas de new, make, append, delete e frame_reset; padrao_cap.
 *            0.1.1 - Linguagem v1.3 P6: nome_len; comentario de TIPOS descreve str e cstr.
 *            0.1.0 - Fase 5c: passe A por categoria (unit, enum, const, struct, assinaturas de proc), passe B nos corpos,
 *                    proc main por ultimo. Para no 1o erro (Diag). Regra de caixa/acento atras de SEM_VERIFICA_CAIXA.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 *
 * ORDEM DE ANALISE (subconjunto.txt secao 5 + adendo 1.0.1): 1) passe A, cada categoria na ordem do fonte: unit, enum, const,
 * struct, assinaturas de proc; 2) passe B: corpos de proc e test na ordem do fonte; 3) proc main existe (E0201 em 1:1).
 *
 * ANOTACOES NA AST (o lowering le tudo isto; campos [sem] de ast.h):
 *  - Todo Expr: tipo (TipoId), flexivel (arvore so de literais numericos; o tipo veio do contexto).
 *  - EX_NOME: ref = REF_LOCAL ou REF_PARAM (ref_id = numero do local no proc, parametros = 0..nparams-1) | REF_CONST (ref_id =
 *    Decl.indice da const) | REF_ENUM (so como base de EX_CAMPO; ref_id = Decl.indice do enum).
 *  - EX_CAMPO: ref = REF_CAMPO (ref_id = indice do campo na struct; a base e struct ou ^struct) | REF_MEMBRO (ref_id = indice
 *    do membro; a base e EX_NOME com REF_ENUM; tipo = o enum).
 *  - EX_CHAMADA: ref = REF_PROC (ref_id = Decl.indice do proc) | REF_EMBUTIDO (ref_id = EmbutidoId); Arg.indice = indice do
 *    parametro ligado (print: 0). tipo = retorno do proc (TIPO_VAZIO se nao devolve nada; pode ser resultado).
 *  - EX_LITERAL_STRUCT: ref = REF_STRUCT (ref_id = Decl.indice da struct); Arg.indice = indice do campo.
 *  - EX_UNARIO '&': tipo = ^struct. EX_CONVERSAO: tipo = o tipo-alvo. EX_TRY: tipo = valor do resultado (TIPO_VAZIO em '! E').
 *    EX_OR_ELSE: tipo = valor do resultado da esquerda.
 *  - Stmt let/var/for: local_id (numero do local) e tipo_local. Caso: tipo, indice_membro, local_id (um local por case quando ha
 *    ligacao 'switch r in ...'; o tipo dele e Caso.tipo). Stmt.nome continua sendo o nome da ligacao.
 *  - Decl: indice (por categoria: const, struct, enum, proc; unit e test = 0), nlocais (D_PROC e D_TEST), retorno.tipo (o tipo
 *    de retorno inteiro: TIPO_VAZIO sem retorno; resultado em 'T ! E' e '! E'); Param.tipo.tipo e Campo.tipo.tipo.
 *  - Numeracao dos locais: por proc/test, 0.. na ordem em que a analise os declara (parametros primeiro), SEM reuso entre
 *    blocos irmaos; nlocais = total.
 * TIPOS (tipos.h): igualdade de tipo = igualdade de TipoId. unidade e f32 com dimensao (TC_REAL); unidades de mesma dimensao
 * sao o mesmo tipo. Texto: str (TIPO_TEXTO, UTF-8 imutavel) e cstr (TIPO_CSTR, para o C); o literal ".." adota o tipo do contexto (cstr, senao str).
 */
#ifndef FORJA_SEMANTIC_H
#define FORJA_SEMANTIC_H

#include "common/diag.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "frontend/ast.h"
#include "frontend/modulos.h"
#include "semantic/dimensao.h"
#include "semantic/escopo.h"
#include "semantic/tipos.h"

/* Liga a regra de caixa/acento dos nomes (ADR-33). Fica ligada se os 51 casos dourados passam com ela. */
enum { SEM_VERIFICA_CAIXA = 1 };

/* v1.11 L15a: quantos tipos [dynamic, N]T diferentes um programa pode usar (cada um e uma struct oculta com nome pre-internado). */
enum { LISTAS_FIXAS_MAX = 64, SEM_LISTA_PARAMS = 40 }; /* SEM_LISTA_PARAMS: IA1-L9c/L9d (L9e: 24 -> 29; S2a: 29 -> 31; S2b: 31 -> 33; K17: 33 -> 40), tamanho de params_lista (sem_lista.c) */

typedef enum {
    SIM_NENHUM, SIM_PROC_EMBUTIDO, SIM_UNIDADE, SIM_CONST, SIM_STRUCT, SIM_ENUM, SIM_PROC,
    SIM_UNIAO /* v1.7 */
} SimboloCategoria;
/* i32, int, f32 e bool NAO estao na tabela: sao resolvidos por id fixo (SemResolverNomeTipo), entao um proc pode se chamar
 * f32 (o corpus gerado tem 'proc f32'): tipo e proc nunca se confundem porque o lugar de uso diz qual dos dois e. */

/* Nome global (tipo, unidade, const, struct, enum, proc), indexado pelo id do nome. */
typedef struct {
    uint8_t categoria; /* SimboloCategoria */
    int32_t indice;    /* Decl.indice; EmbutidoId nos embutidos */
    Decl *decl;
    TipoId tipo;       /* tipo da const, da struct, do enum ou da unidade (TIPO_VENENO: declaracao sem tipo usavel) */
    uint8_t envenenado; /* L8: a declaracao teve erro; o uso dela vira TIPO_VENENO e o erro que vier disso e calado (sem_veneno.c) */
} Simbolo;

/* v1.11 L10a: achado de higiene (sem_higiene.c): o 1o do proc (o de menor posicao) fica pendente ate o proc ter sido conferido inteiro. */
typedef struct { bool tem; DiagCodigo codigo; uint32_t pos; Decl *decl; char causa[240]; DiagSugestoes sug; } SemHigiene;

/* IA1-b07: onde a coercao do valor esta sendo pedida (so alimenta a forma por ocorrencia do E0301). 'valor' diz a que expressao o contexto vale: SemCoagirLento
 * ignora o contexto de outra expressao (um contexto velho nunca vaza). nome = variavel, campo ou parametro; dono = proc chamado, proc do return ou struct do literal (-1 = nenhum). */
typedef enum { CTX_NENHUM, CTX_DECLARACAO, CTX_ATRIBUICAO, CTX_ARGUMENTO, CTX_RETORNO, CTX_CAMPO } ContextoTipoOnde;
typedef struct { uint8_t onde; const Expr *valor; int32_t nome, dono; } ContextoDoTipo;

typedef struct {
    /* entrada */
    const Fonte *fonte;
    Interner *nomes;
    Arena *arena;
    Diag *diag;
    DiagLista lista;        /* v1.11 L7: erros guardados (teto = --max-erros, o driver preenche antes de SemanticaAnalisar); saida: ordenada */
    const Programa *programa;
    const Modulos *modulos;  /* v1.8: NULL (ou qtd_modulos <= 1) em quem nao e o driver */
    uint32_t qtd_modulos;
    /* tabelas */
    DimTabela dims;
    Tipos tipos;
    Escopo escopo;
    Simbolo *globais;       /* por id de CHAVE (IA1-L8): o id do nome na raiz, no export e nos embutidos; "<L><tag>_<nome>" nos simbolos de modulo importado */
    Decl **homonimos;       /* IA1-L8: por id de nome, a cadeia (Decl.homonimo) das declaracoes de topo de modulos importados com esse nome (NULL com 1 modulo) */
    uint32_t chave_ini, chave_fim; /* IA1-L8: faixa de ids que sao chaves com prefixo (SemNomeTexto devolve so o nome puro delas) */
    uint8_t *titulo_visto;  /* por id de texto: titulo de test ja usado */
    uint8_t *simbolo_c_usado; /* v1.10 (A6): [modulo * qtd_nomes + id do simbolo]; 1 = simbolo C de um proc foreign com alias naquele modulo (NULL ate o 1o alias) */
    const Decl **simbolo_c_decl; /* F4: por id do simbolo C, o 1o proc foreign que o declarou (NULL ate o 1o foreign) */
    uint32_t qtd_nomes;
    uint32_t qtd_const, qtd_struct, qtd_enum, qtd_proc, qtd_uniao;
    int32_t nome_assert, nome_panic, nome_cond, nome_msg, nome_transmute, nome_para, nome_len;
    int32_t nome_append, nome_delete, nome_frame_reset, nome_em, nome_cap, nome_item, nome_heap, nome_frame;
    int32_t nome_tipo, nome_lista, nome_valor; /* v1.6: embutidos de memoria e seus parametros */
    int32_t nome_format, nome_format_c, nome_descarte; /* v1.7: format, format_c e '_' */
    Param params_assert[2], params_panic[1]; /* assinaturas dos embutidos assert(cond, msg = "") e panic(msg) */
    Param params_new[2], params_make[4], params_append[3], params_delete[2]; /* v1.6: new, make, append, delete (v1.8: delete ganha em; L5b: make ganha len, append ganha items) */
    TipoId tipo_arena, tipo_pool;            /* v1.8: structs Arena e Pool de std:mem (0 = std:mem nao importado) */
    bool tem_nao_copiavel;                   /* v1.12 S18: std:sync ou std:atomic com marca (Mutex, Cond, Atomic*) importado: liga a conferencia de copia (sem_naocopia.c) */
    int32_t nome_escopo, nome_id, nome_arena, nome_pool; /* v1.8: 'escopo', campo 'id' e os nomes das structs de std:mem */
    int32_t nome_to, nome_scope;             /* L3c: as formas novas (tabela de renomeados) de 'para' e 'escopo' */
    Expr padrao_em;                          /* padrao ficticio do 'em' do delete (so para dizer "tem padrao") */
    Param params_format[2];                  /* v1.7: format e format_c (texto, em) */
    int32_t nome_texto;                      /* v1.7: parametro 'texto' de format/format_c */
    Expr padrao_msg;                         /* padrao ficticio de msg (so para dizer "tem padrao") */
    Expr padrao_cap;                         /* padrao ficticio de cap (literal int 0) */
    bool tocou_veneno;      /* L8: a declaracao em analise tocou TIPO_VENENO/simbolo envenenado: o erro dela (o 1o) sera calado */
    bool pula_passe_b;      /* L8: o passe A parou sem condicao de seguir (modificadores, ciclo de struct) ou deixou um nome sem simbolo envenenado (colisao E0202): o passe B nao roda */
    /* estado do proc/test em analise */
    const Decl *proc;
    int32_t prox_local;
    uint32_t prof_laco;
    bool em_teste, em_defer;
    bool corpo_muta_colecao; /* M6e P93g: o corpo em analise chama embutido que muda lista/map (SemEmbutidoMutaColecao); so ele pode ter E0913 */
    bool corpo_tem_ponteiro; /* v1.11 L3: parametro, retorno ou local com ponteiro no corpo em analise (vira Decl.fuga_relevante) */
    TipoId erro_do_proc;    /* enum E do retorno 'T ! E' / '! E' do proc; 0 se o retorno nao e resultado */
    uint8_t modulo_atual;   /* v1.8: modulo (Decl.modulo) da declaracao em analise; v1.13: o pacote, que pode ter varios arquivos */
    uint32_t arquivo_atual; /* v1.13: trecho da Fonte do arquivo da declaracao em analise (FonteTrechoDoOffset de Decl.pos); os imports e seus apelidos sao por arquivo */
    const Expr *let_em_analise; /* v1.8: valor do let/var em analise (so ele aceita em = escopo) */
    Vec nomes_locais;       /* M6 P65a: LocalNome por local_id do proc em analise (parametros primeiro; copiado para Decl.nomes_locais) */
    Vec rastreados;         /* v1.8: LocalRastreado (sem_interno.h) dos let/var com heap/escopo do proc em analise */
    uint32_t marca_laco;    /* v1.8: rastreados.n na entrada do laco mais interno (break/continue conferem a partir dela) */
    bool tem_opaco, sob_ponteiro, tem_struct_aninhada, abi_foreign, tem_externo; /* v1.8b: usados de P3a em diante */
    bool alvo_lp64;         /* M6c P82: alvo com long de 64 bits (x86_64-linux); o driver preenche depois de SemanticaIniciar; c_long/c_ulong */
    bool usa_clong;         /* M6c P83: o programa cita c_long ou c_ulong (sem_clong.c confere o E0342 so entao; programa sem c_long nao paga) */
    int32_t nome_clong, nome_culong; /* M6c P83: ids de c_long e c_ulong (InternerAchar; -1 = o programa nao os escreve) */
    uint8_t *clong_campos;  /* M6c P83: por id de nome de campo, 1 + tipo de fronteira (CKind) do campo nas structs; 0 = sem campo, 0xFF = structs discordam */
    const TipoRef *tipo_proc_permitido;      /* v1.8b: o TipoRef do parametro de proc foreign onde proc(...) vale */
    int32_t nome_insert, nome_get, nome_get_ptr, nome_has, nome_remove, nome_transpose; /* v1.9: embutidos */
    int32_t nome_key, nome_value, nome_colecao, nome_matriz;                               /* v1.9: parametros deles */
    Param params_insert[3], params_chave[2], params_transpose[1];                          /* v1.9 */
    int32_t nome_slice_from;                                                               /* slice_from(p, len = n): embutido de fronteira C */
    Param params_slice_from[2];
    int32_t nome_min, nome_max, nome_abs, nome_clamp, nome_sqrt;                           /* v1.11 L13: embutidos de matematica */
    int32_t nome_a, nome_b, nome_x, nome_lo, nome_hi;                                      /* v1.11 L13: parametros deles */
    Param params_minmax[2], params_clamp[3], params_um[1];                                 /* v1.11 L13: min/max (a, b), clamp (x, lo, hi), abs/sqrt (x) */
    int32_t nome_conj, nome_y, nome_z, nome_w;                                             /* v1.11 L14a: embutido conj e os campos y, z, w do quat */
    int32_t nome_eprint;                                                                   /* IA1-L5a: embutido eprint (print para stderr) */
    int32_t nome_items, nome_resize, nome_sort, nome_sort_by, nome_less; /* K08a: sort_by(s, less = p) */                                            /* IA1-L5b: append(items =), resize e sort */
    int32_t nome_sort_by_key, nome_binary_search; /* K08b: sort_by_key(s, key = Struct.campo) e binary_search(s, value = v) (key e value: nome_key e nome_value) */
    int32_t nome_dedup; /* K08c: dedup(&l) (sem_lista.c, lower_lista.c) */
    Param params_resize[2], params_sort_by[2], params_sort_by_key[2], params_binary_search[2];                                                                /* IA1-L5b: resize(&lista, len = n) */
    Param params_rotacao[2];                                                               /* IA1-L9b: rotate_left/right(x, n = k) */
    Param params_lista[SEM_LISTA_PARAMS];                                                  /* IA1-L9c/L9d: assinaturas dos embutidos de lista, montadas em sem_lista.c (SemListaMontar) */
    TipoId tipo_quat;   /* v1.11 L14a: TipoId do quat (0 = o programa nao usa quat; registrado no 1o uso por SemQuatGarantir) */
    Decl *decl_quat;    /* v1.11 L14a: a struct oculta {x, y, z, w: f32} (fora da lista do programa; o lowering a registra) */
    TipoId tipo_allocator;                   /* B01a: TipoId do Allocator (0 = o programa nao usa Allocator; registrado no 1o uso por SemAllocatorGarantir) */
    Decl *decl_allocator;                    /* B01a: a struct oculta {id: u32} (fora da lista do programa; o lowering a registra) */
    int32_t nome_alloc;                      /* B01a: o nome 'alloc' (o parametro Allocator da proc do usuario) */
    const TipoRef *alocador_permitido;       /* B01a: o TipoRef do parametro onde Allocator vale (ParamDoProc) */
    bool alocador_lendo;                     /* B01c: ligado so enquanto 'alloc = alloc' (rotulo de chamada) le o nome do parametro Allocator; fora disso ler o nome e E0336 */
    /* v1.11 L15a: '[dynamic, N]T' = struct oculta {itens: [N]T, n: int}, uma por tipo, fora da lista do programa (o lowering as registra).
     * Os nomes "0lista_fixa_<k>" nascem todos em SemanticaIniciar (as tabelas por id de nome tem o tamanho de qtd_nomes). */
    int32_t nome_itens, nome_n;
    int32_t nomes_listas_fixas[LISTAS_FIXAS_MAX];
    Decl *decls_listas_fixas[LISTAS_FIXAS_MAX];
    TipoId tipos_listas_fixas[LISTAS_FIXAS_MAX];
    uint32_t qtd_listas_fixas;
    Expr padrao_value;  /* v1.9: padrao ficticio do value (so para dizer "opcional"; o bit_set nao o tem) */
    Vec mapas_em_for;   /* v1.9: int32_t, nome do map de cada for de map aberto (D12); vazio fora de for de map */
    /* v1.12 P21v-a: proc como valor. usa_proc_valor = o programa tem tipo proc fora do parametro de foreign, nome de proc fora de callback ou chamada
     * indireta (a IR so chega no P21v-b: o lowering recusa, pos_proc_valor aponta o 1o uso); valor_para_foreign = o EX_NOME que a semantica checa agora
     * e argumento direto de proc foreign (e o callback de sempre); adia_assinatura_proc = campo de struct em resolucao (E0909 depois, quando as structs
     * ja tem campos); prof_tipo = profundidade de SemResolverTipoRef (proc so vale no topo). */
    bool usa_proc_valor, valor_para_foreign, adia_assinatura_proc;
    bool comparador_sort_by; /* K08a: o EX_NOME que a semantica checa agora e o comparador de sort_by (so ele pula o E0909 da fronteira C; o str por valor nao cruza C) */
    uint32_t pos_proc_valor, prof_tipo;
    bool usa_layout;    /* v1.11 L9: o programa usa size_of, align_of, offset_of ou static_assert: o valor sai no lowering (o check o baixa e descarta) */
    ContextoDoTipo ctx_tipo; /* IA1-b07: onde a coercao do valor esta sendo pedida (forma do E0301) */
    SemHigiene higiene; /* v1.11 L10a: achado pendente do corpo em analise */
    Vec higienes;       /* v1.11 L10a: SemHigiene dos corpos sem erro, relatados depois da fuga (so os que continuam sem erro) */
    bool corpo_com_erro;  /* IA1-b15: o corpo em analise teve erro guardado na lista e a analise seguiu (SemRecuperar); o corpo nao entra na higiene nem na fuga */
    Vec nomes_relatados;  /* IA1-b15: int32_t, nomes sem declaracao ja relatados (E0201) no corpo em analise: o uso seguinte e cascata calada (SemNomeJaRelatado) */
    uint8_t *import_usado; /* v1.11 L10a: [arquivo (trecho da Fonte; 1 se sem trechos) * qtd_modulos + modulo] = 1 se o arquivo usou 'apelido.nome' desse modulo (NULL com 1 modulo) */
    bool modulo_testado[MODULOS_MAX]; /* S4 P1: modulo cujos 'test' entram no lowering do forja test (so RodarTest liga; zerado no build, check, run e dll) */
} Semantica;

/* Entrada (o chamador preenche, como no Lexer e no Parser): fonte, nomes, arena, diag, programa. Prepara as tabelas e interna
 * os nomes embutidos; chamar depois do parser e uma vez so. */
void SemanticaIniciar(Semantica *s);

/* Analisa o programa inteiro. false = diagnosticos em s->lista, ja ordenados (lista vazia = falha interna, sem diagnostico). */
bool SemanticaAnalisar(Semantica *s);

void SemanticaLiberar(Semantica *s);

/* IA1-L8: simbolo que 'nome' (escrito com o qualificador 'apelido.' do modulo 'qualificador', 0 = nenhum) designa num uso do modulo 'modulo_do_uso'. Vale depois da analise (o
 * forja ast e o forja consultar resolvem os usos assim). Nunca devolve NULL; sem declaracao o simbolo vem com categoria SIM_NENHUM. */
const Simbolo *SemAcharSimboloEm(const Semantica *s, int32_t nome, uint8_t qualificador, uint8_t modulo_do_uso);

#endif
