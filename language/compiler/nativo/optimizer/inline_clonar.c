/* inline_clonar.c - passe `inline` (M7b F2): a copia dos blocos da folha para dentro do chamador (decisao de quem entra: inline.c; ver inline.h).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M7b F2b: InlineGuardar (guarda de retorno antecipado): corta o bloco na chamada e monta cmp+desvia, folha (copia/constante + salta) e via lenta (a chamada + salta); o resto vai para bloco novo no fim do vetor.
 *            0.3.0 - M7b F2 (F2 fib_rec): a copia do corpo (CopiarCorpo/LiberarCorpo) passa a ser do chamador de InlineClonar, que a recebe; a 2a passada de recursao usa a mesma copia.
 *            0.2.0 - M7b R4a: sitio recursivo (folha == chamador) le o corpo de uma copia da funcao tirada antes da montagem (o chamador e refeito em cima dos blocos antigos); cmap com INLINE_BLOCOS_LACO_MAX.
 *            0.1.1- M7b F2 rodada 2: blocos com capacidade exata, bloco sem chamada trocada muda de lugar sem copiar, clone escrito direto no destino, bloco antigo liberado logo apos copiar.
 *            0.1.0 - M7b F2: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe inline
 *
 * Montagem: o chamador e refeito numa passada. Cada bloco antigo vira uma sequencia: [B_pre ... SALTA entrada-do-clone] [blocos nao frios do clone] [B_pos ...]; os blocos frios do
 * clone vao para depois de todos os blocos antigos. Todo alvo passa por tabela (mapa dos antigos = 1o segmento do bloco; cmap do clone). Vregs da folha = vbase + v (os parametros
 * viram vregs novos, copiados do argumento: a folha pode escrever neles); slots da folha = sbase + k. RETORNA vira COPIA/CONST/COPIA_MEM no destino da chamada + SALTA para B_pos.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/inline.h"

#define BLOCO_INSTRS_INICIAL 8u
#define LISTA_INICIAL 16u

typedef struct { BirBloco *v; uint32_t n, cap; } ListaBlocos;

/* Montagem de um chamador. */
typedef struct {
    BirPrograma *p;
    BirFuncao *f;
    ListaBlocos principal, extras;
    BirBloco cur;                    /* bloco em montagem (o ultimo segmento aberto) */
    uint32_t *mapa;                  /* bloco antigo -> id do 1o segmento no vetor novo */
    uint32_t nprincipal;             /* blocos fora dos frios do clone: os extras comecam neste id */
    uint32_t vregs, slots;           /* proximos vreg e slot livres do chamador */
    SitioInline *sitios;             /* os sitios, em ordem (bloco, instr) */
    uint32_t nsitios, proximo;       /* quantos; indice do proximo a emitir */
    uint32_t vregs_mais, slots_mais; /* total que os clones acrescentam */
    BirFuncao propria;               /* copia do corpo original (de quem chama): corpo dos sitios recursivos (so valida se tem_propria) */
    bool tem_propria;
} Montagem;

/* Um sitio em curso: a chamada, a folha e onde cada coisa da folha cai no chamador. */
typedef struct {
    const BirFuncao *g;
    const BirInstr *chama;
    uint32_t vbase, sbase, bpos;     /* bpos = id do B_pos */
    uint32_t cmap[INLINE_BLOCOS_LACO_MAX];
    uint32_t add;                    /* instrucoes que o chamador ganhou neste sitio */
} Sitio;

static void *Realocar(void *antigo, size_t bytes)
{
    void *novo = realloc(antigo, bytes);

    if (novo == NULL)
        FalhaInterna("inline: memoria esgotada (%zu bytes)", bytes);
    return novo;
}

/* O proximo lugar livre do bloco (cresce se faltar); quem chama preenche. */
static BirInstr *ReservarInstr(BirBloco *b)
{
    if (b->n == b->cap) {
        b->cap = b->cap == 0 ? BLOCO_INSTRS_INICIAL : b->cap * 2;
        b->instrs = Realocar(b->instrs, b->cap * sizeof *b->instrs);
    }
    return &b->instrs[b->n++];
}

static void Acrescentar(BirBloco *b, const BirInstr *i)
{
    *ReservarInstr(b) = *i;
}

/* Bloco vazio com espaco para cap instrucoes (Acrescentar ainda cresce se faltar). */
static BirBloco BlocoComCapacidade(bool frio, uint32_t cap)
{
    BirBloco b = {.frio = frio, .cap = cap};

    b.instrs = Realocar(NULL, (size_t)cap * sizeof *b.instrs);
    return b;
}

