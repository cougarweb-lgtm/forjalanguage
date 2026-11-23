/* bibliotecas.c - resolve os 'link' do foreign em tabelas de imports por DLL (P19 do M3).
 * Versao: 0.5.6
 * Changelog: 0.5.6 - M6d P92c: EstaticaArquivo.link gravado em BibsLerEstatica; o vetor de estaticas passa a ser da saida (BibliotecasResolvidas.estaticas), liberado em BibliotecasLiberar.
 * Changelog: 0.5.5 - M5b P39-B3: BibsCoberta (e a normalizacao de nome) sai para bibliotecas_nomes.c, para o arquivo voltar abaixo de 800 linhas (conferir_regras). Mesmo comportamento.
 *            0.5.4 - M5b P39-B2 (D0654): no alvo linux com semente, "c", "m", "msvcrt" e "ucrt" viram um libc.a so (BibsAdicionarSemente, com foreign); sem 'link "c"' o programa com foreign ja liga a libc.
 *            0.5.3 - M5b P36: BibsAdicionarImplicitas nao entra no alvo linux (busca->elf): c, m e kernel32 sao DLL do Windows.
 *            0.5.2 - M5b P36: BibsDependenciasDoMembro pula membro ELF (sem .drectve; quem recusa ELF no Windows e o X0105 do terceiros.c).
 *            0.5.1 - M3b P23y-2: BibsLerArquivo confere ferror (leitura truncada deixa de ser sucesso); mais de IMAGEM_MAX_DLLS tabelas e X0109 (era FalhaInterna).
 *            0.5.0 - M3b P23f: provedores (BibliotecasPedido): o simbolo que um membro de estatica pede e que o runtime, o Ryu ou o legado definem nao vai para a DLL (provedores_usados diz quais entram); os __imp_
 *                    indefinidos de um provedor com traz_imports usado entram na lista de DLL (a ucrtbase do sprintf/sscanf do legado).
 *            0.4.0 - M3b P23e: estatica (ar) deixa de ser X0103: BibsAdicionarEstatica guarda os bytes na arena e EstaticaSelecionar escolhe os membros (raizes = simbolos do programa); o .drectve dos
 *                    membros (/DEFAULTLIB, -l) vira 'link' implicito ate nao entrar biblioteca nova (X0108 se nao acha); o que nenhuma estatica define (pendentes) e pedido as DLLs, como os simbolos do
 *                    programa (o __imp_ de membro perde o prefixo); simbolo do programa que uma estatica define vira "celula" (slot __imp_ no objeto de terceiros).
 *            0.3.1 - M3 F9b-1: decisoes extraidas de BibliotecasResolver.
 *            0.3.0 - M3 P22-O1a: BibsLerBytes anota o caminho e o Hash64 dos bytes lidos em BibliotecasResolvidas.lidas (o --entradas); BibliotecasLiberar libera o vetor.
 *            0.2.0 - M3 P19b: import library (MSVC curta e GNU .dll.a, via implib.c) e .exe resolvem como fonte de simbolo; uma DllLida por DLL achada DENTRO da import library
 *                    (nome_dll vem dela, a DLL nao precisa estar na pasta de busca). Dado e import por ordinal pedidos de uma import library = X0105 (nao X0101); .exe sem export = X0105.
 *                    A ordem de busca e a dos 'link' como antes; estatica (ar) e formato desconhecido seguem X0103.
 *            0.1.1 - M3 P19c: so comentario (o CRT implicito "c" tambem e o de link "msvcrt"/"ucrt"; BibliotecaAchar manda os quatro para a ucrtbase.dll).
 *            0.1.0 - M3 P19: BibliotecasResolver. Cada 'link' vira uma DLL (BibliotecaAchar + PeLerExportacoes); cada simbolo usado vai para a ULTIMA DLL do 'link' que o exporta
 *                    (como o ld: o contrato dizia a 1a, o gcc com -lA -lB entrega B) e, dentro da DLL, fica na ordem da 1a referencia. Depois dos 'link' entram o CRT (ucrtbase) e o
 *                    kernel32 implicitos, como as bibliotecas que o gcc liga sozinho: foreign de libc sem 'link' (strstr, getenv, labs) segue igual ao C; so fallback. Forwarder entra pelo nome (o loader repassa). Import library e .exe sao X0103 ate o P19b;
 *                    objeto /GL e PE malformado ou de 32 bits sao X0105; link sem arquivo, X0102; simbolo em nenhuma DLL, X0101.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/hash.h"
#include "common/vec.h"
#include "nativo/obj/ar_ler.h"
#include "nativo/obj/bibliotecas.h"
#include "nativo/obj/bibliotecas_nomes.h"
#include "nativo/obj/coff_membro.h"
#include "nativo/obj/implib.h"
#include "nativo/obj/pe_ler.h"

#define BIBS_TAM_BLOCO 65536u
#define BIBS_PREFIXO_IMP "__imp_"
#define BIBS_TAM_PREFIXO_IMP 6u

/* Uma DLL de um 'link', ja lida. caminho e nome_dll moram na arena do resultado. Numa import library, caminho e o da biblioteca e nome_dll vem de dentro dela
 * (uma biblioteca pode trazer mais de uma DLL: uma DllLida por DLL). */
