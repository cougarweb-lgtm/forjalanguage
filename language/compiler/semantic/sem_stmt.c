/* sem_stmt.c - comandos: let, var, atribuicao, if, while, for, chamada solta e blocos com escopo.
 * Versao: 0.9.10
 * Changelog: 0.9.10 - M6c P87: let/var com proc foreign 'creates' rastreia o handle (ALOC_HANDLE); ComandoIf abre/fecha a guarda de 'if h == none'.
 *            0.9.9 - M6 P65a: DeclararNomeDoComandoQueFalhou grava o nome na tabela por local_id (sem buraco).
 *            0.9.8 - Portao Z b1: CausaDoDescarte inclui reverse de str (alloc = heap solto da E0310; reverse de lista segue sem efeito).
 *            0.9.7 - B01h: OrigemDaReatribuicao: 'x = expr' com local de origem heap ou 0 recebe a origem do valor; a restritiva (PARAM, ARENA, FRAME, POOL, literal, foreign) gruda.
 *            0.9.6 - B01b: let/var com origem ALOC_PARAM (alloc = alloc) tambem e rastreado (posse como a de heap).
 *            0.9.5 - IA1-L9f: CausaDoDescarte inclui to_str; format(into =) (tipo vazio) e instrucao e nao e descarte.
 *            0.9.4- IA1-L9e: CausaDoDescarte inclui keys, values e from_bytes (memoria nova que nao pode ser descartada).
 *            0.9.3 - IA1-L9d: CausaDoDescarte inclui clone (a lista nova nao pode ser descartada).
 *            0.9.2 - IA1-L5a: SemLvalorMutavelDe recusa s[i] de str (E0705: os bytes do texto nao se alteram, nem pelo '&').
 *            0.9.1 - IA1-L4: SemLvalorMutavelDe aceita a ligacao de switch que recebeu um dono ('switch d in f()' de 'T ! E': delete(d), append).
 *            0.9.0 - IA1-b15: varios erros por corpo: SemBloco recupera de cada comando que falha (SemRecuperar, ComandoRecuperando), o let/var que falhou declara o nome com veneno (cascata calada), if/while com condicao errada ainda analisam os corpos, E0704 para o bloco.
 *            0.8.7 - IA1-b08: o E0311 de 'for item' sobre listas leva a forma e a sugestao da ocorrencia (SemFormaDoDono).
 *            0.8.6 - IA1-b07: let/var e atribuicao marcam s->ctx_tipo antes da coercao e o limite do laco nao inteiro usa SemErroDoInteiro (forma por ocorrencia do E0301).
 *            0.8.5 - IA1-b06: o E0311 de let/var b = a leva a sugestao de emprestimo (SemSugerirEmprestimo).
 *            0.8.4- IA1-b04: o E0709 do '_ = valor' leva a sugestao (SemHigieneSugerirDescarteInutil).
 *            0.8.3 - Linguagem v1.11 L12b: SemRaizDaCadeia para em v.xy (REF_COMPONENTE) e SemLvalorMutavelDe da E0332 (gravar varias letras).
 *            0.8.2 - Linguagem v1.11 L12a: '+=' e '-=' com alvo [N]T vao para SemCompostaVetor (ComandoAtribuicao).
 *            0.8.1 -Linguagem v1.11 L10a-bis: '_ = try f()' (ComandoDescarte) e o grupo da var multipla (MarcarGrupoDaVar).
 *            0.8.0 - Linguagem v1.11 L10a: higiene (sem_higiene.c): '_ = f()' (ComandoDescarte), atribuicao simples nao le a raiz, SemLvalorMutavelDe marca a alteracao, retorno descartado e fim de bloco; RaizDaCadeia vira SemRaizDaCadeia.
 *            0.7.0 -Linguagem v1.9 P3: RaizDaCadeia segue ate a raiz tambem em FORMA_NENHUMA (soa, matrix); ComandoForColecao despacha tipo v1.9 para SemForV19; CorpoDeLaco deixa de ser static.
 *            0.6.5 - Linguagem v1.8 P7: let/var rastreia heap/escopo (posse, defer sintetico do escopo); atribuicao, laco (marca) e fim de bloco conferem a posse.
 *            0.6.4 - Linguagem v1.8 P6a: let/var grava a origem do valor (NovoLocal.origem).
 *            0.6.3 - Linguagem v1.7 P5a: descartar format/format_c como comando = E0310 (CausaDoDescarte).
 *            0.6.2 - Linguagem v1.7 P4: ComandoLetMultiplo (let/var a, b = f(), '_' descarta) e let/var simples de tupla = E0314; saiu o provisorio.
 *            0.6.1 - Linguagem v1.7 P2: let/var tipado e atribuicao coagem o valor (SemCoagir: T -> ?T).
 *            0.6.0 - Linguagem v1.7 P1: COMANDOS[ST_LET_MULTIPLO + 1]; let multiplo recusado com E0101 "chega no P4" (provisorio).
 *            0.5.0 - Linguagem v1.6 P4: listas na cadeia de lvalor, movimento em let/var/'=', 'for item' de dono = E0311.
 *            0.4.0 - Linguagem v1.6 P3a: new/make como comando (resultado descartado) = E0310.
 *            0.3.1 - Linguagem v1.5 P4: ComandoForColecao (item, &item, indice) sobre [N]T, []T e ^[N]T; saiu a recusa provisoria.
 *            0.3.0 - Linguagem v1.5 P3b: AlvoMutavel vira SemLvalorMutavel/SemLvalorMutavelDe (cadeia com [i], ponteiro e fatia, D7).
 *            0.2.2 - Linguagem v1.5 P1a: for de colecao (expr2 NULL) recusado com E0101 "chega no P4" (provisorio).
 *            0.2.1 - Linguagem v1.3 P6: ValorGuardavel aceita str e cstr.
 *            0.2.0 - Linguagem v1.3: AlvoMutavel reescrita (D5): 'E^', 'E.campo' com E de tipo ^Struct e cadeias; resto E0705;
 *                    let/var tipado guarda o tipo declarado (coercao D8).
 *            0.1.0 - Fase 5c: regras do prototipo (benchmark/c/check_stmt.c); sem sombra (E0202); atribuicao so a var (E0705);
 *                    resultado solto (E0601); codigo depois de return/break/continue (E0704).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

typedef bool (*ChecaComando)(Semantica *s, Stmt *st);

/* O valor de um let/var precisa existir. */
static bool ValorGuardavel(Semantica *s, const Expr *e)
{
    if (e->tipo == TIPO_VAZIO) return SemErro(s, E0301, e->pos, "a expressao nao tem valor");
    return true;
}

