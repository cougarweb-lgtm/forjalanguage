/* teste_vetor.c - verificacoes da classe BC_V128 (S1b do SIMD) em BIR sintetica: validador (aceita COPIA/CARREGA/GRAVA com forma, recusa o resto), impressao da forma,
 * alocador de registradores (xmm2..15, despejo sob pressao) e home de 16 B numa regiao propria. Chamada pelo teste_nativo.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - S2 (D0703): SPLAT e ADD SUB MUL DIV de V128 (validador, impressao, recusas) e testes de EXECUCAO: BIR sintetica -> exe (GerarExe de gerar_x64.c) -> codigo de saida; vetor x escalar
 *                    bit a bit (NaN, -0, denormal, inf; f32 e f64), pressao (18 vetores, despejo) e CHAMA no meio, nos modos degrau 0 e alocador + isel; controle negativo.
 *            0.1.0 - S1b (D0703): primeira versao.
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/bir/bir.h"
#include "nativo/x64/x64_interno.h"
#include "nativo/x64/x64_isel_otim.h"
#include "nativo/x64/x64_regalloc.h"

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (vetor) linha %d: %s\n", linha, texto);
}

#define VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

#define NVETORES 16u /* mais que os 14 xmm do banco (xmm2..15) */
#define USOS_POR_VETOR 3u /* def + 3 usos: peso 4, o minimo para um callee-saved (xmm6..15) valer o movups */

static BirInstr InstrBase(uint8_t op, uint8_t classe)
{
    BirInstr i;

    memset(&i, 0, sizeof i);
    i.op = op;
    i.classe = classe;
    i.dst = BIR_SEM_VREG;
    i.pos = BIR_SEM_POS;
    i.simbolo = BIR_SEM_SIMBOLO;
    i.alvo[0] = BIR_SEM_BLOCO;
    i.alvo[1] = BIR_SEM_BLOCO;
    return i;
}

static BirOperando Vreg(uint32_t id, uint8_t classe)
{
    BirOperando o;

    memset(&o, 0, sizeof o);
    o.tipo = BO_VREG;
    o.classe = classe;
    o.id = id;
    return o;
}

static void Acrescentar(BirFuncao *f, BirInstr instr)
{
    BirBloco *b = &f->blocos[0];

    if (b->n == b->cap) {
        b->cap = b->cap == 0 ? 8 : b->cap * 2;
        b->instrs = realloc(b->instrs, b->cap * sizeof *b->instrs);
    }
    b->instrs[b->n++] = instr;
}

/* Programa de 1 funcao (bloco unico) e 2 simbolos (a funcao e um runtime para a CHAMA); vreg 0 = I64 (endereco do slot 0), o resto BC_V128 salvo pedido em contrario. */
static void NovoPrograma(BirPrograma *p, uint32_t nvregs)
{
    BirFuncao *f;

    memset(p, 0, sizeof *p);
    p->simbolos = calloc(2, sizeof *p->simbolos);
    p->nsimbolos = 2;
    p->simbolos[0] = (BirSimbolo){.nome = "vetor_teste", .tipo = BS_FUNCAO};
    p->simbolos[1] = (BirSimbolo){.nome = "externa", .tipo = BS_RUNTIME};
    p->funcoes = calloc(1, sizeof *p->funcoes);
    p->nfuncoes = 1;
    f = &p->funcoes[0];
    f->nvregs = nvregs;
    f->retorno = BC_NADA;
    f->classe_vreg = calloc(nvregs + 1u, 1);
    for (uint32_t k = 1; k < nvregs; k++)
        f->classe_vreg[k] = BC_V128;
    f->classe_vreg[0] = BC_I64;
    f->nblocos = 1;
    f->blocos = calloc(1, sizeof *f->blocos);
    f->slots = calloc(1, sizeof *f->slots);
    f->nslots = 1;
    f->slots[0] = (BirSlot){.tam = 16u * (NVETORES + 1u), .alinhamento = 16};
}

static void LiberarPrograma(BirPrograma *p)
{
    BirFuncao *f = &p->funcoes[0];

    free(f->blocos[0].instrs);
    free(f->blocos);
    free(f->classe_vreg);
    free(f->slots);
    free(f->reg_vreg);
    free(p->funcoes);
    free(p->simbolos);
}

static void EndSlot(BirFuncao *f)
{
    BirInstr i = InstrBase(BIR_END_SLOT, BC_I64);

    i.dst = 0;
    i.a = (BirOperando){.tipo = BO_IMED, .classe = BC_I64, .imed = 0};
    Acrescentar(f, i);
}

static void Carrega(BirFuncao *f, uint32_t dst, int32_t desloc, uint8_t forma)
{
    BirInstr i = InstrBase(BIR_CARREGA, BC_V128);

    i.dst = dst;
    i.a = Vreg(0, BC_I64);
    i.desloc = desloc;
    i.aux = forma;
    Acrescentar(f, i);
}

static void Grava(BirFuncao *f, uint32_t valor, int32_t desloc, uint8_t forma)
{
    BirInstr i = InstrBase(BIR_GRAVA, BC_V128);

    i.a = Vreg(0, BC_I64);
    i.b = Vreg(valor, BC_V128);
    i.desloc = desloc;
    i.aux = forma;
    Acrescentar(f, i);
}

