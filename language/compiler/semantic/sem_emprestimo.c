/* sem_emprestimo.c - E0913: ponteiro para dentro de lista ou map usado depois que a mutacao da colecao pode ter movido o buffer.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6e P93k: if e switch com estado por ramo (ramo que sai por return/break/continue nao propaga; a invalidacao de qualquer ramo que segue vale; revalidar so se em todos); correcao PROVAVEL so para
 *                    append/insert (clear/remove/delete/resize viram dica); a correcao preserva CRLF. DIVIDA (P2): 'defer' segue ignorado (sem_emprestimo.c, default de VisitarComando).
 *            0.1.0 - M6e P93g: primeira versao (D-M6e-9; passe em ORDEM DO FONTE por proc/test, depois do corpo tipado sem erro).
 * Verificacao: construir_rapido.py && python ..\tests\semantica\verificar_semantica.py
 *
 * MODELO: 'let p = &l[i]' / 'l[..]' / 'get_ptr(&l, ...)' (l = nome de [dynamic]T ou map, local ou parametro ^[dynamic]T/^map) abre um emprestimo de p
 * sobre l. append/insert/insert_at/remove/delete/clear/resize de l PELO MESMO NOME invalida o emprestimo; ler p depois e E0913 (regra 1). Mutacao
 * dentro de laco com p declarado fora e p lido em qualquer ponto do corpo tambem (regra 2: o corpo roda de novo; o pre-passe do laco acha as
 * mutacoes). Mutar l dentro de 'for &x in l' e a regra 3. Nova atribuicao 'p = ...' revalida. Nao ha fluxo alem da ordem do fonte e do laco;
 * 'defer' fica de fora (roda no fim do escopo). Um diagnostico por par (emprestimo, mutacao): depois de relatado, p so volta a ser conferido
 * depois de revalidado.
 */
#include "semantic/sem_interno.h"

#include "common/fonte.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { INDICE_MAX = 24 };

typedef struct {
    int32_t var, colecao;    /* local_id do ponteiro e da colecao; colecao < 0 = emprestimo morto */
    const Stmt *decl;        /* o let/var que pegou o emprestimo (NULL = veio de atribuicao) */
    uint32_t pos_decl, pos_mut, quente_laco;
    int32_t nome_mut;
    bool invalido, relatado, mut_solta;
} Emprestimo;

typedef struct { int32_t colecao, item; uint32_t pos_for; } Percorre;
typedef struct { int32_t colecao, nome; uint32_t pos; bool solta; } Mutacao;
typedef struct { const char *ponteiro, *colecao, *mutacao; uint32_t linha_x, linha_y; } Achado;
typedef struct { uint32_t let_pos, linha_fim; char novo[DIAG_NOVO_MAX]; } Correcao;

typedef struct {
    Semantica *s;
    const Decl *proc;
    Vec emp, itera, muts;      /* Emprestimo, Percorre, Mutacao (pre-passe do laco) */
    uint8_t *imutavel;         /* por local_id: parametro, let, indice e item de for (o indice nao muda entre o emprestimo e a mutacao) */
    const Expr *comando;       /* expressao do ST_CHAMADA em curso */
    uint32_t serie;
    bool prescan, parar, erro;
} Emp;

static void VisitarExpr(Emp *m, const Expr *e);
static void VisitarBloco(Emp *m, const Stmt *st);

/* ---------- colecoes e emprestimos ---------- */

static bool EhLocalOuParam(const Expr *e)
{
    return e->tipo_no == EX_NOME && (e->ref == REF_LOCAL || e->ref == REF_PARAM);
}

/* local_id da lista/map que o argumento nomeia ('&l', 'l' ou o proprio ^[dynamic]T/^map); -1 nos demais. */
static int32_t ColecaoDe(const Emp *m, const Expr *e)
{
    if (e == NULL) return -1;
    if (e->tipo_no == EX_UNARIO && e->op == OP_ENDERECO) e = e->a;
    if (!EhLocalOuParam(e)) return -1;
    TipoId t = e->tipo;
    if (TipoClasseDe(&m->s->tipos, t) == TC_PONTEIRO) t = TipoDe(&m->s->tipos, t)->a;
    TipoClasse c = TipoClasseDe(&m->s->tipos, t);
    return c == TC_DINAMICO || c == TC_MAPA ? e->ref_id : -1;
}

