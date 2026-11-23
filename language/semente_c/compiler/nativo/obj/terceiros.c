/* terceiros.c - monta o 4o objeto do ligador com os membros de bibliotecas estaticas puxados pela selecao (P23e do M3b).
 * Versao: 0.5.0
 * Changelog: 0.5.0 - M6d P92c: anota a faixa de secoes de cada membro e, com TerceirosPedido.libmem, chama LibmemRedirecionar e LibmemTabela depois de ler os membros; TerceirosCriarCelula e publica.
 *            0.4.0 - M5b P36: membro ELF entra na uniao no alvo Linux (TrUnirElf: secoes, simbolos com ".m<k>" nos locais, relocacoes com +4 no adendo PC-relativo); no Windows
 *                    segue X0105; COFF no Linux e X0105. Dois fortes com o mesmo nome = X0903; forte vence fraco.
 *            0.3.0 - M5b P35: membro ELF pela magica (ArEhElf) e recusado com X0105 ate o P36 (a uniao de membros e de COFF); antes assumia COFF sem olhar.
 *            0.2.0 - M3b P23f: depois de ordenar, TerceirosConstrutores (terceiros_ctors.c) junta os .CRT$XC* e .ctors numa tabela; TerceirosRelato.tem_ctors.
 *            0.1.1 - M3b P23e: nome que outro objeto da ligacao ja define (memcpy do runtime) nao ganha thunk de DLL (TerceirosPedido.outros).
 *             0.1.0 - M3b P23e: TerceirosMontar. Depois de ler todos os membros (coff_membro.c): fraco sem definicao forte vira alias do padrao; comum sem definicao forte vira uma secao .bss$comum
 *                    (alinhamento do -aligncomm, senao a potencia de 2 do tamanho, no maximo 16); slot __imp_<nome> (8 bytes + ADDR64) para cada simbolo que o programa pede a uma estatica;
 *                    thunk "jmp [rip+__imp_<nome>]" para cada funcao de DLL que o codigo de terceiros chama sem __imp_; secoes COMDAT anuladas ficam vazias e perdem as relocacoes;
 *                    as secoes saem ordenadas pelo nome inteiro (".text" antes de ".text$a" antes de ".text$b"), estavel pela ordem de chegada.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_nativo.py --pasta m3/estatica
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "nativo/obj/ar_ler.h"
#include "nativo/obj/coff_interno.h"
#include "nativo/obj/elf_ler.h"
#include "nativo/obj/coff_membro.h"
#include "nativo/obj/estatica.h"
#include "nativo/obj/terceiros.h"
#include "nativo/obj/terceiros_ctors.h"

#define TR_PREFIXO_IMP "__imp_"
#define TR_TAM_PREFIXO_IMP 6u
#define TR_ALINHA_MAX_COMUM 16u
#define TR_ALINHA_CELULA 8u
#define TR_ALINHA_THUNK 16u
#define TR_TAM_CELULA 8u
#define TR_LOG2_MAX 12u
#define TR_SECAO_COMUM ".bss$comum"
#define TR_SECAO_CELULA ".rdata$imp"
#define TR_SECAO_THUNK ".text$thunk"
/* M5b P36: prefixo dos simbolos locais de um membro ELF (".m<indice>" + nome: membros diferentes nao colidem; continua local, com ponto). */
#define TR_PREFIXO_LOCAL ".m%u"
#define TR_FIM_CAMPO_PC 4

/* jmp qword ptr [rip+disp32]: o disp32 (a partir do byte 2) leva a REL32 contra o __imp_ da funcao. */
static const char TR_JMP_IMPORT[] = {(char)0xFF, 0x25, 0, 0, 0, 0};
#define TR_OFF_DISP_THUNK 2u

static const ObjSimbolo *TrSimbolo(const Objeto *o, uint32_t i)
{
    return VecEm(&o->simbolos, i);
}

/* ---------- leitura dos membros ---------- */

/* ---------- membros ELF (alvo Linux, M5b P36): a uniao vai direto no objeto de saida ---------- */

/* Secoes do membro entram como estao; mapa[s] = o indice da secao na saida. */
static void TrCopiarSecoes(Objeto *saida, const Objeto *ele, Vec *mapa)
{
    for (size_t s = 0; s < ele->secoes.n; s++) {
        const ObjSecao *orig = VecEm(&ele->secoes, s);
        uint32_t nova = ObjSecaoAbrir(saida, orig->nome, orig->tipo, orig->alinhamento);
        ObjSecao *dest = VecEm(&saida->secoes, nova);

        if (orig->bytes.tam > 0)
            BufAnexarN(&dest->bytes, orig->bytes.dados, orig->bytes.tam);
        dest->tam_bss = orig->tam_bss;
        *(uint32_t *)VecEmpurrar(mapa) = nova;
    }
}

