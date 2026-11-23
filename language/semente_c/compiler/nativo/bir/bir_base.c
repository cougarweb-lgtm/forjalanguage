/* bir_base.c - primitivas da construcao da BIR: blocos, instrucoes, vregs, rotulos, lacos e a tabela de simbolos (texto deduplicado).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M3 P19: BirSimboloImport (BS_IMPORT: "__imp_<simbolo>" deduplicado, pos do 1o uso).
 *            0.3.0 - M2 P11a: BirNovoSlot (slots de memoria da funcao).
 *            0.2.0 - M2 P10b: BirSimboloDado (BS_DADO: bytes de .rdata com alinhamento, deduplicados por conteudo).
 *            0.1.0 - backend x64 P6: primeira versao.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj>
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define HASH_INICIAL 64u
#define BLOCO_INSTRS_INICIAL 8u
#define BLOCOS_INICIAL 8u
#define VREGS_INICIAL 16u
#define SIMBOLOS_INICIAL 32u
#define LACOS_INICIAL 8u
#define IMPORT_PREFIXO "__imp_"
#define IMPORT_TAM_PREFIXO 6u

static void *Realocar(void *antigo, size_t bytes)
{
    void *novo = realloc(antigo, bytes);

    if (novo == NULL)
        FalhaInterna("bir: memoria esgotada (%zu bytes)", bytes);
    return novo;
}

uint32_t BirNovoBloco(BirCtx *c, bool frio)
{
    BirFuncao *f = c->f;

    if (f->nblocos == f->cap_blocos) {
        f->cap_blocos = f->cap_blocos == 0 ? BLOCOS_INICIAL : f->cap_blocos * 2;
        f->blocos = Realocar(f->blocos, f->cap_blocos * sizeof *f->blocos);
    }
    f->blocos[f->nblocos] = (BirBloco){.frio = frio};
    return f->nblocos++;
}

uint32_t BirNovoVreg(BirCtx *c, uint8_t classe)
{
    BirFuncao *f = c->f;

    if (f->nvregs == c->cap_vregs) {
        c->cap_vregs = c->cap_vregs == 0 ? VREGS_INICIAL : c->cap_vregs * 2;
        f->classe_vreg = Realocar(f->classe_vreg, c->cap_vregs);
    }
    f->classe_vreg[f->nvregs] = classe;
    return f->nvregs++;
}

uint32_t BirNovoSlot(BirCtx *c, uint32_t tam, uint32_t alinhamento)
{
    BirFuncao *f = c->f;

    if (f->nslots == f->cap_slots) {
        f->cap_slots = f->cap_slots == 0 ? BLOCOS_INICIAL : f->cap_slots * 2;
        f->slots = Realocar(f->slots, f->cap_slots * sizeof *f->slots);
    }
    f->slots[f->nslots] = (BirSlot){.tam = tam, .alinhamento = alinhamento};
    return f->nslots++;
}

/* Instrucao zerada no fim do bloco corrente (um bloco novo se o anterior ja terminou). O ponteiro vale ate a proxima emissao. */
BirInstr *BirEmitir(BirCtx *c, BirOp op, uint8_t classe)
{
    BirBloco *b;
    BirInstr *i;

    if (c->cur == BIR_SEM_BLOCO)
        c->cur = BirNovoBloco(c, false);
    b = &c->f->blocos[c->cur];
    if (b->n == b->cap) {
        b->cap = b->cap == 0 ? BLOCO_INSTRS_INICIAL : b->cap * 2;
        b->instrs = Realocar(b->instrs, b->cap * sizeof *b->instrs);
    }
    i = &b->instrs[b->n++];
    *i = (BirInstr){.op = (uint8_t)op, .classe = classe, .dst = BIR_SEM_VREG, .simbolo = BIR_SEM_SIMBOLO,
                    .alvo = {BIR_SEM_BLOCO, BIR_SEM_BLOCO}, .pos = c->atual != NULL ? c->atual->pos : BIR_SEM_POS, .ir = c->atual};
    return i;
}

BirInstr *BirEmitirPedido(BirCtx *c, BirPedido pedido)
{
    BirInstr *i = BirEmitir(c, (BirOp)pedido.op, c->f->classe_vreg[pedido.dst]);

    i->aux = pedido.aux;
    i->dst = pedido.dst;
    i->a = pedido.a;
    i->b = pedido.b;
    return i;
}

uint32_t BirContaNova(BirCtx *c, uint8_t classe, BirPedido pedido)
{
    pedido.dst = BirNovoVreg(c, classe);
    BirEmitirPedido(c, pedido);
    return pedido.dst;
}

void BirMover(BirCtx *c, uint32_t dst, BirOperando origem)
{
    BirEmitirPedido(c, (BirPedido){.op = origem.tipo == BO_IMED ? BIR_CONST : BIR_COPIA, .dst = dst, .a = origem});
}