/* A colecao cujo buffer o lvalor '&x' aponta: precisa passar por um indice da propria lista ('&l' sozinho e o endereco do cabecalho). */
static int32_t RaizDoIndice(const Emp *m, const Expr *x)
{
    while (x != NULL && (x->tipo_no == EX_CAMPO || x->tipo_no == EX_INDICE)) {
        int32_t c = x->tipo_no == EX_INDICE ? ColecaoDe(m, x->a) : -1;
        if (c >= 0) return c;
        if (x->tipo_no == EX_CAMPO && TipoClasseDe(&m->s->tipos, x->a->tipo) == TC_PONTEIRO) return -1;
        x = x->a;
    }
    return -1;
}

/* A colecao que o valor de 'e' empresta: &l[i], l[..], get_ptr(&m, ...) (com or_else, as, ?). */
static int32_t RaizDoEmprestimo(const Emp *m, const Expr *e)
{
    if (e == NULL) return -1;
    switch (e->tipo_no) {
    case EX_UNARIO: return e->op == OP_ENDERECO ? RaizDoIndice(m, e->a) : -1;
    case EX_FATIA: return ColecaoDe(m, e->a);
    case EX_CHAMADA: return e->ref == REF_EMBUTIDO && e->ref_id == EMB_GET_PTR && e->nargs > 0 ? ColecaoDe(m, e->args[0].valor) : -1;
    case EX_OR_ELSE: {
        int32_t c = RaizDoEmprestimo(m, e->a);
        return c >= 0 ? c : RaizDoEmprestimo(m, e->b);
    }
    case EX_EMBRULHA:
    case EX_CONVERSAO:
    case EX_TRY: return RaizDoEmprestimo(m, e->a);
    default: return -1;
    }
}

static Emprestimo *EmprestimoDe(const Emp *m, int32_t var)
{
    for (size_t i = 0; i < m->emp.n; i++) {
        Emprestimo *e = VecEm(&m->emp, i);
        if (e->var == var) return e;
    }
    return NULL;
}

static void AbrirEmprestimo(Emp *m, int32_t var, int32_t colecao, const Stmt *decl, uint32_t pos)
{
    Emprestimo *e = VecEmpurrar(&m->emp);
    e->var = var;
    e->colecao = colecao;
    e->decl = decl;
    e->pos_decl = pos;
}

/* ---------- texto do erro e da correcao ---------- */

static const char *NomeDoLocal(const Emp *m, int32_t id)
{
    if (m->proc->nomes_locais == NULL || id < 0 || (uint32_t)id >= m->proc->nlocais) return "?";
    return SemNomeTexto(m->s, m->proc->nomes_locais[id].nome);
}

static uint32_t LinhaDe(const Emp *m, uint32_t pos)
{
    return FonteLocalizarNoArquivo(m->s->fonte, pos).linha;
}

static void Guardar(Emp *m)
{
    m->erro = true;
    if (!SemGuardarErro(m->s, m->proc->modulo)) m->parar = true;
}

static void EscreverErro(Emp *m, uint32_t pos, const Achado *a)
{
    SemErro(m->s, E0913, pos,
            "'%s' aponta para dentro de '%s' (linha %u) e o %s da linha %u pode mover o buffer: guarde o indice e leia %s[i] depois, ou pegue &%s[i] de novo depois do %s",
            a->ponteiro, a->colecao, a->linha_x, a->mutacao, a->linha_y, a->colecao, a->colecao, a->mutacao);
}

/* O indice de 'let p = &l[E]' como texto, so quando E e literal inteiro ou nome imutavel (nao muda entre o emprestimo e a mutacao). */
static bool TextoDoIndice(const Emp *m, const Expr *init, char *saida)
{
    if (init == NULL || init->tipo_no != EX_UNARIO || init->op != OP_ENDERECO) return false;
    const Expr *x = init->a;
    if (x->tipo_no != EX_INDICE || x->nargs != 0 || !EhLocalOuParam(x->a)) return false;
    const Expr *b = x->b;
    if (b->tipo_no == EX_NOME) {
        bool imutavel = EhLocalOuParam(b) && b->ref_id >= 0 && (uint32_t)b->ref_id < m->proc->nlocais && m->imutavel[b->ref_id];
        return imutavel && snprintf(saida, INDICE_MAX, "%s", SemNomeTexto(m->s, b->nome)) < INDICE_MAX;
    }
    if (b->tipo_no != EX_INT) return false;
    const char *t = m->s->fonte->texto + b->pos;
    size_t n = 0;
    while ((isalnum((unsigned char)t[n]) || t[n] == '_') && n < INDICE_MAX - 1) n++;
    if (n == 0 || isalnum((unsigned char)t[n])) return false;
    memcpy(saida, t, n);
    saida[n] = '\0';
    return true;
}

