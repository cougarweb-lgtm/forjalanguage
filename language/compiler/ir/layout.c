/* layout.c - tamanho, alinhamento e offset dos tipos da Forja em C11, na ordem que o backend x64 usa (o emissor C que escrevia as structs saiu na 0.21.0).
 * Versao: 0.3.1
 * Changelog: 0.3.1 - G3 G0b: quebra de ciclo ir<->layout (movido de layout/ para ir/).
 *            0.3.0 - 0.21.0 F4 passo 2b: texto do cabecalho (o emit_c_tipos.c saiu); sem mudar codigo.
 *            0.2.0 - M2 P11b: LayoutTipo.membros (tamanho e alinhamento de cada membro) e LayoutConferir (coerencia interna dos layouts; X0906 com o tipo e o campo).
 *            0.1.0 - Linguagem v1.10 P1: escalares por tabela; composto calculado por membros em ordem C (struct, union,
 *                    resultado, opcional, tupla, array, matrix, fatia, str, lista, soa, map, bit_set).
 * Verificacao: python -I ..\tests\nativo\verificar_layout.py
 */
#include <stdio.h>

#include "ir/layout.h"

typedef struct { uint32_t tam, ali; } Forma;

/* Escalares C que aparecem como membro. x86-64: Win (-mabi=ms) e SysV dao o mesmo para estes tipos. */
static const Forma FORMA_NENHUMA = {0, 1};
static const Forma FORMA_UM_BYTE = {1, 1};
static const Forma FORMA_U32 = {4, 4};
static const Forma FORMA_I64 = {8, 8};
static const Forma FORMA_PONTEIRO = {8, 8};
/* forja_mapa: _Static_assert de 56 B em runtime/forja_rt.h. */
static const Forma FORMA_MAPA = {56, 8};

/* Tamanho dos escalares (alinhamento = tamanho); classe fora da tabela = 0 = sem forma. */
static const uint32_t TAMANHO_ESCALAR[TC_QTD] = {
    [TC_BOOL] = 1, [TC_I8] = 1, [TC_U8] = 1, [TC_I16] = 2, [TC_U16] = 2, [TC_I32] = 4, [TC_U32] = 4, [TC_REAL] = 4,
    [TC_ENUM] = 4, [TC_INT] = 8, [TC_UINT] = 8, [TC_F64] = 8, [TC_PONTEIRO] = 8, [TC_ANULAVEL] = 8, [TC_CSTR] = 8,
    [TC_RAWPTR] = 8, [TC_PROC] = 8,
};

typedef struct {
    const Tipos *tipos;
    const IrPrograma *prog;
    Layouts *saida;
    Arena *arena;
} LayoutCtx;

/* Acumulador dos membros de uma struct C: cada membro cai no proximo offset alinhado. ok = false se algum nao tem forma. */
typedef struct {
    LayoutTipo *dono;
    uint32_t n, fim, ali;
    bool ok;
} Membros;

typedef void (*CalcFn)(LayoutCtx *c, TipoId id);

static void Calcular(LayoutCtx *c, TipoId id);

static uint32_t AlinharPara(uint32_t valor, uint32_t ali)
{
    return (valor + ali - 1) / ali * ali;
}

static void FixarForma(LayoutCtx *c, TipoId id, Forma f)
{
    LayoutTipo *lt = &c->saida->tipos[id];

    lt->tam = f.tam;
    lt->ali = f.ali ? f.ali : 1;
    lt->qtd = 0;
    lt->offs = NULL;
    lt->membros = NULL;
    lt->pronto = true;
}

static void CalcularEscalar(LayoutCtx *c, TipoId id)
{
    uint32_t tam = TAMANHO_ESCALAR[TipoClasseDe(c->tipos, id)];

    FixarForma(c, id, (Forma){tam, tam});
}

