/* dwarf_tipos.c - DIE de tipo do DWARF 4 (M6 P67, D-M6-3): cada TipoId alcancado por uma variavel vira UM DIE, filho da CU, referenciado por ref4.
 * Escalares = DW_TAG_base_type; ^T e rawptr = DW_TAG_pointer_type; struct e str/[]T/[dynamic]T = DW_TAG_structure_type com DW_TAG_member (offset do
 * layout); [N]T = DW_TAG_array_type; enum = DW_TAG_enumeration_type; o resto (map, union, ?T, tupla, proc...) = array de u8 do tamanho certo.
 * Recursao (struct que aponta para si) resolve-se por referencia: o item entra numa fila uma vez, e a ref4 se corrige no fim (DwarfPatchRefs).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M6 P67: primeira versao (base, ponteiro, struct, str/[]T/[dynamic]T, [N]T, enum e fallback de bytes; fila de itens e ref4 corrigida).
 * Verificacao: python compiler\construir_rapido.py && python ..\tests\nativo\m6\verificar_gdb.py --forja .\forja.exe --alvo linux
 */
#include <string.h>

#include "nativo/dbg/dwarf_interno.h"

#define DW_ATE_BOOLEAN 0x02
#define DW_ATE_FLOAT 0x04
#define DW_ATE_SIGNED 0x05
#define DW_ATE_UNSIGNED 0x07
#define DWARF_TAM_PONTEIRO 8
#define DWARF_MEMBROS_LISTA 5   /* ptr, len, cap, alocador, epoca: os de [dynamic]T; str e []T usam so os 2 primeiros */

static const char *const NOMES_LISTA[DWARF_MEMBROS_LISTA] = {"ptr", "len", "cap", "alocador", "epoca"};

typedef struct { TipoClasse classe; const char *nome; uint8_t codif; } BaseEscalar;

static const BaseEscalar BASES[] = {
    {TC_BOOL, "bool", DW_ATE_BOOLEAN},
    {TC_I8, "i8", DW_ATE_SIGNED}, {TC_I16, "i16", DW_ATE_SIGNED}, {TC_I32, "i32", DW_ATE_SIGNED}, {TC_INT, "int", DW_ATE_SIGNED},
    {TC_U8, "u8", DW_ATE_UNSIGNED}, {TC_U16, "u16", DW_ATE_UNSIGNED}, {TC_U32, "u32", DW_ATE_UNSIGNED}, {TC_UINT, "uint", DW_ATE_UNSIGNED},
    {TC_F64, "f64", DW_ATE_FLOAT},
};

static TipoClasse DwarfClasse(const DwarfCtx *x, TipoId id)
{
    if (id >= x->tipos.ntipos)
        FalhaInterna("dwarf: tipo %u fora da tabela", (unsigned)id);
    return TipoClasseDe(x->s->tipos, id);
}

static uint32_t DwarfTam(const DwarfCtx *x, TipoId id)
{
    return id < x->lay.qtd ? x->lay.tipos[id].tam : 0;
}

/* Posicao do item em DwarfTipos.off: TIPO em [0, n), PONTEIRO em [n, 2n), VOID em 2n. */
static uint32_t DwarfIndice(const DwarfCtx *x, uint8_t tipo, TipoId id)
{
    uint32_t n = x->tipos.ntipos;

    if (tipo == DWARF_ITEM_VOID)
        return 2 * n;
    if (id >= n)
        FalhaInterna("dwarf: tipo %u fora do registro de DIE", (unsigned)id);
    return tipo == DWARF_ITEM_TIPO ? id : n + id;
}

/* ref4 ao item: escreve 4 bytes a corrigir e enfileira o item se ainda nao saiu nem esta na fila. */
static void DwarfRefItem(DwarfCtx *x, Buf *b, uint8_t tipo, TipoId id)
{
    uint32_t indice = DwarfIndice(x, tipo, id);
    DwarfRef *r = VecEmpurrar(&x->tipos.refs);
    DwarfItem *it;

    r->pos = b->tam;
    r->indice = indice;
    DwarfBytesU32(b, 0); /* ref4: corrigida em DwarfPatchRefs */
    if (x->tipos.na_fila[indice])
        return;
    x->tipos.na_fila[indice] = 1;
    it = VecEmpurrar(&x->tipos.fila);
    it->tipo = tipo;
    it->id = id;
}

void DwarfRefTipo(DwarfCtx *x, Buf *b, TipoId id)
{
    const TipoInfo *info;

    switch (DwarfClasse(x, id)) {
    case TC_PONTEIRO:
    case TC_ANULAVEL:
        info = TipoDe(x->s->tipos, id);
        DwarfRefItem(x, b, DWARF_ITEM_PONTEIRO, info->a);
        break;
    case TC_RAWPTR:
    case TC_CSTR:
        DwarfRefItem(x, b, DWARF_ITEM_VOID, 0);
        break;
    default:
        DwarfRefItem(x, b, DWARF_ITEM_TIPO, id);
    }
}

