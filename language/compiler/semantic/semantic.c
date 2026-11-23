/* semantic.c - nucleo da semantica: relato de erro, tabelas iniciais e ponto de entrada.
 * Versao: 0.26.0
 * Changelog: 0.26.0 - Linguagem v1.13 L1a: import_usado tem uma linha por trecho da Fonte (max(ntrechos, qtd_modulos)); o modulo de pasta tem varios arquivos.
 *            0.25.1 - M6 P65a: nomes_locais iniciado e liberado.
 *            0.25.0 - K08c: embutido dedup (EMB_DEDUP, lista &l) registrado com o nome reservado nome_dedup.
 *            0.24.0 - K08b: embutidos sort_by_key (EMB_SORT_BY_KEY, params x e key) e binary_search (EMB_BINARY_SEARCH, params x e value) registrados.
 *            0.23.0 - K08a: embutido sort_by registrado (EMB_SORT_BY) com os nomes less e params_sort_by (fatia x, less).
 *            0.22.0 - B01a: nome_alloc (o parametro Allocator da proc do usuario).
 *            0.21.0 - IA1-L9e: SemEstatInternar e SemEstatRegistrar (8 embutidos de estatistica, vistas de map e bytes de texto; nomes reservados).
 *            0.20.0 - IA1-L9c/L9d: SemListaInternar, SemListaRegistrar e SemListaMontar (19 embutidos de lista, nomes reservados).
 *            0.19.0 - IA1-L9a/L9b: SemAritmeticaInternar e SemAritmeticaRegistrar (19 embutidos novos, nomes reservados); params_rotacao (x, n).
 *            0.18.0 - IA1-L5b: embutidos resize e sort registrados (nomes reservados); params_make[3] = len, params_append[2] = items, params_resize.
 *            0.17.0 - IA1-L5a: embutido eprint registrado (nome reservado, EMB_EPRINT).
 *            0.16.0 - IA1-L8: SemanticaIniciar chama SemChavesPreencher (Decl.chave, antes de fixar qtd_nomes).
 *            0.15.0 - IA1-L3c: nomes scope e to internados (formas novas de escopo e para).
 *            0.14.0 - IA1-b15: nomes_relatados iniciado e liberado (varios erros por corpo, sem_stmt.c).
 *            0.13.0 - IA1-b14: SemErro chama SemRelacionar (os relacionados do pacote do erro).
 *            0.12.0 - IA1-b07: SemErro do E0301 chama SemFormaDaRegra (forma por ocorrencia; sem_forma.c).
 *            0.11.0 - Linguagem v1.11 L15a: SemanticaIniciar chama SemListasFixasIniciar (nomes das structs ocultas de [dynamic, N]T).
 *            0.10.0 - Linguagem v1.11 L14a: embutido conj registrado (nome reservado) e os nomes y, z, w internados.
 *            0.9.0 -Linguagem v1.11 L13: embutidos min, max, abs, clamp e sqrt registrados (nomes reservados) e assinaturas params_minmax/params_clamp/params_um.
 *            0.8.0 -Linguagem v1.11 L10a: higienes (Vec) e import_usado iniciados; SemanticaAnalisar confere o import sem uso (E1005) depois do main, so sem outro erro.
 *            0.7.0 -Linguagem v1.11 L9: embutidos size_of, align_of, offset_of e static_assert registrados (nomes reservados).
 *            0.6.0 - Linguagem v1.11 L8: SemGuardarErro cala o erro de quem tocou o veneno; SemanticaAnalisar roda o passe B mesmo com erro no passe A (salvo pula_passe_b).
 *            0.5.0- Linguagem v1.11 L7: SemGuardarErro (lista de diagnosticos); SemanticaAnalisar ordena a lista.
 *            0.4.0 - Linguagem v1.9 P3: nomes e embutidos insert, get, get_ptr, has, remove e transpose; assinaturas params_insert/params_chave/params_transpose; mapas_em_for.
 *            0.3.3 - Linguagem v1.8 P7: rastreados (posse estatica) iniciado e liberado.
 *            0.3.2 - Linguagem v1.8 P6a: nomes escopo, id, Arena e Pool; params_delete[1] = em com padrao ficticio.
 *            0.3.1 - Linguagem v1.7 P5a: embutidos format (str) e format_c (cstr) registrados, assinatura (texto, em).
 *            0.3.0 - Linguagem v1.7 P2: nomes format, format_c e "_" internados.
 *            0.2.0 - Linguagem v1.6 P3a: nomes, assinaturas e registro dos embutidos new, make, append, delete e frame_reset.
 *            0.1.1 - Linguagem v1.3 P6: embutido len (nome reservado); assert e panic com mensagem cstr.
 *            0.1.0 - Fase 5c: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include "common/nome_modulo.h"

#include <stdarg.h>
#include <stdio.h>

bool SemErro(Semantica *s, DiagCodigo codigo, uint32_t pos, const char *fmt, ...)
{
    char causa[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(causa, sizeof causa, fmt, ap);
    va_end(ap);
    DiagRelatar(s->diag, codigo, pos, "%s", causa);
    if (codigo == E0301) SemFormaDaRegra(s); /* b07: forma da ocorrencia; quem tem forma melhor a sobrescreve logo depois */
    SemRelacionar(s); /* b14: relacionados do pacote (so nos codigos que o tem) */
    return false;
}

