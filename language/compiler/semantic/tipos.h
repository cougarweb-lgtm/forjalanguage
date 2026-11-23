/* tipos.h - tabela de tipos da Forja com hash-consing: igualdade de tipo = igualdade de TipoId.
 * Versao: 0.10.1
 * Changelog: 0.10.1 - Linguagem v1.12 M3 P21v-a: o nome do parametro de proc vai no campo novo TipoInfo.c da celula da cauda (sai TC_PROC_NOME: a celula extra deslocava os TipoId e mudava o C emitido).
 *            0.10.0 - Linguagem v1.12 M3 P21v-a: ProcAssinatura (TipoProc recebe a assinatura), TipoProcNome e TipoProcCompativel.
 *            0.9.0 -Linguagem v1.11 L15a: TipoListaFixa, TipoEhListaFixa, TipoListaFixaArray/Elemento/N e o texto "[dynamic, N]T" (A08, E0335). A lista de capacidade fixa e uma
 *                    TC_STRUCT oculta marcada por b != 0 (b = TipoId do [N]T de itens): IR, layout e backends a veem como struct comum.
 *            0.8.0 - Linguagem v1.9 P3: TC_MAPA, TC_BIT_SET, TC_SOA, TC_MATRIZ; TipoMapa, TipoBitSet, TipoSoa, TipoMatriz; tem_mapa/tem_bit_set/tem_soa/tem_matriz e TiposTemV19; texto map[K]V, bit_set[E], soa[N]S, matrix[R, C]T.
 *            0.7.0 - Linguagem v1.8b P5a: TC_PROC; TipoProc, TipoProcQtd, TipoProcParam.
 *            0.6.0 - Linguagem v1.7 P2: TC_OPCIONAL, TC_UNIAO, TC_TUPLA, TC_TUPLA_CAUDA; tem_opcional/tem_uniao; TipoOpcional, TipoUniao, TipoTupla, TipoTuplaQtd, TipoTuplaElemento, TipoOpcionalNicho.
 *            0.5.1 - Linguagem v1.6 P4 (desempenho): Tipos.tem_dinamico, ligado por TipoDinamico (atalho de SemConferirMovimento).
 *            0.5.0 - Linguagem v1.6 P2: TC_DINAMICO (a = elemento) e TipoDinamico.
 *            0.4.0 - Linguagem v1.5 P2: TC_ARRAY (a = elemento, b = N) e TC_FATIA (a = elemento); TipoArray e TipoFatia.
 *            0.3.0 - Linguagem v1.3: ids fixos 15 TIPO_RAWPTR e 16 TIPO_CSTR (TIPO_FIXOS_QTD 17); TIPO_STR = TIPO_TEXTO (nome "str");
 *                    classes TC_RAWPTR, TC_CSTR, TC_ANULAVEL; TipoAnulavel; TipoPonteiro sobre qualquer tipo; TipoEhFamiliaPonteiro.
 *            0.2.0 - Linguagem v1.2: i8 i16 u8 u16 u32 uint(=u64) f64 (ids fixos 8..14, depois dos 7 antigos); consultas O(1)
 *                    de largura, sinal e faixa; TipoLerLiteralU64/TipoInteiroCabe/TipoBitsCabem; i64 = int e u64 = uint (alias).
 *            0.1.0 - Fase 5c: classes vazio, bool, i32, int, real (f32 + dimensao), texto, struct, enum, ponteiro, resultado
 *                    (T ! E e so-erro ! E) e veneno (gancho para silenciar cascata de erros).
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\semantica\verificar_semantica.py
 *
 * TipoId 0 = nao resolvido (ast.h). Os ids fixos abaixo existem desde TiposIniciar. Cada tipo e (classe, a, b):
 *   TC_REAL: a = DimId        TC_STRUCT/TC_ENUM: a = id do nome (Interner)   TC_PONTEIRO/TC_ANULAVEL: a = TipoId do apontado
 *   TC_DINAMICO: a = TipoId do elemento
 *   TC_RESULTADO: a = TipoId do valor (TIPO_VAZIO em '! E'), b = TipoId do enum de erro
 *   TC_MAPA: a = TipoId da chave K, b = TipoId do valor V
 *   TC_BIT_SET: a = TipoId do enum, b = numero de membros
 *   TC_SOA: a = TipoId do [N]S ou do [dynamic]S
 *   TC_MATRIZ: a = TipoId do elemento (f32/f64), b = (R << 8) | C
 *   TC_STRUCT oculta de [dynamic, N]T (v1.11 L15a): a = id do nome "ListaFixa_<k>" (Interner), b = TipoId do [N]T do campo itens (0 nas structs comuns)
 */