static Forma FormaDe(LayoutCtx *c, TipoId id)
{
    const LayoutTipo *lt;

    Calcular(c, id);
    lt = &c->saida->tipos[id];
    return (Forma){lt->tam, lt->ali};
}

static void MembrosIniciar(Membros *m, LayoutCtx *c, TipoId id, uint32_t qtd)
{
    LayoutTipo *lt = &c->saida->tipos[id];

    lt->qtd = qtd;
    lt->offs = ArenaAlocar(c->arena, qtd * sizeof(uint32_t));
    lt->membros = ArenaAlocar(c->arena, qtd * sizeof(LayoutMembro));
    m->dono = lt;
    m->n = 0;
    m->fim = 0;
    m->ali = 1;
    m->ok = true;
}

static void MembroForma(Membros *m, Forma f)
{
    m->fim = AlinharPara(m->fim, f.ali);
    m->dono->membros[m->n] = (LayoutMembro){f.tam, f.ali};
    m->dono->offs[m->n++] = m->fim;
    m->fim += f.tam;
    if (f.ali > m->ali) m->ali = f.ali;
    if (f.tam == 0) m->ok = false;
}

static void MembroDeTipo(Membros *m, LayoutCtx *c, TipoId id)
{
    MembroForma(m, FormaDe(c, id));
}

/* Fecha a struct: tamanho = fim arredondado para o maior alinhamento. Um membro sem forma deixa o tipo sem forma. */
static void MembrosFechar(Membros *m)
{
    LayoutTipo *lt = m->dono;

    lt->pronto = true;
    if (!m->ok) {
        lt->tam = 0;
        lt->ali = 1;
        return;
    }
    lt->tam = AlinharPara(m->fim, m->ali);
    lt->ali = m->ali;
}

/* Membros de [dynamic]T e de soa[dynamic]S depois do primeiro: len, cap, alocador e epoca. */
static void MembrosDeLista(Membros *m)
{
    MembroForma(m, FORMA_I64);
    MembroForma(m, FORMA_I64);
    MembroForma(m, FORMA_U32);
    MembroForma(m, FORMA_U32);
}

/* Union C: a union interna tem o tamanho da maior variante, arredondado para o maior alinhamento. */
static Forma FormaDaUniao(LayoutCtx *c, const IrUniao *u)
{
    Forma f = {0, 1};

    for (uint32_t k = 0; k < u->nvariantes; k++) {
        Forma v = FormaDe(c, u->variantes[k]);

        if (v.tam == 0) return FORMA_NENHUMA;
        if (v.tam > f.tam) f.tam = v.tam;
        if (v.ali > f.ali) f.ali = v.ali;
    }
    f.tam = AlinharPara(f.tam, f.ali);
    return f;
}

static void CalcularStruct(LayoutCtx *c, TipoId id)
{
    const IrStruct *s = LayoutEstrutura(c->tipos, c->prog, id);
    Membros m;

    if (s == NULL || s->opaco) {
        FixarForma(c, id, FORMA_NENHUMA);
        return;
    }
    MembrosIniciar(&m, c, id, s->ncampos == 0 ? 1 : s->ncampos);
    if (s->ncampos == 0) MembroForma(&m, FORMA_UM_BYTE); /* struct vazia sai como 'char vazio_' */
    for (uint32_t k = 0; k < s->ncampos; k++) MembroDeTipo(&m, c, s->campos[k].tipo);
    MembrosFechar(&m);
}

static void CalcularUniao(LayoutCtx *c, TipoId id)
{
    const IrUniao *u = LayoutUniao(c->tipos, c->prog, id);
    Membros m;

    if (u == NULL) {
        FixarForma(c, id, FORMA_NENHUMA);
        return;
    }
    MembrosIniciar(&m, c, id, 2);
    MembroForma(&m, FORMA_U32);
    MembroForma(&m, FormaDaUniao(c, u));
    MembrosFechar(&m);
}

