/* sem_memoria.c - memoria explicita da v1.6: quem e dono (lista) e a regra de que dono nao se copia (D3).
 * Versao: 0.17.0
 * Changelog: 0.17.0 - K17: SemEhOrdenavelDoRuntime deixa de ser static (sem_lista.c usa no heap); mesmo texto.
 *            0.16.0 - K08b: sort_by_key (SemChamarSortByKey: chave Struct.campo ligada como offset_of, E0310 da colecao, E0337 da chave e do campo) e binary_search (SemChamarBinarySearch: tupla (int, bool), E0337 do elemento).
 *            0.15.0 - K08a: sort_by(s, less = p) (SemChamarSortBy): fatia de qualquer T, comparador proc (^T, ^T) -> bool conferido por SemChecar + SemCoagir (E0907 da assinatura, E0310 da colecao).
 *            0.14.1 - B01g: o E0310 do 'alloc =' no delete diz tambem que delete nao aceita o alloc de um Allocator.
 *            0.14.0 - B01c: AlocacaoDoNome vira SemAlocacaoDoNome (publica; o conversor do chamador a usa); EmDeAlocador liga alocador_lendo ao ler o nome.
 *            0.13.0 - B01b: EmDeAlocador aceita o valor Allocator ('alloc = alloc' dentro de proc com 'alloc: Allocator'): alocacao ALOC_PARAM e o valor vira o .id do parametro.
 *            0.12.0 - IA1-L9f: append(&buf, item = "texto") em [dynamic]u8 vira os bytes do texto (SemTextoNoBuilder, sem_texto_builder.c); inclui sem_lista.h.
 *            0.11.0 - IA1-L5b: append(&l, items = fatia), make(..., len = n), resize(&l, len = n) e sort(fatia) (SemChamarResize, SemChamarSort).
 *            0.10.0 - IA1-L4: SemTipoEhDono vale para 'T ! E' com dono em T (L-01).
 *            0.9.0 - IA1-L3d1: ConferirValorAntigo registra o valor antigo (escopo) em Diag.trocas para o forja migrar.
 *            0.8.0 - IA1-L3c: 'alloc = scope' (a forma nova de em = escopo; 'scope' so e a palavra se nenhum nome declarado o usa) e flag recusar da tabela de renomeados (ConferirValorAntigo).
 *            0.7.2 - IA1-b08: o E0311 de copia de dono leva a forma da ocorrencia (SemFormaDoDono: return, item de append, copia).
 *            0.7.1 - IA1-b06: o E0310 de append sem endereco leva a sugestao '&lista' (SemSugerirEnderecoDoAppend).
 *            0.7.0- Linguagem v1.11 L15a: append aceita ^[dynamic, N]T; make(...) e delete(l) de [dynamic, N]T = E0335.
 *            0.6.0 (P10) - map: make aceita map, E0311 de copia com texto de tabela, delete(^map) = E0310, delete dentro do for do map = E0318.
 *            0.6.0 - Linguagem v1.9 P7: soa dinamica e dona (SemTipoEhDono, SemColecaoDona, E0311 de copia), make/append/delete de soa dinamica (E0320 em make de soa[N]S).
 *            0.5.1 - Linguagem v1.8 P7: em = escopo so direto no let/var (E0316); item do append e ponto de posse (guarda).
 *            0.5.0 - Linguagem v1.8 P6a: 'em' aceita escopo (provisorio E0101 ate o P7), Arena e Pool; SemConferirAlocador grava a alocacao e troca o valor por .id; delete(p, em = pool).
 *            0.4.0 - Linguagem v1.7 P10: versao de fechamento da v1.7 (sem mudanca de logica).
 *            0.3.3 - Linguagem v1.7 P5a: ConferirAlocador vira SemConferirAlocador; delete aceita str, cstr e ?cstr (literal = E0310).
 *            0.3.2 - Linguagem v1.7 P2: append coage o item (SemCoagir); SemConferirMovimentoDono aceita EX_EMBRULHA.
 *            0.3.1 - Linguagem v1.6 P4 (desempenho): SemConferirMovimento vira o inline de sem_interno.h (atalho sem lista) e esta
 *                     funcao passa a se chamar SemConferirMovimentoDono.
 *            0.3.0 - Linguagem v1.6 P3b: ChamarAppend, ChamarDelete, ChamarFrameReset e o despacho completo (sai o E0101 provisorio).
 *            0.2.0 - Linguagem v1.6 P3a: ChamarNew, ChamarMake, ConferirAlocador, SemTipoSolto e o despacho SemChamarMemoria.
 *            0.1.0 - Linguagem v1.6 P2: SemTipoEhDono e SemConferirMovimento.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

#include "common/renomeados.h"

/* [dynamic]T, array de dono e struct com campo dono sao donos; ponteiro, fatia e anulavel so veem a memoria. */
bool SemTipoEhDono(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe == TC_DINAMICO) return true;
    if (k->classe == TC_ARRAY) return SemTipoEhDono(s, k->a);
    if (k->classe == TC_STRUCT) return s->globais[k->a].decl->contem_dono;
    if (k->classe == TC_RESULTADO) return SemTipoEhDono(s, k->a); /* L-01: 'T ! E' com dono em T e dono (se move; copia e E0311) */
    if (__builtin_expect(k->classe >= TC_MAPA, 0)) return k->classe == TC_MAPA || (k->classe == TC_SOA && TipoSoaDinamica(&s->tipos, t));
    return false;
}

