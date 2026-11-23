/* sroa.c - passe `sroa` (M7b F3), 1a metade: decide quais slots de pilha viram vregs por campo (a troca das instrucoes esta em sroa_reescrever.c; ver sroa.h).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - S4b: OtimSroaVeredito (so a decisao, para o slp); EscalarizarFuncao/SroaFuncao partidas em DecidirFuncao + reescrita. Comportamento do passe igual.
 *            0.1.1 - S1b (D0703): CARREGA/GRAVA de BC_V128 recusa o slot (classe); vetor e opaco.
 *            0.1.0 - M7b F3: primeira versao (raizes de endereco, escape, particao em campos, grupos por copia_mem, orcamentos, remarks).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe sroa
 *
 * Raiz: um vreg e endereco de slot se tem UMA definicao e ela e END_SLOT k, COPIA de raiz ou ADD de raiz com imediato (vira deslocamento). O slot escapa se o endereco aparece fora da base de
 * CARREGA/GRAVA/ZERA_MEM/COPIA_MEM (valor gravado, argumento, retorno, CMP, ADD/SUB com indice dinamico...). Todo acesso e uma faixa [off, off+tam) dentro do slot e todos os acessos
 * a uma faixa tem o mesmo offset, tamanho e classe; sobreposicao parcial ou classe diferente (transmute pela memoria) recusa. Slots ligados por COPIA_MEM formam um grupo (uniao-busca com
 * deslocamento: coordenada canonica = byte + deslocamento do slot) que compartilha a particao; os bytes de uma copia que nenhum acesso cobre viram pedacos inteiros naturais (padding e
 * indefinido na BIR). Cada campo ganha 1 vreg novo (nao SSA) na classe do acesso.
 * Traps e R4: o slot nunca da trap e nenhuma instrucao troca de ordem, entao R4 e a pilha do rastreio (R0001..R0009) nao mudam. A INVARIANTE de checagem.c (copia/conta do contador do laco
 * fora do corpo) nao e acionada: este passe roda depois da prova e nao tira nada do corpo do laco; o CONST 0 de campo vivo na entrada vai na entrada da funcao, nunca dentro de laco
 * (funcao cujo bloco 0 tem predecessor nao e tocada).
 * Alias (E2): slot sem endereco escapado nao sofre alias; o endereco de um slot escalarizado nunca e visto por CALL nem por outro acesso.
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/analises.h"
#include "nativo/optimizer/sroa.h"
#include "nativo/optimizer/sroa_interno.h"

static const char *const NOME_MOTIVO[] = {"", "escapa", "endereco_multiplo", "indice_dinamico", "classe", "sobreposicao", "tamanho", "campos", "grupo", "orcamento"};

#define RAIZ_OFF_MAX (1 << 24) /* deslocamento acumulado de uma raiz: acima disto o ADD nao vira raiz */

typedef struct { uint32_t slot, tam; int32_t off; uint8_t classe; } Acesso;
/* slot_x = SROA_SEM: o lado x e memoria externa (ZERA_MEM so tem o lado a). */
typedef struct { uint32_t slot_a, slot_b; int32_t off_a, off_b; uint32_t n; } Copia;
typedef struct { void *v; uint32_t n, cap; } Vetor;
typedef struct { uint32_t *ini, *ord; } Indice;
/* Uniao-busca com deslocamento: desl = deslocamento do slot para as coordenadas do pai; sh = o dele para as da raiz (preenchido antes de decidir o grupo). */
typedef struct { uint32_t pai, tam, prox, cabeca; int32_t desl, sh; bool conflito, feito; } NoGrupo;

typedef struct {
    BirFuncao *f;
    bool simbolos;
    uint32_t nvregs, nslots;
    uint32_t *ndefs;
    SroaRaiz *raiz;
    SroaSlot *slot;
    Vetor acessos, copias;
    Indice por_slot, por_dono; /* acessos por slot; copias pelo slot vivo do lado a (ou do b) */
    NoGrupo *no;
} Sroa;

static void *Reservar(size_t n, size_t tam)
{
    void *p = calloc(n + 1u, tam);

    if (p == NULL)
        FalhaInterna("sroa: memoria esgotada");
    return p;
}