static void DwarfEmitirBase(Buf *b, const char *nome, uint32_t tam, uint8_t codif)
{
    DwarfBytesUleb(b, DWARF_ABREV_BASE);
    DwarfBytesTexto(b, nome);
    DwarfBytesUleb(b, tam);
    DwarfBytesU8(b, codif);
}

/* Nome e codificacao de um escalar; NULL = nao e escalar. O real sai como f32 ou f64 pelo tamanho. */
static const char *DwarfNomeEscalar(TipoClasse c, uint32_t tam, uint8_t *codif)
{
    if (c == TC_REAL) {
        *codif = DW_ATE_FLOAT;
        return tam == 4 ? "f32" : "f64";
    }
    for (size_t k = 0; k < sizeof BASES / sizeof BASES[0]; k++) {
        if (BASES[k].classe == c) {
            *codif = BASES[k].codif;
            return BASES[k].nome;
        }
    }
    return NULL;
}

static void DwarfCabecalhoEstrutura(Buf *b, const char *nome, uint32_t tam)
{
    DwarfBytesUleb(b, DWARF_ABREV_ESTRUTURA);
    DwarfBytesTexto(b, nome);
    DwarfBytesUleb(b, tam);
}

/* Array de n elementos do tipo elem (ref4) com o intervalo de contagem. Bytes = elem TIPO_U8. */
static void DwarfEmitirVetor(DwarfCtx *x, Buf *b, TipoId elem, uint32_t n)
{
    DwarfBytesUleb(b, DWARF_ABREV_VETOR);
    DwarfRefTipo(x, b, elem);
    DwarfBytesUleb(b, DWARF_ABREV_INTERVALO);
    DwarfBytesUleb(b, n);
    DwarfBytesU8(b, 0);
}

/* Descricao de uma struct de lista: nome do DIE, TipoId da struct, elemento (ptr aponta para ele) e quantos membros pegar do layout. */
typedef struct { const char *nome; TipoId id; TipoId elem; uint32_t qtd; } DwarfLista;

/* Membro k de str, []T ou [dynamic]T: ptr aponta para o elemento; len, cap sao int; alocador e epoca sao u32. */
static void DwarfMembroLista(DwarfCtx *x, Buf *b, const DwarfLista *l, uint32_t k)
{
    DwarfBytesUleb(b, DWARF_ABREV_MEMBRO);
    DwarfBytesTexto(b, NOMES_LISTA[k]);
    if (k == 0)
        DwarfRefItem(x, b, DWARF_ITEM_PONTEIRO, l->elem);
    else
        DwarfRefItem(x, b, DWARF_ITEM_TIPO, k <= 2 ? TIPO_INT : TIPO_U32);
    DwarfBytesUleb(b, LayoutOffsetCampo(&x->lay, l->id, k));
}

static void DwarfEmitirListaFixa(DwarfCtx *x, Buf *b, const DwarfLista *l)
{
    uint32_t qtd_layout = LayoutQtdMembros(&x->lay, l->id);

    DwarfCabecalhoEstrutura(b, l->nome, DwarfTam(x, l->id));
    for (uint32_t k = 0; k < l->qtd && k < qtd_layout; k++)
        DwarfMembroLista(x, b, l, k);
    DwarfBytesU8(b, 0);
}

static const IrEnum *DwarfAcharEnum(const IrPrograma *ir, int32_t nome)
{
    for (uint32_t k = 0; k < ir->nenums; k++) {
        if (ir->enums[k].nome == nome)
            return &ir->enums[k];
    }
    return NULL;
}

/* Struct com definicao na IR: membros pelo nome do campo, tipo (ref4) e offset do layout. NULL na IR (opaca ou sem definicao): false. */
static bool DwarfEmitirEstrutura(DwarfCtx *x, Buf *b, TipoId id)
{
    const IrStruct *s = LayoutEstrutura(x->s->tipos, x->s->ir, id);
    const TipoInfo *info = TipoDe(x->s->tipos, id);

    if (s == NULL || s->opaco)
        return false;
    DwarfCabecalhoEstrutura(b, InternerTexto(x->s->nomes, (int32_t)info->a), DwarfTam(x, id));
    for (uint32_t k = 0; k < s->ncampos; k++) {
        DwarfBytesUleb(b, DWARF_ABREV_MEMBRO);
        DwarfBytesTexto(b, InternerTexto(x->s->nomes, s->campos[k].nome));
        DwarfRefTipo(x, b, s->campos[k].tipo);
        DwarfBytesUleb(b, LayoutOffsetCampo(&x->lay, id, k));
    }
    DwarfBytesU8(b, 0);
    return true;
}

