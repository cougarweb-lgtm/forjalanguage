/* sem_lista.c - embutidos da lista dona (IA1-L9c, K01 e K02): cap, clear, reserve, truncate, pop, insert_at, ordered_remove, unordered_remove, remove_value.
 * Sintaxe (argumento posicional na ordem, ou nomeado em ordem crescente, como append e resize):
 *   cap(l) -> int                         l: [dynamic]T ou ^[dynamic]T (a capacidade atual)
 *   clear(&l)                             len = 0, mantem a capacidade
 *   reserve(&l, cap = n)                  garante capacidade >= n sem mudar o len
 *   truncate(&l, len = n)                 encolhe para n; n > len ou n < 0 = erro de execucao (R0006 em dev)
 *   pop(&l) -> ?T                         tira o ultimo; lista vazia = none
 *   insert_at(&l, i = k, item = x)        i em 0..len (i = len acrescenta); desloca os de i em diante; fora = R0006 em dev
 *   ordered_remove(&l, i = k)             tira o elemento k e fecha o buraco mantendo a ordem; fora = R0006 em dev
 *   unordered_remove(&l, i = k)           tira o elemento k trocando-o pelo ultimo (O(1)); fora = R0006 em dev
 *   remove_value(&l, value = v) -> bool   tira a 1a ocorrencia (str por conteudo); false se nao ha
 *   dedup(&l)                             tira as duplicatas CONSECUTIVAS, mantendo a 1a de cada corrida (lista ordenada nao e exigida)
 * l e o ENDERECO de uma lista var (^[dynamic]T). Elemento dono de memoria so entra (insert_at, como append); tirar, esvaziar e encolher pedem elemento sem dono (E0311).
 * Interface (sem_interno.h): SemListaInternar, SemListaRegistrar, SemListaMontar e SemChamarLista (ChamarPorFaixa em sem_call.c). O abaixamento mora em ir\lower_lista.c.
 *   heap_push(&l, v) e heap_pop(&l) -> ?T; heap_push_by(&l, v, less = p) e heap_pop_by(&l, less = p): fila de prioridade (min-heap pela ordem do sort, ou pelo less).
 * Versao: 0.8.0
 * Changelog: 0.8.0 - K15: set_union, set_intersect e set_diff (EMB_SET_*, SET_NOMES; ChamarConjuntoMapa: map[K]bool, '&a' var via SemConferirClearMapa, E0310/E0318/E0301).
 *            0.7.0 - K09: index_by, find_by, any_by, all_by e count_by (EMB_INDEX_BY..EMB_COUNT_BY; ChamarBuscaPorPred, pred = proc (^T) -> bool, E0310 da colecao, E0907 do pred, E0311 do find_by); "pred" em PARAMS_NOVOS.
 *            0.6.1 - K17 (Assess): heap_pop de elemento ponteiro devolve ?^T (TipoAnulavel), igual ao pop e ao choice; antes saia ?(^T).
 *            0.6.0 - K17: heap_push, heap_pop, heap_push_by e heap_pop_by (EMB_HEAP_*, fora da faixa de EMB_CAP..EMB_REPLACE_VALUE, como os de S2b); sem less so int, real ou str (E0337);
 *            tirar dono = E0311 (antes do E0337); less confere como o do sort_by (E0907; str com comparador_sort_by).
 *            0.5.0 - K08c: dedup(&l) (EMB_DEDUP, SemChamarDedup): tira as duplicatas consecutivas; elemento dono = E0311 antes do E0301.
 *            0.4.0 - S2b: shuffle e choice (EMB_SHUFFLE, EMB_CHOICE) internados e registrados aqui (SORTEIO_NOMES); params_lista ganha ASS_SORTEIO (colecao, rng) e o nome rng.
 *            0.3.0 - S2a: params_lista ganha ASS_COLECAO_ALLOC (reverse com alloc opcional na tabela).
 *            0.2.0 - IA1-L9e: clear(&m) de map (SemChamarClearMapa); ListaJaChecadaPorEndereco; params_lista ganha ASS_MAPA, ASS_BYTES e ASS_TEXTO (e os nomes m, bytes, s).
 *            0.1.0 - IA1-L9c/L9d: primeira versao (tambem registra os embutidos de sem_lista_busca.c e monta params_lista).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

#include <stdio.h>
#include <string.h>

/* Na ordem de EMB_CAP..EMB_REPLACE_VALUE (ast.h). */
static const char *const NOMES[] = {
    "cap", "clear", "reserve", "truncate", "pop", "insert_at", "ordered_remove", "unordered_remove", "remove_value",
    "index_of", "last_index_of", "contains", "count", "copy", "fill", "clone", "swap", "reverse", "replace_value",
};
enum { NOMES_QTD = sizeof NOMES / sizeof NOMES[0] };

