/* coff_membro.c - le os membros COFF de uma biblioteca estatica de terceiros (gcc/mingw e MSVC) para UM Objeto so, a uniao que o ligador trata como o 4o objeto (P23e do M3b).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M3b P23y-2: COMDAT com 2+ externos repetido em outro membro (perdedor mapeia para os simbolos do 1o; vencedor do LARGEST os reaponta, sem X0106 falso); EXACT_MATCH confere bytes E
 *                    relocacoes (alvo, tipo e adendo) depois de lidas as do membro (CmConferirExatos; antes os bytes novos ainda tinham o adendo e o alvo nao era visto); LARGEST anula tambem as
 *                    associadas (.pdata/.xdata) do lider antigo (CoffUniao.associadas); CmConteudoIgual confere o tipo da secao antes do memcmp.
 *            0.2.0 - M3b P23f: secao .tls (ou .tls$...) de um membro = X0107 (diretorio TLS do PE ainda nao montado).
 *            0.1.0 - M3b P23e: primeira versao. Simbolo local (classe STATIC e ".weak.*") ganha o nome ".<membro>.<indice>" (dois membros podem repetir o nome de um static); COMDAT: o lider e o 1o
 *                    externo definido na secao, a selecao vem do auxiliar do simbolo da secao (ANY e o resto: o 1o fica; SAME_SIZE/EXACT_MATCH conferem; LARGEST fica com a maior; NODUPLICATES = X0106;
 *                    ASSOCIATIVE morre com a secao a que se associa); WEAK_EXTERNAL e simbolo comum so ficam anotados na uniao (terceiros.c os fecha depois de todos os membros);
 *                    .drectve vira diretiva da uniao; secao descartavel (.debug_*) nao entra e as relocacoes dela tambem nao; relocacao de tipo desconhecido = X0903 com o tipo e o membro.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_nativo.py --pasta m3/estatica
 */
#include <stdio.h>
#include <string.h>

#include "nativo/obj/coff_interno.h"
#include "nativo/obj/coff_membro.h"

#define CM_CARAC_COMDAT 0x00001000u
#define CM_CARAC_DESCARTAVEL 0x02000000u
#define CM_CARAC_REMOVER 0x00000800u
#define CM_CARAC_RELOCS_EXTRAS 0x01000000u
#define CM_OFF_AUX_NUMERO 12u
#define CM_OFF_AUX_SELECAO 14u
#define CM_NOME_DIRETIVA ".drectve"
#define CM_NOME_TLS ".tls"
#define CM_TAM_NOME_TLS 4u
#define CM_PREFIXO_FRACO ".weak."
#define CM_TAM_PREFIXO_FRACO 6u
#define CM_TAM_NOME_LOCAL 40
#define CM_RELOC_ABSOLUTA 0u
#define CM_SEM_SECAO (-1)

typedef enum {
    CM_SEL_NENHUMA, CM_SEL_NODUP, CM_SEL_QUALQUER, CM_SEL_MESMO_TAMANHO, CM_SEL_EXATO, CM_SEL_ASSOCIATIVA, CM_SEL_MAIOR
} CmSelecao;

/* O que se sabe de cada secao do COFF: onde ela foi parar no Objeto (CM_SEM_SECAO = fora), a selecao do COMDAT e a secao a que se associa (1-based). */
typedef struct {
    int32_t idx;
    uint32_t carac;
    uint32_t par;      /* secao do Objeto do lider anterior: a comparar (conferir) ou a que esta substituiu (promovida) */
    uint32_t simbolo;  /* conferir: o simbolo do lider, so para a mensagem */
    uint16_t assoc;
    uint8_t selecao;
    bool anulada;
    bool tem_lider;
    bool promovida;    /* LARGEST: ficou no lugar do lider anterior */
    bool conferir;     /* EXACT_MATCH: bytes e relocacoes so se comparam com as relocacoes do membro ja lidas */
} CmSecao;

typedef struct {
    CoffUniao *u;
    CoffLeitor L;
    const char *membro;
    Vec secoes;  /* CmSecao, uma por secao do COFF */
    size_t fracos_ini;
} CmMembro;

/* Um simbolo do COFF ja lido (nome so vale durante a chamada). */
typedef struct {
    uint32_t i;
    const char *nome;
    int16_t sec;
    uint32_t valor;
    uint8_t classe;
    const uint8_t *e;
} CmSim;

static CmSecao *CmSec(const CmMembro *m, uint32_t i)
{
    return VecEm(&m->secoes, i);
}