/* IA1-L8: a chave de simbolo de modulo ("<L><tag>_<nome>") aparece em mensagem so como o nome puro; os demais ids (titulo de test, texto) saem como estao. */
const char *SemNomeTexto(const Semantica *s, int32_t nome)
{
    const char *texto = InternerTexto(s->nomes, nome);
    return (uint32_t)nome >= s->chave_ini && (uint32_t)nome < s->chave_fim ? NomeModuloExibicao(texto) : texto;
}

static void RegistrarEmbutido(Semantica *s, int32_t nome, SimboloCategoria categoria, int32_t indice, TipoId tipo)
{
    Simbolo *y = &s->globais[nome];
    y->categoria = (uint8_t)categoria;
    y->indice = indice;
    y->tipo = tipo;
}

static void RegistrarEmbutidos(Semantica *s)
{
    RegistrarEmbutido(s, SYM_PRINT, SIM_PROC_EMBUTIDO, EMB_PRINT, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_assert, SIM_PROC_EMBUTIDO, EMB_ASSERT, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_panic, SIM_PROC_EMBUTIDO, EMB_PANIC, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_transmute, SIM_PROC_EMBUTIDO, EMB_TRANSMUTE, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_len, SIM_PROC_EMBUTIDO, EMB_LEN, TIPO_INT);
    RegistrarEmbutido(s, SYM_EXPECT, SIM_PROC_EMBUTIDO, -1, TIPO_VAZIO);
    RegistrarEmbutido(s, SYM_NEW, SIM_PROC_EMBUTIDO, EMB_NEW, TIPO_VAZIO);
    RegistrarEmbutido(s, SYM_MAKE, SIM_PROC_EMBUTIDO, EMB_MAKE, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_append, SIM_PROC_EMBUTIDO, EMB_APPEND, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_delete, SIM_PROC_EMBUTIDO, EMB_DELETE, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_frame_reset, SIM_PROC_EMBUTIDO, EMB_FRAME_RESET, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_format, SIM_PROC_EMBUTIDO, EMB_FORMAT, TIPO_STR);
    RegistrarEmbutido(s, s->nome_format_c, SIM_PROC_EMBUTIDO, EMB_FORMAT_C, TIPO_CSTR);
    RegistrarEmbutido(s, s->nome_insert, SIM_PROC_EMBUTIDO, EMB_INSERT, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_get, SIM_PROC_EMBUTIDO, EMB_GET, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_get_ptr, SIM_PROC_EMBUTIDO, EMB_GET_PTR, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_has, SIM_PROC_EMBUTIDO, EMB_HAS, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_remove, SIM_PROC_EMBUTIDO, EMB_REMOVE, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_transpose, SIM_PROC_EMBUTIDO, EMB_TRANSPOSE, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_slice_from, SIM_PROC_EMBUTIDO, EMB_SLICE_FROM, TIPO_VAZIO);
    RegistrarEmbutido(s, SYM_SIZE_OF, SIM_PROC_EMBUTIDO, EMB_SIZE_OF, TIPO_INT);
    RegistrarEmbutido(s, SYM_ALIGN_OF, SIM_PROC_EMBUTIDO, EMB_ALIGN_OF, TIPO_INT);
    RegistrarEmbutido(s, SYM_OFFSET_OF, SIM_PROC_EMBUTIDO, EMB_OFFSET_OF, TIPO_INT);
    RegistrarEmbutido(s, SYM_STATIC_ASSERT, SIM_PROC_EMBUTIDO, EMB_STATIC_ASSERT, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_min, SIM_PROC_EMBUTIDO, EMB_MIN, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_max, SIM_PROC_EMBUTIDO, EMB_MAX, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_abs, SIM_PROC_EMBUTIDO, EMB_ABS, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_clamp, SIM_PROC_EMBUTIDO, EMB_CLAMP, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_sqrt, SIM_PROC_EMBUTIDO, EMB_SQRT, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_conj, SIM_PROC_EMBUTIDO, EMB_CONJ, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_eprint, SIM_PROC_EMBUTIDO, EMB_EPRINT, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_resize, SIM_PROC_EMBUTIDO, EMB_RESIZE, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_sort, SIM_PROC_EMBUTIDO, EMB_SORT, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_sort_by, SIM_PROC_EMBUTIDO, EMB_SORT_BY, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_sort_by_key, SIM_PROC_EMBUTIDO, EMB_SORT_BY_KEY, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_binary_search, SIM_PROC_EMBUTIDO, EMB_BINARY_SEARCH, TIPO_VAZIO);
    RegistrarEmbutido(s, s->nome_dedup, SIM_PROC_EMBUTIDO, EMB_DEDUP, TIPO_VAZIO);
    SemAritmeticaRegistrar(s);
    SemListaRegistrar(s);
    SemEstatRegistrar(s);
}