static void EmpurrarBloco(ListaBlocos *l, const BirBloco *b)
{
    if (l->n == l->cap) {
        l->cap = l->cap == 0 ? LISTA_INICIAL : l->cap * 2;
        l->v = Realocar(l->v, l->cap * sizeof *l->v);
    }
    l->v[l->n++] = *b;
}

static uint32_t BlocosNaoFrios(const BirFuncao *g)
{
    uint32_t n = 0;

    for (uint32_t b = 0; b < g->nblocos; b++)
        n += g->blocos[b].frio ? 0 : 1;
    return n;
}

static BirInstr NovaInstr(uint8_t op, uint8_t classe, const BirInstr *origem)
{
    return (BirInstr){.op = op, .classe = classe, .dst = BIR_SEM_VREG, .simbolo = BIR_SEM_SIMBOLO, .alvo = {BIR_SEM_BLOCO, BIR_SEM_BLOCO}, .pos = origem->pos, .ir = origem->ir};
}

/* Operando limpo para virar fonte de COPIA/CONST/COPIA_MEM: sem extensao do argumento, sem tam do agregado. */
static BirOperando Limpo(BirOperando o)
{
    o.extensao = BE_NENHUMA;
    o.tam = 0;
    o.sysv = NULL;
    return o;
}

static void RemapOperando(BirOperando *o, uint32_t vbase)
{
    if (o->tipo == BO_VREG)
        o->id += vbase;
}

static bool EhOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

static void RemapAlvos(BirInstr *i, const uint32_t *mapa)
{
    if (i->op == BIR_SALTA || EhOv(i->op)) {
        i->alvo[0] = mapa[i->alvo[0]];
    } else if (i->op == BIR_DESVIA) {
        i->alvo[0] = mapa[i->alvo[0]];
        i->alvo[1] = mapa[i->alvo[1]];
    }
}

/* O corpo a clonar: a folha, ou a copia do proprio chamador quando o sitio e recursivo. */
static const BirFuncao *CorpoDoSitio(const Montagem *m, const SitioInline *s)
{
    return m->tem_propria && s->folha == (uint32_t)(m->f - m->p->funcoes) ? &m->propria : &m->p->funcoes[s->folha];
}

/* Copia funda de blocos, classes de vreg e slots do corpo (o resto da estrutura e so lido pelo clone). Quem chama libera com LiberarCorpo. */
void CopiarCorpo(const BirFuncao *f, BirFuncao *c)
{
    *c = *f;
    c->blocos = Realocar(NULL, (f->nblocos + 1u) * sizeof *c->blocos);
    for (uint32_t b = 0; b < f->nblocos; b++) {
        c->blocos[b] = f->blocos[b];
        c->blocos[b].instrs = Realocar(NULL, (f->blocos[b].n + 1u) * sizeof *c->blocos[b].instrs);
        memcpy(c->blocos[b].instrs, f->blocos[b].instrs, f->blocos[b].n * sizeof *c->blocos[b].instrs);
    }
    c->classe_vreg = Realocar(NULL, f->nvregs + 1u);
    memcpy(c->classe_vreg, f->classe_vreg, f->nvregs);
    c->slots = Realocar(NULL, (f->nslots + 1u) * sizeof *c->slots);
    if (f->nslots > 0)
        memcpy(c->slots, f->slots, f->nslots * sizeof *c->slots);
}

void LiberarCorpo(BirFuncao *c)
{
    for (uint32_t b = 0; b < c->nblocos; b++)
        free(c->blocos[b].instrs);
    free(c->blocos);
    free(c->classe_vreg);
    free(c->slots);
}

/* ---- preparo ---- */

/* Ids dos blocos antigos e o total de blocos fora dos extras; soma vregs e slots que os clones pedem. */
static void Preparar(Montagem *m)
{
    uint32_t id = 0, si = 0;

    m->mapa = Realocar(NULL, (m->f->nblocos + 1u) * sizeof *m->mapa);
    for (uint32_t b = 0; b < m->f->nblocos; b++) {
        m->mapa[b] = id++;
        for (; si < m->nsitios && m->sitios[si].bloco == b; si++) {
            const BirFuncao *g = CorpoDoSitio(m, &m->sitios[si]);

            id +=BlocosNaoFrios(g) + 1u;
            m->vregs_mais += g->nvregs;
            m->slots_mais += g->nslots;
        }
    }
    m->nprincipal = id;
}

