/* tipos.c - tabela de tipos com hash-consing e texto legivel dos tipos.
 * Versao: 0.10.1
 * Changelog: 0.10.1 - Linguagem v1.12 M3 P21v-a: o nome do parametro de proc vai em TipoInfo.c da propria celula da cauda (ObterNome); sem celula extra os TipoId e o C emitido nao mudam.
 *            0.10.0 - Linguagem v1.12 M3 P21v-a: TipoProc(ProcAssinatura) guarda o nome de cada parametro; TipoProcNome e TipoProcCompativel (sem nomes).
 *            0.9.0 -Linguagem v1.11 L15a: TipoListaFixa (struct oculta de [dynamic, N]T, b = TipoId do [N]T) e o texto "[dynamic, N]T".
 *            0.8.0 - Linguagem v1.9 P3: TipoMapa, TipoBitSet, TipoSoa, TipoMatriz; flags tem_mapa/tem_bit_set/tem_soa/tem_matriz; texto map[K]V, bit_set[E], soa[N]S, matrix[R, C]T.
 *            0.7.0 - Linguagem v1.8b P5a: TipoProc, TipoProcQtd, TipoProcParam e o texto "proc(rawptr) -> i32".
 *            0.6.0 - Linguagem v1.7 P2: TipoOpcional, TipoUniao, TipoTupla (+Qtd/Elemento); tem_opcional/tem_uniao zerados em TiposIniciar; texto de ?T, union e tupla.
 *            0.5.1 - Linguagem v1.6 P4 (desempenho): TiposIniciar zera tem_dinamico; TipoDinamico o liga.
 *            0.5.0 - Linguagem v1.6 P2: TipoDinamico e o texto "[dynamic]int".
 *            0.4.0 - Linguagem v1.5 P2: TipoArray, TipoFatia e o texto "[3]int" / "[]Grifo".
 *            0.3.0 - Linguagem v1.3: ids 15 (rawptr) e 16 (cstr) em TiposIniciar; TipoAnulavel; nomes "str", "rawptr", "cstr"; texto de '?^T'.
 *            0.2.0 - Linguagem v1.2: 7 classes numericas novas, largura/faixa por id fixo, leitura e conferencia de literais.
 *            0.1.0 - Fase 5c: primeira versao.
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/tipos.h"

#include "common/nome_modulo.h"
#include "frontend/lexer.h"

#include <stdlib.h>

enum { CAP_INICIAL = 64 };

static uint32_t HashTipo(const TipoInfo *k)
{
    uint32_t h = 2166136261u;
    h = (h ^ k->classe) * 16777619u;
    h = (h ^ k->a) * 16777619u;
    h = (h ^ k->b) * 16777619u;
    return (h ^ k->c) * 16777619u;
}

static uint32_t *AlocarTabela(uint32_t cap)
{
    uint32_t *tabela = calloc(cap, sizeof *tabela);
    if (tabela == NULL) FalhaInterna("memoria esgotada na tabela de tipos");
    return tabela;
}

static void Crescer(Tipos *t)
{
    uint32_t cap = t->cap * 2;
    uint32_t *nova = AlocarTabela(cap);
    for (size_t id = 1; id < t->itens.n; id++) {
        uint32_t i = HashTipo(TipoDe(t, (TipoId)id)) & (cap - 1);
        while (nova[i] != 0) i = (i + 1) & (cap - 1);
        nova[i] = (uint32_t)id;
    }
    free(t->tabela);
    t->tabela = nova;
    t->cap = cap;
}

/* TipoId de (classe, a, b, c); cria se for novo. */
static TipoId ObterNome(Tipos *t, TipoClasse classe, uint32_t a, uint32_t b, uint32_t c)
{
    TipoInfo k = {(uint8_t)classe, a, b, c};
    uint32_t mascara = t->cap - 1, i = HashTipo(&k) & mascara;
    while (t->tabela[i] != 0) {
        const TipoInfo *x = TipoDe(t, t->tabela[i]);
        if (x->classe == k.classe && x->a == a && x->b == b && x->c == c) return t->tabela[i];
        i = (i + 1) & mascara;
    }
    *(TipoInfo *)VecEmpurrar(&t->itens) = k;
    TipoId novo = (TipoId)(t->itens.n - 1);
    t->tabela[i] = novo;
    if (t->itens.n * 2 > t->cap) Crescer(t);
    return novo;
}

static TipoId Obter(Tipos *t, TipoClasse classe, uint32_t a, uint32_t b) { return ObterNome(t, classe, a, b, 0); }