/* let/var simples nao guarda tupla (D12). */
static bool TuplaNaoVaiParaVariavel(Semantica *s, const Expr *e)
{
    if (TipoClasseDe(&s->tipos, e->tipo) != TC_TUPLA) return true;
    const char *nome = SemNomeTexto(s, e->nome);
    return SemErro(s, E0314, e->pos, "'%s' devolve %u valores: use let a, b = %s(...)", nome, TipoTuplaQtd(&s->tipos, e->tipo), nome);
}

static bool ComandoLetVar(Semantica *s, Stmt *st)
{
    TipoId querido = TIPO_VAZIO;
    if (st->tem_tipo) {
        if (!SemResolverTipoRef(s, &st->tipo)) return false;
        querido = st->tipo.tipo;
    }
    s->let_em_analise = st->expr;
    bool valor_ok = SemExpr(s, st->expr, querido);
    s->let_em_analise = NULL;
    if (!valor_ok) return false;
    s->ctx_tipo = (ContextoDoTipo){CTX_DECLARACAO, st->expr, st->nome, -1};
    if (st->tem_tipo && !SemCoagir(s, st->expr, querido, st->expr->pos)) return false;
    if (!ValorGuardavel(s, st->expr) || !TuplaNaoVaiParaVariavel(s, st->expr)) return false;
    if (!SemConferirMovimento(s, st->expr)) {
        SemSugerirEmprestimo(s, st->expr);
        return false;
    }
    if (!SemConferirPosse(s, st->expr, PONTO_COPIA)) return false;
    st->tipo_local = st->tem_tipo ? querido : st->expr->tipo; /* a coercao ^T -> ?^T guarda o tipo declarado */
    NovoLocal novo = {st->nome, SemPosDepoisDaPalavra(s, st->pos), st->tipo_local, st->tipo_no == ST_LET ? LOC_LET : LOC_VAR,
                      SemOrigemDoValor(st->expr)};
    if (!SemDeclararLocal(s, &novo, &st->local_id)) return false;
    if (s->tem_externo && SemLetCriaHandle(s, st)) return SemRastrearLocal(s, st, ALOC_HANDLE); /* M6c P87: proc foreign 'creates' */
    if (novo.origem != ALOC_HEAP && novo.origem != ALOC_ESCOPO && novo.origem != ALOC_PARAM) return true;
    if (novo.origem == ALOC_ESCOPO) SemDeferDoEscopo(s, st);
    return SemRastrearLocal(s, st, novo.origem);
}

