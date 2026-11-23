/* numeracao.c - passe `numeracao` (M7b R3): percurso da arvore de dominadores e decisao por instrucao (ver numeracao.h e numeracao_interno.h).
 * Versao: 0.3.7
 * Changelog: 0.3.7 - N2 (fato de desvio): conferencia de indice `cmp.ge_u v, n` repetida, dominada pelo ramo falso de outra com o mesmo v e n (sem redefinir v), vira 0 e o desvio dela vira salta; a armadilha dela nao e tocada.
 *            0.3.6 - S2 (D0703): BIR_SPLAT nao tem o operando a reescrito (vetor e opaco).
 *            0.3.5 - S1b (D0703): BC_V128 opaco (sem copia propagada, sem reuso, sem fato); a escrita de vetor ainda mata cargas.
 *            0.3.4 - R6a-2 (D0704): CMP de classe real com imediato tem o operando vreg reescrito pela copy-prop (as guardas de checagem sao inteiras e continuam intactas).
 *            0.3.3- R5 (SIMD): SELECIONA nao tem os operandos a/b reescritos (o padrao min/max real depende de serem os da comparacao).
 *            0.3.2 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.3.1 - M7b R3 rodada 2: funcao de 1 bloco terminada em RETORNA monta as analises sem o calculo geral (AnalisesDeUmBloco).
 *            0.3.0 - M7b R3 rodada 2: um NumCtx para o programa e UMA memoria de trabalho reaproveitada entre funcoes (NumTemp); sem malloc/free por funcao.
 *            0.2.0 - M7b R3 rodada 2: copy-prop com janela (origem com varias definicoes so ate o ultimo uso dela; origem de definicao unica ate NUM_JANELA_EXPR); evita o intervalo do acumulador
 *                    esticar ate o consumo da copia (matrix4_f32 perdia 22% por despejo dos acumuladores).
 *            0.1.0 - M7b R3: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Por instrucao, nesta ordem: (1) troca operando que e copia por origem; (2) conta pura ou carga com a mesma chave de um fato valido vira COPIA do vreg dele; (3) foto das dependencias,
 * definicao do dst (sobe a versao) e registro do fato novo; (4) gravacao/copia/zeramento mata as cargas que podem sobrepor, chamada mata todas.
 * Trap (R4): a instrucao nunca muda de lugar e nenhuma ARMADILHA e tocada; so some a 2a ocorrencia de uma conta/carga cuja 1a ocorrencia domina e
 * executa antes pelo mesmo caminho (se a 2a trapasse, a 1a ja teria trapado, com o mesmo valor). Bloco com *_OV: o que nasce depois do 1o *_OV nao vale no bloco frio e e descartado.
 * Desvio (N2): a 2a conferencia de indice (cmp.ge_u v, n com o mesmo v e n) dominada pelo ramo falso da 1a vira CONST 0 e o seu DESVIA vira SALTA; a armadilha dela fica
 * inalcancavel e nao e tocada (a 1a conferencia ja trapou antes, com a mesma mensagem).
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/bir/analises.h"
#include "nativo/optimizer/numeracao.h"
#include "nativo/optimizer/numeracao_interno.h"

typedef struct { uint32_t bloco, indice, ebb, carga_base, pos; } Visita;
typedef struct { uint32_t bloco, prox_filho, ebb, carga_base; NumMarca marca; bool entrou; } Quadro;
#define PALAVRAS_DO_QUADRO ((sizeof(Quadro) + sizeof(uint32_t) - 1u) / sizeof(uint32_t))

static bool DefineVreg(const BirInstr *i)
{
    if (i->dst == BIR_SEM_VREG)
        return false;
    return i->op != BIR_SALTA && i->op != BIR_DESVIA && i->op != BIR_RETORNA && i->op != BIR_ARMADILHA && i->op != BIR_GRAVA && i->op != BIR_COPIA_MEM &&
           i->op != BIR_ZERA_MEM;
}

static bool EhOv(uint8_t op)
{
    return op >= BIR_ADD_OV && op <= BIR_NEG_OV;
}

static bool EhContaPura(uint8_t op)
{
    switch (op) {
    case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_AND: case BIR_OR: case BIR_XOR: case BIR_NEG: case BIR_NOT: case BIR_EXT_S: case BIR_EXT_Z: case BIR_TRUNC:
    case BIR_DIV: case BIR_REM: case BIR_SHL: case BIR_SHR: case BIR_I_PARA_F: case BIR_F_PARA_F: case BIR_RAIZ:
        return true;
    default:
        return false;
    }
}

/* ---- fatos da funcao ---- */

