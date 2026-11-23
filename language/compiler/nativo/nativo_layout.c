/* nativo_layout.c - etapa ETAPA_LAYOUT: coerencia do layout\ e conferencia do layout do str e do [dynamic]T contra o runtime nativo.
 * Versao: 0.3.1
 * Changelog: 0.3.1 - M3 F9b-1: decisoes extraidas de NativoRtLayoutConferir.
 *            0.3.0 - M2 P17b: forja_rt_layout de 23 palavras (versao 3): confere tambem o map (tamanho e os offsets de chaves, valores, len e epoca de layout.h, contra o primeiro map do programa) e o
 *                    cabecalho da soa dinamica de 1 a 8 colunas (8 * ncol + 24 B, em toda soa[dynamic]S do programa).
 *            0.2.0 - M2 P14b: forja_rt_layout de 10 palavras (versao 2): o [dynamic]T (sizeof, alinhamento, offsets de len, cap, alocador e epoca) e conferido tambem, contra o primeiro
 *                    [dynamic]T da tabela de tipos (todo [dynamic]T tem o mesmo layout; sem nenhum no programa a conferencia da lista e dispensada). NativoRtLayoutConferir recebe os Tipos.
 *            0.1.0 - M2 P11b: NativoLayoutConferir (LayoutCalcular + LayoutConferir + forja_rt_layout) e NativoRtLayoutConferir.
 * Verificacao: construir.bat && teste_nativo.exe (casos de X0906) e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe
 *
 * forja_rt_layout (runtime\nativo\rt_nativo.c) e um vetor de 23 uint32 em .rdata: {versao, sizeof forja_str, alignof forja_str, offsetof(len),
 * sizeof lista, alignof lista, offsetof(len), offsetof(cap), offsetof(alocador), offsetof(epoca), sizeof forja_mapa, offsetof(chaves), offsetof(valores),
 * offsetof(len), offsetof(epoca), cabecalho da soa dinamica de 1 a 8 colunas}.
 * O backend le o simbolo do COFF do rt e compara com o layout\ de str, [dynamic]T, map e soa[dynamic]S, os tipos do runtime que o codigo gerado enxerga por valor.
 */
#include <string.h>
#include "common/arena.h"
#include "nativo/nativo_layout.h"
#define RT_LAYOUT_VERSAO 3u
#define RT_LAYOUT_CAMPOS 23u
#define RT_LAYOUT_SIMBOLO "forja_rt_layout"
#define RT_LAYOUT_LISTA 4u /* 1a palavra da lista */
#define STR_CAMPO_LEN 1u /* membro 1 do str: {ptr, len} */
#define LISTA_MEMBROS 4u /* len, cap, alocador e epoca: membros 1 a 4 da lista (o 0 e o ptr) */
#define RT_LAYOUT_BYTES (RT_LAYOUT_CAMPOS * sizeof(uint32_t))

/* Os 4 uint32 de forja_rt_layout (little endian); false = simbolo ausente, indefinido ou fora da secao. */
static bool LerRtLayout(Objeto *runtime, uint32_t saida[RT_LAYOUT_CAMPOS])
{
    for (size_t k = 0; k < runtime->simbolos.n; k++) {
        const ObjSimbolo *s = VecEm(&runtime->simbolos, k);
        const ObjSecao *secao;

        if (strcmp(s->nome, RT_LAYOUT_SIMBOLO) != 0)
            continue;
        if (s->secao == OBJ_SECAO_INDEFINIDA || (size_t)s->secao >= runtime->secoes.n)
            return false;
        secao = VecEm(&runtime->secoes, (size_t)s->secao);
        if ((size_t)s->off + RT_LAYOUT_BYTES > secao->bytes.tam)
            return false;
        memcpy(saida, secao->bytes.dados + s->off, RT_LAYOUT_BYTES);
        return true;
    }
    return false;
}

/* Um campo do layout de um tipo do runtime: o tipo ('str' ou '[dynamic]T'), prefixo da causa (vazio = o tamanho), o que o backend calculou e o que o runtime declarou. */
typedef struct { const char *tipo, *nome; uint32_t backend, runtime; } CampoRt;

