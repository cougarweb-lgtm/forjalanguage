/* orquestrar.c - orquestrador de projeto: le o forja.proj, monta as camadas, apura o que recompila e por que (--plano) e, sem --plano, chama a execucao (orquestrar_exec.c).
 * Versao: 0.3.2
 * Changelog: 0.3.2 - 0.21.0 F4 passo 1: texto do X0003 de --projeto sem o alternativa --backend c (o backend C foi aposentado).
 *            0.3.1 - M3 F9b-1: decisoes extraidas de ArquivosDeHoje e SaidaConfere.
 *            0.3.0 - M3 F6: medicao por etapa (FORJA_ORQ_TEMPOS): ler_proj, ordem, estado_ler, id_forja, total, avaliar e linha_estado; as mensagens de estado e de forja.exe ilegiveis
 *            dizem a causa (errno + strerror, erro_sistema.c).
 *            0.2.1- M3 P23z (revisor): OrqLinhaDoEstado grava a chave com o hash que o filho gravou no .entradas (ApurarCompilado), nao com o do disco de hoje.
 *            0.2.0 - M3 P22-O2: Plano e ORQ_CAMINHO_MAX foram para orquestrar_interno.h; OrqPrepararPlano, OrqAvaliarProduto, OrqLinhaDoEstado, OrqCaminhoDoEstado/DasEntradas e OrqTextoJson
 *            ficam visiveis para o orquestrar_exec.c; build --projeto sem --plano executa (OrqConstruir) em vez de devolver mensagem e exit 2.
 *            0.1.0 - M3 P22-O1b: OrquestrarProjeto so com --plano. Chave = EstadoChave com os hashes de hoje das linhas do .entradas (atalho tamanho+mtime so com mtime < inicio),
 *            sem as linhas que sao a saida de uma dependencia declarada (essas entram como "dep:<nome>=<hash_interface>"). Execucao dos filhos, jobs e trava ficam para o O2.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "cache/entradas_ler.h"
#include "cache/estado.h"
#include "cache/projeto_grafo.h"
#include "common/hash.h"
#include "driver/erro_sistema.h"
#include "driver/orquestrar.h"
#include "driver/orquestrar_interno.h"
#include "nativo/nativo.h"

#define ORQ_ALVO "x86_64-windows"
#define ORQ_MANIFESTO_PADRAO "forja.proj"

/* O que se apurou de UM produto: dependencias de hoje, linhas do .entradas e o hash de hoje de cada uma. */
typedef struct {
    EstadoDep deps[PROJETO_DEPENDE_MAX];
    Vec gravadas;                   /* EntradaGravada */
    bool tem_entradas;
    EstadoArquivo *arquivos;
    size_t narquivos;
    const EntradaGravada *mudou;    /* a 1a linha (em ordem de caminho) que mudou ou sumiu */
} Avaliacao;

/* ---------- apuracao ---------- */

static bool ConferirBackend(const Opcoes *o)
{
    if (o->backend == NULL || BackendEhX64(o)) return true;
    DiagImprimirJsonSemFonte(X0003, "o orquestrador so usa o backend x64 (o padrao): --projeto nao aceita outro --backend", stderr);
    return false;
}

static uint64_t AgoraFiletime(void)
{
    FILETIME agora;

    GetSystemTimeAsFileTime(&agora);
    return ((uint64_t)agora.dwHighDateTime << 32) | agora.dwLowDateTime;
}

/* Hash64 dos bytes do proprio forja.exe: o id do binario (a versao textual nao basta, o exe muda sem trocar de versao). */
static bool IdDoForja(uint64_t *id)
{
    char caminho[PROJETO_CAMINHO_MAX], *dados;
    size_t tam;
    DWORD n = GetModuleFileNameA(NULL, caminho, sizeof caminho);

    if (n == 0 || n >= sizeof caminho || !ArquivoLerInteiro(caminho, &dados, &tam)) return false;
    *id = Hash64(dados, tam, HASH64_SEMENTE);
    free(dados);
    return true;
}

void OrqCaminhoDoEstado(const Plano *pl, char *destino, size_t tam)
{
    snprintf(destino, tam, "%s\\.forja_cache\\estado\\%s.tsv", pl->projeto.pasta, pl->modo);
}