static void CmMapear(CmMembro *m, uint32_t i, uint32_t idx)
{
    *(uint32_t *)VecEm(&m->L.mapa, i) = idx;
}

static uint32_t CmTamanho(const CmMembro *m, int32_t idx)
{
    const ObjSecao *s = VecEm(&m->L.o->secoes, (size_t)idx);

    return s->tipo == OS_BSS ? s->tam_bss : (uint32_t)s->bytes.tam;
}

/* ---------- diretivas e secoes ---------- */

static void CmAnotarDiretivas(CoffUniao *u, const Vec *itens)
{
    for (size_t k = 0; k < itens->n; k++) {
        const CoffDiretiva *d = VecEm(itens, k);

        switch (d->tipo) {
        case DIRETIVA_BIBLIOTECA:
            *(const char **)VecEmpurrar(&u->bibliotecas) = d->nome;
            break;
        case DIRETIVA_EXPORTA:
            *(const char **)VecEmpurrar(&u->exportados) = d->nome;
            break;
        case DIRETIVA_ALINHA_COMUM:
            *(CoffDiretiva *)VecEmpurrar(&u->alinhamentos) = *d;
            break;
        case DIRETIVA_IGNORADA:
            if (u->ignoradas++ == 0)
                u->primeira_ignorada = d->nome;
            break;
        }
    }
}

static bool CmDiretiva(CmMembro *m, const uint8_t *h)
{
    size_t ptr = (size_t)CoffLe(h + COFF_OFF_SEC_PTR, COFF_TAM_CAMPO);
    size_t raw = (size_t)CoffLe(h + COFF_OFF_SEC_RAW, COFF_TAM_CAMPO);
    Vec itens;

    if (!CoffCabe(m->L.n, ptr, raw))
        return CoffFalha(m->L.erro, "dados da secao .drectve fora do arquivo");
    VecIniciar(&itens, sizeof(CoffDiretiva));
    CoffDiretivasLer((const char *)m->L.b + ptr, raw, &m->u->o->nomes, &itens);
    CmAnotarDiretivas(m->u, &itens);
    VecLiberar(&itens);
    return true;
}

/* ".tls" ou ".tls$..." (dado thread_local do C): o PE precisa do diretorio TLS, que o ligador ainda nao monta. */
static bool CmEhTls(const char *nome)
{
    return strncmp(nome, CM_NOME_TLS, CM_TAM_NOME_TLS) == 0 && (nome[CM_TAM_NOME_TLS] == '\0' || nome[CM_TAM_NOME_TLS] == '$');
}

static bool CmAbrirSecao(CmMembro *m, uint32_t i)
{
    const uint8_t *h = CoffCabecalhoSecao(&m->L, i);
    char buf[COFF_TAM_NOME + 1];
    const char *nome = CoffNomeSecao(&m->L, h, buf);
    uint32_t carac = (uint32_t)CoffLe(h + COFF_OFF_SEC_CARAC, COFF_TAM_CAMPO);
    CmSecao *s = CmSec(m, i);

    *s = (CmSecao){.idx = CM_SEM_SECAO, .carac = carac};
    if (nome == NULL)
        return CoffFalha(m->L.erro, "nome de secao fora da tabela de strings");
    if (strcmp(nome, CM_NOME_DIRETIVA) == 0)
        return CmDiretiva(m, h);
    if ((carac & (CM_CARAC_DESCARTAVEL | CM_CARAC_REMOVER)) != 0)
        return true;
    if (CmEhTls(nome))
        return DiagRelatar(m->L.erro, X0107, 0, "COFF: membro '%s', secao '%s' (dado thread_local): o ligador nativo nao monta o diretorio TLS do PE", m->membro, nome);
    if ((carac & CM_CARAC_RELOCS_EXTRAS) != 0)
        return DiagRelatar(m->L.erro, X0903, 0, "COFF: membro '%s', secao '%s' com contagem de relocacoes estendida (mais de 65535)", m->membro, nome);
    s->idx = (int32_t)ObjSecaoAbrir(m->L.o, nome, CoffTipo(nome, carac), CoffAlinhamento(carac));
    return CoffPreencherSecao(&m->L, (uint32_t)s->idx, h);
}

static bool CmLerSecoes(CmMembro *m)
{
    for (uint32_t i = 0; i < m->L.nsec; i++)
        VecEmpurrar(&m->secoes);
    for (uint32_t i = 0; i < m->L.nsec; i++) {
        if (!CmAbrirSecao(m, i))
            return false;
    }
    return true;
}

