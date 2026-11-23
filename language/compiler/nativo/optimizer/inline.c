/* inline.c - passe `inline` (M7b F2): decide quais chamadas a funcao folha viram copia do corpo (a copia em si: inline_clonar.c; ver inline.h).
 * Versao: 0.5.0
 * Changelog: 0.5.0 - M7b F2b (guarda de retorno antecipado): depois das 2 passadas, cada chamada que sobra a uma folha com "cmp param, #imed; desvia" e um destino que so RETORNA
 *                    o parametro ou um imediato ganha o caminho da folha (cmp + desvia + copia/constante + salta; a chamada fica na via lenta). Orcamento proprio: ORC_GUARDA_*.
 *            0.4.0 - M7b F2 (F2 fib_rec): recursao direta ganha 2a passada: as chamadas a si mesma que sobraram no clone (sobre o corpo ORIGINAL) entram, com teto explicito
 *                    ORC_RECURSAO_CRESCE_MULT x o tamanho do chamador antes do passe (10x); nada de 3o nivel nem recursao mutua.
 *            0.3.0 - M7b R4a: folha COM laco entra (corpo quente <= ORC_QUENTES_CORPO_LACO, ate INLINE_BLOCOS_LACO_MAX blocos; sitio fora de laco so se quentes <= ORC_QUENTES_LIVRE) e recursao DIRETA
 *                    (so chama a si mesma, sem laco, <= ORC_RECURSAO_INSTR instrucoes) clona o proprio corpo 1 nivel em ate ORC_RECURSAO_SITIOS sitios recursivos (a chamada que sobra no clone chama a funcao
 *                    original); recursao mutua e niveis > 1 nao entram (F8).
 *            0.2.0 - M7b F2 rodada 3: ORC_CHAMADOR_LIVRE (chamador que ja cresceu 1024 instrucoes so aceita sitio dentro de laco: o fj_main do corpus crescia 2x sem ganho de execucao); DentroDeLaco usa
 *                    so arestas, RPO, idom e lacos (AnalisesCalcularLacosApenas, sem vivacidade); cabecalho de determinismo corrigido.
 *            0.1.0 - M7b F2: primeira versao (orcamento por instrucao quente, laco do chamador, teto por chamador e global).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe inline
 *
 * Determinismo: as chamadas saem na ordem (bloco, instrucao) de cada chamador e os chamadores na ordem de funcoes[] (a ordem do fonte); nenhuma ordem por ponteiro. A folha e avaliada
 * sob demanda (FolhaDe e preguicosa) sobre a funcao COMO ESTA naquele momento: uma funcao pequena g que chama folha ja foi inlinada em g se for declarada ANTES do chamador (entao g vira
 * folha e entra nele) e nao entra se for declarada DEPOIS (g ainda tem CHAMA). E deterministico e correto, pois todo cheque roda sobre a g ja crescida (ex.: aabb_raio: `acerta` absorve
 * `menor`/`maior`). Fixtures: inline_ordem_decl.fj e inline_ordem_decl2.fj.
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/analises.h"
#include "nativo/optimizer/inline.h"

/* Orcamento (o Fable ajusta): instrucoes de blocos nao frios da folha. */
#define ORC_QUENTES_LIVRE 16u      /* ate aqui entra em qualquer lugar */
#define ORC_QUENTES_LACO 40u       /* ate aqui so se a chamada esta dentro de laco do chamador */
#define ORC_QUENTES_CORPO_LACO 48u /* folha COM laco: 40 de ORC_QUENTES_LACO + 8 de cabeca/saida do laco (soma de laco tipica tem 12..20; passou disso e a copia custa mais que a chamada que roda 1x) */
#define ORC_RECURSAO_INSTR 64u     /* recursiva: instrucoes do corpo (todas, nao so as quentes: o clone copia tudo) */
#define ORC_RECURSAO_SITIOS 2u     /* recursiva: clones por funcao = crescimento de no maximo 2x o corpo (fib tem 2 chamadas) */
#define ORC_RECURSAO_CRESCE_MULT 10u /* recursiva, 2a passada: o chamador inteiro cresce no maximo 10x as instrucoes que tinha antes do passe. 2 niveis de uma recursao binaria sao 1+2+4 copias do corpo (~9x com o custo de argumento e retorno); 4x deixava o fib sem o 2o nivel (medido: fib 9 -> 33 BIR e 4 sitios recusados) */
#define ORC_SLOTS_BYTES 128u       /* soma dos slots da folha */
#define ORC_RETORNO_BYTES 64u      /* retorno agregado */
#define ORC_CHAMADOR_MULT 2u       /* teto do chamador: max(MULT * n, n + FOLGA) instrucoes */
#define ORC_CHAMADOR_FOLGA 256u
#define ORC_CHAMADOR_LIVRE 1024u   /* crescimento do chamador ate aqui entra em qualquer lugar; depois so dentro de laco */
#define ORC_GLOBAL_INSTR 100000u   /* teto das instrucoes acrescentadas no programa todo */
/* Guarda (F2b), orcamento proprio (nao soma no da inline): um sitio custa 5 instrucoes novas (cmp, desvia, copia ou constante, salta da folha, salta da via lenta; a chamada nao conta).
 * Chamador: 12 sitios (fib: 8 residuais = 40). Programa: 2048 instrucoes. */