/* [dynamic]T, map e soa[dynamic]S: as colecoes que o delete libera (v1.9). */
bool SemColecaoDona(const Semantica *s, TipoId t)
{
    TipoClasse c = TipoClasseDe(&s->tipos, t);
    return c == TC_DINAMICO || c == TC_MAPA || (c == TC_SOA && TipoSoaDinamica(&s->tipos, t));
}

/* Copia de dono: lvalor (nome, campo, elemento, p^) e E0311; valor novo (chamada, literal, try) se move. */
bool SemConferirMovimentoDono(Semantica *s, const Expr *e)
{
    char tipo[96];
    if (e->tipo_no == EX_EMBRULHA) return true; /* o embrulhado nunca e dono (D2/D7) */
    if (!SemTipoEhDono(s, e->tipo)) return true;
    if (e->tipo_no == EX_OR_ELSE) return SemConferirMovimentoDono(s, e->b);
    if (e->tipo_no == EX_NOME || e->tipo_no == EX_CAMPO || e->tipo_no == EX_INDICE ||
        (e->tipo_no == EX_UNARIO && e->op == OP_DEREF)) {
        SemDescrever(s, e->tipo, tipo, sizeof tipo);
        if (__builtin_expect(TipoClasseDe(&s->tipos, e->tipo) == TC_MAPA, 0)) {
            SemErro(s, E0311, e->pos, "copiar %s criaria dois donos da mesma tabela: use &m (^map[K]V) para alterar ou ler", tipo);
        } else if (__builtin_expect(TipoClasseDe(&s->tipos, e->tipo) == TC_SOA, 0)) {
            SemErro(s, E0311, e->pos, "copiar %s criaria dois donos das mesmas colunas: use &a (^soa[dynamic]S)", tipo);
        } else {
            SemErro(s, E0311, e->pos, "copiar %s criaria duas donas da mesma memoria: use &x (^[dynamic]T) para alterar ou x[..] para emprestar", tipo);
        }
        SemFormaDoDono(s, DONO_COPIA, e->pos, e->tipo); /* b08: forma da ocorrencia (return, item de append e copia); a sugestao do let/var vem do comando */
        return false;
    }
    return true;
}

/* ---------- new e make (P3a) ---------- */

static bool EmNaoDeclarado(const Semantica *s, const Expr *v);

/* heap, frame e escopo sao nomes contextuais do 'alloc' (D6): nunca procurados como variavel. L3c: 'scope' (a forma nova de escopo, tabela de renomeados) so vale como a palavra
 * quando nenhuma variavel ou global se chama scope; se ha uma, o nome e dela (Arena ou Pool) e o resto segue como antes. */
uint8_t SemAlocacaoDoNome(const Semantica *s, const Expr *v)
{
    if (v->tipo_no != EX_NOME) return ALOC_NENHUM;
    if (v->nome == s->nome_heap) return ALOC_HEAP;
    if (v->nome == s->nome_frame) return ALOC_FRAME;
    if (v->nome == s->nome_escopo) return ALOC_ESCOPO;
    return v->nome == s->nome_scope && EmNaoDeclarado(s, v) ? ALOC_ESCOPO : ALOC_NENHUM;
}

/* L3f: 'escopo' escrito na forma ANTIGA e E0110 quando a entrada da tabela tem 'recusar' (hoje false: vale em silencio). */
static bool ConferirValorAntigo(Semantica *s, const Expr *v)
{
    const Renomeado *r = v->nome == s->nome_escopo ? RenomeadoBuscar(REN_VALOR_ALOCADOR, "escopo") : NULL;
    if (r == NULL) return true;
    RenomeadoNotarTabela(s->diag, v->pos, InternerTam(s->nomes, v->nome), r);
    if (!RenomeadoRecusa(r)) return true;
    return RenomeadoRecusar(s->diag, v->pos, InternerTam(s->nomes, v->nome), r);
}

/* Apontado de ^T; o proprio tipo nos demais (Arena e ^Arena valem igual no 'em'). */
static TipoId BaseDoAlocador(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    return k->classe == TC_PONTEIRO ? (TipoId)k->a : t;
}

/* Troca o valor do 'em' pelo campo .id (u32) da Arena/Pool: o lowering le o handle direto. */
static Expr *TrocarPorIdCriar(Semantica *s, Arg *em)
{
    Expr *base = em->valor;
    Expr *campo = ArenaAlocar(s->arena, sizeof *campo);
    campo->tipo_no = EX_CAMPO;
    campo->a = base;
    campo->nome = s->nome_id;
    campo->pos = campo->pos_op = base->pos;
    em->valor = campo;
    return campo;
}

static bool TrocarPorId(Semantica *s, Arg *em)
{
    return SemChecar(s, TrocarPorIdCriar(s, em), TIPO_VAZIO);
}

/* B01b: 'alloc = alloc' (o parametro Allocator do proc). O .id e do compilador, nao do usuario: SemChecar do campo daria E0336 (ler .id), entao
 * o campo nasce resolvido (id e o campo 0, u32) sem passar pela leitura de campo. */
static bool TrocarPorIdDoParametro(Semantica *s, Arg *em)
{
    Expr *campo = TrocarPorIdCriar(s, em);
    campo->ref = REF_CAMPO;
    campo->ref_id = 0;
    campo->tipo = TIPO_U32;
    return true;
}

/* Nome simples que nao e local nem global: o E0310 do 'em' diz que nao esta declarado (em vez de um E02xx). */
static bool EmNaoDeclarado(const Semantica *s, const Expr *v)
{
    return v->tipo_no == EX_NOME && v->modulo == 0 && EscopoBuscar(&s->escopo, v->nome) == NULL &&
           SemAcharSimbolo(s, v->nome, 0)->categoria == SIM_NENHUM;
}

