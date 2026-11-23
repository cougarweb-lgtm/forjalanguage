/* teste_common_projeto.c - verificacoes de unidade do manifesto, das camadas, da chave e do motivo do orquestrador de projeto (ProjetoVerificacoes, chamada pelo teste_common): 4 do P22-O1b.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22-O1b: parser do forja.proj (ProjetoLer, B0005), camadas (ProjetoCamadas, B0007), chave estavel (EstadoChave, EstadoAmbiente, EstadoDepsTexto, ida e volta do
 *                    estado) e motivo (EstadoMotivo). A chave tem valor fixo calculado fora do C (FNV-1a 64 sobre o texto canonico).
 * Verificacao: (em language\compiler) construir.bat && teste_common.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "cache/estado.h"
#include "cache/projeto_grafo.h"
#include "cache/projeto_ler.h"

#define PROJETO_TAM_PASTA 600
#define PROJETO_TAM_ARQUIVO 700
#define PROJETO_TAM_ENTRADA 8

typedef struct {
    int *total;
    int *falhas;
} Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (projeto) linha %d: %s\n", linha, texto);
}

#define PROJETO_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

static void Escreve(const char *pasta, const char *nome, const char *texto)
{
    char caminho[PROJETO_TAM_ARQUIVO];
    FILE *arq;

    snprintf(caminho, sizeof caminho, "%s\\%s", pasta, nome);
    arq = fopen(caminho, "wb");
    if (arq == NULL)
        return;
    fputs(texto, arq);
    fclose(arq);
}

static void Apaga(const char *pasta, const char *nome)
{
    char caminho[PROJETO_TAM_ARQUIVO];

    snprintf(caminho, sizeof caminho, "%s\\%s", pasta, nome);
    remove(caminho);
}

static bool TerminaCom(const char *texto, const char *fim)
{
    size_t a = strlen(texto), b = strlen(fim);

    return a >= b && strcmp(texto + a - b, fim) == 0;
}

static const char *const ENTRADAS_DO_PROJETO[PROJETO_TAM_ENTRADA] = {"a.fj", "b.fj", "c.fj", "d.fj", "app.fj", "lib.fj", "x.fj", "y.fj"};

/* Cria os "<nome>.fj" das entradas e o forja.proj com o texto dado; devolve o caminho do manifesto. */
static void MontarProjeto(const char *pasta, const char *manifesto, char *caminho, size_t tam)
{
    for (size_t k = 0; k < PROJETO_TAM_ENTRADA; k++)
        Escreve(pasta, ENTRADAS_DO_PROJETO[k], "proc main() {\n}\n");
    Escreve(pasta, "forja.proj", manifesto);
    snprintf(caminho, tam, "%s\\forja.proj", pasta);
}

static void RemoverProjeto(const char *pasta)
{
    for (size_t k = 0; k < PROJETO_TAM_ENTRADA; k++)
        Apaga(pasta, ENTRADAS_DO_PROJETO[k]);
    Apaga(pasta, "forja.proj");
}

/* ---------- 1: parser ---------- */

static bool ParserLeProdutosEOrdena(const char *pasta)
{
    char caminho[PROJETO_TAM_ARQUIVO];
    Projeto p;
    Diag erro = {0};
    bool ok;

    MontarProjeto(pasta, "# comentario\nsaida = out\nproduto app exe app.fj depende lib   # fim\n\nproduto lib dll lib.fj\n", caminho, sizeof caminho);
    ok = ProjetoLer(caminho, &p, &erro);
    ok = ok && p.produtos.n == 2 && strcmp(ProjetoProduto(&p, 0)->nome, "app") == 0 && strcmp(ProjetoProduto(&p, 1)->nome, "lib") == 0;
    ok = ok && ProjetoProduto(&p, 0)->ndep == 1 && ProjetoProduto(&p, 0)->dep[0] == 1 && ProjetoProduto(&p, 1)->tipo == PRODUTO_DLL;
    ok = ok && TerminaCom(ProjetoProduto(&p, 1)->saida, "\\out\\lib.dll") && TerminaCom(ProjetoProduto(&p, 1)->cabecalho, "\\out\\lib.h");
    ok = ok && TerminaCom(ProjetoProduto(&p, 0)->saida, "\\out\\app.exe") && ProjetoProduto(&p, 0)->cabecalho[0] == '\0';
    if (ok)
        ProjetoLiberar(&p);
    return ok;
}