static void *Acrescentar(Vetor *v, size_t tam)
{
    if (v->n == v->cap) {
        void *novo;

        v->cap = v->cap == 0 ? 16u : v->cap * 2u;
        novo = realloc(v->v, (size_t)v->cap * tam);
        if (novo == NULL)
            FalhaInterna("sroa: memoria esgotada");
        v->v = novo;
    }
    return (char *)v->v + (size_t)v->n++ * tam;
}

/* ---- fatos ---- */

static bool DefineVreg(const BirInstr *i)
{
    if (i->dst == BIR_SEM_VREG)
        return false;
    return i->op != BIR_SALTA && i->op != BIR_DESVIA && i->op != BIR_RETORNA && i->op != BIR_ARMADILHA && i->op != BIR_GRAVA && i->op != BIR_COPIA_MEM &&
           i->op != BIR_ZERA_MEM;
}

static void ContarDefs(Sroa *s)
{
    const BirFuncao *f = s->f;

    for (uint32_t k = 0; k < f->nparams && k < s->nvregs; k++)
        s->ndefs[k]++;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t j = 0; j < f->blocos[b].n; j++) {
            const BirInstr *i = &f->blocos[b].instrs[j];

            if (DefineVreg(i) && i->dst < s->nvregs)
                s->ndefs[i->dst]++;
        }
    }
}

static const SroaRaiz *RaizDe(const Sroa *s, const BirOperando *o)
{
    if (o->tipo != BO_VREG || o->id >= s->nvregs || s->raiz[o->id].slot == SROA_SEM)
        return NULL;
    return &s->raiz[o->id];
}

/* A raiz que a instrucao (que define dst) da a dst, se for END_SLOT, COPIA de raiz ou ADD de raiz com imediato. */
static bool DerivaRaiz(const Sroa *s, const BirInstr *i, SroaRaiz *r)
{
    const SroaRaiz *base;
    const BirOperando *imed = &i->b;
    int64_t total;

    if (i->op == BIR_END_SLOT) {
        if (i->a.tipo != BO_IMED || i->a.imed < 0 || (uint64_t)i->a.imed >= s->nslots)
            return false;
        *r = (SroaRaiz){.slot = (uint32_t)i->a.imed};
        return true;
    }
    if (i->op != BIR_COPIA && i->op != BIR_ADD)
        return false;
    base = RaizDe(s, &i->a);
    if (base == NULL && i->op == BIR_ADD) {
        base = RaizDe(s, &i->b);
        imed = &i->a;
    }
    if (base == NULL || (i->op == BIR_ADD && imed->tipo != BO_IMED))
        return false;
    total = (int64_t)base->off + (i->op == BIR_ADD ? imed->imed : 0);
    if (total < -RAIZ_OFF_MAX || total > RAIZ_OFF_MAX)
        return false;
    *r = (SroaRaiz){.slot = base->slot, .off = (int32_t)total};
    return true;
}

/* Uma volta no bloco: true se algum vreg ganhou raiz. So vreg com uma definicao vira raiz. */
static bool RaizesDoBloco(Sroa *s, const BirBloco *b)
{
    bool mudou = false;

    for (uint32_t j = 0; j < b->n; j++) {
        const BirInstr *i = &b->instrs[j];
        SroaRaiz r;

        if (!DefineVreg(i) || i->dst >= s->nvregs || s->raiz[i->dst].slot != SROA_SEM || s->ndefs[i->dst] != 1 || !DerivaRaiz(s, i, &r))
            continue;
        s->raiz[i->dst] = r;
        mudou = true;
    }
    return mudou;
}

/* Ponto fixo: a definicao de uma raiz pode vir depois do uso na ordem dos blocos. */
static void CalcularRaizes(Sroa *s)
{
    bool mudou;

    do {
        mudou = false;
        for (uint32_t b = 0; b < s->f->nblocos; b++)
            mudou |= RaizesDoBloco(s, &s->f->blocos[b]);
    } while (mudou);
}

/* ---- escape e acessos ---- */

static void Recusar(Sroa *s, uint32_t slot, SroaMotivo m)
{
    if (s->slot[slot].motivo == SM_OK)
        s->slot[slot].motivo = (uint8_t)m;
}

static void Escapar(Sroa *s, const BirOperando *o)
{
    const SroaRaiz *r = RaizDe(s, o);

    if (r != NULL)
        Recusar(s, r->slot, SM_ESCAPA);
}