/* assert(cond: bool, msg: texto = "...") e panic(msg: texto): assinaturas ficticias para o mesmo ligador de argumentos. */
static void MontarAssinaturasEmbutidas(Semantica *s)
{
    s->params_assert[0] = (Param){.nome = s->nome_cond};
    s->params_assert[0].tipo.tipo = TIPO_BOOL;
    s->params_assert[1] = (Param){.nome = s->nome_msg, .padrao = &s->padrao_msg};
    s->params_assert[1].tipo.tipo = TIPO_CSTR;
    s->params_panic[0] = (Param){.nome = s->nome_msg};
    s->params_panic[0].tipo.tipo = TIPO_CSTR;
    s->padrao_cap.tipo_no = EX_INT;
    s->padrao_cap.tipo = TIPO_INT;
    s->params_new[0] = (Param){.nome = s->nome_tipo};
    s->params_new[1] = (Param){.nome = s->nome_em};
    s->params_make[0] = (Param){.nome = s->nome_tipo};
    s->params_make[1] = (Param){.nome = s->nome_em};
    s->params_make[2] = (Param){.nome = s->nome_cap, .padrao = &s->padrao_cap};
    s->params_make[3] = (Param){.nome = s->nome_len, .padrao = &s->padrao_cap}; /* L5b: make(..., len = n): a lista nasce com n zeros */
    s->params_append[0] = (Param){.nome = s->nome_lista};
    s->params_append[1] = (Param){.nome = s->nome_item, .padrao = &s->padrao_cap}; /* L5b: item OU items; ChamarAppend exige um dos dois */
    s->params_append[2] = (Param){.nome = s->nome_items, .padrao = &s->padrao_cap};
    s->params_resize[0] = (Param){.nome = s->nome_lista};
    s->params_resize[1] = (Param){.nome = s->nome_len};
    s->params_sort_by[0] = (Param){.nome = s->nome_x};
    s->params_sort_by[1] = (Param){.nome = s->nome_less};
    s->params_sort_by_key[0] = (Param){.nome = s->nome_x};
    s->params_sort_by_key[1] = (Param){.nome = s->nome_key};
    s->params_binary_search[0] = (Param){.nome = s->nome_x};
    s->params_binary_search[1] = (Param){.nome = s->nome_value};
    s->params_delete[0] = (Param){.nome = s->nome_valor};
    s->params_delete[1] = (Param){.nome = s->nome_em, .padrao = &s->padrao_em};
    s->params_format[0] = (Param){.nome = s->nome_texto};
    s->params_format[1] = (Param){.nome = s->nome_em};
    s->padrao_value.tipo_no = EX_INT;
    s->padrao_value.tipo = TIPO_INT;
    s->params_insert[0] = (Param){.nome = s->nome_colecao};
    s->params_insert[1] = (Param){.nome = s->nome_key};
    s->params_insert[2] = (Param){.nome = s->nome_value, .padrao = &s->padrao_value};
    s->params_chave[0] = (Param){.nome = s->nome_colecao};
    s->params_chave[1] = (Param){.nome = s->nome_key};
    s->params_transpose[0] = (Param){.nome = s->nome_matriz};
    s->params_slice_from[0] = (Param){.nome = s->nome_valor};
    s->params_slice_from[1] = (Param){.nome = s->nome_len};
    s->params_minmax[0] = (Param){.nome = s->nome_a};
    s->params_minmax[1] = (Param){.nome = s->nome_b};
    s->params_clamp[0] = (Param){.nome = s->nome_x};
    s->params_clamp[1] = (Param){.nome = s->nome_lo};
    s->params_clamp[2] = (Param){.nome = s->nome_hi};
    s->params_um[0] = (Param){.nome = s->nome_x};
    s->params_rotacao[0] = (Param){.nome = s->nome_x}; /* L9b: rotate_left/right(x, n = k) */
    s->params_rotacao[1] = (Param){.nome = s->nome_n};
    SemListaMontar(s); /* IA1-L9c/L9d: as assinaturas dos 19 embutidos de lista (nome_n vem de SemListasFixasIniciar, antes) */
}