static bool ParserRecusaNomeRepetido(const char *pasta)
{
    char caminho[PROJETO_TAM_ARQUIVO];
    Projeto p;
    Diag erro = {0};
    bool lido;

    MontarProjeto(pasta, "produto a exe a.fj\nproduto a dll b.fj\n", caminho, sizeof caminho);
    lido = ProjetoLer(caminho, &p, &erro);
    return !lido && erro.tem && erro.codigo == B0005 && strstr(erro.causa, "linha 2: nome repetido 'a'") != NULL;
}

/* ---------- 2: camadas ---------- */

static bool CamadasOrdenam(const char *pasta)
{
    static const uint32_t CAMADA_ESPERADA[4] = {0, 1, 2, 0};
    static const uint32_t ORDEM_ESPERADA[4] = {0, 3, 1, 2};
    char caminho[PROJETO_TAM_ARQUIVO];
    uint32_t camada[4], ordem[4];
    Projeto p;
    Diag erro = {0};
    bool ok;

    MontarProjeto(pasta, "produto c exe c.fj depende b\nproduto b dll b.fj depende a\nproduto a dll a.fj\nproduto d exe d.fj\n", caminho, sizeof caminho);
    if (!ProjetoLer(caminho, &p, &erro))
        return false;
    ok = ProjetoCamadas(&p, camada, ordem, &erro) && memcmp(camada, CAMADA_ESPERADA, sizeof camada) == 0 && memcmp(ordem, ORDEM_ESPERADA, sizeof ordem) == 0;
    ProjetoLiberar(&p);
    return ok;
}

static bool CicloEhB0007(const char *pasta)
{
    char caminho[PROJETO_TAM_ARQUIVO];
    uint32_t camada[2], ordem[2];
    Projeto p;
    Diag erro = {0};
    bool ok;

    MontarProjeto(pasta, "produto x exe x.fj depende y\nproduto y dll y.fj depende x\n", caminho, sizeof caminho);
    if (!ProjetoLer(caminho, &p, &erro))
        return false;
    ok = !ProjetoCamadas(&p, camada, ordem, &erro) && erro.tem && erro.codigo == B0007 && strcmp(erro.causa, "ciclo de dependencia: x -> y -> x") == 0;
    ProjetoLiberar(&p);
    return ok;
}

/* ---------- 3: chave ---------- */

static ChavePedido PedidoFixo(const EstadoDep *deps, const EstadoArquivo *arquivos)
{
    ChavePedido cp = {0};

    cp.forja = UINT64_C(0x0123456789abcdef);
    cp.alvo = "x86_64-windows";
    cp.modo = "dev";
    cp.tipo = "exe";
    cp.entrada = "C:\\p\\app.fj";
    cp.lib_pasta = "C:\\p\\bin";
    cp.deps = deps;
    cp.ndeps = 2;
    cp.arquivos = arquivos;
    cp.narquivos = 2;
    return cp;
}

/* Valores calculados em Python (FNV-1a 64) sobre o texto canonico: a chave nao pode mudar sem o texto mudar. */
static bool ChaveTemValorFixo(void)
{
    EstadoDep deps[2] = {{"lib", true, 0xff}, {"motor", false, 0}};
    EstadoArquivo arquivos[2] = {{"C:\\p\\app.fj", UINT64_C(0xba759039a73ec70c)}, {"C:\\p\\mods\\util.fj", UINT64_C(0x9af6850c322b788e)}};
    ChavePedido cp = PedidoFixo(deps, arquivos);
    char texto[ESTADO_DEPS_MAX];
    uint64_t chave = EstadoChave(&cp), ambiente = EstadoAmbiente(&cp);
    bool ok = chave == UINT64_C(0x0563b48e752793f0) && ambiente == UINT64_C(0x19e812b1a8c9ad25) && chave == EstadoChave(&cp);

    arquivos[1].hash++;
    ok = ok && EstadoChave(&cp) == UINT64_C(0x05605e8e7524cbf7) && EstadoAmbiente(&cp) == ambiente;
    EstadoDepsTexto(&cp, texto, sizeof texto);
    return ok && strcmp(texto, "lib=00000000000000ff,motor=-") == 0;
}