/* ---------- percurso da tabela de simbolos (pula os auxiliares) ---------- */

typedef bool (*CmVisita)(CmMembro *m, uint32_t i, const uint8_t *e);

static bool CmPrepararSimbolos(CmMembro *m)
{
    CoffLeitor *L = &m->L;

    if (!CoffCabe(L->n, L->ini_simbolos, (size_t)L->nsim * COFF_TAM_SIMBOLO))
        return CoffFalha(L->erro, "tabela de simbolos fora do arquivo");
    for (uint32_t i = 0; i < L->nsim; i++)
        *(uint32_t *)VecEmpurrar(&L->mapa) = COFF_SIMBOLO_NAO_MAPEADO;
    return true;
}

static bool CmPercorrer(CmMembro *m, CmVisita visita)
{
    for (uint32_t i = 0; i < m->L.nsim;) {
        const uint8_t *e = m->L.b + m->L.ini_simbolos + (size_t)i * COFF_TAM_SIMBOLO;
        uint32_t aux = e[COFF_OFF_SIM_AUX];

        if (i + 1 + aux > m->L.nsim)
            return CoffFalha(m->L.erro, "simbolo auxiliar passa do fim da tabela");
        if (!visita(m, i, e))
            return false;
        i += 1 + aux;
    }
    return true;
}

/* ---------- COMDAT: o simbolo da secao diz a selecao ---------- */

static bool CmFormaDeDefSecao(const uint8_t *e)
{
    return e[COFF_OFF_SIM_CLASSE] == COFF_CLASSE_ESTATICA && e[COFF_OFF_SIM_AUX] == 1 && CoffLe(e + COFF_OFF_SIM_TIPO, 2) == 0 &&
           CoffLe(e + COFF_OFF_SIM_VALOR, COFF_TAM_CAMPO) == 0;
}

static bool CmSecaoComdat(const CmMembro *m, int16_t sec)
{
    return sec > 0 && (uint32_t)sec <= m->L.nsec && (CmSec(m, (uint32_t)sec - 1)->carac & CM_CARAC_COMDAT) != 0;
}

static bool CmColetarSelecao(CmMembro *m, uint32_t i, const uint8_t *e)
{
    int16_t sec = (int16_t)CoffLe(e + COFF_OFF_SIM_SECAO, 2);
    const uint8_t *aux = e + COFF_TAM_SIMBOLO;
    CmSecao *s;

    (void)i;
    if (!CmFormaDeDefSecao(e) || !CmSecaoComdat(m, sec))
        return true;
    s = CmSec(m, (uint32_t)sec - 1);
    s->assoc = (uint16_t)CoffLe(aux + CM_OFF_AUX_NUMERO, 2);
    s->selecao = aux[CM_OFF_AUX_SELECAO];
    return true;
}

static CoffComdat *CmAcharComdat(CoffUniao *u, uint32_t simbolo)
{
    for (size_t k = 0; k < u->comdats.n; k++) {
        CoffComdat *c = VecEm(&u->comdats, k);

        if (c->simbolo == simbolo)
            return c;
    }
    return NULL;
}

/* Mesmo tipo, tamanho e bytes (os campos de relocacao ja foram zerados em CmReloc nas duas secoes). */
static bool CmConteudoIgual(const CmMembro *m, int32_t a, int32_t b)
{
    const ObjSecao *sa = VecEm(&m->L.o->secoes, (size_t)a);
    const ObjSecao *sb = VecEm(&m->L.o->secoes, (size_t)b);

    if (sa->tipo != sb->tipo || CmTamanho(m, a) != CmTamanho(m, b))
        return false;
    return sa->tipo == OS_BSS || sa->bytes.tam == 0 || memcmp(sa->bytes.dados, sb->bytes.dados, sa->bytes.tam) == 0;
}

static bool CmSecaoIgual(const CmMembro *m, int32_t a, int32_t b)
{
    const ObjSecao *sa = VecEm(&m->L.o->secoes, (size_t)a);
    const ObjSecao *sb = VecEm(&m->L.o->secoes, (size_t)b);

    return strcmp(sa->nome, sb->nome) == 0 && sa->tipo == sb->tipo && CmTamanho(m, a) == CmTamanho(m, b);
}

static bool CmLocalDefinido(const ObjSimbolo *s)
{
    return s->nome[0] == '.' && s->secao >= 0;
}