static uint32_t BytesDaClasse(uint8_t classe)
{
    switch (classe) {
    case BC_I8: return 1;
    case BC_I16: return 2;
    case BC_I32: case BC_F32: return 4;
    default: return 8;
    }
}

/* [desloc, desloc+n) a partir da raiz cabe no slot? Senao recusa por tamanho. */
static bool DentroDoSlot(Sroa *s, const SroaRaiz *r, int64_t desloc, uint32_t n)
{
    int64_t off = (int64_t)r->off + desloc;

    if (off >= 0 && off + (int64_t)n <= (int64_t)s->f->slots[r->slot].tam)
        return true;
    Recusar(s, r->slot, SM_TAMANHO);
    return false;
}

/* CARREGA e GRAVA: um acesso de largura da classe na base. */
static void Acessar(Sroa *s, const BirInstr *i)
{
    const SroaRaiz *r = RaizDe(s, &i->a);
    uint32_t n = BytesDaClasse(i->classe);
    Acesso *a;

    if (r != NULL && i->classe == BC_V128) { /* vetor e opaco: o slot nao se desfaz em escalares */
        Recusar(s, r->slot, SM_CLASSE);
        return;
    }
    if (r == NULL || s->slot[r->slot].motivo != SM_OK || !DentroDoSlot(s, r, i->desloc, n))
        return;
    a = Acrescentar(&s->acessos, sizeof *a);
    *a = (Acesso){.slot = r->slot, .off = r->off + i->desloc, .tam = n, .classe = i->classe};
}

static void GuardarCopia(Sroa *s, const SroaRaiz *ra, const SroaRaiz *rb, uint32_t n)
{
    Copia *c = Acrescentar(&s->copias, sizeof *c);

    *c = (Copia){.slot_a = ra != NULL ? ra->slot : SROA_SEM, .slot_b = rb != NULL ? rb->slot : SROA_SEM, .off_a = ra != NULL ? ra->off : 0,
                 .off_b = rb != NULL ? rb->off : 0, .n = n};
}

/* COPIA_MEM e ZERA_MEM: uma faixa que o particionamento precisa cobrir. Copia dentro do mesmo slot recusa. */
static void RegistrarCopia(Sroa *s, const BirInstr *i)
{
    const SroaRaiz *ra = RaizDe(s, &i->a), *rb = i->op == BIR_COPIA_MEM ? RaizDe(s, &i->b) : NULL;
    uint32_t n = (uint32_t)i->desloc;
    bool ok_a = ra == NULL || DentroDoSlot(s, ra, 0, n);
    bool ok_b = rb == NULL || DentroDoSlot(s, rb, 0, n);

    if (ra == NULL && rb == NULL)
        return;
    if (ra != NULL && rb != NULL && ra->slot == rb->slot)
        Recusar(s, ra->slot, SM_SOBREPOSICAO);
    else if (ok_a && ok_b)
        GuardarCopia(s, ra, rb, n);
}

/* COPIA ou ADD cujo operando e raiz: derivacao reconhecida (dst ja e raiz) ou escape/endereco multiplo/indice dinamico. */
static void ClassificarDerivacao(Sroa *s, const BirInstr *i)
{
    const SroaRaiz *ra = RaizDe(s, &i->a), *rb = i->op == BIR_ADD ? RaizDe(s, &i->b) : NULL;
    const SroaRaiz *r = ra != NULL ? ra : rb;
    const BirOperando *outro = ra != NULL ? &i->b : &i->a;
    bool multiplo = i->dst < s->nvregs && s->ndefs[i->dst] != 1;

    if (r == NULL)
        return;
    if (ra != NULL && rb != NULL) {
        Recusar(s, ra->slot, SM_ESCAPA);
        Recusar(s, rb->slot, SM_ESCAPA);
    } else if (i->op == BIR_ADD && outro->tipo == BO_VREG)
        Recusar(s, r->slot, SM_INDICE_DINAMICO);
    else if (i->op == BIR_ADD && outro->tipo != BO_IMED)
        Recusar(s, r->slot, SM_ESCAPA);
    else if (i->dst >= s->nvregs || s->raiz[i->dst].slot == SROA_SEM)
        Recusar(s, r->slot, multiplo ? SM_ENDERECO_MULTIPLO : SM_ESCAPA);
}