static bool ErroEmDesconhecido(Semantica *s, const Expr *v)
{
    char texto[96];
    if (EmNaoDeclarado(s, v)) snprintf(texto, sizeof texto, "'%s' (nao declarado)", SemNomeTexto(s, v->nome));
    else SemDescrever(s, v->tipo, texto, sizeof texto);
    return SemErro(s, E0310, v->pos, "alloc recebe heap, frame, scope, Arena ou Pool (import \"std:mem\"); recebeu %s", texto);
}

/* 'em' com valor de Arena/Pool (ou ^Arena/^Pool): grava a alocacao e troca o valor pelo handle .id. */
static bool EmDeAlocador(Semantica *s, Expr *chamada, Arg *em)
{
    Expr *v = em->valor;
    TipoId base;
    s->alocador_lendo = v->tipo_no == EX_NOME; /* B01c: 'alloc = alloc' e a unica leitura do nome Allocator que vale */
    bool lido = SemChecar(s, v, TIPO_VAZIO);
    s->alocador_lendo = false;
    if (!lido) return false;
    base = BaseDoAlocador(s, v->tipo);
    if (SemEhAllocator(s, v->tipo)) { /* B01b: o valor e o parametro 'alloc: Allocator'; ^Allocator nao existe (E0336 na declaracao) */
        chamada->alocacao = ALOC_PARAM;
        return TrocarPorIdDoParametro(s, em);
    }
    if (s->tipo_arena != 0 && base == s->tipo_arena) chamada->alocacao = ALOC_ARENA;
    else if (s->tipo_pool != 0 && base == s->tipo_pool) chamada->alocacao = ALOC_POOL;
    else return ErroEmDesconhecido(s, v);
    if (chamada->alocacao == ALOC_POOL && chamada->ref_id != EMB_NEW) {
        return SemErro(s, E0310, v->pos, "Pool so serve new: make, format e format_c pedem heap, frame, scope ou Arena");
    }
    return TrocarPorId(s, em);
}

/* 'em' de new, make, format e format_c (D6): grava chamada->alocacao. */
bool SemConferirAlocador(Semantica *s, Expr *chamada, Arg *em)
{
    Expr *v = em->valor;
    uint8_t aloc = SemAlocacaoDoNome(s, v);
    if (aloc == ALOC_ESCOPO && !ConferirValorAntigo(s, v)) return false;
    if (aloc == ALOC_ESCOPO && s->let_em_analise != chamada) {
        v->tipo = TIPO_VAZIO;
        return SemErro(s, E0316, v->pos, "alloc = scope so vale direto no let/var: let g = new(T, alloc = scope)");
    }
    if (aloc != ALOC_NENHUM) {
        v->tipo = TIPO_VAZIO;
        chamada->alocacao = aloc;
        return true;
    }
    if (EmNaoDeclarado(s, v)) return ErroEmDesconhecido(s, v);
    return EmDeAlocador(s, chamada, em);
}

/* 1o argumento (EX_TIPO) resolvido: guarda o TipoId no proprio no. */
static bool ResolverTipoDoArgumento(Semantica *s, Expr *e, TipoRef **ref, TipoId *tipo)
{
    Expr *no = e->args[0].valor;
    *ref = no->alvo;
    if (!SemResolverTipoRef(s, no->alvo)) return false;
    *tipo = no->alvo->tipo;
    no->tipo = *tipo;
    return true;
}

/* new(struct): o valor inicial e o literal T{} (padroes dos campos); campo sem zero e sem padrao = E0503 (D7). */
static bool NewDeStruct(Semantica *s, Expr *e, const TipoRef *ref, TipoId tipo)
{
    Expr *lit = ArenaAlocar(s->arena, sizeof *lit);
    lit->tipo_no = EX_LITERAL_STRUCT;
    lit->nome = TipoDe(&s->tipos, tipo)->a;
    lit->pos = lit->pos_op = ref->pos;
    if (!SemLiteralStruct(s, lit)) return false;
    e->b = lit;
    return true;
}

/* Que tipos new aceita (D7): struct (com literal), ou tipo com zero; resultado e lista nao. */
static bool ConferirTipoDoNew(Semantica *s, Expr *e, const TipoRef *ref, TipoId tipo)
{
    char texto[96];
    TipoClasse classe = TipoClasseDe(&s->tipos, tipo);
    e->b = NULL;
    if (classe == TC_RESULTADO) return SemErro(s, E0310, ref->pos, "new de 'T ! E' nao existe");
    if (classe == TC_DINAMICO) {
        return SemErro(s, E0310, ref->pos, "new([dynamic]T) nao existe: a lista ja guarda os elementos fora; crie com make([dynamic]T, alloc = heap)");
    }
    if (classe == TC_STRUCT) return NewDeStruct(s, e, ref, tipo);
    if (!SemTipoSemZero(s, tipo)) return true;
    SemDescrever(s, tipo, texto, sizeof texto);
    return SemErro(s, E0310, ref->pos, "new(%s): o tipo nao tem valor zero e new cria o valor zero; use ?^T ou guarde o ponteiro num struct com padrao", texto);
}

/* new(T, em = A): devolve ^T nunca nulo com o valor inicial de T. */
static bool ChamarNew(Semantica *s, Expr *e)
{
    TipoRef *ref;
    TipoId tipo;
    if (!SemLigarEmbutido(s, e, s->params_new, 2)) return false;
    if (!ResolverTipoDoArgumento(s, e, &ref, &tipo)) return false;
    if (!SemConferirAlocador(s, e, &e->args[1])) return false;
    if (!ConferirTipoDoNew(s, e, ref, tipo)) return false;
    e->tipo = TipoPonteiro(&s->tipos, tipo);
    return true;
}