static void CrescerChamador(Montagem *m)
{
    BirFuncao *f = m->f;

    m->vregs = f->nvregs;
    m->slots = f->nslots;
    f->classe_vreg = Realocar(f->classe_vreg, f->nvregs + m->vregs_mais + 1u);
    if (m->slots_mais > 0) {
        f->slots = Realocar(f->slots, (f->nslots + m->slots_mais) * sizeof *f->slots);
        f->cap_slots = f->nslots + m->slots_mais;
    }
}

/* ---- um sitio ---- */

static void IniciarSitio(Montagem *m, const SitioInline *s, Sitio *k)
{
    uint32_t pre = m->principal.n, q = pre + 1u, extra = m->nprincipal + m->extras.n;

    k->g = CorpoDoSitio(m, s);
    k->chama = &m->f->blocos[s->bloco].instrs[s->instr];
    k->vbase = m->vregs;
    k->sbase = m->slots;
    k->add = 0;
    for (uint32_t b = 0; b < k->g->nblocos; b++)
        k->cmap[b] = k->g->blocos[b].frio ? extra++ : q++;
    k->bpos = q;
    memcpy(m->f->classe_vreg + k->vbase, k->g->classe_vreg, k->g->nvregs);
    if (k->g->nslots > 0)
        memcpy(m->f->slots + k->sbase, k->g->slots, k->g->nslots * sizeof *k->g->slots);
    m->vregs += k->g->nvregs;
    m->slots += k->g->nslots;
}

/* Cada parametro vira um vreg novo com o valor do argumento (a folha pode regravar o parametro; resolve x = f(x)). */
static void EmitirParametros(Montagem *m, Sitio *k)
{
    for (uint32_t p = 0; p < k->g->nparams; p++) {
        BirOperando arg = Limpo(k->chama->args[p]);
        uint8_t classe = k->g->classe_vreg[p];
        uint8_t op = arg.tipo == BO_IMED ? BIR_CONST : (arg.tipo == BO_SIMBOLO ? BIR_END_SIMBOLO : BIR_COPIA);
        BirInstr i = NovaInstr(op, classe, k->chama);

        i.dst = k->vbase + p;
        i.a = arg;
        Acrescentar(&m->cur, &i);
        k->add++;
    }
}

static void EmitirRetorno(Montagem *m, const Sitio *k, const BirInstr *ret, BirBloco *saida)
{
    BirInstr i;

    if (k->g->retorno_tam > 0) {
        i = NovaInstr(BIR_COPIA_MEM, BC_NADA, ret);
        i.a = Limpo(k->chama->a);
        i.b = Limpo(ret->a);
        RemapOperando(&i.b, k->vbase);
        i.desloc = (int32_t)k->g->retorno_tam;
        Acrescentar(saida, &i);
    } else if (k->chama->dst != BIR_SEM_VREG) {
        i = NovaInstr(ret->a.tipo == BO_IMED ? BIR_CONST : BIR_COPIA, m->f->classe_vreg[k->chama->dst], ret);
        i.dst = k->chama->dst;
        i.a = Limpo(ret->a);
        RemapOperando(&i.a, k->vbase);
        Acrescentar(saida, &i);
    }
    i = NovaInstr(BIR_SALTA, BC_NADA, ret);
    i.alvo[0] = k->bpos;
    Acrescentar(saida, &i);
}

/* Copia uma instrucao da folha para o chamador: vregs, slots e alvos remapeados; args na arena. */
static void ClonarInstr(Montagem *m, const Sitio *k, const BirInstr *src, BirInstr *i)
{
    *i = *src;
    if (i->dst != BIR_SEM_VREG)
        i->dst += k->vbase;
    RemapOperando(&i->a, k->vbase);
    RemapOperando(&i->b, k->vbase);
    if (i->op == BIR_END_SLOT)
        i->a.imed += k->sbase;
    if (i->nargs > 0) {
        i->args = ArenaAlocar(&m->p->arena, i->nargs * sizeof *i->args);
        memcpy(i->args, src->args, i->nargs * sizeof *i->args);
        for (uint32_t q = 0; q < i->nargs; q++)
            RemapOperando(&i->args[q], k->vbase);
    }
    RemapAlvos(i, k->cmap);
}