static void ClassificarInstr(Sroa *s, const BirInstr *i)
{
    switch (i->op) {
    case BIR_CARREGA:
        Acessar(s, i);
        return;
    case BIR_GRAVA:
        Acessar(s, i);
        Escapar(s, &i->b);
        return;
    case BIR_ZERA_MEM: case BIR_COPIA_MEM:
        RegistrarCopia(s, i);
        return;
    case BIR_END_SLOT:
        if (i->dst >= s->nvregs || s->ndefs[i->dst] != 1)
            Recusar(s, (uint32_t)i->a.imed, SM_ENDERECO_MULTIPLO);
        return;
    case BIR_COPIA: case BIR_ADD:
        ClassificarDerivacao(s, i);
        return;
    default:
        Escapar(s, &i->a);
        Escapar(s, &i->b);
        for (uint32_t m = 0; m < i->nargs; m++)
            Escapar(s, &i->args[m]);
    }
}

static void Classificar(Sroa *s)
{
    for (uint32_t b = 0; b < s->f->nblocos; b++) {
        for (uint32_t j = 0; j < s->f->blocos[b].n; j++)
            ClassificarInstr(s, &s->f->blocos[b].instrs[j]);
    }
}

/* ---- grupos de slots ligados por copia_mem ---- */

static uint32_t Achar(const Sroa *s, uint32_t k, int32_t *desl)
{
    int32_t d = 0;

    while (s->no[k].pai != k) {
        d += s->no[k].desl;
        k = s->no[k].pai;
    }
    *desl = d;
    return k;
}

static bool Vivo(const Sroa *s, uint32_t slot)
{
    return slot != SROA_SEM && s->slot[slot].motivo == SM_OK;
}

/* Une os dois lados da copia: o byte x do slot a e o byte x + off_b - off_a do slot b tem a mesma coordenada canonica. Ciclo inconsistente marca conflito (recusa o grupo). */
static void Unir(Sroa *s, const Copia *c)
{
    int32_t da, db, t;
    uint32_t ra = Achar(s, c->slot_a, &da), rb = Achar(s, c->slot_b, &db);

    t = da - db + c->off_a - c->off_b; /* deslocamento das coordenadas de rb para as de ra */
    if (ra == rb) {
        if (t != 0)
            s->no[ra].conflito = true;
        return;
    }
    if (s->no[ra].tam < s->no[rb].tam) {
        uint32_t menor = ra;

        ra = rb;
        rb = menor;
        t = -t;
    }
    s->no[rb].pai = ra;
    s->no[rb].desl = t;
    s->no[ra].tam += s->no[rb].tam;
    s->no[ra].conflito = s->no[ra].conflito || s->no[rb].conflito;
}

static void UnirCopias(Sroa *s)
{
    for (uint32_t k = 0; k < s->nslots; k++)
        s->no[k] = (NoGrupo){.pai = k, .tam = 1, .prox = SROA_SEM, .cabeca = SROA_SEM};
    for (uint32_t k = 0; k < s->copias.n; k++) {
        const Copia *c = (const Copia *)s->copias.v + k;

        if (Vivo(s, c->slot_a) && Vivo(s, c->slot_b))
            Unir(s, c);
    }
    for (uint32_t k = s->nslots; k-- > 0;) {
        int32_t sh;
        uint32_t r;

        if (s->slot[k].motivo != SM_OK)
            continue;
        r = Achar(s, k, &sh);
        s->no[k].sh = sh;
        s->no[k].prox = s->no[r].cabeca;
        s->no[r].cabeca = k;
    }
}

/* Ordenacao por contagem: ini[k]..ini[k+1] sao os itens de chave k em ord; chave == nslots descarta o item. */
static void Agrupar(Indice *x, const uint32_t *chave, uint32_t n, uint32_t nslots)
{
    uint32_t *cur = Reservar(nslots + 1u, sizeof *cur);

    x->ini = Reservar(nslots + 2u, sizeof *x->ini);
    x->ord = Reservar(n, sizeof *x->ord);
    for (uint32_t k = 0; k < n; k++)
        x->ini[chave[k] + 1u]++;
    for (uint32_t k = 0; k <= nslots; k++)
        x->ini[k + 1u] += x->ini[k];
    memcpy(cur, x->ini, (nslots + 1u) * sizeof *cur);
    for (uint32_t k = 0; k < n; k++)
        x->ord[cur[chave[k]]++] = k;
    free(cur);
}

