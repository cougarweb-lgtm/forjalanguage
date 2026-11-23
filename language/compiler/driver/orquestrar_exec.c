/* orquestrar_exec.c - execucao do orquestrador de projeto (P22-O2 do M3): trava, faxina, fila por camadas com N filhos "forja build" em paralelo, falha isolada, estado a cada produto, blocos de saida e resumo.
 * Versao: 0.2.2
 * Changelog: 0.2.2 - M3 F9b-1: decisoes extraidas de ProntidaoDe e TratarProduto (o teste de PRONTO foi para ProntidaoDe). 0.2.1 - M3 F8: Concluir varre a fila logo depois de liberar a vaga, antes do EmitirBloco/estado do filho que terminou (o proximo produto pronto dispara em paralelo ao pos-processamento).
 *            0.2.0- M3 F6: medicao por etapa (FORJA_ORQ_TEMPOS, orquestrar_tempos.h): trava (com "n" = Sleep dados), pastas_faxina, resumo e, por produto, canais, criar, job, retomar, vida_do_filho, latencia, emitir_bloco, declaradas, gravar_estado e ocioso_do_slot. Sem a variavel nada muda.
 *            0.1.0- M3 P22-O2: OrqConstruir. Jobs = --jobs, FORJA_JOBS ou o padrao (processadores logicos, RAM livre / 512 MiB, largura da maior camada, 64). Trava <cache>\trava (B0008). Faxina de *.tmp<pid> e *.velho<pid> de pid morto. Um filho por produto (processo_win.c); falha -> dependentes "pulado", independentes terminam; B0006 pelo .entradas; exit = o maior exit dos filhos que falharam (fora de 1..3 conta 3). Bloco do filho (.out e .err em bytes crus) em stderr ao termino dele; resumo em (camada, nome) em stdout.
 *            Gancho de teste: FORJA_TESTE_INJETAR_PRODUTO=<nome> com FORJA_TESTE_INJETAR=<codigo> repassa a injecao SO ao filho desse produto.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "cache/entradas_ler.h"
#include "common/base.h"
#include "common/diag.h"
#include "driver/orquestrar_interno.h"
#include "driver/processo_win.h"
#include "nativo/nativo.h"

#define EXEC_JOBS_MAX 64
#define EXEC_RAM_POR_FILHO (512ull * 1024 * 1024)
#define EXEC_VARIAVEL_JOBS "FORJA_JOBS"
#define EXEC_INJETAR "FORJA_TESTE_INJETAR"
#define EXEC_INJETAR_PRODUTO "FORJA_TESTE_INJETAR_PRODUTO"
#define EXEC_SEM_DONO UINT32_MAX
#define EXEC_CACHE_MAX (PROJETO_CAMINHO_MAX + 16)       /* <pasta>\.forja_cache */
#define EXEC_ESTADO_MAX (EXEC_CACHE_MAX + 16)           /* <cache>\estado */
#define EXEC_MODO_MAX (EXEC_ESTADO_MAX + 16)            /* <estado>\<dev|release> */

typedef enum { SIT_PENDENTE, SIT_PRONTO, SIT_RODANDO, SIT_RECOMPILADO, SIT_REUSADO, SIT_FALHOU, SIT_PULADO } Situacao;

typedef enum { DEPS_ESPERA, DEPS_LIVRES, DEPS_FALHAS } Prontidao;

typedef enum { TRAVA_OK, TRAVA_OCUPADA, TRAVA_ERRO } ResultadoTrava;

/* O que aconteceu com UM produto. motivo = por que recompila; velho = onde ficou a saida antiga quando estava em uso. */
typedef struct {
    Situacao situacao;
    Processo proc;
    uint64_t inicio_ms;
    uint32_t ms, causa;
    int saida;
    char motivo[ORQ_MOTIVO_MAX];
    char velho[ORQ_CAMINHO_MAX];
} Corrida;

typedef struct {
    Plano *pl;
    Corrida *corrida;
    ProcessoJob job;
    HANDLE trava;
    uint64_t inicio_ms;
    uint32_t jobs, rodando, filhos;
    char exe[PROJETO_CAMINHO_MAX];
    char cache[EXEC_CACHE_MAX];
    char injetar[32], injetar_produto[PROJETO_NOME_MAX];
} Execucao;

typedef struct { uint32_t recompilados, reusados, falharam, pulados; } Contagem;

/* A linha "lib" do .entradas que e a saida de um produto que o produto nao declarou. */
typedef struct { const EntradaGravada *linha; uint32_t dono; } Violacao;

static uint64_t MsAgora(void)
{
    return RelogioNs() / 1000000ull;
}

