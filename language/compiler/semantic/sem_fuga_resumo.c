/* sem_fuga_resumo.c - resumo por callee da fuga de endereco (K1): tabela por Decl.indice, ponto fixo sobre os procs e resultado de chamada.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.11 L7: ConferirCorpos guarda o E0706 de cada corpo na lista (SemGuardarErro) e segue.
 *            0.1.0 - Linguagem v1.11 L3: primeira versao (FugaResumo, FugaPontoFixo, SemFugaPrograma, FugaResultadoDaChamada).
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_fuga.h"

/* ---------- ponto fixo ---------- */

/* Voltas sobre os n procs na ordem do fonte, ate uma volta inteira em que nenhum resumo cresceu. O resumo so cresce (monotono) e o
 * numero de bits e finito, entao termina; 'teto' voltas sem convergir e erro interno (false). */
bool FugaPontoFixo(uint32_t n, uint32_t teto, FugaRodada rodada, void *ctx)
{
    for (uint32_t volta = 0; volta < teto; volta++) {
        bool mudou = false;
        for (uint32_t i = 0; i < n; i++) mudou |= rodada(ctx, i);
        if (!mudou) return true;
    }
    return false;
}

/* ---------- mascaras ---------- */

uint64_t FugaMascaraParams(const Fuga *f, const FugaConj c)
{
    uint64_t mascara = 0;
    for (uint32_t n = 0; n < f->np && n < FUGA_PARAMS_MAX; n++) {
        if (FugaTem(c, FugaBitParam(f, n))) mascara |= 1ull << n;
    }
    return mascara;
}

uint8_t FugaMascaraFlags(const Fuga *f, const FugaConj c)
{
    uint8_t mascara = 0;
    for (uint32_t g = 0; g < FUGA_FLAGS; g++) {
        if (FugaTem(c, FugaBitFlag(f, (FugaFlag)g))) mascara |= (uint8_t)(1u << g);
    }
    return mascara;
}

/* ---------- registro no resumo do proc ---------- */

static void SomarMascara(Fuga *f, uint64_t *destino, uint64_t mascara)
{
    if ((mascara & ~*destino) == 0) return;
    *destino |= mascara;
    f->tab->mudou = true;
}

void FugaRegistrarRetorno(Fuga *f, const FugaConj v)
{
    uint8_t flags = FugaMascaraFlags(f, v);
    SomarMascara(f, &f->meu->retorno_de, FugaMascaraParams(f, v));
    if ((flags & ~f->meu->flags) == 0) return;
    f->meu->flags |= flags;
    f->tab->mudou = true;
}

/* v foi guardado em memoria apontada por p: cada parametro apontado por p ganha os parametros de v; origem alheia ao proc vai em guarda[np]. */
void FugaRegistrarGuarda(Fuga *f, const FugaConj p, const FugaConj v)
{
    uint64_t mascara = FugaMascaraParams(f, v);
    if (mascara == 0) return;
    for (uint32_t d = 0; d < f->np; d++) {
        if (FugaTem(p, FugaBitParam(f, d))) SomarMascara(f, &f->meu->guarda[d], mascara);
    }
    if (FugaMascaraFlags(f, p) != 0) SomarMascara(f, &f->meu->guarda[f->np], mascara);
}

/* Resumo do proc da Forja; NULL em foreign e em proc sem ponteiro nenhum (nao entra na fuga: nao devolve nem guarda ponteiro). */
const FugaResumo *FugaResumoDe(const Fuga *f, const Decl *d)
{
    if (d->externo || d->tipo_no != D_PROC || d->indice < 0 || (uint32_t)d->indice >= f->tab->n) return NULL;
    const FugaResumo *r = &f->tab->proc[d->indice];
    return r->guarda == NULL ? NULL : r;
}

/* Chamada de proc da Forja: o resultado herda as origens dos argumentos que o resumo diz chegarem ao retorno, mais as flags do resumo.
 * foreign continua FOREIGN (o compilador nao enxerga o C); sem Decl ou sem resumo, desconhecido. */
void FugaResultadoDaChamada(Fuga *f, const Expr *e, FugaConj dst)
{
    const Decl *decl = e->ref == REF_PROC ? SemAcharSimbolo(f->s, e->nome, e->modulo)->decl : NULL;
    const FugaResumo *r = decl == NULL ? NULL : FugaResumoDe(f, decl);
    if (r == NULL) {
        FugaLigarFlag(f, dst, decl != NULL && decl->externo ? FUGA_FOREIGN : FUGA_DESC);
        return;
    }
    for (uint32_t g = 0; g < FUGA_FLAGS; g++) {
        if ((r->flags >> g) & 1) FugaLigarFlag(f, dst, (FugaFlag)g);
    }
    for (uint32_t k = 0; k < e->nargs; k++) {
        uint32_t i = (uint32_t)e->args[k].indice;
        if (i < FUGA_PARAMS_MAX && ((r->retorno_de >> i) & 1)) FugaAvaliar(f, e->args[k].valor, dst);
    }
}

