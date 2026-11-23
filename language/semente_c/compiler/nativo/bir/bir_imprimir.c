/* bir_imprimir.c - dump deterministico da BIR (uma linha por instrucao), para revisao e para os dourados.
 * Versao: 0.10.1
 * Changelog: 0.10.1 - G3 G0c: o texto do imediato real vem de FormatarRealCurto (menor texto que volta ao mesmo valor, o do print da Forja), no lugar de %.9g/%.17g.
 *            0.10.0 - S2 (D0703): nome "splat" ("vD:v128 = splat.4xf32 vS"; add/sub/mul/div de vetor saem "mul.4xf32 vA, vB" pelo corpo generico).
 *            0.9.0 - S1b (D0703): classe v128 e a forma do vetor depois do nome da operacao ("copia.4xf32").
 *            0.8.0 - R5 (SIMD): "vD:i32 = seleciona.lt_s(vX, vY) vA, vB" (BIR_SELECIONA).
 *            0.7.2 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.7.1 - M3 F9b-2: decisoes extraidas de ImprimirTextoEscapado, ImprimirMemoria, ImprimirSufixo, ImprimirCorpo e ImprimirCabecalho.
 *            0.7.0 - M3 P21v-c: "chama_ind vN(args) => destino" (BIR_CHAMA_IND; vN = o endereco da funcao).
 *            0.6.0 - Linguagem v1.11 L13: nome "raiz" (BIR_RAIZ; o corpo generico imprime "vN:f32 = raiz vM").
 *            0.5.0 - M2 P11b: agregado por valor: operando "vN/T" (endereco de T bytes), parametro "vK:i64/T", retorno "-> i64/T" e o destino do retorno agregado "chama @f(args) => vN/T".
 *            0.4.0 - M2 P11a: end_slot, carrega, grava, copia_mem e zera_mem, e as linhas "slot kN tam=T ali=A" depois do cabecalho da funcao (so quando ha slots).
 *            0.3.0 - M2 P10b: imediato real "#1.5:f32(0x3fc00000)" (texto %.9g/%.17g e bits), i_para_f (.s/.u), f_para_f, condicoes feq..fge e os dados "dK = align A x\"..\"".
 *            0.2.0 - M2 P9: nomes de div, rem, shl, shr e not; DIV e REM levam ".s" ou ".u" (aux) como os *_ov.
 *            0.1.0 - backend x64 P6: primeira versao.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj>
 *
 * Formato:
 *   funcao fj_dobro(v0:i64, v1:i64) -> i64          (retorno vazio: "-> vazio")
 *   b0:                                              (bloco frio: "b1 frio:")
 *     v2:i64 = mul_ov.s v0, v1 ; frio b1 ; 5:12      (".s" com sinal, ".u" sem sinal; "; linha:coluna" local ao .fj, ausente nas sinteticas)
 *     retorna v2 ; 6:5
 *   b1 frio:
 *     v3:i64 = end @s0 ; 5:12
 *     armadilha @forja_falha_estouro(v3, v0, v1, v4, #5:i32) ; 5:12
 * Operandos: vN | vN.sx / vN.zx (estendido a 64 bits na chamada) | #imediato:classe | @simbolo (@sK = texto) | bN.
 * No fim, um bloco "simbolos:" com uma linha "  sK = \"...\"" por texto, na ordem do primeiro uso (bytes fora do ASCII visivel em octal).
 */
#include <stdio.h>
#include <string.h>

#include "common/real_curto.h"
#include "nativo/bir/bir.h"

static const char *const NOMES_CLASSE[] = {"-", "i8", "i16", "i32", "i64", "f32", "f64", "v128"};

static const char *const NOMES_FORMA_VETOR[BVEC_QTD] = {"4xf32", "2xf64", "4xi32", "2xi64"};