static void IndexarAcessosECopias(Sroa *s)
{
    uint32_t *chave = Reservar(s->acessos.n > s->copias.n ? s->acessos.n : s->copias.n, sizeof *chave);

    for (uint32_t k = 0; k < s->acessos.n; k++)
        chave[k] = ((const Acesso *)s->acessos.v)[k].slot;
    Agrupar(&s->por_slot, chave, s->acessos.n, s->nslots);
    for (uint32_t k = 0; k < s->copias.n; k++) {
        const Copia *c = (const Copia *)s->copias.v + k;

        chave[k] = Vivo(s, c->slot_a) ? c->slot_a : Vivo(s, c->slot_b) ? c->slot_b : s->nslots;
    }
    Agrupar(&s->por_dono, chave, s->copias.n, s->nslots);
    free(chave);
}

/* ---- particao do grupo ---- */

typedef struct { int32_t c; uint32_t tam; uint8_t classe; } Intervalo; /* coordenada canonica */
typedef struct { Intervalo v[SROA_CAMPOS_GRUPO]; uint32_t n; } Conjunto;

static uint32_t PrimeiroNaoMenor(const Conjunto *k, int32_t c)
{
    uint32_t i = 0;

    while (i < k->n && k->v[i].c < c)
        i++;
    return i;
}

static SroaMotivo InserirIntervalo(Conjunto *k, Intervalo novo)
{
    uint32_t i = PrimeiroNaoMenor(k, novo.c);

    if (i > 0 && k->v[i - 1].c + (int32_t)k->v[i - 1].tam > novo.c)
        return SM_SOBREPOSICAO;
    if (i < k->n && k->v[i].c == novo.c) {
        if (k->v[i].tam != novo.tam)
            return SM_SOBREPOSICAO;
        return k->v[i].classe == novo.classe ? SM_OK : SM_CLASSE;
    }
    if (i < k->n && k->v[i].c < novo.c + (int32_t)novo.tam)
        return SM_SOBREPOSICAO;
    if (k->n == SROA_CAMPOS_GRUPO)
        return SM_CAMPOS;
    memmove(&k->v[i + 1], &k->v[i], (k->n - i) * sizeof k->v[0]);
    k->v[i] = novo;
    k->n++;
    return SM_OK;
}

/* Maior pedaco natural (8, 4, 2, 1) alinhado em pos que cabe em resto bytes. */
static uint32_t TamanhoNatural(int32_t pos, uint32_t resto)
{
    static const uint8_t TAMANHOS[] = {8, 4, 2, 1};

    for (uint32_t k = 0; k < 3; k++) {
        int32_t t = TAMANHOS[k];

        if (resto >= (uint32_t)t && ((pos % t) + t) % t == 0)
            return (uint32_t)t;
    }
    return 1;
}

static uint8_t ClasseNatural(uint32_t tam)
{
    return tam == 8 ? BC_I64 : tam == 4 ? BC_I32 : tam == 2 ? BC_I16 : BC_I8;
}

static SroaMotivo PreencherBuraco(Conjunto *k, int32_t *pos, int32_t limite)
{
    while (*pos < limite) {
        uint32_t t = TamanhoNatural(*pos, (uint32_t)(limite - *pos));
        SroaMotivo m = InserirIntervalo(k, (Intervalo){.c = *pos, .tam = t, .classe = ClasseNatural(t)});

        if (m != SM_OK)
            return m;
        *pos += (int32_t)t;
    }
    return SM_OK;
}

/* A faixa [lo, lo+n) tem de ser coberta exatamente por intervalos: os que ja existem e pedacos naturais nos buracos. Intervalo que cruza a borda recusa. */
static SroaMotivo PreencherFaixa(Conjunto *k, int32_t lo, uint32_t n)
{
    int32_t pos = lo, fim = lo + (int32_t)n;

    while (pos < fim) {
        uint32_t i = PrimeiroNaoMenor(k, pos);
        SroaMotivo m;

        if (i > 0 && k->v[i - 1].c + (int32_t)k->v[i - 1].tam > pos)
            return SM_SOBREPOSICAO;
        if (i < k->n && k->v[i].c == pos) {
            pos += (int32_t)k->v[i].tam;
            if (pos > fim)
                return SM_SOBREPOSICAO;
            continue;
        }
        m = PreencherBuraco(k, &pos, i < k->n && k->v[i].c < fim ? k->v[i].c : fim);
        if (m != SM_OK)
            return m;
    }
    return SM_OK;
}