/* Alvos iguais: o mesmo simbolo, ou dois locais (".<membro>.<i>", um por membro) no mesmo offset de secoes de mesmo nome, tipo e tamanho. */
static bool CmAlvoEquivalente(const CmMembro *m, uint32_t a, uint32_t b)
{
    const ObjSimbolo *x = VecEm(&m->L.o->simbolos, a);
    const ObjSimbolo *y = VecEm(&m->L.o->simbolos, b);

    if (a == b)
        return true;
    return CmLocalDefinido(x) && CmLocalDefinido(y) && x->off == y->off && CmSecaoIgual(m, x->secao, y->secao);
}

/* Proxima relocacao da secao a partir de *k (ordem em que entraram no Objeto); NULL no fim. */
static const ObjReloc *CmProximaReloc(const Objeto *o, size_t *k, uint32_t secao)
{
    while (*k < o->relocs.n) {
        const ObjReloc *r = VecEm(&o->relocs, (*k)++);

        if (r->alvo.secao == secao)
            return r;
    }
    return NULL;
}

static bool CmRelocIgual(const CmMembro *m, const ObjReloc *x, const ObjReloc *y)
{
    return x->alvo.off == y->alvo.off && x->correcao.tipo == y->correcao.tipo && x->correcao.adendo == y->correcao.adendo &&
           CmAlvoEquivalente(m, x->correcao.simbolo, y->correcao.simbolo);
}

static bool CmRelocsIguais(const CmMembro *m, uint32_t a, uint32_t b)
{
    size_t ka = 0;
    size_t kb = 0;

    for (;;) {
        const ObjReloc *x = CmProximaReloc(m->L.o, &ka, a);
        const ObjReloc *y = CmProximaReloc(m->L.o, &kb, b);

        if (x == NULL || y == NULL)
            return x == y;
        if (!CmRelocIgual(m, x, y))
            return false;
    }
}

static bool CmComdatDifere(const CmMembro *m, const char *nome, const char *selecao)
{
    return DiagRelatar(m->L.erro, X0903, 0, "COFF: simbolo '%s' COMDAT %s difere entre os membros (o segundo e '%s')", nome, selecao, m->membro);
}

/* EXACT_MATCH, depois de lidas as relocacoes do membro: so ai os bytes da secao nova estao sem o adendo e a lista de relocacoes dela existe. */
static bool CmConferirExatos(const CmMembro *m)
{
    for (uint32_t i = 0; i < m->L.nsec; i++) {
        const CmSecao *s = CmSec(m, i);
        const ObjSimbolo *lider;

        if (!s->conferir)
            continue;
        if (CmConteudoIgual(m, (int32_t)s->par, s->idx) && CmRelocsIguais(m, s->par, (uint32_t)s->idx))
            continue;
        lider = VecEm(&m->L.o->simbolos, s->simbolo);
        return CmComdatDifere(m, lider->nome, "EXACT_MATCH");
    }
    return true;
}

/* SAME_SIZE confere o tamanho agora; EXACT_MATCH confere o tamanho agora e deixa bytes e relocacoes para CmConferirExatos. */
static bool CmConferirTamanho(const CmMembro *m, const CmSim *s, CmSecao *sec, const CoffComdat *c)
{
    if (CmTamanho(m, (int32_t)c->secao) != CmTamanho(m, sec->idx))
        return CmComdatDifere(m, s->nome, sec->selecao == CM_SEL_EXATO ? "EXACT_MATCH" : "SAME_SIZE");
    sec->conferir = sec->selecao == CM_SEL_EXATO;
    sec->par = c->secao;
    sec->simbolo = c->simbolo;
    return true;
}

/* Anula a secao e, em cadeia, as associadas a ela guardadas de membros ja lidos (.pdata/.xdata do lider que perdeu). A aresta usada sai da lista (pai invalido). */
static void CmAnularComAssociadas(CoffUniao *u, uint32_t secao)
{
    *(uint32_t *)VecEmpurrar(&u->anuladas) = secao;
    for (size_t k = 0; k < u->associadas.n; k++) {
        CoffAssociada *a = VecEm(&u->associadas, k);

        if (a->pai != secao)
            continue;
        a->pai = UINT32_MAX;
        CmAnularComAssociadas(u, a->filho);
    }
}

