/* emit_h.c - cabecalho C da DLL: um prototipo por 'pub export proc', para PHP (FFI::cdef) e C.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - 0.21.0 F4 passo 2a: include backend/emit_h.h (o emit_c.h saiu com o emissor C); EMIT_H_VERSAO no lugar de EMIT_C_VERSAO (mesmo valor 0.12.0; o .h gerado nao muda de bytes).
 *            0.3.0 - Linguagem v1.7 P7: ?cstr = "const char *" na ABI; "void forja_liberar(const char *p);" quando algum export devolve cstr/?cstr.
 *            0.2.0 -Linguagem v1.3 P7: struct por valor sai DEFINIDA no .h (ordem por dependencia); ponteiros e ?^T de qualquer tipo.
 *            0.1.1 - Linguagem v1.3: TipoDaAbi ganha os nomes de rawptr ("void *") e cstr ("const char *").
 *            0.1.0 - Linguagem v1.1: primeira versao.
 * Verificacao: python ..\tests\cli\verificar_cli.py --forja <exe>   (o .h passa em gcc -fsyntax-only e no FFI::cdef do PHP)
 *
 * Sem nenhuma diretiva '#': o FFI::cdef do PHP recusa pre-processador. Os tipos sao int32_t, int64_t, float e bool, que o PHP FFI
 * conhece; em C, inclua stdint.h e stdbool.h antes. Struct so atras de ponteiro sai opaca ('struct fj_Nome;'); por valor sai
 * definida, com os mesmos campos e a mesma ordem do C interno.
 */
#include <stdlib.h>

#include "backend/emit_h.h"

static const char *TipoDaAbi(TipoClasse classe)
{
    switch (classe) {
    case TC_VAZIO: return "void";
    case TC_BOOL: return "bool";
    case TC_I32: case TC_ENUM: return "int32_t"; /* enum do C e int (4 bytes) */
    case TC_INT: return "int64_t";
    case TC_REAL: return "float";
    case TC_I8: return "int8_t";
    case TC_I16: return "int16_t";
    case TC_U8: return "uint8_t";
    case TC_U16: return "uint16_t";
    case TC_U32: return "uint32_t";
    case TC_UINT: return "uint64_t";
    case TC_F64: return "double";
    case TC_RAWPTR: return "void *";
    case TC_CSTR: return "const char *";
    default: return NULL;
    }
}

/* "int32_t", "struct fj_Nome", "struct fj_Nome *", "int32_t *" ou "void". A semantica (E0901) garante que so chegam tipos da ABI. */
static void AnexarTipoAbi(const Tipos *tipos, const Interner *nomes, Buf *saida, TipoId tipo)
{
    const TipoInfo *info = TipoDe(tipos, tipo);
    const char *simples = TipoDaAbi(info->classe);

    if (simples != NULL || TipoOpcionalNicho(tipos, tipo)) {
        BufAnexar(saida, simples != NULL ? simples : "const char *");
        return;
    }
    if (info->classe == TC_STRUCT) {
        BufAnexar(saida, "struct fj_");
        BufAnexar(saida, InternerTexto(nomes, (int32_t)info->a));
        return;
    }
    if (info->classe != TC_PONTEIRO && info->classe != TC_ANULAVEL)
        FalhaInterna("emit_h: tipo %u de classe %d nao atravessa a ABI C (a semantica devia ter recusado, E0901)", (unsigned)tipo, (int)info->classe);
    AnexarTipoAbi(tipos, nomes, saida, (TipoId)info->a);
    BufAnexar(saida, " *");
}

/* Uso de cada struct nas assinaturas exportadas: USO_PONTEIRO (so opaca) ou USO_VALOR (definicao completa no .h). */
enum { USO_PONTEIRO = 1, USO_VALOR = 2, USO_EMITIDA = 4 };

typedef struct { const IrPrograma *prog; const Tipos *tipos; const Interner *nomes; Buf *saida; uint8_t *uso; } CtxH;

static uint32_t AcharStruct(const CtxH *h, TipoId tipo)
{
    int32_t nome = (int32_t)TipoDe(h->tipos, tipo)->a;

    for (uint32_t k = 0; k < h->prog->nstructs; k++) {
        if (h->prog->structs[k].nome == nome)
            return k;
    }
    FalhaInterna("emit_h: struct '%s' nao esta no IR", InternerTexto(h->nomes, nome));
}

/* Marca as structs que o tipo toca: atras de ponteiro so opaca; por valor, completa e desce nos campos. */
static void MarcarUsoStruct(CtxH *h, TipoId tipo, bool atras_de_ponteiro)
{
    TipoClasse classe = TipoClasseDe(h->tipos, tipo);

    if (classe == TC_PONTEIRO || classe == TC_ANULAVEL) {
        MarcarUsoStruct(h, (TipoId)TipoDe(h->tipos, tipo)->a, true);
        return;
    }
    if (classe != TC_STRUCT)
        return;
    uint32_t k = AcharStruct(h, tipo);
    if (atras_de_ponteiro) {
        h->uso[k] |= USO_PONTEIRO;
        return;
    }
    if (h->uso[k] & USO_VALOR)
        return;
    h->uso[k] |= USO_VALOR;
    for (uint32_t c = 0; c < h->prog->structs[k].ncampos; c++)
        MarcarUsoStruct(h, h->prog->structs[k].campos[c].tipo, false);
}