/* Segue a cadeia 'nome ('.campo' | '[i]' | '^')*' ate a raiz. NULL = a cadeia passa por ponteiro ou fatia (alteravel de qualquer raiz);
 * senao a raiz (um EX_NOME, ou o valor que nao e enderecavel: chamada, literal). */
const Expr *SemRaizDaCadeia(const Semantica *s, const Expr *x)
{
    ColecaoForma forma;
    for (;;) {
        if (x->tipo_no == EX_FATIA || (x->tipo_no == EX_UNARIO && x->op == OP_DEREF)) return NULL;
        if (x->tipo_no == EX_CAMPO && TipoClasseDe(&s->tipos, x->a->tipo) == TC_PONTEIRO) return NULL;
        if (x->tipo_no == EX_INDICE) {
            SemElementoDe(s, x->a->tipo, &forma);
            if (forma == FORMA_FATIA || forma == FORMA_PONTEIRO_ARRAY || forma == FORMA_PONTEIRO_DINAMICO) return NULL;
        }
        if (x->tipo_no == EX_CAMPO && x->ref == REF_COMPONENTE) return x; /* v1.11 L12b: v.xy e uma copia, nao tem raiz alteravel */
        if (x->tipo_no != EX_CAMPO && x->tipo_no != EX_INDICE) return x;
        x = x->a;
    }
}

/* Lvalor alteravel (D7): const -> E0301 (como no prototipo); so var, ou cadeia por ponteiro/fatia, aceita escrita -> E0705.
 * 'motivo' (ou NULL) troca o fim da mensagem do E0705 quando quem pede e '&', fatia ou 'for &item'. */
bool SemLvalorMutavelDe(Semantica *s, const Expr *alvo, const char *motivo)
{
    if (alvo->tipo_no == EX_INDICE && alvo->a->tipo == TIPO_TEXTO) {
        return SemErro(s, E0705, alvo->pos, "os bytes de um str nao se alteram (s[i] so le): monte um texto novo com format(f\"...\", alloc = heap)");
    }
    SemHigieneAlterar(s, alvo);
    const Expr *raiz = SemRaizDaCadeia(s, alvo);
    if (raiz == NULL) return true;
    if (raiz->tipo_no == EX_CAMPO) {
        return SemErro(s, E0332, raiz->pos_op, "'.%s' monta um array novo: gravar varias letras nao existe, grave uma letra por vez (v.x = ..)",
                       SemNomeTexto(s, raiz->nome));
    }
    if (raiz->tipo_no != EX_NOME) return SemErro(s, E0705, alvo->pos, "este valor nao aceita atribuicao");
    const LocalInfo *local = EscopoBuscar(&s->escopo, raiz->nome);
    if (raiz->ref == REF_CONST) return SemErro(s, E0301, raiz->pos, "constante nao recebe atribuicao");
    if (local == NULL) return SemErro(s, E0705, alvo->pos, "este valor nao aceita atribuicao");
    if (local->natureza == LOC_VAR) return true;
    if (local->natureza == LOC_LIGACAO && SemTipoEhDono(s, local->tipo)) return true; /* L-01: a ligacao de 'T ! E' recebe o dono; delete e append valem nela */
    if (motivo != NULL) return SemErro(s, E0705, alvo->pos, "'%s' e imutavel: %s", SemNomeTexto(s, raiz->nome), motivo);
    return SemErro(s, E0705, alvo->pos, "'%s' e imutavel (declarado com let, parametro, for ou switch); use var", SemNomeTexto(s, raiz->nome));
}