static void Copia(BirFuncao *f, uint32_t dst, uint32_t origem, uint8_t forma)
{
    BirInstr i = InstrBase(BIR_COPIA, BC_V128);

    i.dst = dst;
    i.a = Vreg(origem, BC_V128);
    i.aux = forma;
    Acrescentar(f, i);
}

static void Retorna(BirFuncao *f)
{
    Acrescentar(f, InstrBase(BIR_RETORNA, BC_NADA));
}

/* ---- validador e impressao ---- */

/* v0 = end_slot; v1 = carrega; v2 = copia v1; grava v2. Devolve o programa pronto (sem o RETORNA: cada caso o acrescenta). */
static void MontarValido(BirPrograma *p)
{
    BirFuncao *f;

    NovoPrograma(p, 3);
    f = &p->funcoes[0];
    EndSlot(f);
    Carrega(f, 1, 0, BVEC_4XF32);
    Copia(f, 2, 1, BVEC_2XI64);
    Grava(f, 2, 16, BVEC_2XI64);
}

static void TestaValidoEImpressao(const Contagem *c)
{
    BirPrograma p;
    Diag erro = {0};
    Buf dump = {0};

    MontarValido(&p);
    Retorna(&p.funcoes[0]);
    VERIFICA(c, BirValidar(&p, &erro));
    BirImprimir(&p, &dump);
    VERIFICA(c, dump.dados != NULL && strstr(dump.dados, "v1:v128 = carrega.4xf32") != NULL);
    VERIFICA(c, strstr(dump.dados, "v2:v128 = copia.2xi64") != NULL && strstr(dump.dados, "grava.2xi64") != NULL);
    BufLiberar(&dump);
    LiberarPrograma(&p);
}

static bool Recusa(BirPrograma *p, const char *trecho_da_causa)
{
    Diag erro = {0};

    return !BirValidar(p, &erro) && erro.codigo == X0904 && strstr(erro.causa, trecho_da_causa) != NULL;
}

static void TestaRecusas(const Contagem *c)
{
    BirPrograma p;
    BirFuncao *f;
    BirInstr soma;

    MontarValido(&p); /* ADD de V128 inteiro (4XI32): o S2 so libera as formas de real */
    f = &p.funcoes[0];
    soma = InstrBase(BIR_ADD, BC_V128);
    soma.aux = BVEC_4XI32;
    soma.dst = 2;
    soma.a = Vreg(1, BC_V128);
    soma.b = Vreg(1, BC_V128);
    Acrescentar(f, soma);
    Retorna(f);
    VERIFICA(c, Recusa(&p, "v128"));
    LiberarPrograma(&p);

    MontarValido(&p); /* forma fora da faixa */
    f = &p.funcoes[0];
    f->blocos[0].instrs[2].aux = BVEC_QTD;
    Retorna(f);
    VERIFICA(c, Recusa(&p, "v128"));
    LiberarPrograma(&p);

    MontarValido(&p); /* argumento V128 de chamada */
    f = &p.funcoes[0];
    soma = InstrBase(BIR_CHAMA, BC_NADA);
    soma.simbolo = 1;
    soma.nargs = 1;
    soma.args = calloc(1, sizeof *soma.args);
    soma.args[0] = Vreg(1, BC_V128);
    Acrescentar(f, soma);
    Retorna(f);
    VERIFICA(c, Recusa(&p, "v128"));
    free(soma.args);
    LiberarPrograma(&p);

    MontarValido(&p); /* retorno V128 */
    f = &p.funcoes[0];
    f->retorno = BC_V128;
    Retorna(f);
    VERIFICA(c, Recusa(&p, "retorno v128"));
    LiberarPrograma(&p);

    MontarValido(&p); /* parametro V128 */
    f = &p.funcoes[0];
    f->nparams = 2;
    Retorna(f);
    VERIFICA(c, Recusa(&p, "parametro v128"));
    LiberarPrograma(&p);
}

/* ---- S2: SPLAT e ADD SUB MUL DIV de V128 (validador e impressao) ---- */

static void Splat(BirFuncao *f, uint32_t dst, uint32_t origem, uint8_t forma)
{
    BirInstr i = InstrBase(BIR_SPLAT, BC_V128);

    i.dst = dst;
    i.a = Vreg(origem, f->classe_vreg[origem]);
    i.aux = forma;
    Acrescentar(f, i);
}

static void ContaVetorial(BirFuncao *f, uint8_t op, uint32_t dst, uint32_t a, uint32_t b, uint8_t forma)
{
    BirInstr i = InstrBase(op, BC_V128);

    i.dst = dst;
    i.a = Vreg(a, BC_V128);
    i.b = Vreg(b, BC_V128);
    i.aux = forma;
    Acrescentar(f, i);
}