/* T ! E = {bool e_erro; T valor; E erro;}; sem 'valor' quando T e vazio. */
static void CalcularResultado(LayoutCtx *c, TipoId id)
{
    const TipoInfo *info = TipoDe(c->tipos, id);
    bool tem_valor = TipoClasseDe(c->tipos, info->a) != TC_VAZIO;
    Membros m;

    MembrosIniciar(&m, c, id, tem_valor ? 3 : 2);
    MembroForma(&m, FORMA_UM_BYTE);
    if (tem_valor) MembroDeTipo(&m, c, info->a);
    MembroDeTipo(&m, c, info->b);
    MembrosFechar(&m);
}

/* ?T = {bool tem; T v;}; ?cstr e nicho: const char * com NULL = none. */
static void CalcularOpcional(LayoutCtx *c, TipoId id)
{
    Membros m;

    if (TipoOpcionalNicho(c->tipos, id)) {
        FixarForma(c, id, FORMA_PONTEIRO);
        return;
    }
    MembrosIniciar(&m, c, id, 2);
    MembroForma(&m, FORMA_UM_BYTE);
    MembroDeTipo(&m, c, TipoDe(c->tipos, id)->a);
    MembrosFechar(&m);
}

static void CalcularTupla(LayoutCtx *c, TipoId id)
{
    uint32_t n = TipoTuplaQtd(c->tipos, id);
    Membros m;

    MembrosIniciar(&m, c, id, n);
    for (uint32_t k = 0; k < n; k++) MembroDeTipo(&m, c, TipoTuplaElemento(c->tipos, id, k));
    MembrosFechar(&m);
}

/* Struct com um unico membro T X[n]: o [N]T (membro v) e a matrix (membro m). */
static void CalcularVetorUnico(LayoutCtx *c, TipoId id, TipoId elemento, uint32_t n)
{
    Forma e = FormaDe(c, elemento);
    Membros m;

    MembrosIniciar(&m, c, id, 1);
    MembroForma(&m, (Forma){e.tam * n, e.ali});
    MembrosFechar(&m);
}

static void CalcularArray(LayoutCtx *c, TipoId id)
{
    const TipoInfo *info = TipoDe(c->tipos, id);

    CalcularVetorUnico(c, id, info->a, info->b);
}

static void CalcularMatriz(LayoutCtx *c, TipoId id)
{
    uint32_t n = TipoMatrizLinhas(c->tipos, id) * TipoMatrizColunas(c->tipos, id);

    CalcularVetorUnico(c, id, TipoDe(c->tipos, id)->a, n);
}

/* []T e str: {ptr, len}. */
static void CalcularPonteiroELen(LayoutCtx *c, TipoId id)
{
    Membros m;

    MembrosIniciar(&m, c, id, 2);
    MembroForma(&m, FORMA_PONTEIRO);
    MembroForma(&m, FORMA_I64);
    MembrosFechar(&m);
}

/* [dynamic]T = {ptr, len, cap, alocador, epoca}. */
static void CalcularDinamico(LayoutCtx *c, TipoId id)
{
    Membros m;

    MembrosIniciar(&m, c, id, 5);
    MembroForma(&m, FORMA_PONTEIRO);
    MembrosDeLista(&m);
    MembrosFechar(&m);
}

static void CalcularMapa(LayoutCtx *c, TipoId id)
{
    FixarForma(c, id, FORMA_MAPA);
}

/* bit_set: uint8_t, uint16_t, uint32_t ou uint64_t pelo numero de membros. */
static uint32_t TamanhoBitSet(uint32_t membros)
{
    if (membros <= 8) return 1;
    if (membros <= 16) return 2;
    if (membros <= 32) return 4;
    return 8;
}

static void CalcularBitSet(LayoutCtx *c, TipoId id)
{
    uint32_t tam = TamanhoBitSet(TipoDe(c->tipos, id)->b);

    FixarForma(c, id, (Forma){tam, tam});
}