void BirSalta(BirCtx *c, uint32_t alvo)
{
    BirInstr *i = BirEmitir(c, BIR_SALTA, BC_NADA);

    i->alvo[0] = alvo;
    c->cur = BIR_SEM_BLOCO;
}

void BirDesvia(BirCtx *c, BirOperando cond, uint32_t verdade, uint32_t falso)
{
    BirInstr *i = BirEmitir(c, BIR_DESVIA, BC_NADA);

    i->a = cond;
    i->alvo[0] = verdade;
    i->alvo[1] = falso;
    c->cur = BIR_SEM_BLOCO;
}

void BirRetorna(BirCtx *c, const BirOperando *valor)
{
    BirInstr *i = BirEmitir(c, BIR_RETORNA, valor != NULL ? valor->classe : BC_NADA);

    if (valor != NULL)
        i->a = *valor;
    c->cur = BIR_SEM_BLOCO;
}

BirOperando *BirArgs(BirCtx *c, uint32_t n)
{
    return n == 0 ? NULL : ArenaAlocar(&c->p->arena, n * sizeof(BirOperando));
}

void BirArmadilha(BirCtx *c, uint32_t simbolo, const BirOperando *args, uint32_t nargs)
{
    BirOperando *copia = BirArgs(c, nargs);
    BirInstr *i;

    if (nargs > 0)
        memcpy(copia, args, nargs * sizeof *copia);
    i = BirEmitir(c, BIR_ARMADILHA, BC_NADA);
    i->simbolo = simbolo;
    i->args = copia;
    i->nargs = nargs;
    c->cur = BIR_SEM_BLOCO;
}

uint32_t BirRotuloAlvo(BirCtx *c, BirRotulo *r)
{
    if (r->bloco == BIR_SEM_BLOCO)
        r->bloco = BirNovoBloco(c, false);
    return r->bloco;
}

void BirRotuloSalta(BirCtx *c, BirRotulo *r)
{
    if (c->cur != BIR_SEM_BLOCO)
        BirSalta(c, BirRotuloAlvo(c, r));
}

void BirRotuloColoca(BirCtx *c, BirRotulo *r)
{
    c->cur = r->bloco;
}

void BirEntrarLaco(BirCtx *c, BirRotulo *continua, BirRotulo *pare)
{
    if (c->nlacos == c->cap_lacos) {
        c->cap_lacos = c->cap_lacos == 0 ? LACOS_INICIAL : c->cap_lacos * 2;
        c->lacos = Realocar(c->lacos, c->cap_lacos * sizeof *c->lacos);
    }
    c->lacos[c->nlacos++] = (BirLaco){continua, pare};
}

void BirSairLaco(BirCtx *c)
{
    c->nlacos--;
}

/* ---- simbolos ---- */

static uint32_t HashSimbolo(uint8_t tipo, const char *bytes, uint32_t tam)
{
    uint32_t h = 2166136261u ^ tipo;

    for (uint32_t k = 0; k < tam; k++)
        h = (h ^ (uint8_t)bytes[k]) * 16777619u;
    return h;
}

static const char *ChaveDoSimbolo(const BirSimbolo *s)
{
    return s->tipo == BS_TEXTO || s->tipo == BS_DADO ? s->bytes : s->nome;
}

static bool SimboloIgual(const BirSimbolo *s, uint8_t tipo, const char *bytes, uint32_t tam)
{
    return s->tipo == tipo && s->tam == tam && memcmp(ChaveDoSimbolo(s), bytes, tam) == 0;
}

static void InserirNoHash(BirCtx *c, uint32_t indice)
{
    const BirSimbolo *s = &c->p->simbolos[indice];
    uint32_t pos = HashSimbolo(s->tipo, ChaveDoSimbolo(s), s->tam) & (c->cap_hash - 1);

    while (c->hash[pos] != 0)
        pos = (pos + 1) & (c->cap_hash - 1);
    c->hash[pos] = indice + 1;
}

static void CrescerHash(BirCtx *c)
{
    free(c->hash);
    c->cap_hash = c->cap_hash == 0 ? HASH_INICIAL : c->cap_hash * 2;
    c->hash = calloc(c->cap_hash, sizeof *c->hash);
    if (c->hash == NULL)
        FalhaInterna("bir: memoria esgotada na tabela de simbolos");
    for (uint32_t k = 0; k < c->p->nsimbolos; k++)
        InserirNoHash(c, k);
}

static uint32_t AcharSimbolo(const BirCtx *c, uint8_t tipo, const char *bytes, uint32_t tam)
{
    uint32_t pos;

    if (c->cap_hash == 0)
        return BIR_SEM_SIMBOLO;
    pos = HashSimbolo(tipo, bytes, tam) & (c->cap_hash - 1);
    for (; c->hash[pos] != 0; pos = (pos + 1) & (c->cap_hash - 1)) {
        if (SimboloIgual(&c->p->simbolos[c->hash[pos] - 1], tipo, bytes, tam))
            return c->hash[pos] - 1;
    }
    return BIR_SEM_SIMBOLO;
}