#define ORC_GUARDA_INSTR 5u
#define ORC_GUARDA_CHAMADOR 64u
#define ORC_GUARDA_GLOBAL 2048u

/* MOTIVO_RECURSIVA: so serve ao proprio corpo (para quem chama de fora conta como "chama"); MOTIVO_LACO: recursiva com laco. */
typedef enum { MOTIVO_OK, MOTIVO_CHAMA, MOTIVO_RETORNO, MOTIVO_TAMANHO, MOTIVO_SLOTS, MOTIVO_LACO, MOTIVO_RECURSIVA } Motivo;

static const char *const NOME_MOTIVO[] = {"", "chama", "retorno", "tamanho", "slots", "laco", "chama"};

/* O que o passe sabe de cada funcao como folha. nauto = chamadas a si mesma. */
typedef struct { bool avaliada, avisou; uint8_t motivo; uint32_t quentes, total, nret, nauto, bytes_slots; } Folha;

typedef struct {
    BirPrograma *p;
    Buf *relato;
    uint32_t *funcao_do_simbolo;     /* simbolo -> indice em funcoes[] (NAO_ACHOU se nao ha corpo) */
    Folha *folhas;
    uint32_t global, ninlines;       /* instrucoes acrescentadas no programa; chamadas trocadas */
    uint32_t guarda_global, guarda_chamador; /* F2b: instrucoes da guarda no programa e no chamador da vez (ORC_GUARDA_*) */
    uint32_t teto_recursao;          /* 2a passada: instrucoes totais que o chamador pode ter (ORC_RECURSAO_CRESCE_MULT x o original) */
} Passe;

#define NAO_ACHOU UINT32_MAX

static void Recusa(const Passe *ps, const BirFuncao *g, const char *motivo)
{
    if (ps->relato != NULL)
        BufFormatar(ps->relato, "inline recusa %s: %s\n", ps->p->simbolos[g->simbolo].nome, motivo);
}

/* ---- a funcao e folha? ---- */

static uint32_t Sucessores(const BirInstr *i, uint32_t *saida)
{
    if (i->op == BIR_SALTA || (i->op >= BIR_ADD_OV && i->op <= BIR_NEG_OV)) {
        saida[0] = i->alvo[0];
        return 1;
    }
    if (i->op == BIR_DESVIA) {
        saida[0] = i->alvo[0];
        saida[1] = i->alvo[1];
        return 2;
    }
    return 0;
}