/* make so cria [dynamic]T (D8). */
static bool ConferirTipoDoMake(Semantica *s, const TipoRef *ref, TipoId tipo)
{
    TipoClasse classe = TipoClasseDe(&s->tipos, tipo);
    if (classe == TC_DINAMICO) return true;
    if (__builtin_expect(TipoEhListaFixa(&s->tipos, tipo), 0)) {
        return SemErroDeListaFixa(s, ref->pos, "make nao cria [dynamic, N]T: a lista fixa nasce de {} e nao tem alocador (var l: [dynamic, N]T = {})");
    }
    if (__builtin_expect(classe == TC_MAPA, 0)) return true;
    if (__builtin_expect(classe == TC_SOA, 0)) {
        if (TipoSoaDinamica(&s->tipos, tipo)) return true;
        return SemErro(s, E0320, ref->pos, "soa[N]S nao nasce de make: declare var a: soa[N]S = {}");
    }
    if (classe == TC_FATIA) {
        return SemErro(s, E0310, ref->pos, "make de fatia nao existe: crie a lista com make([dynamic]T, alloc = heap) e empreste com l[..]");
    }
    return SemErro(s, E0310, ref->pos, "make cria lista, map ou soa dinamica: make([dynamic]T, alloc = heap, cap = 0)");
}

/* cap e len: int (literal adota; outro inteiro = E0301); constante negativa = E0421. rotulo = "cap" ou "len" (texto do erro). */
static bool ConferirCap(Semantica *s, Expr *v, const char *rotulo)
{
    bool negativo;
    uint64_t magnitude;
    if (!SemExpr(s, v, TIPO_INT) || !SemCompativel(s, TIPO_INT, v->tipo, v->pos)) return false;
    if (SemInteiroConstante(s, v, &negativo, &magnitude) && negativo && magnitude != 0) {
        return SemErro(s, E0421, v->pos, "%s negativo (-%llu)", rotulo, (unsigned long long)magnitude);
    }
    return true;
}

/* L5b: resize e make(len =) zeram os elementos novos com bytes 0: so numero (inteiro, real) e bool tem esse zero; dono e struct nao. */
static bool ConferirElementoZeravel(Semantica *s, uint32_t pos, TipoId elemento, const char *quem)
{
    char texto[96];
    if (TipoEhInteiro(elemento) || TipoEhReal(&s->tipos, elemento) || elemento == TIPO_BOOL) return true;
    SemDescrever(s, elemento, texto, sizeof texto);
    return SemErro(s, E0310, pos, "%s so vale em lista de inteiro, real ou bool (elemento %s): os demais entram um a um com append", quem, texto);
}

/* cap = N e len = N de make (L5b): len so em [dynamic]T de elemento zeravel. */
static bool ConferirTamanhosDoMake(Semantica *s, Expr *e, TipoId tipo)
{
    for (uint32_t k = 2; k < e->nargs; k++) {
        Arg *g = &e->args[k];
        bool eh_len = g->indice == MAKE_PARAM_LEN;
        if (!ConferirCap(s, g->valor, eh_len ? "len" : "cap")) return false;
        if (!eh_len) continue;
        if (TipoClasseDe(&s->tipos, tipo) != TC_DINAMICO) return SemErro(s, E0310, g->valor->pos, "len so vale em make([dynamic]T, alloc = heap, len = n)");
        if (!ConferirElementoZeravel(s, g->valor->pos, TipoDe(&s->tipos, tipo)->a, "make com len")) return false;
    }
    return true;
}

/* make([dynamic]T, em = A, cap = N, len = M): devolve a lista (len M = M zeros). */
static bool ChamarMake(Semantica *s, Expr *e)
{
    TipoRef *ref;
    TipoId tipo;
    if (!SemLigarEmbutido(s, e, s->params_make, 4)) return false;
    if (!ResolverTipoDoArgumento(s, e, &ref, &tipo)) return false;
    if (!SemConferirAlocador(s, e, &e->args[1])) return false;
    if (!ConferirTipoDoMake(s, ref, tipo)) return false;
    if (!ConferirTamanhosDoMake(s, e, tipo)) return false;
    e->tipo = tipo;
    return true;
}

/* ---------- append, delete e frame_reset (P3b) ---------- */

/* O tipo e ^[dynamic]T (ou ?^[dynamic]T se anulavel_vale)? */
static bool EhPonteiroDeLista(const Semantica *s, TipoId t, bool anulavel_vale)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe != TC_PONTEIRO && !(anulavel_vale && k->classe == TC_ANULAVEL)) return false;
    return TipoClasseDe(&s->tipos, k->a) == TC_DINAMICO;
}

/* O tipo e ^[dynamic, N]T? (v1.11 L15a: o append da lista de capacidade fixa) */
static bool EhPonteiroDeListaFixa(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    return k->classe == TC_PONTEIRO && TipoEhListaFixa(&s->tipos, k->a);
}

/* O tipo e ^soa[dynamic]S (ou ?^soa[dynamic]S se anulavel_vale)? (v1.9) */
static bool EhPonteiroDeSoaDinamica(const Semantica *s, TipoId t, bool anulavel_vale)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe != TC_PONTEIRO && !(anulavel_vale && k->classe == TC_ANULAVEL)) return false;
    return TipoClasseDe(&s->tipos, k->a) == TC_SOA && TipoSoaDinamica(&s->tipos, (TipoId)k->a);
}