typedef struct {
    const char *link;      /* o 'link' do fonte que trouxe esta DLL (o texto vive no pedido) */
    const char *caminho;
    const char *nome_dll;  /* so o arquivo ("ucrtbase.dll"): o nome que o descritor de import leva */
    PeExportacoes exp;     /* o que a DLL exporta por nome (numa import library: so as funcoes) */
    PeExportacoes indisp;  /* import library: dado e import so por ordinal (ordinal = ImplibTipo); o X0105 vem de quem os pede */
} DllLida;

typedef struct {
    const BibliotecasPedido *pedido;
    BibliotecasResolvidas *saida;
    Diag *erro;
    Vec dlls;        /* DllLida: as dos 'link' na ordem deles, depois as implicitas */
    uint32_t explicitas; /* quantas das primeiras sao dos 'link' */
    int32_t *dono;   /* por simbolo da lista: indice da DLL que o exporta */
    Vec estaticas;   /* EstaticaArquivo: as estaticas dos 'link' e dos .drectve, na ordem; os bytes vivem na arena de saida */
    Vec vistas;      /* const char *: bibliotecas implicitas (.drectve) ja tratadas */
    const char *ultima_estatica;  /* o 'link' da ultima estatica: o link_falho dos erros da selecao */
    const char *const *lista;     /* os simbolos que precisam de DLL: os do programa que nenhuma estatica define e os pendentes dos membros */
    uint32_t qtd_lista;
} Resolucao;

/* O que o gcc liga sem ninguem pedir e o foreign sem 'link' usa (strstr, getenv, labs; GetTickCount64): o CRT ("c", "m", "msvcrt" e "ucrt" dao na mesma ucrtbase.dll) e o kernel32.
 * Entra so se nenhum 'link' ja pediu aquela DLL; link e a 2a grafia que a mesma DLL tem no 'link'. */
typedef struct {
    const char *link;
    const char *outra_grafia;
} BibImplicita;

static const BibImplicita BIBS_IMPLICITAS[] = {{"c", "m"}, {"kernel32", NULL}};

/* Na ordem de BibliotecaTipo. */
static const char *const BIBS_NOMES_TIPO[] = {
    "uma DLL", "um executavel", "uma import library MSVC", "uma import library GNU", "uma biblioteca estatica (ar)", "um objeto /GL", "de formato desconhecido",
    "um ELF dinamico (.so)",
};

static bool BibsLerArquivo(const char *caminho, Buf *bytes)
{
    char bloco[BIBS_TAM_BLOCO];
    FILE *arq = fopen(caminho, "rb");
    size_t n;
    bool lido;

    if (arq == NULL)
        return false;
    while ((n = fread(bloco, 1, sizeof bloco, arq)) > 0)
        BufAnexarN(bytes, bloco, n);
    lido = ferror(arq) == 0;  /* leitura truncada por erro de E/S nao e sucesso */
    fclose(arq);
    return lido;
}

static const char *BibsNomeDoArquivo(const char *caminho)
{
    const char *nome = caminho;

    for (const char *p = caminho; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/')
            nome = p + 1;
    }
    return nome;
}

/* ---------- uma DLL por 'link' ---------- */

static bool BibsAchar(Resolucao *r, const char *link, BibliotecaAchada *achada)
{
    if (BibliotecaAchar(r->pedido->busca, link, achada))
        return true;
    r->saida->link_falho = link;
    if (achada->procurei_em.dados != NULL)
        BufAnexar(&r->saida->procurei_em, achada->procurei_em.dados);
    return DiagRelatar(r->erro, X0102, 0, "biblioteca do 'link' \"%s\" nao encontrada", link);
}

static bool BibsTipoLigavel(BibliotecaTipo tipo)
{
    switch (tipo) {
    case BIB_DLL:
    case BIB_EXE:
    case BIB_IMPORT_MSVC:
    case BIB_IMPORT_GNU:
    case BIB_ESTATICA_AR:
        return true;
    case BIB_OBJETO_GL:
    case BIB_DESCONHECIDO:
    case BIB_ELF_DINAMICO:
        break;
    }
    return false;
}