static void ContarDefs(NumCtx *c)
{
    const BirFuncao *f = c->f;

    c->ndefs = NumTemp(c, (size_t)c->nvregs + 1u, 0);
    c->def_b = NumTemp(c, (size_t)c->nvregs + 1u, 0xFF);
    c->def_k = NumTemp(c, (size_t)c->nvregs + 1u, 0xFF);
    for (uint32_t v = 0; v < f->nparams && v < c->nvregs; v++)
        c->ndefs[v]++;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++) {
            const BirInstr *i = &f->blocos[b].instrs[k];

            if (!DefineVreg(i) || i->dst >= c->nvregs)
                continue;
            c->ndefs[i->dst]++;
            c->def_b[i->dst] = b;
            c->def_k[i->dst] = k;
        }
    }
}

static void MarcarUso(NumCtx *c, const BirOperando *o, uint32_t pos)
{
    if (o->tipo == BO_VREG && o->id < c->nvregs)
        c->ultimo_uso[o->id] = pos;
}

/* Posicao linear de cada instrucao alcancavel (blocos na ordem do RPO, como o alocador numera) e ultimo uso de cada vreg (a, b e argumentos). */
static void CalcularUltimosUsos(NumCtx *c)
{
    uint32_t pos = 0;

    c->ultimo_uso = NumTemp(c, (size_t)c->nvregs + 1u, 0);
    c->pos_bloco = NumTemp(c, (size_t)c->f->nblocos + 1u, 0);
    for (uint32_t r = 0; r < c->a->nrpo; r++) {
        const BirBloco *bloco = &c->f->blocos[c->a->rpo[r]];

        c->pos_bloco[c->a->rpo[r]] = pos;
        for (uint32_t k = 0; k < bloco->n; k++, pos++) {
            MarcarUso(c, &bloco->instrs[k].a, pos);
            MarcarUso(c, &bloco->instrs[k].b, pos);
            for (uint32_t j = 0; j < bloco->instrs[k].nargs; j++)
                MarcarUso(c, &bloco->instrs[k].args[j], pos);
        }
    }
}

/* O vreg tem definicao unica e ela domina o ponto (bloco, k)? Parametro domina tudo. */
static bool UnicaDefDomina(const NumCtx *c, uint32_t v, uint32_t bloco, uint32_t k)
{
    if (v >= c->nvregs || c->ndefs[v] != 1)
        return false;
    if (v < c->f->nparams)
        return true;
    if (c->def_b[v] == NUM_SEM)
        return false;
    if (c->def_b[v] == bloco)
        return c->def_k[v] < k;
    return AnalisesDomina(c->a, c->def_b[v], bloco);
}

/* ---- chaves ---- */

static int64_t ValorDe(const BirOperando *o)
{
    return o->tipo == BO_IMED ? o->imed : (o->tipo == BO_NADA ? 0 : (int64_t)o->id);
}

static bool EhComutativa(const BirInstr *i)
{
    bool soma_ou_logica = i->op == BIR_ADD || i->op == BIR_MUL || i->op == BIR_AND || i->op == BIR_OR || i->op == BIR_XOR;

    return soma_ou_logica && !BirClasseEhReal(i->classe); /* real: a ordem dos operandos decide qual NaN sai */
}

static NumChave ChaveDe(const BirInstr *i)
{
    NumChave k = {.op = i->op, .aux = i->aux, .classe = i->classe, .ta = i->a.tipo, .ca = i->a.classe, .va = ValorDe(&i->a), .tb = i->b.tipo, .cb = i->b.classe, .vb = ValorDe(&i->b),
                  .desloc = i->desloc};

    if (EhComutativa(i) && (k.ta > k.tb || (k.ta == k.tb && k.va > k.vb))) {
        k.ta = i->b.tipo; k.ca = i->b.classe; k.va = ValorDe(&i->b);
        k.tb = i->a.tipo; k.cb = i->a.classe; k.vb = ValorDe(&i->a);
    }
    return k;
}