static SroaMotivo InserirAcessosDoSlot(const Sroa *s, Conjunto *k, uint32_t slot)
{
    for (uint32_t x = s->por_slot.ini[slot]; x < s->por_slot.ini[slot + 1u]; x++) {
        const Acesso *a = (const Acesso *)s->acessos.v + s->por_slot.ord[x];
        SroaMotivo m = InserirIntervalo(k, (Intervalo){.c = a->off + s->no[slot].sh, .tam = a->tam, .classe = a->classe});

        if (m != SM_OK)
            return m;
    }
    return SM_OK;
}

static SroaMotivo PreencherCopiasDoSlot(const Sroa *s, Conjunto *k, uint32_t slot)
{
    for (uint32_t x = s->por_dono.ini[slot]; x < s->por_dono.ini[slot + 1u]; x++) {
        const Copia *c = (const Copia *)s->copias.v + s->por_dono.ord[x];
        SroaMotivo m = PreencherFaixa(k, (c->slot_a == slot ? c->off_a : c->off_b) + s->no[slot].sh, c->n);

        if (m != SM_OK)
            return m;
    }
    return SM_OK;
}

/* Os intervalos do grupo que cabem inteiros no slot viram os campos dele (offset em bytes do slot). */
static SroaMotivo MontarCampos(Sroa *s, const Conjunto *k, uint32_t slot)
{
    SroaSlot *si = &s->slot[slot];
    int32_t ini = s->no[slot].sh, fim = ini + (int32_t)s->f->slots[slot].tam;

    si->nf = 0;
    for (uint32_t x = 0; x < k->n; x++) {
        const Intervalo *v = &k->v[x];

        if (v->c < ini || v->c + (int32_t)v->tam > fim)
            continue;
        if (si->nf == ORC_SROA_CAMPOS_SLOT)
            return SM_CAMPOS;
        si->campo[si->nf++] = (SroaCampo){.off = v->c - ini, .tam = v->tam, .classe = v->classe};
    }
    return SM_OK;
}

/* Decide o grupo de cabeca `cabeca` (raiz `raiz`): particao comum, depois os campos de cada membro. */
static SroaMotivo DecidirGrupo(Sroa *s, uint32_t raiz, uint32_t cabeca)
{
    Conjunto k = {.n = 0};
    SroaMotivo m = s->no[raiz].conflito ? SM_GRUPO : SM_OK;

    for (uint32_t x = cabeca; x != SROA_SEM && m == SM_OK; x = s->no[x].prox)
        m = InserirAcessosDoSlot(s, &k, x);
    for (uint32_t x = cabeca; x != SROA_SEM && m == SM_OK; x = s->no[x].prox)
        m = PreencherCopiasDoSlot(s, &k, x);
    for (uint32_t x = cabeca; x != SROA_SEM && m == SM_OK; x = s->no[x].prox)
        m = MontarCampos(s, &k, x);
    return m;
}

/* Numera os vregs dos campos do grupo a partir de *usado (nvregs + *usado); false se estoura o orcamento da funcao. */
static bool NumerarGrupo(Sroa *s, uint32_t cabeca, uint32_t *usado)
{
    uint32_t total = 0;

    for (uint32_t x = cabeca; x != SROA_SEM; x = s->no[x].prox)
        total += s->slot[x].nf;
    if (*usado + total > ORC_SROA_VREGS_FUNCAO)
        return false;
    for (uint32_t x = cabeca; x != SROA_SEM; x = s->no[x].prox) {
        for (uint32_t m = 0; m < s->slot[x].nf; m++)
            s->slot[x].campo[m].vreg = s->nvregs + (*usado)++;
    }
    return true;
}

static void RecusarGrupo(Sroa *s, uint32_t cabeca, SroaMotivo m)
{
    for (uint32_t x = cabeca; x != SROA_SEM; x = s->no[x].prox) {
        s->slot[x].motivo = (uint8_t)m;
        s->slot[x].nf = 0;
    }
}