static const char *const NOMES_OP[BIR_QTD] = {
    [BIR_CONST] = "const", [BIR_COPIA] = "copia", [BIR_ADD] = "add", [BIR_SUB] = "sub", [BIR_MUL] = "mul", [BIR_AND] = "and",
    [BIR_OR] = "or", [BIR_XOR] = "xor", [BIR_NEG] = "neg", [BIR_CMP] = "cmp", [BIR_EXT_S] = "ext_s", [BIR_EXT_Z] = "ext_z",
    [BIR_TRUNC] = "trunc", [BIR_ADD_OV] = "add_ov", [BIR_SUB_OV] = "sub_ov", [BIR_MUL_OV] = "mul_ov", [BIR_NEG_OV] = "neg_ov",
    [BIR_END_SIMBOLO] = "end", [BIR_CHAMA] = "chama", [BIR_SALTA] = "salta", [BIR_DESVIA] = "desvia", [BIR_RETORNA] = "retorna",
    [BIR_ARMADILHA] = "armadilha", [BIR_DIV] = "div", [BIR_REM] = "rem", [BIR_SHL] = "shl", [BIR_SHR] = "shr", [BIR_NOT] = "not",
    [BIR_I_PARA_F] = "i_para_f", [BIR_F_PARA_F] = "f_para_f", [BIR_END_SLOT] = "end_slot", [BIR_CARREGA] = "carrega", [BIR_GRAVA] = "grava",
    [BIR_COPIA_MEM] = "copia_mem", [BIR_ZERA_MEM] = "zera_mem", [BIR_RAIZ] = "raiz",
    [BIR_CHAMA_IND] = "chama_ind", [BIR_SELECIONA] = "seleciona", [BIR_SPLAT] = "splat",
};

static const char *const NOMES_COND[] = {"eq", "ne", "lt_s", "le_s", "gt_s", "ge_s", "lt_u", "le_u", "gt_u", "ge_u",
                                         "feq", "fne", "flt", "fle", "fgt", "fge"};

static const char *NomeDaClasse(uint8_t classe)
{
    return classe < sizeof NOMES_CLASSE / sizeof NOMES_CLASSE[0] ? NOMES_CLASSE[classe] : "?";
}

static const char *NomeDoOp(uint8_t op)
{
    return op < BIR_QTD && NOMES_OP[op] != NULL ? NOMES_OP[op] : "?";
}

static void ImprimirByteEscapado(unsigned char ch, Buf *saida)
{
    switch (ch) {
    case '"':
    case '\\': BufFormatar(saida, "\\%c", ch); return;
    case '\n': BufAnexarN(saida, "\\n", 2); return;
    case '\t': BufAnexarN(saida, "\\t", 2); return;
    case '\r': BufAnexarN(saida, "\\r", 2); return;
    default: break;
    }
    if (ch < 0x20 || ch > 0x7e)
        BufFormatar(saida, "\\%03o", (unsigned)ch);
    else
        BufAnexarN(saida, (const char *)&ch, 1);
}

static void ImprimirTextoEscapado(const BirSimbolo *s, Buf *saida)
{
    BufAnexarN(saida, "\"", 1);
    for (uint32_t k = 0; k < s->tam; k++)
        ImprimirByteEscapado((unsigned char)s->bytes[k], saida);
    BufAnexarN(saida, "\"", 1);
}

static void ImprimirSimbolo(const BirPrograma *p, uint32_t indice, Buf *saida)
{
    const BirSimbolo *s = indice < p->nsimbolos ? &p->simbolos[indice] : NULL;

    if (s == NULL)
        BufAnexar(saida, "@?");
    else if (s->tipo == BS_TEXTO)
        BufFormatar(saida, "@s%u", (unsigned)s->ordem);
    else if (s->tipo == BS_DADO)
        BufFormatar(saida, "@d%u", (unsigned)s->ordem);
    else
        BufFormatar(saida, "@%s", s->nome);
}