/* O tipo e ^map[K]V (ou ?^map[K]V se anulavel_vale)? (v1.9) */
static bool EhPonteiroDeMapa(const Semantica *s, TipoId t, bool anulavel_vale)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe != TC_PONTEIRO && !(anulavel_vale && k->classe == TC_ANULAVEL)) return false;
    return TipoClasseDe(&s->tipos, k->a) == TC_MAPA;
}

/* append em soa dinamica por valor ou em tipo que nao e lista nem soa dinamica. */
static bool ErroDaColecaoDoAppend(Semantica *s, const Expr *lista)
{
    char texto[96];
    if (TipoClasseDe(&s->tipos, lista->tipo) == TC_DINAMICO || TipoEhListaFixa(&s->tipos, lista->tipo)) {
        SemErro(s, E0310, lista->pos, "append recebe o endereco da lista: append(&lista, item = x)");
        SemSugerirEnderecoDoAppend(s, lista);
        return false;
    }
    if (s->tipos.tem_soa && TipoClasseDe(&s->tipos, lista->tipo) == TC_SOA && TipoSoaDinamica(&s->tipos, lista->tipo)) {
        SemErro(s, E0310, lista->pos, "append recebe o endereco da soa: append(&a, item = s)");
        SemSugerirEnderecoDoAppend(s, lista);
        return false;
    }
    SemDescrever(s, lista->tipo, texto, sizeof texto);
    return SemErro(s, E0310, lista->pos, "append pede ^[dynamic]T ou ^soa[dynamic]S (tipo %s)", texto);
}

/* 1o argumento de append: ^[dynamic]T ou ^soa[dynamic]S; '&x' exige x alteravel (var). */
static bool ConferirListaDoAppend(Semantica *s, Expr *lista)
{
    bool soa = false;
    if (!SemChecar(s, lista, TIPO_VAZIO)) return false;
    if (!EhPonteiroDeLista(s, lista->tipo, false) && !EhPonteiroDeListaFixa(s, lista->tipo)) {
        soa = s->tipos.tem_soa && EhPonteiroDeSoaDinamica(s, lista->tipo, false);
        if (!soa) return ErroDaColecaoDoAppend(s, lista);
    }
    if (lista->tipo_no != EX_UNARIO || lista->op != OP_ENDERECO) return true;
    return SemLvalorMutavelDe(s, lista->a, soa ? "append altera a soa: declare com var" : "append altera a lista: declare com var");
}

/* O elemento da colecao do append: lista, lista fixa ou soa dinamica. */
static TipoId ElementoDoAppend(const Semantica *s, const Expr *lista)
{
    TipoId apontado = (TipoId)TipoDe(&s->tipos, lista->tipo)->a;
    TipoId elemento = TipoDe(&s->tipos, apontado)->a;
    if (__builtin_expect(TipoClasseDe(&s->tipos, apontado) == TC_SOA, 0)) elemento = TipoDe(&s->tipos, elemento)->a;
    if (__builtin_expect(TipoEhListaFixa(&s->tipos, apontado), 0)) elemento = TipoListaFixaElemento(&s->tipos, apontado); /* v1.11 L15a */
    return elemento;
}

/* L5b: 'items' e fatia []T (ou str, numa lista de u8) cujo elemento e o da lista; a lista e [dynamic]T de elemento sem dono. */
static bool ConferirItemsDoAppend(Semantica *s, Expr *lista, Expr *items, TipoId elemento)
{
    char texto[96];
    TipoId apontado = (TipoId)TipoDe(&s->tipos, lista->tipo)->a;
    if (TipoClasseDe(&s->tipos, apontado) != TC_DINAMICO) {
        return SemErro(s, E0310, items->pos, "items so vale em lista dinamica ([dynamic]T): em lista fixa e soa, um append por elemento");
    }
    if (!SemChecar(s, items, TIPO_VAZIO)) return false;
    bool fatia_do_elemento = TipoClasseDe(&s->tipos, items->tipo) == TC_FATIA && (TipoId)TipoDe(&s->tipos, items->tipo)->a == elemento;
    if (!fatia_do_elemento && !(items->tipo == TIPO_TEXTO && elemento == TIPO_U8)) {
        SemDescrever(s, items->tipo, texto, sizeof texto);
        return SemErro(s, E0301, items->pos, "items pede a fatia dos elementos da lista (a[..]; str numa lista de u8), nao %s", texto);
    }
    if (SemTipoEhDono(s, elemento)) {
        return SemErro(s, E0311, items->pos, "items copiaria elementos que sao donos de memoria: acrescente um a um com append(&l, item = ...) movendo cada dono");
    }
    return true;
}

/* append(&lista, item = x) ou append(&lista, items = fatia): x do tipo do elemento; dono entra por movimento (D9). */
static bool ChamarAppend(Semantica *s, Expr *e)
{
    if (!SemLigarEmbutido(s, e, s->params_append, 3)) return false;
    Expr *lista = e->args[0].valor;
    if (!ConferirListaDoAppend(s, lista)) return false;
    Arg *item = ArgumentoDoParametro(e, APPEND_PARAM_ITEM), *items = ArgumentoDoParametro(e, APPEND_PARAM_ITEMS);
    if (item == NULL && items == NULL) return SemErro(s, E0503, e->pos_op, "falta o argumento 'item' (um elemento) ou 'items' (uma fatia)");
    if (item != NULL && items != NULL) return SemErro(s, E0501, items->pos_nome, "item e items juntos: um append acrescenta um elemento (item) ou uma fatia (items)");
    TipoId elemento = ElementoDoAppend(s, lista);
    e->tipo = TIPO_VAZIO;
    if (items != NULL) return ConferirItemsDoAppend(s, lista, items->valor, elemento);
    if (!SemChecar(s, item->valor, elemento)) return false;
    if (elemento == TIPO_U8 && item->valor->tipo == TIPO_TEXTO) return SemTextoNoBuilder(s, lista, item); /* L9f: item str em [dynamic]u8 = os bytes do texto */
    if (!SemCoagir(s, item->valor, elemento, item->valor->pos)) return false;
    return SemConferirMovimento(s, item->valor) && SemConferirPosse(s, item->valor, PONTO_GUARDA);
}