/* Decide todos os grupos na ordem do menor slot de cada um. Devolve os vregs novos. */
static uint32_t DecidirGrupos(Sroa *s)
{
    uint32_t usado = 0;

    for (uint32_t k = 0; k < s->nslots; k++) {
        int32_t sh;
        uint32_t r;
        SroaMotivo m;

        if (s->slot[k].motivo != SM_OK)
            continue;
        r = Achar(s, k, &sh);
        if (s->no[r].feito)
            continue;
        s->no[r].feito = true;
        m = DecidirGrupo(s, r, s->no[r].cabeca);
        if (m == SM_OK && !NumerarGrupo(s, s->no[r].cabeca, &usado))
            m = SM_ORCAMENTO;
        if (m != SM_OK)
            RecusarGrupo(s, s->no[r].cabeca, m);
    }
    return usado;
}

/* ---- entrada da funcao ---- */

/* Aresta para o bloco 0? Entao o CONST 0 de entrada rodaria a cada volta: a funcao nao e tocada. */
static bool Bloco0TemPredecessor(const BirFuncao *f)
{
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t j = 0; j < f->blocos[b].n; j++) {
            const BirInstr *i = &f->blocos[b].instrs[j];
            bool desvio = i->op == BIR_DESVIA && i->alvo[1] == 0;
            bool alvo0 = (i->op == BIR_SALTA || i->op == BIR_DESVIA || (i->op >= BIR_ADD_OV && i->op <= BIR_NEG_OV)) && i->alvo[0] == 0;

            if (desvio || alvo0)
                return true;
        }
    }
    return false;
}

/* Campo vivo na entrada do bloco 0 e lido antes de escrito: o slot dele tinha lixo; o vreg ganha CONST 0 no inicio do bloco 0 (senao o intervalo do alocador estica ate la). */
static void ZerarCamposVivosNaEntrada(BirFuncao *f, const SroaSlot *info)
{
    AnalisesBir an;
    uint32_t vreg[ORC_SROA_VREGS_FUNCAO], n = 0;
    uint8_t classe[ORC_SROA_VREGS_FUNCAO];
    BirBloco *b0 = &f->blocos[0];
    BirInstr modelo = b0->instrs[0];

    AnalisesCalcular(f, &an);
    for (uint32_t k = 0; k < f->nslots; k++) {
        for (uint32_t m = 0; info[k].motivo == SM_OK && m < info[k].nf; m++) {
            if (!AnalisesVivoEntra(&an, 0, info[k].campo[m].vreg))
                continue;
            vreg[n] = info[k].campo[m].vreg;
            classe[n++] = info[k].campo[m].classe;
        }
    }
    AnalisesLiberar(&an);
    if (n == 0)
        return;
    b0->instrs = realloc(b0->instrs, ((size_t)b0->n + n) * sizeof *b0->instrs);
    if (b0->instrs == NULL)
        FalhaInterna("sroa: memoria esgotada");
    memmove(b0->instrs + n, b0->instrs, (size_t)b0->n * sizeof *b0->instrs);
    for (uint32_t k = 0; k < n; k++) {
        b0->instrs[k] = (BirInstr){.op = BIR_CONST, .classe = classe[k], .dst = vreg[k], .a = {.tipo = BO_IMED, .classe = classe[k]}, .simbolo = BIR_SEM_SIMBOLO,
                                   .alvo = {BIR_SEM_BLOCO, BIR_SEM_BLOCO}, .pos = modelo.pos, .ir = modelo.ir};
    }
    b0->n += n;
    b0->cap = b0->n;
}

/* ---- funcao ---- */

/* Slots que nem entram na conta: de depuracao (--simbolos) e maiores que o limite. Devolve quantos ficam candidatos. */
static uint32_t FiltrarSlots(Sroa *s, const BirFuncao *f)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < s->nslots; k++) {
        s->slot[k].motivo = f->slots[k].tam > ORC_SROA_SLOT_BYTES ? SM_TAMANHO : SM_OK;
        n += s->slot[k].motivo == SM_OK;
    }
    for (uint32_t k = 0; s->simbolos && k < f->ncasas; k++) {
        if (f->casas[k].slot >= 0 && (uint32_t)f->casas[k].slot < s->nslots) {
            n -= s->slot[f->casas[k].slot].motivo == SM_OK;
            s->slot[f->casas[k].slot].motivo = SM_PULADO;
        }
    }
    return n;
}

