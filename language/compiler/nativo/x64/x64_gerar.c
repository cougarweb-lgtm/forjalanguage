/* x64_gerar.c - X64Gerar: BIR -> Objeto. Para cada BirFuncao: quadro, alocacao, selecao e codificacao em .text; os textos vao para .rdata.
 * Versao: 0.9.0
 * Changelog: 0.9.0 - M7a F5: com BirFuncao.isel_otim, X64Ctx.usos/defs e a selecao pos-alocador (IselOtimTentar) antes de X64Selecionar; sem a flag nada muda.
 *            0.8.1 - M6e P93d: X64Ctx.guarda = AlvoColheita.guarda.
 *            0.8.0 - M6 P65b: ganhos do DbgPrograma (AlvoColheita.dbg): DbgProcAbrir + X64DbgLocais no inicio da funcao, DbgLinhaVista por instrucao (so grava quando a pos muda) e DbgProcFechar antes do padding; a FRT1 deixa de ter vetores proprios (X64RastroIniciar/Funcao saem).
 *            0.7.0 - M5b P38b: depois das funcoes, X64GerarEntradasSysv emite o stub fjt_ de cada callback escalar do alvo linux (nenhum byte muda sem entradas).
 *            0.6.1 - M5b P37: QuadroCalcular recebe o programa (a area SysV e o salvamento de rsi/rdi/xmm6-15 dependem dele).
 * Changelog: 0.6.0 - IA1-b12: com AlvoColheita.rastreio, X64RastroFuncao por funcao e X64RastroFechar no fim (tabela de posicoes do --rastreio no .rdata); sem a flag nada muda.
 *            0.5.0 - M4 P24a: X64Gerar recebe a AlvoColheita e mede 3 fatias (regs, isel, codificar) com uma leitura de relogio por funcao e fatia; nenhum byte gerado muda.
 *            0.4.1 - M2 P11b: X64Ctx.cls guarda AbiArg (classe e tam) em vez de BirClasse.
 *            0.4.0 - M2 P11a: X64Ctx.slot_off (onde cada slot de memoria mora no quadro).
 *            0.3.0 - M2 P10b: BS_DADO em .rdata (simbolo local ".d<ordem>", alinhado: a secao do programa leva o maior alinhamento dos dados e o ligador respeita
 *                    o alinhamento de cada contribuicao).
 *            0.2.1 - backend x64 P8b: X64Ctx.origem (prologo, codigo, frio, epilogo) para a listagem do --debug-code.
 *            0.2.0 - backend x64 P8: .pdata/.xdata por funcao (X64UnwindRegistrar), listagem opcional do --debug-code, laco da sondagem em bloco_off.
 *            0.1.0 - backend x64 P7a: X64Gerar (simbolo global por funcao, .s<ordem> local por texto, padding 0xCC ate 16 B entre funcoes).
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe> e python -I ..\tests\nativo\obj\verificar_obj.py
 *
 * Ordem dentro da funcao: prologo, blocos quentes, epilogo unico, blocos frios (QuadroOrdenar). Os saltos locais saem como rel32 com
 * zero e sao corrigidos no fim da funcao, quando o inicio de cada bloco ja e conhecido.
 */
#include <stdio.h>

#include "common/arena.h"
#include "common/base.h"
#include "nativo/x64/x64_gerar.h"
#include "nativo/x64/x64_interno.h"
#include "nativo/x64/x64_isel_otim.h"

#define GERAR_NOME_TEXTO 24

typedef enum { FATIA_REGS, FATIA_ISEL, FATIA_CODIFICAR } FatiaTempo;

/* Fecha a fatia em curso: soma ao acumulador da fatia o tempo desde a ultima marca e avanca a marca. Uma leitura de relogio por chamada, e as
 * chamadas sao por FUNCAO (nunca por instrucao): sem medicao (tempos NULL) nao le o relogio. */
static void Fatiar(X64Ctx *c, FatiaTempo fatia)
{
    uint64_t agora;
    AlvoTempos *t = c->tempos;

    if (t == NULL)
        return;
    agora = RelogioNs();
    if (fatia == FATIA_REGS)
        t->regs_ns += agora - c->marca;
    else if (fatia == FATIA_ISEL)
        t->isel_ns += agora - c->marca;
    else
        t->codificar_ns += agora - c->marca;
    c->marca = agora;
}

