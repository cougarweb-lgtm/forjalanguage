/* sem_fuga.c - fuga intraprocedural (K1): o endereco de um local, ou a fatia de um array local, nao sai do proc (E0706).
 * Versao: 0.2.1
 * Changelog: 0.2.1 - Linguagem v1.13 L1a: SemEntrarNaDecl (modulo e arquivo da declaracao).
 *            0.2.0 - Linguagem v1.11 L3: SemFugaProc com tabela de resumos e modo silencioso; return e gravacao alimentam o resumo; recipiente
 *                    ([dynamic], map, soa) por valor aceita l[i] = v; FugaEfeitos antes de cada comando; 'let a, b = f()' usa o resumo; sem a
 *                    saida antecipada de proc sem locais (o resumo precisa do return).
 *            0.1.0 - Linguagem v1.11 L2: passe por proc depois do corpo tipado sem erro; conjunto de origens por local; sumidouros: return e
 *                    gravacao atraves de ponteiro de origem alheia ao proc (parametro, heap, global, foreign, desconhecido). Os E0706
 *                    sintaticos de sem_fluxo.c passam a sair daqui.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_fuga.h"

#include <string.h>

typedef bool (*AndaComando)(Fuga *f, const Stmt *st);

/* ---------- conjuntos ---------- */

static void RegistrarNovos(Fuga *f, uint32_t var, uint32_t palavra, uint64_t novo, const Expr *origem)
{
    while (novo != 0) {
        uint32_t bit = palavra * 64 + (uint32_t)__builtin_ctzll(novo);
        novo &= novo - 1;
        if (bit < f->nl) FugaProvRegistrar(f, var, bit, origem);
    }
}

void FugaUnirEm(Fuga *f, uint32_t var, const FugaConj v, const Expr *origem)
{
    FugaConj c = FugaConjDe(f, var);
    for (uint32_t w = 0; w < f->pal; w++) {
        uint64_t novo = v[w] & ~c[w];
        if (novo == 0) continue;
        c[w] |= novo;
        f->mudou = true;
        RegistrarNovos(f, var, w, novo, origem);
    }
}

int32_t FugaPrimeiroLocal(const Fuga *f, const FugaConj c)
{
    for (uint32_t w = 0; w < f->pal && w * 64 < f->nl; w++) {
        uint64_t palavra = c[w];
        if (palavra == 0) continue;
        uint32_t bit = w * 64 + (uint32_t)__builtin_ctzll(palavra);
        return bit < f->nl ? (int32_t)bit : -1;
    }
    return -1;
}

/* ---------- locais ---------- */

static void Declarar(Fuga *f, int32_t id, int32_t nome, uint32_t pos, TipoId tipo)
{
    if (id < 0 || (uint32_t)id >= f->nl) return;
    f->locais[id] = (FugaLocal){nome, pos, SemFugaTipoTemPonteiro(f->s, tipo)};
}

static bool Rastreado(const Fuga *f, int32_t id)
{
    return id >= 0 && (uint32_t)id < f->nl && f->locais[id].rastreado;
}

/* Se o local guarda ponteiro, acumula nele o que 'f->v' tem. */
static void GuardarNoLocal(Fuga *f, int32_t id, const Expr *origem)
{
    if (Rastreado(f, id)) FugaUnirEm(f, (uint32_t)id, f->v, origem);
}

/* ---------- sumidouros ---------- */

static bool SumidouroDoValor(Fuga *f, const Expr *valor, const FugaSumidouro *sum)
{
    FugaZerar(f, f->v);
    FugaAvaliar(f, valor, f->v);
    FugaRegistrarRetorno(f, f->v);
    int32_t bit = FugaPrimeiroLocal(f, f->v);
    if (bit < 0) return true;
    f->candidato = true;
    return f->silencioso || FugaReportar(f, valor, (uint32_t)bit, sum);
}

static bool FugaReturn(Fuga *f, const Stmt *st)
{
    FugaSumidouro sum = {SUM_RETURN, -1, st->pos, -1};
    if (st->expr == NULL) return true;
    if (st->expr->tipo_no != EX_TUPLA) return SumidouroDoValor(f, st->expr, &sum);
    for (uint32_t k = 0; k < st->expr->nargs; k++) {
        if (!SumidouroDoValor(f, st->expr->args[k].valor, &sum)) return false;
    }
    return true;
}

/* ---------- gravacao ---------- */

/* Base de uma cadeia de lvalor que continua POR VALOR ('s.c' de struct, 'a[i]' de array); NULL quando salta por ponteiro, fatia ou outro. */
static const Expr *BaseDireta(const Fuga *f, const Expr *x)
{
    if (x->tipo_no == EX_CAMPO) return TipoClasseDe(&f->s->tipos, x->a->tipo) == TC_PONTEIRO ? NULL : x->a;
    if (x->tipo_no != EX_INDICE) return NULL;
    return FugaContemPorValor(f, x->a->tipo) ? x->a : NULL; /* array, lista, map e soa por valor: gravar no elemento contamina o local */
}