/* O lider do COMDAT perdeu para a secao que veio depois (LARGEST): o simbolo passa a apontar para a nova e a velha, com as associadas dela, fica sem efeito. */
static void CmPromover(CmMembro *m, CoffComdat *c, const CmSim *s, CmSecao *sec)
{
    ObjSimbolo *sim = VecEm(&m->L.o->simbolos, c->simbolo);

    CmAnularComAssociadas(m->u, c->secao);
    sim->secao = sec->idx;
    sim->off = s->valor;
    sec->promovida = true;
    sec->par = c->secao;
    c->secao = (uint32_t)sec->idx;
    c->tam = CmTamanho(m, sec->idx);
}

static bool CmComdatDuplicado(CmMembro *m, const CmSim *s, CmSecao *sec, CoffComdat *c)
{
    switch (sec->selecao) {
    case CM_SEL_NODUP:
        return DiagRelatar(m->L.erro, X0106, 0, "COFF: simbolo '%s' definido em dois membros e a secao e COMDAT NODUPLICATES (o segundo e '%s')", s->nome, m->membro);
    case CM_SEL_MESMO_TAMANHO:
    case CM_SEL_EXATO:
        if (!CmConferirTamanho(m, s, sec, c))
            return false;
        break;
    case CM_SEL_MAIOR:
        if (CmTamanho(m, sec->idx) > c->tam) {
            CmPromover(m, c, s, sec);
            return true;
        }
        break;
    default:
        break;
    }
    sec->anulada = true;
    return true;
}

/* ---------- simbolos ---------- */

static bool CmDefinirGlobal(CmMembro *m, const CmSim *s, int32_t secao)
{
    uint32_t idx = ObjSimboloReferir(m->L.o, s->nome);
    const ObjSimbolo *sim = VecEm(&m->L.o->simbolos, idx);

    if (sim->secao != OBJ_SECAO_INDEFINIDA)
        return DiagRelatar(m->L.erro, X0106, 0, "COFF: simbolo '%s' definido em dois membros (o segundo e '%s')", s->nome, m->membro);
    ObjSimboloDefinir(m->L.o, s->nome, secao, s->valor);
    CmMapear(m, s->i, idx);
    return true;
}

static bool CmLiderComdat(CmMembro *m, const CmSim *s, CmSecao *sec)
{
    CoffUniao *u = m->u;
    uint32_t idx = ObjSimboloReferir(m->L.o, s->nome);
    CoffComdat *c = CmAcharComdat(u, idx);

    sec->tem_lider = true;
    if (c != NULL) {
        CmMapear(m, s->i, idx);
        return CmComdatDuplicado(m, s, sec, c);
    }
    if (!CmDefinirGlobal(m, s, sec->idx))
        return false;
    *(CoffComdat *)VecEmpurrar(&u->comdats) = (CoffComdat){idx, (uint32_t)sec->idx, CmTamanho(m, sec->idx), sec->selecao};
    return true;
}

/* Static e os ".weak.*" do gcc (o padrao de um fraco) so valem dentro do membro: nome unico por membro e indice. */
static bool CmDefinirLocal(CmMembro *m, const CmSim *s, int32_t secao)
{
    char nome[CM_TAM_NOME_LOCAL];
    uint32_t idx;

    snprintf(nome, sizeof nome, ".%u.%u", m->u->membros, s->i);
    idx = ObjSimboloReferir(m->L.o, nome);
    ObjSimboloDefinir(m->L.o, nome, secao, s->valor);
    CmMapear(m, s->i, idx);
    return true;
}

static bool CmCompartilhado(const CmSim *s)
{
    return s->classe == COFF_CLASSE_EXTERNA && strncmp(s->nome, CM_PREFIXO_FRACO, CM_TAM_PREFIXO_FRACO) != 0;
}

static bool CmEhComdatDeLider(const CmSecao *sec)
{
    return (sec->carac & CM_CARAC_COMDAT) != 0 && sec->selecao != CM_SEL_NENHUMA && sec->selecao != CM_SEL_ASSOCIATIVA;
}

/* 2o externo (e os seguintes) de uma secao COMDAT cujo lider ja foi tratado. O perdedor so aponta para o simbolo que o membro anterior ja definiu; o vencedor do LARGEST
 * toma o simbolo que era da secao antiga; no 1o membro (nada definido ainda) e uma definicao comum. */