_Static_assert(NOMES_QTD == EMB_REPLACE_VALUE - EMB_CAP + 1, "NOMES precisa seguir EmbutidoId");

/* Nomes dos parametros que nenhum outro embutido internou. */
static const char *const PARAMS_NOVOS[] = {"i", "j", "dst", "src", "old", "new", "alloc", "m", "bytes", "s", "rng", "v", "pred"};

/* S2b: embutidos fora da faixa de EMB_CAP..EMB_REPLACE_VALUE (ids no fim de EmbutidoId, ast.h); registrados junto com a lista. */
static const struct { const char *nome; int32_t id; } SORTEIO_NOMES[] = {{"shuffle", EMB_SHUFFLE}, {"choice", EMB_CHOICE}};

/* K17: heap_push, heap_pop e as versoes _by (ids depois de EMB_DEDUP, ast.h). */
static const struct { const char *nome; int32_t id; } HEAP_NOMES[] = {
    {"heap_push", EMB_HEAP_PUSH}, {"heap_pop", EMB_HEAP_POP}, {"heap_push_by", EMB_HEAP_PUSH_BY}, {"heap_pop_by", EMB_HEAP_POP_BY}};

/* K09: index_by, find_by, any_by, all_by e count_by (ids depois de EMB_HEAP_POP_BY, ast.h). */
static const struct { const char *nome; int32_t id; } PRED_NOMES[] = {
    {"index_by", EMB_INDEX_BY}, {"find_by", EMB_FIND_BY}, {"any_by", EMB_ANY_BY}, {"all_by", EMB_ALL_BY}, {"count_by", EMB_COUNT_BY}};

/* K15: set_union, set_intersect e set_diff sobre map[K]bool (ids depois de EMB_COUNT_BY, ast.h). */
static const struct { const char *nome; int32_t id; } SET_NOMES[] = {
    {"set_union", EMB_SET_UNION}, {"set_intersect", EMB_SET_INTERSECT}, {"set_diff", EMB_SET_DIFF}};

static int32_t NomeInterno(Semantica *s, const char *texto)
{
    return Internar(s->nomes, texto, (uint32_t)strlen(texto));
}

void SemListaInternar(Semantica *s)
{
    for (size_t k = 0; k < NOMES_QTD; k++) NomeInterno(s, NOMES[k]);
    for (size_t k = 0; k < sizeof PARAMS_NOVOS / sizeof PARAMS_NOVOS[0]; k++) NomeInterno(s, PARAMS_NOVOS[k]);
    for (size_t k = 0; k < sizeof SORTEIO_NOMES / sizeof SORTEIO_NOMES[0]; k++) NomeInterno(s, SORTEIO_NOMES[k].nome);
    for (size_t k = 0; k < sizeof HEAP_NOMES / sizeof HEAP_NOMES[0]; k++) NomeInterno(s, HEAP_NOMES[k].nome);
    for (size_t k = 0; k < sizeof PRED_NOMES / sizeof PRED_NOMES[0]; k++) NomeInterno(s, PRED_NOMES[k].nome);
    for (size_t k = 0; k < sizeof SET_NOMES / sizeof SET_NOMES[0]; k++) NomeInterno(s, SET_NOMES[k].nome);
}