/* DLL, executavel, import library (MSVC curta, GNU) e estatica (ar) resolvem; objeto /GL (IL) o backend nunca liga. */
static bool BibsTipoAceito(Resolucao *r, const char *link, const BibliotecaAchada *achada)
{
    if (BibsTipoLigavel(achada->tipo))
        return true;
    r->saida->link_falho = link;
    if (achada->tipo == BIB_OBJETO_GL)
        return DiagRelatar(r->erro, X0105, 0, "biblioteca '%s' (link \"%s\") e um objeto /GL (IL do compilador); o backend nativo nao o liga", achada->caminho, link);
    if (achada->tipo == BIB_ELF_DINAMICO)
        return DiagRelatar(r->erro, X0003, 0, "ELF dinamico: S14 (biblioteca '%s', link \"%s\"): o alvo Linux liga so a estatica lib<x>.a do vendor\\linux", achada->caminho, link);
    return DiagRelatar(r->erro, X0103, 0, "biblioteca '%s' (link \"%s\") e %s; o backend nativo liga DLL, executavel, import library e estatica (ar)", achada->caminho,
                       link, BIBS_NOMES_TIPO[achada->tipo]);
}

/* O arquivo lido por inteiro entra em 'lidas' com o hash dos bytes que ja estao na memoria (o --entradas nao relê o arquivo). */
static void BibsAnotarLida(Resolucao *r, const char *caminho, const Buf *bytes)
{
    BibliotecaLida *lida = VecEmpurrar(&r->saida->lidas);

    lida->caminho = ArenaCopiarTexto(&r->saida->memoria, caminho, strlen(caminho));
    lida->hash = Hash64(bytes->dados, bytes->tam, HASH64_SEMENTE);
}

/* Todos os bytes do arquivo da biblioteca; X0105 se nao abre. */
static bool BibsLerBytes(Resolucao *r, const char *link, const char *caminho, Buf *bytes)
{
    if (BibsLerArquivo(caminho, bytes)) {
        BibsAnotarLida(r, caminho, bytes);
        return true;
    }
    r->saida->link_falho = link;
    return DiagRelatar(r->erro, X0105, 0, "nao consegui ler a biblioteca '%s' (link \"%s\")", caminho, link);
}

/* A causa do X0105 que o leitor (PE ou import library) deixou em 'lido' vira a do erro do resolvedor, com a biblioteca e o 'link'. */
static bool BibsRelatarLeitura(Resolucao *r, const char *link, const char *caminho, const Diag *lido)
{
    r->saida->link_falho = link;
    return DiagRelatar(r->erro, X0105, 0, "biblioteca '%s' (link \"%s\"): %s", caminho, link, lido->causa);
}

static bool BibsLerExportacoes(Resolucao *r, const char *link, const char *caminho, PeExportacoes *exp)
{
    Buf bytes = {0};
    Diag lido = {0};
    bool ok = BibsLerBytes(r, link, caminho, &bytes);

    if (!ok)
        return false;
    ok = PeLerExportacoes((const uint8_t *)bytes.dados, bytes.tam, exp, &lido);
    BufLiberar(&bytes);
    return ok || BibsRelatarLeitura(r, link, caminho, &lido);
}

/* Abre uma DllLida no fim do vetor (os ponteiros antigos deixam de valer). */
static DllLida *BibsNovaDll(Resolucao *r, const char *link, const char *caminho)
{
    DllLida *d = VecEmpurrar(&r->dlls);

    *d = (DllLida){.link = link};
    d->caminho = ArenaCopiarTexto(&r->saida->memoria, caminho, strlen(caminho));
    VecIniciar(&d->exp.itens, sizeof(PeExportacao));
    VecIniciar(&d->indisp.itens, sizeof(PeExportacao));
    return d;
}

/* DLL ou EXE: as exportacoes vem do PE. Executavel sem nenhuma exportacao nao serve de fonte de simbolo (X0105). */
static bool BibsAdicionarPe(Resolucao *r, const char *link, const BibliotecaAchada *achada)
{
    DllLida *d = BibsNovaDll(r, link, achada->caminho);

    d->nome_dll = BibsNomeDoArquivo(d->caminho);
    if (!BibsLerExportacoes(r, link, d->caminho, &d->exp))
        return false;
    if (achada->tipo == BIB_EXE && d->exp.itens.n == 0) {
        r->saida->link_falho = link;
        return DiagRelatar(r->erro, X0105, 0, "o executavel '%s' (link \"%s\") nao exporta nenhum simbolo", d->caminho, link);
    }
    return true;
}

/* A import library que esta sendo lida: as DllLida dela comecam em 'desde' no vetor de DLLs. */
typedef struct {
    Resolucao *r;
    const char *link;
    const char *caminho;
    uint32_t desde;
} ImplibEmLeitura;

/* A DllLida da import library com o nome de DLL da entrada; abre uma se ainda nao ha. */
static DllLida *BibsDllDoImplib(const ImplibEmLeitura *l, const ImplibEntrada *e)
{
    Resolucao *r = l->r;
    DllLida *d;

    for (uint32_t k = l->desde; k < r->dlls.n; k++) {
        d = VecEm(&r->dlls, k);
        if (strcmp(d->nome_dll, e->dll) == 0)
            return d;
    }
    d = BibsNovaDll(r, l->link, l->caminho);
    d->nome_dll = ArenaCopiarTexto(&r->saida->memoria, e->dll, strlen(e->dll));
    return d;
}

