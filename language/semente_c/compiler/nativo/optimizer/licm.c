/* licm.c - passe `licm` (R6a fatia 1): sobe para o pre-cabecalho a aritmetica pura invariante do laco (ver licm.h).
 * Versao: 0.4.2
 * Changelog: 0.4.2 - S4c (tempo de build): funcao sem laco sai antes de calcular vivacidade, usos e constantes (AnalisesCalcularLacosApenas); a vivacidade so e calculada quando um laco passa do pre-cabecalho (OrcamentoDeInteiros) e le so os vivos do cabecalho; defs e sobem so se aloca depois dessa checagem. Mesma BIR de saida.
 *            0.4.1- S1b (D0703): BC_V128 opaco (OperacaoSobe recusa; contagem de pressao de GPR usa BirClasseEhXmm); mesmo codigo sem V128.
 *            0.4.0 - R6a-3 (D0704): pressao de registradores inteiros: CONST inteira e a aritmetica inteira de definicao unica calculada so de imediatos/CONST nao sobem mais (heuristica (1): o isel embute imediato e `mov imm` e barato;
 *                    subir deixava 4 inteiros vivos no laco externo do nbody_f64 `avancar` e o alocador espilhou contador e limites: spills 20->34); mais o teto (2) OrcamentoDeInteiros: so sobem tantos inteiros quantos faltarem para
 *                    LICM_GPR_LIVRES (6) menos os inteiros ja vivos na entrada do cabecalho (so a (1) deixava 1 ponteiro `imul+lea` de elemento subir do laco j e o contador i ainda espilhava: 403 instr/29 spills; com o teto, o `avancar`
 *                    sai identico ao da base sem licm). Real (xmm) segue o regime de LICM_CONST_REAL_MAX; a regra do ADD x,imm de endereco (0.3.0) fica como estava.
 *            0.3.1 - R6a-3: a regra do add x, imm de endereco so vale com o passe `endereco` ligado (BirPrograma.endereco_ligado); com --sem-otim endereco o licm sobe como antes (neutralidade byte a byte). 0.3.0 - R6a-3 (D0704): `ADD x, imm32` cujo unico uso e a base de um CARREGA/GRAVA nao sobe (o isel funde no [x+imm+d]; subir so gastava um registrador no laco): OtimEnderecoSomaFundivel (endereco.h).
 *            0.2.0 - R6a-2 (D0704): imediato real de CMP real dentro do laco vira CONST vreg no pre-cabecalho (ate LICM_CONST_REAL_MAX por laco, qualquer bloco nao frio; escolhido aqui em vez de
 *                    um passo anterior porque o licm ja tem pre-cabecalho e ponto de insercao).
 *            0.1.0 - R6a: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Do laco mais interno para o mais externo; depois de subir algo as analises sao refeitas (a vivacidade de quem subiu muda e o laco de fora a consulta). O vreg e nao-SSA: por isso a condicao (c) do licm.h
 * (uma unica definicao no laco, nao vivo na entrada do cabecalho nem nas saidas) e o que garante que o valor subido e o mesmo que a instrucao daria em toda leitura.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/bir/analises.h"
#include "nativo/optimizer/cfg.h"
#include "nativo/optimizer/endereco.h"
#include "nativo/optimizer/licm.h"

#define LICM_CONST_REAL_MAX 4u /* CONST reais que o licm cria por laco (1 xmm cada) */
#define LICM_GPR_LIVRES 6u     /* 11 GPR que o alocador reparte (rax, r11, rsp e rbp ficam de fora) menos 5 para os temporarios do corpo do laco (ponteiros de elemento, produto de indice, copias) */

/* O que vale para a funcao inteira e nao muda ao subir. */
typedef struct {
    const EnderecoUsos *usos; /* usos por vreg (NULL com --sem-otim endereco) */
    uint8_t *constante;       /* por vreg (dos que existiam antes do licm): inteiro de definicao unica calculado so de imediatos (ver ConstantesDaFuncao) */
    uint32_t nconstante;
    bool *vivo_pendente;      /* a vivacidade de an ainda nao foi calculada: so o 1o laco que passa do pre-cabecalho a calcula (OrcamentoDeInteiros) */
} LicmGlobal;