void SemanticaIniciar(Semantica *s)
{
    s->nome_assert = Internar(s->nomes, "assert", 6);
    s->nome_panic = Internar(s->nomes, "panic", 5);
    s->nome_cond = Internar(s->nomes, "cond", 4);
    s->nome_msg = Internar(s->nomes, "msg", 3);
    s->nome_transmute = Internar(s->nomes, "transmute", 9);
    s->nome_para = Internar(s->nomes, "para", 4);
    s->nome_len = Internar(s->nomes, "len", 3);
    s->nome_append = Internar(s->nomes, "append", 6);
    s->nome_delete = Internar(s->nomes, "delete", 6);
    s->nome_frame_reset = Internar(s->nomes, "frame_reset", 11);
    s->nome_em = Internar(s->nomes, "em", 2);
    s->nome_cap = Internar(s->nomes, "cap", 3);
    s->nome_item = Internar(s->nomes, "item", 4);
    s->nome_heap = Internar(s->nomes, "heap", 4);
    s->nome_frame = Internar(s->nomes, "frame", 5);
    s->nome_tipo = Internar(s->nomes, "tipo", 4);
    s->nome_lista = Internar(s->nomes, "lista", 5);
    s->nome_valor = Internar(s->nomes, "valor", 5);
    s->nome_format = Internar(s->nomes, "format", 6);
    s->nome_format_c = Internar(s->nomes, "format_c", 8);
    s->nome_descarte = Internar(s->nomes, "_", 1);
    s->nome_texto = Internar(s->nomes, "texto", 5);
    s->nome_escopo = Internar(s->nomes, "escopo", 6);
    s->nome_scope = Internar(s->nomes, "scope", 5);
    s->nome_to = Internar(s->nomes, "to", 2);
    s->nome_id = Internar(s->nomes, "id", 2);
    s->nome_arena = Internar(s->nomes, "Arena", 5);
    s->nome_pool = Internar(s->nomes, "Pool", 4);
    s->nome_alloc = Internar(s->nomes, "alloc", 5);
    s->nome_insert = Internar(s->nomes, "insert", 6);
    s->nome_get = Internar(s->nomes, "get", 3);
    s->nome_get_ptr = Internar(s->nomes, "get_ptr", 7);
    s->nome_has = Internar(s->nomes, "has", 3);
    s->nome_remove = Internar(s->nomes, "remove", 6);
    s->nome_transpose = Internar(s->nomes, "transpose", 9);
    s->nome_slice_from = Internar(s->nomes, "slice_from", 10);
    s->nome_key = Internar(s->nomes, "key", 3);
    s->nome_value = Internar(s->nomes, "value", 5);
    s->nome_colecao = Internar(s->nomes, "colecao", 7);
    s->nome_matriz = Internar(s->nomes, "matriz", 6);
    s->nome_min = Internar(s->nomes, "min", 3);
    s->nome_max = Internar(s->nomes, "max", 3);
    s->nome_abs = Internar(s->nomes, "abs", 3);
    s->nome_clamp = Internar(s->nomes, "clamp", 5);
    s->nome_sqrt = Internar(s->nomes, "sqrt", 4);
    s->nome_conj = Internar(s->nomes, "conj", 4);
    s->nome_y = Internar(s->nomes, "y", 1);
    s->nome_z = Internar(s->nomes, "z", 1);
    s->nome_w = Internar(s->nomes, "w", 1);
    s->nome_a = Internar(s->nomes, "a", 1);
    s->nome_b = Internar(s->nomes, "b", 1);
    s->nome_x = Internar(s->nomes, "x", 1);
    s->nome_lo = Internar(s->nomes, "lo", 2);
    s->nome_hi = Internar(s->nomes, "hi", 2);
    s->nome_eprint = Internar(s->nomes, "eprint", 6);
    s->nome_items = Internar(s->nomes, "items", 5);
    s->nome_resize = Internar(s->nomes, "resize", 6);
    s->nome_sort = Internar(s->nomes, "sort", 4);
    s->nome_sort_by = Internar(s->nomes, "sort_by", 7);
    s->nome_less = Internar(s->nomes, "less", 4);
    s->nome_sort_by_key = Internar(s->nomes, "sort_by_key", 11);
    s->nome_binary_search = Internar(s->nomes, "binary_search", 13);
    s->nome_dedup = Internar(s->nomes, "dedup", 5);
    SemAritmeticaInternar(s); /* IA1-L9: os 19 nomes de aritmetica com volta, bits e mul_hi_u64 (antes de qtd_nomes) */
    SemListaInternar(s);      /* IA1-L9c/L9d: os 19 nomes dos embutidos de lista e os dos parametros novos (antes de qtd_nomes) */
    SemEstatInternar(s);      /* IA1-L9e: os 8 nomes de estatistica, vistas de map e bytes de texto (antes de qtd_nomes) */
    SemListasFixasIniciar(s); /* v1.11 L15a: antes de qtd_nomes (as tabelas por id de nome cobrem os nomes das structs ocultas) */
    SemChavesPreencher(s);    /* IA1-L8: idem para as chaves por modulo (internadas aqui, uma vez) */
    s->qtd_nomes = (uint32_t)s->nomes->nomes.n;
    VecIniciar(&s->rastreados, sizeof(LocalRastreado));
    VecIniciar(&s->nomes_locais, sizeof(LocalNome));
    VecIniciar(&s->mapas_em_for, sizeof(int32_t));
    VecIniciar(&s->higienes, sizeof(SemHigiene));
    VecIniciar(&s->nomes_relatados, sizeof(int32_t));
    if (s->qtd_modulos > 1) { /* v1.13: uma linha por trecho da Fonte (arquivo), nao por modulo */
        size_t arquivos = s->fonte->ntrechos > s->qtd_modulos ? s->fonte->ntrechos : s->qtd_modulos;
        s->import_usado = ArenaAlocar(s->arena, arquivos * s->qtd_modulos);
    }
    DimIniciar(&s->dims);
    TiposIniciar(&s->tipos, s->nomes, &s->dims);
    EscopoIniciar(&s->escopo, s->arena, s->qtd_nomes);
    s->globais = ArenaAlocar(s->arena, (size_t)s->qtd_nomes * sizeof *s->globais);
    s->titulo_visto = ArenaAlocar(s->arena, s->qtd_nomes);
    SemHomonimosIniciar(s);
    RegistrarEmbutidos(s);
    MontarAssinaturasEmbutidas(s);
}