/* resize(&lista, len = n): a lista passa a ter n elementos; os novos sao zero. So [dynamic]T de numero ou bool. */
bool SemChamarResize(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, s->params_resize, 2)) return false;
    Expr *lista = e->args[0].valor;
    if (!SemChecar(s, lista, TIPO_VAZIO)) return false;
    if (!EhPonteiroDeLista(s, lista->tipo, false)) {
        SemDescrever(s, lista->tipo, texto, sizeof texto);
        if (TipoClasseDe(&s->tipos, lista->tipo) == TC_DINAMICO) return SemErro(s, E0310, lista->pos, "resize recebe o endereco da lista: resize(&lista, len = n)");
        return SemErro(s, E0310, lista->pos, "resize pede ^[dynamic]T (tipo %s)", texto);
    }
    if (lista->tipo_no == EX_UNARIO && lista->op == OP_ENDERECO && !SemLvalorMutavelDe(s, lista->a, "resize altera a lista: declare com var")) return false;
    if (!ConferirCap(s, e->args[1].valor, "len")) return false;
    if (!ConferirElementoZeravel(s, lista->pos, TipoDe(&s->tipos, (TipoId)TipoDe(&s->tipos, lista->tipo)->a)->a, "resize")) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* sort(fatia): ordena no lugar, crescente, uma fatia de inteiro, real ou str (sem comparador). Lista e array entram como l[..]. */
bool SemChamarSort(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, s->params_um, 1)) return false;
    Expr *v = e->args[0].valor;
    if (!SemChecar(s, v, TIPO_VAZIO)) return false;
    e->args[0].indice = 0;
    TipoId elemento = TipoClasseDe(&s->tipos, v->tipo) == TC_FATIA ? (TipoId)TipoDe(&s->tipos, v->tipo)->a : 0;
    if (elemento == 0 || !(TipoEhInteiro(elemento) || elemento == TIPO_F32 || elemento == TIPO_F64 || elemento == TIPO_TEXTO)) {
        SemDescrever(s, v->tipo, texto, sizeof texto);
        return SemErro(s, E0310, v->pos, "sort pede uma fatia de inteiro, real (f32, f64) ou str: sort(l[..]); recebeu %s", texto);
    }
    e->tipo = TIPO_VAZIO;
    return true;
}

/* sort_by(s, less = p): ordena no lugar, ESTAVEL, com o comparador p (proc (a: ^T, b: ^T) -> bool; less(a, b) verdadeiro = a vem antes de b).
 * s e fatia de qualquer T (lista e array entram como l[..], igual ao sort); assinatura errada = E0907 (sem_callback.c). */
bool SemChamarSortBy(Semantica *s, Expr *e)
{
    char texto[96];
    TipoId params[2], elemento, comparador;
    ProcAssinatura assinatura = {params, NULL, 2, TIPO_BOOL};
    if (!SemLigarEmbutido(s, e, s->params_sort_by, 2)) return false;
    Expr *v = ArgumentoDoParametro(e, 0)->valor, *p = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, v, TIPO_VAZIO)) return false;
    elemento = TipoClasseDe(&s->tipos, v->tipo) == TC_FATIA ? (TipoId)TipoDe(&s->tipos, v->tipo)->a : 0;
    if (elemento == 0) {
        SemDescrever(s, v->tipo, texto, sizeof texto);
        return SemErro(s, E0310, v->pos, "sort_by pede uma fatia: sort_by(l[..], less = p); recebeu %s", texto);
    }
    params[0] = TipoPonteiro(&s->tipos, elemento);
    params[1] = params[0];
    comparador = TipoProc(&s->tipos, &assinatura);
    s->comparador_sort_by = true;
    bool ok_comparador = SemChecar(s, p, comparador);
    s->comparador_sort_by = false;
    if (!ok_comparador || !SemCoagir(s, p, comparador, p->pos)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* K08b: o runtime compara inteiro, f32, f64 e str (a ordem do sort); enum, struct, bool e ponteiro nao tem essa ordem. K17: exportada (sem_lista.c). */
bool SemEhOrdenavelDoRuntime(TipoId t)
{
    return TipoEhInteiro(t) || t == TIPO_F32 || t == TIPO_F64 || t == TIPO_TEXTO;
}

/* K08b: a chave de sort_by_key e Struct.campo, com Struct = o tipo do elemento e campo direto (aninhado fica de fora). Liga o EX_CAMPO como
 * offset_of (sem_layout.c) e devolve o tipo do campo em *campo; chave fora desse formato = E0337. */
static bool LigarChaveDoSort(Semantica *s, Expr *chave, TipoId elemento, TipoId *campo)
{
    const Simbolo *y = NULL;
    const Decl *est = NULL;
    int32_t idx = -1;
    if (chave->tipo_no == EX_CAMPO && chave->a->tipo_no == EX_NOME) y = SemAcharSimbolo(s, chave->a->nome, chave->a->modulo);
    if (y != NULL && y->categoria == SIM_STRUCT && y->tipo == elemento) est = SemDeclDaStruct(s, elemento);
    if (est != NULL) idx = SemAcharCampo(est, chave->nome);
    if (idx < 0) return SemErro(s, E0337, chave->pos, "sort_by_key: a chave nao e um campo da struct do elemento (use key = Struct.campo)");
    NomeUsado uso = {chave->a->nome, chave->a->modulo != 0 ? chave->a->pos_op : chave->a->pos, chave->a->modulo};
    if (!SemVisivel(s, y, &uso)) return false;
    chave->a->ref = REF_STRUCT;
    chave->a->ref_id = y->indice;
    chave->a->tipo = elemento;
    chave->ref = REF_CAMPO;
    chave->ref_id = idx;
    *campo = est->campos[idx].tipo.tipo;
    chave->tipo = *campo;
    s->usa_layout = true;
    return true;
}

/* K08b: sort_by_key(s, key = Struct.campo): ordena no lugar, ESTAVEL, pela chave do campo (inteiro, f32, f64 ou str). Fatia de struct; outra colecao = E0310. */
bool SemChamarSortByKey(Semantica *s, Expr *e)
{
    char texto[96];
    TipoId elemento, campo = TIPO_VAZIO;
    if (!SemLigarEmbutido(s, e, s->params_sort_by_key, 2)) return false;
    Expr *v = ArgumentoDoParametro(e, 0)->valor, *chave = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, v, TIPO_VAZIO)) return false;
    elemento = TipoClasseDe(&s->tipos, v->tipo) == TC_FATIA ? (TipoId)TipoDe(&s->tipos, v->tipo)->a : 0;
    if (elemento == 0 || TipoClasseDe(&s->tipos, elemento) != TC_STRUCT) {
        SemDescrever(s, v->tipo, texto, sizeof texto);
        return SemErro(s, E0310, v->pos, "sort_by_key pede uma fatia de struct: sort_by_key(l[..], key = Struct.campo); recebeu %s", texto);
    }
    if (!LigarChaveDoSort(s, chave, elemento, &campo)) return false;
    if (!SemEhOrdenavelDoRuntime(campo)) {
        SemDescrever(s, campo, texto, sizeof texto);
        return SemErro(s, E0337, chave->pos, "sort_by_key: o campo da chave nao e ordenavel (tipo %s): so inteiro, f32, f64 e str", texto);
    }
    e->tipo = TIPO_VAZIO;
    return true;
}

