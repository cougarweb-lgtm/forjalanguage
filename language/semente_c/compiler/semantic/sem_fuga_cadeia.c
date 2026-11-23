/* sem_fuga_cadeia.c - de onde veio cada endereco local (proveniencia) e o relato do E0706 com a cadeia.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.11 L3: SUM_CALLEE ("o proc 'f' guarda o parametro 'p' em memoria que sobrevive a chamada", elo final "chamada a f");
 *                    FugaPortador entra nos argumentos de proc da Forja; sem proveniencia nas voltas silenciosas.
 *            0.1.0 - Linguagem v1.11 L2: primeira versao; causas antigas ('return &x', 'return x[..]') mantidas nos casos diretos.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_fuga.h"

#include <stdio.h>
#include <string.h>

enum { CADEIA_PORTADORES_MAX = 6, CADEIA_ELEMENTOS_MAX = CADEIA_PORTADORES_MAX + 2, CADEIA_JSON_MAX = 360 };
enum { CAUSA_GERAL, CAUSA_ENDERECO, CAUSA_FATIA }; /* causas antigas do E0706 sintatico, mantidas nos casos diretos */

typedef struct { const char *nome; uint32_t pos; } ElementoCadeia;
typedef struct { ElementoCadeia el[CADEIA_ELEMENTOS_MAX]; uint32_t n; } Cadeia;

/* ---------- proveniencia ---------- */

static int32_t PortadorEmArgs(const Fuga *f, const Expr *e, uint32_t bit, int32_t excluir)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        int32_t c = FugaPortador(f, e->args[i].valor, bit, excluir);
        if (c >= 0) return c;
    }
    return -1;
}

static int32_t PortadorDoNome(const Fuga *f, const Expr *e, uint32_t bit, int32_t excluir)
{
    bool local = (e->ref == REF_LOCAL || e->ref == REF_PARAM) && (uint32_t)e->ref_id < f->nl;
    return local && e->ref_id != excluir && FugaTem(FugaConjDe(f, (uint32_t)e->ref_id), bit) ? e->ref_id : -1;
}

/* Variavel de 'e' cujo conjunto tem LOCAL(bit). Numa chamada entra nos argumentos (o resultado de proc da Forja e de slice_from carrega o
 * local do argumento); os demais embutidos e foreign nao carregam. */
int32_t FugaPortador(const Fuga *f, const Expr *e, uint32_t bit, int32_t excluir)
{
    if (e == NULL) return -1;
    if (e->tipo_no == EX_NOME) return PortadorDoNome(f, e, bit, excluir);
    if (e->tipo_no == EX_CHAMADA) return e->ref == REF_EMBUTIDO && e->ref_id != EMB_SLICE_FROM ? -1 : PortadorEmArgs(f, e, bit, excluir);
    int32_t c = FugaPortador(f, e->a, bit, excluir);
    if (c < 0) c = FugaPortador(f, e->b, bit, excluir);
    return c >= 0 ? c : PortadorEmArgs(f, e, bit, excluir);
}

void FugaProvRegistrar(Fuga *f, uint32_t var, uint32_t bit, const Expr *origem)
{
    if (f->silencioso) return; /* a proveniencia so serve ao relato */
    int32_t de = FugaPortador(f, origem, bit, (int32_t)var);
    FugaProv *p = VecEmpurrar(&f->prov);
    p->var = (int32_t)var;
    p->bit = (int32_t)bit;
    p->de = de;
}

static int32_t ProvDe(const Fuga *f, int32_t var, uint32_t bit)
{
    for (size_t i = 0; i < f->prov.n; i++) {
        const FugaProv *p = VecEm(&f->prov, i);
        if (p->var == var && (uint32_t)p->bit == bit) return p->de;
    }
    return -1;
}

/* ---------- sumidouro ---------- */