typedef struct {
    const BirFuncao *f;
    AnalisesBir *an;
    const LacoBir *laco;
    const LicmGlobal *g;
    const EnderecoUsos *usos; /* usos por vreg da funcao inteira (nao mudam ao subir) */
    uint8_t *no_laco;    /* por bloco: pertence ao laco */
    uint8_t *cada_volta;  /* por bloco do laco: domina todo bloco que volta ao cabecalho (roda em toda volta que chega ao fim) */
    uint32_t *defs;       /* por vreg: definicoes dentro do laco (as que subiram ja descontadas) */
    uint32_t *saidas;     /* blocos fora do laco que uma aresta do laco alcanca */
    uint32_t orcamento;   /* inteiros novos que ainda podem subir: LICM_GPR_LIVRES menos os inteiros ja vivos na entrada do cabecalho */
    uint32_t nsaidas, pre, ins; /* pre-cabecalho e indice onde o que sobe entra (antes do CMP + DESVIA ou do SALTA final) */
} LacoCtx;

static bool OperandoSimples(const BirOperando *o)
{
    return o->tam == 0 && o->extensao == BE_NENHUMA && o->sysv == NULL;
}

/* Operacao pura e sem trap (R4: nada que possa armadilhar sobe); real so nas contas que nao trapam. */
static bool OperacaoSobe(const BirInstr *i)
{
    bool real = BirClasseEhReal((BirClasse)i->classe);

    if (i->classe == BC_V128) /* vetor e opaco para o licm */
        return false;
    switch (i->op) {
    case BIR_CONST: case BIR_COPIA: case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_NEG:
        return true;
    case BIR_AND: case BIR_OR: case BIR_XOR: case BIR_NOT: case BIR_EXT_S: case BIR_EXT_Z: case BIR_TRUNC:
        return !real;
    case BIR_END_SLOT: case BIR_END_SIMBOLO:
        return true;
    default:
        return false;
    }
}

/* Imediato, simbolo, nada, ou vreg sem definicao no laco. */
static bool OperandoInvariante(const LacoCtx *c, const BirOperando *o)
{
    if (!OperandoSimples(o))
        return false;
    if (o->tipo != BO_VREG)
        return true;
    return o->id < c->f->nvregs && c->defs[o->id] == 0;
}

static bool OperandoConstante(const LicmGlobal *g, const BirOperando *o)
{
    if (!OperandoSimples(o) || o->tipo == BO_SIMBOLO)
        return false;
    return o->tipo != BO_VREG || (o->id < g->nconstante && g->constante[o->id]);
}

static bool ContaDeConstantes(const LicmGlobal *g, const BirInstr *i, uint32_t nvregs)
{
    if (BirClasseEhXmm((BirClasse)i->classe) || i->nargs != 0 || i->dst == BIR_SEM_VREG || i->dst >= nvregs)
        return false;
    switch (i->op) {
    case BIR_CONST: case BIR_COPIA: case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_NEG:
    case BIR_AND: case BIR_OR: case BIR_XOR: case BIR_NOT: case BIR_EXT_S: case BIR_EXT_Z: case BIR_TRUNC:
        return OperandoConstante(g, &i->a) && OperandoConstante(g, &i->b);
    default:
        return false;
    }
}

/* Marca os vregs inteiros de definicao unica na funcao que so dependem de imediatos (CONST e a aritmetica sobre elas). Esses o isel embute como imediato ou refaz com `mov imm`/`lea` barato: subir
 * um deles gastava um registrador no laco de fora (nbody_f64: limites 6, 5 e 5-1 do raiz inlinado espilharam o contador). Ponto fixo: cada volta so acrescenta marcas.
 * LIMITE: niveis a contagem de definicoes e o ponto fixo sao dois percursos bloco x instrucao; separar so repassaria o ctx.
 * LIMITE: cognitiva idem (dois percursos aninhados). */
