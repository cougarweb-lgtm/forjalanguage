/* passes.c - gerenciador de passes de otimizacao da BIR (ver passes.h).
 * Versao: 0.18.0
 * Changelog: 0.18.0 - G0d: OtimExecutar recebe OtimRelatos; item passes do --debug-code: a BIR inteira na entrada e depois de cada passe que rodou (RelatarBir, AplicarERelatar).
 *            0.17.0 - M8 P2: passe `perfil` (OtimPerfilAplicar, perfil.c) fora da tabela (nao e otimizacao: --sem-otim nao o aceita): com BirPrograma.perfil roda UMA vez, depois do `layout` e antes do `alocador`, tambem em dev (onde nenhum outro passe roda) e com --sem-otim tudo.
 *            0.16.0 - S4 (costura): passe `costura` (OtimCosturaAplicar, costura.c), depois do `cfg` e antes da `numeracao`: bloco pequeno puro que desvia num CMP decidido pela aresta de entrada vira copia nessa aresta (jump threading com faixa do parametro).
 *            0.15.0 - S3 (SIMD): passe `slp` (OtimSlpAplicar, slp.c), depois do `inline` e antes do `sroa`: 4 gravacoes F32 (2 F64) contiguas da mesma conta lane a lane viram vetor. 0.14.1 - R6a-3: OtimExecutar grava BirPrograma.endereco_ligado (o `licm` so deixa o add x, imm de endereco com o `endereco` ligado; --sem-otim endereco volta a ser neutro). 0.14.0 - R6a-3: passe `endereco` (OtimEnderecoAplicar, endereco.c), depois do `licm` (que sobe o `add x, imm` da cadeia) e antes do `layout`/alocador.
 *            0.13.0 - R6a: passe `licm` (OtimLicmAplicar, licm.c), depois da `numeracao` (constante e copia ja propagadas) e antes do `layout` (que so renumera os blocos).
 *            0.12.0 - R5 (SIMD): passe `selecao` (OtimSelecaoAplicar, selecao.c), depois do `sroa` e antes do `cfg` (o cfg funde a juncao na cabeca; a selecao ja remove os blocos dos bracos).
 *            0.11.0 - M7b R4b: passes `cfg` (OtimCfgAplicar, cfg.c; antes da numeracao) e `layout` (OtimLayoutAplicar, layout.c; depois da numeracao, antes do alocador).
 *            0.10.0 - M7b R3: passe `numeracao` (OtimNumeracaoAplicar, numeracao.c), depois do `sroa` e antes do alocador: copia propagada, conta e carga repetidas por EBB e dominador.
 *            0.9.0- M7b F3: passe `sroa` (OtimSroaAplicar, sroa.c), depois do `inline` e antes do alocador: agregado local pequeno que nao escapa vira um vreg por campo.
 *            0.8.1 -M7b F2 rodada 3: passe de programa (aplicar_programa) valida a BIR uma vez ao fim, nao uma vez por funcao.
 *            0.8.0 - M7b F2: passe `inline` (OtimInlineAplicar, inline.c), depois da copia_arg e antes do alocador; PasseOtim.aplicar_programa; o relato do passe traz as linhas `inline ...`.
 *            0.7.0 - M7b F1: passe `divconst` (X64DivConstMarcar, x64_isel_otim_div.c), depois do `isel`: divisao e resto por constante viram multiplicacao e deslocamento.
 *            0.6.0 - M7a: passe `copia_arg` (OtimCopiaArgAplicar, copia_arg.c), depois da checagem e antes do alocador: tira a copia do argumento agregado quando o chamado so le.
 *            0.5.0 - M7a F6: passe `checagem` (OtimChecagemAplicar, checagem.c), antes do alocador: tira a checagem de indice que o laco contado prova.
 *            0.4.0 - M7a F5: passe `isel` (X64IselMarcar, x64_isel_otim.c).
 *            0.3.0 - M7a F3: passe `alocador` (x64_regalloc.c); aplicar recebe o programa.
 *            0.2.0 - M7a F1: FORJA_BIR_ANALISES=1 (modo teste) roda as analises (analises.h) em toda funcao e confere as invariantes, sem alterar a BIR.
 *            0.1.0 - M7a F0: tabela vazia, OtimExecutar, OtimConferirLista, relato do item otim do --debug-code.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --selftest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/analises.h"
#include "nativo/optimizer/cfg.h"
#include "nativo/optimizer/checagem.h"
#include "nativo/optimizer/endereco.h"
#include "nativo/optimizer/copia_arg.h"
#include "nativo/optimizer/costura.h"
#include "nativo/optimizer/inline.h"
#include "nativo/optimizer/licm.h"
#include "nativo/optimizer/numeracao.h"
#include "nativo/optimizer/passes.h"
#include "nativo/optimizer/perfil.h"
#include "nativo/optimizer/selecao.h"
#include "nativo/optimizer/slp.h"
#include "nativo/optimizer/sroa.h"
#include "nativo/x64/x64_isel_otim.h"
#include "nativo/x64/x64_regalloc.h"

/* A tabela termina na sentinela {NULL}; passe novo entra antes dela, na ordem em que roda. O alocador e o isel vao por ultimo: nao mudam a BIR, o alocador diz onde cada
 * vreg vive e o isel liga a selecao pos-alocador (coalescing, cmp+jcc, enderecos com indice). */