static uint32_t NovoSimbolo(BirCtx *c, BirSimbolo novo)
{
    BirPrograma *p = c->p;

    if (p->nsimbolos == p->cap_simbolos) {
        p->cap_simbolos = p->cap_simbolos == 0 ? SIMBOLOS_INICIAL : p->cap_simbolos * 2;
        p->simbolos = Realocar(p->simbolos, p->cap_simbolos * sizeof *p->simbolos);
    }
    p->simbolos[p->nsimbolos] = novo;
    if ((c->n_hash + 1) * 2 > c->cap_hash)
        CrescerHash(c);
    InserirNoHash(c, p->nsimbolos);
    c->n_hash++;
    return p->nsimbolos++;
}

/* Funcao do programa: "fj_<nome>" copiado na arena (o nome internado nao leva o prefixo). */
uint32_t BirSimboloFuncao(BirCtx *c, const char *nome_sem_prefixo)
{
    size_t n = strlen(nome_sem_prefixo);
    char *nome = ArenaAlocar(&c->p->arena, n + 4);

    memcpy(nome, "fj_", 3);
    memcpy(nome + 3, nome_sem_prefixo, n + 1);
    return NovoSimbolo(c, (BirSimbolo){.nome = nome, .tipo = BS_FUNCAO, .tam = (uint32_t)(n + 3)});
}

/* forja_programa: funcao do programa cujo simbolo nao leva o prefixo fj_ (o nome e literal do codigo). */
uint32_t BirSimboloGlobal(BirCtx *c, const char *nome)
{
    return NovoSimbolo(c, (BirSimbolo){.nome = nome, .tipo = BS_FUNCAO, .tam = (uint32_t)strlen(nome)});
}

/* Funcao do runtime nativo (rt_win.o): o nome e literal do codigo, sem copia. */
uint32_t BirSimboloRuntime(BirCtx *c, const char *nome)
{
    uint32_t tam = (uint32_t)strlen(nome);
    uint32_t achado = AcharSimbolo(c, BS_RUNTIME, nome, tam);

    if (achado != BIR_SEM_SIMBOLO)
        return achado;
    return NovoSimbolo(c, (BirSimbolo){.nome = nome, .tipo = BS_RUNTIME, .tam = tam});
}

/* Slot da IAT de um foreign de DLL: "__imp_<simbolo>" copiado na arena, um por simbolo; pos fica com o 1o uso (o X0101 aponta a linha dele). */
uint32_t BirSimboloImport(BirCtx *c, const char *simbolo, uint32_t pos)
{
    size_t n = strlen(simbolo) + IMPORT_TAM_PREFIXO;
    char *nome = ArenaAlocar(&c->p->arena, n + 1);
    uint32_t achado;

    memcpy(nome, IMPORT_PREFIXO, IMPORT_TAM_PREFIXO);
    memcpy(nome + IMPORT_TAM_PREFIXO, simbolo, n - IMPORT_TAM_PREFIXO + 1);
    achado = AcharSimbolo(c, BS_IMPORT, nome, (uint32_t)n);
    if (achado != BIR_SEM_SIMBOLO)
        return achado;
    return NovoSimbolo(c, (BirSimbolo){.nome = nome, .tipo = BS_IMPORT, .tam = (uint32_t)n, .pos = pos});
}

/* Texto (literal de print, nome de operacao, arquivo): uma copia por conteudo; o numero sN segue a ordem do 1o uso. */
uint32_t BirSimboloTexto(BirCtx *c, const char *bytes, uint32_t tam)
{
    uint32_t achado = AcharSimbolo(c, BS_TEXTO, bytes, tam);

    if (achado != BIR_SEM_SIMBOLO)
        return achado;
    return NovoSimbolo(c, (BirSimbolo){.bytes = ArenaCopiarTexto(&c->p->arena, bytes, tam), .tipo = BS_TEXTO, .tam = tam,
                                       .ordem = c->n_texto++});
}

/* Dado de .rdata (mascara de sinal do NEG real): uma copia por conteudo; o numero dN segue a ordem do 1o uso. */
uint32_t BirSimboloDado(BirCtx *c, const char *bytes, uint32_t tam, uint32_t alinhamento)
{
    uint32_t achado = AcharSimbolo(c, BS_DADO, bytes, tam);

    if (achado == BIR_SEM_SIMBOLO)
        return NovoSimbolo(c, (BirSimbolo){.bytes = ArenaCopiarTexto(&c->p->arena, bytes, tam), .tipo = BS_DADO, .tam = tam,
                                           .ordem = c->n_dado++, .alinhamento = alinhamento});
    if (alinhamento > c->p->simbolos[achado].alinhamento)
        c->p->simbolos[achado].alinhamento = alinhamento;
    return achado;
}