static void ConstantesDaFuncao(const BirFuncao *f, LicmGlobal *g)
{
    uint32_t *defs = calloc((size_t)f->nvregs + 1u, sizeof *defs);
    bool mudou = true;

    g->constante = calloc((size_t)f->nvregs + 1u, 1);
    g->nconstante = f->nvregs;
    if (defs == NULL || g->constante == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t m = 0; m < f->blocos[b].n; m++) {
            uint32_t d = f->blocos[b].instrs[m].dst;

            if (d != BIR_SEM_VREG && d < f->nvregs)
                defs[d]++;
        }
    }
    while (mudou) {
        mudou = false;
        for (uint32_t b = 0; b < f->nblocos; b++) {
            for (uint32_t m = 0; m < f->blocos[b].n; m++) {
                const BirInstr *i = &f->blocos[b].instrs[m];

                if (ContaDeConstantes(g, i, f->nvregs) && defs[i->dst] == 1 && !g->constante[i->dst]) {
                    g->constante[i->dst] = 1;
                    mudou = true;
                }
            }
        }
    }
    free(defs);
}

static bool VivoNaSaida(const LacoCtx *c, uint32_t vreg)
{
    if (AnalisesVivoEntra(c->an, c->laco->cabecalho, vreg))
        return true;
    for (uint32_t k = 0; k < c->nsaidas; k++) {
        if (AnalisesVivoEntra(c->an, c->saidas[k], vreg))
            return true;
    }
    return false;
}

static bool LeVreg(const BirOperando *o, uint32_t vreg)
{
    return o->tipo == BO_VREG && o->id == vreg;
}

/* A instrucao k do fim do pre-cabecalho (CMP + DESVIA ou SALTA) le ou escreve o vreg? */
static bool FimToca(const BirInstr *k, uint32_t vreg)
{
    return k->dst == vreg || LeVreg(&k->a, vreg) || LeVreg(&k->b, vreg);
}

/* A instrucao sobe para ANTES do fim do pre-cabecalho: nao pode mexer no que o CMP e o DESVIA leem nem ler o que o CMP escreve. */
static bool CompativelComFim(const LacoCtx *c, const BirInstr *i)
{
    const BirBloco *p = &c->f->blocos[c->pre];

    for (uint32_t k = c->ins; k < p->n; k++) {
        if (FimToca(&p->instrs[k], i->dst) || (p->instrs[k].dst != BIR_SEM_VREG && (LeVreg(&i->a, p->instrs[k].dst) || LeVreg(&i->b, p->instrs[k].dst))))
            return false;
    }
    return true;
}

static bool PodeSubir(const LacoCtx *c, const BirInstr *i)
{
    if (!OperacaoSobe(i) || i->nargs != 0 || i->dst == BIR_SEM_VREG || i->dst >= c->f->nvregs || c->defs[i->dst] != 1)
        return false;
    if (!OperandoInvariante(c, &i->a) || !OperandoInvariante(c, &i->b) || (c->usos != NULL && OtimEnderecoSomaFundivel(c->usos, i)))
        return false;
    if (i->dst < c->g->nconstante && c->g->constante[i->dst])
        return false; /* pressao de GPR: constante inteira fica no laco (ver ConstantesDaFuncao) */
    if (c->orcamento == 0 && !BirClasseEhXmm((BirClasse)i->classe))
        return false; /* pressao de GPR: o laco ja tem inteiros demais vivos (ver OrcamentoDeInteiros) */
    return CompativelComFim(c, i) && !VivoNaSaida(c, i->dst);
}

static void GarantirCapacidade(BirBloco *b, uint32_t n)
{
    BirInstr *novo;

    if (n <= b->cap)
        return;
    novo = realloc(b->instrs, (size_t)n * sizeof *novo);
    if (novo == NULL)
        FalhaInterna("otim: memoria esgotada");
    b->instrs = novo;
    b->cap = n;
}

/* Termina em SALTA para h, ou em DESVIA com so um dos alvos em h (laco com guarda: o outro alvo vai para saidas[] porque o que sobe tambem roda por la, sem trap). */
static bool FimDoPreCabecalho(LacoCtx *c, uint32_t pre)
{
    const BirBloco *p = &c->f->blocos[pre];
    const BirInstr *t = &p->instrs[p->n - 1];
    uint32_t h = c->laco->cabecalho;

    c->pre = pre;
    c->ins = p->n - 1u;
    if (t->op == BIR_SALTA)
        return t->alvo[0] == h;
    if (t->op != BIR_DESVIA || (t->alvo[0] == h) == (t->alvo[1] == h))
        return false;
    c->saidas[c->nsaidas++] = t->alvo[0] == h ? t->alvo[1] : t->alvo[0];
    if (p->n >= 2 && p->instrs[p->n - 2].op == BIR_CMP && LeVreg(&t->a, p->instrs[p->n - 2].dst))
        c->ins = p->n - 2u; /* o CMP fica colado no DESVIA (o isel funde os dois) */
    return true;
}