/* O membro ELF que entra na uniao: a saida, o membro lido, os mapas (secao e simbolo) e o indice do membro (o prefixo dos locais). */
typedef struct {
    Objeto *saida;
    const Objeto *ele;
    Vec mapa_sec;
    Vec mapa_sim;
    uint32_t indice;
    Diag *erro;
} TrUniaoElf;

/* Define o simbolo global do membro na saida: indefinido vira definido; fraco nao troca o definido; forte troca o fraco; dois fortes = X0903 (o ld recusa igual). */
static bool TrDefinirElf(TrUniaoElf *u, const ObjSimbolo *s, const char *nome, int32_t secao)
{
    int64_t prev = ObjAcharSimbolo(u->saida, nome);
    ObjSimbolo *p = prev >= 0 ? VecEm(&u->saida->simbolos, (size_t)prev) : NULL;

    if (p == NULL || p->secao == OBJ_SECAO_INDEFINIDA) {
        uint32_t idx = ObjSimboloDefinir(u->saida, nome, secao, s->off);
        ((ObjSimbolo *)VecEm(&u->saida->simbolos, idx))->fraco = s->fraco;
        return true;
    }
    if (s->fraco)
        return true;
    if (!p->fraco)
        return DiagRelatar(u->erro, X0903, 0, "ELF: simbolo '%s' definido em dois membros de biblioteca estatica", nome);
    p->secao = secao;
    p->off = s->off;
    p->fraco = false;
    return true;
}

/* Um simbolo do membro entra na saida; o local ganha ".m<indice>" na frente (so o membro o enxerga, e o nome segue local). mapa_sim[j] = indice dele na saida. */
static bool TrCopiarSimbolos(TrUniaoElf *u)
{
    for (size_t j = 0; j < u->ele->simbolos.n; j++) {
        const ObjSimbolo *s = VecEm(&u->ele->simbolos, j);
        Buf local = {0};
        const char *nome = s->nome;
        bool ok = true;

        if (!s->global) {
            BufFormatar(&local, TR_PREFIXO_LOCAL "%s", u->indice, s->nome);
            nome = local.dados;
        }
        if (s->secao != OBJ_SECAO_INDEFINIDA)
            ok = TrDefinirElf(u, s, nome, (int32_t)*(const uint32_t *)VecEm(&u->mapa_sec, (size_t)s->secao));
        if (ok) {
            uint32_t idx = ObjSimboloReferir(u->saida, nome);
            if (s->fraco)
                ((ObjSimbolo *)VecEm(&u->saida->simbolos, idx))->fraco = true;
            *(uint32_t *)VecEmpurrar(&u->mapa_sim) = idx;
        }
        BufLiberar(&local);
        if (!ok)
            return false;
    }
    return true;
}

/* A convencao do Objeto neutro: o campo PC-relativo acaba 4 bytes depois do P, entao o adendo do ELF (que ja traz o -4) ganha +4. */
static bool TrRelativoAoPc(ObjTipoReloc tipo)
{
    return tipo == OR_REL32 || tipo == OR_GOTPCREL || tipo == OR_GOTPCRELX;
}

static void TrCopiarRelocs(TrUniaoElf *u)
{
    for (size_t r = 0; r < u->ele->relocs.n; r++) {
        const ObjReloc *rel = VecEm(&u->ele->relocs, r);
        ObjCorrecao c = rel->correcao;

        c.simbolo = *(const uint32_t *)VecEm(&u->mapa_sim, c.simbolo);
        if (TrRelativoAoPc(c.tipo))
            c.adendo += TR_FIM_CAMPO_PC;
        ObjRelocar(u->saida, (ObjAlvoReloc){*(const uint32_t *)VecEm(&u->mapa_sec, rel->alvo.secao), rel->alvo.off}, c);
    }
}

/* Um membro ELF inteiro: le (X0105/X0110 do leitor, com o membro) e junta secoes, simbolos e relocacoes. */
static bool TrUnirElf(Objeto *saida, const EstaticaMembro *m, uint32_t indice, Diag *erro)
{
    Objeto ele;
    ElfLErro e = {0};
    TrUniaoElf u = {.saida = saida, .ele = &ele, .indice = indice, .erro = erro};
    bool ok = true;

    ObjIniciar(&ele);
    VecIniciar(&u.mapa_sec, sizeof(uint32_t));
    VecIniciar(&u.mapa_sim, sizeof(uint32_t));
    if (ElfLerObjeto(m->dados, m->tam, &ele, &e) != ELFL_OK) {
        ok = ArRelatarMembroElf(erro, m->nome, &e);
    } else {
        TrCopiarSecoes(saida, &ele, &u.mapa_sec);
        ok = TrCopiarSimbolos(&u);
        if (ok)
            TrCopiarRelocs(&u);
    }
    VecLiberar(&u.mapa_sec);
    VecLiberar(&u.mapa_sim);
    ObjLiberar(&ele);
    return ok;
}