/* v0 = end_slot; v1 = carrega; v2 = f32 (escalar); v3 = splat v2; v4 = mul v1 v3; v5 = add v4 v1; v6 = f64; v7 = splat v6; v8 = div.2xf64 v7 v7. */
static void MontarContaVetorial(BirPrograma *p)
{
    BirFuncao *f;

    NovoPrograma(p, 9);
    f = &p->funcoes[0];
    f->classe_vreg[2] = BC_F32;
    f->classe_vreg[6] = BC_F64;
    EndSlot(f);
    Carrega(f, 1, 0, BVEC_4XF32);
    Splat(f, 3, 2, BVEC_4XF32);
    ContaVetorial(f, BIR_MUL, 4, 1, 3, BVEC_4XF32);
    ContaVetorial(f, BIR_ADD, 5, 4, 1, BVEC_4XF32);
    Splat(f, 7, 6, BVEC_2XF64);
    ContaVetorial(f, BIR_DIV, 8, 7, 7, BVEC_2XF64);
    Retorna(f);
}

static void TestaContaVetorialValida(const Contagem *c)
{
    BirPrograma p;
    Diag erro = {0};
    Buf dump = {0};

    MontarContaVetorial(&p);
    VERIFICA(c, BirValidar(&p, &erro));
    BirImprimir(&p, &dump);
    VERIFICA(c, dump.dados != NULL && strstr(dump.dados, "v3:v128 = splat.4xf32 v2") != NULL);
    VERIFICA(c, strstr(dump.dados, "v4:v128 = mul.4xf32 v1, v3") != NULL && strstr(dump.dados, "v5:v128 = add.4xf32 v4, v1") != NULL);
    VERIFICA(c, strstr(dump.dados, "v7:v128 = splat.2xf64 v6") != NULL && strstr(dump.dados, "v8:v128 = div.2xf64 v7, v7") != NULL);
    BufLiberar(&dump);
    LiberarPrograma(&p);
}

/* Cada defeito do SPLAT e da conta vetorial e recusado: origem de classe errada, forma inteira, DIV/SUB com operando escalar, REM de vetor. */
static void TestaContaVetorialRecusas(const Contagem *c)
{
    BirPrograma p;
    BirFuncao *f;

    MontarContaVetorial(&p); /* splat 4XF32 de um F64 */
    p.funcoes[0].blocos[0].instrs[2].a = Vreg(6, BC_F64);
    VERIFICA(c, Recusa(&p, "v128"));
    LiberarPrograma(&p);

    MontarContaVetorial(&p); /* splat com forma inteira */
    p.funcoes[0].blocos[0].instrs[2].aux = BVEC_4XI32;
    VERIFICA(c, Recusa(&p, "v128"));
    LiberarPrograma(&p);

    MontarContaVetorial(&p); /* mul com a forma inteira 2XI64 */
    p.funcoes[0].blocos[0].instrs[3].aux = BVEC_2XI64;
    VERIFICA(c, Recusa(&p, "v128"));
    LiberarPrograma(&p);

    MontarContaVetorial(&p); /* mul com operando b escalar */
    p.funcoes[0].blocos[0].instrs[3].b = Vreg(2, BC_F32);
    VERIFICA(c, Recusa(&p, "v128"));
    LiberarPrograma(&p);

    MontarContaVetorial(&p); /* rem de vetor nao existe */
    f = &p.funcoes[0];
    f->blocos[0].instrs[3].op = BIR_REM;
    VERIFICA(c, Recusa(&p, "v128"));
    LiberarPrograma(&p);
}

/* ---- alocador e home de 16 B ---- */

/* v0 = end_slot; v1..N = carrega (todos vivos juntos); cada um e gravado USOS_POR_VETOR vezes. */
static void MontarPressao(BirPrograma *p)
{
    BirFuncao *f;

    NovoPrograma(p, NVETORES + 1u);
    f = &p->funcoes[0];
    EndSlot(f);
    for (uint32_t k = 1; k <= NVETORES; k++)
        Carrega(f, k, (int32_t)(16u * (k - 1u)), BVEC_4XF32);
    for (uint32_t u = 0; u < USOS_POR_VETOR; u++) {
        for (uint32_t k = 1; k <= NVETORES; k++)
            Grava(f, k, (int32_t)(16u * (k - 1u)), BVEC_4XF32);
    }
    Retorna(f);
}

/* Contagem dos vetores com registrador (todos em 2..15 e distintos) e dos despejados; devolve false se algum registrador sair do banco ou repetir. */
static bool BancoXmmValido(const BirFuncao *f, uint32_t *em_reg, uint32_t *despejados)
{
    uint32_t usados = 0;

    *em_reg = 0;
    *despejados = 0;
    for (uint32_t v = 1; v <= NVETORES; v++) {
        uint8_t reg = f->reg_vreg[v];

        if (reg == BIR_SEM_REG) {
            (*despejados)++;
            continue;
        }
        if (reg < 2 || reg > 15 || ((usados >> reg) & 1u))
            return false;
        usados |= 1u << reg;
        (*em_reg)++;
    }
    return true;
}