/* 'let' -> 'var' e "p = &l[E]" logo depois da linha da mutacao (comando de chamada sozinho na linha). */
static bool CalcularCorrecao(const Emp *m, const Emprestimo *e, Correcao *c)
{
    const char *t = m->s->fonte->texto;
    char indice[INDICE_MAX];
    if (e->decl == NULL || e->decl->tipo_no != ST_LET || !e->mut_solta || strncmp(t + e->decl->pos, "let", 3) != 0) return false;
    const char *mutacao = SemNomeTexto(m->s, e->nome_mut);
    /* P93k: a correcao PROVAVEL so vale para append/insert (o elemento do indice segue existindo); clear/remove/delete/resize ficam em dica */
    if (strcmp(mutacao, "append") != 0 && strcmp(mutacao, "insert") != 0) return false;
    if (!TextoDoIndice(m, e->decl->expr, indice)) return false;
    uint32_t ini = e->pos_mut, fim = e->pos_mut;
    while (ini > 0 && (t[ini - 1] == ' ' || t[ini - 1] == '\t')) ini--;
    if (ini > 0 && t[ini - 1] != '\n') return false;
    while (t[fim] != '\0' && t[fim] != '\n' && t[fim] != '\r') fim++;
    int n = snprintf(c->novo, sizeof c->novo, "%s%.*s%s = &%s[%s]", t[fim] == '\r' ? "\r\n" : "\n", (int)(e->pos_mut - ini), t + ini,
                     NomeDoLocal(m, e->var), NomeDoLocal(m, e->colecao), indice);
    if (n < 0 || n >= (int)sizeof c->novo) return false;
    c->let_pos = e->decl->pos;
    c->linha_fim = fim;
    return true;
}

static void SugerirReemprestimo(Emp *m, const Emprestimo *e, const Achado *a)
{
    DiagSugestoes *sug = &m->s->diag->sug;
    Correcao c;
    if (!CalcularCorrecao(m, e, &c)) {
        DiagSugerir(sug, DIAG_SO_DICA, "guarde o indice e leia %s[i] depois do %s, ou pegue o endereco de novo depois dele", a->colecao, a->mutacao);
        return;
    }
    DiagSugerir(sug, DIAG_PROVAVEL, "troque let por var e pegue %s = &%s[...] de novo logo depois do %s", a->ponteiro, a->colecao, a->mutacao);
    if (DiagEditar(sug, c.let_pos, c.let_pos + 3, "var") && DiagEditar(sug, c.linha_fim, c.linha_fim, c.novo)) return;
    sug->itens[sug->n - 1].nedicoes = 0;
    sug->itens[sug->n - 1].confianca = DIAG_SO_DICA;
}

static void RelatarEmprestimo(Emp *m, const Emprestimo *e, uint32_t pos_uso)
{
    Achado a = {NomeDoLocal(m, e->var), NomeDoLocal(m, e->colecao), SemNomeTexto(m->s, e->nome_mut), LinhaDe(m, e->pos_decl),
                LinhaDe(m, e->pos_mut)};
    EscreverErro(m, pos_uso, &a);
    SugerirReemprestimo(m, e, &a);
    Guardar(m);
}

/* ---------- uso e mutacao ---------- */

static void UsoDeNome(Emp *m, const Expr *e)
{
    if (m->prescan || !EhLocalOuParam(e)) return;
    Emprestimo *emp = EmprestimoDe(m, e->ref_id);
    if (emp == NULL || emp->colecao < 0 || emp->relatado || (!emp->invalido && emp->quente_laco == 0)) return;
    emp->relatado = true;
    RelatarEmprestimo(m, emp, e->pos);
}

/* Regra 3: mutar a colecao dentro de 'for &x in l'. A posicao e a da chamada que muta. */
static void ConferirPercorridos(Emp *m, const Mutacao *mut)
{
    for (size_t i = 0; i < m->itera.n && !m->parar; i++) {
        const Percorre *p = VecEm(&m->itera, i);
        if (p->colecao != mut->colecao) continue;
        Achado a = {NomeDoLocal(m, p->item), NomeDoLocal(m, p->colecao), SemNomeTexto(m->s, mut->nome), LinhaDe(m, p->pos_for),
                    LinhaDe(m, mut->pos)};
        EscreverErro(m, mut->pos, &a);
        DiagSugerir(&m->s->diag->sug, DIAG_SO_DICA, "junte os novos numa lista auxiliar e faca o %s depois do for", a.mutacao);
        Guardar(m);
    }
}

