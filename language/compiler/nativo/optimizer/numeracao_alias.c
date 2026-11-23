/* numeracao_alias.c - alias de memoria do passe `numeracao`: uma gravacao (ou copia/zeramento) pode sobrepor uma carga lembrada? Ver numeracao_interno.h.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - S1b (D0703): NumTamanhoDaClasse(BC_V128) = 16.
 *            0.1.0 - M7b R3: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Regras (so dizem "nao sobrepoe" com prova; na duvida sobrepoe): mesma base (mesmo vreg, ou END_SLOT de definicao unica do mesmo slot) compara as faixas [desloc, desloc+tam); slots
 * diferentes nunca se sobrepoem; base de array e passo (p = base + i*S, p de definicao unica, base de definicao unica ou slot) iguais com as duas faixas dentro de [0, S) e disjuntas
 * nao se sobrepoem, seja qual for o indice de cada lado (E2 do M7b: campos distintos do mesmo elemento ou de elementos distintos).
 */
#include "nativo/optimizer/numeracao_interno.h"

uint32_t NumTamanhoDaClasse(uint8_t classe)
{
    static const uint8_t BYTES[] = {0, 1, 2, 4, 8, 4, 8, 16}; /* BC_NADA, I8, I16, I32, I64, F32, F64, V128 */

    return classe < sizeof BYTES ? BYTES[classe] : 0u;
}

static const BirInstr *DefinicaoUnica(const NumCtx *c, uint32_t vreg)
{
    if (vreg >= c->nvregs || c->ndefs[vreg] != 1 || vreg < c->f->nparams || c->def_b[vreg] == NUM_SEM)
        return NULL;
    return &c->f->blocos[c->def_b[vreg]].instrs[c->def_k[vreg]];
}

/* A base de um acesso: END_SLOT de definicao unica identifica o slot (dois vregs do mesmo slot sao a mesma base); senao o proprio vreg. */
NumBase NumBaseDe(const NumCtx *c, uint32_t vreg)
{
    const BirInstr *d = DefinicaoUnica(c, vreg);

    if (d != NULL && d->op == BIR_END_SLOT)
        return (NumBase){1, (uint32_t)d->a.imed};
    return (NumBase){0, vreg};
}

/* O vreg e idx * S (MUL por imediato > 0) ou idx << k (k < 31) de definicao unica? Devolve o passo S em *passo; 0 = nao e. */
static bool ProdutoComPasso(const NumCtx *c, uint32_t vreg, int64_t *passo)
{
    const BirInstr *m = DefinicaoUnica(c, vreg);

    if (m == NULL || m->b.tipo != BO_IMED || m->b.imed <= 0 || (m->op != BIR_MUL && m->op != BIR_SHL) || (m->op == BIR_SHL && m->b.imed >= 31))
        return false;
    *passo = m->op == BIR_MUL ? m->b.imed : (int64_t)1 << m->b.imed;
    return true;
}

/* p = base + i*S: devolve true com a base (de definicao unica ou slot) e o passo S. */
static bool FormaDeArray(const NumCtx *c, uint32_t p, NumBase *base, int64_t *passo)
{
    const BirInstr *soma = DefinicaoUnica(c, p);

    if (soma == NULL || soma->op != BIR_ADD || soma->classe != BC_I64 || soma->a.tipo != BO_VREG || soma->b.tipo != BO_VREG)
        return false;
    for (uint32_t lado = 0; lado < 2; lado++) {
        const BirOperando *raiz = lado == 0 ? &soma->a : &soma->b;
        const BirOperando *prod = lado == 0 ? &soma->b : &soma->a;

        if (!ProdutoComPasso(c, prod->id, passo))
            continue;
        *base = NumBaseDe(c, raiz->id);
        return base->slot || c->ndefs[raiz->id] == 1;
    }
    return false;
}

static bool FaixaNoElemento(int64_t desloc, uint32_t tam, int64_t passo)
{
    return tam > 0 && desloc >= 0 && desloc + tam <= passo;
}

/* x = carga, y = escrita (tamanho em y->vb): mesma base de array, mesmo passo e campos disjuntos dentro do elemento. */
static bool CamposDisjuntosDoArray(const NumCtx *c, const NumChave *x, const NumChave *y)
{
    uint32_t tam_x = NumTamanhoDaClasse(x->classe), tam_y = (uint32_t)y->vb;
    NumBase bx, by;
    int64_t sx, sy;

    if (!FormaDeArray(c, (uint32_t)x->va, &bx, &sx) || !FormaDeArray(c, (uint32_t)y->va, &by, &sy))
        return false;
    if (sx != sy || bx.slot != by.slot || bx.id != by.id)
        return false;
    if (!FaixaNoElemento(x->desloc, tam_x, sx) || !FaixaNoElemento(y->desloc, tam_y, sx))
        return false;
    return x->desloc + (int64_t)tam_x <= y->desloc || y->desloc + (int64_t)tam_y <= x->desloc;
}

/* carga = chave de CARREGA (va = vreg da base); gravacao = chave com op GRAVA/COPIA_MEM/ZERA_MEM (va = base, desloc = deslocamento, vb = bytes escritos; 0 = desconhecido). */
bool NumPodeSobrepor(const NumCtx *c, const NumChave *carga, const NumChave *gravacao)
{
    uint32_t tam_c = NumTamanhoDaClasse(carga->classe);
    int64_t tam_g = gravacao->vb;
    NumBase bc, bg;

    if (tam_g <= 0 || carga->ta != BO_VREG || gravacao->ta != BO_VREG)
        return true;
    bc = NumBaseDe(c, (uint32_t)carga->va);
    bg = NumBaseDe(c, (uint32_t)gravacao->va);
    if (bc.slot == bg.slot && bc.id == bg.id)
        return carga->desloc < gravacao->desloc + tam_g && gravacao->desloc < carga->desloc + (int64_t)tam_c;
    if (bc.slot && bg.slot)
        return false;
    return !CamposDisjuntosDoArray(c, carga, gravacao);
}