static void BibsGuardarEntrada(DllLida *d, const ImplibEntrada *e)
{
    PeExportacoes *alvo = e->tipo == IMPLIB_CODIGO ? &d->exp : &d->indisp;
    PeExportacao *x = VecEmpurrar(&alvo->itens);

    x->nome = ArenaCopiarTexto(&alvo->textos, e->nome, strlen(e->nome));
    x->ordinal = (uint32_t)e->tipo;
}

/* Import library: cada entrada vai para a DLL dela (as funcoes em exp, dado e ordinal em indisp). */
static bool BibsAdicionarImplib(Resolucao *r, const char *link, const BibliotecaAchada *achada)
{
    Buf bytes = {0};
    Diag lido = {0};
    ImplibEntradas ent;
    ImplibEmLeitura leitura = {r, link, achada->caminho, r->dlls.n};

    if (!BibsLerBytes(r, link, achada->caminho, &bytes))
        return false;
    bool ok = ImplibLer((const uint8_t *)bytes.dados, bytes.tam, &ent, &lido);
    BufLiberar(&bytes);
    for (size_t i = 0; ok && i < ent.itens.n; i++) {
        const ImplibEntrada *e = VecEm(&ent.itens, i);
        BibsGuardarEntrada(BibsDllDoImplib(&leitura, e), e);
    }
    ImplibLiberar(&ent);
    return ok || BibsRelatarLeitura(r, link, achada->caminho, &lido);
}

/* Estatica (ar): os bytes vao para a arena de saida (os membros puxados apontam para dentro deles) e a biblioteca entra na fila da selecao. */
static bool BibsLerEstatica(Resolucao *r, const char *link, const char *caminho, EstaticaArquivo *arq)
{
    Buf bytes = {0};

    if (!BibsLerBytes(r, link, caminho, &bytes))
        return false;
    arq->nome = ArenaCopiarTexto(&r->saida->memoria, caminho, strlen(caminho));
    arq->bytes = (const uint8_t *)ArenaCopiarTexto(&r->saida->memoria, bytes.dados, bytes.tam);
    arq->n = bytes.tam;
    arq->link = ArenaCopiarTexto(&r->saida->memoria, link, strlen(link));
    BufLiberar(&bytes);
    return true;
}

/* O mesmo arquivo num 'link' repetido: reaproveita os bytes (a selecao reconhece o repetido pelo ponteiro e nao o conta como definicao em duas bibliotecas). */
static bool BibsReaproveitarEstatica(const Resolucao *r, const char *caminho, EstaticaArquivo *arq)
{
    for (size_t k = 0; k < r->estaticas.n; k++) {
        const EstaticaArquivo *e = VecEm(&r->estaticas, k);

        if (strcmp(e->nome, caminho) == 0) {
            *arq = *e;
            return true;
        }
    }
    return false;
}

static bool BibsAdicionarEstatica(Resolucao *r, const char *link, const BibliotecaAchada *achada)
{
    EstaticaArquivo novo = {0};

    r->ultima_estatica = link;
    if (!BibsReaproveitarEstatica(r, achada->caminho, &novo) && !BibsLerEstatica(r, link, achada->caminho, &novo))
        return false;
    *(EstaticaArquivo *)VecEmpurrar(&r->estaticas) = novo;
    return true;
}

static bool BibsAdicionarAchada(Resolucao *r, const char *link, const BibliotecaAchada *achada)
{
    if (!BibsTipoAceito(r, link, achada))
        return false;
    if (achada->tipo == BIB_ESTATICA_AR)
        return BibsAdicionarEstatica(r, link, achada);
    if (achada->tipo == BIB_IMPORT_MSVC || achada->tipo == BIB_IMPORT_GNU)
        return BibsAdicionarImplib(r, link, achada);
    return BibsAdicionarPe(r, link, achada);
}

static bool BibsAdicionarDll(Resolucao *r, const char *link)
{
    BibliotecaAchada achada;
    bool ok = BibsAchar(r, link, &achada) && BibsAdicionarAchada(r, link, &achada);

    BibliotecaLiberar(&achada);
    return ok;
}

/* ---------- que DLL exporta cada simbolo ---------- */

static int BibsComparaNome(const void *chave, const void *item)
{
    return strcmp((const char *)chave, ((const PeExportacao *)item)->nome);
}

static bool BibsExporta(const Resolucao *r, uint32_t k, const char *simbolo)
{
    const DllLida *d = VecEm(&r->dlls, k);

    return d->exp.itens.n > 0 && bsearch(simbolo, d->exp.itens.dados, d->exp.itens.n, sizeof(PeExportacao), BibsComparaNome) != NULL;
}

