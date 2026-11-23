/* ligar_alcance.c - alcancabilidade do ligador do backend nativo (G4 do plano de concorrencia): descarta secao e import que ninguem usa.
 * Versao: 0.5.2
 * Changelog: 0.5.2 - M5b P41: _DYNAMIC entra na lista de simbolos do ligador ELF (indice 6; so existe no pie, o musl do rcrt1.o o referencia).
 *            0.5.1 - M5b P36: _GLOBAL_OFFSET_TABLE_ entra na lista de simbolos do ligador ELF (indice 5).
 *            0.5.0 - M5b P36: .init_array e raiz (AlcanceMarcarInitArray); fraco indefinido e simbolo do ligador ELF (AlcanceIndiceSimboloDoLigador) nao sao erro de alcance.
 *            0.4.1 - M3b P23f: so a contagem de objetos (ALCANCE_QTD_OBJETOS 5, o legado); sem mudanca de codigo.
 *            0.4.0 - M3b P23e: quatro objetos; AlcanceResolver tenta o global de outro objeto ANTES de tratar __imp_X como import (o terceiros define o slot __imp_ das estaticas).
 *            0.3.0 - M3 F5: tres objetos (programa, runtime, Ryu); o global indefinido se resolve em qualquer outro objeto (era so o "outro" de 1 - o).
 *            0.2.0 - M3 P19: imports_usados em bitmap (AlcanceImportUsado).
 *            0.1.0 - M2 P17c: primeira versao. Raizes por tabela; segue as relocacoes COFF das secoes vivas (REL32, ADDR32NB, ADDR64); __imp_ alcancavel
 *                    marca o bit do import; secao .pdata so sobe quando uma secao de codigo que ela descreve ja esta viva (e o .xdata dela vem pela relocacao).
 *                    Ordem das secoes vivas = ordem de entrada (so um vetor de bytes por secao, nada de ordem de ponteiro).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#include <string.h>

#include "nativo/obj/ligar_alcance.h"

typedef struct {
    uint32_t objeto;
    uint32_t secao;
} AlcancePar;

typedef enum { ALVO_NENHUM, ALVO_SECAO, ALVO_IMPORT } AlvoTipo;

static const ObjSimbolo *AlcanceSimbolo(const Alcance *a, int o, uint32_t i)
{
    return VecEm(&a->entrada->obj[o]->simbolos, i);
}

bool AlcanceVivo(const Alcance *a, int o, uint32_t sec)
{
    return *(const uint8_t *)VecEm(&a->vivo[o], sec) != 0;
}

static void AlcanceMarcar(Alcance *a, int o, uint32_t sec)
{
    uint8_t *vivo = VecEm(&a->vivo[o], sec);
    if (*vivo)
        return;
    *vivo = 1;
    AlcancePar *p = VecEmpurrar(&a->pilha);
    p->objeto = (uint32_t)o;
    p->secao = sec;
}

/* ---------- indice das relocacoes por secao (ordenacao por contagem, estavel) ---------- */

static void AlcanceContar(Alcance *a, int o)
{
    const Objeto *obj = a->entrada->obj[o];
    uint32_t *ini = a->ini[o].dados;
    for (size_t r = 0; r < obj->relocs.n; r++)
        ini[((const ObjReloc *)VecEm(&obj->relocs, r))->alvo.secao + 1]++;
    for (size_t s = 0; s < obj->secoes.n; s++)
        ini[s + 1] += ini[s];
}

static void AlcanceDistribuir(Alcance *a, int o)
{
    const Objeto *obj = a->entrada->obj[o];
    Vec cursor;
    VecIniciar(&cursor, sizeof(uint32_t));
    for (size_t s = 0; s < obj->secoes.n; s++)
        *(uint32_t *)VecEmpurrar(&cursor) = *(const uint32_t *)VecEm(&a->ini[o], s);
    for (size_t r = 0; r < obj->relocs.n; r++) {
        uint32_t sec = ((const ObjReloc *)VecEm(&obj->relocs, r))->alvo.secao;
        uint32_t *posicao = VecEm(&cursor, sec);
        ((uint32_t *)a->ord[o].dados)[(*posicao)++] = (uint32_t)r;
    }
    VecLiberar(&cursor);
}

static void AlcanceIndexar(Alcance *a, int o)
{
    const Objeto *obj = a->entrada->obj[o];
    VecIniciar(&a->vivo[o], sizeof(uint8_t));
    VecIniciar(&a->ini[o], sizeof(uint32_t));
    VecIniciar(&a->ord[o], sizeof(uint32_t));
    for (size_t s = 0; s < obj->secoes.n; s++)
        VecEmpurrar(&a->vivo[o]);
    for (size_t s = 0; s <= obj->secoes.n; s++)
        VecEmpurrar(&a->ini[o]);
    for (size_t r = 0; r < obj->relocs.n; r++)
        VecEmpurrar(&a->ord[o]);
    AlcanceContar(a, o);
    AlcanceDistribuir(a, o);
}