static bool CampoIgual(Diag *erro, const CampoRt *campo)
{
    if (campo->backend == campo->runtime)
        return true;
    return DiagRelatar(erro, X0906, 0, "layout de '%s': %sbackend %u B, runtime %u B", campo->tipo, campo->nome, (unsigned)campo->backend, (unsigned)campo->runtime);
}

static bool StrConfere(const uint32_t rt[RT_LAYOUT_CAMPOS], const Layouts *layouts, Diag *erro)
{
    CampoRt campos[3] = {
        {"str", "", LayoutTamanho(layouts, TIPO_STR), rt[1]},
        {"str", "alinhamento ", LayoutAlinhamento(layouts, TIPO_STR), rt[2]},
        {"str", "offset de len ", LayoutOffsetCampo(layouts, TIPO_STR, STR_CAMPO_LEN), rt[3]},
    };

    for (uint32_t k = 0; k < 3; k++) {
        if (!CampoIgual(erro, &campos[k]))
            return false;
    }
    return true;
}

/* O primeiro tipo da classe dada na tabela de tipos (0 = o programa nao tem). */
static TipoId PrimeiroDaClasse(const Layouts *layouts, const Tipos *tipos, TipoClasse classe)
{
    for (TipoId id = 1; id < layouts->qtd; id++) {
        if (TipoClasseDe(tipos, id) == classe)
            return id;
    }
    return 0;
}

static bool ListaConfere(const uint32_t rt[RT_LAYOUT_CAMPOS], const Layouts *layouts, TipoId lista, Diag *erro)
{
    static const char *const NOMES[LISTA_MEMBROS] = {"offset de len ", "offset de cap ", "offset de alocador ", "offset de epoca "};
    CampoRt tamanho = {"[dynamic]T", "", LayoutTamanho(layouts, lista), rt[RT_LAYOUT_LISTA]};
    CampoRt alinhamento = {"[dynamic]T", "alinhamento ", LayoutAlinhamento(layouts, lista), rt[RT_LAYOUT_LISTA + 1]};

    if (!CampoIgual(erro, &tamanho) || !CampoIgual(erro, &alinhamento))
        return false;
    for (uint32_t k = 0; k < LISTA_MEMBROS; k++) {
        CampoRt campo = {"[dynamic]T", NOMES[k], LayoutOffsetCampo(layouts, lista, k + 1), rt[RT_LAYOUT_LISTA + 2 + k]};

        if (!CampoIgual(erro, &campo))
            return false;
    }
    return true;
}

#define RT_LAYOUT_MAPA 10u /* 1a palavra do map: sizeof, depois os offsets de chaves, valores, len e epoca */
#define MAPA_MEMBROS 4u
#define RT_LAYOUT_SOA 15u /* 1a palavra do cabecalho da soa dinamica (1 coluna); as outras 7 seguem */
#define SOA_COLUNAS_MAX 8u
#define SOA_MEMBRO_LEN 1u /* membro 1 da soa dinamica: len, logo depois das 8 * ncol bytes das colunas */
#define SOA_BYTES_POR_COLUNA 8u

/* O map (opaco no layout\): tamanho do layout\ e os offsets que o codigo gerado usa (layout.h) contra o runtime. */
static bool MapaConfere(const uint32_t rt[RT_LAYOUT_CAMPOS], const Layouts *layouts, TipoId mapa, Diag *erro)
{
    static const char *const NOMES[MAPA_MEMBROS] = {"offset de chaves ", "offset de valores ", "offset de len ", "offset de epoca "};
    static const uint32_t OFFSETS[MAPA_MEMBROS] = {LAYOUT_MAPA_OFF_CHAVES, LAYOUT_MAPA_OFF_VALORES, LAYOUT_MAPA_OFF_LEN, LAYOUT_MAPA_OFF_EPOCA};
    CampoRt tamanho = {"map", "", LayoutTamanho(layouts, mapa), rt[RT_LAYOUT_MAPA]};

    if (!CampoIgual(erro, &tamanho))
        return false;
    for (uint32_t k = 0; k < MAPA_MEMBROS; k++) {
        CampoRt campo = {"map", NOMES[k], OFFSETS[k], rt[RT_LAYOUT_MAPA + 1 + k]};

        if (!CampoIgual(erro, &campo))
            return false;
    }
    return true;
}