/* soa[N]S: uma struct com f_<campo>[N] por coluna. */
static void CalcularSoaFixa(LayoutCtx *c, TipoId id, const IrStruct *s, uint32_t n)
{
    Membros m;

    MembrosIniciar(&m, c, id, s->ncampos);
    for (uint32_t k = 0; k < s->ncampos; k++) {
        Forma e = FormaDe(c, s->campos[k].tipo);

        MembroForma(&m, (Forma){e.tam * n, e.ali});
    }
    MembrosFechar(&m);
}

/* soa[dynamic]S = {void *col[ncampos]; len; cap; alocador; epoca}: as colunas sao ponteiros. */
static void CalcularSoaDinamica(LayoutCtx *c, TipoId id, const IrStruct *s)
{
    Membros m;

    MembrosIniciar(&m, c, id, 5);
    MembroForma(&m, (Forma){8 * s->ncampos, 8});
    MembrosDeLista(&m);
    MembrosFechar(&m);
}

static void CalcularSoa(LayoutCtx *c, TipoId id)
{
    TipoId base = TipoDe(c->tipos, id)->a;
    const IrStruct *s = LayoutEstrutura(c->tipos, c->prog, TipoDe(c->tipos, base)->a);

    if (s == NULL || s->opaco) {
        FixarForma(c, id, FORMA_NENHUMA);
        return;
    }
    if (TipoClasseDe(c->tipos, base) == TC_ARRAY) {
        CalcularSoaFixa(c, id, s, TipoDe(c->tipos, base)->b);
        return;
    }
    CalcularSoaDinamica(c, id, s);
}

/* Classe com calculo proprio; as demais (escalares) saem da tabela TAMANHO_ESCALAR. */
static const CalcFn CALCULO_DE[TC_QTD] = {
    [TC_STRUCT] = CalcularStruct,
    [TC_UNIAO] = CalcularUniao,
    [TC_RESULTADO] = CalcularResultado,
    [TC_OPCIONAL] = CalcularOpcional,
    [TC_TUPLA] = CalcularTupla,
    [TC_ARRAY] = CalcularArray,
    [TC_MATRIZ] = CalcularMatriz,
    [TC_FATIA] = CalcularPonteiroELen,
    [TC_TEXTO] = CalcularPonteiroELen,
    [TC_DINAMICO] = CalcularDinamico,
    [TC_SOA] = CalcularSoa,
    [TC_MAPA] = CalcularMapa,
    [TC_BIT_SET] = CalcularBitSet,
};

static void Calcular(LayoutCtx *c, TipoId id)
{
    CalcFn calcular;

    if (c->saida->tipos[id].pronto) return;
    calcular = CALCULO_DE[TipoClasseDe(c->tipos, id)];
    if (calcular == NULL) CalcularEscalar(c, id);
    else calcular(c, id);
}

const IrStruct *LayoutEstrutura(const Tipos *t, const IrPrograma *p, TipoId id)
{
    int32_t nome = (int32_t)TipoDe(t, id)->a;

    for (uint32_t k = 0; k < p->nstructs; k++) {
        if (p->structs[k].nome == nome) return &p->structs[k];
    }
    return NULL;
}

const IrUniao *LayoutUniao(const Tipos *t, const IrPrograma *p, TipoId id)
{
    int32_t nome = (int32_t)TipoDe(t, id)->a;

    for (uint32_t k = 0; k < p->nunioes; k++) {
        if (p->unioes[k].nome == nome) return &p->unioes[k];
    }
    return NULL;
}

void LayoutCalcular(const Tipos *t, const IrPrograma *p, Arena *a, Layouts *saida)
{
    LayoutCtx c = {t, p, saida, a};

    saida->qtd = (uint32_t)t->itens.n;
    saida->tipos = ArenaAlocar(a, saida->qtd * sizeof(LayoutTipo));
    for (TipoId id = 1; id < saida->qtd; id++) Calcular(&c, id);
}