/* Unico predecessor do cabecalho fora do laco (ver FimDoPreCabecalho); false = laco recusado. */
static bool AcharPreCabecalho(LacoCtx *c)
{
    const AnalisesBir *an = c->an;
    uint32_t h = c->laco->cabecalho, fora = 0, pre = BIR_SEM_BLOCO;

    for (uint32_t k = an->pred_ini[h]; k < an->pred_ini[h + 1]; k++) {
        if (!c->no_laco[an->pred[k]]) {
            fora++;
            pre = an->pred[k];
        }
    }
    if (fora != 1 || pre == h || c->f->blocos[pre].n == 0)
        return false;
    return FimDoPreCabecalho(c, pre);
}

/* So sobe o que roda em toda volta: subir de um ramo raro gasta um registrador o laco inteiro (particulas_soa: dois `a + 8192` dos ramos de rebote tiraram o limite do laco do registrador). */
static void MarcarCadaVolta(LacoCtx *c)
{
    const AnalisesBir *an = c->an;
    uint32_t h = c->laco->cabecalho;

    for (uint32_t k = 0; k < c->laco->ncorpo; k++) {
        uint32_t b = c->laco->corpo[k];

        c->cada_volta[b] = 1;
        for (uint32_t m = an->pred_ini[h]; m < an->pred_ini[h + 1]; m++) {
            uint32_t volta = an->pred[m];

            if (c->no_laco[volta] && !AnalisesDomina(an, b, volta))
                c->cada_volta[b] = 0;
        }
    }
}

/* Pressao de GPR: quantos inteiros novos o laco aguenta vivos alem dos que ja atravessam o cabecalho (laco aninhado ja soma os do laco de fora, pois eles estao vivos aqui). */
static uint32_t OrcamentoDeInteiros(const LacoCtx *c)
{
    uint32_t vivos = 0;
    const ConjuntoVivo *entra;

    if (*c->g->vivo_pendente) {
        AnalisesCompletarVivo(c->f, c->an);
        *c->g->vivo_pendente = false;
    }
    entra = &c->an->vivo_entra[c->laco->cabecalho]; /* percorre so os vivos (era todo vreg da funcao por laco: fj_main tem 50 mil) */

    for (uint32_t k = 0; k < entra->n; k++) {
        uint32_t v = entra->v[k];

        if (v < c->f->nvregs && !BirClasseEhXmm((BirClasse)c->f->classe_vreg[v]))
            vivos++;
    }
    return vivos >= LICM_GPR_LIVRES ? 0 : LICM_GPR_LIVRES - vivos;
}

/* Marca o corpo, conta as definicoes de cada vreg no laco e lista as saidas. false = laco recusado. */
static bool PrepararLaco(LacoCtx *c)
{
    const AnalisesBir *an = c->an;

    for (uint32_t k = 0; k < c->laco->ncorpo; k++)
        c->no_laco[c->laco->corpo[k]] = 1;
    if (!AcharPreCabecalho(c))
        return false;
    MarcarCadaVolta(c);
    c->orcamento = OrcamentoDeInteiros(c);
    c->defs = calloc((size_t)c->f->nvregs + 1u, sizeof *c->defs);
    if (c->defs == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t k = 0; k < c->laco->ncorpo; k++) {
        uint32_t b = c->laco->corpo[k];
        const BirBloco *blk = &c->f->blocos[b];

        for (uint32_t m = 0; m < blk->n; m++) {
            if (blk->instrs[m].dst != BIR_SEM_VREG && blk->instrs[m].dst < c->f->nvregs)
                c->defs[blk->instrs[m].dst]++;
        }
        for (uint32_t m = an->suc_ini[b]; m < an->suc_ini[b + 1]; m++) {
            if (!c->no_laco[an->suc[m]])
                c->saidas[c->nsaidas++] = an->suc[m];
        }
    }
    return true;
}

/* Tira a instrucao k do bloco e a poe em sobem[]. */
static void Retirar(BirBloco *b, uint32_t k, BirInstr *sobem, uint32_t *n)
{
    sobem[(*n)++] = b->instrs[k];
    memmove(&b->instrs[k], &b->instrs[k + 1], (size_t)(b->n - k - 1u) * sizeof *b->instrs);
    b->n--;
}