bool SemLvalorMutavel(Semantica *s, const Expr *alvo)
{
    return SemLvalorMutavelDe(s, alvo, NULL);
}

static const char *CausaDoDescarte(const Expr *e);

static bool EhDescarte(const Semantica *s, const Stmt *st)
{
    return st->op == 0 && st->alvo->tipo_no == EX_NOME && st->alvo->modulo == 0 && st->alvo->nome == s->nome_descarte;
}

/* v1.11 L10a: '_ = f()' descarta de proposito o valor de uma chamada; depois de conferida vira o comando 'f()' (o lowering nao ve a atribuicao).
 * resultado ('T ! E') segue pedindo tratamento (E0601); '_ = x' sem chamada, ou de chamada sem retorno, e E0709.
 * v1.11 L10a-bis: '_ = try f()' tambem vale (o payload e descartado; o erro sobe): o Stmt vira o 'try f()' como comando, o .c sai igual. */
static bool ComandoDescarte(Semantica *s, Stmt *st)
{
    char texto[96];
    const Expr *valor = st->expr;
    if (!SemExpr(s, st->expr, TIPO_VAZIO)) return false;
    const char *causa = CausaDoDescarte(valor);
    if (causa != NULL) return SemErro(s, E0310, valor->pos_op, "%s", causa);
    if (TipoClasseDe(&s->tipos, valor->tipo) == TC_RESULTADO) {
        SemDescrever(s, valor->tipo, texto, sizeof texto);
        return SemErro(s, E0601, valor->pos, "o resultado %s nao se descarta com '_ =': use _ = try f(), try, or_else, switch ou let", texto);
    }
    const Expr *chamada = valor->tipo_no == EX_TRY ? valor->a : valor;
    if (chamada->tipo_no != EX_CHAMADA || valor->tipo == TIPO_VAZIO) {
        if (SemHigienePropor(s, E0709, st->pos, "'_ =' so descarta o valor devolvido por uma chamada que devolve valor"))
            SemHigieneSugerirDescarteInutil(s, st->pos, chamada->tipo_no == EX_CHAMADA);
        return true;
    }
    st->tipo_no = ST_CHAMADA;
    st->alvo = NULL;
    return true;
}

/* B01h: origem restritiva que gruda: 'x = expr' troca a origem do local so se ela era heap ou 0 (sem fluxo, D19: a restricao de um
 * caminho vale para o todo). Fica fora do atalho de rastreados, que nao cobre local sem rastreio. Sempre devolve true (cadeia do &&). */
static bool OrigemDaReatribuicao(Semantica *s, const Stmt *st)
{
    if (st->op != 0 || st->alvo->tipo_no != EX_NOME || st->alvo->ref != REF_LOCAL || st->alvo->modulo != 0) return true;
    LocalInfo *local = EscopoBuscarMut(&s->escopo, st->alvo->nome);
    if (local == NULL || (local->origem != ALOC_HEAP && local->origem != ALOC_NENHUM)) return true;
    local->origem = SemOrigemDoValor(st->expr);
    return true;
}