/* O ponteiro (ou fatia) pelo qual a cadeia salta: 'p.c', 'p[i]' de fatia ou de qualquer ponteiro (^[N]T, ^[dynamic]T...) e 'p^'. */
static const Expr *PonteiroDoSalto(const Fuga *f, const Expr *x)
{
    if (x->tipo_no == EX_CAMPO) return x->a;
    if (x->tipo_no == EX_UNARIO && x->op == OP_DEREF) return x->a;
    if (x->tipo_no != EX_INDICE) return NULL;
    TipoClasse classe = TipoClasseDe(&f->s->tipos, x->a->tipo);
    return classe == TC_FATIA || classe == TC_PONTEIRO ? x->a : NULL;
}

/* 'p.c = v', 'p[i] = v', 'p^ = v': se p aponta para memoria que vive alem do proc e v tem endereco local, E0706. */
static bool GravarPorPonteiro(Fuga *f, const Expr *x, const Stmt *st)
{
    const Expr *ponteiro = PonteiroDoSalto(f, x);
    if (ponteiro == NULL) return true;
    FugaZerar(f, f->p);
    FugaAvaliar(f, ponteiro, f->p);
    return FugaArmazenar(f, &(FugaGravacao){st->expr, st->pos, NULL, -1});
}

static bool FugaAtribuicao(Fuga *f, const Stmt *st)
{
    const Expr *x = st->alvo;
    if (st->op != 0 || !FugaRastreia(f, x->tipo)) return true;
    FugaZerar(f, f->v);
    FugaAvaliar(f, st->expr, f->v);
    while (x->tipo_no != EX_NOME) {
        const Expr *base = BaseDireta(f, x);
        if (base == NULL) return GravarPorPonteiro(f, x, st);
        x = base;
    }
    if (x->ref == REF_LOCAL || x->ref == REF_PARAM) GuardarNoLocal(f, x->ref_id, st->expr);
    return true;
}

/* ---------- declaracoes ---------- */

static bool FugaLet(Fuga *f, const Stmt *st)
{
    /* Volta silenciosa: local sem ponteiro nao carrega origem e o nome so serve ao relato (locais zerados = nao rastreado). Na volta que
     * relata todo local entra, porque &x de um local sem ponteiro tambem e origem e a cadeia precisa do nome dele. */
    if (f->silencioso && !FugaRastreia(f, st->tipo_local)) return true;
    Declarar(f, st->local_id, st->nome, SemPosDepoisDaPalavra(f->s, st->pos), st->tipo_local);
    if (!Rastreado(f, st->local_id)) return true;
    FugaZerar(f, f->v);
    FugaAvaliar(f, st->expr, f->v);
    GuardarNoLocal(f, st->local_id, st->expr);
    return true;
}

/* 'let a, b = f()': o proc devolve a tupla; cada nome com ponteiro recebe a origem da chamada inteira (o resumo nao separa os elementos). */
static bool FugaLetMultiplo(Fuga *f, const Stmt *st)
{
    for (uint32_t k = 0; k < st->nnomes; k++) {
        const NomeLet *n = &st->nomes[k];
        Declarar(f, n->local_id, n->nome, n->pos, n->tipo);
        if (!Rastreado(f, n->local_id)) continue;
        FugaZerar(f, f->v);
        FugaAvaliar(f, st->expr, f->v);
        GuardarNoLocal(f, n->local_id, st->expr);
    }
    return true;
}

/* ---------- blocos e lacos ---------- */

static bool FugaBloco(Fuga *f, const Stmt *primeiro);

static bool FugaIf(Fuga *f, const Stmt *st)
{
    return FugaBloco(f, st->corpo) && FugaBloco(f, st->senao);
}

static bool FugaWhile(Fuga *f, const Stmt *st)
{
    f->laco = true;
    return FugaBloco(f, st->corpo);
}

/* O que o item de 'for item in L' / 'for &item in L' recebe: copia do elemento (recipiente por valor herda, o resto e leitura alheia) ou
 * ponteiro para ele (array por valor aponta para o proprio local; lista aponta para o heap; fatia e ^[N]T herdam). */
static void OrigemDoItem(Fuga *f, const Stmt *st, FugaConj dst)
{
    TipoClasse classe = TipoClasseDe(&f->s->tipos, st->expr->tipo);
    if (!st->item_ref) {
        if (FugaContemPorValor(f, st->expr->tipo)) FugaAvaliar(f, st->expr, dst);
        else FugaLigarFlag(f, dst, FUGA_DESC);
    } else if (classe == TC_ARRAY) {
        FugaEndereco(f, st->expr, dst);
    } else if (classe == TC_DINAMICO) {
        FugaLigarFlag(f, dst, FUGA_HEAP);
    } else {
        FugaAvaliar(f, st->expr, dst);
    }
}