/* Chave do que uma gravacao/copia/zeramento escreve: va = vreg da base, desloc, vb = bytes (0 = desconhecido, mata tudo). */
static NumChave ChaveDaEscrita(const BirInstr *i)
{
    NumChave g = {.op = i->op, .ta = i->a.tipo, .va = ValorDe(&i->a)};

    if (i->op == BIR_GRAVA) {
        g.desloc = i->desloc;
        g.vb = NumTamanhoDaClasse(i->b.classe);
    } else {
        g.vb = i->desloc;
    }
    return g;
}

/* ---- passo 1: copia propagada ---- */

static bool OperandoReescrevivel(const BirInstr *i, const BirOperando *o, bool lado_b)
{
    bool real = BirClasseEhReal(i->classe);

    if (o->tipo != BO_VREG || o->tam != 0 || o->extensao != BE_NENHUMA || o->classe == BC_V128) /* vetor e opaco */
        return false;
    switch (i->op) {
    case BIR_DESVIA: return false;                                /* o cmp + desvia e a guarda que o validador confere */
    case BIR_SPLAT: return false;                                   /* S2: o splat de vetor e opaco, o vreg escalar fica como esta */
    case BIR_SELECIONA: return false;                             /* a e b tem de continuar os mesmos operandos da comparacao (padrao minss/maxss do validador) */
    case BIR_CMP: return BirClasseEhReal((BirClasse)o->classe) || i->b.tipo != BO_IMED; /* cmp inteiro vN, #imed e o teste de guarda: o vN fica; real nunca e guarda (R6a-2; a classe do CMP e a do dst, i8) */
    case BIR_DIV: case BIR_REM: case BIR_SHL: case BIR_SHR: return !lado_b || real; /* o divisor/contador e o vreg que a guarda testa */
    default: return true;
    }
}

static void ReescreverOperando(NumCtx *c, const Visita *v, BirInstr *i, bool lado_b)
{
    BirOperando *o = lado_b ? &i->b : &i->a;
    uint32_t k;
    const NumFato *fato;

    if (!OperandoReescrevivel(i, o, lado_b) || o->id >= c->nvregs)
        return;
    k = c->cp[o->id];
    if (k == NUM_SEM)
        return;
    fato = &c->fatos[k];
    if (!NumFatoValido(c, fato, v->ebb))
        return;
    /* Janela: origem com varias definicoes so se propaga ate o ultimo uso dela (nao alonga o intervalo: o acumulador nao fica vivo ate o consumo da copia); origem de definicao unica,
     * ate NUM_JANELA_EXPR instrucoes depois da copia. */
    if (c->ndefs[fato->res] != 1 ? v->pos > c->ultimo_uso[fato->res] : c->seq - fato->seq > NUM_JANELA_EXPR)
        return;
    if (fato->ebb == v->ebb)
        c->n.copia.ebb++;
    else
        c->n.copia.dom++;
    o->id = fato->res;
    o->classe = c->f->classe_vreg[fato->res];
}

/* ---- passo 2: conta pura ou carga repetida ---- */

static void TrocarPorCopia(const NumCtx *c, BirInstr *i, uint32_t origem)
{
    i->op = BIR_COPIA;
    i->aux = 0;
    i->desloc = 0;
    i->a = (BirOperando){.tipo = BO_VREG, .classe = c->f->classe_vreg[origem], .id = origem};
    i->b = (BirOperando){0};
}

static void TentarReusar(NumCtx *c, const Visita *v, BirInstr *i)
{
    bool carga = i->op == BIR_CARREGA;
    NumChave chave = ChaveDe(i);
    uint32_t k = NumBuscar(c, &chave, v->ebb);
    const NumFato *fato;
    NumPar *par = carga ? &c->n.carga : &c->n.expr;

    if (k == NUM_SEM || i->dst >= c->nvregs)
        return;
    fato = &c->fatos[k];
    if (c->seq - fato->seq > (carga ? NUM_JANELA_CARGA : NUM_JANELA_EXPR) || fato->res == i->dst || c->f->classe_vreg[fato->res] != c->f->classe_vreg[i->dst])
        return;
    if (fato->ebb == v->ebb)
        par->ebb++;
    else
        par->dom++;
    TrocarPorCopia(c, i, fato->res);
}

/* ---- passo 3: fato novo ---- */