/* Busca em profundidade (a folha tem no maximo INLINE_BLOCOS_LACO_MAX blocos): ha aresta de volta? cor 0 = nao visto, 1 = na pilha, 2 = terminado. */
static bool CicloDesde(const BirFuncao *g, uint32_t b, uint8_t *cor)
{
    cor[b] = 1;
    for (uint32_t j = 0; j < g->blocos[b].n; j++) {
        uint32_t suc[2], n = Sucessores(&g->blocos[b].instrs[j], suc);

        for (uint32_t q = 0; q < n; q++) {
            if (suc[q] >= g->nblocos || cor[suc[q]] == 1 || (cor[suc[q]] == 0 && CicloDesde(g, suc[q], cor)))
                return true;
        }
    }
    cor[b] = 2;
    return false;
}

static bool TemCiclo(const BirFuncao *g)
{
    uint8_t cor[INLINE_BLOCOS_LACO_MAX] = {0};

    return CicloDesde(g, 0, cor);
}

static bool EhSlot(const BirOperando *o, const uint8_t *eh, uint32_t nvregs)
{
    return o->tipo == BO_VREG && o->id < nvregs && eh[o->id] != 0;
}

/* O endereco de slot so serve de base de memoria (ou de origem do RETORNA agregado): nunca guardado, somado nem passado adiante. */
static bool UsoDeSlotSeguro(const BirFuncao *g, const BirInstr *i, const uint8_t *eh)
{
    uint32_t nv = g->nvregs;

    if (i->dst != BIR_SEM_VREG && i->dst < nv && eh[i->dst] && i->op != BIR_END_SLOT)
        return false;
    switch (i->op) {
    case BIR_END_SLOT: case BIR_COPIA_MEM:
        return true;
    case BIR_CARREGA: case BIR_ZERA_MEM:
        return !EhSlot(&i->b, eh, nv);
    case BIR_GRAVA:
        return !EhSlot(&i->b, eh, nv);
    case BIR_RETORNA:
        return g->retorno_tam > 0 || !EhSlot(&i->a, eh, nv);
    default:
        return !EhSlot(&i->a, eh, nv) && !EhSlot(&i->b, eh, nv);
    }
}

static bool SlotsSeguros(const BirFuncao *g)
{
    uint8_t *eh = calloc(g->nvregs + 1u, 1);
    bool ok = true;

    if (eh == NULL)
        return false;
    for (uint32_t b = 0; b < g->nblocos; b++) {
        for (uint32_t j = 0; j < g->blocos[b].n; j++) {
            const BirInstr *i = &g->blocos[b].instrs[j];

            if (i->op == BIR_END_SLOT && i->dst < g->nvregs)
                eh[i->dst] = 1;
        }
    }
    for (uint32_t b = 0; b < g->nblocos && ok; b++) {
        for (uint32_t j = 0; j < g->blocos[b].n && ok; j++)
            ok = UsoDeSlotSeguro(g, &g->blocos[b].instrs[j], eh);
    }
    free(eh);
    return ok;
}

static bool RetornaValido(const BirFuncao *g, const BirInstr *r)
{
    if (g->retorno_tam > 0)
        return r->a.tipo == BO_VREG && r->a.tam == g->retorno_tam;
    if (g->retorno == BC_NADA)
        return r->a.tipo == BO_NADA;
    return (r->a.tipo == BO_VREG || r->a.tipo == BO_IMED) && r->a.classe == g->retorno;
}

/* Conta as instrucoes e acha o que impede o inline que se ve numa varredura: chamada e RETORNA invalido. */
static uint8_t Varrer(const BirFuncao *g, Folha *fo)
{
    bool retorno_ruim = false;

    for (uint32_t b = 0; b < g->nblocos; b++) {
        fo->total += g->blocos[b].n;
        fo->quentes += g->blocos[b].frio ? 0 : g->blocos[b].n;
        for (uint32_t j = 0; j < g->blocos[b].n; j++) {
            const BirInstr *i = &g->blocos[b].instrs[j];

            if (i->op == BIR_CHAMA_IND || (i->op == BIR_CHAMA && i->simbolo != g->simbolo))
                return MOTIVO_CHAMA;
            fo->nauto += i->op == BIR_CHAMA;
            if (i->op == BIR_RETORNA) {
                fo->nret++;
                retorno_ruim |= !RetornaValido(g, i);
            }
        }
    }
    return retorno_ruim || fo->nret == 0 || g->retorno_tam > ORC_RETORNO_BYTES ? MOTIVO_RETORNO : MOTIVO_OK;
}