static const FugaFlag FLAGS_POR_PRIORIDADE[] = {FUGA_HEAP, FUGA_GLOBAL, FUGA_FOREIGN, FUGA_DESC};
static const FugaSumTipo SUM_DO_FLAG[FUGA_FLAGS] = {[FUGA_HEAP] = SUM_HEAP, [FUGA_GLOBAL] = SUM_GLOBAL, [FUGA_FOREIGN] = SUM_FOREIGN, [FUGA_DESC] = SUM_DESC};

/* O ponteiro de destino aponta para memoria que vive alem do proc? Parametro primeiro, depois heap, global, foreign e desconhecido. */
bool FugaSumidouroDe(const Fuga *f, const FugaConj p, uint32_t pos, FugaSumidouro *saida)
{
    *saida = (FugaSumidouro){SUM_PARAM, -1, pos, -1};
    for (uint32_t n = 0; n < f->np; n++) {
        if (!FugaTem(p, FugaBitParam(f, n))) continue;
        saida->nome = f->proc->params[n].nome;
        return true;
    }
    for (size_t i = 0; i < sizeof FLAGS_POR_PRIORIDADE / sizeof FLAGS_POR_PRIORIDADE[0]; i++) {
        if (!FugaTem(p, FugaBitFlag(f, FLAGS_POR_PRIORIDADE[i]))) continue;
        saida->tipo = SUM_DO_FLAG[FLAGS_POR_PRIORIDADE[i]];
        return true;
    }
    return false;
}

static void RotuloDoSumidouro(const Fuga *f, const FugaSumidouro *sum, char *saida, size_t cap)
{
    switch (sum->tipo) {
    case SUM_RETURN: snprintf(saida, cap, "return"); break;
    case SUM_PARAM: snprintf(saida, cap, "memoria do parametro %s", SemNomeTexto(f->s, sum->nome)); break;
    case SUM_HEAP: snprintf(saida, cap, "heap"); break;
    case SUM_GLOBAL: snprintf(saida, cap, "global"); break;
    case SUM_FOREIGN: snprintf(saida, cap, "memoria de foreign"); break;
    case SUM_CALLEE:
        snprintf(saida, cap, "o proc '%s' guarda o parametro '%s' em memoria que sobrevive a chamada", SemNomeTexto(f->s, sum->callee),
                 SemNomeTexto(f->s, sum->nome));
        break;
    default: snprintf(saida, cap, "memoria apontada por ponteiro de origem desconhecida"); break;
    }
}

/* ---------- cadeia ---------- */

static void AcrescentarElemento(Cadeia *c, const char *nome, uint32_t pos)
{
    c->el[c->n++] = (ElementoCadeia){nome, pos};
}

/* origem (o local) -> variaveis que carregaram o endereco, da mais antiga a mais nova -> sumidouro. */
static void MontarCadeia(const Fuga *f, const Expr *valor, uint32_t bit, const char *rotulo_sum, uint32_t pos_sum, Cadeia *c)
{
    int32_t pilha[CADEIA_PORTADORES_MAX];
    uint32_t n = 0;
    for (int32_t v = FugaPortador(f, valor, bit, -1); v >= 0 && n < CADEIA_PORTADORES_MAX; v = ProvDe(f, v, bit)) pilha[n++] = v;
    c->n = 0;
    AcrescentarElemento(c, SemNomeTexto(f->s, f->locais[bit].nome), f->locais[bit].pos);
    while (n > 0) {
        const FugaLocal *l = &f->locais[pilha[--n]];
        AcrescentarElemento(c, SemNomeTexto(f->s, l->nome), l->pos);
    }
    AcrescentarElemento(c, rotulo_sum, pos_sum);
}