static void ClonarBloco(Montagem *m, Sitio *k, const BirBloco *src)
{
    BirBloco novo = BlocoComCapacidade(src->frio, src->n + 1u);

    for (uint32_t j = 0; j < src->n; j++) {
        const BirInstr *si = &src->instrs[j];

        if (si->op == BIR_RETORNA) {
            EmitirRetorno(m, k, si, &novo);
        } else {
            ClonarInstr(m, k, si, ReservarInstr(&novo));
        }
    }
    k->add += novo.n;
    EmpurrarBloco(src->frio ? &m->extras : &m->principal, &novo);
}

/* Fecha B_pre com o salto para a folha, empurra os blocos clonados e abre B_pos. */
static void EmitirSitio(Montagem *m, SitioInline *s)
{
    Sitio k;
    BirInstr salto;

    IniciarSitio(m, s, &k);
    EmitirParametros(m, &k);
    salto = NovaInstr(BIR_SALTA, BC_NADA, k.chama);
    salto.alvo[0] = k.cmap[0];
    Acrescentar(&m->cur, &salto);
    EmpurrarBloco(&m->principal, &m->cur);
    for (uint32_t b = 0; b < k.g->nblocos; b++)
        ClonarBloco(m, &k, &k.g->blocos[b]);
    s->instr_add = k.add; /* parametros + clones (RETORNA ja conta como copia + salto); o salto de B_pre e a chamada que sai se anulam */
}

/* ---- o chamador ---- */

/* Onde termina o segmento que comeca depois do sitio recem emitido: a proxima chamada trocada do mesmo bloco ou o fim dele (n). */
static uint32_t ProximoFimDeSegmento(const Montagem *m, uint32_t b, uint32_t n)
{
    bool outro = m->proximo < m->nsitios && m->sitios[m->proximo].bloco == b;

    return (outro ? m->sitios[m->proximo].instr : n) + 1u;
}

static void MontarBloco(Montagem *m, uint32_t b)
{
    const BirBloco *old = &m->f->blocos[b];

    bool tem_sitio = m->proximo < m->nsitios && m->sitios[m->proximo].bloco == b;

    if (!tem_sitio) { /* sem chamada trocada: o bloco muda de lugar com as instrucoes (so os alvos sao renumerados) */
        BirBloco *mover = &m->f->blocos[b];

        for (uint32_t j = 0; j < mover->n; j++)
            RemapAlvos(&mover->instrs[j], m->mapa);
        EmpurrarBloco(&m->principal, mover);
        mover->instrs = NULL;
        return;
    }
    m->cur = BlocoComCapacidade(old->frio, ProximoFimDeSegmento(m, b, old->n));
    for (uint32_t j = 0; j < old->n; j++) {
        BirInstr *i;

        if (m->proximo < m->nsitios && m->sitios[m->proximo].bloco == b && m->sitios[m->proximo].instr == j) {
            EmitirSitio(m, &m->sitios[m->proximo++]);
            m->cur = BlocoComCapacidade(old->frio, ProximoFimDeSegmento(m, b, old->n) - j);
            continue;
        }
        i = ReservarInstr(&m->cur);
        *i = old->instrs[j];
        RemapAlvos(i, m->mapa);
    }
    EmpurrarBloco(&m->principal, &m->cur);
    free(m->f->blocos[b].instrs); /* ja copiado: devolve o espaco antes dos proximos blocos pedirem o seu */
    m->f->blocos[b].instrs = NULL;
}

/* Troca o vetor de blocos do chamador pelo novo (principal + extras) e libera o antigo. */
static void Concluir(Montagem *m)
{
    BirFuncao *f = m->f;
    uint32_t total = m->principal.n + m->extras.n;
    BirBloco *novo = Realocar(NULL, (total + 1u) * sizeof *novo);

    for (uint32_t b = 0; b < f->nblocos; b++)
        free(f->blocos[b].instrs);
    free(f->blocos);
    memcpy(novo, m->principal.v, m->principal.n * sizeof *novo);
    memcpy(novo + m->principal.n, m->extras.v, m->extras.n * sizeof *novo);
    f->blocos = novo;
    f->nblocos = f->cap_blocos = total;
    f->nvregs = m->vregs;
    f->nslots = m->slots;
    free(m->principal.v);
    free(m->extras.v);
    free(m->mapa);
}