static bool FugaFor(Fuga *f, const Stmt *st)
{
    f->laco = true;
    if (st->expr2 == NULL) {
        Declarar(f, st->local_id, st->nome, SemPosDepoisDaPalavra(f->s, st->pos), st->tipo_local);
        if (Rastreado(f, st->local_id)) {
            FugaZerar(f, f->v);
            OrigemDoItem(f, st, f->v);
            GuardarNoLocal(f, st->local_id, st->expr);
        }
    }
    return FugaBloco(f, st->corpo);
}

/* 'switch r in v': a ligacao de cada case recebe a origem do valor inteiro. */
static bool FugaSwitch(Fuga *f, const Stmt *st)
{
    for (uint32_t k = 0; k < st->ncasos; k++) {
        const Caso *c = &st->casos[k];
        Declarar(f, c->local_id, st->nome, SemPosDepoisDaPalavra(f->s, st->pos), c->tipo);
        if (Rastreado(f, c->local_id)) {
            FugaZerar(f, f->v);
            FugaAvaliar(f, st->expr, f->v);
            GuardarNoLocal(f, c->local_id, st->expr);
        }
        if (!FugaBloco(f, c->corpo)) return false;
    }
    return true;
}

static bool FugaDefer(Fuga *f, const Stmt *st)
{
    return FugaBloco(f, st->corpo);
}

static bool SemEfeito(Fuga *f, const Stmt *st)
{
    (void)f;
    (void)st;
    return true;
}

static const AndaComando COMANDOS_FUGA[ST_LET_MULTIPLO + 1] = {
    [ST_LET] = FugaLet, [ST_VAR] = FugaLet, [ST_ATRIBUICAO] = FugaAtribuicao, [ST_IF] = FugaIf, [ST_WHILE] = FugaWhile,
    [ST_FOR] = FugaFor, [ST_RETURN] = FugaReturn, [ST_CHAMADA] = SemEfeito, [ST_BREAK] = SemEfeito, [ST_CONTINUE] = SemEfeito,
    [ST_DEFER] = FugaDefer, [ST_SWITCH] = FugaSwitch, [ST_EXPECT] = SemEfeito, [ST_LET_MULTIPLO] = FugaLetMultiplo,
};

static bool FugaBloco(Fuga *f, const Stmt *primeiro)
{
    for (const Stmt *st = primeiro; st != NULL; st = st->prox) {
        if (!FugaEfeitos(f, st) || !COMANDOS_FUGA[st->tipo_no](f, st)) return false;
    }
    return true;
}

/* ---------- ponto de entrada ---------- */

/* O resumo deste proc: a entrada da tabela (D_PROC) ou, em test, um provisorio sem parametros que ninguem le. */
static FugaResumo *ResumoDoProc(Semantica *s, const Decl *d, FugaTabela *tab)
{
    if (d->tipo_no == D_PROC && d->indice >= 0 && (uint32_t)d->indice < tab->n) return &tab->proc[d->indice];
    FugaResumo *provisorio = ArenaAlocar(s->arena, sizeof *provisorio);
    provisorio->guarda = ArenaAlocar(s->arena, sizeof *provisorio->guarda);
    return provisorio;
}

static void FugaIniciar(Fuga *f, Semantica *s, const Decl *d, FugaTabela *tab)
{
    *f = (Fuga){.s = s, .proc = d, .nl = d->nlocais, .np = d->nparams, .tab = tab};
    f->meu = ResumoDoProc(s, d, tab);
    f->pal = (f->nl + f->np + FUGA_FLAGS + 63) / 64;
    size_t palavras_locais = ((size_t)f->nl * sizeof *f->locais + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    size_t palavras = ((size_t)f->nl + 2) * f->pal + palavras_locais; /* conj (nl), v e p (1 cada), locais */
    VecReservar(&tab->rascunho, palavras);
    uint64_t *area = tab->rascunho.dados;
    memset(area, 0, palavras * sizeof *area);
    f->conj = area;
    f->v = area + (size_t)f->nl * f->pal;
    f->p = f->v + f->pal;
    f->locais = (FugaLocal *)(f->p + f->pal);
    VecIniciar(&f->prov, sizeof(FugaProv));
    SemEntrarNaDecl(s, d); /* IA1-L8: os nomes de proc chamados no corpo se resolvem no modulo dele (FugaResultadoDaChamada, ResolverAlvo) */
    for (uint32_t i = 0; i < d->nparams; i++) {
        Declarar(f, (int32_t)i, d->params[i].nome, d->params[i].pos, d->params[i].tipo.tipo);
        if (f->locais[i].rastreado) FugaLigar(FugaConjDe(f, i), FugaBitParam(f, i));
    }
}

bool SemFugaProc(Semantica *s, const Decl *d, FugaTabela *tab, bool silencioso)
{
    Fuga f;
    FugaIniciar(&f, s, d, tab);
    f.silencioso = silencioso;
    bool ok = true;
    do {
        f.mudou = false;
        f.laco = false;
        ok = FugaBloco(&f, d->corpo);
    } while (ok && f.mudou && f.laco);
    VecLiberar(&f.prov);
    return ok && !(silencioso && f.candidato);
}