/* Lista JSON ["x (local, 2:9)", "p (3:9)", "return (4:5)"]; elemento que nao cabe no limite fica de fora (o JSON sai sempre fechado). */
static void CadeiaEmJson(const Fuga *f, const Cadeia *c, char *json, size_t cap)
{
    size_t usado = 0;
    json[usado++] = '[';
    for (uint32_t i = 0; i < c->n; i++) {
        char item[128];
        FontePosicao p = FonteLocalizarNoArquivo(f->s->fonte, c->el[i].pos);
        int n = snprintf(item, sizeof item, "%s\"%s (%s%u:%u)\"", i == 0 ? "" : ", ", c->el[i].nome, i == 0 ? "local, " : "", p.linha, p.coluna);
        if (n < 0 || usado + (size_t)n + 2 > cap) break;
        memcpy(json + usado, item, (size_t)n);
        usado += (size_t)n;
    }
    json[usado++] = ']';
    json[usado] = '\0';
}

static void CadeiaEmTexto(const Cadeia *c, char *texto, size_t cap)
{
    size_t usado = 0;
    texto[0] = '\0';
    for (uint32_t i = 0; i < c->n && usado < cap; i++) {
        usado += (size_t)snprintf(texto + usado, cap - usado, "%s%s", i == 0 ? "" : " -> ", c->el[i].nome);
    }
}

/* 'return &x...' e 'return x[..]' diretos (sem passar por variavel) mantem as causas do E0706 sintatico antigo. */
static int CausaDireta(const Expr *valor, const FugaSumidouro *sum, const Cadeia *c)
{
    if (sum->tipo != SUM_RETURN || c->n != 2) return CAUSA_GERAL;
    if (valor->tipo_no == EX_UNARIO && valor->op == OP_ENDERECO) return CAUSA_ENDERECO;
    return valor->tipo_no == EX_FATIA ? CAUSA_FATIA : CAUSA_GERAL;
}

/* Ultimo elo da cadeia: o rotulo do sumidouro; na gravacao por callee, a propria chamada. */
static void FimDaCadeia(const Fuga *f, const FugaSumidouro *sum, const char *rotulo, char *fim, size_t cap)
{
    if (sum->tipo == SUM_CALLEE) snprintf(fim, cap, "chamada a %s", SemNomeTexto(f->s, sum->callee));
    else snprintf(fim, cap, "%s", rotulo);
}

static void RelatarFuga(const Fuga *f, const Expr *valor, const FugaSumidouro *sum, const Cadeia *c, const char *rotulo, const char *texto)
{
    int direta = CausaDireta(valor, sum, c);
    const char *nome = c->el[0].nome;
    const char *proc = SemNomeTexto(f->s, f->proc->nome);
    if (direta == CAUSA_ENDERECO) {
        SemErro(f->s, E0706, valor->pos, "'return &%s' devolve o endereco de uma variavel que deixa de existir quando o proc retorna", nome);
    } else if (direta == CAUSA_FATIA) {
        SemErro(f->s, E0706, valor->pos, "'return %s[...]' devolve uma fatia de um array que deixa de existir quando o proc retorna", nome);
    } else if (sum->tipo == SUM_CALLEE) {
        SemErro(f->s, E0706, valor->pos, "'%s' e local de '%s' e %s: %s", nome, proc, rotulo, texto);
    } else {
        SemErro(f->s, E0706, valor->pos, "'%s' e local de '%s' e o endereco sai por %s: %s", nome, proc, rotulo, texto);
    }
}

bool FugaReportar(Fuga *f, const Expr *valor, uint32_t bit, const FugaSumidouro *sum)
{
    char rotulo[160], fim[160], texto[256], json[CADEIA_JSON_MAX];
    Cadeia c;
    RotuloDoSumidouro(f, sum, rotulo, sizeof rotulo);
    FimDaCadeia(f, sum, rotulo, fim, sizeof fim);
    MontarCadeia(f, valor, bit, fim, sum->pos, &c);
    CadeiaEmTexto(&c, texto, sizeof texto);
    CadeiaEmJson(f, &c, json, sizeof json);
    RelatarFuga(f, valor, sum, &c, rotulo, texto);
    DiagAcrescentarExtra(f->s->diag, "cadeia", json);
    return false;
}