/* inicios (uint32_t por membro) = quantas secoes a uniao tinha antes dele: a faixa de secoes de cada membro, para o libmem (M6d). */
static bool TrLerMembros(const TerceirosPedido *p, CoffUniao *u, Vec *inicios, Diag *erro)
{
    for (size_t k = 0; k < p->membros->n; k++) {
        const EstaticaMembro *m = VecEm(p->membros, k);

        CoffMembroEntrada entrada = {m->dados, m->tam, m->nome};

        *(uint32_t *)VecEmpurrar(inicios) = (uint32_t)u->o->secoes.n;
        if (ArEhElf(m->dados, m->tam)) {
            if (p->formato != IMAGEM_ELF)
                return DiagRelatar(erro, X0105, 0, "biblioteca estatica, membro '%s': formato ELF no alvo Windows (so o alvo Linux liga ELF)", m->nome);
            if (!TrUnirElf(u->o, m, (uint32_t)k, erro))
                return false;
            continue;
        }
        if (p->formato == IMAGEM_ELF)
            return DiagRelatar(erro, X0105, 0, "biblioteca estatica, membro '%s': formato COFF no alvo Linux (o alvo Linux liga so ELF)", m->nome);
        if (!CoffUniaoLerMembro(u, &entrada, erro))
            return false;
    }
    return true;
}

/* ---------- fracos ---------- */

/* Sem definicao forte, o fraco passa a valer o padrao (se o padrao esta definido; padrao absoluto = fraco indefinido, que o ligador recusa se alguem o citar). */
static void TrFecharFraco(Objeto *o, const CoffFraco *f)
{
    const ObjSimbolo *fraco = TrSimbolo(o, f->simbolo);
    const ObjSimbolo *padrao;
    const char *nome = fraco->nome;

    if (fraco->secao != OBJ_SECAO_INDEFINIDA || f->padrao == COFF_SIMBOLO_NAO_MAPEADO)
        return;
    padrao = TrSimbolo(o, f->padrao);
    if (padrao->secao >= 0)
        ObjSimboloDefinir(o, nome, padrao->secao, padrao->off);
}

static void TrFecharFracos(CoffUniao *u)
{
    for (size_t k = 0; k < u->fracos.n; k++)
        TrFecharFraco(u->o, VecEm(&u->fracos, k));
}

/* ---------- comuns ---------- */

static uint32_t TrAlinhamentoDoComum(const CoffUniao *u, const char *nome, uint32_t tam)
{
    uint32_t alinha = 1;

    for (size_t k = 0; k < u->alinhamentos.n; k++) {
        const CoffDiretiva *d = VecEm(&u->alinhamentos, k);

        if (strcmp(d->nome, nome) == 0 && d->valor <= TR_LOG2_MAX)
            return 1u << d->valor;
    }
    while (alinha < tam && alinha < TR_ALINHA_MAX_COMUM)
        alinha <<= 1;
    return alinha;
}

static void TrFecharComum(CoffUniao *u, const CoffComum *c)
{
    const ObjSimbolo *sim = TrSimbolo(u->o, c->simbolo);
    const char *nome = sim->nome;
    uint32_t secao;

    if (sim->secao != OBJ_SECAO_INDEFINIDA)
        return;
    secao = ObjSecaoAbrir(u->o, TR_SECAO_COMUM, OS_BSS, TrAlinhamentoDoComum(u, nome, c->tam));
    ((ObjSecao *)VecEm(&u->o->secoes, secao))->tam_bss = c->tam;
    ObjSimboloDefinir(u->o, nome, (int32_t)secao, 0);
}

static void TrFecharComuns(CoffUniao *u)
{
    for (size_t k = 0; k < u->comuns.n; k++)
        TrFecharComum(u, VecEm(&u->comuns, k));
}

/* ---------- slots __imp_ das raizes e thunks ---------- */