/* LIMITE: parametros 5 (programa, chamador, sitios+n e o corpo que a 2a passada reusa; struct de uso unico so acrescentaria um salto de leitura). */
void InlineClonar(BirPrograma *programa, uint32_t chamador, SitioInline *sitios, uint32_t n, const BirFuncao *corpo)
{
    Montagem m = {.p = programa, .f = &programa->funcoes[chamador], .sitios = sitios, .nsitios = n};

    if (corpo != NULL) {
        m.propria = *corpo;
        m.tem_propria = true;
    }
    Preparar(&m);
    CrescerChamador(&m);
    for (uint32_t b = 0; b < m.f->nblocos; b++)
        MontarBloco(&m, b);
    Concluir(&m);
}

/* ---- guarda de retorno antecipado (F2b): inline.c escolhe o sitio; aqui so o corte e a montagem dos blocos ---- */

/* Bloco novo no fim do vetor (o vetor cresce: quem guarda ponteiro para bloco nao pode atravessar esta chamada). */
static uint32_t NovoBloco(BirFuncao *f, bool frio)
{
    uint32_t id = f->nblocos;

    f->blocos = Realocar(f->blocos, (id + 1u) * sizeof *f->blocos);
    f->blocos[id] = BlocoComCapacidade(frio, BLOCO_INSTRS_INICIAL);
    f->nblocos = f->cap_blocos = id + 1u;
    return id;
}

/* Vreg novo da funcao (o cond do cmp da guarda): so a classe e nova, o resto da funcao nao muda de numero. */
static uint32_t NovoVreg(BirFuncao *f, uint8_t classe)
{
    uint32_t v = f->nvregs++;

    f->classe_vreg = Realocar(f->classe_vreg, f->nvregs + 1u);
    f->classe_vreg[v] = classe;
    return v;
}

static void SaltarPara(BirBloco *b, const BirInstr *origem, uint32_t alvo)
{
    BirInstr i = NovaInstr(BIR_SALTA, BC_NADA, origem);

    i.alvo[0] = alvo;
    Acrescentar(b, &i);
}

/* O valor que a folha devolve, no destino da chamada: COPIA do argumento do parametro, ou CONST do imediato. Sem destino (resultado descartado): nada. */
static void EscreverValorDaFolha(BirFuncao *f, const GuardaRetorno *gr, const BirInstr *chama, uint32_t folha)
{
    BirInstr i;

    if (chama->dst == BIR_SEM_VREG)
        return;
    i = NovaInstr(gr->devolvido.tipo == BO_IMED ? BIR_CONST : BIR_COPIA, f->classe_vreg[chama->dst], chama);
    i.dst = chama->dst;
    i.a = Limpo(gr->devolvido.tipo == BO_VREG ? chama->args[gr->param] : gr->devolvido);
    Acrescentar(&f->blocos[folha], &i);
}

uint32_t InlineGuardar(BirPrograma *programa, const SitioGuarda *s)
{
    BirFuncao *f = &programa->funcoes[s->chamador];
    const GuardaRetorno *gr = &s->gr;
    const BirInstr chama = f->blocos[s->bloco].instrs[s->instr];
    uint32_t cond = NovoVreg(f, BC_I8);
    uint32_t folha = NovoBloco(f, false), lenta = NovoBloco(f, false), resto = NovoBloco(f, false);
    BirBloco *antes = &f->blocos[s->bloco];
    BirInstr i;

    for (uint32_t j = s->instr + 1u; j < antes->n; j++)
        Acrescentar(&f->blocos[resto], &antes->instrs[j]);
    antes->n = s->instr;
    i = NovaInstr(BIR_CMP, BC_I8, &chama);
    i.aux = gr->aux;
    i.dst = cond;
    i.a = Limpo(chama.args[gr->param]);
    i.b = Limpo(gr->limite);
    Acrescentar(antes, &i);
    i = NovaInstr(BIR_DESVIA, BC_NADA, &chama);
    i.a = (BirOperando){.tipo = BO_VREG, .classe = BC_I8, .id = cond};
    i.alvo[0] = gr->folha_verdadeira ? folha : lenta;
    i.alvo[1] = gr->folha_verdadeira ? lenta : folha;
    Acrescentar(antes, &i);
    EscreverValorDaFolha(f, gr, &chama, folha);
    SaltarPara(&f->blocos[folha], &chama, resto);
    Acrescentar(&f->blocos[lenta], &chama);
    SaltarPara(&f->blocos[lenta], &chama, resto);
    return resto;
}