void TiposIniciar(Tipos *t, const Interner *nomes, const DimTabela *dims)
{
    VecIniciar(&t->itens, sizeof(TipoInfo));
    VecEmpurrar(&t->itens); /* slot 0 = nao resolvido */
    t->cap = CAP_INICIAL;
    t->tabela = AlocarTabela(t->cap);
    t->nomes = nomes;
    t->dims = dims;
    t->tem_dinamico = false;
    t->tem_opcional = false;
    t->tem_uniao = false;
    t->tem_mapa = false;
    t->tem_bit_set = false;
    t->tem_soa = false;
    t->tem_matriz = false;
    Obter(t, TC_VAZIO, 0, 0);
    Obter(t, TC_BOOL, 0, 0);
    Obter(t, TC_I32, 0, 0);
    Obter(t, TC_INT, 0, 0);
    Obter(t, TC_REAL, DIM_ADIMENSIONAL, 0);
    Obter(t, TC_TEXTO, 0, 0);
    Obter(t, TC_VENENO, 0, 0);
    /* v1.2: a ordem aqui fixa os ids 8..14 (TIPO_I8..TIPO_F64); v1.3: 15 TIPO_RAWPTR e 16 TIPO_CSTR */
    static const TipoClasse NOVAS[] = {TC_I8, TC_I16, TC_U8, TC_U16, TC_U32, TC_UINT, TC_F64, TC_RAWPTR, TC_CSTR};
    for (size_t k = 0; k < sizeof NOVAS / sizeof NOVAS[0]; k++) {
        if (Obter(t, NOVAS[k], 0, 0) != (TipoId)(TIPO_I8 + k)) FalhaInterna("tipos: id fixo fora de ordem");
    }
}

void TiposLiberar(Tipos *t)
{
    free(t->tabela);
    VecLiberar(&t->itens);
    t->tabela = NULL;
}

TipoId TipoReal(Tipos *t, DimId dim) { return Obter(t, TC_REAL, dim, 0); }
TipoId TipoStruct(Tipos *t, int32_t nome) { return Obter(t, TC_STRUCT, (uint32_t)nome, 0); }
TipoId TipoListaFixa(Tipos *t, int32_t nome, TipoId array) { return Obter(t, TC_STRUCT, (uint32_t)nome, array); }
TipoId TipoEnum(Tipos *t, int32_t nome) { return Obter(t, TC_ENUM, (uint32_t)nome, 0); }
TipoId TipoPonteiro(Tipos *t, TipoId apontado) { return Obter(t, TC_PONTEIRO, apontado, 0); }
TipoId TipoAnulavel(Tipos *t, TipoId apontado) { return Obter(t, TC_ANULAVEL, apontado, 0); }
TipoId TipoResultado(Tipos *t, TipoId valor, TipoId erro) { return Obter(t, TC_RESULTADO, valor, erro); }
TipoId TipoArray(Tipos *t, TipoId elemento, uint32_t n) { return Obter(t, TC_ARRAY, elemento, n); }
TipoId TipoFatia(Tipos *t, TipoId elemento) { return Obter(t, TC_FATIA, elemento, 0); }
TipoId TipoDinamico(Tipos *t, TipoId elemento)
{
    t->tem_dinamico = true;
    return Obter(t, TC_DINAMICO, elemento, 0);
}

TipoId TipoOpcional(Tipos *t, TipoId valor)
{
    t->tem_opcional = true;
    return Obter(t, TC_OPCIONAL, valor, 0);
}

TipoId TipoMapa(Tipos *t, TipoId chave, TipoId valor)
{
    t->tem_mapa = true;
    t->tem_dinamico = true;
    return Obter(t, TC_MAPA, chave, valor);
}

TipoId TipoBitSet(Tipos *t, TipoId enumerado, uint32_t membros)
{
    t->tem_bit_set = true;
    return Obter(t, TC_BIT_SET, enumerado, membros);
}

TipoId TipoSoa(Tipos *t, TipoId base)
{
    t->tem_soa = true;
    if (TipoClasseDe(t, base) == TC_DINAMICO) t->tem_dinamico = true;
    return Obter(t, TC_SOA, base, 0);
}

TipoId TipoMatriz(Tipos *t, TipoId elemento, uint32_t linhas, uint32_t colunas)
{
    t->tem_matriz = true;
    return Obter(t, TC_MATRIZ, elemento, (linhas << 8) | colunas);
}

TipoId TipoUniao(Tipos *t, int32_t nome)
{
    t->tem_uniao = true;
    return Obter(t, TC_UNIAO, (uint32_t)nome, 0);
}

