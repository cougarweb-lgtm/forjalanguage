/* pdb_tipos.c - tipos do PDB (M6b P77a/P77b, D-M6-3): o TypeIndex CodeView de cada variavel. Tipo simples (escalar, ponteiro de 64 bits para escalar, rawptr/cstr) usa o
 * indice direto T_* e nao grava registro; os demais viram registros LF_* no TPI, gravados SOB DEMANDA (so os tipos que alguma variavel usa) em ordem topologica:
 * um registro so cita indices menores (DFS em pos-ordem, memoria por TipoId). struct = LF_FIELDLIST + LF_STRUCTURE; str, []T e [dynamic]T = LF_STRUCTURE (ptr, len[, cap,
 * alocador, epoca]) como o DWARF; [N]T = LF_ARRAY; enum = LF_FIELDLIST de LF_ENUMERATE + LF_ENUM; ^T e ?^T nao simples = LF_POINTER; o resto (?T, union, map, soa, matrix,
 * tupla, proc, bit_set, resultado) = LF_ARRAY de u8 do tamanho certo. Struct que aponta para si: LF_STRUCTURE fwdref (resolve so pelo hash; fora do aceite, limitacao conhecida).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6b P77b: PdbTipos (registros LF_* sob demanda, um vetor u32 de 2n por TipoId alocado na 1a variavel nao simples, LayoutCalcular preguicoso); PdbTipoIndice.
 *            0.1.0 - M6b P77a: primeira versao (PdbTipoSimples, funcao pura).
 * Verificacao: python compiler\construir_rapido.py && python ..\tests\nativo\m6b\ler_pdb.py <arquivo.pdb> --exigir-locais --exigir-tipos
 */
#include "nativo/dbg/pdb.h"

#define CV_T_VOID 0x0003u
#define CV_T_U8 0x69u
#define CV_T_UQUAD 0x23u                  /* tipo do indice de um LF_ARRAY */
#define CV_T_INT4 0x74u
#define CV_T_UINT4 0x75u
#define CV_T_INT8 0x76u
#define CV_PTR_64 0x0600u                 /* modo de ponteiro de 64 bits nos bits 8..11 do TypeIndex simples */
#define CV_SIMPLES_MAX 0x100u             /* abaixo disso o TypeIndex e um tipo simples sem modo de ponteiro */
#define CV_ATTR_PTR64 (0x0Cu | 8u << 13) /* LF_POINTER: ptrtype 0x0C (64 bits) e tamanho 8 */
#define CV_ATTR_PUBLICO 3u
#define CV_PROP_FWDREF 0x0080u
#define CV_REG_MAX 0xFF00u                /* acima disso o registro nao cabe no u16 de tamanho: o tipo cai para bytes */
#define CV_TAM_MEMBRO_MAX 24u             /* leaf + attr + tipo + numero (ate 10) + folga de alinhamento, sem o nome */
#define PDB_EM_CURSO 1u                   /* marca no vetor de indices: o tipo esta sendo registrado (ciclo) */

#define LF_POINTER 0x1002u
#define LF_FIELDLIST 0x1203u
#define LF_ENUMERATE 0x1502u
#define LF_ARRAY 0x1503u
#define LF_STRUCTURE 0x1505u
#define LF_ENUM 0x1507u
#define LF_MEMBER 0x150Du

#define PDB_MEMBROS_LISTA 5               /* ptr, len, cap, alocador, epoca: os de [dynamic]T; str e []T usam so os 2 primeiros */

static const char *const NOMES_LISTA[PDB_MEMBROS_LISTA] = {"ptr", "len", "cap", "alocador", "epoca"};

/* Escalar -> TypeIndex CodeView; 0 = nao e escalar. */
static uint32_t PdbEscalar(TipoClasse c)
{
    switch (c) {
    case TC_BOOL: return 0x30u;
    case TC_I8: return 0x68u;
    case TC_U8: return 0x69u;
    case TC_I16: return 0x72u;
    case TC_U16: return 0x73u;
    case TC_I32: return 0x74u;
    case TC_U32: return 0x75u;
    case TC_INT: return 0x76u;
    case TC_UINT: return 0x77u;
    case TC_REAL: return 0x40u;
    case TC_F64: return 0x41u;
    default: return 0;
    }
}