static bool ComandoAtribuicao(Semantica *s, Stmt *st)
{
    if (EhDescarte(s, st)) return ComandoDescarte(s, st);
    bool lida_antes = SemHigieneRaizLida(s, st->alvo);
    if (!SemExpr(s, st->alvo, TIPO_VAZIO) || !SemLvalorMutavel(s, st->alvo)) return false;
    if (st->op == 0) SemHigieneEscrita(s, st->alvo, lida_antes);
    TipoId querido = st->alvo->tipo;
    bool alvo_nomeado = st->alvo->tipo_no == EX_NOME || st->alvo->tipo_no == EX_CAMPO;
    if (!SemExpr(s, st->expr, querido)) return false;
    s->ctx_tipo = (ContextoDoTipo){CTX_ATRIBUICAO, st->expr, alvo_nomeado ? st->alvo->nome : -1, -1};
    if (!SemCoagir(s, st->expr, querido, st->expr->pos)) return false;
    if (st->op != 0 && TipoClasseDe(&s->tipos, querido) == TC_ARRAY) return SemCompostaVetor(s, st); /* v1.11 L12a: [N]T += [N]T */
    if (st->op != 0 && !TipoEhNumerico(&s->tipos, querido)) {
        return SemErro(s, E0301, st->expr->pos, "'+=' e '-=' pedem numero");
    }
    if (st->op != 0) return true;
    return SemConferirReatribuicao(s, st) && SemConferirMovimento(s, st->expr) &&
           SemConferirPosse(s, st->expr, st->alvo->tipo_no == EX_NOME ? PONTO_COPIA : PONTO_GUARDA) &&
           OrigemDaReatribuicao(s, st);
}

/* IA1-b15: erro na condicao de if/while guarda o erro e ainda analisa os corpos (independentes da condicao). false = lista cheia. */
static bool CondicaoRecuperando(Semantica *s, Expr *cond)
{
    return SemCondicao(s, cond) || SemRecuperar(s);
}

static bool ComandoIf(Semantica *s, Stmt *st)
{
    if (!CondicaoRecuperando(s, st->expr)) return false;
    int32_t guarda = s->rastreados.n == 0 ? -1 : SemGuardaNoneAbrir(s, st->expr); /* M6c P87: 'if h == none' dispensa o handle no then */
    bool then_ok = SemBloco(s, st->corpo);
    if (guarda >= 0) SemGuardaNoneFechar(s, guarda);
    return then_ok && SemBloco(s, st->senao);
}

/* Corpo de laco: conta a profundidade para break/continue. */
bool CorpoDeLaco(Semantica *s, Stmt *corpo)
{
    uint32_t marca_anterior = s->marca_laco;
    s->marca_laco = (uint32_t)s->rastreados.n;
    s->prof_laco++;
    bool ok = SemBloco(s, corpo);
    s->prof_laco--;
    s->marca_laco = marca_anterior;
    return ok;
}

static bool ComandoWhile(Semantica *s, Stmt *st)
{
    return CondicaoRecuperando(s, st->expr) && CorpoDeLaco(s, st->corpo);
}

/* Limites do for: mesma categoria inteira; um limite so de literais adota o tipo do outro. */
static bool LimitesDoFor(Semantica *s, Stmt *st)
{
    Expr *a = st->expr, *b = st->expr2;
    SemMarcarFlexivel(a);
    SemMarcarFlexivel(b);
    if (a->flexivel && !b->flexivel) {
        if (!SemChecar(s, b, TIPO_VAZIO) || !SemChecar(s, a, b->tipo)) return false;
    } else if (!SemChecar(s, a, TIPO_VAZIO) || !SemChecar(s, b, a->tipo)) {
        return false;
    }
    if (!TipoEhInteiro(a->tipo)) return SemErroDoInteiro(s, a, "o limite do laco");
    st->tipo_local = a->tipo;
    return a->tipo == b->tipo || SemCompativel(s, a->tipo, b->tipo, b->pos);
}