/* ---------- para onde uma relocacao leva ---------- */

static bool AlcanceEhImport(const char *nome)
{
    return strncmp(nome, ALCANCE_PREFIXO_IMP, ALCANCE_TAM_PREFIXO_IMP) == 0;
}

/* O global definido com esse nome em outro objeto que nao o: a secao dele em *alvo. Falso se nenhum outro o define. */
static bool AlcanceDefinidoEm(const Alcance *a, int o, const char *nome, AlcancePar *alvo)
{
    int64_t i = ObjAcharSimbolo(a->entrada->obj[o], nome);
    if (i < 0)
        return false;
    const ObjSimbolo *def = AlcanceSimbolo(a, o, (uint32_t)i);
    if (!def->global || def->secao < 0)
        return false;
    *alvo = (AlcancePar){(uint32_t)o, (uint32_t)def->secao};
    return true;
}

static bool AlcanceAcharEmOutro(const Alcance *a, int o, const char *nome, AlcancePar *alvo)
{
    for (int outro = 0; outro < ALCANCE_QTD_OBJETOS; outro++) {
        if (outro != o && AlcanceDefinidoEm(a, outro, nome, alvo))
            return true;
    }
    return false;
}

/* Simbolos que o ligador ELF define (M5b P36): a tabela fica aqui, para o alcance nao os tomar por indefinidos. */
static const char *const ALCANCE_SIMBOLOS_DO_LIGADOR[] = {"__init_array_start", "__init_array_end", "__fini_array_start", "__fini_array_end", "__ehdr_start", "_GLOBAL_OFFSET_TABLE_", "_DYNAMIC"};

int AlcanceIndiceSimboloDoLigador(const char *nome)
{
    for (int k = 0; k < (int)(sizeof ALCANCE_SIMBOLOS_DO_LIGADOR / sizeof ALCANCE_SIMBOLOS_DO_LIGADOR[0]); k++) {
        if (strcmp(ALCANCE_SIMBOLOS_DO_LIGADOR[k], nome) == 0)
            return k;
    }
    return -1;
}

/* O simbolo vira secao (a do proprio objeto, ou a do global definido em outro), import __imp_ ou nada (indefinido). */
static AlvoTipo AlcanceResolver(const Alcance *a, int o, const ObjSimbolo *s, AlcancePar *alvo)
{
    if (s->secao >= 0) {
        *alvo = (AlcancePar){(uint32_t)o, (uint32_t)s->secao};
        return ALVO_SECAO;
    }
    if (AlcanceAcharEmOutro(a, o, s->nome, alvo))
        return ALVO_SECAO;
    return AlcanceEhImport(s->nome) ? ALVO_IMPORT : ALVO_NENHUM;
}

bool AlcanceImportUsado(const Alcance *a, uint32_t k)
{
    return (a->imports_usados[k / ALCANCE_BITS_PALAVRA] & (1u << (k % ALCANCE_BITS_PALAVRA))) != 0;
}

/* O 1o nome igual vence (as tabelas vem em fila e a 1a que tem o simbolo e a que o ld usaria). */
static void AlcanceUsarImport(Alcance *a, const char *nome)
{
    const char *base = nome + ALCANCE_TAM_PREFIXO_IMP;
    for (uint32_t k = 0; k < a->entrada->qtd_imports; k++) {
        if (strcmp(a->entrada->imports[k], base) == 0) {
            a->imports_usados[k / ALCANCE_BITS_PALAVRA] |= 1u << (k % ALCANCE_BITS_PALAVRA);
            return;
        }
    }
    if (a->import_fora == NULL)
        a->import_fora = nome;
}

static void AlcanceSeguir(Alcance *a, int o, const ObjReloc *r)
{
    const ObjSimbolo *s = AlcanceSimbolo(a, o, r->correcao.simbolo);
    AlcancePar alvo;
    switch (AlcanceResolver(a, o, s, &alvo)) {
    case ALVO_SECAO:
        AlcanceMarcar(a, (int)alvo.objeto, alvo.secao);
        break;
    case ALVO_IMPORT:
        AlcanceUsarImport(a, s->nome);
        break;
    case ALVO_NENHUM:
        /* fraco indefinido vale 0 (D-R6) e os simbolos do ligador ELF sao dados por ele: nenhum deles e erro de alcance */
        if (a->indefinido == NULL && !s->fraco && AlcanceIndiceSimboloDoLigador(s->nome) < 0)
            a->indefinido = s->nome;
        break;
    }
}