/* Contrato: devolve o TypeIndex simples de id, ou 0 quando o tipo nao e simples (struct, array, str, slice, enum, ?T, ^struct...). */
uint32_t PdbTipoSimples(const Tipos *t, TipoId id)
{
    TipoClasse c = TipoClasseDe(t, id);
    uint32_t apontado;

    if (c == TC_RAWPTR || c == TC_CSTR)
        return CV_PTR_64 | CV_T_VOID;
    if (c != TC_PONTEIRO)
        return PdbEscalar(c);
    apontado = PdbEscalar(TipoClasseDe(t, TipoDe(t, id)->a));
    return apontado != 0 ? CV_PTR_64 | apontado : 0;
}

void PdbTiposIniciar(PdbTipos *t, const Tipos *tipos, const IrPrograma *ir, const Interner *nomes)
{
    memset(t, 0, sizeof *t);
    t->tipos = tipos;
    t->ir = ir;
    t->nomes = nomes;
    t->proximo = PDB_PRIMEIRO_TIPO;
}

void PdbTiposLiberar(PdbTipos *t)
{
    BufLiberar(&t->reg);
    ArenaLiberar(&t->arena);
}

/* ---- escrita de registros ---- */

/* Fecha o buffer em multiplo de 4 com LF_PAD (F3 F2 F1, terminando em F1); vale para o fim de registro e de LF_MEMBER/LF_ENUMERATE. */
static void PdbAlinhar4(Buf *b)
{
    static const char pad[3] = {(char)0xF3, (char)0xF2, (char)0xF1};
    uint32_t n = (4u - (uint32_t)(b->tam & 3u)) & 3u;

    if (n != 0)
        BufAnexarN(b, pad + (3 - n), n);
}

static size_t PdbRegAbrir(Buf *b, uint32_t leaf)
{
    size_t pos = b->tam;

    PdbU16(b, 0);                                     /* tamanho: remendado em PdbRegFechar */
    PdbU16(b, leaf);
    return pos;
}

/* Fecha o registro aberto em pos e devolve o TypeIndex dele (o tamanho gravado nao conta o proprio campo). */
static uint32_t PdbRegFechar(PdbTipos *t, size_t pos)
{
    size_t tam;

    PdbAlinhar4(&t->reg);
    tam = t->reg.tam - pos - 2;
    if (tam > 0xFFFFu)
        FalhaInterna("pdb: registro de tipo de %u bytes passa de 64 KiB", (unsigned)tam);
    t->reg.dados[pos] = (char)tam;
    t->reg.dados[pos + 1] = (char)(tam >> 8);
    return t->proximo++;
}

/* Numeric leaf: < 0x8000 direto; senao LF_ULONG (0x8004) ou LF_UQUADWORD (0x800A); negativos em LF_CHAR/SHORT/LONG/QUADWORD. */
static void PdbNumero(Buf *b, int64_t v)
{
    if (v >= 0 && v < 0x8000) {
        PdbU16(b, (uint32_t)v);
    } else if (v >= 0 && v <= 0xFFFFFFFFll) {
        PdbU16(b, 0x8004);
        PdbU32(b, (uint32_t)v);
    } else if (v >= 0) {
        PdbU16(b, 0x800A);
        PdbU32(b, (uint32_t)v);
        PdbU32(b, (uint32_t)((uint64_t)v >> 32));
    } else if (v >= -128) {
        PdbU16(b, 0x8000);
        BufAnexarN(b, (const char *)&(int8_t){(int8_t)v}, 1);
    } else if (v >= -32768) {
        PdbU16(b, 0x8001);
        PdbU16(b, (uint32_t)v);
    } else if (v >= INT32_MIN) {
        PdbU16(b, 0x8003);
        PdbU32(b, (uint32_t)v);
    } else {
        PdbU16(b, 0x8009);
        PdbU32(b, (uint32_t)v);
        PdbU32(b, (uint32_t)((uint64_t)v >> 32));
    }
}

