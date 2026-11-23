/* interface_dll.c - texto canonico da interface de uma DLL Forja (a base do hash que forja_dll_info devolve).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - G3 G0c: ComparaEntrada desempata (procs: ordem em prog->funcoes; tipos: TipoId), ordem total.
 *            0.1.0 - M8 H1 (D-M8-2): primeira versao. Procs exportados em ordem de nome, tipos alcancados (struct com tamanho, alinhamento e offset de cada campo; enum com
 *                    valores; struct opaca) em ordem de nome. Nao leva nome de parametro, corpo, constante nem nada fora da ABI.
 * Verificacao: python -I ..\tests\rodar_bateria.py --so interop (caso dinamica\dll_info)
 */
#include <stdlib.h>
#include <string.h>

#include "backend/interface_dll.h"
#include "common/arena.h"
#include "common/vec.h"
#include "ir/layout.h"

#define ENUM_BASE "i32" /* enum de export e int32_t na ABI (TC_ENUM no emit_h) */

typedef struct {
    const IrPrograma *prog;
    const Tipos *tipos;
    const Interner *nomes;
    Layouts lay;
    Vec alcancados; /* TipoId de struct e enum que a interface toca, sem repetir */
} Interface;

typedef struct { const char *nome; const IrFuncao *proc; TipoId tipo; } Entrada;

static const IrStruct *AcharStruct(const Interface *x, TipoId tipo)
{
    int32_t nome = (int32_t)TipoDe(x->tipos, tipo)->a;

    for (uint32_t k = 0; k < x->prog->nstructs; k++) {
        if (x->prog->structs[k].nome == nome)
            return &x->prog->structs[k];
    }
    FalhaInterna("interface_dll: struct '%s' nao esta no IR", InternerTexto(x->nomes, nome));
}

static const IrEnum *AcharEnum(const Interface *x, TipoId tipo)
{
    int32_t nome = (int32_t)TipoDe(x->tipos, tipo)->a;

    for (uint32_t k = 0; k < x->prog->nenums; k++) {
        if (x->prog->enums[k].nome == nome)
            return &x->prog->enums[k];
    }
    FalhaInterna("interface_dll: enum '%s' nao esta no IR", InternerTexto(x->nomes, nome));
}

static const char *NomeEscalar(TipoClasse classe)
{
    switch (classe) {
    case TC_VAZIO: return "void";
    case TC_BOOL: return "bool";
    case TC_I8: return "i8";
    case TC_I16: return "i16";
    case TC_I32: return "i32";
    case TC_INT: return "int";
    case TC_U8: return "u8";
    case TC_U16: return "u16";
    case TC_U32: return "u32";
    case TC_UINT: return "uint";
    case TC_REAL: return "f32";
    case TC_F64: return "f64";
    case TC_RAWPTR: return "rawptr";
    case TC_CSTR: return "cstr";
    default: return NULL;
    }
}

/* "i32", "^Vec3", "?^u8", "?cstr", "[4]f32": o tipo como a Forja o escreve; a semantica (E0901) garante que so chegam tipos da ABI. */
static void AnexarTipo(const Interface *x, Buf *saida, TipoId tipo)
{
    const TipoInfo *info = TipoDe(x->tipos, tipo);
    const char *escalar = NomeEscalar(info->classe);

    if (escalar != NULL)
        BufAnexar(saida, escalar);
    else if (TipoOpcionalNicho(x->tipos, tipo))
        BufAnexar(saida, "?cstr");
    else if (info->classe == TC_STRUCT || info->classe == TC_ENUM)
        BufAnexar(saida, InternerTexto(x->nomes, (int32_t)info->a));
    else if (info->classe == TC_PONTEIRO || info->classe == TC_ANULAVEL) {
        BufAnexar(saida, info->classe == TC_PONTEIRO ? "^" : "?^");
        AnexarTipo(x, saida, (TipoId)info->a);
    } else if (info->classe == TC_ARRAY) {
        BufFormatar(saida, "[%u]", (unsigned)info->b);
        AnexarTipo(x, saida, (TipoId)info->a);
    } else
        FalhaInterna("interface_dll: tipo %u de classe %d nao atravessa a ABI C (a semantica devia ter recusado, E0901)", (unsigned)tipo, (int)info->classe);
}

static bool JaAlcancado(const Interface *x, TipoId tipo)
{
    for (size_t k = 0; k < x->alcancados.n; k++) {
        if (*(const TipoId *)VecEm(&x->alcancados, k) == tipo)
            return true;
    }
    return false;
}

/* Guarda as structs e enums que o tipo toca (atras de ponteiro e dentro de array tambem): descendo nos campos, a interface inclui o que o host le pelos ponteiros. */
static void Alcancar(Interface *x, TipoId tipo)
{
    const TipoInfo *info = TipoDe(x->tipos, tipo);

    if (info->classe == TC_PONTEIRO || info->classe == TC_ANULAVEL || info->classe == TC_ARRAY) {
        Alcancar(x, (TipoId)info->a);
        return;
    }
    if ((info->classe != TC_STRUCT && info->classe != TC_ENUM) || JaAlcancado(x, tipo))
        return;
    *(TipoId *)VecEmpurrar(&x->alcancados) = tipo;
    if (info->classe != TC_STRUCT)
        return;
    const IrStruct *st = AcharStruct(x, tipo);

    for (uint32_t c = 0; c < st->ncampos; c++)
        Alcancar(x, st->campos[c].tipo);
}