/* for item in L / for &item in L / for i, item in L: L e [N]T, []T ou ^[N]T; o item e o elemento (copia) ou ^elemento. */
static bool ComandoForColecao(Semantica *s, Stmt *st)
{
    ColecaoForma forma;
    char tipo[96];
    if (!SemExpr(s, st->expr, TIPO_VAZIO)) return false;
    TipoId elemento = SemElementoDe(s, st->expr->tipo, &forma);
    if (forma == FORMA_NENHUMA) {
        if (SemEhTipoV19(s, st->expr->tipo)) return SemForV19(s, st);
        SemDescrever(s, st->expr->tipo, tipo, sizeof tipo);
        return SemErro(s, E0308, st->expr->pos, "for de item percorre array, fatia ou ^[N]T (tipo %s); para contar use for i in 0..<n", tipo);
    }
    if (st->item_ref && (forma == FORMA_ARRAY || forma == FORMA_DINAMICO) &&
        !SemLvalorMutavelDe(s, st->expr, "'for &item' altera os elementos: declare a lista com var")) {
        return false;
    }
    if (!st->item_ref && SemTipoEhDono(s, elemento)) {
        SemErro(s, E0311, SemPosDepoisDaPalavra(s, st->pos), "'for item' copiaria cada lista: use 'for &item in ...'");
        SemFormaDoDono(s, DONO_FOR, SemPosDepoisDaPalavra(s, st->pos), TIPO_VAZIO);
        return false;
    }
    uint32_t altura = EscopoAltura(&s->escopo);
    uint32_t pos = SemPosDepoisDaPalavra(s, st->pos);
    NovoLocal indice = {st->nome_indice, pos, TIPO_INT, LOC_FOR, 0};
    st->tipo_local = st->item_ref ? TipoPonteiro(&s->tipos, elemento) : elemento;
    NovoLocal item = {st->nome, pos, st->tipo_local, LOC_FOR, 0};
    bool ok = (st->nome_indice < 0 || SemDeclararLocal(s, &indice, &st->local_indice)) &&
              SemDeclararLocal(s, &item, &st->local_id) && CorpoDeLaco(s, st->corpo);
    EscopoRecuar(&s->escopo, altura);
    return ok;
}

static bool ComandoFor(Semantica *s, Stmt *st)
{
    if (st->expr2 == NULL) return ComandoForColecao(s, st);
    if (!LimitesDoFor(s, st)) return false;
    uint32_t altura = EscopoAltura(&s->escopo);
    NovoLocal novo = {st->nome, SemPosDepoisDaPalavra(s, st->pos), st->tipo_local, LOC_FOR, 0};
    if (!SemDeclararLocal(s, &novo, &st->local_id)) return false;
    bool ok = CorpoDeLaco(s, st->corpo);
    EscopoRecuar(&s->escopo, altura);
    return ok;
}

/* Causa do descarte de um resultado que aloca (new, make, format, format_c); NULL se a chamada nao aloca. */
static const char *CausaDoDescarte(const Expr *e)
{
    if (e->tipo_no != EX_CHAMADA || e->ref != REF_EMBUTIDO) return NULL;
    if (e->ref_id == EMB_NEW || e->ref_id == EMB_MAKE) {
        return "o resultado de new/make precisa ser guardado: let p = new(...); senao a memoria se perde";
    }
    if (e->ref_id == EMB_CLONE) return "o resultado de clone precisa ser guardado: let c = clone(l, alloc = heap); senao a lista se perde";
    if (e->ref_id == EMB_REVERSE && e->tipo == TIPO_STR) return "o resultado de reverse de str precisa ser guardado: let r = reverse(s, alloc = heap); senao a memoria se perde";
    if (e->ref_id == EMB_KEYS || e->ref_id == EMB_VALUES || e->ref_id == EMB_FROM_BYTES) {
        return "o resultado de keys, values e from_bytes precisa ser guardado: let k = keys(m, alloc = heap); senao a memoria se perde";
    }
    if (e->ref_id == EMB_TO_STR) return "o resultado de to_str precisa ser guardado: let s = to_str(&buf); senao a memoria se perde";
    if ((e->ref_id == EMB_FORMAT && e->tipo != TIPO_VAZIO) || e->ref_id == EMB_FORMAT_C) { /* format(into =) e instrucao, sem resultado (L9f) */
        return "o resultado de format/format_c precisa ser guardado: let s = format(...); senao a memoria se perde";
    }
    return NULL;
}