static void FotografarOperandos(const NumCtx *c, const BirInstr *i, NumSnap *s)
{
    *s = (NumSnap){.dep = {NUM_SEM, NUM_SEM, NUM_SEM}};
    if (i->a.tipo == BO_VREG && i->a.id < c->nvregs) {
        s->dep[1] = i->a.id;
        s->ver[1] = c->ver[i->a.id];
    }
    if (i->b.tipo == BO_VREG && i->b.id < c->nvregs) {
        s->dep[2] = i->b.id;
        s->ver[2] = c->ver[i->b.id];
    }
}

/* Global = o resultado tem definicao unica e todo operando vreg tem definicao unica que domina este ponto. */
static bool FatoEhGlobal(const NumCtx *c, const Visita *v, const NumSnap *s)
{
    if (c->ndefs[s->dep[0]] != 1)
        return false;
    for (uint32_t k = 1; k < 3; k++) {
        if (s->dep[k] != NUM_SEM && !UnicaDefDomina(c, s->dep[k], v->bloco, v->indice))
            return false;
    }
    return true;
}

static void RegistrarCopia(NumCtx *c, const Visita *v, const BirInstr *i, NumSnap *s)
{
    uint32_t d = i->dst, o = i->a.id;

    if (i->a.tipo != BO_VREG || o >= c->nvregs || o == d || i->a.tam != 0 || i->a.extensao != BE_NENHUMA || c->f->classe_vreg[o] != c->f->classe_vreg[d])
        return;
    s->dep[0] = d;
    s->ver[0] = c->ver[d];
    s->dep[2] = NUM_SEM;
    NumInserir(c, (NumFato){.copia = 1, .res = o, .dep = {s->dep[0], s->dep[1], NUM_SEM}, .ver = {s->ver[0], s->ver[1], 0}, .ebb = v->ebb, .seq = c->seq,
                            .global = FatoEhGlobal(c, v, s)});
}

static void RegistrarValor(NumCtx *c, const Visita *v, const BirInstr *i, NumSnap *s)
{
    bool carga = i->op == BIR_CARREGA;

    if (i->dst >= c->nvregs || i->dst == s->dep[1] || i->dst == s->dep[2])
        return;
    if (carga && c->cargas.n - v->carga_base >= NUM_CARGAS_EBB)
        return;
    s->dep[0] = i->dst;
    s->ver[0] = c->ver[i->dst];
    NumInserir(c, (NumFato){.chave = ChaveDe(i), .res = i->dst, .dep = {s->dep[0], s->dep[1], s->dep[2]}, .ver = {s->ver[0], s->ver[1], s->ver[2]}, .ebb = v->ebb, .seq = c->seq,
                            .global = !carga && FatoEhGlobal(c, v, s)});
}

/* ---- passo 4: memoria ---- */

/* escrita = NULL: chamada, mata toda carga lembrada da EBB. */
static void MatarCargas(NumCtx *c, const Visita *v, const NumChave *escrita)
{
    for (uint32_t j = v->carga_base; j < c->cargas.n; j++) {
        uint32_t k = ((const uint32_t *)c->cargas.v)[j];

        if (!c->fatos[k].morta && (escrita == NULL || NumPodeSobrepor(c, &c->fatos[k].chave, escrita))) {
            NumMatar(c, k);
            c->n.mortos++;
        }
    }
}

static void EfeitoNaMemoria(NumCtx *c, const Visita *v, const BirInstr *i)
{
    NumChave escrita;

    switch (i->op) {
    case BIR_GRAVA: case BIR_COPIA_MEM: case BIR_ZERA_MEM:
        escrita = ChaveDaEscrita(i);
        MatarCargas(c, v, &escrita);
        break;
    case BIR_CHAMA: case BIR_CHAMA_IND:
        MatarCargas(c, v, NULL);
        break;
    default:
        break;
    }
}

/* ---- passo 4b: conferencia de indice repetida (N2) ---- */

/* Operando de conferencia: imediato, ou vreg escalar sem endereco de agregado nem vetor. */
static bool OperandoDeConferencia(const BirOperando *o)
{
    if (o->tipo == BO_IMED)
        return true;
    return o->tipo == BO_VREG && o->tam == 0 && o->extensao == BE_NENHUMA && o->classe != BC_V128;
}