static void Mutar(Emp *m, const Expr *e)
{
    int32_t colecao = e->nargs > 0 ? ColecaoDe(m, e->args[0].valor) : -1;
    if (colecao < 0) return;
    Mutacao mut = {colecao, e->nome, e->pos, e == m->comando};
    if (m->prescan) {
        *(Mutacao *)VecEmpurrar(&m->muts) = mut;
        return;
    }
    ConferirPercorridos(m, &mut);
    for (size_t i = 0; i < m->emp.n; i++) {
        Emprestimo *emp = VecEm(&m->emp, i);
        if (emp->colecao != colecao || emp->invalido) continue;
        emp->invalido = true;
        emp->pos_mut = mut.pos;
        emp->nome_mut = mut.nome;
        emp->mut_solta = mut.solta;
    }
}

static void VisitarExpr(Emp *m, const Expr *e)
{
    if (e == NULL || m->parar) return;
    VisitarExpr(m, e->a);
    VisitarExpr(m, e->b);
    for (uint32_t i = 0; i < e->nargs; i++) VisitarExpr(m, e->args[i].valor);
    if (e->tipo_no == EX_NOME) UsoDeNome(m, e);
    else if (e->tipo_no == EX_CHAMADA && e->ref == REF_EMBUTIDO && SemEmbutidoMutaColecao(e->ref_id)) Mutar(m, e);
}

/* ---------- lacos ---------- */

static const Mutacao *MutacaoDa(const Emp *m, size_t desde, int32_t colecao)
{
    for (size_t k = desde; k < m->muts.n; k++) {
        const Mutacao *mut = VecEm(&m->muts, k);
        if (mut->colecao == colecao) return mut;
    }
    return NULL;
}

/* Pre-passe: as mutacoes do corpo (e da condicao) deixam "quente" todo emprestimo ja aberto sobre a mesma colecao. Devolve a serie do laco. */
static uint32_t MarcarQuentes(Emp *m, const Expr *cond, const Stmt *corpo)
{
    size_t desde = m->muts.n;
    m->prescan = true;
    VisitarExpr(m, cond);
    VisitarBloco(m, corpo);
    m->prescan = false;
    uint32_t serie = ++m->serie;
    for (size_t i = 0; i < m->emp.n; i++) {
        Emprestimo *emp = VecEm(&m->emp, i);
        const Mutacao *mut = emp->colecao >= 0 && emp->quente_laco == 0 ? MutacaoDa(m, desde, emp->colecao) : NULL;
        if (mut == NULL) continue;
        emp->quente_laco = serie;
        if (emp->invalido) continue;
        emp->pos_mut = mut->pos;
        emp->nome_mut = mut->nome;
        emp->mut_solta = mut->solta;
    }
    m->muts.n = desde;
    return serie;
}

static void Laco(Emp *m, const Expr *cond, const Stmt *corpo)
{
    uint32_t serie = m->prescan ? 0 : MarcarQuentes(m, cond, corpo);
    VisitarExpr(m, cond);
    VisitarBloco(m, corpo);
    if (serie == 0) return;
    for (size_t i = 0; i < m->emp.n; i++) {
        Emprestimo *emp = VecEm(&m->emp, i);
        if (emp->quente_laco == serie) emp->quente_laco = 0;
    }
}

static void MarcarImutavel(const Emp *m, int32_t id)
{
    if (id >= 0 && (uint32_t)id < m->proc->nlocais) m->imutavel[id] = 1;
}

static void Para(Emp *m, const Stmt *st)
{
    VisitarExpr(m, st->expr);
    VisitarExpr(m, st->expr2);
    int32_t colecao = -1;
    if (!m->prescan) {
        MarcarImutavel(m, st->local_id);
        MarcarImutavel(m, st->local_indice);
        colecao = st->expr2 == NULL && st->item_ref ? ColecaoDe(m, st->expr) : -1;
    }
    if (colecao >= 0) *(Percorre *)VecEmpurrar(&m->itera) = (Percorre){colecao, st->local_id, st->pos};
    Laco(m, NULL, st->corpo);
    if (colecao >= 0) m->itera.n--;
}

/* ---------- comandos ---------- */