#ifndef FORJA_TIPOS_H
#define FORJA_TIPOS_H

#include "common/buf.h"
#include "common/intern.h"
#include "frontend/ast.h"
#include "semantic/dimensao.h"

typedef enum {
    TC_VAZIO, TC_BOOL, TC_I32, TC_INT, TC_REAL, TC_TEXTO, TC_STRUCT, TC_ENUM, TC_PONTEIRO, TC_RESULTADO, TC_VENENO,
    /* v1.2: acrescentadas no fim para nao mexer nos ids fixos 1..7 */
    TC_I8, TC_I16, TC_U8, TC_U16, TC_U32, TC_UINT, TC_F64,
    /* v1.3: TC_TEXTO passa a ser o str; rawptr (void *), cstr (const char *) e '?^T' (a = TipoId do apontado) */
    TC_RAWPTR, TC_CSTR, TC_ANULAVEL,
    /* v1.5: '[N]T' (a = TipoId do elemento, b = N) e '[]T' (a = TipoId do elemento) */
    TC_ARRAY, TC_FATIA,
    /* v1.6: '[dynamic]T' (a = TipoId do elemento) */
    TC_DINAMICO,
    /* v1.7: '?T' (a = TipoId do valor), union (a = id do nome), tupla (a = 1o elemento, b = cauda TC_TUPLA_CAUDA ou 0) */
    TC_OPCIONAL, TC_UNIAO, TC_TUPLA, TC_TUPLA_CAUDA,
    /* v1.8b: callback de foreign (a = TipoId do retorno, TIPO_VAZIO sem retorno; b = cauda TC_TUPLA_CAUDA dos parametros ou 0) */
    TC_PROC,
    /* v1.9: map (a = K, b = V), bit_set (a = TipoId do enum, b = numero de membros), soa (a = TipoId do [N]S ou do [dynamic]S),
     * matrix (a = elemento f32/f64, b = (R << 8) | C) */
    TC_MAPA, TC_BIT_SET, TC_SOA, TC_MATRIZ,
    TC_QTD
} TipoClasse;

/* Ids fixos: 1..7 desde a Fase 5c; 8..14 desde a v1.2; 15..16 desde a v1.3. i64 e o mesmo tipo que int (4) e u64 o mesmo que uint (13). */
enum {
    TIPO_VAZIO = 1, TIPO_BOOL, TIPO_I32, TIPO_INT, TIPO_F32, TIPO_TEXTO, TIPO_VENENO,
    TIPO_I8, TIPO_I16, TIPO_U8, TIPO_U16, TIPO_U32, TIPO_UINT, TIPO_F64, TIPO_RAWPTR, TIPO_CSTR, TIPO_FIXOS_QTD
};
enum { TIPO_STR = TIPO_TEXTO }; /* v1.3: o id 6 e o str (texto UTF-8 imutavel: ponteiro + bytes) */

/* c = id do NOME no Interner (UINT32_MAX = sem nome) so nas celulas TC_TUPLA_CAUDA da cauda de um TC_PROC (v1.12 P21v-a); 0 nas demais classes */
typedef struct { uint8_t classe; uint32_t a, b, c; } TipoInfo;

typedef struct {
    Vec itens;   /* TipoInfo, indexado por TipoId (o slot 0 e o "nao resolvido") */
    uint32_t *tabela, cap; /* hash aberto: TipoId (0 = vazio) */
    const Interner *nomes;
    const DimTabela *dims;
    bool tem_dinamico; /* algum '[dynamic]T' foi criado: sem ele nenhum tipo e dono e a regra de copia nao tem o que conferir */
    bool tem_opcional, tem_uniao; /* v1.7: algum '?T' / union foi criado (atalhos do caminho quente) */
    bool tem_mapa, tem_bit_set, tem_soa, tem_matriz; /* v1.9: algum tipo da classe foi criado (atalhos) */
} Tipos;