/* Selecao pos-alocador (BirFuncao.isel_otim): IselOtimTentar consome 1 ou mais instrucoes, 0 = a selecao comum assume, -1 = falhou. */
static bool GerarBloco(X64Ctx *c, const BirBloco *bloco)
{
    for (uint32_t k = 0; k < bloco->n; k++) {
        int32_t consumidas = 0;

        if (c->dbg != NULL)
            DbgLinhaVista(c->dbg, X64Offset(c), bloco->instrs[k].pos);
        if (c->f->isel_otim)
            consumidas = IselOtimTentar(c, bloco, k);
        if (consumidas < 0)
            return false;
        if (consumidas > 0) {
            k += (uint32_t)consumidas - 1u;
            continue;
        }
        if (!X64Selecionar(c, &bloco->instrs[k]))
            return false;
    }
    return true;
}

static bool GerarCorpo(X64Ctx *c)
{
    for (c->pos_ordem = 0; c->pos_ordem < c->nordem; c->pos_ordem++) {
        uint32_t bloco = c->ordem[c->pos_ordem];

        c->bloco_off[bloco] = X64Offset(c);
        c->instr = NULL;
        if (bloco == c->f->nblocos) {
            c->origem = ORIGEM_EPILOGO;
            if (!QuadroEpilogo(c))
                return false;
        } else {
            c->origem = c->f->blocos[bloco].frio ? ORIGEM_FRIO : ORIGEM_CODIGO;
            if (!GerarBloco(c, &c->f->blocos[bloco]))
                return false;
        }
    }
    return true;
}

static bool GerarCodigo(X64Ctx *c)
{
    c->instr = NULL;
    c->origem = ORIGEM_PROLOGO;
    if (!AlocClassesSuportadas(c))
        return false;
    Fatiar(c, FATIA_REGS);
    if (!QuadroPrologo(c) || !GerarCorpo(c))
        return false;
    Fatiar(c, FATIA_ISEL);
    X64CorrigirSaltos(c);
    X64UnwindRegistrar(c);
    if (c->dbg != NULL)
        DbgProcFechar(c->dbg, X64Offset(c));
    X64Padding(c);
    return true;
}

/* Arrays de apoio da funcao (ordem, inicios de bloco, argumentos de chamada) numa arena que morre com ela. */
static bool GerarFuncao(X64Ctx *c, const BirFuncao *f)
{
    Arena arena = {0};
    uint32_t *slot_off;
    bool ok;

    c->f = f;
    c->usos = c->defs = NULL;
    if (f->isel_otim) {
        uint8_t *usos = ArenaAlocar(&arena, f->nvregs + 1u), *defs = ArenaAlocar(&arena, f->nvregs + 1u);

        IselOtimContarUsos(f, usos, defs);
        c->usos = usos;
        c->defs = defs;
    }
    c->quadro = QuadroCalcular(c->p, f);
    slot_off = ArenaAlocar(&arena, sizeof(uint32_t) * (f->nslots + 1u));
    QuadroPosicionarSlots(f, slot_off);
    c->slot_off = slot_off;
    c->nordem = f->nblocos + 1;
    c->ordem = ArenaAlocar(&arena, sizeof(uint32_t) * c->nordem);
    c->bloco_off = ArenaAlocar(&arena, sizeof(uint32_t) * (c->nordem + 1u)); /* +1: laco da sondagem */
    c->cls = ArenaAlocar(&arena, sizeof(AbiArg) * (c->quadro.maxargs + 1u));
    c->abi = ArenaAlocar(&arena, sizeof(AbiLocal) * (c->quadro.maxargs + 1u));
    QuadroOrdenar(f, c->ordem);
    VecIniciar(&c->saltos, sizeof(SaltoLocal));
    c->inicio = X64Offset(c);
    c->func_sim = ObjSimboloDefinir(c->o, c->p->simbolos[f->simbolo].nome, (int32_t)c->texto, c->inicio);
    if (c->dbg != NULL) {
        DbgAbertura abertura = {c->p->simbolos[f->simbolo].nome, c->inicio, c->quadro.tam};

        DbgProcAbrir(c->dbg, f, &abertura);
        X64DbgLocais(c);
    }
    if (c->lista != NULL)
        X64ListarFuncao(c);
    ok = GerarCodigo(c);
    VecLiberar(&c->saltos);
    ArenaLiberar(&arena);
    Fatiar(c, FATIA_CODIFICAR);
    return ok;
}