/* Enum com definicao na IR: enumeradores com o valor C (sequencial quando a IR nao tem valores). */
static bool DwarfEmitirEnum(DwarfCtx *x, Buf *b, TipoId id)
{
    const TipoInfo *info = TipoDe(x->s->tipos, id);
    const IrEnum *e = DwarfAcharEnum(x->s->ir, (int32_t)info->a);

    if (e == NULL)
        return false;
    DwarfBytesUleb(b, DWARF_ABREV_ENUM);
    DwarfBytesTexto(b, InternerTexto(x->s->nomes, (int32_t)info->a));
    DwarfBytesUleb(b, DwarfTam(x, id));
    for (uint32_t k = 0; k < e->nmembros; k++) {
        DwarfBytesUleb(b, DWARF_ABREV_ENUMERADOR);
        DwarfBytesTexto(b, InternerTexto(x->s->nomes, e->membros[k]));
        DwarfBytesSleb(b, e->valores != NULL ? e->valores[k] : (int64_t)k);
    }
    DwarfBytesU8(b, 0);
    return true;
}

/* Struct ou enum sem definicao na IR cai aqui como bytes: so o tamanho certo. */
static void DwarfEmitirComposto(DwarfCtx *x, Buf *b, TipoId id, TipoClasse c)
{
    if (c == TC_STRUCT && DwarfEmitirEstrutura(x, b, id))
        return;
    if (c == TC_ENUM && DwarfEmitirEnum(x, b, id))
        return;
    DwarfEmitirVetor(x, b, TIPO_U8, DwarfTam(x, id));
}

/* Um TipoId: escalar, str/[]T/[dynamic]T, [N]T, struct/enum ou o resto como bytes. */
static void DwarfEmitirTipo(DwarfCtx *x, Buf *b, TipoId id)
{
    const TipoInfo *info = TipoDe(x->s->tipos, id);
    TipoClasse c = DwarfClasse(x, id);
    uint32_t tam = DwarfTam(x, id);
    uint8_t codif = 0;
    const char *base = DwarfNomeEscalar(c, tam, &codif);

    if (base != NULL)
        DwarfEmitirBase(b, base, tam, codif);
    else if (c == TC_TEXTO)
        DwarfEmitirListaFixa(x, b, &(DwarfLista){"str", id, TIPO_U8, 2});
    else if (c == TC_FATIA)
        DwarfEmitirListaFixa(x, b, &(DwarfLista){"[]T", id, info->a, 2});
    else if (c == TC_DINAMICO)
        DwarfEmitirListaFixa(x, b, &(DwarfLista){"[dynamic]T", id, info->a, DWARF_MEMBROS_LISTA});
    else if (c == TC_ARRAY)
        DwarfEmitirVetor(x, b, info->a, info->b);
    else
        DwarfEmitirComposto(x, b, id, c);
}

/* Item da fila: DIE de tipo, ponteiro para o tipo (id = apontado) ou void *. */
static void DwarfEmitirItem(DwarfCtx *x, DwarfItem it)
{
    Buf *b = DwarfBufSecao(x, x->sec_info);

    x->tipos.off[DwarfIndice(x, it.tipo, it.id)] = (uint32_t)b->tam;
    if (it.tipo == DWARF_ITEM_VOID) {
        DwarfBytesUleb(b, DWARF_ABREV_PONTEIRO_VOID);
        DwarfBytesUleb(b, DWARF_TAM_PONTEIRO);
    } else if (it.tipo == DWARF_ITEM_PONTEIRO) {
        DwarfBytesUleb(b, DWARF_ABREV_PONTEIRO);
        DwarfBytesUleb(b, DWARF_TAM_PONTEIRO);
        DwarfRefTipo(x, b, it.id);
    } else {
        DwarfEmitirTipo(x, b, it.id);
    }
}

void DwarfTiposIniciar(DwarfCtx *x)
{
    uint32_t n = (uint32_t)x->s->tipos->itens.n;
    size_t tam = 2 * (size_t)n + 1;

    x->tipos.ntipos = n;
    x->tipos.off = ArenaAlocar(&x->arena, tam * sizeof(uint32_t));
    x->tipos.na_fila = ArenaAlocar(&x->arena, tam);
    for (size_t i = 0; i < tam; i++) {
        x->tipos.off[i] = DWARF_SEM_DIE;
        x->tipos.na_fila[i] = 0;
    }
    VecIniciar(&x->tipos.fila, sizeof(DwarfItem));
    VecIniciar(&x->tipos.refs, sizeof(DwarfRef));
}

/* Emite a fila inteira; cada item pode enfileirar outros (a fila cresce enquanto anda). Copia o item: a emissao pode realocar a fila. */
void DwarfEscreverTipos(DwarfCtx *x)
{
    for (size_t i = 0; i < x->tipos.fila.n; i++) {
        DwarfItem it = *(const DwarfItem *)VecEm(&x->tipos.fila, i);

        DwarfEmitirItem(x, it);
    }
}

void DwarfPatchRefs(DwarfCtx *x)
{
    Buf *b = DwarfBufSecao(x, x->sec_info);

    for (size_t i = 0; i < x->tipos.refs.n; i++) {
        const DwarfRef *r = VecEm(&x->tipos.refs, i);
        uint32_t alvo = x->tipos.off[r->indice];

        if (alvo == DWARF_SEM_DIE)
            FalhaInterna("dwarf: referencia a tipo sem DIE (indice %u)", (unsigned)r->indice);
        DwarfPatchU32(b, r->pos, alvo);
    }
}