static void PdbNome(Buf *b, const char *s)
{
    BufAnexarN(b, s, strlen(s) + 1);
}

static void PdbRegMembro(Buf *b, uint32_t tipo, uint32_t off, const char *nome)
{
    PdbU16(b, LF_MEMBER);
    PdbU16(b, CV_ATTR_PUBLICO);
    PdbU32(b, tipo);
    PdbNumero(b, off);
    PdbNome(b, nome);
    PdbAlinhar4(b);
}

/* ---- registros por classe ---- */

static void PdbLayout(PdbTipos *t)
{
    if (t->lay_pronto)
        return;
    LayoutCalcular(t->tipos, t->ir, &t->arena, &t->lay);
    t->lay_pronto = true;
}

static uint32_t PdbTamanho(PdbTipos *t, TipoId id)
{
    PdbLayout(t);
    return id < t->lay.qtd ? t->lay.tipos[id].tam : 0;
}

/* Ponteiro de 64 bits para o tipo alvo: simples sem modo vira 0x0600|T, o resto LF_POINTER. */
static uint32_t PdbPonteiroPara(PdbTipos *t, uint32_t alvo)
{
    size_t pos;

    if (alvo < CV_SIMPLES_MAX)
        return CV_PTR_64 | alvo;
    pos = PdbRegAbrir(&t->reg, LF_POINTER);
    PdbU32(&t->reg, alvo);
    PdbU32(&t->reg, CV_ATTR_PTR64);
    return PdbRegFechar(t, pos);
}

/* LF_ARRAY de elem com tam BYTES e nome vazio; o indice e T_UQUAD. */
static uint32_t PdbRegArray(PdbTipos *t, uint32_t elem, uint32_t tam)
{
    size_t pos = PdbRegAbrir(&t->reg, LF_ARRAY);

    PdbU32(&t->reg, elem);
    PdbU32(&t->reg, CV_T_UQUAD);
    PdbNumero(&t->reg, tam);
    PdbNome(&t->reg, "");
    return PdbRegFechar(t, pos);
}

static uint32_t PdbRegBytes(PdbTipos *t, TipoId id)
{
    return PdbRegArray(t, CV_T_U8, PdbTamanho(t, id));
}

/* Tipo de um membro ou apontado: o indice do tipo, ou void quando o tipo nao tem forma (vazio, veneno). */
static uint32_t PdbIndiceDe(PdbTipos *t, TipoId id)
{
    uint32_t idx = PdbTipoIndice(t, id);

    return idx != 0 ? idx : CV_T_VOID;
}

/* Dados de uma LF_STRUCTURE: nome, membros, o LF_FIELDLIST deles e o tamanho. */
typedef struct { const char *nome; uint32_t qtd, campos, tam; } PdbEstrutura;

static uint32_t PdbRegEstrutura(PdbTipos *t, const PdbEstrutura *e, uint32_t propriedade)
{
    size_t pos = PdbRegAbrir(&t->reg, LF_STRUCTURE);

    PdbU16(&t->reg, e->qtd);
    PdbU16(&t->reg, propriedade);
    PdbU32(&t->reg, e->campos);
    PdbU32(&t->reg, 0);                               /* derived */
    PdbU32(&t->reg, 0);                               /* vshape */
    PdbNumero(&t->reg, e->tam);
    PdbNome(&t->reg, e->nome);
    return PdbRegFechar(t, pos);
}

