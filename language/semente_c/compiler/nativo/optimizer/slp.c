/* slp.c - passe `slp` (S3 do SIMD): sementes de gravacao, uma tentativa por grupo e o relato (ver slp.h).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - S4b: veredito real do sroa (OtimSroaVeredito) 1x por funcao com semente; a gravacao segue com o limiar de 32 B, a carga contigua usa o veredito (slp_arvore.c).
 *            0.1.1 - S4a (D0707): recusa grupo em slot que o sroa escalarizaria (slot_escalarizavel, regressao 8,5x em Vec4 local); deslocamentos em int64.
 *            0.1.0- S3 (D0703): primeira versao. Arquivos: slp_arvore.c (grafo vetorial), slp_seguro.c (conferencias), slp_emitir.c (reescrita do bloco), slp_interno.h.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Por bloco quente com gravacoes F32/F64 bastantes: coleta as gravacoes (SlpChaveBase = base do acesso), forma o grupo de cada gravacao que pode ser a lane 0 (uma e so uma gravacao em cada
 * deslocamento d + k*passo da mesma base) e tenta; aplicou, recomeca o bloco (as posicoes mudaram), recusou, nao tenta de novo o mesmo grupo.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/slp.h"
#include "nativo/optimizer/sroa.h"
#include "nativo/optimizer/slp_interno.h"

#define LOJAS_MAX 256u
#define BLOCO_MAX 4000u    /* blocos maiores ficam como estao: SlpChaveBase e SlpDefNoBloco varrem o bloco */
#define TENTADAS_MAX 64u

typedef struct { uint32_t pos; uint8_t classe; SlpChave chave; int64_t desloc; } Loja;
typedef struct { uint8_t tipo; uint32_t id; int64_t desloc; } Tentada;
typedef struct { Tentada v[TENTADAS_MAX]; uint32_t n; } Tentadas;

static bool ClasseDeLane(uint8_t classe)
{
    return classe == BC_F32 || classe == BC_F64;
}

static bool BlocoTemSemente(const BirBloco *b)
{
    uint32_t f32 = 0, f64 = 0;

    if (b->frio || b->n > BLOCO_MAX)
        return false;
    for (uint32_t k = 0; k < b->n; k++) {
        f32 += (b->instrs[k].op == BIR_GRAVA && b->instrs[k].b.classe == BC_F32) ? 1u : 0u;
        f64 += (b->instrs[k].op == BIR_GRAVA && b->instrs[k].b.classe == BC_F64) ? 1u : 0u;
    }
    return f32 >= 4u || f64 >= 2u;
}

static uint32_t ColetarLojas(const SlpCtx *c, Loja *lojas)
{
    const BirBloco *b = &c->f->blocos[c->bloco];
    uint32_t n = 0;

    for (uint32_t k = 0; k < b->n && n < LOJAS_MAX; k++) {
        const BirInstr *i = &b->instrs[k];

        if (i->op != BIR_GRAVA || !ClasseDeLane(i->b.classe) || i->b.tipo != BO_VREG || i->a.tipo != BO_VREG || i->a.tam != 0 || i->b.tam != 0)
            continue;
        SlpChave chave = SlpChaveBase(c, i->a.id, k);
        int64_t desloc = (int64_t)i->desloc + chave.off; /* 64 bits: a soma em int32 estouraria perto de 2^31 */

        if (desloc != (int32_t)desloc)
            continue; /* desloc_grande: gravacao fora do alcance de int32 nao e semente */
        lojas[n++] = (Loja){k, i->b.classe, chave, desloc};
    }
    return n;
}

/* A unica gravacao da mesma base, classe e deslocamento? SLP_SEM se nao ha nenhuma ou ha mais de uma. */
static uint32_t UnicaNoDeslocamento(const Loja *lojas, uint32_t n, const Loja *modelo, int64_t desloc)
{
    uint32_t achada = SLP_SEM;

    for (uint32_t j = 0; j < n; j++) {
        if (lojas[j].classe != modelo->classe || lojas[j].desloc != desloc || !SlpMesmaChave(&lojas[j].chave, &modelo->chave))
            continue;
        if (achada != SLP_SEM)
            return SLP_SEM;
        achada = j;
    }
    return achada;
}

static uint32_t QuantasNaMesmaBase(const Loja *lojas, uint32_t n, const Loja *modelo)
{
    uint32_t q = 0;

    for (uint32_t j = 0; j < n; j++)
        q += (lojas[j].classe == modelo->classe && SlpMesmaChave(&lojas[j].chave, &modelo->chave)) ? 1u : 0u;
    return q;
}