/* O Internar de um nome ja internado devolve o mesmo id (SemListaInternar o criou antes de fixar qtd_nomes). */
void SemListaRegistrar(Semantica *s)
{
    for (int k = 0; k < NOMES_QTD; k++) {
        Simbolo *y = &s->globais[NomeInterno(s, NOMES[k])];
        y->categoria = (uint8_t)SIM_PROC_EMBUTIDO;
        y->indice = EMB_CAP + k;
        y->tipo = TIPO_VAZIO;
    }
    for (size_t k = 0; k < sizeof SORTEIO_NOMES / sizeof SORTEIO_NOMES[0]; k++) {
        Simbolo *y = &s->globais[NomeInterno(s, SORTEIO_NOMES[k].nome)];
        y->categoria = (uint8_t)SIM_PROC_EMBUTIDO;
        y->indice = SORTEIO_NOMES[k].id;
        y->tipo = TIPO_VAZIO;
    }
    for (size_t k = 0; k < sizeof HEAP_NOMES / sizeof HEAP_NOMES[0]; k++) {
        Simbolo *y = &s->globais[NomeInterno(s, HEAP_NOMES[k].nome)];
        y->categoria = (uint8_t)SIM_PROC_EMBUTIDO;
        y->indice = HEAP_NOMES[k].id;
        y->tipo = TIPO_VAZIO;
    }
    for (size_t k = 0; k < sizeof PRED_NOMES / sizeof PRED_NOMES[0]; k++) {
        Simbolo *y = &s->globais[NomeInterno(s, PRED_NOMES[k].nome)];
        y->categoria = (uint8_t)SIM_PROC_EMBUTIDO;
        y->indice = PRED_NOMES[k].id;
        y->tipo = TIPO_VAZIO;
    }
    for (size_t k = 0; k < sizeof SET_NOMES / sizeof SET_NOMES[0]; k++) {
        Simbolo *y = &s->globais[NomeInterno(s, SET_NOMES[k].nome)];
        y->categoria = (uint8_t)SIM_PROC_EMBUTIDO;
        y->indice = SET_NOMES[k].id;
        y->tipo = TIPO_VAZIO;
    }
}

/* params_lista: cada assinatura na posicao ASS_*; so os nomes importam (o tipo de cada argumento e conferido pelos tratadores). */
void SemListaMontar(Semantica *s)
{
    Param *p = s->params_lista;
    int32_t i = NomeInterno(s, "i"), j = NomeInterno(s, "j"), dst = NomeInterno(s, "dst"), src = NomeInterno(s, "src");
    int32_t antigo = NomeInterno(s, "old"), novo = NomeInterno(s, "new");

    p[ASS_LISTA] = (Param){.nome = s->nome_lista};
    p[ASS_RESERVE] = (Param){.nome = s->nome_lista};
    p[ASS_RESERVE + 1] = (Param){.nome = s->nome_cap};
    p[ASS_INDICE] = (Param){.nome = s->nome_lista};
    p[ASS_INDICE + 1] = (Param){.nome = i};
    p[ASS_INSERT_AT] = (Param){.nome = s->nome_lista};
    p[ASS_INSERT_AT + 1] = (Param){.nome = i};
    p[ASS_INSERT_AT + 2] = (Param){.nome = s->nome_item};
    p[ASS_LISTA_VALOR] = (Param){.nome = s->nome_lista};
    p[ASS_LISTA_VALOR + 1] = (Param){.nome = s->nome_value};
    p[ASS_COLECAO] = (Param){.nome = s->nome_colecao};
    p[ASS_COLECAO_VALOR] = (Param){.nome = s->nome_colecao};
    p[ASS_COLECAO_VALOR + 1] = (Param){.nome = s->nome_value};
    p[ASS_COPY] = (Param){.nome = dst};
    p[ASS_COPY + 1] = (Param){.nome = src};
    p[ASS_CLONE] = (Param){.nome = s->nome_colecao};
    p[ASS_CLONE + 1] = (Param){.nome = NomeInterno(s, "alloc")}; /* o rotulo ja na forma nova: o clone nunca teve `em`, entao nao entra na tabela de renomeados */
    p[ASS_SWAP] = (Param){.nome = s->nome_colecao};
    p[ASS_SWAP + 1] = (Param){.nome = i};
    p[ASS_SWAP + 2] = (Param){.nome = j};
    p[ASS_REPLACE] = (Param){.nome = s->nome_colecao};
    p[ASS_REPLACE + 1] = (Param){.nome = antigo};
    p[ASS_REPLACE + 2] = (Param){.nome = novo};
    p[ASS_REPLACE + 3] = (Param){.nome = s->nome_n, .padrao = &s->padrao_cap}; /* n e opcional: o padrao so diz "pode faltar" (o lowering le NULL) */
    p[ASS_MAPA] = (Param){.nome = NomeInterno(s, "m")};
    p[ASS_MAPA + 1] = (Param){.nome = NomeInterno(s, "alloc")};
    p[ASS_BYTES] = (Param){.nome = NomeInterno(s, "bytes")};
    p[ASS_BYTES + 1] = (Param){.nome = NomeInterno(s, "alloc")};
    p[ASS_TEXTO] = (Param){.nome = NomeInterno(s, "s")};
    p[ASS_COLECAO_ALLOC] = (Param){.nome = s->nome_colecao};
    p[ASS_COLECAO_ALLOC + 1] = (Param){.nome = NomeInterno(s, "alloc"), .padrao = &s->padrao_cap}; /* opcional na tabela: reverse de str exige (E0503 no tratador); lista nao aceita */
    p[ASS_SORTEIO] = (Param){.nome = s->nome_colecao};
    p[ASS_SORTEIO + 1] = (Param){.nome = NomeInterno(s, "rng"), .padrao = &s->padrao_cap}; /* opcional na tabela: a falta vira E0338 no tratador (sem_sorteio.c), nao E0503 */
    p[ASS_HEAP_PUSH] = (Param){.nome = s->nome_lista};
    p[ASS_HEAP_PUSH + 1] = (Param){.nome = NomeInterno(s, "v")};
    p[ASS_HEAP_PUSH_BY] = (Param){.nome = s->nome_lista};
    p[ASS_HEAP_PUSH_BY + 1] = (Param){.nome = NomeInterno(s, "v")};
    p[ASS_HEAP_PUSH_BY + 2] = (Param){.nome = s->nome_less};
    p[ASS_HEAP_POP_BY] = (Param){.nome = s->nome_lista};
    p[ASS_HEAP_POP_BY + 1] = (Param){.nome = s->nome_less};
}