static bool CmDefinirDeComdat(CmMembro *m, const CmSim *s, const CmSecao *sec)
{
    uint32_t idx = ObjSimboloReferir(m->L.o, s->nome);
    ObjSimbolo *sim = VecEm(&m->L.o->simbolos, idx);

    if (sec->anulada) {
        CmMapear(m, s->i, idx);
        return true;
    }
    if (sec->promovida && sim->secao == (int32_t)sec->par) {
        sim->secao = sec->idx;
        sim->off = s->valor;
        CmMapear(m, s->i, idx);
        return true;
    }
    return CmDefinirGlobal(m, s, sec->idx);
}

static bool CmDefinir(CmMembro *m, const CmSim *s)
{
    CmSecao *sec;

    if ((uint32_t)s->sec > m->L.nsec)
        return DiagRelatar(m->L.erro, X0903, 0, "COFF: simbolo '%s' aponta para a secao %d, que nao existe", s->nome, s->sec);
    sec = CmSec(m, (uint32_t)s->sec - 1);
    if (sec->idx == CM_SEM_SECAO)
        return true;
    if (!CmCompartilhado(s))
        return CmDefinirLocal(m, s, sec->idx);
    if (CmEhComdatDeLider(sec))
        return sec->tem_lider ? CmDefinirDeComdat(m, s, sec) : CmLiderComdat(m, s, sec);
    return CmDefinirGlobal(m, s, sec->idx);
}

static bool CmRegistrarFraco(CmMembro *m, const CmSim *s)
{
    uint32_t idx = ObjSimboloReferir(m->L.o, s->nome);
    uint32_t padrao = COFF_SIMBOLO_NAO_MAPEADO;

    if (s->e[COFF_OFF_SIM_AUX] >= 1)
        padrao = (uint32_t)CoffLe(s->e + COFF_TAM_SIMBOLO, COFF_TAM_CAMPO);
    *(CoffFraco *)VecEmpurrar(&m->u->fracos) = (CoffFraco){idx, padrao};
    CmMapear(m, s->i, idx);
    return true;
}

static CoffComum *CmAcharComum(CoffUniao *u, uint32_t simbolo)
{
    for (size_t k = 0; k < u->comuns.n; k++) {
        CoffComum *c = VecEm(&u->comuns, k);

        if (c->simbolo == simbolo)
            return c;
    }
    return NULL;
}

/* Dois membros com o mesmo simbolo comum: vale o maior tamanho. */
static bool CmRegistrarComum(CmMembro *m, const CmSim *s)
{
    uint32_t idx = ObjSimboloReferir(m->L.o, s->nome);
    CoffComum *c = CmAcharComum(m->u, idx);

    if (c == NULL)
        *(CoffComum *)VecEmpurrar(&m->u->comuns) = (CoffComum){idx, s->valor};
    else if (s->valor > c->tam)
        c->tam = s->valor;
    CmMapear(m, s->i, idx);
    return true;
}

/* Secao 0: indefinido (externo, valor 0), comum (externo, valor = tamanho) ou fraco; o resto (.file, rotulo) nao entra. */
static bool CmSemSecao(CmMembro *m, const CmSim *s)
{
    if (s->classe == COFF_CLASSE_FRACA)
        return CmRegistrarFraco(m, s);
    if (s->classe != COFF_CLASSE_EXTERNA)
        return true;
    if (s->valor > 0)
        return CmRegistrarComum(m, s);
    CmMapear(m, s->i, ObjSimboloReferir(m->L.o, s->nome));
    return true;
}

static bool CmLerSimbolo(CmMembro *m, uint32_t i, const uint8_t *e)
{
    char buf[COFF_TAM_NOME + 1];
    CmSim s = {.i = i, .nome = CoffNomeSimbolo(&m->L, e, buf), .sec = (int16_t)CoffLe(e + COFF_OFF_SIM_SECAO, 2),
               .valor = (uint32_t)CoffLe(e + COFF_OFF_SIM_VALOR, COFF_TAM_CAMPO), .classe = e[COFF_OFF_SIM_CLASSE], .e = e};

    if (s.nome == NULL)
        return CoffFalha(m->L.erro, "nome de simbolo fora da tabela de strings");
    if (s.sec == COFF_SEC_NENHUMA)
        return CmSemSecao(m, &s);
    if (s.sec < 0)
        return true;
    return CmDefinir(m, &s);
}