/* O corpo passa do tamanho que o tipo dele (com laco, recursivo ou comum) aceita? */
static bool CorpoGrande(const BirFuncao *g, const Folha *fo, bool laco)
{
    if (g->nblocos > INLINE_BLOCOS_LACO_MAX)
        return true;
    if (fo->nauto > 0)
        return fo->total > ORC_RECURSAO_INSTR || g->nblocos > INLINE_BLOCOS_MAX;
    if (laco)
        return fo->quentes > ORC_QUENTES_CORPO_LACO;
    return fo->quentes > ORC_QUENTES_LACO || g->nblocos > INLINE_BLOCOS_MAX;
}

static uint8_t Classificar(const BirFuncao *g, Folha *fo)
{
    uint8_t m = Varrer(g, fo);
    bool laco;

    if (m != MOTIVO_OK)
        return m;
    if (g->nblocos > INLINE_BLOCOS_LACO_MAX)
        return MOTIVO_TAMANHO;
    laco = TemCiclo(g);
    if (laco && fo->nauto > 0)
        return MOTIVO_LACO;
    if (CorpoGrande(g, fo, laco))
        return MOTIVO_TAMANHO;
    for (uint32_t k = 0; k < g->nslots; k++)
        fo->bytes_slots += g->slots[k].tam;
    if (fo->bytes_slots > ORC_SLOTS_BYTES || (g->nslots > 0 && !SlotsSeguros(g)))
        return MOTIVO_SLOTS;
    return fo->nauto > 0 ? MOTIVO_RECURSIVA : MOTIVO_OK;
}

/* A funcao j e candidata a folha: tem corpo e nao e o forja_programa. Avalia na 1a vez. */
static Folha *FolhaDe(Passe *ps, uint32_t j)
{
    Folha *fo = &ps->folhas[j];
    const BirFuncao *g = &ps->p->funcoes[j];

    if (!fo->avaliada) {
        fo->avaliada = true;
        fo->motivo = g->sintetica || g->nblocos == 0 ? MOTIVO_TAMANHO : Classificar(g, fo);
    }
    return fo;
}

/* ---- a chamada serve? ---- */

/* O argumento k combina com o parametro: mesma classe, agregado do mesmo tamanho (a extensao BE_* do argumento so vale para a convencao de fora). */
static bool ArgumentoCombina(const BirFuncao *g, const BirInstr *chama, uint32_t k)
{
    const BirOperando *o = &chama->args[k];
    uint32_t tam = g->param_tam != NULL ? g->param_tam[k] : 0;

    if (o->tipo != BO_VREG && o->tipo != BO_IMED && o->tipo != BO_SIMBOLO)
        return false;
    if (o->tam != tam || (tam > 0 && o->tipo != BO_VREG) || o->sysv != NULL)
        return false;
    return o->classe == g->classe_vreg[k];
}

static bool ArgumentosCombinam(const BirFuncao *g, const BirInstr *chama)
{
    if (chama->nargs != g->nparams)
        return false;
    for (uint32_t k = 0; k < chama->nargs; k++) {
        if (!ArgumentoCombina(g, chama, k))
            return false;
    }
    return true;
}

static bool RetornoCombina(const BirFuncao *g, const BirFuncao *f, const BirInstr *chama)
{
    if (g->retorno_tam > 0)
        return chama->a.tipo == BO_VREG && chama->a.tam == g->retorno_tam && chama->dst == BIR_SEM_VREG;
    if (chama->dst == BIR_SEM_VREG)
        return true;
    return g->retorno != BC_NADA && chama->dst < f->nvregs && f->classe_vreg[chama->dst] == g->retorno;
}

/* ---- um chamador ---- */