/* "struct fj_Vec3 { float f_x; float f_y; float f_z; };": a definicao do .h, depois das structs que ela tem por valor. */
static void EmitirDefinicaoH(CtxH *h, uint32_t k)
{
    const IrStruct *st = &h->prog->structs[k];

    if (h->uso[k] & USO_EMITIDA)
        return;
    h->uso[k] |= USO_EMITIDA;
    for (uint32_t c = 0; c < st->ncampos; c++) {
        if (TipoClasseDe(h->tipos, st->campos[c].tipo) == TC_STRUCT)
            EmitirDefinicaoH(h, AcharStruct(h, st->campos[c].tipo));
    }
    BufFormatar(h->saida, "struct fj_%s { ", InternerTexto(h->nomes, st->nome));
    if (st->ncampos == 0)
        BufAnexar(h->saida, "char vazio_; ");
    for (uint32_t c = 0; c < st->ncampos; c++) {
        AnexarTipoAbi(h->tipos, h->nomes, h->saida, st->campos[c].tipo);
        BufAnexar(h->saida, " f_");
        BufAnexar(h->saida, InternerTexto(h->nomes, st->campos[c].nome));
        BufAnexar(h->saida, "; ");
    }
    BufAnexar(h->saida, "};\n");
}

/* "struct fj_Nome;" para a struct que so aparece atras de ponteiro; depois a definicao das que vao por valor. */
static void DeclararStructs(CtxH *h)
{
    for (uint32_t f = 0; f < h->prog->nfuncoes; f++) {
        const IrFuncao *fn = &h->prog->funcoes[f];
        if (!fn->exportado)
            continue;
        MarcarUsoStruct(h, fn->retorno, false);
        for (uint32_t p = 0; p < fn->nparams; p++)
            MarcarUsoStruct(h, fn->params[p].tipo, false);
    }
    for (uint32_t k = 0; k < h->prog->nstructs; k++) {
        if (h->uso[k] == USO_PONTEIRO)
            BufFormatar(h->saida, "struct fj_%s;\n", InternerTexto(h->nomes, h->prog->structs[k].nome));
    }
    for (uint32_t k = 0; k < h->prog->nstructs; k++) {
        if (h->uso[k] & USO_VALOR)
            EmitirDefinicaoH(h, k);
    }
}

uint32_t ContarExportados(const IrPrograma *prog)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < prog->nfuncoes; k++) {
        if (prog->funcoes[k].exportado)
            n++;
    }
    return n;
}

bool ExportaCstr(const IrPrograma *prog, const Tipos *tipos)
{
    for (uint32_t k = 0; k < prog->nfuncoes; k++) {
        const IrFuncao *f = &prog->funcoes[k];
        if (f->exportado && (TipoClasseDe(tipos, f->retorno) == TC_CSTR || TipoOpcionalNicho(tipos, f->retorno)))
            return true;
    }
    return false;
}

void EmitirCabecalho(const IrPrograma *prog, const Tipos *tipos, const Interner *nomes, Buf *saida)
{
    BufFormatar(saida, "/* Gerado pela Forja (emit_c %s). Prototipos dos 'pub export proc'. */\n", EMIT_H_VERSAO);
    CtxH h = {prog, tipos, nomes, saida, calloc(prog->nstructs + 1, 1)};
    if (h.uso == NULL)
        FalhaInterna("emit_h: sem memoria para %u structs", (unsigned)prog->nstructs);
    DeclararStructs(&h);
    free(h.uso);
    for (uint32_t k = 0; k < prog->nfuncoes; k++) {
        const IrFuncao *f = &prog->funcoes[k];
        if (!f->exportado)
            continue;
        AnexarTipoAbi(tipos, nomes, saida, f->retorno);
        BufAnexarN(saida, " ", 1);
        BufAnexar(saida, InternerTexto(nomes, f->nome));
        BufAnexarN(saida, "(", 1);
        if (f->nparams == 0)
            BufAnexar(saida, "void");
        for (uint32_t p = 0; p < f->nparams; p++) {
            if (p > 0)
                BufAnexar(saida, ", ");
            AnexarTipoAbi(tipos, nomes, saida, f->params[p].tipo);
            BufAnexarN(saida, " a", 2);
            BufAnexarU64(saida, p);
        }
        BufAnexar(saida, ");\n");
    }
    if (ExportaCstr(prog, tipos))
        BufAnexar(saida, "void forja_liberar(const char *p);\n");
}