/* Esvazia a pilha: cada secao viva marca o que as relocacoes dela citam. */
static void AlcanceDrenar(Alcance *a)
{
    while (a->pilha.n > 0) {
        AlcancePar p = *(const AlcancePar *)VecEm(&a->pilha, a->pilha.n - 1);
        a->pilha.n--;
        const uint32_t *ini = a->ini[p.objeto].dados;
        const uint32_t *ord = a->ord[p.objeto].dados;
        const Vec *rels = &a->entrada->obj[p.objeto]->relocs;
        for (uint32_t k = ini[p.secao]; k < ini[p.secao + 1]; k++)
            AlcanceSeguir(a, (int)p.objeto, VecEm(rels, ord[k]));
    }
}

/* ---------- .pdata: sobe com o codigo que descreve ---------- */

/* Alguma relocacao da secao (BeginAddress/EndAddress) aponta para uma secao de codigo ja viva? */
static bool AlcanceDescreveCodigoVivo(const Alcance *a, int o, uint32_t sec)
{
    const uint32_t *ini = a->ini[o].dados;
    const uint32_t *ord = a->ord[o].dados;
    const Objeto *obj = a->entrada->obj[o];
    for (uint32_t k = ini[sec]; k < ini[sec + 1]; k++) {
        const ObjReloc *r = VecEm(&obj->relocs, ord[k]);
        AlcancePar alvo;
        if (AlcanceResolver(a, o, AlcanceSimbolo(a, o, r->correcao.simbolo), &alvo) != ALVO_SECAO)
            continue;
        const ObjSecao *destino = VecEm(&a->entrada->obj[alvo.objeto]->secoes, alvo.secao);
        if (destino->tipo == OS_TEXTO && AlcanceVivo(a, (int)alvo.objeto, alvo.secao))
            return true;
    }
    return false;
}

/* Uma volta sobre as secoes .pdata ainda mortas; true se alguma subiu (o que ela cita pode acordar mais codigo). */
static bool AlcanceSubirPdata(Alcance *a)
{
    bool subiu = false;
    for (int o = 0; o < ALCANCE_QTD_OBJETOS; o++) {
        const Vec *secoes = &a->entrada->obj[o]->secoes;
        for (uint32_t s = 0; s < secoes->n; s++) {
            const ObjSecao *sec = VecEm(secoes, s);
            if (sec->tipo != OS_PDATA || AlcanceVivo(a, o, s) || !AlcanceDescreveCodigoVivo(a, o, s))
                continue;
            AlcanceMarcar(a, o, s);
            subiu = true;
        }
    }
    return subiu;
}

/* ---------- raizes ---------- */

static void AlcanceMarcarRaiz(Alcance *a, const AlcanceRaiz *raiz)
{
    const Objeto *obj = a->entrada->obj[raiz->objeto];
    if (raiz->simbolo == NULL) {
        for (uint32_t s = 0; s < obj->secoes.n; s++)
            AlcanceMarcar(a, raiz->objeto, s);
        return;
    }
    int64_t i = ObjAcharSimbolo(obj, raiz->simbolo);
    if (i < 0)
        return;
    const ObjSimbolo *s = VecEm(&obj->simbolos, (size_t)i);
    if (s->global && s->secao >= 0)
        AlcanceMarcar(a, raiz->objeto, (uint32_t)s->secao);
}

/* .init_array (ELF, M5b P36): os construtores rodam sem ninguem os citar, entao a secao inteira e raiz. */
static void AlcanceMarcarInitArray(Alcance *a)
{
    for (int o = 0; o < ALCANCE_QTD_OBJETOS; o++) {
        const Vec *secoes = &a->entrada->obj[o]->secoes;
        for (uint32_t s = 0; s < secoes->n; s++) {
            if (((const ObjSecao *)VecEm(secoes, s))->tipo == OS_INIT_ARRAY)
                AlcanceMarcar(a, o, s);
        }
    }
}

void AlcanceCalcular(const AlcanceEntrada *entrada, Alcance *a)
{
    memset(a, 0, sizeof *a);
    a->entrada = entrada;
    VecIniciar(&a->pilha, sizeof(AlcancePar));
    for (int o = 0; o < ALCANCE_QTD_OBJETOS; o++)
        AlcanceIndexar(a, o);
    for (uint32_t k = 0; k < entrada->qtd_raizes; k++)
        AlcanceMarcarRaiz(a, &entrada->raizes[k]);
    AlcanceMarcarInitArray(a);
    do {
        AlcanceDrenar(a);
    } while (AlcanceSubirPdata(a));
}

void AlcanceLiberar(Alcance *a)
{
    for (int o = 0; o < ALCANCE_QTD_OBJETOS; o++) {
        VecLiberar(&a->vivo[o]);
        VecLiberar(&a->ini[o]);
        VecLiberar(&a->ord[o]);
    }
    VecLiberar(&a->pilha);
}