/* Estado da varredura de um chamador: instrucoes (orcamento) e a analise de lacos, calculada so se algum candidato a pede. */
typedef struct {
    uint32_t n, cap, usado, nauto; /* nauto: sitios recursivos aceitos */
    AnalisesBir an;
    bool tem_an;
    bool segundo;                  /* 2a passada: so chamadas a si mesma, limitadas pelo cap (teto_recursao) */
} Chamador;

static bool DentroDeLaco(Chamador *ch, const BirFuncao *f, uint32_t bloco)
{
    if (!ch->tem_an) {
        AnalisesCalcularLacosApenas(f, &ch->an);
        ch->tem_an = true;
    }
    return bloco < ch->an.nblocos && ch->an.prof_laco[bloco] > 0;
}

static uint32_t CustoDoSitio(const BirFuncao *g, const Folha *fo, const BirInstr *chama)
{
    bool copia = g->retorno_tam > 0 || chama->dst != BIR_SEM_VREG;

    return fo->total + chama->nargs + (copia ? fo->nret : 0);
}

/* Motivo (texto) de recusar esta chamada, ou NULL se entra. */
static const char *RecusarChamada(Passe *ps, Chamador *ch, const BirFuncao *f, const SitioInline *s)
{
    const BirInstr *chama = &f->blocos[s->bloco].instrs[s->instr];
    const BirFuncao *g = &ps->p->funcoes[s->folha];
    const Folha *fo = &ps->folhas[s->folha];
    uint32_t custo = CustoDoSitio(g, fo, chama);
    bool propria = g == f; /* recursao: o corpo ja e limitado por ORC_RECURSAO_INSTR e os clones por ORC_RECURSAO_SITIOS */

    if (!ArgumentosCombinam(g, chama))
        return "extensao";
    if (!RetornoCombina(g, f, chama))
        return "retorno";
    if (propria && !ch->segundo && ch->nauto >= ORC_RECURSAO_SITIOS)
        return "orcamento";
    if (!propria && fo->quentes > ORC_QUENTES_LIVRE && !DentroDeLaco(ch, f, s->bloco))
        return "tamanho";
    if (ch->usado >= ORC_CHAMADOR_LIVRE && !DentroDeLaco(ch, f, s->bloco))
        return "orcamento"; /* chamador que ja cresceu o bastante: so o que esta em laco (o resto de uma sequencia reta roda uma vez e so custa build) */
    if (ch->n + ch->usado + custo > ch->cap || ps->global + ch->usado + custo > ORC_GLOBAL_INSTR)
        return "orcamento";
    ch->usado += custo;
    ch->nauto += propria;
    return NULL;
}

/* Examina o CHAMA (bloco b, instrucao j): true se vira sitio. */
static bool AvaliarChamada(Passe *ps, Chamador *ch, const BirFuncao *f, SitioInline *s)
{
    const BirInstr *i = &f->blocos[s->bloco].instrs[s->instr];
    const BirFuncao *g;
    const char *motivo;
    Folha *fo;

    if (f->blocos[s->bloco].frio || i->simbolo >= ps->p->nsimbolos || ps->funcao_do_simbolo[i->simbolo] == NAO_ACHOU)
        return false;
    s->folha = ps->funcao_do_simbolo[i->simbolo];
    g = &ps->p->funcoes[s->folha];
    if (ch->segundo && g != f)
        return false; /* 2a passada: so a propria funcao (recursao mutua fica fora) */
    fo = FolhaDe(ps, s->folha);
    if (fo->motivo != MOTIVO_OK && !(fo->motivo == MOTIVO_RECURSIVA && g == f)) {
        if (!fo->avisou && !g->sintetica)
            Recusa(ps, g, NOME_MOTIVO[fo->motivo]);
        fo->avisou = true;
        return false;
    }
    motivo = RecusarChamada(ps, ch, f, s);
    if (motivo != NULL)
        Recusa(ps, g, motivo);
    return motivo == NULL;
}

static uint32_t ContarInstrucoes(const BirFuncao *f)
{
    uint32_t n = 0;

    for (uint32_t b = 0; b < f->nblocos; b++)
        n += f->blocos[b].n;
    return n;
}