void TerceirosCriarCelula(Objeto *o, const char *nome)
{
    Buf imp = {0};
    uint32_t secao = ObjSecaoAbrir(o, TR_SECAO_CELULA, OS_RDATA, TR_ALINHA_CELULA);
    static const char zeros[TR_TAM_CELULA] = {0};

    BufAnexarN(&((ObjSecao *)VecEm(&o->secoes, secao))->bytes, zeros, sizeof zeros);
    BufFormatar(&imp, TR_PREFIXO_IMP "%s", nome);
    ObjSimboloDefinir(o, imp.dados, (int32_t)secao, 0);
    ObjRelocar(o, (ObjAlvoReloc){secao, 0}, (ObjCorrecao){OR_ADDR64, ObjSimboloReferir(o, nome), 0});
    BufLiberar(&imp);
}

static void TrCriarCelulas(const TerceirosPedido *p, Objeto *o)
{
    for (uint32_t k = 0; k < p->qtd_celulas; k++)
        TerceirosCriarCelula(o, p->celulas[k]);
}

static bool TrNaTabela(const TerceirosPedido *p, const char *nome)
{
    for (uint32_t t = 0; t < p->qtd_tabelas; t++) {
        for (uint32_t s = 0; s < p->tabelas[t].qtd; s++) {
            if (strcmp(p->tabelas[t].simbolos[s], nome) == 0)
                return true;
        }
    }
    return false;
}

/* Outro objeto da ligacao (programa, runtime, Ryu) ja define o nome (ex.: o memcpy do runtime): o simbolo de terceiros liga nele, sem thunk para a DLL. */
static bool TrDefinidoFora(const TerceirosPedido *p, const char *nome)
{
    for (uint32_t k = 0; k < p->qtd_outros; k++) {
        const Vec *simbolos = &p->outros[k]->simbolos;

        for (size_t i = 0; i < simbolos->n; i++) {
            const ObjSimbolo *s = VecEm(simbolos, i);

            if (s->global && s->secao >= 0 && strcmp(s->nome, nome) == 0)
                return true;
        }
    }
    return false;
}

static bool TrPrecisaThunk(const TerceirosPedido *p, const ObjSimbolo *s)
{
    return s->global && s->secao == OBJ_SECAO_INDEFINIDA && strncmp(s->nome, TR_PREFIXO_IMP, TR_TAM_PREFIXO_IMP) != 0 && TrNaTabela(p, s->nome) && !TrDefinidoFora(p, s->nome);
}

static void TrCriarThunk(Objeto *o, const char *nome)
{
    Buf imp = {0};
    uint32_t secao = ObjSecaoAbrir(o, TR_SECAO_THUNK, OS_TEXTO, TR_ALINHA_THUNK);

    BufAnexarN(&((ObjSecao *)VecEm(&o->secoes, secao))->bytes, TR_JMP_IMPORT, sizeof TR_JMP_IMPORT);
    BufFormatar(&imp, TR_PREFIXO_IMP "%s", nome);
    ObjSimboloDefinir(o, nome, (int32_t)secao, 0);
    ObjRelocar(o, (ObjAlvoReloc){secao, TR_OFF_DISP_THUNK}, (ObjCorrecao){OR_REL32, ObjSimboloReferir(o, imp.dados), 0});
    BufLiberar(&imp);
}

/* So os indefinidos que ja existiam antes dos thunks (criar thunk cria o __imp_ indefinido, que nao pede outro). */
static void TrCriarThunks(const TerceirosPedido *p, Objeto *o)
{
    size_t antes = o->simbolos.n;

    for (size_t i = 0; i < antes; i++) {
        const ObjSimbolo *s = TrSimbolo(o, (uint32_t)i);
        const char *nome = s->nome;

        if (TrPrecisaThunk(p, s))
            TrCriarThunk(o, nome);
    }
}

/* ---------- secoes anuladas e ordem ---------- */

static void TrEsvaziarSecao(ObjSecao *s)
{
    BufLiberar(&s->bytes);
    s->bytes = (Buf){0};
    s->tam_bss = 0;
}

static bool TrEstaAnulada(const Vec *marcas, uint32_t secao)
{
    return *(const uint8_t *)VecEm(marcas, secao) != 0;
}

static void TrDescartarRelocs(Objeto *o, const Vec *marcas)
{
    size_t gravadas = 0;

    for (size_t r = 0; r < o->relocs.n; r++) {
        const ObjReloc *rel = VecEm(&o->relocs, r);

        if (!TrEstaAnulada(marcas, rel->alvo.secao))
            *(ObjReloc *)VecEm(&o->relocs, gravadas++) = *rel;
    }
    o->relocs.n = gravadas;
}