/* ---------- conferencias comuns ---------- */

bool SemListaInteiro(Semantica *s, Expr *v, const char *papel)
{
    bool negativo;
    uint64_t magnitude;
    if (!SemChecar(s, v, TIPO_INT)) return false;
    if (!TipoEhInteiro(v->tipo)) return SemErroDoInteiro(s, v, papel);
    if (SemInteiroConstante(s, v, &negativo, &magnitude) && negativo && magnitude != 0) {
        return SemErro(s, E0421, v->pos, "%s negativo (-%llu)", papel, (unsigned long long)magnitude);
    }
    return true;
}

bool SemListaValorDoElemento(Semantica *s, Expr *v, TipoId elemento)
{
    return SemChecar(s, v, elemento) && SemCoagir(s, v, elemento, v->pos);
}

bool SemListaComparavel(Semantica *s, const Expr *e, TipoId elemento)
{
    char texto[96];
    if (TipoEhInteiro(elemento) || TipoEhReal(&s->tipos, elemento) || elemento == TIPO_BOOL || elemento == TIPO_TEXTO ||
        TipoClasseDe(&s->tipos, elemento) == TC_ENUM) {
        return true;
    }
    SemDescrever(s, elemento, texto, sizeof texto);
    return SemErro(s, E0301, e->pos_op, "%s pede elemento comparavel com == (inteiro, real, bool, str ou enum); %s nao tem ==: compare campo a campo num laco",
                   SemNomeTexto(s, e->nome), texto);
}

bool SemListaSemDono(Semantica *s, const Expr *e, TipoId elemento, const char *efeito)
{
    if (!SemTipoEhDono(s, elemento)) return true;
    return SemErro(s, E0311, e->pos_op, "%s %s elementos que sao donos de memoria (a memoria deles se perderia ou ficaria com dois donos); trate cada dono com delete antes",
                   SemNomeTexto(s, e->nome), efeito);
}

/* ^[dynamic]T? */
static bool PonteiroDeLista(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    return k->classe == TC_PONTEIRO && TipoClasseDe(&s->tipos, k->a) == TC_DINAMICO;
}

/* ^map[K]V? (clear(&m), L9e) */
static bool PonteiroDeMapa(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    return k->classe == TC_PONTEIRO && TipoClasseDe(&s->tipos, k->a) == TC_MAPA;
}