/* As chamadas do chamador que entram, em ordem (bloco, instrucao). Devolve quantas (buffer malloc em *saida; NULL se nenhuma).
 * LIMITE: cognitiva 20 e 4 niveis (varredura bloco x instrucao + crescimento do vetor de sitios; partir criaria helper de uso unico).
 * LIMITE: niveis 4 (mesmo motivo: bloco, instrucao, filtro da chamada e crescimento do vetor). */
static uint32_t ColetarSitios(Passe *ps, const BirFuncao *f, SitioInline **saida, bool segundo)
{
    Chamador ch = {.n = ContarInstrucoes(f), .segundo = segundo};
    SitioInline *v = NULL;
    uint32_t n = 0, cap = 0;

    ch.cap = ch.n * ORC_CHAMADOR_MULT > ch.n + ORC_CHAMADOR_FOLGA ? ch.n * ORC_CHAMADOR_MULT : ch.n + ORC_CHAMADOR_FOLGA;
    if (segundo)
        ch.cap = ps->teto_recursao;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t j = 0; j < f->blocos[b].n; j++) {
            SitioInline s = {.bloco = b, .instr = j};

            if (f->blocos[b].instrs[j].op != BIR_CHAMA || !AvaliarChamada(ps, &ch, f, &s))
                continue;
            if (n == cap) {
                cap = cap == 0 ? 8u : cap * 2u;
                v = realloc(v, cap * sizeof *v);
                if (v == NULL)
                    FalhaInterna("inline: memoria esgotada");
            }
            v[n++] = s;
        }
    }
    if (ch.tem_an)
        AnalisesLiberar(&ch.an);
    *saida = v;
    return n;
}

static void RelatarSitios(Passe *ps, uint32_t chamador, const SitioInline *s, uint32_t n)
{
    const BirPrograma *p = ps->p;

    for (uint32_t k = 0; k < n; k++) {
        const BirFuncao *g = &p->funcoes[s[k].folha];

        ps->global += s[k].instr_add;
        ps->ninlines++;
        if (ps->relato != NULL)
            BufFormatar(ps->relato, "inline %s <- %s b%u +%u instr +%u B\n", p->simbolos[p->funcoes[chamador].simbolo].nome, p->simbolos[g->simbolo].nome,
                        (unsigned)s[k].bloco, (unsigned)s[k].instr_add, (unsigned)ps->folhas[s[k].folha].bytes_slots);
    }
}

/* Um chamador com sitios aprovados. Se ele se chama, a 1a passada clona o corpo original nos sitios e a 2a troca as chamadas a si mesma
 * que sobraram nos clones (ainda sobre o corpo original, com teto ORC_RECURSAO_CRESCE_MULT x o tamanho antes do passe). */
static void InlinarChamador(Passe *ps, uint32_t chamador, SitioInline *sitios, uint32_t n)
{
    BirPrograma *p = ps->p;
    BirFuncao *f = &p->funcoes[chamador];
    BirFuncao corpo;
    SitioInline *segundos;
    bool recursiva = false;
    uint32_t n2;

    for (uint32_t k = 0; k < n && !recursiva; k++)
        recursiva = sitios[k].folha == chamador;
    if (recursiva) {
        CopiarCorpo(f, &corpo);
        ps->teto_recursao = ORC_RECURSAO_CRESCE_MULT * ContarInstrucoes(f);
    }
    InlineClonar(p, chamador, sitios, n, recursiva ? &corpo : NULL);
    RelatarSitios(ps, chamador, sitios, n);
    free(sitios);
    if (!recursiva)
        return;
    n2 = ColetarSitios(ps, f, &segundos, true);
    if (n2 > 0) {
        InlineClonar(p, chamador, segundos, n2, &corpo);
        RelatarSitios(ps, chamador, segundos, n2);
        free(segundos);
    }
    LiberarCorpo(&corpo);
}

/* ---- guarda de retorno antecipado (F2b): a chamada que sobra ganha cmp + desvia e, no caminho da folha, o valor sem chamar (InlineGuardar corta o bloco) ---- */