/* Chamada como comando: um valor resultado nao pode ser ignorado (dentro de defer o erro nao tem para onde ir). */
static bool ComandoChamada(Semantica *s, Stmt *st)
{
    char texto[96];
    if (!SemExpr(s, st->expr, TIPO_VAZIO)) return false;
    const char *causa = CausaDoDescarte(st->expr);
    if (causa != NULL) return SemErro(s, E0310, st->expr->pos_op, "%s", causa);
    if (TipoClasseDe(&s->tipos, st->expr->tipo) != TC_RESULTADO) {
        SemHigieneRetornoDescartado(s, st->expr);
        return true;
    }
    SemDescrever(s, st->expr->tipo, texto, sizeof texto);
    if (s->em_defer) {
        return SemErro(s, E0702, st->expr->pos, "o defer de '%s' descartaria o erro do resultado %s; adie uma chamada sem resultado",
                       SemNomeTexto(s, st->expr->nome), texto);
    }
    return SemErro(s, E0601, st->expr->pos, "o resultado de '%s' (%s) foi ignorado: use try, or_else, switch ou let",
                   SemNomeTexto(s, st->expr->nome), texto);
}

/* v1.11 L10a-bis: 'var a, b = f()' e uma declaracao so; os nomes dela dividem o grupo (id do 1o + 1) e o E0707 vale pelo grupo. */
static int32_t MarcarGrupoDaVar(Semantica *s, const NomeLet *n, int32_t grupo)
{
    if (grupo == 0) grupo = n->local_id + 1;
    EscopoBuscarMut(&s->escopo, n->nome)->grupo = grupo;
    return grupo;
}

/* Um local por nome do let multiplo, do elemento k da tupla; '_' descarta o elemento (sem local, local_id -1). */
static bool DeclararNomesDoLet(Semantica *s, Stmt *st)
{
    TipoId tupla = st->expr->tipo;
    int32_t grupo = 0;
    for (uint32_t k = 0; k < st->nnomes; k++) {
        NomeLet *n = &st->nomes[k];
        n->tipo = TipoTuplaElemento(&s->tipos, tupla, k);
        n->local_id = -1;
        if (n->nome == s->nome_descarte) continue;
        NovoLocal novo = {n->nome, n->pos, n->tipo, st->op ? LOC_VAR : LOC_LET, 0};
        if (!SemDeclararLocal(s, &novo, &n->local_id)) return false;
        if (st->op) grupo = MarcarGrupoDaVar(s, n, grupo);
    }
    return true;
}

/* 'let q, r = f()' / 'var a, _ = f()' (D12): o valor e uma tupla com um elemento por nome. */
static bool ComandoLetMultiplo(Semantica *s, Stmt *st)
{
    char texto[96];
    if (!SemExpr(s, st->expr, TIPO_VAZIO)) return false;
    if (TipoClasseDe(&s->tipos, st->expr->tipo) != TC_TUPLA) {
        SemDescrever(s, st->expr->tipo, texto, sizeof texto);
        return SemErro(s, E0314, st->expr->pos, "let a, b = ... pede um proc com retorno multiplo; recebeu %s", texto);
    }
    uint32_t qtd = TipoTuplaQtd(&s->tipos, st->expr->tipo);
    if (qtd != st->nnomes) {
        return SemErro(s, E0314, st->expr->pos, "'%s' devolve %u valores e o let tem %u nomes", SemNomeTexto(s, st->expr->nome), qtd,
                       st->nnomes);
    }
    return DeclararNomesDoLet(s, st);
}
static const ChecaComando COMANDOS[ST_LET_MULTIPLO + 1] = {
    [ST_LET_MULTIPLO] = ComandoLetMultiplo,
    [ST_LET] = ComandoLetVar, [ST_VAR] = ComandoLetVar, [ST_ATRIBUICAO] = ComandoAtribuicao, [ST_IF] = ComandoIf,
    [ST_WHILE] = ComandoWhile, [ST_FOR] = ComandoFor, [ST_RETURN] = SemReturn, [ST_CHAMADA] = ComandoChamada,
    [ST_BREAK] = SemLacoControle, [ST_CONTINUE] = SemLacoControle, [ST_DEFER] = SemDefer, [ST_SWITCH] = SemSwitch,
    [ST_EXPECT] = SemExpect,
};

bool SemComando(Semantica *s, Stmt *st)
{
    return COMANDOS[st->tipo_no](s, st);
}