void OrqCaminhoDasEntradas(const Plano *pl, const Produto *pr, char *destino, size_t tam)
{
    snprintf(destino, tam, "%s\\.forja_cache\\estado\\%s\\%s.entradas", pl->projeto.pasta, pl->modo, pr->nome);
}

/* A linha e a saida de uma dependencia declarada do produto: entra na chave como "dep:", nao como arquivo (o corpo da dll mudar nao recompila o dependente). */
static bool EhSaidaDeDependencia(const Plano *pl, const Produto *pr, const char *caminho)
{
    for (uint32_t j = 0; j < pr->ndep; j++) {
        if (_stricmp(ProjetoProduto(&pl->projeto, pr->dep[j])->saida, caminho) == 0) return true;
    }
    return false;
}

static void DependenciasDeHoje(const Plano *pl, const Produto *pr, Avaliacao *av)
{
    for (uint32_t j = 0; j < pr->ndep; j++) {
        const Produto *dep = ProjetoProduto(&pl->projeto, pr->dep[j]);
        const EstadoLinha *linha = EstadoAchar(&pl->estado, dep->nome);

        av->deps[j] = (EstadoDep){dep->nome, linha != NULL && linha->tem_interface, linha != NULL ? linha->hash_interface : 0};
    }
}

/* A linha gravada mudou ou sumiu: nao achou o arquivo ou o hash de hoje difere do gravado. */
static bool EntradaMudouOuSumiu(bool achou, uint64_t hash, const EntradaGravada *g)
{
    return !achou || hash != g->hash;
}

/* Hash de hoje de cada linha do .entradas (menos as saidas de dependencias); guarda a 1a que mudou ou sumiu. */
static void ArquivosDeHoje(const Plano *pl, const Produto *pr, Avaliacao *av)
{
    av->arquivos = calloc(av->gravadas.n + 1, sizeof *av->arquivos);
    if (av->arquivos == NULL) FalhaInterna("memoria esgotada no plano");
    for (size_t k = 0; k < av->gravadas.n; k++) {
        const EntradaGravada *g = VecEm(&av->gravadas, k);
        uint64_t hash = 0;
        bool achou;

        if (EhSaidaDeDependencia(pl, pr, g->caminho)) continue;
        achou = EntradaHashAtual(g, pl->inicio, &hash);
        av->arquivos[av->narquivos++] = (EstadoArquivo){g->caminho, hash};
        if (av->mudou == NULL && EntradaMudouOuSumiu(achou, hash, g)) av->mudou = g;
    }
}

static void Apurar(const Plano *pl, const Produto *pr, Avaliacao *av)
{
    char entradas[ORQ_CAMINHO_MAX];

    OrqCaminhoDasEntradas(pl, pr, entradas, sizeof entradas);
    av->tem_entradas = EntradasLer(entradas, &av->gravadas);
    DependenciasDeHoje(pl, pr, av);
    ArquivosDeHoje(pl, pr, av);
}

/* Estado pos-build: o hash que o filho gravou no .entradas (o que ele compilou), nunca o do disco de hoje. Edicao entre o fim do filho e a gravacao do
 * estado tem de aparecer como mudanca no proximo plano; com o hash de hoje o binario antigo ficaria com a chave do conteudo novo (build "em dia" e velho). */
static void ApurarCompilado(const Plano *pl, const Produto *pr, Avaliacao *av)
{
    char entradas[ORQ_CAMINHO_MAX];

    OrqCaminhoDasEntradas(pl, pr, entradas, sizeof entradas);
    av->tem_entradas = EntradasLer(entradas, &av->gravadas);
    DependenciasDeHoje(pl, pr, av);
    av->arquivos = calloc(av->gravadas.n + 1, sizeof *av->arquivos);
    if (av->arquivos == NULL) FalhaInterna("memoria esgotada no plano");
    for (size_t k = 0; k < av->gravadas.n; k++) {
        const EntradaGravada *g = VecEm(&av->gravadas, k);

        if (EhSaidaDeDependencia(pl, pr, g->caminho)) continue;
        av->arquivos[av->narquivos++] = (EstadoArquivo){g->caminho, g->hash};
    }
}

static void LiberarAvaliacao(Avaliacao *av)
{
    VecLiberar(&av->gravadas);
    free(av->arquivos);
}

/* ---------- fatos ---------- */