static uint32_t Minimo(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

/* ---------- jobs ---------- */

/* Quantos produtos cabem lado a lado na camada mais larga: mais filhos que isso nunca rodam juntos. */
static uint32_t LarguraMaxima(const Plano *pl)
{
    size_t n = pl->projeto.produtos.n;
    uint32_t *conta = calloc(n + 1, sizeof *conta);
    uint32_t maior = 1;

    if (conta == NULL) FalhaInterna("memoria esgotada contando as camadas");
    for (size_t k = 0; k < n; k++) {
        uint32_t largura = ++conta[pl->camada[k]];

        if (largura > maior) maior = largura;
    }
    free(conta);
    return Minimo(maior, EXEC_JOBS_MAX);
}

/* Processadores logicos, limitado por RAM fisica livre / 512 MiB, pela largura da maior camada e por 64 (minimo 1). */
static uint32_t JobsPadrao(const Plano *pl)
{
    MEMORYSTATUSEX mem = {0};
    uint32_t jobs = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    mem.dwLength = sizeof mem;
    if (GlobalMemoryStatusEx(&mem)) jobs = Minimo(jobs, (uint32_t)(mem.ullAvailPhys / EXEC_RAM_POR_FILHO));
    jobs = Minimo(Minimo(jobs, LarguraMaxima(pl)), EXEC_JOBS_MAX);
    return jobs > 0 ? jobs : 1;
}

/* --jobs vence FORJA_JOBS, que vence o padrao. false = FORJA_JOBS fora de 1..64 (uso errado). */
static bool ResolverJobs(const Plano *pl, uint32_t *jobs)
{
    const char *variavel = getenv(EXEC_VARIAVEL_JOBS);

    if (pl->opcoes->jobs != NULL) return OpcoesJobsTexto(pl->opcoes->jobs, jobs);
    if (variavel == NULL) {
        *jobs = JobsPadrao(pl);
        return true;
    }
    if (OpcoesJobsTexto(variavel, jobs)) return true;
    fprintf(stderr, "forja: " EXEC_VARIAVEL_JOBS " invalida '%s': use um numero inteiro de 1 a 64\n", variavel);
    return false;
}

/* ---------- pastas, faxina e trava ---------- */

static bool EhPasta(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);

    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/* Cria a pasta e as que faltam acima dela (o "C:\" do comeco fica de fora). */
static bool PastaGarantir(const char *caminho)
{
    char parcial[ORQ_CAMINHO_MAX];
    size_t n = strlen(caminho);

    if (n >= sizeof parcial) return false;
    memcpy(parcial, caminho, n + 1);
    for (size_t k = 3; k < n; k++) {
        if (parcial[k] != '\\') continue;
        parcial[k] = '\0';
        CreateDirectoryA(parcial, NULL);
        parcial[k] = '\\';
    }
    CreateDirectoryA(parcial, NULL);
    return EhPasta(caminho);
}

/* "x.dll.tmp1234" ou "x.dll.velho1234": o pid do forja que gravou. */
static bool SufixoDeProcesso(const char *nome, uint32_t *pid)
{
    const char *ponto = strrchr(nome, '.');
    const char *digitos;
    char *fim;

    if (ponto == NULL) return false;
    if (strncmp(ponto + 1, "tmp", 3) == 0)
        digitos = ponto + 4;
    else if (strncmp(ponto + 1, "velho", 5) == 0)
        digitos = ponto + 6;
    else
        return false;
    if (*digitos < '0' || *digitos > '9') return false;
    *pid = (uint32_t)strtoul(digitos, &fim, 10);
    return *fim == '\0';
}

/* Apaga os *.tmp<pid> e *.velho<pid> da pasta cujo pid ja morreu (um .velho de DLL ainda carregada nao apaga: o Windows recusa, e a proxima faxina tenta de novo). */
static void FaxinarPasta(const char *pasta)
{
    char padrao[ORQ_CAMINHO_MAX], arquivo[ORQ_CAMINHO_MAX];
    WIN32_FIND_DATAA f;
    HANDLE busca;
    uint32_t pid;

    snprintf(padrao, sizeof padrao, "%s\\*", pasta);
    busca = FindFirstFileA(padrao, &f);
    if (busca == INVALID_HANDLE_VALUE) return;
    do {
        if ((f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 || !SufixoDeProcesso(f.cFileName, &pid) || ProcessoVivo(pid)) continue;
        snprintf(arquivo, sizeof arquivo, "%s\\%s", pasta, f.cFileName);
        DeleteFileA(arquivo);
    } while (FindNextFileA(busca, &f));
    FindClose(busca);
}

/* Saida, estado\<modo> e log existem (o filho em pasta inexistente daria X0302) e as sobras de build morto saem. */
static bool PastasEFaxina(const Execucao *ex)
{
    const Plano *pl = ex->pl;
    char estado[EXEC_ESTADO_MAX], modo[EXEC_MODO_MAX], log[EXEC_ESTADO_MAX];

    snprintf(estado, sizeof estado, "%s\\estado", ex->cache);
    snprintf(modo, sizeof modo, "%s\\%s", estado, pl->modo);
    snprintf(log, sizeof log, "%s\\log", ex->cache);
    if (!PastaGarantir(pl->projeto.saida) || !PastaGarantir(modo) || !PastaGarantir(log)) {
        fputs("forja: nao consegui criar as pastas de saida, estado e log do projeto\n", stderr);
        return false;
    }
    FaxinarPasta(pl->projeto.saida);
    FaxinarPasta(estado);
    FaxinarPasta(modo);
    return true;
}

/* As pastas e a faxina, na etapa pastas_faxina. */
static bool PrepararPastas(const Execucao *ex)
{
    uint64_t ini = TemposAgora(&ex->pl->tempos);
    bool ok = PastasEFaxina(ex);

    TemposFechar(&ex->pl->tempos, "pastas_faxina", NULL, ini);
    return ok;
}

/* <cache>\trava sem compartilhamento e apagada ao fechar: outro orquestrador no projeto nao a abre, e ela some sozinha se este morrer. 'esperas' conta os Sleep(10). */
static ResultadoTrava TravaTentar(Execucao *ex, int64_t *esperas)
{
    char caminho[ORQ_CAMINHO_MAX];
    DWORD erro = 0;

    snprintf(caminho, sizeof caminho, "%s\\trava", ex->cache);
    for (int tentativa = 0; tentativa < 4; tentativa++) {
        ex->trava = CreateFileA(caminho, GENERIC_WRITE | DELETE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (ex->trava != INVALID_HANDLE_VALUE) return TRAVA_OK;
        erro = GetLastError();
        if (erro == ERROR_SHARING_VIOLATION) return TRAVA_OCUPADA;
        (*esperas)++;
        Sleep(10);  /* ACCESS_DENIED: a trava do build anterior ainda esta em "apagar ao fechar" */
    }
    fprintf(stderr, "forja: nao consegui abrir a trava '%s' (erro %lu)\n", caminho, (unsigned long)erro);
    return TRAVA_ERRO;
}

/* TravaTentar na etapa trava, com "n" = quantos Sleep(10) foram dados. */
static ResultadoTrava TravaAbrir(Execucao *ex)
{
    uint64_t ini = TemposAgora(&ex->pl->tempos);
    int64_t esperas = 0;
    ResultadoTrava trava = TravaTentar(ex, &esperas);

    TemposFechar(&ex->pl->tempos, "trava", NULL, ini);
    TemposAcrescentarN(&ex->pl->tempos, esperas);
    return trava;
}

/* ---------- filhos ---------- */

typedef struct { char texto[ORQ_CAMINHO_MAX]; } CaminhoLog;

/* <cache>\log\<nome>.out ou .err: onde o stdout e o stderr do filho caem. */
static void CaminhoDoLog(const Execucao *ex, const char *nome, const char *extensao, CaminhoLog *destino)
{
    snprintf(destino->texto, sizeof destino->texto, "%s\\log\\%s.%s", ex->cache, nome, extensao);
}

typedef struct { const char *v[20]; } ArgsFilho;

/* forja build --backend x64 [--modo dll] [--release] <entrada> -o <saida> --entradas <arq> --lib-pasta <saida>. */
static void MontarArgs(const Execucao *ex, const Produto *pr, const char *entradas, ArgsFilho *a)
{
    size_t n = 0;

    a->v[n++] = "build";
    a->v[n++] = "--backend";
    a->v[n++] = "x64";
    if (pr->tipo == PRODUTO_DLL) {
        a->v[n++] = "--modo";
        a->v[n++] = "dll";
    }
    if (ex->pl->opcoes->release) a->v[n++] = "--release";
    a->v[n++] = pr->entrada;
    a->v[n++] = "-o";
    a->v[n++] = pr->saida;
    a->v[n++] = "--entradas";
    a->v[n++] = entradas;
    a->v[n++] = "--lib-pasta";
    a->v[n++] = ex->pl->projeto.saida;
    a->v[n] = NULL;
}

/* Gancho de teste: a injecao FORJA_TESTE_INJETAR so chega ao filho do produto nomeado em FORJA_TESTE_INJETAR_PRODUTO; os outros a perdem. */
static void AplicarInjecao(const Execucao *ex, const char *nome)
{
    if (ex->injetar_produto[0] == '\0') return;
    _putenv_s(EXEC_INJETAR, strcmp(ex->injetar_produto, nome) == 0 ? ex->injetar : "");
}

static void FalharNoDisparo(Execucao *ex, uint32_t k)
{
    Corrida *c = &ex->corrida[k];

    fprintf(stderr, "forja: [%s] nao consegui disparar o filho (erro %lu)\n", ProjetoProduto(&ex->pl->projeto, k)->nome, (unsigned long)c->proc.erro);
    c->situacao = SIT_FALHOU;
    c->saida = SAIDA_INTERNO;
}

/* As marcas do disparo so existem com a medicao ligada; sem ela o ProcessoDisparar nem le o relogio. */
static ProcessoFases *FasesSePedido(const Execucao *ex, ProcessoFases *fases)
{
    return ex->pl->tempos.ativo ? fases : NULL;
}

/* As quatro etapas do disparo a partir das marcas: canais (abrir + fechar), criar, job, retomar. */
static void AnotarFases(const Execucao *ex, const char *nome, const ProcessoFases *f)
{
    Tempos *t = &ex->pl->tempos;
    const uint64_t *m = f->marca;

    if (!t->ativo) return;
    TemposAnotarTicks(t, "canais", nome, (m[1] - m[0]) + (m[3] - m[2]));
    TemposAnotarTicks(t, "criar", nome, m[2] - m[1]);
    TemposAnotarTicks(t, "job", nome, m[4] - m[3]);
    TemposAnotarTicks(t, "retomar", nome, m[5] - m[4]);
}

static void Disparar(Execucao *ex, uint32_t k)
{
    const Produto *pr = ProjetoProduto(&ex->pl->projeto, k);
    Corrida *c = &ex->corrida[k];
    char entradas[ORQ_CAMINHO_MAX];
    CaminhoLog out, err;
    ArgsFilho args;
    ProcessoFases fases = {{0}};
    ProcessoPedido pedido = {ex->exe, args.v, out.texto, err.texto, &ex->job, FasesSePedido(ex, &fases)};

    TemposSlotOcupado(&ex->pl->tempos, pr->nome);
    OrqCaminhoDasEntradas(ex->pl, pr, entradas, sizeof entradas);
    CaminhoDoLog(ex, pr->nome, "out", &out);
    CaminhoDoLog(ex, pr->nome, "err", &err);
    MontarArgs(ex, pr, entradas, &args);
    AplicarInjecao(ex, pr->nome);
    c->inicio_ms = MsAgora();
    if (!ProcessoDisparar(&pedido, &c->proc)) {
        FalharNoDisparo(ex, k);
        return;
    }
    AnotarFases(ex, pr->nome, &fases);
    c->situacao = SIT_RODANDO;
    ex->rodando++;
    ex->filhos++;
}

/* ---------- saida do filho ---------- */

/* Bytes crus no stderr do orquestrador, sem a traducao LF -> CRLF do stdio: o JSON do filho sai como o filho o escreveu. */
static void EscreverBruto(const char *dados, size_t tam)
{
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD escrito;

    while (tam > 0 && h != INVALID_HANDLE_VALUE && h != NULL) {
        if (!WriteFile(h, dados, (DWORD)tam, &escrito, NULL) || escrito == 0) return;
        dados += escrito;
        tam -= escrito;
    }
}

/* O bloco do filho inteiro (cabecalho, .out e .err), de uma vez so, no instante em que ele termina: dois filhos nunca se intercalam. Vazio = nada sai. */
static void EmitirBloco(const Execucao *ex, uint32_t k)
{
    const char *nome = ProjetoProduto(&ex->pl->projeto, k)->nome;
    CaminhoLog caminho;
    char *out = NULL, *err = NULL;
    size_t tam_out = 0, tam_err = 0;

    CaminhoDoLog(ex, nome, "out", &caminho);
    if (!ArquivoLerInteiro(caminho.texto, &out, &tam_out)) tam_out = 0;
    CaminhoDoLog(ex, nome, "err", &caminho);
    if (!ArquivoLerInteiro(caminho.texto, &err, &tam_err)) tam_err = 0;
    if (tam_out + tam_err > 0) {
        fflush(stdout);
        fprintf(stderr, "forja: [%s] saida do build:\n", nome);
        fflush(stderr);
        EscreverBruto(out, tam_out);
        EscreverBruto(err, tam_err);
    }
    free(out);
    free(err);
}

/* ---------- B0006 e estado ---------- */

static bool DependeDe(const Produto *pr, uint32_t q)
{
    for (uint32_t j = 0; j < pr->ndep; j++) {
        if (pr->dep[j] == q) return true;
    }
    return false;
}

/* O produto (diferente de k e nao declarado por ele) cuja saida e esse caminho; EXEC_SEM_DONO = nenhum. */
static uint32_t DonoNaoDeclarado(const Plano *pl, uint32_t k, const char *caminho)
{
    const Produto *pr = ProjetoProduto(&pl->projeto, k);

    for (uint32_t q = 0; q < pl->projeto.produtos.n; q++) {
        if (q != k && !DependeDe(pr, q) && _stricmp(ProjetoProduto(&pl->projeto, q)->saida, caminho) == 0) return q;
    }
    return EXEC_SEM_DONO;
}

static bool AcharViolacao(const Plano *pl, uint32_t k, const Vec *gravadas, Violacao *v)
{
    for (size_t i = 0; i < gravadas->n; i++) {
        const EntradaGravada *g = VecEm(gravadas, i);

        if (strcmp(g->tipo, "lib") != 0) continue;
        v->dono = DonoNaoDeclarado(pl, k, g->caminho);
        v->linha = g;
        if (v->dono != EXEC_SEM_DONO) return true;
    }
    return false;
}

/* B0006: o .entradas de k lista a saida de outro produto fora do 'depende' de k. O JSON sai logo depois do bloco do filho. */
static bool ConferirDeclaradas(const Execucao *ex, uint32_t k)
{
    const Plano *pl = ex->pl;
    const Produto *pr = ProjetoProduto(&pl->projeto, k);
    char entradas[ORQ_CAMINHO_MAX], causa[ORQ_CAMINHO_MAX * 2];
    Violacao v = {NULL, EXEC_SEM_DONO};
    uint64_t ini = TemposAgora(&pl->tempos);
    Vec gravadas;
    bool achou;

    OrqCaminhoDasEntradas(pl, pr, entradas, sizeof entradas);
    EntradasLer(entradas, &gravadas);
    achou = AcharViolacao(pl, k, &gravadas, &v);
    if (achou) {
        const char *dono = ProjetoProduto(&pl->projeto, v.dono)->nome;

        snprintf(causa, sizeof causa, "o produto '%s' liga '%s', saida do produto '%s', sem declarar 'depende %s'", pr->nome, v.linha->caminho, dono, dono);
        DiagImprimirJsonSemFonte(B0006, causa, stderr);
    }
    VecLiberar(&gravadas);
    TemposFechar(&ex->pl->tempos, "declaradas", pr->nome, ini);
    return !achou;
}

/* Troca (ou acrescenta) a linha do produto no estado em memoria: o dependente seguinte ja ve a hash_interface real desta dll. */
static void GuardarLinha(Estado *e, const EstadoLinha *nova)
{
    for (size_t k = 0; k < e->linhas.n; k++) {
        EstadoLinha *atual = VecEm(&e->linhas, k);

        if (strcmp(atual->nome, nova->nome) == 0) {
            *atual = *nova;
            return;
        }
    }
    *(EstadoLinha *)VecEmpurrar(&e->linhas) = *nova;
}

static bool GravarEstado(const Execucao *ex, uint32_t k)
{
    char caminho[ORQ_CAMINHO_MAX];
    uint64_t ini = TemposAgora(&ex->pl->tempos);
    int err = 0;
    bool ok;

    OrqCaminhoDoEstado(ex->pl, caminho, sizeof caminho);
    ok = EstadoGravarAtomico(caminho, &ex->pl->estado, &err);
    TemposFechar(&ex->pl->tempos, "gravar_estado", ProjetoProduto(&ex->pl->projeto, k)->nome, ini);
    if (!ok) fprintf(stderr, "forja: nao consegui gravar o estado '%s' (errno %d)\n", caminho, err);
    return ok;
}

/* A saida antiga de uma DLL em uso fica em <saida>.velho<pid do filho> (ArquivoGravarAtomico): o resumo conta isso. */
static void NotarSaidaEmUso(Corrida *c, const Produto *pr)
{
    char velho[ORQ_CAMINHO_MAX];
    uint64_t tamanho, mtime;

    snprintf(velho, sizeof velho, "%s.velho%lu", pr->saida, (unsigned long)c->proc.pid);
    if (ArquivoMedir(velho, &tamanho, &mtime)) snprintf(c->velho, sizeof c->velho, "%s", velho);
}

/* Filho com exit 0: confere o B0006, apura a linha do estado e grava o estado inteiro de forma atomica. false = c->saida diz o exit do produto. */
static bool ConferirEGuardar(Execucao *ex, uint32_t k)
{
    const Produto *pr = ProjetoProduto(&ex->pl->projeto, k);
    Corrida *c = &ex->corrida[k];
    EstadoLinha linha;

    if (!ConferirDeclaradas(ex, k)) {
        c->saida = SAIDA_DIAGNOSTICO;
        return false;
    }
    if (!OrqLinhaDoEstado(ex->pl, k, &linha)) {
        fprintf(stderr, "forja: [%s] o filho terminou, mas o .entradas, a saida ou o .h dele nao puderam ser lidos\n", pr->nome);
        c->saida = SAIDA_INTERNO;
        return false;
    }
    GuardarLinha(&ex->pl->estado, &linha);
    if (!GravarEstado(ex, k)) {
        c->saida = SAIDA_INTERNO;
        return false;
    }
    NotarSaidaEmUso(c, pr);
    return true;
}

/* vida_do_filho e latencia do filho que acordou o orquestrador (precisa do handle: antes do ProcessoConcluir). */
static void NotarFilho(const Execucao *ex, uint32_t k)
{
    uint64_t criacao, saida;

    if (!ex->pl->tempos.ativo || !ProcessoTempos(&ex->corrida[k].proc, &criacao, &saida)) return;
    TemposDoFilho(&ex->pl->tempos, ProjetoProduto(&ex->pl->projeto, k)->nome, criacao, saida);
}

static bool Varrer(Execucao *ex);

static void Concluir(Execucao *ex, uint32_t k)
{
    Corrida *c = &ex->corrida[k];
    uint64_t ini;

    NotarFilho(ex, k);
    c->saida = ProcessoConcluir(&c->proc);
    c->ms = (uint32_t)(MsAgora() - c->inicio_ms);
    ex->rodando--;
    while (Varrer(ex)) continue; /* vaga livre: dispara o que nao depende de k (segue RODANDO, dependentes esperam) antes do pos-processamento de k */
    ini =TemposAgora(&ex->pl->tempos);
    EmitirBloco(ex, k);
    TemposFechar(&ex->pl->tempos, "emitir_bloco", ProjetoProduto(&ex->pl->projeto, k)->nome, ini);
    c->situacao = (c->saida == 0 && ConferirEGuardar(ex, k)) ? SIT_RECOMPILADO : SIT_FALHOU;
}

/* ---------- fila ---------- */

static Prontidao ProntidaoDe(const Execucao *ex, uint32_t k, uint32_t *causa)
{
    const Produto *pr = ProjetoProduto(&ex->pl->projeto, k);
    Prontidao prontidao = DEPS_LIVRES;

    if (ex->corrida[k].situacao == SIT_PRONTO) return prontidao;
    for (uint32_t j = 0; j < pr->ndep; j++) {
        Situacao s = ex->corrida[pr->dep[j]].situacao;

        if (s >= SIT_FALHOU) { /* FALHOU e PULADO sao os dois ultimos do enum Situacao */
            *causa = pr->dep[j];
            return DEPS_FALHAS;
        }
        if (s <= SIT_RODANDO) prontidao = DEPS_ESPERA; /* PENDENTE, PRONTO e RODANDO sao os tres primeiros */
    }
    return prontidao;
}

/* Dependencias terminadas: a chave de hoje (com a hash_interface real das dlls que acabaram de ser feitas) manda reusar ou deixa o produto PRONTO para disparar. */
static void Avaliar(Execucao *ex, uint32_t k)
{
    Corrida *c = &ex->corrida[k];
    bool recompila = OrqAvaliarProduto(ex->pl, k, c->motivo, sizeof c->motivo);

    c->situacao = recompila ? SIT_PRONTO : SIT_REUSADO;
}

/* PENDENTE ou PRONTO: pula, avalia, dispara se houver vaga. true = a situacao mudou. */
static bool TratarProduto(Execucao *ex, uint32_t k)
{
    Corrida *c = &ex->corrida[k];
    uint32_t causa = 0;
    Prontidao prontidao = ProntidaoDe(ex, k, &causa);
    bool mudou = c->situacao == SIT_PENDENTE; /* so o PENDENTE ainda precisa de Avaliar */

    if (prontidao == DEPS_ESPERA) return false;
    if (prontidao == DEPS_FALHAS) {
        c->situacao = SIT_PULADO;
        c->causa = causa;
        return true;
    }
    if (mudou) Avaliar(ex, k);
    if (c->situacao == SIT_REUSADO || ex->rodando >= ex->jobs) return mudou;
    Disparar(ex, k);
    return true;
}

/* Uma varredura em (camada, nome); true = alguma coisa mudou (dispararam, reusaram ou pularam produtos). */
static bool Varrer(Execucao *ex)
{
    bool mudou = false;

    for (size_t i = 0; i < ex->pl->projeto.produtos.n; i++) {
        uint32_t k = ex->pl->ordem[i];
        Situacao s = ex->corrida[k].situacao;

        if (s == SIT_PENDENTE || s == SIT_PRONTO) mudou |= TratarProduto(ex, k);
    }
    return mudou;
}

static void EsperarUm(Execucao *ex)
{
    Processo lista[EXEC_JOBS_MAX];
    uint32_t dono[EXEC_JOBS_MAX], n = 0, qual = 0;

    for (uint32_t k = 0; k < ex->pl->projeto.produtos.n; k++) {
        if (ex->corrida[k].situacao != SIT_RODANDO) continue;
        lista[n] = ex->corrida[k].proc;
        dono[n++] = k;
    }
    if (!ProcessoEsperarQualquer(lista, n, &qual)) FalhaInterna("a espera pelos filhos falhou (erro %lu)", (unsigned long)GetLastError());
    TemposAcordou(&ex->pl->tempos);
    Concluir(ex, dono[qual]);
}

/* Varre ate nada mudar; com filho rodando espera um terminar e varre de novo; sem filho rodando e nada a mudar, acabou. */
static void Escalonar(Execucao *ex)
{
    for (;;) {
        while (Varrer(ex)) {
        }
        if (ex->rodando == 0) return;
        EsperarUm(ex);
    }
}

/* ---------- resumo ---------- */

static Contagem Contar(const Execucao *ex)
{
    Contagem n = {0, 0, 0, 0};

    for (size_t k = 0; k < ex->pl->projeto.produtos.n; k++) {
        Situacao s = ex->corrida[k].situacao;

        n.recompilados += s == SIT_RECOMPILADO;
        n.reusados += s == SIT_REUSADO;
        n.falharam += s == SIT_FALHOU;
        n.pulados += s == SIT_PULADO;
    }
    return n;
}

static const char *NomeDaCausa(const Execucao *ex, const Corrida *c)
{
    return ProjetoProduto(&ex->pl->projeto, c->causa)->nome;
}

static void ResumoTexto(const Execucao *ex, uint32_t k)
{
    const Corrida *c = &ex->corrida[k];
    const char *nome = ProjetoProduto(&ex->pl->projeto, k)->nome;

    if (c->situacao == SIT_RECOMPILADO) {
        printf("forja: [%s] recompilado (%s) %u ms", nome, c->motivo, c->ms);
        if (c->velho[0] != '\0') printf(" | saida em uso: a antiga ficou em %s", c->velho);
        putchar('\n');
    } else if (c->situacao == SIT_REUSADO) {
        printf("forja: [%s] reusado\n", nome);
    } else if (c->situacao == SIT_FALHOU) {
        printf("forja: [%s] falhou (exit %d)\n", nome, c->saida);
    } else {
        printf("forja: [%s] pulado (depende de %s)\n", nome, NomeDaCausa(ex, c));
    }
}

static const char *AcaoDe(Situacao s)
{
    if (s == SIT_RECOMPILADO) return "recompilado";
    if (s == SIT_REUSADO) return "reusado";
    return s == SIT_FALHOU ? "falhou" : "pulado";
}

static void ResumoJson(const Execucao *ex, uint32_t k)
{
    const Corrida *c = &ex->corrida[k];
    char motivo[ORQ_MOTIVO_MAX + PROJETO_NOME_MAX] = "";

    if (c->situacao == SIT_PULADO) snprintf(motivo, sizeof motivo, "depende de %s", NomeDaCausa(ex, c));
    if (c->situacao == SIT_RECOMPILADO || c->situacao == SIT_FALHOU) snprintf(motivo, sizeof motivo, "%s", c->motivo);
    printf("{\"produto\": \"%s\", \"acao\": \"%s\", \"motivo\": ", ProjetoProduto(&ex->pl->projeto, k)->nome, AcaoDe(c->situacao));
    OrqTextoJson(motivo);
    printf(", \"ms\": %u, \"exit\": %d", c->ms, c->saida);
    if (c->velho[0] != '\0') {
        printf(", \"antigo\": ");
        OrqTextoJson(c->velho);
    }
    puts("}");
}

static void ResumoProjeto(const Execucao *ex)
{
    Contagem n = Contar(ex);
    uint32_t total = (uint32_t)ex->pl->projeto.produtos.n;
    unsigned long long ms = MsAgora() - ex->inicio_ms;

    if (ex->pl->opcoes->json)
        printf("{\"projeto\": {\"produtos\": %u, \"recompilados\": %u, \"reusados\": %u, \"falharam\": %u, \"pulados\": %u, \"jobs\": %u, \"ms\": %llu, \"filhos\": %u}}\n", total,
               n.recompilados, n.reusados, n.falharam, n.pulados, ex->jobs, ms, ex->filhos);
    else
        printf("forja: projeto: %u produtos, %u recompilados, %u reusados, %u falharam, %u pulados, jobs=%u, %llu ms\n", total, n.recompilados, n.reusados, n.falharam,
               n.pulados, ex->jobs, ms);
    fflush(stdout);
}

/* O resumo, na ordem (camada, nome): igual com 1 ou N jobs. */
static void ImprimirResumo(const Execucao *ex)
{
    for (size_t i = 0; i < ex->pl->projeto.produtos.n; i++) {
        if (ex->pl->opcoes->json)
            ResumoJson(ex, ex->pl->ordem[i]);
        else
            ResumoTexto(ex, ex->pl->ordem[i]);
    }
    ResumoProjeto(ex);
}

/* O maior exit entre os filhos que falharam; fora de 1..3 (queda do processo) conta como 3. Pulado nao conta. */
static int CodigoAgregado(const Execucao *ex)
{
    int maior = 0;

    for (size_t k = 0; k < ex->pl->projeto.produtos.n; k++) {
        int codigo = ex->corrida[k].saida;

        if (ex->corrida[k].situacao != SIT_FALHOU) continue;
        if (codigo < 1 || codigo > SAIDA_INTERNO) codigo = SAIDA_INTERNO;
        if (codigo > maior) maior = codigo;
    }
    return maior;
}

/* ---------- fluxo ---------- */

static void LerInjecao(Execucao *ex)
{
    const char *produto = getenv(EXEC_INJETAR_PRODUTO);
    const char *codigo = getenv(EXEC_INJETAR);

    if (produto == NULL || codigo == NULL) return;
    snprintf(ex->injetar_produto, sizeof ex->injetar_produto, "%s", produto);
    snprintf(ex->injetar, sizeof ex->injetar, "%s", codigo);
}

static bool IniciarExecucao(Execucao *ex, Plano *pl)
{
    DWORD n;

    ex->pl = pl;
    if (!ResolverJobs(pl, &ex->jobs)) return false;
    n = GetModuleFileNameA(NULL, ex->exe, sizeof ex->exe);
    if (n == 0 || n >= sizeof ex->exe) {
        fputs("forja: nao consegui o caminho do proprio forja.exe\n", stderr);
        return false;
    }
    ex->corrida = calloc(pl->projeto.produtos.n, sizeof *ex->corrida);
    if (ex->corrida == NULL) FalhaInterna("memoria esgotada no orquestrador");
    snprintf(ex->cache, sizeof ex->cache, "%s\\.forja_cache", pl->projeto.pasta);
    LerInjecao(ex);
    if (!ProcessoJobCriar(&ex->job)) fputs("forja: aviso: nao consegui criar o job object; os filhos nao morrem sozinhos se o orquestrador morrer\n", stderr);
    ex->inicio_ms = MsAgora();
    return true;
}

static void FinalizarExecucao(Execucao *ex)
{
    ProcessoJobFechar(&ex->job);
    if (ex->trava != NULL && ex->trava != INVALID_HANDLE_VALUE) CloseHandle(ex->trava);
    free(ex->corrida);
}

/* Pasta do cache, trava (B0008) e preparo; depois a fila, o resumo e o exit. */
static int Trabalhar(Execucao *ex)
{
    ResultadoTrava trava;
    char causa[ORQ_CAMINHO_MAX];
    uint64_t ini;

    if (!PastaGarantir(ex->cache)) {
        fprintf(stderr, "forja: nao consegui criar a pasta '%s'\n", ex->cache);
        return SAIDA_USO;
    }
    trava = TravaAbrir(ex);
    if (trava == TRAVA_OCUPADA) {
        snprintf(causa, sizeof causa, "%s: projeto em build por outro processo", ex->pl->projeto.pasta);
        DiagImprimirJsonSemFonte(B0008, causa, stderr);
    }
    if (trava != TRAVA_OK || !PrepararPastas(ex) || !OrqPrepararPlano(ex->pl)) return SAIDA_USO;
    Escalonar(ex);
    ini = TemposAgora(&ex->pl->tempos);
    ImprimirResumo(ex);
    TemposFechar(&ex->pl->tempos, "resumo", NULL, ini);
    return CodigoAgregado(ex);
}

int OrqConstruir(Plano *pl)
{
    Execucao ex = {0};
    int codigo = SAIDA_USO;

    if (IniciarExecucao(&ex, pl)) codigo = Trabalhar(&ex);
    FinalizarExecucao(&ex);
    return codigo;
}