/* K08b: binary_search(s, value = v) -> (int, bool): lower bound de v na fatia ordenada e se achou (igualdade; NaN nunca). Colecao errada = E0310; elemento sem ordem = E0337. */
bool SemChamarBinarySearch(Semantica *s, Expr *e)
{
    char texto[96];
    TipoId elemento, elementos[2];
    if (!SemLigarEmbutido(s, e, s->params_binary_search, 2)) return false;
    Expr *v = ArgumentoDoParametro(e, 0)->valor, *valor = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, v, TIPO_VAZIO)) return false;
    elemento = TipoClasseDe(&s->tipos, v->tipo) == TC_FATIA ? (TipoId)TipoDe(&s->tipos, v->tipo)->a : 0;
    if (elemento == 0) {
        SemDescrever(s, v->tipo, texto, sizeof texto);
        return SemErro(s, E0310, v->pos, "binary_search pede uma fatia: binary_search(l[..], value = v); recebeu %s", texto);
    }
    if (!SemEhOrdenavelDoRuntime(elemento)) {
        SemDescrever(s, elemento, texto, sizeof texto);
        return SemErro(s, E0337, v->pos, "binary_search: o elemento nao e ordenavel (tipo %s): so inteiro, f32, f64 e str", texto);
    }
    if (!SemChecar(s, valor, elemento) || !SemCoagir(s, valor, elemento, valor->pos)) return false;
    elementos[0] = TIPO_INT;
    elementos[1] = TIPO_BOOL;
    e->tipo = TipoTupla(&s->tipos, elementos, 2);
    return true;
}

/* delete de str, cstr ou ?cstr: so libera o que veio de format/format_c (em = heap); literal direto nao existe. */
static bool ConferirTextoDoDelete(Semantica *s, const Expr *v)
{
    if (v->tipo_no != EX_TEXTO) return true;
    return SemErro(s, E0310, v->pos, "delete de texto literal nao existe: so libera o que veio de format/format_c (alloc = heap)");
}

/* delete: ^T vindo de new, ou [dynamic]T (zera a lista, entao pede var). */
static bool ConferirAlvoDoDelete(Semantica *s, Expr *v)
{
    char texto[96];
    TipoClasse classe = TipoClasseDe(&s->tipos, v->tipo);
    if (classe == TC_DINAMICO) return SemLvalorMutavelDe(s, v, "delete(lista) zera a lista: declare com var");
    if (__builtin_expect(TipoEhListaFixa(&s->tipos, v->tipo), 0)) {
        return SemErroDeListaFixa(s, v->pos, "delete nao existe para [dynamic, N]T: a lista vive no proprio valor; esvazie com l = {}");
    }
    if (__builtin_expect(classe >= TC_MAPA, 0) && SemColecaoDona(s, v->tipo)) {
        return SemLvalorMutavelDe(s, v, "delete zera a colecao: declare com var");
    }
    if (EhPonteiroDeLista(s, v->tipo, true)) {
        return SemErro(s, E0310, v->pos, "delete(p^) apaga a lista apontada; o ponteiro nao veio de new");
    }
    if ((__builtin_expect(s->tipos.tem_soa, 0) && EhPonteiroDeSoaDinamica(s, v->tipo, true)) ||
        (__builtin_expect(s->tipos.tem_mapa, 0) && EhPonteiroDeMapa(s, v->tipo, true))) {
        return SemErro(s, E0310, v->pos, "delete(p^) apaga a colecao apontada; o ponteiro nao veio de new");
    }
    if (classe == TC_PONTEIRO || classe == TC_ANULAVEL) return true;
    if (v->tipo == TIPO_STR || v->tipo == TIPO_CSTR || TipoOpcionalNicho(&s->tipos, v->tipo)) return ConferirTextoDoDelete(s, v);
    SemDescrever(s, v->tipo, texto, sizeof texto);
    return SemErro(s, E0310, v->pos, "delete pede ^T vindo de new ou [dynamic]T (tipo %s)", texto);
}