/* Grava um estado, le de volta e confere as 8 colunas. */
static bool EstadoVaiEVolta(const char *pasta)
{
    char caminho[PROJETO_TAM_ARQUIVO];
    Estado e, lido;
    EstadoLinha *l;
    const EstadoLinha *achada;
    bool ok;

    snprintf(caminho, sizeof caminho, "%s\\estado.tsv", pasta);
    VecIniciar(&e.linhas, sizeof(EstadoLinha));
    l = VecEmpurrar(&e.linhas);
    snprintf(l->nome, sizeof l->nome, "lib");
    snprintf(l->saida, sizeof l->saida, "C:\\p\\bin\\lib.dll");
    snprintf(l->deps, sizeof l->deps, "motor=00000000000000aa");
    l->chave = UINT64_C(0x1122334455667788);
    l->tem_interface = true;
    l->hash_interface = 0xbeef;
    l->tamanho = 4096;
    l->mtime = UINT64_C(134387910829012890);
    l->ambiente = UINT64_C(0x99);
    ok = EstadoGravarAtomico(caminho, &e, NULL) && EstadoLer(caminho, &lido);
    achada = ok ? EstadoAchar(&lido, "lib") : NULL;
    ok = ok && achada != NULL && achada->chave == l->chave && achada->tem_interface && achada->hash_interface == 0xbeef && achada->tamanho == 4096;
    ok = ok && achada->mtime == l->mtime && achada->ambiente == 0x99 && strcmp(achada->deps, l->deps) == 0 && strcmp(achada->saida, l->saida) == 0;
    EstadoLiberar(&e);
    EstadoLiberar(&lido);
    remove(caminho);
    return ok;
}

/* ---------- 4: motivo ---------- */

static bool MotivoDiz(MotivoPedido fatos, const char *esperado)
{
    char texto[200];
    bool recompila = EstadoMotivo(&fatos, texto, sizeof texto);

    if (esperado == NULL)
        return !recompila;
    return recompila && strcmp(texto, esperado) == 0;
}

static bool MotivosPorPrioridade(void)
{
    MotivoPedido base = {0};
    MotivoPedido igual, mudou;
    bool ok;

    base.dep_mudou = "";
    base.arquivo_mudou = "";
    igual = base;
    igual.chave_igual = true;
    igual.ambiente_igual = true;
    igual.saida_ok = true;
    mudou = base;
    mudou.ambiente_igual = true;
    ok = MotivoDiz(igual, NULL);
    ok = ok && MotivoDiz((MotivoPedido){.forcado = true, .primeira_vez = true, .dep_mudou = "", .arquivo_mudou = ""}, "forcado (--tudo)");
    ok = ok && MotivoDiz((MotivoPedido){.primeira_vez = true, .dep_mudou = "", .arquivo_mudou = ""}, "primeira vez");
    igual.saida_ok = false;
    ok = ok && MotivoDiz(igual, "saida ausente ou alterada");
    ok = ok && MotivoDiz(base, "forja.exe, modo ou opcoes mudaram") && MotivoDiz(mudou, "forja.exe, modo ou opcoes mudaram");
    mudou.arquivo_mudou = "src\\a.fj";
    ok = ok && MotivoDiz(mudou, "fonte mudou: src\\a.fj");
    mudou.arquivo_e_lib = true;
    ok = ok && MotivoDiz(mudou, "biblioteca mudou: src\\a.fj");
    mudou.dep_mudou = "motor";
    return ok && MotivoDiz(mudou, "interface de motor mudou");
}

void ProjetoVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};
    const char *pasta_temporaria = getenv("TEMP");
    char pasta[PROJETO_TAM_PASTA];

    snprintf(pasta, sizeof pasta, "%s\\forja_projeto_%lu", pasta_temporaria != NULL ? pasta_temporaria : ".", (unsigned long)GetCurrentProcessId());
    CreateDirectoryA(pasta, NULL);
    PROJETO_VERIFICA(&c, ParserLeProdutosEOrdena(pasta) && ParserRecusaNomeRepetido(pasta));
    PROJETO_VERIFICA(&c, CamadasOrdenam(pasta) && CicloEhB0007(pasta));
    PROJETO_VERIFICA(&c, ChaveTemValorFixo() && EstadoVaiEVolta(pasta));
    PROJETO_VERIFICA(&c, MotivosPorPrioridade());
    RemoverProjeto(pasta);
    RemoveDirectoryA(pasta);
}