/* Tira do bloco o que pode subir; *n conta as que subiram. */
static void ColherDoBloco(LacoCtx *c, BirBloco *b, BirInstr *sobem, uint32_t *n)
{
    for (uint32_t m = 0; m < b->n && !b->frio;) {
        if (!PodeSubir(c, &b->instrs[m])) {
            m++;
            continue;
        }
        c->defs[b->instrs[m].dst]--;
        if (!BirClasseEhXmm((BirClasse)b->instrs[m].classe))
            c->orcamento--;
        Retirar(b, m, sobem, n);
    }
}

/* Varre o corpo ate nao subir mais nada (uma que sobe libera quem a le); devolve quantas subiram. */
static uint32_t ColherInvariantes(LacoCtx *c, BirFuncao *f, BirInstr *sobem)
{
    uint32_t n = 0, antes = UINT32_MAX;

    while (antes != n) {
        antes = n;
        for (uint32_t k = 0; k < c->laco->ncorpo; k++) {
            if (c->cada_volta[c->laco->corpo[k]])
                ColherDoBloco(c, &f->blocos[c->laco->corpo[k]], sobem, &n);
        }
    }
    return n;
}

/* Vreg novo da classe dada (o BIR e nao-SSA e o vreg novo tem definicao unica, fora de todo laco ate ser inserido). */
static uint32_t NovoVreg(BirFuncao *f, uint8_t classe)
{
    uint8_t *classes = realloc(f->classe_vreg, ((size_t)f->nvregs + 1u) * sizeof *classes);

    if (classes == NULL)
        FalhaInterna("otim: memoria esgotada");
    f->classe_vreg = classes;
    classes[f->nvregs] = classe;
    return f->nvregs++;
}

/* CMP real com imediato num bloco quente do laco: o imediato vira CONST vreg no pre-cabecalho (o isel refazia `mov r11d, imm; movd` a cada volta). Vale ate LICM_CONST_REAL_MAX por laco
 * (1 xmm cada), em qualquer bloco nao frio: o bloco raro nao e problema porque a constante custa xmm, nao GPR. Escolhido no licm e nao no numeracao: ele ja sabe o pre-cabecalho e o ponto de insercao. */
static uint32_t IcarImediatosReais(const LacoCtx *c, BirFuncao *f, BirInstr *sobem, uint32_t n)
{
    uint32_t feitos = 0;

    for (uint32_t k = 0; k < c->laco->ncorpo && feitos < LICM_CONST_REAL_MAX; k++) {
        BirBloco *b = &f->blocos[c->laco->corpo[k]];

        for (uint32_t m = 0; m < b->n && !b->frio && feitos < LICM_CONST_REAL_MAX; m++) {
            BirInstr *i = &b->instrs[m];
            uint32_t v;

            if (i->op != BIR_CMP || !BirClasseEhReal((BirClasse)i->a.classe) || i->b.tipo != BO_IMED || i->b.tam != 0 || i->b.sysv != NULL)
                continue;
            v = NovoVreg(f, i->b.classe);
            sobem[n + feitos++] = (BirInstr){.op = BIR_CONST, .classe = i->b.classe, .dst = v, .a = i->b, .pos = i->pos, .alvo = {BIR_SEM_BLOCO, BIR_SEM_BLOCO}, .simbolo = BIR_SEM_SIMBOLO};
            i->b = (BirOperando){.tipo = BO_VREG, .classe = i->b.classe, .id = v};
        }
    }
    return feitos;
}

/* Poe as instrucoes que subiram no ponto de insercao do pre-cabecalho, na ordem em que subiram (quem le vem depois de quem escreve). */
static void InserirNoPreCabecalho(BirFuncao *f, const LacoCtx *c, const BirInstr *sobem, uint32_t n)
{
    BirBloco *p = &f->blocos[c->pre];

    GarantirCapacidade(p, p->n + n);
    memmove(&p->instrs[c->ins + n], &p->instrs[c->ins], (size_t)(p->n - c->ins) * sizeof *p->instrs);
    memcpy(&p->instrs[c->ins], sobem, (size_t)n * sizeof *sobem);
    p->n += n;
}