static const PasseOtim PASSES[] = {
    {"checagem", OtimChecagemAplicar, true, NULL},   /* muda a BIR: antes do alocador */
    {"copia_arg", OtimCopiaArgAplicar, true, NULL},  /* muda a BIR: depois da checagem, antes do alocador */
    {"inline", NULL, true, OtimInlineAplicar},       /* muda a BIR (programa inteiro): depois da copia_arg, antes do alocador */
    {"slp", NULL, true, OtimSlpAplicar},             /* muda a BIR (programa inteiro): depois do inline, antes do sroa (o agregado ainda esta no slot e as gravacoes ainda sao memoria) */
    {"sroa", NULL, true, OtimSroaAplicar},           /* muda a BIR (programa inteiro): depois do slp, antes do alocador */
    {"selecao", NULL, true, OtimSelecaoAplicar},     /* muda a BIR (programa inteiro): depois do sroa, antes do cfg (ele funde a juncao na cabeca; a selecao ja tira os blocos dos bracos) */
    {"cfg", OtimCfgAplicar, true, NULL},             /* muda a BIR: depois do sroa, antes da numeracao (que explora as cadeias de bloco de entrada unica) */
    {"costura", OtimCosturaAplicar, true, NULL},     /* muda a BIR (por funcao): depois do cfg (blocos ja fundidos), antes da numeracao (que propaga as copias das arestas) */
    {"numeracao", NULL, true, OtimNumeracaoAplicar}, /* muda a BIR (programa inteiro): depois do sroa (ele cria os vregs de campo), antes do alocador */
    {"licm", NULL, true, OtimLicmAplicar},           /* muda a BIR (programa inteiro): depois da numeracao (constante e copia ja propagadas), antes do layout (que so renumera) */
    {"endereco", NULL, true, OtimEnderecoAplicar},   /* muda a BIR (programa inteiro): depois do licm (que sobe o add x, imm da cadeia), antes do layout e do alocador */
    {"layout", OtimLayoutAplicar, true, NULL},       /* so renumera os blocos: por ultimo antes do alocador */
    {"alocador", X64AlocadorAplicar, true, NULL},
    {"isel", X64IselMarcar, true, NULL},
    {"divconst", X64DivConstMarcar, true, NULL},     /* so age com o isel ligado: quem emite e a selecao pos-alocador */
    {NULL, NULL, false, NULL},
};

/* Fora da tabela: --perfil nao e otimizacao. Roda antes do alocador (OtimExecutar), que ja salva o que vive na chamada. */
static const PasseOtim PASSE_PERFIL = {"perfil", NULL, false, OtimPerfilAplicar};

uint32_t OtimQtdPasses(void)
{
    uint32_t n = 0;

    while (PASSES[n].nome != NULL)
        n++;
    return n;
}

const char *OtimNomePasse(uint32_t k)
{
    return PASSES[k].nome;
}

/* O trecho [p, p+tam) e o nome de algum passe? */
static bool NomeEhPasse(const char *p, size_t tam)
{
    for (uint32_t k = 0; k < OtimQtdPasses(); k++) {
        if (strlen(PASSES[k].nome) == tam && strncmp(PASSES[k].nome, p, tam) == 0)
            return true;
    }
    return false;
}