/* Cada home despejado: 16 B inteiros depois dos slots de vreg, dentro do quadro, sem sobrepor outro home nem o slot do vreg escalar v0. */
static bool HomesSemSobreposicao(const BirFuncao *f)
{
    uint32_t escalares = X64_SLOT_BYTES * f->nvregs + AlocBytesEmpurrados(f);
    uint32_t quadro = AlocBytesSlots(f);

    for (uint32_t v = 1; v <= NVETORES; v++) {
        int32_t d = AlocOperandoDe(f, v, 128).desloc;

        if (d > -(int32_t)(escalares + X64_VETOR_BYTES) || d < -(int32_t)quadro)
            return false;
        for (uint32_t w = v + 1u; w <= NVETORES; w++) {
            int32_t e = AlocOperandoDe(f, w, 128).desloc;

            if (d - e < (int32_t)X64_VETOR_BYTES && e - d < (int32_t)X64_VETOR_BYTES)
                return false;
        }
    }
    return AlocOperandoDe(f, 0, 64).desloc >= -(int32_t)escalares;
}

static void TestaAlocador(const Contagem *c)
{
    BirPrograma p;
    Diag erro = {0};
    uint32_t em_reg, despejados;
    BirFuncao *f;

    MontarPressao(&p);
    f = &p.funcoes[0];
    VERIFICA(c, BirValidar(&p, &erro));
    X64AlocadorAplicar(&p, f);
    VERIFICA(c, f->reg_vreg != NULL);
    if (f->reg_vreg != NULL) {
        VERIFICA(c, BancoXmmValido(f, &em_reg, &despejados));
        VERIFICA(c, em_reg >= 1 && despejados >= NVETORES - 14u && em_reg + despejados == NVETORES);
        VERIFICA(c, f->regs_salvos_xmm != 0 && (f->regs_salvos_xmm & ~(0x3FFu << 6)) == 0);
        VERIFICA(c, HomesSemSobreposicao(f));
    }
    /* o quadro cresce 16 B por vetor sobre os slots de 8 B dos vregs */
    VERIFICA(c, AlocBytesSlots(f) == X64_SLOT_BYTES * f->nvregs + AlocBytesEmpurrados(f) + X64_VETOR_BYTES * NVETORES);
    LiberarPrograma(&p);
}

/* ---- S2: BIR sintetica -> exe -> execucao: o vetor tem de dar, lane a lane, os mesmos bits do escalar SSE ---- */

int GerarExe(const BirPrograma *programa, void *saida); /* tests\nativo\x64\gerar_x64.c */

#define EXEC_SLOT_BASE 0u /* vreg 0 = end_slot 0 em toda funcao sintetica */
#define EXEC_NVETORES_PRESSAO 18u /* mais que os 14 xmm do banco, com o R e o produto vivos */
#define EXEC_NVETORES_CHAMADA 12u

typedef struct { uint8_t forma; uint64_t x[4], y[4], s; } DadosVetoriais;

/* Bits de f32 (low 32) e f64: NaN com payload, -0, denormal, inf, extremos e valores comuns. */
static const DadosVetoriais DADOS_VETORIAIS[] = {
    {BVEC_4XF32, {0x7FC0ABCD, 0x80000000, 0x00000001, 0x7F800000}, {0x3FC00000, 0x80000000, 0x40400000, 0x7F800000}, 0x40000000},
    {BVEC_4XF32, {0x7149F2CA, 0xF149F2CA, 0x40400000, 0x7FC01111}, {0x7FC02222, 0x00000000, 0x80000000, 0x7FC03333}, 0x7FC04444},
    {BVEC_4XF32, {0x3F800000, 0x40490FDB, 0x00400000, 0xC0A00000}, {0x40000000, 0x3E99999A, 0x007FFFFF, 0x3F000000}, 0x3F333333},
    {BVEC_2XF64, {0x7FF8000000ABCDEFull, 0x8000000000000000ull}, {0x0000000000000001ull, 0x7FF0000000000000ull}, 0x4000000000000000ull},
    {BVEC_2XF64, {0x7E37E43C8800759Cull, 0x4008000000000000ull}, {0x7FF8000000001111ull, 0x8000000000000000ull}, 0x3FF8000000000000ull},
    {BVEC_2XF64, {0x3FF0000000000000ull, 0xC014000000000000ull}, {0x4000000000000000ull, 0x000FFFFFFFFFFFFFull}, 0x3FD3333333333333ull},
};

static uint32_t NovoVreg(BirFuncao *f, uint8_t classe)
{
    f->classe_vreg = realloc(f->classe_vreg, (size_t)f->nvregs + 2u);
    f->classe_vreg[f->nvregs] = classe;
    return f->nvregs++;
}

static void Const(BirFuncao *f, uint32_t dst, int64_t valor)
{
    BirInstr i = InstrBase(BIR_CONST, f->classe_vreg[dst]);

    i.dst = dst;
    i.a = (BirOperando){.tipo = BO_IMED, .classe = i.classe, .imed = valor};
    Acrescentar(f, i);
}

static void CarregaEsc(BirFuncao *f, uint32_t dst, int32_t desloc)
{
    BirInstr i = InstrBase(BIR_CARREGA, f->classe_vreg[dst]);

    i.dst = dst;
    i.a = Vreg(EXEC_SLOT_BASE, BC_I64);
    i.desloc = desloc;
    Acrescentar(f, i);
}