static ChavePedido MontarChave(const Plano *pl, const Produto *pr, const Avaliacao *av)
{
    ChavePedido cp = {0};

    cp.forja = pl->forja_id;
    cp.alvo = ORQ_ALVO;
    cp.modo = pl->modo;
    cp.tipo = pr->tipo == PRODUTO_DLL ? "dll" : "exe";
    cp.entrada = pr->entrada;
    cp.lib_pasta = pl->projeto.saida;
    cp.deps = av->deps;
    cp.ndeps = pr->ndep;
    cp.arquivos = av->arquivos;
    cp.narquivos = av->narquivos;
    return cp;
}

/* O Hash64 do arquivo inteiro; false = nao abriu. */
static bool HashDoArquivo(const char *caminho, uint64_t *hash)
{
    char *dados;
    size_t tam;

    if (!ArquivoLerInteiro(caminho, &dados, &tam)) return false;
    *hash = Hash64(dados, tam, HASH64_SEMENTE);
    free(dados);
    return true;
}

/* A saida existe com o tamanho e o mtime gravados. */
static bool SaidaMedidaConfere(const Produto *pr, const EstadoLinha *ant)
{
    uint64_t tamanho, mtime;

    if (!ArquivoMedir(pr->saida, &tamanho, &mtime)) return false;
    return tamanho == ant->tamanho && mtime == ant->mtime;
}

/* A saida existe como foi gravada (tamanho e mtime); na dll, o .h tambem existe e ainda tem a hash_interface gravada. */
static bool SaidaConfere(const Produto *pr, const EstadoLinha *ant)
{
    uint64_t hash;

    if (!SaidaMedidaConfere(pr, ant)) return false;
    if (pr->tipo != PRODUTO_DLL) return true;
    return ant->tem_interface && HashDoArquivo(pr->cabecalho, &hash) && hash == ant->hash_interface;
}

/* Nome da 1a dependencia cuja "nome=<hash>" nao esta na coluna deps gravada; "" = todas iguais. */
static const char *DependenciaQueMudou(const Avaliacao *av, size_t ndeps, const char *gravadas)
{
    char palheiro[ESTADO_DEPS_MAX + 3], agulha[PROJETO_NOME_MAX + 24];

    snprintf(palheiro, sizeof palheiro, ",%s,", gravadas);
    for (size_t k = 0; k < ndeps; k++) {
        if (av->deps[k].tem)
            snprintf(agulha, sizeof agulha, ",%s=%016llx,", av->deps[k].nome, (unsigned long long)av->deps[k].hash);
        else
            snprintf(agulha, sizeof agulha, ",%s=-,", av->deps[k].nome);
        if (strstr(palheiro, agulha) == NULL) return av->deps[k].nome;
    }
    return "";
}

/* O caminho relativo a pasta do projeto quando esta dentro dela. */
static const char *RelativoAoProjeto(const Plano *pl, const char *caminho)
{
    size_t tam = strlen(pl->projeto.pasta);

    if (_strnicmp(caminho, pl->projeto.pasta, tam) == 0 && caminho[tam] == '\\') return caminho + tam + 1;
    return caminho;
}

static MotivoPedido ApurarFatos(const Plano *pl, const Produto *pr, const EstadoLinha *ant, const Avaliacao *av)
{
    ChavePedido cp = MontarChave(pl, pr, av);
    MotivoPedido m = {0};

    m.forcado = pl->opcoes->tudo;
    m.primeira_vez = ant == NULL || !av->tem_entradas;
    m.dep_mudou = "";
    m.arquivo_mudou = "";
    if (m.primeira_vez) return m;
    m.chave_igual = EstadoChave(&cp) == ant->chave;
    m.ambiente_igual = EstadoAmbiente(&cp) == ant->ambiente;
    m.saida_ok = SaidaConfere(pr, ant);
    m.dep_mudou = DependenciaQueMudou(av, pr->ndep, ant->deps);
    if (av->mudou != NULL) {
        m.arquivo_mudou = RelativoAoProjeto(pl, av->mudou->caminho);
        m.arquivo_e_lib = strcmp(av->mudou->tipo, "lib") == 0;
    }
    return m;
}