/* "nome1, nome2" ou "nenhum passe registrado" (a lista dos passos validos da mensagem de erro). */
static void ListarPasses(char *saida, size_t n)
{
    size_t usado = 0;

    saida[0] = '\0';
    if (OtimQtdPasses() == 0) {
        snprintf(saida, n, "nenhum passe registrado");
        return;
    }
    for (uint32_t k = 0; k < OtimQtdPasses() && usado + 1 < n; k++)
        usado += (size_t)snprintf(saida + usado, n - usado, "%s%s", k > 0 ? ", " : "", PASSES[k].nome);
}

bool OtimConferirLista(const char *lista, char *causa, size_t n)
{
    const char *p = lista;
    char validos[200];

    while (*p != '\0') {
        const char *fim = strchr(p, ',');
        size_t tam = fim != NULL ? (size_t)(fim - p) : strlen(p);

        if (!(tam == 4 && strncmp(p, "tudo", 4) == 0) && !NomeEhPasse(p, tam)) {
            ListarPasses(validos, sizeof validos);
            snprintf(causa, n, "--sem-otim: passo desconhecido '%.*s' (validos: tudo, %s)", (int)tam, p, validos);
            return false;
        }
        p += tam + (fim != NULL ? 1 : 0);
    }
    if (lista[0] == '\0') {
        ListarPasses(validos, sizeof validos);
        snprintf(causa, n, "--sem-otim pede um passo (validos: tudo, %s)", validos);
        return false;
    }
    return true;
}

/* O passe esta em sem_otim ("tudo" desliga todos)? */
static bool Desligado(const char *sem_otim, const char *nome)
{
    const char *p = sem_otim;

    while (p != NULL && *p != '\0') {
        const char *fim = strchr(p, ',');
        size_t tam = fim != NULL ? (size_t)(fim - p) : strlen(p);

        if ((tam == 4 && strncmp(p, "tudo", 4) == 0) || (strlen(nome) == tam && strncmp(nome, p, tam) == 0))
            return true;
        p += tam + (fim != NULL ? 1 : 0);
    }
    return false;
}

static uint32_t ContarInstrucoes(const BirFuncao *f)
{
    uint32_t n = 0;

    for (uint32_t b = 0; b < f->nblocos; b++)
        n += f->blocos[b].n;
    return n;
}

/* Quantos passes rodam (ligados e nao desligados) neste programa; o dev nunca roda. */
static uint32_t ContarAtivos(const BirPrograma *p, const char *sem_otim)
{
    uint32_t n = 0;

    if (p->modo != BM_RELEASE)
        return 0;
    for (uint32_t k = 0; k < OtimQtdPasses(); k++) {
        if (PASSES[k].ligado_por_padrao && !Desligado(sem_otim, PASSES[k].nome))
            n++;
    }
    return n;
}

/* Modo teste (FORJA_BIR_VALIDAR=1): a BIR continua valida depois do passe? */
static bool ValidarDepois(const BirPrograma *p, const PasseOtim *passe, const BirFuncao *f, Diag *erro)
{
    char causa[sizeof erro->causa];
    static int ligado = -1; /* getenv por funcao x passe custava ~50 ms no corpus de 20 mil funcoes: le uma vez */

    if (ligado < 0) {
        const char *e = getenv("FORJA_BIR_VALIDAR");

        ligado = e != NULL && strcmp(e, "1") == 0;
    }
    if (!ligado || BirValidar(p, erro))
        return true;
    snprintf(causa, sizeof causa, "%s", erro->causa);
    return DiagRelatar(erro, X0904, 0, "BIR invalida depois do passe '%s' na funcao %s: %s", passe->nome, p->simbolos[f->simbolo].nome, causa);
}

/* Modo teste (FORJA_BIR_ANALISES=1): calcula as analises de cada funcao e confere as invariantes; nao altera a BIR. */
static bool ConferirAnalises(const BirPrograma *p, Diag *erro)
{
    const char *e = getenv("FORJA_BIR_ANALISES");

    if (e == NULL || strcmp(e, "1") != 0)
        return true;
    for (uint32_t i = 0; i < p->nfuncoes; i++) {
        AnalisesBir analises;
        char causa[200];
        bool ok;

        if (p->funcoes[i].nblocos == 0)
            continue;
        AnalisesCalcular(&p->funcoes[i], &analises);
        ok = AnalisesConferir(&p->funcoes[i], &analises, causa, sizeof causa);
        AnalisesLiberar(&analises);
        if (!ok)
            return DiagRelatar(erro, X0904, 0, "analises da BIR inconsistentes na funcao %s: %s", p->simbolos[p->funcoes[i].simbolo].nome, causa);
    }
    return true;
}