static bool EhSoaDinamica(const Layouts *layouts, const Tipos *tipos, TipoId id)
{
    return TipoClasseDe(tipos, id) == TC_SOA && TipoClasseDe(tipos, TipoDe(tipos, id)->a) == TC_DINAMICO && LayoutTamanho(layouts, id) != 0;
}

/* O cabecalho (8 * ncol + 24 B) da soa dinamica de 1 a 8 colunas contra a palavra do runtime; mais de 8 colunas nao tem palavra e fica sem conferencia. */
static bool SoaConfere(const uint32_t rt[RT_LAYOUT_CAMPOS], const Layouts *layouts, TipoId soa, Diag *erro)
{
    static const char *const NOMES[SOA_COLUNAS_MAX] = {"cabecalho de 1 coluna ", "cabecalho de 2 colunas ", "cabecalho de 3 colunas ", "cabecalho de 4 colunas ",
                                                       "cabecalho de 5 colunas ", "cabecalho de 6 colunas ", "cabecalho de 7 colunas ", "cabecalho de 8 colunas "};
    uint32_t colunas = LayoutOffsetCampo(layouts, soa, SOA_MEMBRO_LEN) / SOA_BYTES_POR_COLUNA;
    CampoRt campo;

    if (colunas < 1 || colunas > SOA_COLUNAS_MAX)
        return true;
    campo = (CampoRt){"soa[dynamic]S", NOMES[colunas - 1], LayoutTamanho(layouts, soa), rt[RT_LAYOUT_SOA + colunas - 1]};
    return CampoIgual(erro, &campo);
}

/* Toda soa[dynamic]S do programa. */
static bool SoasConferem(const uint32_t rt[RT_LAYOUT_CAMPOS], const Layouts *layouts, const Tipos *tipos, Diag *erro)
{
    for (TipoId id = 1; id < layouts->qtd; id++) {
        if (EhSoaDinamica(layouts, tipos, id) && !SoaConfere(rt, layouts, id, erro))
            return false;
    }
    return true;
}

/* Le o forja_rt_layout do runtime e confere a versao dele (X0906 se faltar ou for de outra versao). */
static bool LerRtLayoutConferido(Objeto *runtime, uint32_t rt[RT_LAYOUT_CAMPOS], Diag *erro)
{
    if (!LerRtLayout(runtime, rt))
        return DiagRelatar(erro, X0906, 0, "o runtime nao tem o simbolo %s (rt_win.o de outra versao do backend)", RT_LAYOUT_SIMBOLO);
    if (rt[0] != RT_LAYOUT_VERSAO)
        return DiagRelatar(erro, X0906, 0, "%s do runtime e da versao %u; o backend conhece a %u", RT_LAYOUT_SIMBOLO, (unsigned)rt[0], (unsigned)RT_LAYOUT_VERSAO);
    return true;
}

/* [dynamic]T e map so sao conferidos se o programa tem algum. */
static bool ListaEMapaConferem(const uint32_t rt[RT_LAYOUT_CAMPOS], const Layouts *layouts, const Tipos *tipos, Diag *erro)
{
    TipoId lista = PrimeiroDaClasse(layouts, tipos, TC_DINAMICO);
    TipoId mapa;

    if (lista != 0 && !ListaConfere(rt, layouts, lista, erro))
        return false;
    mapa = PrimeiroDaClasse(layouts, tipos, TC_MAPA);
    return mapa == 0 || MapaConfere(rt, layouts, mapa, erro);
}

bool NativoRtLayoutConferir(Objeto *runtime, const Layouts *layouts, const Tipos *tipos, Diag *erro)
{
    uint32_t rt[RT_LAYOUT_CAMPOS];

    if (!LerRtLayoutConferido(runtime, rt, erro) || !StrConfere(rt, layouts, erro))
        return false;
    if (!ListaEMapaConferem(rt, layouts, tipos, erro))
        return false;
    return SoasConferem(rt, layouts, tipos, erro);
}

bool NativoLayoutConferir(const NativoPedido *pedido, Objeto *runtime, Diag *erro)
{
    Arena arena = {0};
    Layouts layouts;
    bool ok;

    LayoutCalcular(pedido->tipos, pedido->ir, &arena, &layouts);
    ok = LayoutConferir(&layouts, pedido->tipos, erro) && NativoRtLayoutConferir(runtime, &layouts, pedido->tipos, erro);
    ArenaLiberar(&arena);
    return ok;
}