/* A ULTIMA DLL do 'link' que exporta o simbolo: o ld liga .dll direto e, quando duas exportam o mesmo nome, fica com a do ultimo -l (medido com
 * -lfx_basica -lfx_lixo e a ordem trocada); a tabela de nomes do PE vem ordenada, entao e busca binaria. Sem dono entre os 'link', vale a 1a DLL
 * implicita que o exporta (o CRT e o kernel32 que o gcc liga sozinho). -1 se nenhuma. */
static int32_t BibsDonoDe(const Resolucao *r, const char *simbolo)
{
    for (uint32_t k = r->explicitas; k > 0; k--) {
        if (BibsExporta(r, k - 1, simbolo))
            return (int32_t)(k - 1);
    }
    for (uint32_t k = r->explicitas; k < r->dlls.n; k++) {
        if (BibsExporta(r, k, simbolo))
            return (int32_t)k;
    }
    return -1;
}

static const char *const BIBS_MOTIVO_INDISP[] = {
    "", "e um dado exportado (Type DATA), nao uma funcao; o backend nativo importa so funcoes", "e importado so por ordinal; o backend nativo importa por nome",
};

/* O simbolo existe numa import library mas como dado ou por ordinal: o motivo vem de BIBS_MOTIVO_INDISP; NULL se nenhuma biblioteca o tem assim. */
static const PeExportacao *BibsIndisponivel(const Resolucao *r, const char *simbolo, const DllLida **dona)
{
    for (uint32_t k = 0; k < r->dlls.n; k++) {
        const DllLida *d = VecEm(&r->dlls, k);
        const PeExportacao *x = d->indisp.itens.n == 0 ? NULL : bsearch(simbolo, d->indisp.itens.dados, d->indisp.itens.n, sizeof(PeExportacao), BibsComparaNome);

        if (x != NULL) {
            *dona = d;
            return x;
        }
    }
    return NULL;
}

/* X0105 em vez de X0101: a import library tem o simbolo, mas de um jeito que o backend nao importa. */
static bool BibsSimboloIndisponivel(Resolucao *r, const char *simbolo, const PeExportacao *x, const DllLida *dona)
{
    r->saida->link_falho = dona->link;
    return DiagRelatar(r->erro, X0105, 0, "o simbolo '%s' da import library '%s' (link \"%s\") %s", simbolo, dona->caminho, dona->link, BIBS_MOTIVO_INDISP[x->ordinal]);
}

/* Posicao do simbolo entre os do programa (BibliotecasPedido.simbolos); -1 se quem o pediu foi um membro de estatica. */
static int32_t BibsPosicaoNoPedido(const Resolucao *r, const char *simbolo)
{
    for (uint32_t k = 0; k < r->pedido->qtd_simbolos; k++) {
        if (strcmp(r->pedido->simbolos[k], simbolo) == 0)
            return (int32_t)k;
    }
    return -1;
}

static bool BibsSimboloFaltando(Resolucao *r, uint32_t j)
{
    const DllLida *dona = NULL;
    const PeExportacao *indisp = BibsIndisponivel(r, r->lista[j], &dona);

    if (indisp != NULL)
        return BibsSimboloIndisponivel(r, r->lista[j], indisp, dona);
    for (uint32_t k = 0; k < r->dlls.n; k++) {
        const DllLida *d = VecEm(&r->dlls, k);

        if (k > 0 && strcmp(((const DllLida *)VecEm(&r->dlls, k - 1))->caminho, d->caminho) == 0)
            continue;  /* import library com mais de uma DLL: o arquivo conta uma vez */
        BufAnexar(&r->saida->procurei_em, d->caminho);
        BufAnexar(&r->saida->procurei_em, "\n");
    }
    r->saida->simbolo_faltando = BibsPosicaoNoPedido(r, r->lista[j]);
    r->saida->simbolo_faltando_nome = ArenaCopiarTexto(&r->saida->memoria, r->lista[j], strlen(r->lista[j]));
    return DiagRelatar(r->erro, X0101, 0, "simbolo '%s' nao encontrado nas bibliotecas do 'link'", r->lista[j]);
}

static bool BibsAcharDonos(Resolucao *r)
{
    r->dono = ArenaAlocar(&r->saida->memoria, ((size_t)r->qtd_lista + 1) * sizeof *r->dono);
    for (uint32_t j = 0; j < r->qtd_lista; j++) {
        r->dono[j] = BibsDonoDe(r, r->lista[j]);
        if (r->dono[j] < 0)
            return BibsSimboloFaltando(r, j);
    }
    return true;
}

/* ---------- uma tabela por DLL que fornece algum simbolo ---------- */