/* "nome(T1, T2) -> R" */
static void LinhaDoProc(Interface *x, Buf *saida, const IrFuncao *f)
{
    BufAnexar(saida, InternerTexto(x->nomes, f->nome));
    BufAnexar(saida, "(");
    for (uint32_t p = 0; p < f->nparams; p++) {
        if (p > 0)
            BufAnexar(saida, ", ");
        AnexarTipo(x, saida, f->params[p].tipo);
        Alcancar(x, f->params[p].tipo);
    }
    BufAnexar(saida, ") -> ");
    AnexarTipo(x, saida, f->retorno);
    Alcancar(x, f->retorno);
    BufAnexar(saida, "\n");
}

/* "struct N size S align A { campo: T @off; }" (campo com nome e offset: a migracao futura e por nome) ou "struct N opaque". */
static void LinhaDaStruct(const Interface *x, Buf *saida, TipoId tipo)
{
    const IrStruct *st = AcharStruct(x, tipo);

    BufFormatar(saida, "struct %s ", InternerTexto(x->nomes, st->nome));
    if (st->opaco) {
        BufAnexar(saida, "opaque\n");
        return;
    }
    BufFormatar(saida, "size %u align %u {", (unsigned)LayoutTamanho(&x->lay, tipo), (unsigned)LayoutAlinhamento(&x->lay, tipo));
    for (uint32_t c = 0; c < st->ncampos; c++) {
        BufFormatar(saida, " %s: ", InternerTexto(x->nomes, st->campos[c].nome));
        AnexarTipo(x, saida, st->campos[c].tipo);
        BufFormatar(saida, " @%u;", (unsigned)LayoutOffsetCampo(&x->lay, tipo, c));
    }
    BufAnexar(saida, " }\n");
}

/* "enum N: i32 { a = 0; b = 1; }": o valor e o do C (IrEnum.valores) ou o indice do membro. */
static void LinhaDoEnum(const Interface *x, Buf *saida, TipoId tipo)
{
    const IrEnum *en = AcharEnum(x, tipo);

    BufFormatar(saida, "enum %s: " ENUM_BASE " {", InternerTexto(x->nomes, en->nome));
    for (uint32_t m = 0; m < en->nmembros; m++) {
        BufFormatar(saida, " %s = ", InternerTexto(x->nomes, en->membros[m]));
        BufAnexarI64(saida, en->valores != NULL ? en->valores[m] : (int64_t)m);
        BufAnexar(saida, ";");
    }
    BufAnexar(saida, " }\n");
}

static int ComparaEntrada(const void *a, const void *b)
{
    const Entrada *x = a, *y = b;
    int c = strcmp(x->nome, y->nome);

    if (c != 0) return c;
    if (x->proc != y->proc) return x->proc < y->proc ? -1 : 1; /* procs: mesmo array prog->funcoes, endereco = ordem de declaracao */
    return x->tipo < y->tipo ? -1 : (x->tipo > y->tipo ? 1 : 0);
}

static void LinhasDosProcs(Interface *x, Buf *saida)
{
    uint32_t n = 0;
    Entrada *procs = malloc((x->prog->nfuncoes + 1u) * sizeof *procs);

    if (procs == NULL)
        FalhaInterna("interface_dll: sem memoria para %u procs", (unsigned)x->prog->nfuncoes);
    for (uint32_t k = 0; k < x->prog->nfuncoes; k++) {
        if (x->prog->funcoes[k].exportado)
            procs[n++] = (Entrada){InternerTexto(x->nomes, x->prog->funcoes[k].nome), &x->prog->funcoes[k], 0};
    }
    qsort(procs, n, sizeof *procs, ComparaEntrada);
    for (uint32_t k = 0; k < n; k++)
        LinhaDoProc(x, saida, procs[k].proc);
    free(procs);
}

static void LinhasDosTipos(const Interface *x, Buf *saida)
{
    uint32_t n = (uint32_t)x->alcancados.n;
    Entrada *tipos = malloc((n + 1u) * sizeof *tipos);

    if (tipos == NULL)
        FalhaInterna("interface_dll: sem memoria para %u tipos", (unsigned)n);
    for (uint32_t k = 0; k < n; k++) {
        TipoId tipo = *(const TipoId *)VecEm(&x->alcancados, k);
        tipos[k] = (Entrada){InternerTexto(x->nomes, (int32_t)TipoDe(x->tipos, tipo)->a), NULL, tipo};
    }
    qsort(tipos, n, sizeof *tipos, ComparaEntrada);
    for (uint32_t k = 0; k < n; k++) {
        if (TipoClasseDe(x->tipos, tipos[k].tipo) == TC_STRUCT)
            LinhaDaStruct(x, saida, tipos[k].tipo);
        else
            LinhaDoEnum(x, saida, tipos[k].tipo);
    }
    free(tipos);
}

void EmitirInterfaceDll(const IrPrograma *prog, const Tipos *tipos, const Interner *nomes, Buf *saida)
{
    Arena arena = {0};
    Interface x = {.prog = prog, .tipos = tipos, .nomes = nomes};

    LayoutCalcular(tipos, prog, &arena, &x.lay);
    VecIniciar(&x.alcancados, sizeof(TipoId));
    LinhasDosProcs(&x, saida);
    LinhasDosTipos(&x, saida);
    VecLiberar(&x.alcancados);
    ArenaLiberar(&arena);
}