static void GravaEsc(BirFuncao *f, uint32_t valor, int32_t desloc)
{
    BirInstr i = InstrBase(BIR_GRAVA, f->classe_vreg[valor]);

    i.a = Vreg(EXEC_SLOT_BASE, BC_I64);
    i.b = Vreg(valor, f->classe_vreg[valor]);
    i.desloc = desloc;
    Acrescentar(f, i);
}

/* dst = a op b na classe do dst; aux = forma (vetor) ou 0. */
static void Conta(BirFuncao *f, uint8_t op, uint32_t dst, uint32_t a, uint32_t b, uint8_t aux)
{
    BirInstr i = InstrBase(op, f->classe_vreg[dst]);

    i.dst = dst;
    i.a = Vreg(a, f->classe_vreg[a]);
    i.b = Vreg(b, f->classe_vreg[b]);
    i.aux = aux;
    Acrescentar(f, i);
}

/* acc (I32) += (carga de a != carga de b), as duas cargas na classe inteira dada. */
static void SomaDiferenca(BirFuncao *f, uint32_t acc, uint8_t classe_inteira, int32_t desloc_a, int32_t desloc_b)
{
    uint32_t p = NovoVreg(f, classe_inteira), q = NovoVreg(f, classe_inteira), dif = NovoVreg(f, BC_I8), e = NovoVreg(f, BC_I32);
    BirInstr cmp = InstrBase(BIR_CMP, BC_I8), ext = InstrBase(BIR_EXT_Z, BC_I32);

    CarregaEsc(f, p, desloc_a);
    CarregaEsc(f, q, desloc_b);
    cmp.dst = dif;
    cmp.aux = BCOND_NE;
    cmp.a = Vreg(p, classe_inteira);
    cmp.b = Vreg(q, classe_inteira);
    Acrescentar(f, cmp);
    ext.dst = e;
    ext.a = Vreg(dif, BC_I8);
    Acrescentar(f, ext);
    Conta(f, BIR_ADD, acc, acc, e, 0);
}

static void RetornaVreg(BirFuncao *f, uint32_t v)
{
    BirInstr i = InstrBase(BIR_RETORNA, BC_NADA);

    i.a = Vreg(v, f->classe_vreg[v]);
    Acrescentar(f, i);
}

/* Programa sintetico de `nfuncoes` funcoes: [0] = forja_programa (retorna I32, o codigo de saida), [1] = fj_sujo; cada uma com o slot 0 de `bytes` e o vreg 0 = end_slot 0. */
static void NovoProgramaExec(BirPrograma *p, uint32_t nfuncoes, uint32_t bytes)
{
    memset(p, 0, sizeof *p);
    p->simbolos = calloc(2, sizeof *p->simbolos);
    p->nsimbolos = 2;
    p->simbolos[0] = (BirSimbolo){.nome = "forja_programa", .tipo = BS_FUNCAO, .tam = 14};
    p->simbolos[1] = (BirSimbolo){.nome = "fj_sujo", .tipo = BS_FUNCAO, .tam = 7};
    p->modo = BM_RELEASE;
    p->funcoes = calloc(nfuncoes, sizeof *p->funcoes);
    p->nfuncoes = nfuncoes;
    for (uint32_t k = 0; k < nfuncoes; k++) {
        BirFuncao *f = &p->funcoes[k];

        f->simbolo = k;
        f->retorno = k == 0 ? BC_I32 : BC_NADA;
        f->sintetica = k == 0;
        f->nblocos = 1;
        f->blocos = calloc(1, sizeof *f->blocos);
        f->slots = calloc(1, sizeof *f->slots);
        f->nslots = 1;
        f->slots[0] = (BirSlot){.tam = bytes, .alinhamento = 16};
        f->classe_vreg = calloc(1, 1);
        NovoVreg(f, BC_I64);
        EndSlot(f);
    }
}

static void LiberarProgramaExec(BirPrograma *p)
{
    for (uint32_t k = 0; k < p->nfuncoes; k++) {
        free(p->funcoes[k].blocos[0].instrs);
        free(p->funcoes[k].blocos);
        free(p->funcoes[k].classe_vreg);
        free(p->funcoes[k].slots);
        free(p->funcoes[k].reg_vreg);
    }
    free(p->funcoes);
    free(p->simbolos);
}

/* Valida, aloca (otimizado: alocador + isel pos-alocador; senao tudo no home), gera o exe em %TEMP% e o executa. Devolve o codigo de saida; -1 = BIR invalida, -2 = falha ao gerar. */
static int ExecutarPrograma(BirPrograma *p, bool otimizado, const char *rotulo)
{
    const char *pasta_temp = getenv("TEMP");
    char exe[600], comando[640];
    Diag erro = {0};
    int codigo;

    if (!BirValidar(p, &erro))
        return -1;
    for (uint32_t k = 0; otimizado && k < p->nfuncoes; k++) {
        X64AlocadorAplicar(p, &p->funcoes[k]);
        X64IselMarcar(p, &p->funcoes[k]);
    }
    snprintf(exe, sizeof exe, "%s\\forja_s2_vetor_%s_%d.exe", pasta_temp != NULL ? pasta_temp : ".", rotulo, otimizado ? 1 : 0);
    if (GerarExe(p, exe) != 0)
        return -2;
    snprintf(comando, sizeof comando, "\"%s\"", exe);
    codigo = system(comando);
    remove(exe);
    return codigo;
}