void TiposIniciar(Tipos *t, const Interner *nomes, const DimTabela *dims);
void TiposLiberar(Tipos *t);

TipoId TipoReal(Tipos *t, DimId dim);
TipoId TipoStruct(Tipos *t, int32_t nome);
TipoId TipoEnum(Tipos *t, int32_t nome);
TipoId TipoPonteiro(Tipos *t, TipoId apontado);    /* '^T' vale sobre qualquer tipo */
TipoId TipoAnulavel(Tipos *t, TipoId apontado);    /* '?^T' */
TipoId TipoResultado(Tipos *t, TipoId valor, TipoId erro);
TipoId TipoArray(Tipos *t, TipoId elemento, uint32_t n); /* '[N]T' */
TipoId TipoFatia(Tipos *t, TipoId elemento);             /* '[]T' */
TipoId TipoDinamico(Tipos *t, TipoId elemento);          /* '[dynamic]T' */
TipoId TipoOpcional(Tipos *t, TipoId valor);             /* '?T' (v1.7) */
TipoId TipoUniao(Tipos *t, int32_t nome);                /* union Nome (v1.7) */
TipoId TipoTupla(Tipos *t, const TipoId *elementos, uint32_t n); /* retorno multiplo, n >= 2 (v1.7) */
uint32_t TipoTuplaQtd(const Tipos *t, TipoId tupla);
TipoId TipoTuplaElemento(const Tipos *t, TipoId tupla, uint32_t k);
/* 'proc(nome: T, ...) -> R' (v1.8b; v1.12: o NOME de cada parametro faz parte da identidade, a compatibilidade o ignora: TipoProcCompativel).
 * nomes = id interno de cada nome (NULL = sem nome, -1); n = 0 vale. */
typedef struct { const TipoId *params; const int32_t *nomes; uint32_t n; TipoId retorno; } ProcAssinatura;
TipoId TipoProc(Tipos *t, const ProcAssinatura *a);
uint32_t TipoProcQtd(const Tipos *t, TipoId proc);
TipoId TipoProcParam(const Tipos *t, TipoId proc, uint32_t k); /* k-esimo parametro, 0-based */
int32_t TipoProcNome(const Tipos *t, TipoId proc, uint32_t k); /* nome do k-esimo parametro (-1 sem nome) */
bool TipoProcCompativel(const Tipos *t, TipoId a, TipoId b);   /* mesmo retorno e mesmos tipos de parametro, sem olhar os nomes */

TipoId TipoMapa(Tipos *t, TipoId chave, TipoId valor);           /* map[K]V (v1.9): tem_mapa = tem_dinamico = true */
TipoId TipoBitSet(Tipos *t, TipoId enumerado, uint32_t membros);  /* bit_set[E] (v1.9) */
TipoId TipoSoa(Tipos *t, TipoId base);                            /* soa[N]S / soa[dynamic]S (v1.9); base [dynamic]S liga tem_dinamico */
TipoId TipoMatriz(Tipos *t, TipoId elemento, uint32_t linhas, uint32_t colunas); /* matrix[R, C]T (v1.9); b = (l << 8) | c */

/* '[dynamic, N]T' (v1.11 L15a): struct oculta de nome 'nome' (C-safe, unico por tipo) cujo campo itens e o 'array' ([N]T, TipoId do TipoArray). */
TipoId TipoListaFixa(Tipos *t, int32_t nome, TipoId array);