/* Imediato real: o texto curto (menos digitos que voltam ao mesmo valor, FormatarRealCurto) e os bits, ex. "#1.5:f32(0x3fc00000)"; o nan sai sempre "nan" (o sinal esta nos bits). */
static void ImprimirImediatoReal(const BirOperando *o, Buf *saida)
{
    uint32_t bits32 = (uint32_t)o->imed;
    uint64_t bits64 = (uint64_t)o->imed;
    float f32;
    double f64;

    BufAnexar(saida, "#");
    if (o->classe == BC_F32) {
        memcpy(&f32, &bits32, sizeof f32);
        FormatarRealCurto(saida, (double)f32, true);
        BufFormatar(saida, ":f32(0x%08x)", (unsigned)bits32);
        return;
    }
    memcpy(&f64, &bits64, sizeof f64);
    FormatarRealCurto(saida, f64, false);
    BufFormatar(saida, ":f64(0x%016llx)", (unsigned long long)bits64);
}

static void ImprimirOperando(const BirPrograma *p, const BirOperando *o, Buf *saida)
{
    switch (o->tipo) {
    case BO_VREG:
        BufFormatar(saida, "v%u%s", (unsigned)o->id, o->extensao == BE_SINAL ? ".sx" : (o->extensao == BE_ZERO ? ".zx" : ""));
        if (o->tam != 0)
            BufFormatar(saida, "/%u", (unsigned)o->tam);
        break;
    case BO_IMED:
        if (BirClasseEhReal(o->classe))
            ImprimirImediatoReal(o, saida);
        else
            BufFormatar(saida, "#%lld:%s", (long long)o->imed, NomeDaClasse(o->classe));
        break;
    case BO_SIMBOLO: ImprimirSimbolo(p, o->id, saida); break;
    default: BufAnexarN(saida, "-", 1); break;
    }
}

static void ImprimirArgs(const BirPrograma *p, const BirInstr *i, Buf *saida)
{
    BufAnexarN(saida, "(", 1);
    for (uint32_t k = 0; k < i->nargs; k++) {
        if (k > 0)
            BufAnexarN(saida, ", ", 2);
        ImprimirOperando(p, &i->args[k], saida);
    }
    BufAnexarN(saida, ")", 1);
}

static bool TemOperandoB(const BirInstr *i)
{
    return i->b.tipo != BO_NADA;
}

/* Memoria: "end_slot kN", "carrega [vA+D]", "grava [vA+D], b", "copia_mem [vA], [vB] n=N", "zera_mem [vA] n=N". */
static void ImprimirEnderecoMemoria(const BirInstr *i, Buf *saida)
{
    BufFormatar(saida, " [v%u", (unsigned)i->a.id);
    if (i->op == BIR_CARREGA || i->op == BIR_GRAVA)
        BufFormatar(saida, "+%d", (int)i->desloc);
    BufAnexarN(saida, "]", 1);
}

static void ImprimirMemoria(const BirPrograma *p, const BirInstr *i, Buf *saida)
{
    if (i->op == BIR_END_SLOT) {
        BufFormatar(saida, " k%u", (unsigned)i->a.imed);
        return;
    }
    ImprimirEnderecoMemoria(i, saida);
    if (i->op == BIR_GRAVA) {
        BufAnexar(saida, ", ");
        ImprimirOperando(p, &i->b, saida);
    }
    if (i->op == BIR_COPIA_MEM)
        BufFormatar(saida, ", [v%u]", (unsigned)i->b.id);
    if (i->op == BIR_COPIA_MEM || i->op == BIR_ZERA_MEM)
        BufFormatar(saida, " n=%d", (int)i->desloc);
}

/* A operacao leva ".s" ou ".u": os *_ov, DIV e REM inteiros e I_PARA_F. */
static bool TemSufixoSinal(const BirInstr *i)
{
    return (i->op >= BIR_ADD_OV && i->op <= BIR_NEG_OV) || ((i->op == BIR_DIV || i->op == BIR_REM) && !BirClasseEhXmm(i->a.classe)) || i->op == BIR_I_PARA_F;
}