static uint32_t InstrucoesDoLaco(const BirFuncao *f, const LacoBir *laco)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < laco->ncorpo; k++)
        n += f->blocos[laco->corpo[k]].n;
    return n;
}

/* Um laco: devolve quantas instrucoes subiram (0 = recusado ou nada a subir). */
static uint32_t TentarLaco(BirFuncao *f, AnalisesBir *an, const LacoBir *laco, const LicmGlobal *g)
{
    LacoCtx c = {.f = f, .an = an, .laco = laco, .g = g, .usos = g->usos};
    BirInstr *sobem = NULL;
    uint32_t n = 0, ninstr = InstrucoesDoLaco(f, laco);

    c.no_laco = calloc((size_t)f->nblocos + 1u, 1);
    c.cada_volta = calloc((size_t)f->nblocos + 1u, 1);
    c.saidas = calloc((size_t)an->suc_ini[f->nblocos] + 2u, sizeof *c.saidas); /* +1: o outro alvo do DESVIA do pre-cabecalho */
    if (c.no_laco == NULL || c.cada_volta == NULL || c.saidas == NULL)
        FalhaInterna("otim: memoria esgotada");
    if (PrepararLaco(&c)) {
        sobem = malloc((size_t)ninstr * sizeof *sobem + 1u);
        if (sobem == NULL)
            FalhaInterna("otim: memoria esgotada");
        n = ColherInvariantes(&c, f, sobem);        n += IcarImediatosReais(&c, f, sobem, n); /* depois da colheita: c.defs nao conhece os vregs novos */
        if (n > 0)
            InserirNoPreCabecalho(f, &c, sobem, n);
    }
    free(c.no_laco);
    free(c.defs);
    free(c.cada_volta);
    free(c.saidas);
    free(sobem);
    return n;
}

/* Indices dos lacos do menor para o maior (o interno tem menos blocos que o que o contem). */
static uint32_t *OrdemInternoPrimeiro(const AnalisesBir *an)
{
    uint32_t *ordem = malloc((size_t)an->nlacos * sizeof *ordem + 1u);

    if (ordem == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t k = 0; k < an->nlacos; k++) {
        uint32_t j = k;

        while (j > 0 && an->lacos[ordem[j - 1]].ncorpo > an->lacos[k].ncorpo) {
            ordem[j] = ordem[j - 1];
            j--;
        }
        ordem[j] = k;
    }
    return ordem;
}

static uint32_t LicmFuncao(const BirPrograma *p, BirFuncao *f)
{
    AnalisesBir an;
    EnderecoUsos usos;
    bool vivo_pendente = true; /* a vivacidade so e calculada quando um laco passa da checagem do pre-cabecalho (OrcamentoDeInteiros) */
    LicmGlobal g = {.usos = p->endereco_ligado ? &usos : NULL, .vivo_pendente = &vivo_pendente};
    uint32_t *ordem, total = 0;

    if (!CfgElegivel(p, f))
        return 0;
    AnalisesCalcularLacosApenas(f, &an);
    if (an.nlacos == 0) { /* sem laco nada sobe: pula a vivacidade, os usos e as constantes */
        AnalisesLiberar(&an);
        return 0;
    }
    if (p->endereco_ligado)
        OtimEnderecoUsosCalcular(f, &usos);
    ConstantesDaFuncao(f, &g);
    ordem = OrdemInternoPrimeiro(&an);
    for (uint32_t k = 0; k < an.nlacos; k++) {
        uint32_t n = TentarLaco(f, &an, &an.lacos[ordem[k]], &g);

        if (n == 0)
            continue;
        total += n;
        AnalisesLiberar(&an);
        AnalisesCalcular(f, &an);
        vivo_pendente = false;
    }
    free(ordem);
    free(g.constante);
    if (p->endereco_ligado)
        OtimEnderecoUsosLiberar(&usos);
    AnalisesLiberar(&an);
    return total;
}

void OtimLicmAplicar(BirPrograma *programa, Buf *relato)
{
    for (uint32_t i = 0; i < programa->nfuncoes; i++) {
        BirFuncao *f = &programa->funcoes[i];
        uint32_t n = LicmFuncao(programa, f);

        if (n > 0 && relato != NULL)
            BufFormatar(relato, "licm %s: %u instrucoes subiram\n", programa->simbolos[f->simbolo].nome, (unsigned)n);
    }
}