static bool EhSaida(const Stmt *st)
{
    return st->tipo_no == ST_RETURN || st->tipo_no == ST_BREAK || st->tipo_no == ST_CONTINUE;
}

bool SemRecuperar(Semantica *s)
{
    s->corpo_com_erro = true;
    if (!s->diag->tem) return !s->lista.cortado;
    return SemGuardarErro(s, s->modulo_atual);
}

/* IA1-b15: o let/var/let multiplo que falhou ainda declara o nome: com o tipo escrito (se resolveu) ou TIPO_VENENO; os usos seguintes nao viram
 * "nome nao declarado" (cascata). Nome ja existente (E0202) e '_' ficam como estao. */
static void DeclararNomeDoComandoQueFalhou(Semantica *s, const NovoLocal *novo)
{
    if (novo->nome < 0 || novo->nome == s->nome_descarte || EscopoBuscar(&s->escopo, novo->nome) != NULL) return;
    SemGravarNomeLocal(s, novo->nome, novo->tipo, novo->pos);
    LocalInfo local = {novo->nome, novo->tipo, novo->natureza, 0, s->prox_local++, 0, novo->pos, LOCAL_LIDO | LOCAL_ALTERADO, 0};
    EscopoEmpurrar(&s->escopo, &local);
}

static void DeclararVenenoDoComando(Semantica *s, const Stmt *st)
{
    if (st->tipo_no == ST_LET_MULTIPLO) {
        for (uint32_t k = 0; k < st->nnomes; k++) {
            NovoLocal novo = {st->nomes[k].nome, st->nomes[k].pos, TIPO_VENENO, st->op ? LOC_VAR : LOC_LET, 0};
            DeclararNomeDoComandoQueFalhou(s, &novo);
        }
        return;
    }
    if (st->tipo_no != ST_LET && st->tipo_no != ST_VAR) return;
    TipoId tipo = st->tem_tipo && st->tipo.tipo > TIPO_VAZIO ? st->tipo.tipo : TIPO_VENENO;
    NovoLocal novo = {st->nome, SemPosDepoisDaPalavra(s, st->pos), tipo, st->tipo_no == ST_LET ? LOC_LET : LOC_VAR, 0};
    DeclararNomeDoComandoQueFalhou(s, &novo);
}

/* IA1-b15: um comando; se falhar, o erro vai para a lista, o escopo volta ao que era antes do comando e o bloco segue com o veneno no nome
 * que o comando declararia. O veneno tocado vale por comando (tocou_veneno do corpo e a soma). false = lista cheia. */
static bool ComandoRecuperando(Semantica *s, Stmt *st)
{
    uint32_t altura = EscopoAltura(&s->escopo);
    bool veneno_antes = s->tocou_veneno;
    s->tocou_veneno = false;
    bool ok = SemComando(s, st);
    if (!ok) {
        EscopoRecuar(&s->escopo, altura);
        ok = SemRecuperar(s);
        if (ok) DeclararVenenoDoComando(s, st);
    }
    s->tocou_veneno = s->tocou_veneno || veneno_antes;
    return ok;
}

bool SemBloco(Semantica *s, Stmt *primeiro)
{
    uint32_t altura = EscopoAltura(&s->escopo);
    uint32_t marca = (uint32_t)s->rastreados.n;
    const Stmt *anterior = NULL;
    for (Stmt *st = primeiro; st != NULL; st = st->prox) {
        if (anterior != NULL && EhSaida(anterior)) {
            SemErro(s, E0704, st->pos, "comando depois de return, break ou continue nunca executa");
            if (!SemRecuperar(s)) return false;
            break; /* o codigo morto nao e analisado */
        }
        if (!ComandoRecuperando(s, st)) return false;
        anterior = st;
    }
    if (s->corpo_com_erro) s->rastreados.n = marca; /* a posse de um corpo com erro esta meio contada: o vazamento seria cascata */
    else if (s->rastreados.n != marca && !SemConferirFimDoBloco(s, marca)) return false;
    SemHigieneFechar(s, altura);
    EscopoRecuar(&s->escopo, altura);
    return true;
}