static inline const TipoInfo *TipoDe(const Tipos *t, TipoId id) { return (const TipoInfo *)VecEm(&t->itens, id); }
static inline TipoClasse TipoClasseDe(const Tipos *t, TipoId id) { return (TipoClasse)TipoDe(t, id)->classe; }
static inline bool TipoEhListaFixa(const Tipos *t, TipoId id)
{
    const TipoInfo *k = TipoDe(t, id);
    return k->classe == TC_STRUCT && k->b != 0;
}
static inline TipoId TipoListaFixaArray(const Tipos *t, TipoId lista) { return TipoDe(t, lista)->b; }
static inline TipoId TipoListaFixaElemento(const Tipos *t, TipoId lista) { return TipoDe(t, TipoListaFixaArray(t, lista))->a; }
static inline uint32_t TipoListaFixaN(const Tipos *t, TipoId lista) { return TipoDe(t, TipoListaFixaArray(t, lista))->b; }
/* TipoId da lista fixa se 'id' e ela ou ^ela; 0 nos demais (caminho de erro e das formas de colecao). */
static inline TipoId TipoListaFixaDe(const Tipos *t, TipoId id, bool *por_ponteiro)
{
    const TipoInfo *k = TipoDe(t, id);
    *por_ponteiro = k->classe == TC_PONTEIRO;
    if (*por_ponteiro) id = k->a;
    return TipoEhListaFixa(t, id) ? id : 0;
}
static inline bool TiposTemV19(const Tipos *t) { return t->tem_mapa | t->tem_bit_set | t->tem_soa | t->tem_matriz; }
static inline uint32_t TipoMatrizLinhas(const Tipos *t, TipoId m) { return TipoDe(t, m)->b >> 8; }
static inline uint32_t TipoMatrizColunas(const Tipos *t, TipoId m) { return TipoDe(t, m)->b & 0xFFu; }
static inline bool TipoSoaDinamica(const Tipos *t, TipoId soa) { return TipoClasseDe(t, TipoDe(t, soa)->a) == TC_DINAMICO; }
static inline bool TipoEhInteiro(TipoId id) { return id == TIPO_I32 || id == TIPO_INT || (id >= TIPO_I8 && id <= TIPO_UINT); }
static inline bool TipoEhSemSinal(TipoId id) { return id >= TIPO_U8 && id <= TIPO_UINT; }
static inline bool TipoEhReal(const Tipos *t, TipoId id) { return id == TIPO_F64 || TipoClasseDe(t, id) == TC_REAL; }
static inline bool TipoEhNumerico(const Tipos *t, TipoId id) { return TipoEhInteiro(id) || TipoEhReal(t, id); }
/* ^T, ?^T, rawptr e cstr. Uso restrito: caminho de ERRO da semantica e emissao do 'as'; nunca no caminho de sucesso de ExBinario. */
static inline bool TipoEhFamiliaPonteiro(const Tipos *t, TipoId id)
{
    TipoClasse c = TipoClasseDe(t, id);
    return c == TC_PONTEIRO || c == TC_ANULAVEL || c == TC_RAWPTR || c == TC_CSTR;
}

/* '?cstr' e o unico nicho da v1.7: const char * com NULL = none (sem struct). */
static inline bool TipoOpcionalNicho(const Tipos *t, TipoId id)
{
    const TipoInfo *k = TipoDe(t, id);
    return k->classe == TC_OPCIONAL && k->a == TIPO_CSTR;
}

/* Largura em bits: i8/u8 8, i16/u16 16, i32/u32/f32/unidade 32, int/uint/f64 64; 0 para o que nao e numerico. */
uint8_t TipoLarguraBits(const Tipos *t, TipoId id);
/* Limites de um tipo inteiro (id fixo): Min e 0 nos sem sinal; Max como uint64_t (127, 32767, ..., UINT64_MAX). */
int64_t TipoInteiroMin(TipoId id);
uint64_t TipoInteiroMax(TipoId id);
/* Nome do id fixo 1..16 ("i8", "uint", "f64", "str", "rawptr", "cstr", "(sem valor)"...); NULL fora dessa faixa. */
const char *TipoNomeFixo(TipoId id);

/* Le os digitos decimais de 'digitos' (ate o 1o nao-digito); false se passa de UINT64_MAX. */
bool TipoLerLiteralU64(const char *digitos, uint64_t *valor);
/* A magnitude (com o sinal a parte) cabe no tipo inteiro? Com sinal W bits: -2^(W-1)..2^(W-1)-1; sem sinal: 0..max. */
bool TipoInteiroCabe(TipoId tipo, bool negativo, uint64_t magnitude);
/* Um valor ja calculado (bits em int64_t; sem sinal le como uint64_t) cabe no tipo? Tipo nao inteiro (enum, bool) = true. */
bool TipoBitsCabem(TipoId tipo, int64_t bits);

/* Texto legivel ("int", "Metros", "^Grifo", "?^Grifo", "[3]int", "[]Grifo", "Malha ! ErroCarga", "! ErroCarga"): unidade usa o nome da 1a unit da dimensao. */
void TipoAnexarTexto(const Tipos *t, TipoId id, Buf *saida);

#endif