/* O maior alinhamento pedido pelos dados (BS_DADO): o da secao .rdata do programa; sem dados fica 1, como no M1. */
static uint32_t AlinhamentoDoRdata(const BirPrograma *p)
{
    uint32_t maior = 1;

    for (uint32_t k = 0; k < p->nsimbolos; k++) {
        if (p->simbolos[k].tipo == BS_DADO && p->simbolos[k].alinhamento > maior)
            maior = p->simbolos[k].alinhamento;
    }
    return maior;
}

/* Acrescenta o simbolo local (".s<ordem>" texto com NUL, ".d<ordem>" dado alinhado, com zeros de preenchimento) em .rdata e devolve o do Objeto. */
static uint32_t DefinirEmRdata(Objeto *o, uint32_t rdata, const BirSimbolo *s)
{
    ObjSecao *dados = VecEm(&o->secoes, rdata);
    char nome[GERAR_NOME_TEXTO];
    uint32_t off;

    if (s->tipo == BS_DADO) {
        while (dados->bytes.tam % s->alinhamento != 0)
            BufAnexarN(&dados->bytes, "", 1);
    }
    off = (uint32_t)dados->bytes.tam;
    snprintf(nome, sizeof nome, s->tipo == BS_DADO ? ".d%u" : ".s%u", (unsigned)s->ordem);
    BufAnexarN(&dados->bytes, s->bytes, (size_t)s->tam + (s->tipo == BS_TEXTO ? 1u : 0u));
    return ObjSimboloDefinir(o, nome, (int32_t)rdata, off);
}

/* Textos (com NUL) e dados em .rdata; funcoes e runtime viram referencia (as funcoes se definem em GerarFuncao). */
static void DefinirSimbolos(const BirPrograma *p, Objeto *o, uint32_t *obj_sim)
{
    uint32_t rdata = 0;
    bool tem_rdata = false;

    for (uint32_t k = 0; k < p->nsimbolos; k++) {
        const BirSimbolo *s = &p->simbolos[k];

        if (s->tipo != BS_TEXTO && s->tipo != BS_DADO) {
            obj_sim[k] = ObjSimboloReferir(o, s->nome);
            continue;
        }
        if (!tem_rdata)
            rdata = ObjSecaoAbrir(o, ".rdata", OS_RDATA, AlinhamentoDoRdata(p));
        tem_rdata = true;
        obj_sim[k] = DefinirEmRdata(o, rdata, s);
    }
}

/* Listagem e tempos pedidos (colheita NULL = nenhum dos dois); a primeira marca do relogio vale desde aqui. */
static void IniciarColheita(X64Ctx *c, const AlvoColheita *colheita)
{
    if (colheita == NULL)
        return;
    c->lista = colheita->lista;
    c->tempos = colheita->tempos;
    c->dbg = colheita->dbg;
    c->rastreio = colheita->rastreio && colheita->dbg != NULL; /* a FRT1 le do DbgPrograma */
    c->guarda = colheita->guarda;
    if (c->tempos != NULL)
        c->marca = RelogioNs();
}

bool X64Gerar(const BirPrograma *programa, Objeto *o, Diag *erro, const AlvoColheita *colheita)
{
    Arena arena = {0};
    X64Ctx c = {.p = programa, .o = o, .erro = erro};
    uint32_t *obj_sim = ArenaAlocar(&arena, sizeof(uint32_t) * (programa->nsimbolos + 1u));
    bool ok = true;

    IniciarColheita(&c, colheita);
    c.texto = ObjSecaoAbrir(o, ".text", OS_TEXTO, X64_ALINHAMENTO_FUNCAO);
    c.pdata = ObjSecaoAbrir(o, ".pdata", OS_PDATA, X64_ALINHAMENTO_UNWIND);
    c.xdata = ObjSecaoAbrir(o, ".xdata", OS_XDATA, X64_ALINHAMENTO_UNWIND);
    c.sim_xdata = ObjSimboloDefinir(o, ".unwind", (int32_t)c.xdata, 0);
    DefinirSimbolos(programa, o, obj_sim);
    c.obj_sim = obj_sim;
    Fatiar(&c, FATIA_CODIFICAR); /* secoes e simbolos do Objeto: parte da montagem, nao do preparo da primeira funcao */
    for (uint32_t k = 0; ok && k < programa->nfuncoes; k++)
        ok = GerarFuncao(&c, &programa->funcoes[k]);
    if (ok && programa->nentradas_sysv != 0)
        ok = X64GerarEntradasSysv(&c);
    if (ok && c.rastreio)
        X64RastroFechar(&c);
    ArenaLiberar(&arena);
    return ok;
}