/* true = recompilaria; motivo recebe o texto. */
bool OrqAvaliarProduto(const Plano *pl, uint32_t k, char *motivo, size_t n)
{
    const Produto *pr = ProjetoProduto(&pl->projeto, k);
    Avaliacao av = {0};
    MotivoPedido fatos;
    uint64_t ini = TemposAgora(&pl->tempos);
    bool recompila;

    Apurar(pl, pr, &av);
    fatos = ApurarFatos(pl, pr, EstadoAchar(&pl->estado, pr->nome), &av);
    recompila = EstadoMotivo(&fatos, motivo, n);
    LiberarAvaliacao(&av);
    TemposFechar((Tempos *)&pl->tempos, "avaliar", pr->nome, ini);
    return recompila;
}

/* hash_interface = Hash64 dos bytes do .h que o filho gravou (so dll); false = a dll ficou sem .h legivel. */
static bool InterfaceDoProduto(const Produto *pr, EstadoLinha *l)
{
    l->tem_interface = pr->tipo == PRODUTO_DLL;
    return !l->tem_interface || HashDoArquivo(pr->cabecalho, &l->hash_interface);
}

bool OrqLinhaDoEstado(const Plano *pl, uint32_t k, EstadoLinha *l)
{
    const Produto *pr = ProjetoProduto(&pl->projeto, k);
    Avaliacao av = {0};
    ChavePedido cp;
    uint64_t ini = TemposAgora(&pl->tempos);
    bool ok;

    ApurarCompilado(pl, pr, &av);
    cp = MontarChave(pl, pr, &av);
    memset(l, 0, sizeof *l);
    snprintf(l->nome, sizeof l->nome, "%s", pr->nome);
    snprintf(l->saida, sizeof l->saida, "%s", pr->saida);
    l->chave = EstadoChave(&cp);
    l->ambiente = EstadoAmbiente(&cp);
    EstadoDepsTexto(&cp, l->deps, sizeof l->deps);
    if (strcmp(l->deps, "-") == 0) l->deps[0] = '\0';
    ok = av.tem_entradas && ArquivoMedir(pr->saida, &l->tamanho, &l->mtime) && InterfaceDoProduto(pr, l);
    LiberarAvaliacao(&av);
    TemposFechar((Tempos *)&pl->tempos, "linha_estado", pr->nome, ini);
    return ok;
}

/* ---------- saida ---------- */

void OrqTextoJson(const char *texto)
{
    putchar('"');
    for (const char *p = texto; *p != '\0'; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p);
    }
    putchar('"');
}

static void ImprimirProduto(const Opcoes *o, const char *nome, bool recompila, const char *motivo)
{
    if (o->json) {
        printf("{\"produto\": \"%s\", \"acao\": \"%s\", \"motivo\": ", nome, recompila ? "recompilaria" : "reusado");
        OrqTextoJson(recompila ? motivo : "");
        puts("}");
    } else if (recompila) {
        printf("forja: [%s] recompilaria (%s)\n", nome, motivo);
    } else {
        printf("forja: [%s] reusado\n", nome);
    }
}

static void ImprimirResumo(const Opcoes *o, uint32_t total, uint32_t recompilam)
{
    if (o->json)
        printf("{\"plano\": {\"produtos\": %u, \"recompilariam\": %u, \"reusados\": %u}}\n", total, recompilam, total - recompilam);
    else
        printf("forja: plano: %u produtos, %u recompilariam, %u reusados\n", total, recompilam, total - recompilam);
    fflush(stdout);
}

/* ---------- fluxo ---------- */

static void LiberarPlano(Plano *pl)
{
    free(pl->camada);
    free(pl->ordem);
    EstadoLiberar(&pl->estado);
    ProjetoLiberar(&pl->projeto);
}

/* O estado gravado do modo (etapa estado_ler). Falha = mensagem com a causa. */
static bool LerEstadoMedido(Plano *pl)
{
    char caminho[ORQ_CAMINHO_MAX], causa[ERRO_TEXTO_MAX];
    uint64_t ini = TemposAgora(&pl->tempos);
    bool lido;
    int err;

    OrqCaminhoDoEstado(pl, caminho, sizeof caminho);
    errno = 0;
    lido = EstadoLer(caminho, &pl->estado);
    err = errno;
    TemposFechar(&pl->tempos, "estado_ler", NULL, ini);
    if (lido) return true;
    ErroCrtTexto(err, causa, sizeof causa);
    fprintf(stderr, "forja: nao consegui ler o estado '%s' (%s)\n", caminho, causa);
    return false;
}