/* O padrao de cada fraco deste membro: o indice COFF que o auxiliar guarda vira o simbolo do Objeto (absoluto = NAO_MAPEADO). */
static void CmResolverFracos(CmMembro *m)
{
    for (size_t k = m->fracos_ini; k < m->u->fracos.n; k++) {
        CoffFraco *f = VecEm(&m->u->fracos, k);

        f->padrao = f->padrao < m->L.nsim ? *(const uint32_t *)VecEm(&m->L.mapa, f->padrao) : COFF_SIMBOLO_NAO_MAPEADO;
    }
}

/* ---------- COMDAT associativo ---------- */

static bool CmAssociadaAnulada(const CmMembro *m, uint32_t i)
{
    CmSecao *s = CmSec(m, i);

    if (s->anulada || s->selecao != CM_SEL_ASSOCIATIVA || s->assoc == 0 || s->assoc > m->L.nsec)
        return false;
    if (!CmSec(m, (uint32_t)s->assoc - 1u)->anulada)
        return false;
    s->anulada = true;
    return true;
}

/* A secao associada a uma anulada tambem e anulada (ate nao mudar mais: a cadeia pode ter mais de um elo). */
static void CmPropagarAnuladas(CmMembro *m)
{
    bool mudou = true;

    while (mudou) {
        mudou = false;
        for (uint32_t i = 0; i < m->L.nsec; i++)
            mudou = CmAssociadaAnulada(m, i) || mudou;
    }
}

/* O COMDAT associativo vivo e o pai dele (tambem vivo), ou NULL. */
static const CmSecao *CmPaiVivo(const CmMembro *m, const CmSecao *s)
{
    const CmSecao *pai;

    if (s->selecao != CM_SEL_ASSOCIATIVA || s->idx == CM_SEM_SECAO || s->assoc == 0 || s->assoc > m->L.nsec)
        return NULL;
    pai = CmSec(m, (uint32_t)s->assoc - 1u);
    return pai->idx == CM_SEM_SECAO ? NULL : pai;
}

/* Guarda as associativas vivas deste membro: se o lider delas perder depois para um LARGEST de outro membro, elas saem com ele. */
static void CmRegistrarAssociadas(CmMembro *m)
{
    for (uint32_t i = 0; i < m->L.nsec; i++) {
        const CmSecao *s = CmSec(m, i);
        const CmSecao *pai = s->anulada ? NULL : CmPaiVivo(m, s);

        if (pai != NULL)
            *(CoffAssociada *)VecEmpurrar(&m->u->associadas) = (CoffAssociada){(uint32_t)pai->idx, (uint32_t)s->idx};
    }
}

static void CmRegistrarAnuladas(CmMembro *m)
{
    for (uint32_t i = 0; i < m->L.nsec; i++) {
        const CmSecao *s = CmSec(m, i);

        if (s->anulada && s->idx != CM_SEM_SECAO)
            *(uint32_t *)VecEmpurrar(&m->u->anuladas) = (uint32_t)s->idx;
    }
}

/* ---------- relocacoes ---------- */

static bool CmAlvoDaReloc(const CmMembro *m, uint32_t simb, uint32_t *alvo)
{
    if (simb >= m->L.mapa.n)
        return false;
    *alvo = *(const uint32_t *)VecEm(&m->L.mapa, simb);
    return *alvo != COFF_SIMBOLO_NAO_MAPEADO;
}

static bool CmReloc(CmMembro *m, uint32_t idx, const uint8_t *r)
{
    uint32_t va = (uint32_t)CoffLe(r, COFF_TAM_CAMPO);
    uint32_t simb = (uint32_t)CoffLe(r + COFF_TAM_CAMPO, COFF_TAM_CAMPO);
    uint16_t codigo = (uint16_t)CoffLe(r + 2 * COFF_TAM_CAMPO, 2);
    ObjSecao *s = VecEm(&m->L.o->secoes, idx);
    CoffRelocSuportada tipo;
    uint32_t alvo;

    if (codigo == CM_RELOC_ABSOLUTA)
        return true;
    if (!CoffTipoReloc(codigo, &tipo))
        return DiagRelatar(m->L.erro, X0903, 0, "COFF: relocacao de tipo 0x%x na secao '%s' do membro '%s' (aceitas: ADDR64, ADDR32NB, REL32, REL32_1..5, SECTION, SECREL)", codigo, s->nome, m->membro);
    if (!CmAlvoDaReloc(m, simb, &alvo))
        return DiagRelatar(m->L.erro, X0903, 0, "COFF: membro '%s', relocacao na secao '%s' aponta para o simbolo %u, que nao entrou no objeto", m->membro, s->nome, simb);
    if (s->tipo == OS_BSS || !CoffCabe(s->bytes.tam, va, tipo.tam))
        return DiagRelatar(m->L.erro, X0903, 0, "COFF: membro '%s', relocacao em 0x%x fora dos bytes da secao '%s'", m->membro, va, s->nome);
    ObjRelocar(m->L.o, (ObjAlvoReloc){idx, va}, (ObjCorrecao){tipo.tipo, alvo, CoffAdendo((const uint8_t *)s->bytes.dados + va, tipo.tam) + tipo.delta});
    memset(s->bytes.dados + va, 0, tipo.tam);
    return true;
}