/* `cmp.ge_u v, n` (n imediato ou vreg) com dst vreg valido: a forma que BirConferirIndice e a checagem montam. */
static bool EhConferenciaDeIndice(const NumCtx *c, const BirInstr *i)
{
    return i->op == BIR_CMP && i->aux == BCOND_GE_U && i->classe != BC_V128 && i->dst < c->nvregs && OperandoDeConferencia(&i->a) && i->a.tipo == BO_VREG &&
           i->a.id < c->nvregs && OperandoDeConferencia(&i->b) && (i->b.tipo != BO_VREG || i->b.id < c->nvregs);
}

/* Conferencia repetida: se um fato de desvio ja diz que `v < n` vale aqui (mesma chave, versoes iguais), o cmp vira constante 0. A armadilha do ramo verdadeiro
 * nao e tocada: a 1a conferencia ja passou e ja trapa com a mesma mensagem (R4). */
static bool DobrarConferenciaRepetida(NumCtx *c, const Visita *v, BirInstr *i)
{
    NumChave chave;
    uint32_t k;

    if (!EhConferenciaDeIndice(c, i))
        return false;
    chave = ChaveDe(i);
    k = NumBuscar(c, &chave, v->ebb);
    if (k == NUM_SEM)
        return false;
    if (c->fatos[k].ebb == v->ebb)
        c->n.expr.ebb++;
    else
        c->n.expr.dom++;
    i->op = BIR_CONST;
    i->aux = 0;
    i->desloc = 0;
    i->a = (BirOperando){.tipo = BO_IMED, .classe = i->classe, .imed = 0};
    i->b = (BirOperando){0};
    return true;
}

/* O cmp dobrado era a condicao do desvio que fecha o bloco (CONST logo antes dele): o desvio vira salta para o ramo falso, o unico que o valor 0 alcanca. O frio
 * do ramo verdadeiro fica sem predecessor. */
static void FecharDesvioConhecido(BirBloco *bloco, uint32_t dobrado)
{
    BirInstr *d, *c;

    if (dobrado == NUM_SEM || bloco->n < 2)
        return;
    d = &bloco->instrs[bloco->n - 1];
    c = &bloco->instrs[bloco->n - 2];
    if (d->op != BIR_DESVIA || d->a.tipo != BO_VREG || d->a.id != dobrado || c->op != BIR_CONST || c->dst != dobrado)
        return;
    d->op = BIR_SALTA;
    d->alvo[0] = d->alvo[1];
    d->a = (BirOperando){0};
}

/* Devolve true quando a instrucao virou constante por um fato de desvio (ProcessarBloco fecha o desvio dela). */
static bool ProcessarInstrucao(NumCtx *c, const Visita *v, BirInstr *i)
{
    NumSnap s;
    bool dobrou;

    ReescreverOperando(c, v, i, false);
    ReescreverOperando(c, v, i, true);
    c->seq++;
    dobrou = DobrarConferenciaRepetida(c, v, i);
    if (i->classe != BC_V128 && (EhContaPura(i->op) || i->op == BIR_CARREGA))
        TentarReusar(c, v, i);
    FotografarOperandos(c, i, &s);
    if (DefineVreg(i) && i->dst < c->nvregs)
        NumDefinir(c, i->dst);
    if (i->classe != BC_V128) { /* vetor e opaco: nao vira fato nem copia (so o efeito na memoria vale) */
        if (i->op == BIR_COPIA)
            RegistrarCopia(c, v, i, &s);
        else if (EhContaPura(i->op) || i->op == BIR_CARREGA)
            RegistrarValor(c, v, i, &s);
    }
    EfeitoNaMemoria(c, v, i);
    return dobrou;
}

static void ProcessarBloco(NumCtx *c, BirFuncao *f, const Quadro *q)
{
    Visita v = {.bloco = q->bloco, .ebb = q->ebb, .carga_base = q->carga_base};
    BirBloco *bloco = &f->blocos[q->bloco];
    NumMarca marca_ov = {0};
    bool tem_ov = false;
    uint32_t dobrado = NUM_SEM;

    for (uint32_t k = 0; k < bloco->n; k++) {
        if (!tem_ov && EhOv(bloco->instrs[k].op)) {
            tem_ov = true;
            marca_ov = NumMarcar(c);
        }
        v.indice = k;
        v.pos = c->pos_bloco[q->bloco] + k;
        if (ProcessarInstrucao(c, &v, &bloco->instrs[k]))
            dobrado = bloco->instrs[k].dst;
    }
    if (tem_ov)
        NumRestaurar(c, marca_ov, false);
    FecharDesvioConhecido(bloco, dobrado);
}