/* O id do forja.exe (etapa id_forja). Falha = mensagem com a causa. */
static bool IdentificarForja(Plano *pl)
{
    char causa[ERRO_TEXTO_MAX];
    uint64_t ini = TemposAgora(&pl->tempos);
    bool lido;
    int err;

    errno = 0;
    lido = IdDoForja(&pl->forja_id);
    err = errno;
    TemposFechar(&pl->tempos, "id_forja", NULL, ini);
    if (lido) return true;
    ErroCrtTexto(err, causa, sizeof causa);
    fprintf(stderr, "forja: nao consegui ler o proprio forja.exe para o id do binario (%s)\n", causa);
    return false;
}

bool OrqPrepararPlano(Plano *pl)
{
    if (!LerEstadoMedido(pl) || !IdentificarForja(pl)) return false;
    pl->inicio = AgoraFiletime();
    return true;
}

/* B0005 (ou B0007): o JSON em stderr; sem erro.tem o arquivo nao abriu e a mensagem ja saiu. */
static int RelatarErro(const Diag *erro)
{
    if (erro->tem) DiagImprimirJsonSemFonte(erro->codigo, erro->causa, stderr);
    return SAIDA_USO;
}

static int Planejar(Plano *pl)
{
    uint32_t recompilam = 0, total = (uint32_t)pl->projeto.produtos.n;
    char motivo[PROJETO_CAMINHO_MAX + 64];

    if (!OrqPrepararPlano(pl)) return SAIDA_USO;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t k = pl->ordem[i];
        bool recompila = OrqAvaliarProduto(pl, k, motivo, sizeof motivo);

        ImprimirProduto(pl->opcoes, ProjetoProduto(&pl->projeto, k)->nome, recompila, motivo);
        if (recompila) recompilam++;
    }
    ImprimirResumo(pl->opcoes, total, recompilam);
    return SAIDA_OK;
}

/* Camadas e ordem. false = ciclo (B0007 ja impresso). */
static bool MontarOrdem(Plano *pl)
{
    Diag erro = {0};
    size_t n = pl->projeto.produtos.n;
    uint64_t ini = TemposAgora(&pl->tempos);
    bool montou;

    pl->camada = calloc(n, sizeof *pl->camada);
    pl->ordem = calloc(n, sizeof *pl->ordem);
    if (pl->camada == NULL || pl->ordem == NULL) FalhaInterna("memoria esgotada no plano");
    montou = ProjetoCamadas(&pl->projeto, pl->camada, pl->ordem, &erro);
    TemposFechar(&pl->tempos, "ordem", NULL, ini);
    if (montou) return true;
    RelatarErro(&erro);
    return false;
}

/* ProjetoLer com a etapa ler_proj medida. */
static bool LerProjetoMedido(Plano *pl, Diag *erro)
{
    const char *manifesto = pl->opcoes->entrada != NULL ? pl->opcoes->entrada : ORQ_MANIFESTO_PADRAO;
    uint64_t ini = TemposAgora(&pl->tempos);
    bool lido = ProjetoLer(manifesto, &pl->projeto, erro);

    TemposFechar(&pl->tempos, "ler_proj", NULL, ini);
    return lido;
}

/* Planeja ou constroi; depois grava o total e as linhas medidas (TemposGravar fica de fora do total). */
static int Executar(Plano *pl)
{
    int codigo = pl->opcoes->plano ? Planejar(pl) : OrqConstruir(pl);

    TemposFechar(&pl->tempos, "total", NULL, pl->tempos.inicio);
    TemposGravar(&pl->tempos);
    return codigo;
}

int OrquestrarProjeto(const Opcoes *o)
{
    Plano pl = {0};
    Diag erro = {0};
    int codigo = SAIDA_USO;

    if (!ConferirBackend(o)) return SAIDA_USO;
    pl.opcoes = o;
    pl.modo = o->release ? "release" : "dev";
    TemposIniciar(&pl.tempos);
    if (!LerProjetoMedido(&pl, &erro)) return RelatarErro(&erro);
    if (!MontarOrdem(&pl)) {
        LiberarPlano(&pl);
        return SAIDA_USO;
    }
    codigo = Executar(&pl);
    LiberarPlano(&pl);
    return codigo;
}