/* O 1o argumento (ja checado) dos que alteram a lista: '&l' (^[dynamic]T) com l var; *elemento = T. */
static bool ListaJaChecadaPorEndereco(Semantica *s, Expr *e, TipoId *elemento)
{
    char texto[96], motivo[96];
    const char *quem = SemNomeTexto(s, e->nome);
    Expr *lista = ArgumentoDoParametro(e, 0)->valor;
    if (!PonteiroDeLista(s, lista->tipo)) {
        SemDescrever(s, lista->tipo, texto, sizeof texto);
        if (TipoClasseDe(&s->tipos, lista->tipo) == TC_DINAMICO) return SemErro(s, E0310, lista->pos, "%s recebe o endereco da lista: %s(&lista, ...)", quem, quem);
        return SemErro(s, E0310, lista->pos, "%s pede ^[dynamic]T (tipo %s)", quem, texto);
    }
    snprintf(motivo, sizeof motivo, "%s altera a lista: declare com var", quem);
    if (lista->tipo_no == EX_UNARIO && lista->op == OP_ENDERECO && !SemLvalorMutavelDe(s, lista->a, motivo)) return false;
    *elemento = TipoDe(&s->tipos, (TipoId)TipoDe(&s->tipos, lista->tipo)->a)->a;
    return true;
}

/* 1o argumento dos que alteram a lista: checa e confere. */
static bool ListaPorEndereco(Semantica *s, Expr *e, TipoId *elemento)
{
    return SemChecar(s, ArgumentoDoParametro(e, 0)->valor, TIPO_VAZIO) && ListaJaChecadaPorEndereco(s, e, elemento);
}

/* ---------- os tratadores ---------- */

static bool ChamarCap(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_LISTA], 1)) return false;
    Expr *lista = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, lista, TIPO_VAZIO)) return false;
    if (TipoClasseDe(&s->tipos, lista->tipo) != TC_DINAMICO && !PonteiroDeLista(s, lista->tipo)) {
        SemDescrever(s, lista->tipo, texto, sizeof texto);
        return SemErro(s, E0310, lista->pos, "cap pede [dynamic]T ou ^[dynamic]T (tipo %s)", texto);
    }
    e->tipo = TIPO_INT;
    return true;
}

/* clear(&l) e pop(&l): so a lista; clear solta os elementos, pop devolve o ultimo (?T, ponteiro vira ?^X como no get de map). */
static bool ChamarClearPop(Semantica *s, Expr *e, int32_t embutido)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_LISTA], 1)) return false;
    Expr *alvo = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, alvo, TIPO_VAZIO)) return false;
    if (embutido == EMB_CLEAR && (PonteiroDeMapa(s, alvo->tipo) || TipoClasseDe(&s->tipos, alvo->tipo) == TC_MAPA)) return SemChamarClearMapa(s, e, alvo);
    if (!ListaJaChecadaPorEndereco(s, e, &elemento)) return false;
    if (!SemListaSemDono(s, e, elemento, embutido == EMB_CLEAR ? "esvaziaria" : "tiraria")) return false;
    if (embutido == EMB_CLEAR) {
        e->tipo = TIPO_VAZIO;
        return true;
    }
    bool ponteiro = TipoClasseDe(&s->tipos, elemento) == TC_PONTEIRO;
    e->tipo = ponteiro ? TipoAnulavel(&s->tipos, (TipoId)TipoDe(&s->tipos, elemento)->a) : TipoOpcional(&s->tipos, elemento);
    return true;
}

/* reserve(&l, cap = n) e truncate(&l, len = n): um inteiro no 2o argumento. */
static bool ChamarReserveTruncate(Semantica *s, Expr *e, int32_t embutido)
{
    TipoId elemento;
    bool reserva = embutido == EMB_RESERVE;
    const Param *assinatura = reserva ? &s->params_lista[ASS_RESERVE] : s->params_resize;
    if (!SemLigarEmbutido(s, e, assinatura, 2) || !ListaPorEndereco(s, e, &elemento)) return false;
    if (!reserva && !SemListaSemDono(s, e, elemento, "soltaria")) return false;
    if (!SemListaInteiro(s, ArgumentoDoParametro(e, 1)->valor, reserva ? "cap" : "len")) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* insert_at(&l, i = k, item = x): x do tipo do elemento, entra por movimento e e ponto de guarda (como o item do append). */
static bool ChamarInsertAt(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_INSERT_AT], 3) || !ListaPorEndereco(s, e, &elemento)) return false;
    if (!SemListaInteiro(s, ArgumentoDoParametro(e, 1)->valor, "o indice")) return false;
    Expr *item = ArgumentoDoParametro(e, 2)->valor;
    if (!SemListaValorDoElemento(s, item, elemento)) return false;
    if (!SemConferirMovimento(s, item) || !SemConferirPosse(s, item, PONTO_GUARDA)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* ordered_remove(&l, i = k) e unordered_remove(&l, i = k). */
static bool ChamarRemoveEm(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_INDICE], 2) || !ListaPorEndereco(s, e, &elemento)) return false;
    if (!SemListaSemDono(s, e, elemento, "tiraria")) return false;
    if (!SemListaInteiro(s, ArgumentoDoParametro(e, 1)->valor, "o indice")) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* remove_value(&l, value = v) -> bool. */