/* str, []T e [dynamic]T: membros ptr (aponta para o elemento), len e cap (int), alocador e epoca (u32), nos offsets do layout. */
static uint32_t PdbRegLista(PdbTipos *t, TipoId id, const char *nome, uint32_t qtd_max)
{
    const TipoInfo *info = TipoDe(t->tipos, id);
    TipoId elem = info->a;
    uint32_t qtd = LayoutQtdMembros(&t->lay, id);
    uint32_t ptr, campos;
    size_t pos;

    if (TipoClasseDe(t->tipos, id) == TC_TEXTO)
        elem = TIPO_U8;
    ptr = PdbPonteiroPara(t, PdbIndiceDe(t, elem));   /* o registro do ponteiro sai ANTES de abrir o fieldlist */
    qtd = qtd < qtd_max ? qtd : qtd_max;
    pos = PdbRegAbrir(&t->reg, LF_FIELDLIST);
    for (uint32_t k = 0; k < qtd; k++)
        PdbRegMembro(&t->reg, k == 0 ? ptr : k <= 2 ? CV_T_INT8 : CV_T_UINT4, LayoutOffsetCampo(&t->lay, id, k), NOMES_LISTA[k]);
    campos = PdbRegFechar(t, pos);
    return PdbRegEstrutura(t, &(PdbEstrutura){nome, qtd, campos, PdbTamanho(t, id)}, 0);
}

/* Resolve o tipo de cada membro (grava os registros que faltam) e estima o tamanho do LF_FIELDLIST. */
static uint32_t PdbResolverCampos(PdbTipos *t, const IrStruct *s)
{
    uint32_t total = 0;

    for (uint32_t k = 0; k < s->ncampos; k++) {
        PdbIndiceDe(t, s->campos[k].tipo);
        total += CV_TAM_MEMBRO_MAX + (uint32_t)strlen(InternerTexto(t->nomes, s->campos[k].nome));
    }
    return total;
}

/* struct com definicao na IR: membros pelo nome do campo, tipo e offset do layout. Opaca ou sem definicao, ou grande demais: bytes. */
static uint32_t PdbRegStruct(PdbTipos *t, TipoId id)
{
    const IrStruct *s;
    PdbEstrutura e;
    size_t pos;

    PdbLayout(t);
    s = LayoutEstrutura(t->tipos, t->ir, id);
    if (s == NULL || s->opaco || PdbResolverCampos(t, s) > CV_REG_MAX)
        return PdbRegBytes(t, id);
    pos = PdbRegAbrir(&t->reg, LF_FIELDLIST);
    for (uint32_t k = 0; k < s->ncampos; k++)
        PdbRegMembro(&t->reg, PdbIndiceDe(t, s->campos[k].tipo), LayoutOffsetCampo(&t->lay, id, k), InternerTexto(t->nomes, s->campos[k].nome));
    e.campos = PdbRegFechar(t, pos);
    e.nome = InternerTexto(t->nomes, (int32_t)TipoDe(t->tipos, id)->a);
    e.qtd = s->ncampos;
    e.tam = PdbTamanho(t, id);
    return PdbRegEstrutura(t, &e, 0);
}

static const IrEnum *PdbAcharEnum(const IrPrograma *ir, int32_t nome)
{
    for (uint32_t k = 0; k < ir->nenums; k++) {
        if (ir->enums[k].nome == nome)
            return &ir->enums[k];
    }
    return NULL;
}

/* enum: LF_FIELDLIST de LF_ENUMERATE (valor da IR ou o indice) e LF_ENUM com o tipo base pelo tamanho. Sem definicao na IR ou grande demais: bytes. */
static uint32_t PdbRegEnum(PdbTipos *t, TipoId id)
{
    int32_t nome = (int32_t)TipoDe(t->tipos, id)->a;
    const IrEnum *e = PdbAcharEnum(t->ir, nome);
    uint32_t tam = PdbTamanho(t, id), campos, base;
    size_t pos;

    if (e == NULL || (uint64_t)e->nmembros * CV_TAM_MEMBRO_MAX > CV_REG_MAX)
        return PdbRegBytes(t, id);
    pos = PdbRegAbrir(&t->reg, LF_FIELDLIST);
    for (uint32_t k = 0; k < e->nmembros; k++) {
        PdbU16(&t->reg, LF_ENUMERATE);
        PdbU16(&t->reg, CV_ATTR_PUBLICO);
        PdbNumero(&t->reg, e->valores != NULL ? e->valores[k] : (int64_t)k);
        PdbNome(&t->reg, InternerTexto(t->nomes, e->membros[k]));
        PdbAlinhar4(&t->reg);
    }
    campos = PdbRegFechar(t, pos);
    base = tam == 1 ? 0x68u : tam == 2 ? 0x72u : tam == 8 ? CV_T_INT8 : CV_T_INT4;
    pos = PdbRegAbrir(&t->reg, LF_ENUM);
    PdbU16(&t->reg, e->nmembros);
    PdbU16(&t->reg, 0);
    PdbU32(&t->reg, base);
    PdbU32(&t->reg, campos);
    PdbNome(&t->reg, InternerTexto(t->nomes, nome));
    return PdbRegFechar(t, pos);
}