/* A folha so devolve: RETORNA do parametro k (vreg k) ou de um imediato. */
static bool LeafDevolve(const BirFuncao *g, uint32_t bloco, uint32_t param, BirOperando *devolvido)
{
    const BirInstr *r;

    if (bloco >= g->nblocos || g->blocos[bloco].n != 1)
        return false;
    r = &g->blocos[bloco].instrs[0];
    if (r->op != BIR_RETORNA || !RetornaValido(g, r))
        return false;
    if (r->a.tipo == BO_VREG && r->a.id != param)
        return false;
    *devolvido = r->a;
    return true;
}

/* Bloco de entrada = "cmp vparam, #imed" seguido de "desvia" sobre o resultado; comparacao inteira, parametro escalar. */
static bool EntradaComparaParametro(const BirFuncao *g, GuardaRetorno *gr)
{
    const BirInstr *cmp, *des;

    if (g->blocos[0].n != 2)
        return false;
    cmp = &g->blocos[0].instrs[0];
    des = &g->blocos[0].instrs[1];
    if (cmp->op != BIR_CMP || cmp->aux > BCOND_GE_U || cmp->a.tipo != BO_VREG || cmp->a.id >= g->nparams || cmp->b.tipo != BO_IMED)
        return false;
    if (des->op != BIR_DESVIA || des->a.tipo != BO_VREG || des->a.id != cmp->dst)
        return false;
    if (g->param_tam != NULL && g->param_tam[cmp->a.id] != 0)
        return false;
    gr->aux = cmp->aux;
    gr->param = cmp->a.id;
    gr->limite = cmp->b;
    return true;
}

/* A folha tem a guarda? Um dos destinos do desvia de entrada so devolve (LeafDevolve). A folha nao precisa ser folha: so o caminho dela entra no chamador. */
static bool GuardaDaFolha(const BirFuncao *g, GuardaRetorno *gr)
{
    const BirInstr *des;

    if (g->sintetica || g->nblocos < 2u || g->retorno_tam > 0 || g->retorno == BC_NADA || !EntradaComparaParametro(g, gr))
        return false;
    des = &g->blocos[0].instrs[1];
    for (uint8_t lado = 0; lado < 2; lado++) {
        if (LeafDevolve(g, des->alvo[lado], gr->param, &gr->devolvido)) {
            gr->folha_verdadeira = lado == 0;
            return true;
        }
    }
    return false;
}

/* A chamada aceita a guarda: a folha tem a guarda, o argumento do parametro e um vreg (a guarda le o mesmo valor) e o retorno combina. */
static bool ChamadaGuardavel(const Passe *ps, const BirFuncao *f, const BirInstr *chama, GuardaRetorno *gr)
{
    const BirFuncao *g;

    if (chama->simbolo >= ps->p->nsimbolos || ps->funcao_do_simbolo[chama->simbolo] == NAO_ACHOU)
        return false;
    g = &ps->p->funcoes[ps->funcao_do_simbolo[chama->simbolo]];
    if (chama->nargs != g->nparams || !GuardaDaFolha(g, gr) || chama->args[gr->param].tipo != BO_VREG)
        return false;
    return ArgumentoCombina(g, chama, gr->param) && RetornoCombina(g, f, chama);
}

static bool GuardaCabe(const Passe *ps)
{
    return ps->guarda_chamador + ORC_GUARDA_INSTR <= ORC_GUARDA_CHAMADOR && ps->guarda_global + ORC_GUARDA_INSTR <= ORC_GUARDA_GLOBAL;
}

/* Acha no bloco s->bloco (que nao e frio) a 1a chamada que aceita a guarda; grava instr e gr em s. */
static bool SitioDeGuarda(const Passe *ps, SitioGuarda *s)
{
    const BirFuncao *f = &ps->p->funcoes[s->chamador];
    const BirBloco *bl = &f->blocos[s->bloco];

    if (bl->frio)
        return false;
    for (uint32_t j = 0; j < bl->n; j++) {
        if (bl->instrs[j].op == BIR_CHAMA && ChamadaGuardavel(ps, f, &bl->instrs[j], &s->gr)) {
            s->instr = j;
            return true;
        }
    }
    return false;
}