static bool ChamarRemoveValue(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_LISTA_VALOR], 2) || !ListaPorEndereco(s, e, &elemento)) return false;
    if (!SemListaComparavel(s, e, elemento) || !SemListaSemDono(s, e, elemento, "tiraria")) return false;
    if (!SemListaValorDoElemento(s, ArgumentoDoParametro(e, 1)->valor, elemento)) return false;
    e->tipo = TIPO_BOOL;
    return true;
}

/* K08c: dedup(&l): lista var de elemento sem dono e comparavel com == (real: NaN nao se funde, como contains).
 * O dono vem antes do comparavel: dono nunca e comparavel, e a mensagem certa para [dynamic]T e E0311. */
bool SemChamarDedup(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_LISTA], 1) || !ListaPorEndereco(s, e, &elemento)) return false;
    if (!SemListaSemDono(s, e, elemento, "removeria")) return false;
    if (!SemListaComparavel(s, e, elemento)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* K17: sem less, o elemento precisa de ordem propria do runtime (int, real ou str); enum, struct, bool e dono nao a tem: E0337. */
static bool HeapOrdenavelSemLess(Semantica *s, const Expr *e, TipoId elemento)
{
    char texto[96];
    if (SemEhOrdenavelDoRuntime(elemento)) return true;
    SemDescrever(s, elemento, texto, sizeof texto);
    return SemErro(s, E0337, e->pos_op, "%s sem less: o elemento nao tem ordem propria (tipo %s); so inteiro, f32, f64 e str. Use %s_by(&l, less = p)",
                   SemNomeTexto(s, e->nome), texto, SemNomeTexto(s, e->nome));
}

/* K17: less = p dos heap_*_by: proc (^T, ^T) -> bool, conferido como o do sort_by (E0907 da assinatura; str pula o E0909 so no comparador). */
static bool ConferirLessDoHeap(Semantica *s, Expr *p, TipoId elemento)
{
    TipoId params[2], comparador;
    ProcAssinatura assinatura = {params, NULL, 2, TIPO_BOOL};
    params[0] = TipoPonteiro(&s->tipos, elemento);
    params[1] = params[0];
    comparador = TipoProc(&s->tipos, &assinatura);
    s->comparador_sort_by = true;
    bool ok_comparador = SemChecar(s, p, comparador);
    s->comparador_sort_by = false;
    return ok_comparador && SemCoagir(s, p, comparador, p->pos);
}

/* K17: heap_push(&l, v) e heap_push_by(&l, v, less = p): lista var; o valor entra por movimento e e ponto de guarda (como insert_at); dono entra. */
static bool ChamarHeapPush(Semantica *s, Expr *e, bool com_less)
{
    TipoId elemento;
    const Param *assinatura = com_less ? &s->params_lista[ASS_HEAP_PUSH_BY] : &s->params_lista[ASS_HEAP_PUSH];
    if (!SemLigarEmbutido(s, e, assinatura, com_less ? 3 : 2) || !ListaPorEndereco(s, e, &elemento)) return false;
    if (com_less) {
        if (!ConferirLessDoHeap(s, ArgumentoDoParametro(e, 2)->valor, elemento)) return false;
    } else if (!HeapOrdenavelSemLess(s, e, elemento)) {
        return false;
    }
    Expr *item = ArgumentoDoParametro(e, 1)->valor;
    if (!SemListaValorDoElemento(s, item, elemento)) return false;
    if (!SemConferirMovimento(s, item) || !SemConferirPosse(s, item, PONTO_GUARDA)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* K17: heap_pop(&l) -> ?T e heap_pop_by(&l, less = p): tirar um dono e E0311 (antes do E0337); lista vazia nao da erro (none). */
static bool ChamarHeapPop(Semantica *s, Expr *e, bool com_less)
{
    TipoId elemento;
    const Param *assinatura = com_less ? &s->params_lista[ASS_HEAP_POP_BY] : &s->params_lista[ASS_LISTA];
    if (!SemLigarEmbutido(s, e, assinatura, com_less ? 2 : 1) || !ListaPorEndereco(s, e, &elemento)) return false;
    if (!SemListaSemDono(s, e, elemento, "tiraria")) return false;
    if (com_less) {
        if (!ConferirLessDoHeap(s, ArgumentoDoParametro(e, 1)->valor, elemento)) return false;
    } else if (!HeapOrdenavelSemLess(s, e, elemento)) {
        return false;
    }
    bool ponteiro = TipoClasseDe(&s->tipos, elemento) == TC_PONTEIRO;
    e->tipo = ponteiro ? TipoAnulavel(&s->tipos, (TipoId)TipoDe(&s->tipos, elemento)->a) : TipoOpcional(&s->tipos, elemento);
    return true;
}

bool SemChamarHeapPush(Semantica *s, Expr *e) { return ChamarHeapPush(s, e, false); }
bool SemChamarHeapPushBy(Semantica *s, Expr *e) { return ChamarHeapPush(s, e, true); }
bool SemChamarHeapPop(Semantica *s, Expr *e) { return ChamarHeapPop(s, e, false); }
bool SemChamarHeapPopBy(Semantica *s, Expr *e) { return ChamarHeapPop(s, e, true); }

/* K09: pred = p das buscas por predicado: proc (^T) -> bool, conferido como o less do heap (E0907 da assinatura; comparador_sort_by so pula o E0909 do str). */
static bool ConferirPredDaBusca(Semantica *s, Expr *p, TipoId elemento)
{
    TipoId params[1], predicado;
    ProcAssinatura assinatura = {params, NULL, 1, TIPO_BOOL};
    params[0] = TipoPonteiro(&s->tipos, elemento);
    predicado = TipoProc(&s->tipos, &assinatura);
    s->comparador_sort_by = true;
    bool ok_predicado = SemChecar(s, p, predicado);
    s->comparador_sort_by = false;
    return ok_predicado && SemCoagir(s, p, predicado, p->pos);
}

/* K09: index_by(s, pred = p) e as demais: s e fatia de T (lista e array entram como l[..], como o sort_by); outra colecao = E0310. Devolve T em *elemento. */
static bool ConferirBuscaPorPred(Semantica *s, Expr *e, TipoId *elemento)
{
    char texto[96];
    Param params[2] = {{.nome = NomeInterno(s, "x")}, {.nome = NomeInterno(s, "pred")}};
    if (!SemLigarEmbutido(s, e, params, 2)) return false;
    Expr *v = ArgumentoDoParametro(e, 0)->valor, *p = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, v, TIPO_VAZIO)) return false;
    *elemento = TipoClasseDe(&s->tipos, v->tipo) == TC_FATIA ? (TipoId)TipoDe(&s->tipos, v->tipo)->a : 0;
    if (*elemento == 0) {
        SemDescrever(s, v->tipo, texto, sizeof texto);
        return SemErro(s, E0310, v->pos, "%s pede uma fatia: %s(l[..], pred = p); recebeu %s", SemNomeTexto(s, e->nome), SemNomeTexto(s, e->nome), texto);
    }
    return ConferirPredDaBusca(s, p, *elemento);
}

/* K09: index_by -> (int, bool) (indice da 1a que satisfaz pred, -1 se nenhuma; e se achou); find_by -> ?T (T copiado: dono = E0311); any_by, all_by -> bool; count_by -> int. */
static bool ChamarBuscaPorPred(Semantica *s, Expr *e, int32_t embutido)
{
    TipoId elemento, tipos[2] = {TIPO_INT, TIPO_BOOL};
    if (!ConferirBuscaPorPred(s, e, &elemento)) return false;
    if (embutido == EMB_FIND_BY && !SemListaSemDono(s, e, elemento, "copiaria")) return false;
    switch (embutido) {
    case EMB_INDEX_BY: e->tipo = TipoTupla(&s->tipos, tipos, 2); break;
    case EMB_FIND_BY: e->tipo = TipoClasseDe(&s->tipos, elemento) == TC_PONTEIRO ? TipoAnulavel(&s->tipos, (TipoId)TipoDe(&s->tipos, elemento)->a) : TipoOpcional(&s->tipos, elemento); break;
    case EMB_COUNT_BY: e->tipo = TIPO_INT; break;
    default: e->tipo = TIPO_BOOL; break;
    }
    return true;
}

bool SemChamarIndexBy(Semantica *s, Expr *e) { return ChamarBuscaPorPred(s, e, EMB_INDEX_BY); }
bool SemChamarFindBy(Semantica *s, Expr *e) { return ChamarBuscaPorPred(s, e, EMB_FIND_BY); }
bool SemChamarAnyBy(Semantica *s, Expr *e) { return ChamarBuscaPorPred(s, e, EMB_ANY_BY); }
bool SemChamarAllBy(Semantica *s, Expr *e) { return ChamarBuscaPorPred(s, e, EMB_ALL_BY); }
bool SemChamarCountBy(Semantica *s, Expr *e) { return ChamarBuscaPorPred(s, e, EMB_COUNT_BY); }

/* K15: o map de um map ou ^map que e map[K]bool; 0 se outra coisa (bit_set incluso). */
static TipoId MapaBoolDoConjunto(Semantica *s, TipoId t)
{
    TipoId m = TipoClasseDe(&s->tipos, t) == TC_PONTEIRO ? (TipoId)TipoDe(&s->tipos, t)->a : t;
    if (TipoClasseDe(&s->tipos, m) != TC_MAPA || TipoDe(&s->tipos, m)->b != TIPO_BOOL) return 0;
    return m;
}

/* K15: set_union, set_intersect e set_diff(&a, b) sobre map[K]bool (presenca da chave = membro). a e '&x' de map var, fora do for (SemConferirClearMapa);
 * b e map ou ^map do mesmo tipo. Colecao que nao e map[K]bool = E0310; a sem '&' = E0318; b de outro tipo = E0301 (ou E0310 se nao e map[K]bool). */
static bool ChamarConjuntoMapa(Semantica *s, Expr *e)
{
    char texto[96];
    Param params[2] = {{.nome = NomeInterno(s, "dst")}, {.nome = NomeInterno(s, "src")}};
    if (!SemLigarEmbutido(s, e, params, 2)) return false;
    Expr *a = ArgumentoDoParametro(e, 0)->valor, *b = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, a, TIPO_VAZIO) || !SemChecar(s, b, TIPO_VAZIO)) return false;
    TipoId mapa = MapaBoolDoConjunto(s, a->tipo), outro = MapaBoolDoConjunto(s, b->tipo);
    if (mapa == 0) {
        SemDescrever(s, a->tipo, texto, sizeof texto);
        return SemErro(s, E0310, a->pos, "%s pede map[K]bool (presenca = membro): recebeu %s", SemNomeTexto(s, e->nome), texto);
    }
    if (TipoClasseDe(&s->tipos, a->tipo) != TC_PONTEIRO) {
        return SemErro(s, E0318, a->pos, "%s recebe o endereco do map: %s(&a, b)", SemNomeTexto(s, e->nome), SemNomeTexto(s, e->nome));
    }
    if (!SemConferirClearMapa(s, a)) return false;
    if (outro != mapa) {
        SemDescrever(s, b->tipo, texto, sizeof texto);
        return SemErro(s, outro == 0 ? E0310 : E0301, b->pos, "%s pede o mesmo map[K]bool do 1o argumento (recebeu %s)", SemNomeTexto(s, e->nome), texto);
    }
    e->tipo = TIPO_VAZIO;
    return true;
}

bool SemChamarSetUnion(Semantica *s, Expr *e) { return ChamarConjuntoMapa(s, e); }
bool SemChamarSetIntersect(Semantica *s, Expr *e) { return ChamarConjuntoMapa(s, e); }
bool SemChamarSetDiff(Semantica *s, Expr *e) { return ChamarConjuntoMapa(s, e); }

bool SemChamarLista(Semantica *s, Expr *e, int32_t embutido)
{
    switch (embutido) {
    case EMB_CAP: return ChamarCap(s, e);
    case EMB_CLEAR:
    case EMB_POP: return ChamarClearPop(s, e, embutido);
    case EMB_RESERVE:
    case EMB_TRUNCATE: return ChamarReserveTruncate(s, e, embutido);
    case EMB_INSERT_AT: return ChamarInsertAt(s, e);
    case EMB_ORDERED_REMOVE:
    case EMB_UNORDERED_REMOVE: return ChamarRemoveEm(s, e);
    case EMB_REMOVE_VALUE: return ChamarRemoveValue(s, e);
    default: return SemChamarBusca(s, e, embutido);
    }
}