/* Posicao do modulo na ordem do driver (m->ordem: dependencias primeiro, raiz por ultimo); sem modulos importados vale o proprio indice. */
static uint32_t PosicaoDoModulo(const Semantica *s, uint8_t modulo)
{
    if (s->modulos == NULL || s->qtd_modulos <= 1) return modulo;
    for (uint32_t i = 0; i < s->qtd_modulos; i++) {
        if (s->modulos->ordem[i] == modulo) return i;
    }
    return modulo;
}

/* Diagnostico da declaracao em analise que tocou o veneno (L8) sai calado: DiagListaGuardar o descarta. */
bool SemGuardarErro(Semantica *s, uint8_t modulo)
{
    if (s->tocou_veneno && s->diag->tem) s->diag->calado = true;
    return DiagListaGuardar(&s->lista, s->diag, PosicaoDoModulo(s, modulo));
}

/* L8: com o veneno o passe B roda MESMO com erro no passe A (da mais erros independentes): declaracao com erro fica envenenada, o uso dela
 * vira TIPO_VENENO e o erro que viria disso e calado, entao nao ha cascata; o corpo de proc com assinatura invalida nao e analisado.
 * Fica de fora o que nao tem como seguir (pula_passe_b: modificadores, ciclo de struct e nome que perdeu a disputa do E0202, onde o uso
 * cairia noutro simbolo e nao no veneno) e o corte da lista; ai vale o encadeamento do L7. O main so e conferido sem erro
 * nenhum (falta de main com outro erro no arquivo seria cascata). Cada passe guarda um erro por declaracao; o diagnostico pendente no fim e
 * de fora de declaracao (modificadores, main): vai para a lista como o da raiz. */
bool SemanticaAnalisar(Semantica *s)
{
    bool ok = SemPasseA(s);
    if (!s->pula_passe_b &&!s->lista.cortado) ok = SemPasseB(s) && ok;
    if (ok) ok = SemVerificarMain(s);
    if (ok) ok = SemHigieneImports(s);
    s->tocou_veneno = false;
    if (s->diag->tem) SemGuardarErro(s, 0);
    DiagListaOrdenar(&s->lista);
    return ok;
}

void SemanticaLiberar(Semantica *s)
{
    VecLiberar(&s->rastreados);
    VecLiberar(&s->nomes_locais);
    VecLiberar(&s->mapas_em_for);
    VecLiberar(&s->higienes);
    VecLiberar(&s->nomes_relatados);
    EscopoLiberar(&s->escopo);
    TiposLiberar(&s->tipos);
    DimLiberar(&s->dims);
}