/* Sufixo depois do nome da operacao: .cond do CMP ou .s/.u. */
static void ImprimirSufixo(const BirInstr *i, Buf *saida)
{
    if (i->op == BIR_CMP || i->op == BIR_SELECIONA)
        BufFormatar(saida, ".%s", i->aux < sizeof NOMES_COND / sizeof NOMES_COND[0] ? NOMES_COND[i->aux] : "?");
    if (TemSufixoSinal(i))
        BufAnexar(saida, i->aux == BIR_SINAL ? ".s" : ".u");
}

/* Retorno agregado do CHAMA: " => vN/T", o endereco de destino. */
static void ImprimirDestinoAgregado(const BirPrograma *p, const BirInstr *i, Buf *saida)
{
    if (i->a.tipo == BO_NADA)
        return;
    BufAnexar(saida, " => ");
    ImprimirOperando(p, &i->a, saida);
}

/* " @simbolo(args)" no CHAMA e na ARMADILHA; " vN(args)" no CHAMA_IND (vN guarda o endereco da funcao). */
static void ImprimirChamada(const BirPrograma *p, const BirInstr *i, Buf *saida)
{
    BufAnexarN(saida, " ", 1);
    if (i->op == BIR_CHAMA_IND)
        ImprimirOperando(p, &i->b, saida);
    else
        ImprimirSimbolo(p, i->simbolo, saida);
    ImprimirArgs(p, i, saida);
    ImprimirDestinoAgregado(p, i, saida);
}

/* Operandos a, b e os alvos do DESVIA. */
static void ImprimirOperandosGerais(const BirPrograma *p, const BirInstr *i, Buf *saida)
{
    if (i->a.tipo != BO_NADA) {
        BufAnexarN(saida, " ", 1);
        ImprimirOperando(p, &i->a, saida);
    }
    if (TemOperandoB(i)) {
        BufAnexar(saida, ", ");
        ImprimirOperando(p, &i->b, saida);
    }
    if (i->op == BIR_DESVIA)
        BufFormatar(saida, ", b%u, b%u", (unsigned)i->alvo[0], (unsigned)i->alvo[1]);
}

/* Parte depois do "dst = ": nome da operacao (com .cond ou .s/.u) e os operandos. */
static void ImprimirCorpo(const BirPrograma *p, const BirInstr *i, Buf *saida)
{
    BufAnexar(saida, NomeDoOp(i->op));
    if (i->classe == BC_V128) /* a forma do vetor vai em aux: "copia.4xf32" */
        BufFormatar(saida, ".%s", i->aux < BVEC_QTD ? NOMES_FORMA_VETOR[i->aux] : "?");
    if (i->op >= BIR_END_SLOT) {
        ImprimirMemoria(p, i, saida);
        return;
    }
    ImprimirSufixo(i, saida);
    if (i->op == BIR_SELECIONA) { /* "seleciona.cond(x, y) a, b": a se x cond y, senao b */
        ImprimirArgs(p, i, saida);
        ImprimirOperandosGerais(p, i, saida);
        return;
    }
    if (i->op == BIR_CHAMA || i->op == BIR_CHAMA_IND || i->op == BIR_ARMADILHA) {
        ImprimirChamada(p, i, saida);
        return;
    }
    if (i->op == BIR_SALTA) {
        BufFormatar(saida, " b%u", (unsigned)i->alvo[0]);
        return;
    }
    ImprimirOperandosGerais(p, i, saida);
}