/* Item passes (G0d): "== <rotulo>" e a BIR inteira; o BirImprimir e canonico (numeracao dos vregs/blocos/simbolos, sem endereco), entao o texto repete byte a byte. */
static void RelatarBir(const BirPrograma *programa, Buf *passes, const char *rotulo)
{
    if (passes == NULL)
        return;
    BufFormatar(passes, "== %s\n", rotulo);
    BirImprimir(programa, passes);
}

/* Roda um passe em todas as funcoes (ou uma vez no programa, se tem aplicar_programa) e relata instrucoes antes e depois de cada funcao. */
static bool AplicarPasse(BirPrograma *programa, const PasseOtim *passe, Buf *relato, Diag *erro)
{
    uint32_t *antes = calloc(programa->nfuncoes + 1u, sizeof *antes);

    if (antes == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t i = 0; passe->aplicar_programa != NULL && i < programa->nfuncoes; i++)
        antes[i] = ContarInstrucoes(&programa->funcoes[i]);
    if (passe->aplicar_programa != NULL)
        passe->aplicar_programa(programa, relato);
    for (uint32_t i = 0; i < programa->nfuncoes; i++) {
        BirFuncao *f = &programa->funcoes[i];

        if (passe->aplicar_programa == NULL) {
            antes[i] = ContarInstrucoes(f);
            passe->aplicar(programa, f);
        }
        if (relato != NULL)
            BufFormatar(relato, "passe %s funcao %s antes=%u depois=%u\n", passe->nome, programa->simbolos[f->simbolo].nome, (unsigned)antes[i],
                        (unsigned)ContarInstrucoes(f));
        if (passe->aplicar_programa == NULL && !ValidarDepois(programa, passe, f, erro)) {
            free(antes);
            return false;
        }
    }
    free(antes);
    /* o BirValidar confere o programa inteiro: num passe de programa uma vez basta (por funcao era quadratico com FORJA_BIR_VALIDAR=1); a causa traz a funcao */
    return passe->aplicar_programa == NULL || programa->nfuncoes == 0 || ValidarDepois(programa, passe, &programa->funcoes[0], erro);
}

/* Passe + o dump da BIR depois dele (item passes). */
static bool AplicarERelatar(BirPrograma *programa, const PasseOtim *passe, const OtimRelatos *relatos, Diag *erro)
{
    if (!AplicarPasse(programa, passe, relatos->contadores, erro))
        return false;
    if (relatos->passes != NULL) {
        char rotulo[64];

        snprintf(rotulo, sizeof rotulo, "passe %s", passe->nome);
        RelatarBir(programa, relatos->passes, rotulo);
    } /* o NULL fica fora: o rotulo so e montado com o item pedido */
    return true;
}

bool OtimExecutar(BirPrograma *programa, const char *sem_otim, const OtimRelatos *relatos, Diag *erro)
{
    static const OtimRelatos NENHUM = {NULL, NULL};

    if (relatos == NULL)
        relatos = &NENHUM;
    if (!ConferirAnalises(programa, erro))
        return false;
    if (relatos->contadores != NULL)
        BufFormatar(relatos->contadores, "otim: %u passes\n", (unsigned)ContarAtivos(programa, sem_otim));
    RelatarBir(programa, relatos->passes, "entrada");
    if (ContarAtivos(programa, sem_otim) == 0)
        return !programa->perfil || AplicarERelatar(programa, &PASSE_PERFIL, relatos, erro);
    programa->endereco_ligado = !Desligado(sem_otim, "endereco");
    for (uint32_t k = 0; k < OtimQtdPasses(); k++) {
        const PasseOtim *passe = &PASSES[k];

        if (programa->perfil && passe->aplicar == X64AlocadorAplicar && !AplicarERelatar(programa, &PASSE_PERFIL, relatos, erro))
            return false;
        if (!passe->ligado_por_padrao || Desligado(sem_otim, passe->nome))
            continue;
        if (!AplicarERelatar(programa, passe, relatos, erro))
            return false;
    }
    return true;
}