/* Tupla de n >= 2 elementos: cauda encadeada de tras para a frente (hash-consing: mesma lista, mesmo TipoId). */
TipoId TipoTupla(Tipos *t, const TipoId *elementos, uint32_t n)
{
    TipoId cauda = 0;
    for (uint32_t k = n - 1; k >= 1; k--) cauda = Obter(t, TC_TUPLA_CAUDA, elementos[k], cauda);
    return Obter(t, TC_TUPLA, elementos[0], cauda);
}

uint32_t TipoTuplaQtd(const Tipos *t, TipoId tupla)
{
    uint32_t n = 1;
    for (uint32_t c = TipoDe(t, tupla)->b; c != 0; c = TipoDe(t, c)->b) n++;
    return n;
}

TipoId TipoTuplaElemento(const Tipos *t, TipoId tupla, uint32_t k)
{
    const TipoInfo *info = TipoDe(t, tupla);
    if (k == 0) return info->a;
    uint32_t c = info->b;
    for (; k > 1; k--) c = TipoDe(t, c)->b;
    return TipoDe(t, c)->a;
}

/* Tipo de callback: a = retorno, b = cauda dos parametros (mesma cadeia da tupla, mas aceita n = 0). */
TipoId TipoProc(Tipos *t, const ProcAssinatura *a)
{
    TipoId cauda = 0;
    for (uint32_t k = a->n; k >= 1; k--) {
        uint32_t nome = a->nomes != NULL ? (uint32_t)a->nomes[k - 1] : UINT32_MAX; /* -1 = sem nome */
        cauda = ObterNome(t, TC_TUPLA_CAUDA, a->params[k - 1], cauda, nome);
    }
    return Obter(t, TC_PROC, a->retorno, cauda);
}

uint32_t TipoProcQtd(const Tipos *t, TipoId proc)
{
    uint32_t n = 0;
    for (uint32_t c = TipoDe(t, proc)->b; c != 0; c = TipoDe(t, c)->b) n++;
    return n;
}

/* A celula da cauda do k-esimo parametro: a = tipo, b = a proxima celula, c = id do nome. */
static const TipoInfo *ParamDoProc(const Tipos *t, TipoId proc, uint32_t k)
{
    uint32_t c = TipoDe(t, proc)->b;
    for (; k > 0; k--) c = TipoDe(t, c)->b;
    return TipoDe(t, c);
}

TipoId TipoProcParam(const Tipos *t, TipoId proc, uint32_t k) { return ParamDoProc(t, proc, k)->a; }
int32_t TipoProcNome(const Tipos *t, TipoId proc, uint32_t k) { return (int32_t)ParamDoProc(t, proc, k)->c; }

bool TipoProcCompativel(const Tipos *t, TipoId a, TipoId b)
{
    if (a == b) return true;
    if (TipoClasseDe(t, a) != TC_PROC || TipoClasseDe(t, b) != TC_PROC) return false;
    uint32_t n = TipoProcQtd(t, a);
    if (n != TipoProcQtd(t, b) || !TipoProcCompativel(t, (TipoId)TipoDe(t, a)->a, (TipoId)TipoDe(t, b)->a)) return false;
    for (uint32_t k = 0; k < n; k++) {
        if (!TipoProcCompativel(t, TipoProcParam(t, a, k), TipoProcParam(t, b, k))) return false;
    }
    return true;
}

static const char *const NOME_FIXO[TC_QTD] = {
    [TC_VAZIO] = "(sem valor)", [TC_BOOL] = "bool", [TC_I32] = "i32", [TC_INT] = "int", [TC_TEXTO] = "str",
    [TC_VENENO] = "(erro)", [TC_I8] = "i8", [TC_I16] = "i16", [TC_U8] = "u8", [TC_U16] = "u16", [TC_U32] = "u32",
    [TC_UINT] = "uint", [TC_F64] = "f64", [TC_RAWPTR] = "rawptr", [TC_CSTR] = "cstr",
    [TC_MAPA] = NULL, [TC_BIT_SET] = NULL, [TC_SOA] = NULL, [TC_MATRIZ] = NULL,
};