/* Secao COMDAT que perdeu (e as associadas a ela): sem bytes e sem relocacoes, entao nada dela entra no PE nem arrasta outra secao. */
static void TrAnular(const CoffUniao *u)
{
    Objeto *o = u->o;
    Vec marcas;

    VecIniciar(&marcas, sizeof(uint8_t));
    for (size_t k = 0; k < o->secoes.n; k++)
        VecEmpurrar(&marcas);
    for (size_t k = 0; k < u->anuladas.n; k++) {
        uint32_t secao = *(const uint32_t *)VecEm(&u->anuladas, k);

        *(uint8_t *)VecEm(&marcas, secao) = 1;
        TrEsvaziarSecao(VecEm(&o->secoes, secao));
    }
    TrDescartarRelocs(o, &marcas);
    VecLiberar(&marcas);
}

typedef struct {
    const char *nome;
    uint32_t antigo;
} TrChave;

static int TrComparar(const void *a, const void *b)
{
    const TrChave *x = a;
    const TrChave *y = b;
    int c = strcmp(x->nome, y->nome);

    if (c != 0)
        return c;
    return (x->antigo > y->antigo) - (x->antigo < y->antigo);
}

static void TrRenumerar(Objeto *o, const Vec *posicao)
{
    for (size_t i = 0; i < o->simbolos.n; i++) {
        ObjSimbolo *s = VecEm(&o->simbolos, i);

        if (s->secao >= 0)
            s->secao = (int32_t)*(const uint32_t *)VecEm(posicao, (size_t)s->secao);
    }
    for (size_t r = 0; r < o->relocs.n; r++) {
        ObjReloc *rel = VecEm(&o->relocs, r);

        rel->alvo.secao = *(const uint32_t *)VecEm(posicao, rel->alvo.secao);
    }
}

/* Ordena pelo nome inteiro (o sufixo depois do '$' decide a ordem dentro do grupo), estavel pela ordem de chegada, e renumera simbolos e relocacoes. */
static void TrOrdenar(Objeto *o)
{
    size_t n = o->secoes.n;
    TrChave *chaves = malloc((n + 1) * sizeof *chaves);
    Vec novas, posicao;

    if (chaves == NULL)
        FalhaInterna("terceiros: sem memoria para ordenar %zu secoes", n);
    VecIniciar(&novas, sizeof(ObjSecao));
    VecIniciar(&posicao, sizeof(uint32_t));
    for (size_t k = 0; k < n; k++) {
        chaves[k] = (TrChave){((const ObjSecao *)VecEm(&o->secoes, k))->nome, (uint32_t)k};
        VecEmpurrar(&posicao);
    }
    qsort(chaves, n, sizeof *chaves, TrComparar);
    for (size_t k = 0; k < n; k++) {
        *(ObjSecao *)VecEmpurrar(&novas) = *(const ObjSecao *)VecEm(&o->secoes, chaves[k].antigo);
        *(uint32_t *)VecEm(&posicao, chaves[k].antigo) = (uint32_t)k;
    }
    TrRenumerar(o, &posicao);
    VecLiberar(&o->secoes);
    o->secoes = novas;
    VecLiberar(&posicao);
    free(chaves);
}

/* ---------- entrada ---------- */

static void TrRelatar(const CoffUniao *u, TerceirosRelato *relato)
{
    relato->ignoradas = u->ignoradas;
    relato->primeira_ignorada = u->primeira_ignorada;
    for (size_t k = 0; relato->exportados != NULL && k < u->exportados.n; k++)
        *(const char **)VecEmpurrar(relato->exportados) = *(const char *const *)VecEm(&u->exportados, k);
}

bool TerceirosMontar(const TerceirosPedido *pedido, Objeto *saida, TerceirosRelato *relato, Diag *erro)
{
    CoffUniao u;
    Vec inicios;
    bool ok;

    CoffUniaoIniciar(&u, saida);
    VecIniciar(&inicios, sizeof(uint32_t));
    ok = TrLerMembros(pedido, &u, &inicios, erro);
    if (ok && pedido->libmem != NULL && pedido->libmem->qtd_slots > 0) {
        LibmemRedirecionar(saida, pedido->membros, &inicios, pedido->libmem);
        LibmemTabela(saida, pedido->libmem);
    }
    VecLiberar(&inicios);
    if (ok) {
        TrFecharFracos(&u);
        TrFecharComuns(&u);
        TrCriarCelulas(pedido, saida);
        TrCriarThunks(pedido, saida);
        TrAnular(&u);
        TrOrdenar(saida);
        relato->tem_ctors = TerceirosConstrutores(saida);
        TrRelatar(&u, relato);
    }
    CoffUniaoLiberar(&u);
    return ok;
}