/* ---------- programa ---------- */

static Decl *CorpoDe(const CorposDaFuga *c, uint32_t i)
{
    return *(Decl **)VecEm(&c->corpos, i);
}

void FugaCorposIniciar(CorposDaFuga *c, Semantica *s)
{
    *c = (CorposDaFuga){.s = s};
    VecIniciar(&c->corpos, sizeof(Decl *));
    VecIniciar(&c->tab.rascunho, sizeof(uint64_t));
}

void FugaCorposLiberar(CorposDaFuga *c)
{
    VecLiberar(&c->corpos);
    VecLiberar(&c->tab.rascunho);
}

/* Chamada pelo passe B logo depois de tipar o corpo de 'd' (a lista de Decl ja esta no cache; percorre-la de novo custava +0,4 ms no corpus de
 * 200 mil linhas). Uma entrada por proc da Forja com ponteiro (os demais ficam zerados: nao devolvem nem guardam ponteiro; foreign: fronteira
 * insegura); guarda[] com nparams + 1 palavras. A tabela so nasce com o 1o corpo relevante. */
void FugaCorposRegistrar(CorposDaFuga *c, Decl *d)
{
    FugaTabela *tab = &c->tab;
    if (!d->fuga_relevante) return;
    if (tab->proc == NULL) {
        tab->n = (uint32_t)c->s->qtd_proc;
        tab->proc = ArenaAlocar(c->s->arena, ((size_t)tab->n + 1) * sizeof *tab->proc);
    }
    *(Decl **)VecEmpurrar(&c->corpos) = d;
    if (d->tipo_no != D_PROC) return;
    c->nprocs++;
    if (d->indice < 0 || (uint32_t)d->indice >= tab->n) return;
    FugaResumo *r = &tab->proc[d->indice];
    r->nparams = d->nparams;
    r->guarda = ArenaAlocar(c->s->arena, ((size_t)d->nparams + 1) * sizeof *r->guarda);
}

/* Volta silenciosa: refaz o resumo do corpo i e anota em 'suspeito' se algum endereco local chegaria a um sumidouro com os resumos de agora. */
static bool RodadaDoCorpo(void *ctx, uint32_t i)
{
    CorposDaFuga *c = ctx;
    Decl *d = CorpoDe(c, i);
    if (d->tipo_no != D_PROC) return false; /* test nao e chamado: nao tem resumo */
    c->tab.mudou = false;
    c->suspeito[i] = !SemFugaProc(c->s, d, &c->tab, true);
    return c->tab.mudou;
}

/* A volta que relata: so os suspeitos da ultima volta (resumos ja convergidos) e os test, na ordem do fonte; o 1o E0706 de cada corpo vai para a
 * lista de diagnosticos (um erro por declaracao) e a volta segue no proximo, ate a lista encher. */
static bool ConferirCorpos(CorposDaFuga *c)
{
    bool ok = true;
    for (uint32_t i = 0; i < c->corpos.n; i++) {
        Decl *d = CorpoDe(c, i);
        if ((!c->suspeito[i] && d->tipo_no != D_TEST) || SemFugaProc(c->s, d, &c->tab, false)) continue;
        ok = false;
        if (!c->s->diag->tem) continue; /* sem diagnostico nao ha o que guardar */
        d->com_erro = true;
        if (!SemGuardarErro(c->s, d->modulo)) break;
    }
    return ok;
}

/* Fim do passe B: resumos ate o ponto fixo (voltas silenciosas) e depois a volta que relata. Ponto fixo que nao converge devolve false sem
 * diagnostico (erro interno, exit 3). Corpo sem ponteiro nenhum fica de fora (nao tem endereco para escapar); a unica perda e o destino que
 * e resultado de chamada sem local nenhum (guardar(instancia(), &x)). */
bool SemFugaPrograma(CorposDaFuga *c)
{
    c->suspeito = ArenaAlocar(c->s->arena, c->corpos.n + 1);
    return FugaPontoFixo((uint32_t)c->corpos.n, c->nprocs + 1, RodadaDoCorpo, c) && ConferirCorpos(c);
}