static const LayoutTipo *LayoutDe(const Layouts *l, TipoId id)
{
    if (id >= l->qtd) FalhaInterna("layout: tipo %u fora da tabela", (unsigned)id);
    return &l->tipos[id];
}

uint32_t LayoutTamanho(const Layouts *l, TipoId id)
{
    const LayoutTipo *lt = LayoutDe(l, id);

    if (lt->tam == 0) FalhaInterna("layout: tipo %u sem forma em C", (unsigned)id);
    return lt->tam;
}

uint32_t LayoutAlinhamento(const Layouts *l, TipoId id)
{
    const LayoutTipo *lt = LayoutDe(l, id);

    if (lt->tam == 0) FalhaInterna("layout: tipo %u sem forma em C", (unsigned)id);
    return lt->ali;
}

uint32_t LayoutOffsetCampo(const Layouts *l, TipoId id, uint32_t k)
{
    const LayoutTipo *lt = LayoutDe(l, id);

    if (k >= lt->qtd) FalhaInterna("layout: tipo %u sem o membro %u", (unsigned)id, (unsigned)k);
    return lt->offs[k];
}

uint32_t LayoutQtdMembros(const Layouts *l, TipoId id)
{
    return LayoutDe(l, id)->qtd;
}

/* ---- LayoutConferir ---- */

#define CONFERIR_NOME_MAX 96

/* O tipo em conferencia: o nome legivel (para a causa do X0906) e onde relatar. */
typedef struct {
    const LayoutTipo *lt;
    Diag *d;
    char nome[CONFERIR_NOME_MAX];
} Conferencia;

static bool PotenciaDeDois(uint32_t n)
{
    return n != 0 && (n & (n - 1u)) == 0;
}

static bool MembroCoerente(const Conferencia *cf, uint32_t k)
{
    const LayoutMembro *m = &cf->lt->membros[k];
    uint32_t off = cf->lt->offs[k];

    if (!PotenciaDeDois(m->ali) || off % m->ali != 0)
        return DiagRelatar(cf->d, X0906, 0, "layout de '%s', campo %u: offset %u B nao e multiplo do alinhamento %u B (potencia de 2)", cf->nome,
                           (unsigned)k, (unsigned)off, (unsigned)m->ali);
    if (off + m->tam > cf->lt->tam)
        return DiagRelatar(cf->d, X0906, 0, "layout de '%s', campo %u: offset %u B + %u B passa do tamanho %u B", cf->nome, (unsigned)k, (unsigned)off,
                           (unsigned)m->tam, (unsigned)cf->lt->tam);
    return true;
}

static bool TipoCoerente(Conferencia *cf)
{
    if (!PotenciaDeDois(cf->lt->ali) || cf->lt->tam % cf->lt->ali != 0)
        return DiagRelatar(cf->d, X0906, 0, "layout de '%s': tamanho %u B incoerente com o alinhamento %u B (potencia de 2 que divide o tamanho)", cf->nome,
                           (unsigned)cf->lt->tam, (unsigned)cf->lt->ali);
    for (uint32_t k = 0; k < cf->lt->qtd; k++) {
        if (!MembroCoerente(cf, k))
            return false;
    }
    return true;
}

static void NomeLegivel(const Tipos *t, TipoId id, Conferencia *cf)
{
    Buf texto = {0};

    TipoAnexarTexto(t, id, &texto);
    snprintf(cf->nome, sizeof cf->nome, "%s", texto.dados != NULL ? texto.dados : "?");
    BufLiberar(&texto);
}

bool LayoutConferir(const Layouts *l, const Tipos *t, Diag *d)
{
    Conferencia cf = {.d = d};

    for (TipoId id = 1; id < l->qtd; id++) {
        cf.lt = &l->tipos[id];
        if (cf.lt->tam == 0)
            continue; /* sem forma em C */
        NomeLegivel(t, id, &cf);
        if (!TipoCoerente(&cf))
            return false;
    }
    return true;
}