static bool BibsMontarTabela(Resolucao *r, uint32_t k)
{
    BibliotecasResolvidas *s = r->saida;
    uint32_t qtd = 0, i = 0;
    const char **nomes;

    for (uint32_t j = 0; j < r->qtd_lista; j++)
        qtd += r->dono[j] == (int32_t)k;
    if (qtd == 0)
        return true;
    if (s->qtd_tabelas == IMAGEM_MAX_DLLS)
        return DiagRelatar(r->erro, X0109, 0, "mais de %d DLLs com import nos 'link' do programa (teto do ligador nativo)", IMAGEM_MAX_DLLS);
    nomes = ArenaAlocar(&s->memoria, qtd * sizeof *nomes);
    for (uint32_t j = 0; j < r->qtd_lista; j++) {
        if (r->dono[j] == (int32_t)k)
            nomes[i++] = ArenaCopiarTexto(&s->memoria, r->lista[j], strlen(r->lista[j]));
    }
    s->tabelas[s->qtd_tabelas++] = (ImportTabela){((const DllLida *)VecEm(&r->dlls, k))->nome_dll, nomes, qtd};
    return true;
}

static void BibsLiberarDlls(Resolucao *r)
{
    for (uint32_t k = 0; k < r->dlls.n; k++) {
        PeExportacoesLiberar(&((DllLida *)VecEm(&r->dlls, k))->exp);
        PeExportacoesLiberar(&((DllLida *)VecEm(&r->dlls, k))->indisp);
    }
    VecLiberar(&r->dlls);
}

static bool BibsPediuLink(const Resolucao *r, const char *nome)
{
    for (uint32_t k = 0; nome != NULL && k < r->pedido->qtd_links; k++) {
        if (strcmp(r->pedido->links[k], nome) == 0)
            return true;
    }
    return false;
}

static bool BibsAdicionarImplicitas(Resolucao *r)
{
    bool ok = true;

    if (r->pedido->busca != NULL && r->pedido->busca->elf)
        return true; /* alvo linux: sem DLL implicita (o CRT e o kernel32 sao do Windows) */
    for (size_t k = 0; ok && k < sizeof BIBS_IMPLICITAS / sizeof BIBS_IMPLICITAS[0]; k++) {
        const BibImplicita *b = &BIBS_IMPLICITAS[k];

        if (!BibsPediuLink(r, b->link) && !BibsPediuLink(r, b->outra_grafia))
            ok = BibsAdicionarDll(r, b->link);
    }
    return ok;
}

/* M5b P39-B2: no alvo linux com semente, "c", "m", "msvcrt" e "ucrt" sao o mesmo libc.a: quem o poe e BibsAdicionarSemente (uma vez so). */
static bool BibsEhLibcDaSemente(const Resolucao *r, const char *link)
{
    const BibliotecaBusca *b = r->pedido->busca;

    if (b == NULL || !b->elf || b->semente == NULL)
        return false;
    return strcmp(link, "c") == 0 || strcmp(link, "m") == 0 || strcmp(link, "msvcrt") == 0 || strcmp(link, "ucrt") == 0;
}

/* Cada 'link' do pedido vira uma ou mais DLLs (ou a estatica do alvo linux); para no 1o que falha. */
static bool BibsAdicionarLinks(Resolucao *r)
{
    bool ok = true;

    for (uint32_t k = 0; ok && k < r->pedido->qtd_links; k++) {
        if (!BibsEhLibcDaSemente(r, r->pedido->links[k]))
            ok = BibsAdicionarDll(r, r->pedido->links[k]);
    }
    return ok;
}

/* M5b P39-B2: programa com foreign no alvo linux liga a libc da semente (libc.a) mesmo sem 'link "c"': o crt1.o chama o main e o musl, e as libs de terceiros pedem a libc. */
static bool BibsAdicionarSemente(Resolucao *r)
{
    if (r->pedido->busca == NULL || r->pedido->busca->semente == NULL || r->pedido->qtd_simbolos == 0)
        return true;
    return BibsAdicionarDll(r, "c");
}

static bool BibsMontarTabelas(Resolucao *r)
{
    for (uint32_t k = 0; k < r->dlls.n; k++) {
        if (!BibsMontarTabela(r, k))
            return false;
    }
    return true;
}

/* ---------- estaticas: selecao, .drectve e pendentes ---------- */

static bool BibsJaTratada(const Resolucao *r, const char *nome)
{
    for (size_t k = 0; k < r->vistas.n; k++) {
        if (strcmp(*(const char *const *)VecEm(&r->vistas, k), nome) == 0)
            return true;
    }
    return BibsPediuLink(r, nome);
}

/* Uma biblioteca que o .drectve do membro pede: ja pedida ou coberta pelo ligador nao entra de novo; senao tem de existir (X0108) e entra como um 'link' a mais (*novas conta). */
static bool BibsDependencia(Resolucao *r, const char *nome, const char *membro, uint32_t *novas)
{
    BibliotecaAchada achada;
    bool ok;

    if (BibsJaTratada(r, nome))
        return true;
    *(const char **)VecEmpurrar(&r->vistas) = nome;
    if (BibsCoberta(nome))
        return true;
    if (!BibliotecaAchar(r->pedido->busca, nome, &achada)) {
        BibliotecaLiberar(&achada);
        r->saida->link_falho = nome;
        return DiagRelatar(r->erro, X0108, 0, "biblioteca '%s' pedida pelo .drectve do membro '%s' nao encontrada nas pastas de busca", nome, membro);
    }
    ok = BibsAdicionarAchada(r, nome, &achada);
    BibliotecaLiberar(&achada);
    (*novas)++;
    return ok;
}