/* Quantos vregs V128 do programa receberam registrador do alocador (so no modo otimizado). */
static uint32_t VetoresEmRegistrador(const BirFuncao *f)
{
    uint32_t n = 0;

    for (uint32_t v = 0; f->reg_vreg != NULL && v < f->nvregs; v++)
        n += f->classe_vreg[v] == BC_V128 && f->reg_vreg[v] != BIR_SEM_REG;
    return n;
}

/* Um conjunto de dados: grava x e y na memoria, calcula (x * s + y - x) / y em vetor (splat de s) e em escalar, lane a lane; acc += lanes com bits diferentes. corrompe: o escalar
 * usa ADD no lugar do SUB (controle negativo: o resultado tem de divergir). Memoria: x em 0, y em 16, vetor em 32, escalar em 48. */
static void EmitirConjunto(BirFuncao *f, const DadosVetoriais *d, uint32_t acc, bool corrompe)
{
    bool f32 = d->forma == BVEC_4XF32;
    uint8_t ce = f32 ? BC_F32 : BC_F64;
    int32_t lado = f32 ? 4 : 8;
    uint32_t lanes = f32 ? 4u : 2u, s = NovoVreg(f, ce), v1 = NovoVreg(f, BC_V128), v2 = NovoVreg(f, BC_V128), v3 = NovoVreg(f, BC_V128), v4 = NovoVreg(f, BC_V128);
    uint32_t v5 = NovoVreg(f, BC_V128), v6 = NovoVreg(f, BC_V128), v7 = NovoVreg(f, BC_V128);

    for (uint32_t l = 0; l < lanes; l++) {
        uint32_t x = NovoVreg(f, ce), y = NovoVreg(f, ce);

        Const(f, x, (int64_t)d->x[l]);
        GravaEsc(f, x, lado * (int32_t)l);
        Const(f, y, (int64_t)d->y[l]);
        GravaEsc(f, y, 16 + lado * (int32_t)l);
    }
    Const(f, s, (int64_t)d->s);
    Carrega(f, v1, 0, d->forma);
    Carrega(f, v2, 16, d->forma);
    Splat(f, v3, s, d->forma);
    ContaVetorial(f, BIR_MUL, v4, v1, v3, d->forma);
    ContaVetorial(f, BIR_ADD, v5, v4, v2, d->forma);
    ContaVetorial(f, BIR_SUB, v6, v5, v1, d->forma);
    ContaVetorial(f, BIR_DIV, v7, v6, v2, d->forma);
    Grava(f, v7, 32, d->forma);
    for (uint32_t l = 0; l < lanes; l++) {
        uint32_t a = NovoVreg(f, ce), b = NovoVreg(f, ce), t = NovoVreg(f, ce), t2 = NovoVreg(f, ce), t3 = NovoVreg(f, ce), t4 = NovoVreg(f, ce);

        CarregaEsc(f, a, lado * (int32_t)l);
        CarregaEsc(f, b, 16 + lado * (int32_t)l);
        Conta(f, BIR_MUL, t, a, s, 0);
        Conta(f, BIR_ADD, t2, t, b, 0);
        Conta(f, corrompe ? BIR_ADD : BIR_SUB, t3, t2, a, 0);
        Conta(f, BIR_DIV, t4, t3, b, 0);
        GravaEsc(f, t4, 48 + lado * (int32_t)l);
        SomaDiferenca(f, acc, f32 ? BC_I32 : BC_I64, 32 + lado * (int32_t)l, 48 + lado * (int32_t)l);
    }
}

/* forja_programa devolve o numero de lanes diferentes entre o vetor e o escalar, nos conjuntos da forma pedida. */
static void MontarVetorContraEscalar(BirPrograma *p, uint8_t forma, bool corrompe)
{
    BirFuncao *f;
    uint32_t acc;

    NovoProgramaExec(p, 1, 64);
    f = &p->funcoes[0];
    acc = NovoVreg(f, BC_I32);
    Const(f, acc, 0);
    for (uint32_t k = 0; k < sizeof DADOS_VETORIAIS / sizeof DADOS_VETORIAIS[0]; k++) {
        if (DADOS_VETORIAIS[k].forma == forma)
            EmitirConjunto(f, &DADOS_VETORIAIS[k], acc, corrompe);
    }
    RetornaVreg(f, acc);
}

static void TestaVetorContraEscalar(const Contagem *c, uint8_t forma, bool corrompe, const char *rotulo)
{
    BirPrograma p;

    for (int otimizado = 0; otimizado <= 1; otimizado++) {
        MontarVetorContraEscalar(&p, forma, corrompe);
        int codigo = ExecutarPrograma(&p, otimizado != 0, rotulo);

        if (corrompe)
            VERIFICA(c, codigo > 0); /* o controle negativo tem de rodar e divergir */
        else
            VERIFICA(c, codigo == 0);
        if (otimizado && !corrompe)
            VERIFICA(c, VetoresEmRegistrador(&p.funcoes[0]) >= 1);
        LiberarProgramaExec(&p);
    }
}