/* ---- percurso ---- */

typedef struct { uint32_t *ini, *filho; } Filhos;

/* Filhos de cada bloco na arvore de dominadores (CSR; ordem crescente do RPO, deterministica). */
static Filhos ConstruirFilhos(NumCtx *c, const AnalisesBir *a)
{
    Filhos t = {NumTemp(c, (size_t)a->nblocos + 2u, 0), NumTemp(c, (size_t)a->nblocos + 1u, 0)};
    uint32_t *pos = NumTemp(c, (size_t)a->nblocos + 1u, 0);

    for (uint32_t r = 1; r < a->nrpo; r++)
        t.ini[a->idom[a->rpo[r]] + 1u]++;
    for (uint32_t b = 0; b < a->nblocos; b++)
        t.ini[b + 1u] += t.ini[b];
    for (uint32_t b = 0; b < a->nblocos; b++)
        pos[b] = t.ini[b];
    for (uint32_t r = 1; r < a->nrpo; r++)
        t.filho[pos[a->idom[a->rpo[r]]]++] = a->rpo[r];
    return t;
}

static bool PredecessorUnico(const AnalisesBir *a, uint32_t b)
{
    return a->pred_ini[b + 1u] - a->pred_ini[b] == 1u;
}

/* Fato de desvio (N2): `bloco` tem um so predecessor, que termina em `cmp.ge_u v, n; desvia c, frio, bloco` (bloco = ramo falso). Entao `v < n` vale no inicio do bloco
 * e na sua subarvore, enquanto v e n nao mudam. Chamado depois da marca do filho: o fato sai junto com ele. */
static void FatoDoDesvioFalso(NumCtx *c, const BirFuncao *f, uint32_t bloco, uint32_t ebb)
{
    const BirBloco *pai;
    const BirInstr *d, *cmp;
    NumSnap s;
    Visita v = {.bloco = bloco, .indice = 0};

    if (bloco == 0 || !PredecessorUnico(c->a, bloco))
        return;
    pai = &f->blocos[c->a->idom[bloco]];
    if (pai->n < 2)
        return;
    d = &pai->instrs[pai->n - 1];
    cmp = &pai->instrs[pai->n - 2];
    if (d->op != BIR_DESVIA || d->alvo[1] != bloco || d->alvo[0] == bloco || d->a.tipo != BO_VREG || d->a.id != cmp->dst || !EhConferenciaDeIndice(c, cmp))
        return;
    FotografarOperandos(c, cmp, &s);
    s.dep[0] = cmp->dst;
    s.ver[0] = c->ver[cmp->dst];
    NumInserir(c, (NumFato){.chave = ChaveDe(cmp), .res = cmp->dst, .dep = {s.dep[0], s.dep[1], s.dep[2]}, .ver = {s.ver[0], s.ver[1], s.ver[2]}, .ebb = ebb,
                            .seq = c->seq, .global = FatoEhGlobal(c, &v, &s)});
}

static void Percorrer(NumCtx *c, BirFuncao *f)
{
    const AnalisesBir *a = c->a;
    Filhos filhos = ConstruirFilhos(c, a);
    Quadro *pilha = (Quadro *)NumTemp(c, ((size_t)a->nblocos + 1u) * PALAVRAS_DO_QUADRO, 0);
    uint32_t topo = 0;

    pilha[topo++] = (Quadro){.bloco = 0, .ebb = 0, .carga_base = 0};
    while (topo > 0) {
        Quadro *q = &pilha[topo - 1u];

        if (!q->entrou) {
            q->entrou = true;
            q->marca = NumMarcar(c);
            FatoDoDesvioFalso(c, f, q->bloco, q->ebb);
            ProcessarBloco(c, f, q);
        }
        if (filhos.ini[q->bloco] + q->prox_filho < filhos.ini[q->bloco + 1u]) {
            uint32_t filho = filhos.filho[filhos.ini[q->bloco] + q->prox_filho++];
            bool unico = PredecessorUnico(a, filho);

            pilha[topo++] = (Quadro){.bloco = filho, .ebb = unico ? q->ebb : filho, .carga_base = unico ? q->carga_base : c->cargas.n};
            continue;
        }
        NumRestaurar(c, q->marca, true);
        topo--;
    }
}