static void RelatarGuarda(const Passe *ps, const SitioGuarda *s)
{
    const BirPrograma *p = ps->p;
    const BirInstr *chama = &p->funcoes[s->chamador].blocos[s->bloco].instrs[s->instr];

    if (ps->relato != NULL)
        BufFormatar(ps->relato, "inline guarda %s <- %s b%u\n", p->simbolos[p->funcoes[s->chamador].simbolo].nome, p->simbolos[chama->simbolo].nome,
                    (unsigned)s->bloco);
}

/* Corta o bloco em cada sitio aceito; o resto (id devolvido) e onde a varredura continua, assim os blocos novos nao sao olhados de novo. */
static void GuardarBloco(Passe *ps, SitioGuarda *s)
{
    while (GuardaCabe(ps) && SitioDeGuarda(ps, s)) {
        RelatarGuarda(ps, s);
        s->bloco = InlineGuardar(ps->p, s);
        ps->guarda_chamador += ORC_GUARDA_INSTR;
        ps->guarda_global += ORC_GUARDA_INSTR;
    }
}

/* Guarda os blocos ORIGINAIS do chamador (nblocos antes do passe); os que a guarda cria entram so pela varredura do resto. */
static void AplicarGuardasChamador(Passe *ps, uint32_t chamador)
{
    uint32_t nblocos = ps->p->funcoes[chamador].nblocos;
    SitioGuarda s = {.chamador = chamador};

    ps->guarda_chamador = 0;
    for (uint32_t b = 0; b < nblocos; b++) {
        s.bloco = b;
        GuardarBloco(ps, &s);
    }
}

/* Depois das duas passadas da inline: a guarda percorre cada funcao (sintetica fica de fora) e relata o total. */
static void AplicarGuardasPrograma(Passe *ps)
{
    for (uint32_t i = 0; i < ps->p->nfuncoes; i++) {
        if (!ps->p->funcoes[i].sintetica)
            AplicarGuardasChamador(ps, i);
    }
    if (ps->relato != NULL)
        BufFormatar(ps->relato, "inline guarda total +%u instr (teto %u)\n", (unsigned)ps->guarda_global, (unsigned)ORC_GUARDA_GLOBAL);
}

void OtimInlineAplicar(BirPrograma *programa, Buf *relato)
{
    Passe ps = {.p = programa, .relato = relato};

    if (programa->nfuncoes == 0)
        return;
    ps.funcao_do_simbolo = malloc((programa->nsimbolos + 1u) * sizeof *ps.funcao_do_simbolo);
    ps.folhas = calloc(programa->nfuncoes, sizeof *ps.folhas);
    if (ps.funcao_do_simbolo == NULL || ps.folhas == NULL)
        FalhaInterna("inline: memoria esgotada");
    for (uint32_t k = 0; k < programa->nsimbolos; k++)
        ps.funcao_do_simbolo[k] = NAO_ACHOU;
    for (uint32_t k = programa->nfuncoes; k-- > 0;) {
        if (programa->funcoes[k].nblocos > 0 && programa->funcoes[k].simbolo < programa->nsimbolos)
            ps.funcao_do_simbolo[programa->funcoes[k].simbolo] = k;
    }
    for (uint32_t i = 0; i < programa->nfuncoes; i++) {
        SitioInline *sitios;
        uint32_t n = programa->funcoes[i].nblocos == 0 ? 0 : ColetarSitios(&ps, &programa->funcoes[i], &sitios, false);

        if (n == 0)
            continue;
        InlinarChamador(&ps, i, sitios, n);
    }
    AplicarGuardasPrograma(&ps);
    if (relato != NULL)
        BufFormatar(relato, "inline total %u chamadas +%u instr (teto %u)\n", (unsigned)ps.ninlines, (unsigned)ps.global, (unsigned)ORC_GLOBAL_INSTR);
    free(ps.funcao_do_simbolo);
    free(ps.folhas);
}