/* [N]T: LF_ARRAY do elemento com o tamanho do layout em bytes. */
static uint32_t PdbRegVetor(PdbTipos *t, TipoId id)
{
    uint32_t elem = PdbIndiceDe(t, TipoDe(t->tipos, id)->a);

    return PdbRegArray(t, elem, PdbTamanho(t, id));
}

/* Struct no meio do proprio registro (ciclo): LF_STRUCTURE fwdref sem membros, uma vez por tipo (guardada na 2a metade do vetor). */
static uint32_t PdbRegAdiantado(PdbTipos *t, TipoId id)
{
    uint32_t *slot = &t->indice[t->ntipos + id];
    PdbEstrutura e = {0};

    if (*slot != 0)
        return *slot;
    if (TipoClasseDe(t->tipos, id) != TC_STRUCT)
        return CV_T_VOID;
    e.nome = InternerTexto(t->nomes, (int32_t)TipoDe(t->tipos, id)->a);
    *slot = PdbRegEstrutura(t, &e, CV_PROP_FWDREF);
    return *slot;
}

/* Despacho por classe: so o tipo nao simples chega aqui. */
static uint32_t PdbRegistrar(PdbTipos *t, TipoId id, TipoClasse c)
{
    switch (c) {
    case TC_PONTEIRO:
    case TC_ANULAVEL: return PdbPonteiroPara(t, PdbIndiceDe(t, TipoDe(t->tipos, id)->a));
    case TC_TEXTO: PdbLayout(t); return PdbRegLista(t, id, "str", 2);
    case TC_FATIA: PdbLayout(t); return PdbRegLista(t, id, "[]T", 2);
    case TC_DINAMICO: PdbLayout(t); return PdbRegLista(t, id, "[dynamic]T", PDB_MEMBROS_LISTA);
    case TC_ARRAY: return PdbRegVetor(t, id);
    case TC_STRUCT: return PdbRegStruct(t, id);
    case TC_ENUM: return PdbRegEnum(t, id);
    default: return PdbRegBytes(t, id);
    }
}

/* Contrato: TypeIndex de id (simples sem registro, ou o registro gravado agora/antes); 0 = tipo sem forma (vazio, veneno). Primeira chamada com tipo nao simples aloca o vetor. */
uint32_t PdbTipoIndice(PdbTipos *t, TipoId id)
{
    uint32_t simples = PdbTipoSimples(t->tipos, id);
    TipoClasse c;
    uint32_t *slot;

    if (simples != 0)
        return simples;
    c = TipoClasseDe(t->tipos, id);
    if (c == TC_VAZIO || c == TC_VENENO)
        return 0;
    if (t->indice == NULL) {
        t->ntipos = (uint32_t)t->tipos->itens.n;
        t->indice = ArenaAlocar(&t->arena, 2 * (size_t)t->ntipos * sizeof(uint32_t));
    }
    if (id >= t->ntipos)
        FalhaInterna("pdb: tipo %u fora da tabela de tipos (%u)", (unsigned)id, (unsigned)t->ntipos);
    slot = &t->indice[id];
    if (*slot >= PDB_PRIMEIRO_TIPO)
        return *slot;
    if (*slot == PDB_EM_CURSO)
        return PdbRegAdiantado(t, id);
    *slot = PDB_EM_CURSO;
    *slot = PdbRegistrar(t, id, c);
    return *slot;
}