static void DeclararLocal(Emp *m, const Stmt *st)
{
    VisitarExpr(m, st->expr);
    if (m->prescan || st->local_id < 0 || (uint32_t)st->local_id >= m->proc->nlocais) return;
    m->imutavel[st->local_id] = st->tipo_no == ST_LET;
    int32_t colecao = RaizDoEmprestimo(m, st->expr);
    if (colecao >= 0) AbrirEmprestimo(m, st->local_id, colecao, st, st->pos);
}

/* 'p = valor': revalida o emprestimo de p (ou o abre, se o valor empresta uma colecao). */
static void Revalidar(Emp *m, const Stmt *st)
{
    int32_t colecao = RaizDoEmprestimo(m, st->expr);
    Emprestimo *emp = EmprestimoDe(m, st->alvo->ref_id);
    if (emp == NULL) {
        if (colecao >= 0) AbrirEmprestimo(m, st->alvo->ref_id, colecao, NULL, st->pos);
        return;
    }
    emp->colecao = colecao;
    emp->decl = NULL;
    emp->pos_decl = st->pos;
    emp->invalido = emp->relatado = false;
    emp->quente_laco = 0;
}

static void Atribuir(Emp *m, const Stmt *st)
{
    if (st->op == 0 && EhLocalOuParam(st->alvo)) {
        VisitarExpr(m, st->expr);
        if (!m->prescan) Revalidar(m, st);
        return;
    }
    VisitarExpr(m, st->alvo);
    VisitarExpr(m, st->expr);
}

/* ---------- ramificacao (P93k): estado por ramo ---------- */

typedef struct { Vec estado; bool sai; } Ramo; /* o estado dos emprestimos no fim do ramo; sai = o ramo termina em return/break/continue */

static void SalvarEstado(const Emp *m, Vec *destino)
{
    VecIniciar(destino, sizeof(Emprestimo));
    for (size_t i = 0; i < m->emp.n; i++) *(Emprestimo *)VecEmpurrar(destino) = *(const Emprestimo *)VecEm(&m->emp, i);
}

static void RestaurarEstado(Emp *m, const Vec *origem)
{
    m->emp.n = 0;
    for (size_t i = 0; i < origem->n; i++) *(Emprestimo *)VecEmpurrar(&m->emp) = *(const Emprestimo *)VecEm(origem, i);
}

/* O bloco nao chega ao fim: return, break ou continue no nivel dele, ou um if/else em que os dois ramos saem. */
static bool BlocoSai(const Stmt *st)
{
    for (; st != NULL; st = st->prox) {
        if (st->tipo_no == ST_RETURN || st->tipo_no == ST_BREAK || st->tipo_no == ST_CONTINUE) return true;
        if (st->tipo_no == ST_IF && st->senao != NULL && BlocoSai(st->corpo) && BlocoSai(st->senao)) return true;
    }
    return false;
}

static void VisitarRamo(Emp *m, const Vec *base, const Stmt *corpo, Ramo *ramo)
{
    RestaurarEstado(m, base);
    VisitarBloco(m, corpo);
    ramo->sai = BlocoSai(corpo);
    SalvarEstado(m, &ramo->estado);
}

/* Estado depois da ramificacao: ramo que sai nao propaga; a invalidacao de qualquer ramo que segue vale (conservador); revalidar so conta se ocorrer em todos os que seguem.
 * Emprestimo aberto dentro de um ramo (variavel declarada antes, atribuida la) entra se ainda nao existe. */
static void JuntarRamos(Emp *m, const Vec *base, const Ramo *ramos, size_t n)
{
    RestaurarEstado(m, base);
    for (size_t i = 0; i < base->n; i++) {
        const Emprestimo *fonte = NULL;
        for (size_t r = 0; r < n; r++) {
            const Emprestimo *x = ramos[r].sai ? NULL : VecEm(&ramos[r].estado, i);
            if (x != NULL && (fonte == NULL || (x->invalido && !fonte->invalido))) fonte = x;
        }
        if (fonte != NULL) *(Emprestimo *)VecEm(&m->emp, i) = *fonte;
    }
    for (size_t r = 0; r < n; r++) {
        if (ramos[r].sai) continue;
        for (size_t k = base->n; k < ramos[r].estado.n; k++) {
            const Emprestimo *x = VecEm(&ramos[r].estado, k);
            if (EmprestimoDe(m, x->var) == NULL) *(Emprestimo *)VecEmpurrar(&m->emp) = *x;
        }
    }
}