/* Tabelas por id fixo (1..16); o indice 0 e o "nao resolvido". LARGURA_FIXA de rawptr/cstr = 0 (nao sao numericos). */
static const uint8_t LARGURA_FIXA[TIPO_FIXOS_QTD] = {
    [TIPO_I32] = 32, [TIPO_INT] = 64, [TIPO_F32] = 32, [TIPO_I8] = 8, [TIPO_I16] = 16, [TIPO_U8] = 8, [TIPO_U16] = 16,
    [TIPO_U32] = 32, [TIPO_UINT] = 64, [TIPO_F64] = 64,
};
static const char *const NOME_DO_ID[TIPO_FIXOS_QTD] = {
    [TIPO_VAZIO] = "(sem valor)", [TIPO_BOOL] = "bool", [TIPO_I32] = "i32", [TIPO_INT] = "int", [TIPO_F32] = "f32",
    [TIPO_TEXTO] = "str", [TIPO_VENENO] = "(erro)", [TIPO_I8] = "i8", [TIPO_I16] = "i16", [TIPO_U8] = "u8",
    [TIPO_U16] = "u16", [TIPO_U32] = "u32", [TIPO_UINT] = "uint", [TIPO_F64] = "f64", [TIPO_RAWPTR] = "rawptr",
    [TIPO_CSTR] = "cstr",
};

uint8_t TipoLarguraBits(const Tipos *t, TipoId id)
{
    if (id < TIPO_FIXOS_QTD) return LARGURA_FIXA[id];
    return TipoClasseDe(t, id) == TC_REAL ? 32 : 0;
}

const char *TipoNomeFixo(TipoId id) { return id < TIPO_FIXOS_QTD ? NOME_DO_ID[id] : NULL; }

/* Max por id fixo; so os ids inteiros (3, 4 e 8..13) sao consultados. */
uint64_t TipoInteiroMax(TipoId id)
{
    switch (id) {
    case TIPO_I8: return 127u;
    case TIPO_I16: return 32767u;
    case TIPO_I32: return 2147483647u;
    case TIPO_INT: return (uint64_t)INT64_MAX;
    case TIPO_U8: return 255u;
    case TIPO_U16: return 65535u;
    case TIPO_U32: return 4294967295u;
    default: return UINT64_MAX; /* TIPO_UINT */
    }
}

int64_t TipoInteiroMin(TipoId id)
{
    switch (id) {
    case TIPO_I8: return INT8_MIN;
    case TIPO_I16: return INT16_MIN;
    case TIPO_I32: return INT32_MIN;
    case TIPO_INT: return INT64_MIN;
    default: return 0;
    }
}

bool TipoLerLiteralU64(const char *digitos, uint64_t *valor)
{
    return LexerLerInteiroU64(digitos, valor); /* v1.10: decimal, 0x... e 0b... */
}

bool TipoInteiroCabe(TipoId tipo, bool negativo, uint64_t magnitude)
{
    if (TipoEhSemSinal(tipo)) return negativo ? magnitude == 0 : magnitude <= TipoInteiroMax(tipo);
    /* com sinal: o menor valor tem uma unidade a mais de modulo que o maior */
    return negativo ? magnitude <= TipoInteiroMax(tipo) + 1u : magnitude <= TipoInteiroMax(tipo);
}

bool TipoBitsCabem(TipoId tipo, int64_t bits)
{
    if (!TipoEhInteiro(tipo)) return true;
    if (TipoEhSemSinal(tipo)) return (uint64_t)bits <= TipoInteiroMax(tipo);
    return bits >= TipoInteiroMin(tipo) && (bits < 0 || (uint64_t)bits <= TipoInteiroMax(tipo));
}

static void TextoNome(const Tipos *t, int32_t nome, Buf *saida)
{
    const char *puro = NomeModuloExibicao(InternerTexto(t->nomes, nome)); /* IA1-L8: tipo de modulo aparece com o nome puro */
    BufAnexarN(saida, puro, strlen(puro));
}

static void TextoReal(const Tipos *t, DimId dim, Buf *saida)
{
    int32_t nome = DimNome(t->dims, dim);
    if (dim == DIM_ADIMENSIONAL) BufAnexar(saida, "f32");
    else if (nome < 0) BufAnexar(saida, "(unidade sem nome)");
    else TextoNome(t, nome, saida);
}

/* v1.11 L15a: a struct oculta da lista de capacidade fixa escreve o tipo como o programa o escreveu: [dynamic, N]T. */
static void TextoListaFixa(const Tipos *t, const TipoInfo *k, Buf *saida)
{
    const TipoInfo *itens = TipoDe(t, k->b);
    BufAnexar(saida, "[dynamic, ");
    BufAnexarU64(saida, itens->b);
    BufAnexar(saida, "]");
    TipoAnexarTexto(t, itens->a, saida);
}