static bool BibsDependenciasDoMembro(Resolucao *r, const EstaticaMembro *m, uint32_t *novas)
{
    Vec nomes;
    CoffDestinoNomes destino = {&r->saida->memoria, &nomes};
    Diag lido = {0};
    bool ok;

    if (ArEhElf(m->dados, m->tam))
        return true;
    VecIniciar(&nomes, sizeof(const char *));
    ok = CoffBibliotecasDoMembro(m->dados, m->tam, &destino, &lido);
    if (!ok) {
        r->saida->link_falho = r->ultima_estatica;
        *r->erro = lido;
    }
    for (size_t k = 0; ok && k < nomes.n; k++)
        ok = BibsDependencia(r, *(const char *const *)VecEm(&nomes, k), m->nome, novas);
    VecLiberar(&nomes);
    return ok;
}

static bool BibsSelecionar(Resolucao *r)
{
    BibliotecasResolvidas *s = r->saida;
    EstaticaPedido ped = {.arquivos = r->estaticas.dados, .qtd_arquivos = (uint32_t)r->estaticas.n, .raizes = r->pedido->simbolos, .qtd_raizes = r->pedido->qtd_simbolos};

    if (s->tem_estaticas)
        EstaticaLiberar(&s->selecao);
    s->tem_estaticas = true;
    if (EstaticaSelecionar(&ped, &s->selecao, r->erro))
        return true;
    s->link_falho = r->ultima_estatica;
    return false;
}

/* Seleciona os membros e le o .drectve de cada um; biblioteca nova (estatica ou DLL) refaz a selecao, ate nao entrar mais nenhuma. */
static bool BibsResolverEstaticas(Resolucao *r)
{
    uint32_t novas;

    if (r->estaticas.n == 0)
        return true;
    do {
        novas = 0;
        if (!BibsSelecionar(r))
            return false;
        for (size_t k = 0; k < r->saida->selecao.membros.n; k++) {
            if (!BibsDependenciasDoMembro(r, VecEm(&r->saida->selecao.membros, k), &novas))
                return false;
        }
    } while (novas > 0);
    return true;
}

static bool BibsNaLista(const Resolucao *r, const char *nome)
{
    for (uint32_t k = 0; k < r->qtd_lista; k++) {
        if (strcmp(r->lista[k], nome) == 0)
            return true;
    }
    return false;
}

static bool BibsEhPendente(const EstaticaSelecao *sel, const char *nome)
{
    for (size_t k = 0; k < sel->pendentes.n; k++) {
        if (strcmp(*(const char *const *)VecEm(&sel->pendentes, k), nome) == 0)
            return true;
    }
    return false;
}

/* O pendente de um membro vira o simbolo da DLL: "__imp_X" pede o slot de X; "X" pede X (o thunk e do terceiros.c). */
static const char *BibsSimboloDoPendente(const char *pendente)
{
    if (strncmp(pendente, BIBS_PREFIXO_IMP, BIBS_TAM_PREFIXO_IMP) == 0)
        return pendente + BIBS_TAM_PREFIXO_IMP;
    return pendente;
}

/* Sem estatica: a lista e o pedido. Com estatica: so o que nenhuma estatica define (raizes e o que os membros pedem), sem repeticao, e as raizes que uma estatica define viram celulas. */
static void BibsMontarCelulas(Resolucao *r)
{
    const BibliotecasPedido *p = r->pedido;

    for (uint32_t k = 0; k < p->qtd_simbolos; k++) {
        if (!BibsEhPendente(&r->saida->selecao, p->simbolos[k]))
            *(const char **)VecEmpurrar(&r->saida->celulas) = ArenaCopiarTexto(&r->saida->memoria, p->simbolos[k], strlen(p->simbolos[k]));
    }
}

/* O objeto define esse nome como global? */
static bool BibsProvedorDefine(const Objeto *o, const char *nome)
{
    int64_t i = ObjAcharSimbolo(o, nome);
    const ObjSimbolo *s;

    if (i < 0)
        return false;
    s = VecEm(&o->simbolos, (size_t)i);
    return s->global && s->secao >= 0;
}

/* O provedor que satisfaz o simbolo que um MEMBRO pediu (nunca o __imp_X, nem o que o programa pede: esses vao para a DLL); -1 se nenhum. */
static int32_t BibsProvedorDe(const Resolucao *r, const char *nome)
{
    if (strncmp(nome, BIBS_PREFIXO_IMP, BIBS_TAM_PREFIXO_IMP) == 0 || BibsPosicaoNoPedido(r, nome) >= 0)
        return -1;
    for (uint32_t k = 0; k < r->pedido->qtd_provedores; k++) {
        if (BibsProvedorDefine(r->pedido->provedores[k].objeto, nome))
            return (int32_t)k;
    }
    return -1;
}