static void Relatar(const BirPrograma *p, const BirFuncao *f, const NumContagem *n, Buf *relato)
{
    if (n->copia.ebb + n->copia.dom + n->expr.ebb + n->expr.dom + n->carga.ebb + n->carga.dom + n->mortos == 0)
        return;
    BufFormatar(relato, "numeracao %s: copia ebb=%u dom=%u, expr ebb=%u dom=%u, carga ebb=%u dom=%u, mortos=%u\n", p->simbolos[f->simbolo].nome, (unsigned)n->copia.ebb,
                (unsigned)n->copia.dom, (unsigned)n->expr.ebb, (unsigned)n->expr.dom, (unsigned)n->carga.ebb, (unsigned)n->carga.dom, (unsigned)n->mortos);
}

/* Palavras de memoria de trabalho que uma funcao pede (limite superior de tudo que NumTemp fatia): vetores por vreg, por bloco, tabela de fatos (2*instrucoes+64), bytes de `tirada`, pilha do percurso. */
static size_t PalavrasDeTrabalho(const BirFuncao *f, uint32_t total)
{
    size_t nv = (size_t)f->nvregs + 1u, nb = (size_t)f->nblocos + 1u;

    return 7u * nv + 6u * nb + 2u * (size_t)total + 64u + total / 4u + 2u + nb * PALAVRAS_DO_QUADRO + 16u;
}

static void NumeracaoCorpo(NumCtx *c, const BirPrograma *p, BirFuncao *f, Buf *relato)
{
    uint32_t total = 0;

    for (uint32_t b = 0; b < f->nblocos; b++)
        total += f->blocos[b].n;
    NumPreparar(c, PalavrasDeTrabalho(f, total));
    ContarDefs(c);
    CalcularUltimosUsos(c);
    NumFatosIniciar(c, total);
    Percorrer(c, f);
    c->n.mortos += p->simbolos_dbg ? 0u : NumRemoverMortos(f, c);
    if (relato != NULL)
        Relatar(p, f, &c->n, relato);
}

/* Funcao de 1 bloco que termina em RETORNA: sem aresta, so o bloco 0 sem predecessor. Monta as analises que o passe le sem rodar o calculo geral (era ~30% do custo do passe;
 * a maioria das funcoes do corpus grande tem 1 bloco). */
static bool AnalisesDeUmBloco(const BirFuncao *f, AnalisesBir *saida)
{
    static uint32_t zero = 0, um = 1, ini[2] = {0, 0};

    if (f->nblocos != 1 || f->blocos[0].n == 0 || f->blocos[0].instrs[f->blocos[0].n - 1u].op != BIR_RETORNA)
        return false;
    memset(saida, 0, sizeof *saida);
    saida->nblocos = 1;
    saida->nvregs = f->nvregs;
    saida->nrpo = 1;
    saida->pred_ini = ini;
    saida->rpo = saida->pos_rpo = saida->idom = saida->dom_pre = &zero;
    saida->dom_tam = &um;
    return true;
}

static void NumeracaoFuncao(NumCtx *c, const BirPrograma *p, BirFuncao *f, Buf *relato)
{
    AnalisesBir analises;
    bool sintetizada;

    if (f->sintetica || f->nblocos == 0 || f->nvregs == 0)
        return;
    sintetizada = AnalisesDeUmBloco(f, &analises);
    if (!sintetizada)
        AnalisesCalcularLacosApenas(f, &analises);
    if (analises.pred_ini[1] - analises.pred_ini[0] == 0) { /* bloco 0 com predecessor: nao se toca (como o sroa) */
        c->f = f;
        c->a = &analises;
        c->nvregs = f->nvregs;
        NumeracaoCorpo(c, p, f, relato);
    }
    if (!sintetizada)
        AnalisesLiberar(&analises);
}

void OtimNumeracaoAplicar(BirPrograma *programa, Buf *relato)
{
    NumCtx contexto = {0};

    for (uint32_t i = 0; i < programa->nfuncoes; i++)
        NumeracaoFuncao(&contexto, programa, &programa->funcoes[i], relato);
    NumFatosLiberar(&contexto);
}