static void Relatar(const BirPrograma *p, const BirFuncao *f, const SroaSlot *info, Buf *relato)
{
    const char *nome = p->simbolos[f->simbolo].nome;

    for (uint32_t k = 0; k < f->nslots; k++) {
        if (info[k].motivo == SM_PULADO)
            continue;
        if (info[k].motivo == SM_OK)
            BufFormatar(relato, "sroa %s k%u: %u B, %u campos\n", nome, (unsigned)k, (unsigned)f->slots[k].tam, (unsigned)info[k].nf);
        else
            BufFormatar(relato, "sroa recusa %s k%u: %s\n", nome, (unsigned)k, NOME_MOTIVO[info[k].motivo]);
    }
}

static void LiberarTudo(Sroa *s)
{
    free(s->ndefs);
    free(s->raiz);
    free(s->acessos.v);
    free(s->copias.v);
    free(s->por_slot.ini);
    free(s->por_slot.ord);
    free(s->por_dono.ini);
    free(s->por_dono.ord);
    free(s->no);
}

/* Filtro, fatos e decisao de uma funcao (s->slot ja alocado); nao toca na BIR. true se algum slot fica SM_OK (escalariza); *usado = vregs novos que a reescrita pede. */
static bool DecidirFuncao(Sroa *s, uint32_t *usado)
{
    uint32_t candidatos = FiltrarSlots(s, s->f);
    bool algum = false;

    *usado = 0;
    if (candidatos > 0 && Bloco0TemPredecessor(s->f)) {
        for (uint32_t k = 0; k < s->nslots; k++)
            s->slot[k].motivo = s->slot[k].motivo == SM_OK ? SM_ORCAMENTO : s->slot[k].motivo;
        candidatos = 0;
    }
    if (candidatos == 0)
        return false;
    s->ndefs = Reservar(s->nvregs, sizeof *s->ndefs);
    s->raiz = Reservar(s->nvregs, sizeof *s->raiz);
    s->no = Reservar(s->nslots, sizeof *s->no);
    for (uint32_t k = 0; k < s->nvregs; k++)
        s->raiz[k].slot = SROA_SEM;
    ContarDefs(s);
    CalcularRaizes(s);
    Classificar(s);
    UnirCopias(s);
    IndexarAcessosECopias(s);
    *usado = DecidirGrupos(s);
    for (uint32_t k = 0; k < s->nslots && !algum; k++)
        algum = s->slot[k].motivo == SM_OK;
    return algum;
}

static bool FuncaoElegivel(const BirFuncao *f)
{
    return f->nslots != 0 && f->nblocos != 0 && !f->sintetica;
}

static void SroaFuncao(const BirPrograma *p, BirFuncao *f, Buf *relato)
{
    Sroa s = {.f = f, .simbolos = p->simbolos_dbg, .nvregs = f->nvregs, .nslots = f->nslots};
    uint32_t usado;

    if (!FuncaoElegivel(f))
        return;
    s.slot = Reservar(f->nslots, sizeof *s.slot);
    if (DecidirFuncao(&s, &usado)) {
        SroaReescrever(f, &(SroaPlano){.info = s.slot, .raiz = s.raiz, .nvregs_antigo = s.nvregs, .nvregs_novo = s.nvregs + usado});
        ZerarCamposVivosNaEntrada(f, s.slot);
    }
    if (relato != NULL)
        Relatar(p, f, s.slot, relato);
    LiberarTudo(&s);
    free(s.slot);
}

void OtimSroaVeredito(const BirPrograma *p, BirFuncao *f, bool *escalariza)
{
    Sroa s = {.f = f, .simbolos = p->simbolos_dbg, .nvregs = f->nvregs, .nslots = f->nslots};
    uint32_t usado;

    if (!FuncaoElegivel(f))
        return;
    s.slot = Reservar(f->nslots, sizeof *s.slot);
    DecidirFuncao(&s, &usado);
    for (uint32_t k = 0; k < f->nslots; k++)
        escalariza[k] = s.slot[k].motivo == SM_OK;
    LiberarTudo(&s);
    free(s.slot);
}

void OtimSroaAplicar(BirPrograma *programa, Buf *relato)
{
    for (uint32_t i = 0; i < programa->nfuncoes; i++)
        SroaFuncao(programa, &programa->funcoes[i], relato);
}