static bool CmRelocsDaSecao(CmMembro *m, uint32_t i)
{
    const uint8_t *h = CoffCabecalhoSecao(&m->L, i);
    size_t ptr = (size_t)CoffLe(h + COFF_OFF_SEC_PTR_RELOC, COFF_TAM_CAMPO);
    uint32_t qtd = (uint32_t)CoffLe(h + COFF_OFF_SEC_QTD_RELOC, 2);

    if (!CoffCabe(m->L.n, ptr, (size_t)qtd * COFF_TAM_RELOC))
        return CoffFalha(m->L.erro, "tabela de relocacoes fora do arquivo");
    for (uint32_t k = 0; k < qtd; k++) {
        if (!CmReloc(m, (uint32_t)CmSec(m, i)->idx, m->L.b + ptr + (size_t)k * COFF_TAM_RELOC))
            return false;
    }
    return true;
}

static bool CmLerRelocacoes(CmMembro *m)
{
    for (uint32_t i = 0; i < m->L.nsec; i++) {
        if (CmSec(m, i)->idx != CM_SEM_SECAO && !CmRelocsDaSecao(m, i))
            return false;
    }
    return true;
}

/* ---------- o membro inteiro ---------- */

/* Secoes, depois a selecao dos COMDAT (1a volta nos simbolos) e depois os simbolos (2a volta). */
static bool CmLerSecoesESimbolos(CmMembro *m)
{
    return CmLerSecoes(m) && CmPrepararSimbolos(m) && CmPercorrer(m, CmColetarSelecao) && CmPercorrer(m, CmLerSimbolo);
}

static bool CmLerMembro(CmMembro *m)
{
    if (!CoffLerCabecalho(&m->L))
        return false;
    CoffLerTabelaStrings(&m->L);
    if (!CmLerSecoesESimbolos(m))
        return false;
    CmResolverFracos(m);
    CmPropagarAnuladas(m);
    CmRegistrarAssociadas(m);
    if (!CmLerRelocacoes(m) || !CmConferirExatos(m))
        return false;
    CmRegistrarAnuladas(m);
    return true;
}

bool CoffUniaoLerMembro(CoffUniao *u, const CoffMembroEntrada *membro, Diag *erro)
{
    CmMembro m = {.u = u, .L = {membro->dados, membro->tam, u->o, erro, 0, 0, 0, 0, 0, 0, {0}}, .membro = membro->nome, .fracos_ini = u->fracos.n};
    bool ok;

    VecIniciar(&m.secoes, sizeof(CmSecao));
    VecIniciar(&m.L.mapa, sizeof(uint32_t));
    ok = CmLerMembro(&m);
    u->membros++;
    VecLiberar(&m.secoes);
    VecLiberar(&m.L.mapa);
    return ok;
}

void CoffUniaoIniciar(CoffUniao *u, Objeto *o)
{
    memset(u, 0, sizeof *u);
    u->o = o;
    VecIniciar(&u->comdats, sizeof(CoffComdat));
    VecIniciar(&u->comuns, sizeof(CoffComum));
    VecIniciar(&u->fracos, sizeof(CoffFraco));
    VecIniciar(&u->alinhamentos, sizeof(CoffDiretiva));
    VecIniciar(&u->bibliotecas, sizeof(const char *));
    VecIniciar(&u->exportados, sizeof(const char *));
    VecIniciar(&u->anuladas, sizeof(uint32_t));
    VecIniciar(&u->associadas, sizeof(CoffAssociada));
}

void CoffUniaoLiberar(CoffUniao *u)
{
    VecLiberar(&u->comdats);
    VecLiberar(&u->comuns);
    VecLiberar(&u->fracos);
    VecLiberar(&u->alinhamentos);
    VecLiberar(&u->bibliotecas);
    VecLiberar(&u->exportados);
    VecLiberar(&u->anuladas);
    VecLiberar(&u->associadas);
}