static void ImprimirInstr(const BirPrograma *p, const BirFuncao *f, const BirInstr *i, Buf *saida)
{
    BufAnexar(saida, "  ");
    if (i->dst != BIR_SEM_VREG)
        BufFormatar(saida, "v%u:%s = ", (unsigned)i->dst, NomeDaClasse(f->classe_vreg[i->dst]));
    ImprimirCorpo(p, i, saida);
    if (i->op >= BIR_ADD_OV && i->op <= BIR_NEG_OV)
        BufFormatar(saida, " ; frio b%u", (unsigned)i->alvo[0]);
    if (i->pos != BIR_SEM_POS && p->fonte != NULL) {
        FontePosicao pos = FonteLocalizarNoArquivo(p->fonte, i->pos);

        BufFormatar(saida, " ; %u:%u", (unsigned)pos.linha, (unsigned)pos.coluna);
    }
    BufAnexarN(saida, "\n", 1);
}

static void ImprimirParametros(const BirFuncao *f, Buf *saida)
{
    for (uint32_t k = 0; k < f->nparams; k++) {
        BufFormatar(saida, "%sv%u:%s", k > 0 ? ", " : "", (unsigned)k, NomeDaClasse(f->classe_vreg[k]));
        if (f->param_tam != NULL && f->param_tam[k] != 0)
            BufFormatar(saida, "/%u", (unsigned)f->param_tam[k]);
    }
}

static void ImprimirCabecalho(const BirPrograma *p, const BirFuncao *f, Buf *saida)
{
    BufFormatar(saida, "funcao %s(", p->simbolos[f->simbolo].nome);
    ImprimirParametros(f, saida);
    BufFormatar(saida, ") -> %s", f->retorno == BC_NADA ? "vazio" : NomeDaClasse(f->retorno));
    if (f->retorno_tam != 0)
        BufFormatar(saida, "/%u", (unsigned)f->retorno_tam);
    BufAnexarN(saida, "\n", 1);
    for (uint32_t k = 0; k < f->nslots; k++)
        BufFormatar(saida, "  slot k%u tam=%u ali=%u\n", (unsigned)k, (unsigned)f->slots[k].tam, (unsigned)f->slots[k].alinhamento);
}

static void ImprimirFuncao(const BirPrograma *p, const BirFuncao *f, Buf *saida)
{
    ImprimirCabecalho(p, f, saida);
    for (uint32_t b = 0; b < f->nblocos; b++) {
        const BirBloco *bloco = &f->blocos[b];

        BufFormatar(saida, "b%u%s:\n", (unsigned)b, bloco->frio ? " frio" : "");
        for (uint32_t k = 0; k < bloco->n; k++)
            ImprimirInstr(p, f, &bloco->instrs[k], saida);
    }
}

/* Dado: "  dK = align A x\"00 00 ...\"" com os bytes em hexa. */
static void ImprimirDado(const BirSimbolo *s, Buf *saida)
{
    BufFormatar(saida, "  d%u = align %u x\"", (unsigned)s->ordem, (unsigned)s->alinhamento);
    for (uint32_t k = 0; k < s->tam; k++)
        BufFormatar(saida, "%s%02x", k > 0 ? " " : "", (unsigned)(uint8_t)s->bytes[k]);
    BufAnexar(saida, "\"\n");
}

static void ImprimirSimbolos(const BirPrograma *p, Buf *saida)
{
    bool cabecalho = false;

    for (uint32_t k = 0; k < p->nsimbolos; k++) {
        const BirSimbolo *s = &p->simbolos[k];

        if (s->tipo != BS_TEXTO && s->tipo != BS_DADO)
            continue;
        if (!cabecalho)
            BufAnexar(saida, "simbolos:\n");
        cabecalho = true;
        if (s->tipo == BS_DADO) {
            ImprimirDado(s, saida);
            continue;
        }
        BufFormatar(saida, "  s%u = ", (unsigned)s->ordem);
        ImprimirTextoEscapado(s, saida);
        BufAnexarN(saida, "\n", 1);
    }
}

void BirImprimir(const BirPrograma *programa, Buf *saida)
{
    for (uint32_t k = 0; k < programa->nfuncoes; k++)
        ImprimirFuncao(programa, &programa->funcoes[k], saida);
    ImprimirSimbolos(programa, saida);
}