static void BibsPoeNaLista(Resolucao *r, const char **escrita, const char *nome)
{
    if (!BibsNaLista(r, nome))
        escrita[r->qtd_lista++] = nome;
}

/* Os __imp_X indefinidos de um provedor usado pedem X a DLL (o __stdio_common_vsprintf do legado vem da ucrtbase). */
static void BibsImportsDoProvedor(Resolucao *r, const char **escrita, const Objeto *o)
{
    for (size_t i = 0; i < o->simbolos.n; i++) {
        const ObjSimbolo *s = VecEm(&o->simbolos, i);

        if (s->secao == OBJ_SECAO_INDEFINIDA && strncmp(s->nome, BIBS_PREFIXO_IMP, BIBS_TAM_PREFIXO_IMP) == 0)
            BibsPoeNaLista(r, escrita, s->nome + BIBS_TAM_PREFIXO_IMP);
    }
}

static void BibsImportsDosProvedores(Resolucao *r, const char **escrita)
{
    for (uint32_t k = 0; k < r->pedido->qtd_provedores; k++) {
        if (((r->saida->provedores_usados >> k) & 1u) != 0 && r->pedido->provedores[k].traz_imports)
            BibsImportsDoProvedor(r, escrita, r->pedido->provedores[k].objeto);
    }
}

/* Cabe cada pendente e, no pior caso, todos os simbolos dos provedores (os __imp_ deles). */
static size_t BibsCapacidadeDaLista(const Resolucao *r)
{
    size_t capacidade = r->saida->selecao.pendentes.n + 1;

    for (uint32_t k = 0; k < r->pedido->qtd_provedores; k++)
        capacidade += r->pedido->provedores[k].objeto->simbolos.n;
    return capacidade;
}

static void BibsMontarListaComEstaticas(Resolucao *r)
{
    const EstaticaSelecao *sel = &r->saida->selecao;
    const char **escrita = ArenaAlocar(&r->saida->memoria, BibsCapacidadeDaLista(r) * sizeof *escrita);

    r->lista = escrita;
    BibsMontarCelulas(r);
    for (size_t k = 0; k < sel->pendentes.n; k++) {
        const char *pendente = *(const char *const *)VecEm(&sel->pendentes, k);
        int32_t provedor = BibsProvedorDe(r, pendente);

        if (provedor >= 0)
            r->saida->provedores_usados |= 1u << provedor;
        else
            BibsPoeNaLista(r, escrita, BibsSimboloDoPendente(pendente));
    }
    BibsImportsDosProvedores(r, escrita);
}

static void BibsMontarLista(Resolucao *r)
{
    if (r->saida->tem_estaticas) {
        BibsMontarListaComEstaticas(r);
        return;
    }
    r->lista = r->pedido->simbolos;
    r->qtd_lista = r->pedido->qtd_simbolos;
}

/* Os 'link' (e as estaticas e suas dependencias), a lista de simbolos de DLL, as DLLs implicitas e o dono de cada simbolo; as tabelas saem se tudo deu certo. */
static bool BibsResolverTudo(Resolucao *r)
{
    if (!BibsAdicionarLinks(r) || !BibsAdicionarSemente(r) || !BibsResolverEstaticas(r))
        return false;
    r->explicitas = r->dlls.n;
    BibsMontarLista(r);
    if (r->qtd_lista > 0 && !BibsAdicionarImplicitas(r))
        return false;
    return BibsAcharDonos(r) && BibsMontarTabelas(r);
}

bool BibliotecasResolver(const BibliotecasPedido *pedido, BibliotecasResolvidas *saida, Diag *erro)
{
    Resolucao r = {.pedido = pedido, .saida = saida, .erro = erro};
    bool ok;

    *saida = (BibliotecasResolvidas){.simbolo_faltando = -1};
    VecIniciar(&saida->lidas, sizeof(BibliotecaLida));
    VecIniciar(&saida->celulas, sizeof(const char *));
    VecIniciar(&r.dlls, sizeof(DllLida));
    VecIniciar(&r.estaticas, sizeof(EstaticaArquivo));
    VecIniciar(&r.vistas, sizeof(const char *));
    ok = BibsResolverTudo(&r);
    BibsLiberarDlls(&r);
    saida->estaticas = r.estaticas;
    VecLiberar(&r.vistas);
    return ok;
}

void BibliotecasLiberar(BibliotecasResolvidas *saida)
{
    if (saida->tem_estaticas)
        EstaticaLiberar(&saida->selecao);
    ArenaLiberar(&saida->memoria);
    BufLiberar(&saida->procurei_em);
    VecLiberar(&saida->lidas);
    VecLiberar(&saida->celulas);
    VecLiberar(&saida->estaticas);
}