/* Pressao: 18 vetores vivos (mais que os xmm livres) contra o mesmo calculo com cada vetor recarregado na hora do uso; os resultados tem de ser iguais bit a bit.
 * Memoria: dados em 16 k, saida "alta pressao" depois deles e saida "baixa pressao" depois dessa. */
static void EmitirPressao(BirFuncao *f, uint32_t acc)
{
    uint32_t alta = 16u * EXEC_NVETORES_PRESSAO, baixa = 2u * alta, v[EXEC_NVETORES_PRESSAO], r, r2;

    for (uint32_t k = 0; k < EXEC_NVETORES_PRESSAO; k++) {
        for (uint32_t l = 0; l < 4; l++) {
            uint32_t x = NovoVreg(f, BC_F32);

            Const(f, x, k == 3 && l == 1 ? 0x7FC0ABCD : (k == 5 && l == 2 ? 0x80000000 : 0x3F800000 + (int64_t)((k * 2654435761u + l * 40503u) >> 13 & 0x3FFFFF)));
            GravaEsc(f, x, (int32_t)(16u * k + 4u * l));
        }
    }
    for (uint32_t k = 0; k < EXEC_NVETORES_PRESSAO; k++) {
        v[k] = NovoVreg(f, BC_V128);
        Carrega(f, v[k], (int32_t)(16u * k), BVEC_4XF32);
    }
    r = NovoVreg(f, BC_V128);
    Copia(f, r, v[0], BVEC_4XF32);
    for (uint32_t k = 1; k < EXEC_NVETORES_PRESSAO; k++)
        ContaVetorial(f, BIR_ADD, r, r, v[k], BVEC_4XF32);
    for (uint32_t k = 0; k < EXEC_NVETORES_PRESSAO; k++) {
        uint32_t t = NovoVreg(f, BC_V128);

        ContaVetorial(f, BIR_MUL, t, v[k], r, BVEC_4XF32);
        Grava(f, t, (int32_t)(alta + 16u * k), BVEC_4XF32);
    }
    r2 = NovoVreg(f, BC_V128);
    Carrega(f, r2, 0, BVEC_4XF32);
    for (uint32_t k = 1; k < EXEC_NVETORES_PRESSAO; k++) {
        uint32_t u = NovoVreg(f, BC_V128);

        Carrega(f, u, (int32_t)(16u * k), BVEC_4XF32);
        ContaVetorial(f, BIR_ADD, r2, r2, u, BVEC_4XF32);
    }
    for (uint32_t k = 0; k < EXEC_NVETORES_PRESSAO; k++) {
        uint32_t u = NovoVreg(f, BC_V128), t = NovoVreg(f, BC_V128);

        Carrega(f, u, (int32_t)(16u * k), BVEC_4XF32);
        ContaVetorial(f, BIR_MUL, t, u, r2, BVEC_4XF32);
        Grava(f, t, (int32_t)(baixa + 16u * k), BVEC_4XF32);
        SomaDiferenca(f, acc, BC_I64, (int32_t)(alta + 16u * k), (int32_t)(baixa + 16u * k));
        SomaDiferenca(f, acc, BC_I64, (int32_t)(alta + 16u * k + 8u), (int32_t)(baixa + 16u * k + 8u));
    }
}

static void TestaPressao(const Contagem *c)
{
    BirPrograma p;

    for (int otimizado = 0; otimizado <= 1; otimizado++) {
        uint32_t acc;
        BirFuncao *f;

        NovoProgramaExec(&p, 1, 16u * 3u * EXEC_NVETORES_PRESSAO);
        f = &p.funcoes[0];
        acc = NovoVreg(f, BC_I32);
        Const(f, acc, 0);
        EmitirPressao(f, acc);
        RetornaVreg(f, acc);
        VERIFICA(c, ExecutarPrograma(&p, otimizado != 0, "pressao") == 0);
        if (otimizado) { /* houve despejo de verdade: ha vetor em registrador e ha vetor sem ele */
            uint32_t em_reg = VetoresEmRegistrador(f), sem_reg = 0;

            for (uint32_t v = 0; v < f->nvregs; v++)
                sem_reg += f->classe_vreg[v] == BC_V128 && f->reg_vreg[v] == BIR_SEM_REG;
            VERIFICA(c, em_reg >= 1 && sem_reg >= 1);
        }
        LiberarProgramaExec(&p);
    }
}

