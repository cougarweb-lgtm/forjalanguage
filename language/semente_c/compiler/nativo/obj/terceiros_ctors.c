/* terceiros_ctors.c - tabela de construtores do 4o objeto do ligador (P23f do M3b): .CRT$XC* (MSVC) e .ctors / .ctors.NNNNN (gcc) viram uma tabela de ponteiros na ordem de execucao.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3b P23f: TerceirosConstrutores. Cada relocacao ADDR64 de uma secao de construtor vira uma entrada da tabela .rdata$ctors (a mesma correcao: simbolo e adendo);
 *                    forja_ctors_ini e forja_ctors_fim marcam a tabela. Sem relocacao (os ponteiros zero das secoes sentinela .CRT$XCA/.CRT$XCZ e o -1 e o 0 das pontas da lista do gcc) nao ha entrada.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_nativo.py --pasta m3/estatica
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/obj/terceiros_ctors.h"

#define TC_PREFIXO_XC ".CRT$XC"
#define TC_NOME_CTORS ".ctors"
#define TC_SECAO_TABELA ".rdata$ctors"
#define TC_TAM_PONTEIRO 8u
#define TC_NAO_E_CTOR (-1)
#define TC_GRUPO_XC 0
#define TC_GRUPO_CTORS 1

/* Uma entrada da tabela: de que grupo e secao veio, o offset do ponteiro nela e a correcao que o preenche. */
typedef struct {
    int grupo;
    uint32_t secao;
    uint32_t off;
    ObjCorrecao correcao;
} TcEntrada;

static int TcGrupoDaSecao(const char *nome)
{
    size_t tam_ctors = strlen(TC_NOME_CTORS);

    if (strncmp(nome, TC_PREFIXO_XC, strlen(TC_PREFIXO_XC)) == 0)
        return TC_GRUPO_XC;
    if (strncmp(nome, TC_NOME_CTORS, tam_ctors) == 0 && (nome[tam_ctors] == '\0' || nome[tam_ctors] == '.'))
        return TC_GRUPO_CTORS;
    return TC_NAO_E_CTOR;
}

static void TcColetar(const Objeto *o, Vec *entradas)
{
    for (size_t r = 0; r < o->relocs.n; r++) {
        const ObjReloc *rel = VecEm(&o->relocs, r);
        int grupo = TcGrupoDaSecao(((const ObjSecao *)VecEm(&o->secoes, rel->alvo.secao))->nome);

        if (grupo != TC_NAO_E_CTOR && rel->correcao.tipo == OR_ADDR64)
            *(TcEntrada *)VecEmpurrar(entradas) = (TcEntrada){grupo, rel->alvo.secao, rel->alvo.off, rel->correcao};
    }
}

static int TcMenor(uint32_t a, uint32_t b)
{
    return (a > b) - (a < b);
}

/* XC pela ordem do nome da secao e do offset; .ctors na ordem inversa (a lista do ld roda do fim para o comeco). */
static int TcComparar(const void *a, const void *b)
{
    const TcEntrada *x = a;
    const TcEntrada *y = b;
    int c = TcMenor((uint32_t)x->grupo, (uint32_t)y->grupo);

    if (c != 0)
        return c;
    c = TcMenor(x->secao, y->secao);
    if (c == 0)
        c = TcMenor(x->off, y->off);
    return x->grupo == TC_GRUPO_CTORS ? -c : c;
}

static void TcCriarTabela(Objeto *o, const Vec *entradas)
{
    static const char zeros[TC_TAM_PONTEIRO] = {0};
    uint32_t secao = ObjSecaoAbrir(o, TC_SECAO_TABELA, OS_RDATA, TC_TAM_PONTEIRO);
    ObjSecao *tabela = VecEm(&o->secoes, secao);

    for (size_t k = 0; k < entradas->n; k++) {
        const TcEntrada *e = VecEm(entradas, k);

        BufAnexarN(&tabela->bytes, zeros, sizeof zeros);
        ObjRelocar(o, (ObjAlvoReloc){secao, (uint32_t)(k * TC_TAM_PONTEIRO)}, e->correcao);
    }
    ObjSimboloDefinir(o, TERCEIROS_SIMBOLO_CTORS_INI, (int32_t)secao, 0);
    ObjSimboloDefinir(o, TERCEIROS_SIMBOLO_CTORS_FIM, (int32_t)secao, (uint32_t)(entradas->n * TC_TAM_PONTEIRO));
}

bool TerceirosConstrutores(Objeto *o)
{
    Vec entradas;
    bool criou;

    VecIniciar(&entradas, sizeof(TcEntrada));
    TcColetar(o, &entradas);
    criou = entradas.n > 0;
    if (criou) {
        qsort(entradas.dados, entradas.n, sizeof(TcEntrada), TcComparar);
        TcCriarTabela(o, &entradas);
    }
    VecLiberar(&entradas);
    return criou;
}