/* delete(p, em = pool): o 'em' so recebe um Pool e o alvo e ^T vindo de new(T, em = pool). */
static bool ConferirEmDoDelete(Semantica *s, Expr *e)
{
    static const char *const SO_POOL = "o alloc do delete so recebe um Pool: delete(p, alloc = pool); delete nao aceita 'alloc = ...' de Allocator (heap, frame, o alloc do proc)";
    Arg *em = &e->args[1];
    TipoClasse classe;
    if (SemAlocacaoDoNome(s, em->valor) != ALOC_NENHUM) return SemErro(s, E0310, em->valor->pos, "%s", SO_POOL);
    s->alocador_lendo = em->valor->tipo_no == EX_NOME; /* B01c: rotulo de embutido: o E0310 do Pool vale, nao o E0336 de leitura */
    bool lido = SemChecar(s, em->valor, TIPO_VAZIO);
    s->alocador_lendo = false;
    if (!lido) return false;
    if (s->tipo_pool == 0 || BaseDoAlocador(s, em->valor->tipo) != s->tipo_pool) {
        return SemErro(s, E0310, em->valor->pos, "%s", SO_POOL);
    }
    classe = TipoClasseDe(&s->tipos, e->args[0].valor->tipo);
    if (classe != TC_PONTEIRO && classe != TC_ANULAVEL) {
        return SemErro(s, E0310, e->args[0].valor->pos, "delete(p, alloc = pool) pede ^T vindo de new(T, alloc = pool)");
    }
    e->alocacao = ALOC_POOL;
    return TrocarPorId(s, em);
}

/* slice_from(p, len = n): fatia []T EMPRESTADA de um ^T ou ?^T vindo de foreign (vetor C de n elementos). Nao e dona: delete = E0310.
 * Aritmetica de ponteiro continua proibida; esta e a unica porta do vetor C para a fatia. n e qualquer inteiro (vira int; checado em dev). */
bool SemChamarSliceFrom(Semantica *s, Expr *e)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, s->params_slice_from, 2)) return false;
    Expr *p = ArgumentoDoParametro(e, 0)->valor, *n = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, p, TIPO_VAZIO) || !SemChecar(s, n, TIPO_INT)) return false;
    TipoClasse classe = TipoClasseDe(&s->tipos, p->tipo);
    SemDescrever(s, p->tipo, texto, sizeof texto);
    if (classe != TC_PONTEIRO && classe != TC_ANULAVEL) {
        return SemErro(s, E0301, p->pos, "slice_from pede ^T ou ?^T vindo de foreign (tipo %s); rawptr e cstr nao tem tamanho de elemento", texto);
    }
    TipoId elemento = (TipoId)TipoDe(&s->tipos, p->tipo)->a;
    if (SemTipoEhOpaco(s, elemento)) {
        return SemErro(s, E0301, p->pos, "slice_from de ponteiro para tipo opaco (%s): o tamanho do elemento e desconhecido", texto);
    }
    if (!TipoEhInteiro(n->tipo)) {
        return SemErro(s, E0301, n->pos, "len de slice_from pede um inteiro (a contagem de elementos do vetor C)");
    }
    e->tipo = TipoFatia(&s->tipos, elemento);
    return true;
}

static bool ChamarDelete(Semantica *s, Expr *e)
{
    if (!SemLigarEmbutido(s, e, s->params_delete, 2)) return false;
    if (!SemChecar(s, e->args[0].valor, TIPO_VAZIO)) return false;
    if (!ConferirAlvoDoDelete(s, e->args[0].valor)) return false;
    if (__builtin_expect(s->mapas_em_for.n != 0, 0)) {
        const Expr *alvo = e->args[0].valor;
        if (!SemConferirMapaNoFor(s, alvo, alvo->tipo_no == EX_NOME ? alvo->nome : -1)) return false;
    }
    e->tipo = TIPO_VAZIO;
    e->alocacao = ALOC_HEAP;
    if (e->nargs > 1 && !ConferirEmDoDelete(s, e)) return false;
    return SemConferirDelete(s, e);
}

static bool ChamarFrameReset(Semantica *s, Expr *e)
{
    if (!SemLigarEmbutido(s, e, NULL, 0)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* Despacho dos embutidos de memoria. */
bool SemChamarMemoria(Semantica *s, Expr *e, int32_t embutido)
{
    if (embutido == EMB_NEW) return ChamarNew(s, e);
    if (embutido == EMB_MAKE) return ChamarMake(s, e);
    if (embutido == EMB_APPEND) return ChamarAppend(s, e);
    if (embutido == EMB_DELETE) return ChamarDelete(s, e);
    if (embutido == EMB_FRAME_RESET) return ChamarFrameReset(s, e);
    FalhaInterna("SemChamarMemoria: embutido %d desconhecido", (int)embutido);
}

/* CHECAGENS[EX_TIPO]: um tipo so vale como 1o argumento de new e make (esses o resolvem sem passar por SemChecar). */
bool SemTipoSolto(Semantica *s, Expr *e)
{
    return SemErro(s, E0310, e->pos, "um tipo so vale como 1o argumento de new e make");
}