/* fj_sujo: 12 vetores vivos ao mesmo tempo (splat de constantes distintas), o que usa xmm2..15 (o prologo guarda e o epilogo restaura os xmm6..15). Nao devolve nada. */
static void MontarSujo(BirFuncao *f)
{
    uint32_t v[EXEC_NVETORES_CHAMADA], r = NovoVreg(f, BC_V128);

    for (uint32_t k = 0; k < EXEC_NVETORES_CHAMADA; k++) {
        uint32_t s = NovoVreg(f, BC_F32);

        v[k] = NovoVreg(f, BC_V128);
        Const(f, s, 0x40E00000 + (int64_t)k * 0x100000);
        Splat(f, v[k], s, BVEC_4XF32);
    }
    Copia(f, r, v[0], BVEC_4XF32);
    for (uint32_t k = 1; k < EXEC_NVETORES_CHAMADA; k++)
        ContaVetorial(f, BIR_MUL, r, r, v[k], BVEC_4XF32);
    for (uint32_t k = 0; k < EXEC_NVETORES_CHAMADA; k++) {
        uint32_t t = NovoVreg(f, BC_V128);

        ContaVetorial(f, BIR_ADD, t, v[k], r, BVEC_4XF32);
        Grava(f, t, (int32_t)(16u * k), BVEC_4XF32);
    }
    Acrescentar(f, InstrBase(BIR_RETORNA, BC_NADA));
}

/* Principal: va e vb vivos atravessam a CHAMA de fj_sujo; o resultado (va * vb + va) * vb depois dela tem de ser igual ao mesmo calculo com va e vb recarregados da memoria. */
static void MontarChamada(BirPrograma *p, uint32_t atravessam[3])
{
    BirFuncao *f;
    BirInstr chama = InstrBase(BIR_CHAMA, BC_NADA);
    uint32_t acc, va, vb, vp, vq, vr, wa, wb, wp, wq, wr;

    NovoProgramaExec(p, 2, 16u * EXEC_NVETORES_CHAMADA);
    MontarSujo(&p->funcoes[1]);
    f = &p->funcoes[0];
    f->slots[0].tam = 64;
    acc = NovoVreg(f, BC_I32);
    Const(f, acc, 0);
    for (uint32_t l = 0; l < 8; l++) {
        uint32_t x = NovoVreg(f, BC_F32);

        Const(f, x, l == 2 ? 0x7FC01234 : 0x3F800000 + (int64_t)l * 0x200000);
        GravaEsc(f, x, (int32_t)(4u * l));
    }
    va = NovoVreg(f, BC_V128);
    vb = NovoVreg(f, BC_V128);
    vp = NovoVreg(f, BC_V128);
    Carrega(f, va, 0, BVEC_4XF32);
    Carrega(f, vb, 16, BVEC_4XF32);
    ContaVetorial(f, BIR_MUL, vp, va, vb, BVEC_4XF32);
    atravessam[0] = va;
    atravessam[1] = vb;
    atravessam[2] = vp;
    chama.simbolo = 1;
    Acrescentar(f, chama);
    vq = NovoVreg(f, BC_V128);
    vr = NovoVreg(f, BC_V128);
    ContaVetorial(f, BIR_ADD, vq, vp, va, BVEC_4XF32);
    ContaVetorial(f, BIR_MUL, vr, vq, vb, BVEC_4XF32);
    Grava(f, vr, 32, BVEC_4XF32);
    wa = NovoVreg(f, BC_V128);
    wb = NovoVreg(f, BC_V128);
    wp = NovoVreg(f, BC_V128);
    wq = NovoVreg(f, BC_V128);
    wr = NovoVreg(f, BC_V128);
    Carrega(f, wa, 0, BVEC_4XF32);
    Carrega(f, wb, 16, BVEC_4XF32);
    ContaVetorial(f, BIR_MUL, wp, wa, wb, BVEC_4XF32);
    ContaVetorial(f, BIR_ADD, wq, wp, wa, BVEC_4XF32);
    ContaVetorial(f, BIR_MUL, wr, wq, wb, BVEC_4XF32);
    Grava(f, wr, 48, BVEC_4XF32);
    SomaDiferenca(f, acc, BC_I64, 32, 48);
    SomaDiferenca(f, acc, BC_I64, 40, 56);
    RetornaVreg(f, acc);
}

static void TestaChamada(const Contagem *c)
{
    BirPrograma p;

    for (int otimizado = 0; otimizado <= 1; otimizado++) {
        uint32_t atravessam[3];

        MontarChamada(&p, atravessam);
        VERIFICA(c, ExecutarPrograma(&p, otimizado != 0, "chamada") == 0);
        if (otimizado) { /* o vetor vivo atravessa a chamada em xmm6..15 (callee-saved) ou no home, nunca em xmm2..5 */
            const BirFuncao *f = &p.funcoes[0];
            bool ok = f->reg_vreg != NULL;

            for (uint32_t k = 0; ok && k < 3; k++)
                ok = f->reg_vreg[atravessam[k]] == BIR_SEM_REG || f->reg_vreg[atravessam[k]] >= 6;
            VERIFICA(c, ok);
        }
        LiberarProgramaExec(&p);
    }
}

void VetorVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    TestaValidoEImpressao(&c);
    TestaRecusas(&c);
    TestaContaVetorialValida(&c);
    TestaContaVetorialRecusas(&c);
    TestaAlocador(&c);
    TestaVetorContraEscalar(&c, BVEC_4XF32, false, "f32");
    TestaVetorContraEscalar(&c, BVEC_2XF64, false, "f64");
    TestaVetorContraEscalar(&c, BVEC_4XF32, true, "controle");
    TestaPressao(&c);
    TestaChamada(&c);
}