static void TextoResultado(const Tipos *t, const TipoInfo *k, Buf *saida)
{
    if (k->a != TIPO_VAZIO) {
        TipoAnexarTexto(t, k->a, saida);
        BufAnexar(saida, " ");
    }
    BufAnexar(saida, "! ");
    TipoAnexarTexto(t, k->b, saida);
}

/* Texto de '?T', union e tupla "(A, B)"; a cauda da tupla nunca aparece sozinha. */
static void TextoDaV17(const Tipos *t, const TipoInfo *k, Buf *saida)
{
    if (k->classe == TC_OPCIONAL) {
        BufAnexar(saida, "?");
        TipoAnexarTexto(t, k->a, saida);
    } else if (k->classe == TC_UNIAO) TextoNome(t, (int32_t)k->a, saida);
    else if (k->classe == TC_TUPLA) {
        BufAnexar(saida, "(");
        TipoAnexarTexto(t, k->a, saida);
        for (uint32_t c = k->b; c != 0; c = TipoDe(t, c)->b) {
            BufAnexar(saida, ", ");
            TipoAnexarTexto(t, TipoDe(t, c)->a, saida);
        }
        BufAnexar(saida, ")");
    } else if (k->classe == TC_PROC) {
        BufAnexar(saida, "proc(");
        for (uint32_t c = k->b; c != 0; c = TipoDe(t, c)->b) {
            TipoAnexarTexto(t, TipoDe(t, c)->a, saida);
            if (TipoDe(t, c)->b != 0) BufAnexar(saida, ", ");
        }
        BufAnexar(saida, ")");
        if (k->a != TIPO_VAZIO) {
            BufAnexar(saida, " -> ");
            TipoAnexarTexto(t, k->a, saida);
        }
    } else FalhaInterna("tipos: TC_TUPLA_CAUDA nao tem texto proprio");
}

/* Texto de map[K]V, bit_set[E], soa[N]S ("soa" + texto do base) e matrix[R, C]T. */
static void TextoDaV19(const Tipos *t, const TipoInfo *k, Buf *saida)
{
    if (k->classe == TC_MAPA) {
        BufAnexar(saida, "map[");
        TipoAnexarTexto(t, k->a, saida);
        BufAnexar(saida, "]");
        TipoAnexarTexto(t, k->b, saida);
    } else if (k->classe == TC_BIT_SET) {
        BufAnexar(saida, "bit_set[");
        TipoAnexarTexto(t, k->a, saida);
        BufAnexar(saida, "]");
    } else if (k->classe == TC_SOA) {
        BufAnexar(saida, "soa");
        TipoAnexarTexto(t, k->a, saida);
    } else if (k->classe == TC_MATRIZ) {
        BufAnexar(saida, "matrix[");
        BufAnexarU64(saida, k->b >> 8);
        BufAnexar(saida, ", ");
        BufAnexarU64(saida, k->b & 0xFFu);
        BufAnexar(saida, "]");
        TipoAnexarTexto(t, k->a, saida);
    } else FalhaInterna("tipos: classe sem texto");
}

void TipoAnexarTexto(const Tipos *t, TipoId id, Buf *saida)
{
    const TipoInfo *k = TipoDe(t, id);
    if (NOME_FIXO[k->classe] != NULL) BufAnexar(saida, NOME_FIXO[k->classe]);
    else if (k->classe == TC_REAL) TextoReal(t, k->a, saida);
    else if (k->classe == TC_STRUCT && k->b != 0) TextoListaFixa(t, k, saida);
    else if (k->classe == TC_STRUCT || k->classe == TC_ENUM) TextoNome(t, (int32_t)k->a, saida);
    else if (k->classe == TC_PONTEIRO || k->classe == TC_ANULAVEL) {
        BufAnexar(saida, k->classe == TC_ANULAVEL ? "?^" : "^");
        TipoAnexarTexto(t, k->a, saida);
    } else if (k->classe == TC_ARRAY || k->classe == TC_FATIA || k->classe == TC_DINAMICO) {
        BufAnexar(saida, "[");
        if (k->classe == TC_ARRAY) BufAnexarU64(saida, k->b);
        if (k->classe == TC_DINAMICO) BufAnexar(saida, "dynamic");
        BufAnexar(saida, "]");
        TipoAnexarTexto(t, k->a, saida);
    } else if (k->classe >= TC_MAPA) TextoDaV19(t, k, saida);
    else if (k->classe >= TC_OPCIONAL) TextoDaV17(t, k, saida);
    else TextoResultado(t, k, saida);
}