/* Visita n corpos (NULL = caminho vazio que segue) a partir do mesmo estado e junta o resultado. */
static void Ramificar(Emp *m, const Stmt *const *corpos, size_t n)
{
    Vec base;
    Ramo *ramos = ArenaAlocar(m->s->arena, n * sizeof *ramos);
    memset(ramos, 0, n * sizeof *ramos);
    SalvarEstado(m, &base);
    for (size_t r = 0; r < n && !m->parar; r++) VisitarRamo(m, &base, corpos[r], &ramos[r]);
    if (!m->parar) JuntarRamos(m, &base, ramos, n);
    for (size_t r = 0; r < n; r++) {
        if (ramos[r].estado.dados != NULL) VecLiberar(&ramos[r].estado);
    }
    VecLiberar(&base);
}

static void Se(Emp *m, const Stmt *st)
{
    VisitarExpr(m, st->expr);
    if (m->prescan) {
        VisitarBloco(m, st->corpo);
        VisitarBloco(m, st->senao);
        return;
    }
    const Stmt *const corpos[2] = {st->corpo, st->senao};
    Ramificar(m, corpos, 2);
}

static void Trocar(Emp *m, const Stmt *st)
{
    VisitarExpr(m, st->expr);
    if (m->prescan) {
        for (uint32_t k = 0; k < st->ncasos && !m->parar; k++) VisitarBloco(m, st->casos[k].corpo);
        return;
    }
    bool tem_padrao = false;
    const Stmt **corpos = ArenaAlocar(m->s->arena, ((size_t)st->ncasos + 1) * sizeof *corpos);
    for (uint32_t k = 0; k < st->ncasos; k++) {
        corpos[k] = st->casos[k].corpo;
        tem_padrao = tem_padrao || st->casos[k].padrao;
    }
    corpos[st->ncasos] = NULL; /* sem default o switch pode nao entrar em nenhum case: o estado de antes tambem segue */
    Ramificar(m, (const Stmt *const *)corpos, tem_padrao ? st->ncasos : (size_t)st->ncasos + 1);
}

static void LetMultiplo(Emp *m, const Stmt *st)
{
    VisitarExpr(m, st->expr);
    if (m->prescan) return;
    for (uint32_t k = 0; k < st->nnomes; k++) {
        if (st->nomes[k].local_id >= 0 && (uint32_t)st->nomes[k].local_id < m->proc->nlocais) m->imutavel[st->nomes[k].local_id] = st->op == 0;
    }
}

static void VisitarComando(Emp *m, const Stmt *st)
{
    switch (st->tipo_no) {
    case ST_LET:
    case ST_VAR: DeclararLocal(m, st); break;
    case ST_ATRIBUICAO: Atribuir(m, st); break;
    case ST_IF: Se(m, st); break;
    case ST_WHILE: Laco(m, st->expr, st->corpo); break;
    case ST_FOR: Para(m, st); break;
    case ST_CHAMADA:
        m->comando = st->expr;
        VisitarExpr(m, st->expr);
        break;
    case ST_SWITCH: Trocar(m, st); break;
    case ST_LET_MULTIPLO: LetMultiplo(m, st); break;
    case ST_RETURN:
    case ST_EXPECT:
        VisitarExpr(m, st->expr);
        VisitarExpr(m, st->expr2);
        break;
    default: break; /* break, continue e defer (roda no fim do escopo, fora da ordem do fonte) */
    }
}

static void VisitarBloco(Emp *m, const Stmt *st)
{
    for (; st != NULL && !m->parar; st = st->prox) VisitarComando(m, st);
}

/* Confere o proc/test ja tipado sem erro. false = algum E0913 foi guardado na lista. */
bool SemEmprestimoCorpo(Semantica *s, const Decl *d)
{
    Emp m = {.s = s, .proc = d};
    VecIniciar(&m.emp, sizeof(Emprestimo));
    VecIniciar(&m.itera, sizeof(Percorre));
    VecIniciar(&m.muts, sizeof(Mutacao));
    m.imutavel = ArenaAlocar(s->arena, (size_t)d->nlocais + 1);
    memset(m.imutavel, 0, (size_t)d->nlocais + 1);
    for (uint32_t i = 0; i < d->nparams && i < d->nlocais; i++) m.imutavel[i] = 1;
    VisitarBloco(&m, d->corpo);
    VecLiberar(&m.emp);
    VecLiberar(&m.itera);
    VecLiberar(&m.muts);
    return !m.erro;
}