/* Grupo cheio que comeca em lojas[i0]: 4 gravacoes F32 (passo 4) ou 2 F64 (passo 8). */
static bool MontarGrupo(const Loja *lojas, uint32_t n, uint32_t i0, SlpPlano *pl)
{
    const Loja *m = &lojas[i0];
    int64_t passo = m->classe == BC_F32 ? 4 : 8;

    memset(pl, 0, sizeof *pl);
    pl->classe = m->classe;
    pl->lanes = m->classe == BC_F32 ? 4u : 2u;
    for (uint32_t l = 0; l < pl->lanes; l++) {
        uint32_t j = UnicaNoDeslocamento(lojas, n, m, m->desloc + passo * (int64_t)l);

        if (j == SLP_SEM) {
            pl->motivo = "lanes_nao_contiguas";
            return false;
        }
        pl->loja[l] = lojas[j].pos;
    }
    if (m->chave.tipo == SB_VREG)
        pl->pend[pl->npend++] = (SlpPendente){m->chave.id, m->chave.def};
    return true;
}

static bool JaTentada(const Tentadas *t, const Loja *m)
{
    for (uint32_t k = 0; k < t->n; k++) {
        if (t->v[k].tipo == m->chave.tipo && t->v[k].id == m->chave.id && t->v[k].desloc == m->desloc)
            return true;
    }
    return false;
}

static void MarcarTentada(Tentadas *t, const Loja *m)
{
    if (t->n < TENTADAS_MAX)
        t->v[t->n++] = (Tentada){m->chave.tipo, m->chave.id, m->desloc};
}

static void Recusar(const SlpCtx *c, Buf *relato, const SlpPlano *pl)
{
    if (relato != NULL)
        BufFormatar(relato, "slp recusou: %s (%s b%u)\n", pl->motivo != NULL ? pl->motivo : "forma", c->p->simbolos[c->f->simbolo].nome, (unsigned)c->bloco);
}

/* Tenta o primeiro grupo ainda nao tentado do bloco; true se aplicou (as posicoes do bloco mudaram). */
static bool TentarUmGrupo(SlpCtx *c, Buf *relato, Tentadas *tentadas)
{
    Loja lojas[LOJAS_MAX];
    SlpPlano pl;
    uint32_t n = ColetarLojas(c, lojas);

    for (uint32_t i0 = 0; i0 < n; i0++) {
        if (JaTentada(tentadas, &lojas[i0]))
            continue;
        if (!MontarGrupo(lojas, n, i0, &pl)) {
            if (QuantasNaMesmaBase(lojas, n, &lojas[i0]) >= pl.lanes) { /* ha gravacoes bastantes para um grupo, mas nao contiguas */
                Recusar(c, relato, &pl);
                MarcarTentada(tentadas, &lojas[i0]);
            }
            continue;
        }
        if (SlpSlotEscalarizavel(c, &lojas[i0].chave)) { /* o sroa escalarizaria este slot; vetorizar o impediria */
            pl.motivo = "slot_escalarizavel";
            Recusar(c, relato, &pl);
            MarcarTentada(tentadas, &lojas[i0]);
            continue;
        }
        if (SlpMontarArvore(c, &pl) && SlpConferirPlano(c, &pl)) {
            SlpAplicarPlano(c, &pl);
            SlpContar(c);
            return true;
        }
        Recusar(c, relato, &pl);
        MarcarTentada(tentadas, &lojas[i0]);
    }
    return false;
}

static void SlpFuncao(const BirPrograma *p, BirFuncao *f, Buf *relato)
{
    SlpCtx c = {.p = p, .f = f};
    uint32_t grupos = 0;

    if (p->simbolos_dbg)
        return;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        Tentadas tentadas = {0};

        if (!BlocoTemSemente(&f->blocos[b]))
            continue;
        c.bloco = b;
        if (c.ndefs == NULL)
            SlpContar(&c);
        if (c.escalariza == NULL) { /* a BIR so muda por slp, que nao cria nem tira slot: o veredito vale a funcao toda */
            c.escalariza = calloc((size_t)f->nslots + 1u, sizeof *c.escalariza);
            if (c.escalariza == NULL)
                FalhaInterna("otim: memoria esgotada");
            OtimSroaVeredito(p, f, c.escalariza);
        }
        while (TentarUmGrupo(&c, relato, &tentadas))
            grupos++;
    }
    SlpLiberar(&c);
    free(c.escalariza);
    if (grupos > 0 && relato != NULL)
        BufFormatar(relato, "slp %s: %u grupos vetorizados\n", p->simbolos[f->simbolo].nome, (unsigned)grupos);
}

void OtimSlpAplicar(BirPrograma *programa, Buf *relato)
{
    for (uint32_t i = 0; i < programa->nfuncoes; i++)
        SlpFuncao(programa, &programa->funcoes[i], relato);
}
