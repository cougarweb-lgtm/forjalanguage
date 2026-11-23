/* opcoes.c - opcoes da linha de comando do forja: leitura por tabela e validacao por comando (extraido do main.c no P7b do M1).
 * Versao: 0.28.0
 * Changelog: 0.28.0 - M8 P2: opcao --perfil (marca; ValidarPerfil: build, run e test, exe; nao vale com --modo dll nem --suporte, pelo mesmo erro de opcao do --guarda-memoria); conta em TemOpcaoDeArquivo.
 *            0.27.0 - 0.21.0 F4 passo 2a: ConferirAposentados vira OpcoesConferirAposentados (publica) e roda em PrepararOpcoes, antes de OpcoesValidar: `build --modo c x.fj` sem -o sai com X0003 e nao com 'build exige -o'; sai o texto/validacao morto de --modo c (ValidarBuild, ValidarGuarda, ValidarChecagem, ValidarSemOtim, CompilacaoDeDev, OpcoesResolverBackend).
 *            0.26.0 - 0.21.0 F4 passo 1: --backend c, --modo c e --prova-layout saem com X0003 (o backend C foi aposentado; ConferirBackend, ConferirAposentados e ValidarProvaLayout removido); X64NaoGera sai.
 *            0.25.0 - 0.21.0 F2: --release sem --backend resolve para x64 (CompilacaoDeDev deixou de excluir o release; o backend c so por --modo c ou --backend c explicito).
 *            0.24.0 - M7a F0: opcao --sem-otim (ValidarSemOtim, so release) e no TemOpcaoDeArquivo.
 *            0.23.0 - O0 P13: opcao --checagem sim|nao (ValidarChecagem) e OpcoesChecagem (dev sempre confere; padrao do release provisorio sim ate o P5).
 *            0.22.0 - M6e P93d: opcao --guarda-memoria (marca; ValidarGuarda: so dev, backend x64, exe).
 *            0.21.0 - M6 P66: opcao --simbolos (sim|nao; ValidarSimbolos) e OpcoesSimbolos (padrao: sim em dev, nao em release).
 *            0.20.0 - M5b P41: opcao --pie (marca; ValidarPie: so com --target x86_64-linux, senao erro de uso).
 *            0.19.0 - F1 P2: opcao --canonico (marca; so check, OPCAO_MARCA e a recusa em OpcoesValidar).
 *            0.18.0 - S4 P2: forja test <prefixo>:<nome> (std, vendor ou engine): ValidarAlvoDoTeste (prefixo desconhecido = uso errado) e OpcoesPrepararAlvoDoTeste (caminho virtual <pasta>\<prefixo>_<nome>.teste.fj e raiz sintetica em raiz_alvo).
 *            0.17.1 - IA1-b13: so comentario do ValidarRastreio (o backend C passa a ter pilha).
 *            0.17.0 - IA1-b12: opcao --rastreio (marca; so build, run e test, ValidarRastreio).
 *            0.16.0 - IA1-I1: opcoes --no <id> e --sem-pos (so ast; ValidarAst) e --projeto tambem no ast (mostra todos os modulos).
 *            0.15.0 - IA1-b10: opcoes --limite-tempo, --limite-mem, --limite-saida (run e test) e --args-file (so run); ValidarLimites (o formato do valor e de LimitesLer).
 *            0.14.0 - IA1-b02: opcao --resumo (marca; so check, build, run e test, ValidarResumo; o build --projeto a recusa).
 *            0.13.0 - M5 P27a (D-M5-9): recebe do main.c a conferencia de backend e alvo (OpcoesConferirBackendAlvo) e acrescenta o alvo x86_64-linux: AlvoEhLinux; --target x86_64-linux implica --backend x64 (OpcoesResolverBackend);
 *                    X0003 com a saida certa para --backend c / --modo c, --modo dll (M5c), run e test sem -o; forja test aceita -o so com o alvo linux; build --projeto so gera x86_64-windows.
 *            0.12.0 - M4 P25b (D-M4-8): OpcoesResolverBackend - sem --backend, build/run/test de um arquivo em dev (sem --release, modo exe ou dll) recebem x64; --release e --modo c ficam no C.
 *            0.11.0 - M3 P22-O2: opcao --jobs N (1..64; so com --projeto; OpcoesJobsTexto, ValidarJobs).
 *            0.10.0 -M3 P22-O1b: opcoes --projeto, --plano e --tudo (build do projeto; ValidarProjeto recusa mistura com -o, --modo e as opcoes de arquivo .fj; o caminho do forja.proj e opcional).
 *            0.9.0 - M3 P22-O1a: opcoes --entradas <arq> e --lib-pasta <pasta> (so build; ValidarEntradas) e OpcoesConferirSoX64 (X0003 fora do backend x64).
 *            0.8.0 - Linguagem v1.11 L16a: opcao --filtro "<trecho>" (so test, ate 255 bytes; ValidarFiltro), --json passa a valer tambem no test e OpcoesAmbienteDoTeste.
 *            0.7.0 - Linguagem v1.11 L7: opcao --max-erros N (1..100; OpcoesMaxErros, ValidarMaxErros; o explicar nao a aceita).
 *            0.6.0 - Linguagem v1.11 L6: opcao --stdin (marca; so check, ValidarStdin).
 *            0.5.0 - P18a: opcao --traco (marca; so build, run e test, ValidarTraco).
 *            0.4.0 - forja run ... -- args: o '--' encerra as opcoes do forja e o resto do argv vai intacto ao programa (so no run).
 *            0.3.0 - backend x64 P8b: opcoes --suporte e --json (build --backend x64, sem -o), OpcoesLinhaDeComando.
 *            0.2.0 - backend x64 P8: opcao --debug-code (so build).
 *            0.1.0 - backend x64 P7b: texto movido do main.c 0.18.0 sem mudar comportamento (LerOpcoes -> OpcoesLer, ValidarOpcoes -> OpcoesValidar);
 *                    BackendEhX64 e novo.
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <direct.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/diag.h"
#include "driver/limites.h"
#include "driver/opcoes.h"
#include "nativo/nativo.h"
#include "nativo/optimizer/passes.h"

#define OPCOES_ALVO_WINDOWS "x86_64-windows"
#define OPCOES_ALVO_LINUX "x86_64-linux"
#define OPCOES_FILTRO_MAX 255
#define OPCOES_JOBS_MAX 64          /* MAXIMUM_WAIT_OBJECTS do WaitForMultipleObjects */

/* Opcao de linha de comando: com valor (o proximo argv) ou marca (liga um bool). destino = offsetof do campo em Opcoes. */
typedef enum { OPCAO_VALOR, OPCAO_MARCA } TipoOpcao;
typedef struct { const char *nome; TipoOpcao tipo; size_t destino; } OpcaoCli;

static const OpcaoCli OPCOES_CLI[] = {
    {"-o", OPCAO_VALOR, offsetof(Opcoes, saida)},
    {"--modo", OPCAO_VALOR, offsetof(Opcoes, modo)},
    {"--tempos", OPCAO_VALOR, offsetof(Opcoes, tempos)},
    {"--backend", OPCAO_VALOR, offsetof(Opcoes, backend)},
    {"--target", OPCAO_VALOR, offsetof(Opcoes, target)},
    {"--prova-layout", OPCAO_VALOR, offsetof(Opcoes, prova)},
    {"--debug-code", OPCAO_VALOR, offsetof(Opcoes, debug_code)},
    {"--max-erros", OPCAO_VALOR, offsetof(Opcoes, max_erros)},
    {"--filtro", OPCAO_VALOR, offsetof(Opcoes, filtro)},
    {"--entradas", OPCAO_VALOR, offsetof(Opcoes, entradas)},
    {"--lib-pasta", OPCAO_VALOR, offsetof(Opcoes, lib_pasta)},
    {"--jobs", OPCAO_VALOR, offsetof(Opcoes, jobs)},
    {"--limite-tempo", OPCAO_VALOR, offsetof(Opcoes, limite_tempo)},
    {"--limite-mem", OPCAO_VALOR, offsetof(Opcoes, limite_mem)},
    {"--limite-saida", OPCAO_VALOR, offsetof(Opcoes, limite_saida)},
    {"--args-file", OPCAO_VALOR, offsetof(Opcoes, args_file)},
    {"--no", OPCAO_VALOR, offsetof(Opcoes, no)},
    {"--sem-pos", OPCAO_MARCA, offsetof(Opcoes, sem_pos)},
    {"--tipos", OPCAO_MARCA, offsetof(Opcoes, tipos)},
    {"--release", OPCAO_MARCA, offsetof(Opcoes, release)},
    {"--suporte", OPCAO_MARCA, offsetof(Opcoes, suporte)},
    {"--json", OPCAO_MARCA, offsetof(Opcoes, json)},
    {"--resumo", OPCAO_MARCA, offsetof(Opcoes, resumo)},
    {"--traco", OPCAO_MARCA, offsetof(Opcoes, traco)},
    {"--rastreio", OPCAO_MARCA, offsetof(Opcoes, rastreio)},
    {"--pie", OPCAO_MARCA, offsetof(Opcoes, pie)},
    {"--guarda-memoria", OPCAO_MARCA, offsetof(Opcoes, guarda_memoria)},
    {"--perfil", OPCAO_MARCA, offsetof(Opcoes, perfil)},
    {"--simbolos", OPCAO_VALOR, offsetof(Opcoes, simbolos)},
    {"--checagem", OPCAO_VALOR, offsetof(Opcoes, checagem)},
    {"--sem-otim", OPCAO_VALOR, offsetof(Opcoes, sem_otim)},
    {"--stdin", OPCAO_MARCA, offsetof(Opcoes, stdin_fonte)},
    {"--projeto", OPCAO_MARCA, offsetof(Opcoes, projeto)},
    {"--plano", OPCAO_MARCA, offsetof(Opcoes, plano)},
    {"--tudo", OPCAO_MARCA, offsetof(Opcoes, tudo)},
    {"--canonico", OPCAO_MARCA, offsetof(Opcoes, canonico)},
};

static const OpcaoCli *AcharOpcao(const char *nome)
{
    for (size_t k = 0; k < sizeof OPCOES_CLI / sizeof OPCOES_CLI[0]; k++) {
        if (strcmp(OPCOES_CLI[k].nome, nome) == 0) return &OPCOES_CLI[k];
    }
    return NULL;
}

/* Uma opcao (ou o arquivo de entrada) em argv[*i]. Devolve NULL se ok, ou o motivo do uso errado. */
static const char *LerOpcao(int argc, char **argv, int *i, Opcoes *o)
{
    const char *a = argv[*i];
    const OpcaoCli *op;

    if (a[0] != '-') {
        if (o->entrada != NULL) return "mais de um arquivo de entrada";
        o->entrada = a;
        return NULL;
    }
    op = AcharOpcao(a);
    if (op == NULL) return "opcao desconhecida";
    if (op->tipo == OPCAO_MARCA) {
        *(bool *)((char *)o + op->destino) = true;
        return NULL;
    }
    if (*i + 1 >= argc) return "opcao sem valor";
    *(const char **)((char *)o + op->destino) = argv[++*i];
    return NULL;
}

/* Devolve NULL se ok, ou o motivo do uso errado. */
const char *OpcoesLer(int argc, char **argv, Opcoes *o)
{
    const char *erro;

    o->comando = argv[1];
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {   /* o resto e do programa, nao do forja */
            o->args_programa = (const char *const *)(argv + i + 1);
            o->nargs_programa = argc - i - 1;
            break;
        }
        erro = LerOpcao(argc, argv, &i, o);
        if (erro != NULL) return erro;
    }
    if (o->entrada != NULL || o->suporte || o->projeto) return NULL;
    return strcmp(o->comando, "explicar") == 0 ? "falta o codigo do diagnostico" : "falta o arquivo .fj";
}

/* Acrescenta " arg" (entre aspas se tiver espaco) a saida[usado..n); devolve o novo usado, no maximo n - 1. */
static size_t AnexarArgumento(char *saida, size_t n, size_t usado, const char *arg)
{
    const char *aspas = strchr(arg, ' ') != NULL ? "\"" : "";
    int escrito = snprintf(saida + usado, n - usado, " %s%s%s", aspas, arg, aspas);

    if (escrito < 0 || (size_t)escrito >= n - usado) return n - 1;
    return usado + (size_t)escrito;
}

void OpcoesLinhaDeComando(int argc, char **argv, char *saida, size_t n)
{
    size_t usado = (size_t)snprintf(saida, n, "forja");

    for (int k = 1; k < argc && usado < n - 1; k++) usado = AnexarArgumento(saida, n, usado, argv[k]);
}

/* Variavel de ambiente do filho: valor vazio remove a variavel do processo. */
static void DefinirAmbiente(const char *nome, const char *valor)
{
    _putenv_s(nome, valor != NULL ? valor : "");
}

void OpcoesAmbienteDoTeste(const Opcoes *o)
{
    DefinirAmbiente("FORJA_TESTE_FILTRO", o->filtro);
    DefinirAmbiente("FORJA_TESTE_JSON", o->json ? "1" : NULL);
}

bool ComandoEh(const Opcoes *o, const char *nome)
{
    return strcmp(o->comando, nome) == 0;
}

static bool ComandoConhecido(const Opcoes *o)
{
    static const char *const COMANDOS[] = {"check", "ast", "build", "test", "run", "explicar"};

    for (size_t k = 0; k < sizeof COMANDOS / sizeof COMANDOS[0]; k++) {
        if (ComandoEh(o, COMANDOS[k])) return true;
    }
    return false;
}

bool ModoEhC(const Opcoes *o)
{
    return o->modo != NULL && strcmp(o->modo, "c") == 0;
}

bool ModoEhDll(const Opcoes *o)
{
    return o->modo != NULL && strcmp(o->modo, "dll") == 0;
}

bool BackendEhX64(const Opcoes *o)
{
    return o->backend != NULL && strcmp(o->backend, "x64") == 0;
}

bool AlvoEhLinux(const Opcoes *o)
{
    return o->target != NULL && strcmp(o->target, OPCOES_ALVO_LINUX) == 0;
}

/* build, run e test de UM arquivo, em dev ou em --release (0.21.0 F2: o release tambem e x64), em modo exe ou dll: o grupo que o padrao leva ao x64. */
static bool CompilacaoDeDev(const Opcoes *o)
{
    if (o->projeto) return false;
    return ComandoEh(o, "build") || ComandoEh(o, "run") || ComandoEh(o, "test");
}

void OpcoesResolverBackend(Opcoes *o)
{
    if (o->backend == NULL && (CompilacaoDeDev(o) || AlvoEhLinux(o))) o->backend = "x64"; /* o ELF so existe no x64, tambem em --release */
}

uint32_t OpcoesMaxErros(const Opcoes *o)
{
    char *fim;
    long n;

    if (o->max_erros == NULL) return 0;
    n = strtol(o->max_erros, &fim, 10);
    return (fim != o->max_erros && *fim == '\0' && n >= 1 && n <= DIAG_MAX) ? (uint32_t)n : 0;
}

bool OpcoesJobsTexto(const char *texto, uint32_t *n)
{
    char *fim;
    long v;

    if (texto == NULL || texto[0] < '0' || texto[0] > '9') return false;
    v = strtol(texto, &fim, 10);
    if (*fim != '\0' || v < 1 || v > OPCOES_JOBS_MAX) return false;
    *n = (uint32_t)v;
    return true;
}

/* --max-erros N: N inteiro de 1 a DIAG_MAX (100); 0, texto ou fora do intervalo e uso errado. */
static const char *ValidarMaxErros(const Opcoes *o)
{
    if (o->max_erros == NULL || OpcoesMaxErros(o) != 0) return NULL;
    return "--max-erros aceita um numero inteiro de 1 a 100";
}

/* Regras do comando build: exe (padrao) ou dll; --release so vale no exe e na dll. */
static const char *ValidarBuild(const Opcoes *o)
{
    if (o->modo != NULL && !ModoEhDll(o) && strcmp(o->modo, "exe") != 0) return "--modo aceita exe ou dll";
    if (o->saida == NULL && !o->suporte) return "build exige -o <saida>";
    return NULL;
}

/* forja test <prefixo>:<nome> (S4 P2): o ':' so conta depois de 2 caracteres e antes de '\' ou '/' (C:\x.fj e um arquivo). */
static bool EntradaEhAlvo(const char *entrada)
{
    const char *dois = strchr(entrada, ':');
    if (dois == NULL || dois - entrada < 2) return false;
    return strcspn(entrada, "\\/") > (size_t)(dois - entrada);
}

/* Prefixo do alvo (o texto antes do ':') tem que ser std, vendor ou engine. */
static bool PrefixoDeAlvoConhecido(const char *entrada)
{
    static const char *const PREFIXOS[] = {"std", "vendor", "engine"};
    size_t n = strcspn(entrada, ":");

    for (size_t k = 0; k < sizeof PREFIXOS / sizeof PREFIXOS[0]; k++) {
        if (strlen(PREFIXOS[k]) == n && strncmp(entrada, PREFIXOS[k], n) == 0) return true;
    }
    return false;
}

/* Alvo de modulo so existe no test; prefixo desconhecido (xx:y) e uso errado (exit 2). */
static const char *ValidarAlvoDoTeste(const Opcoes *o)
{
    if (!ComandoEh(o, "test") || o->entrada == NULL || !EntradaEhAlvo(o->entrada)) return NULL;
    if (!PrefixoDeAlvoConhecido(o->entrada)) return "forja test aceita std:<modulo>, vendor:<modulo> ou engine:<modulo>";
    return NULL;
}

/* test e run so pegam o arquivo (run tambem --release): -o, --modo e --tipos seriam ignorados em silencio. */
static const char *ValidarTestRun(const Opcoes *o)
{
    bool saida_do_test_linux = ComandoEh(o, "test") && AlvoEhLinux(o); /* so o test do alvo linux tem -o: constroi o exe de teste e nao o roda */
    const char *erro_alvo = ValidarAlvoDoTeste(o);

    if (erro_alvo != NULL) return erro_alvo;
    if ((o->saida != NULL && !saida_do_test_linux) || o->modo != NULL || o->tipos) return "test e run nao aceitam -o, --modo ou --tipos";
    if (o->release && ComandoEh(o, "test")) return "--release so vale em run e build exe";
    return NULL;
}

/* explicar so pega o codigo: nenhuma opcao de compilacao vale nele. */
static const char *ValidarExplicar(const Opcoes *o)
{
    if (o->saida != NULL || o->modo != NULL || o->tempos != NULL || o->tipos || o->release || o->max_erros != NULL)
        return "explicar so aceita o codigo do diagnostico";
    return NULL;
}

static const char *ValidarCheckAst(const Opcoes *o)
{
    if (o->tipos && !ComandoEh(o, "ast")) return "--tipos so vale no comando ast";
    if (o->no != NULL && o->no[0] == '\0') return "--no precisa do id do no (campo \"id\" do forja ast)";
    if (o->release) return "--release so vale no build exe";
    return NULL;
}

/* --backend e --target so valem em build, run e test: nos outros comandos seriam ignorados em silencio. */
static const char *ValidarBackendAlvo(const Opcoes *o)
{
    if (o->backend == NULL && o->target == NULL) return NULL;
    if (ComandoEh(o, "build") || ComandoEh(o, "test") || ComandoEh(o, "run")) return NULL;
    return "--backend e --target so valem em build, run e test";
}

/* --debug-code grava <saida>.debug.txt: so o build tem um -o para ficar ao lado. */
static const char *ValidarDebugCode(const Opcoes *o)
{
    if (o->debug_code == NULL || ComandoEh(o, "build")) return NULL;
    return "--debug-code so vale em build";
}

/* --suporte --json lista o que o backend x64 recusa, sem gerar exe: so build --backend x64, e as duas marcas andam juntas. */
static const char *ValidarSuporte(const Opcoes *o)
{
    if (!o->suporte && !o->json) return NULL;
    if (!o->suporte) return ComandoEh(o, "test") ? NULL : "--json so vale com --suporte ou no test";
    if (!ComandoEh(o, "build") || !BackendEhX64(o)) return "--suporte so vale em build --backend x64";
    if (!o->json) return "--suporte exige --json";
    if (o->saida != NULL || o->debug_code != NULL) return "--suporte nao gera arquivo: sem -o nem --debug-code";
    return NULL;
}

/* --traco (P18a) grava o traco no programa que roda: so build exe (ou --modo c), run e test valem; dll e --suporte nao geram programa. */
static const char *ValidarTraco(const Opcoes *o)
{
    if (!o->traco) return NULL;
    if (!ComandoEh(o, "build") && !ComandoEh(o, "test") && !ComandoEh(o, "run")) return "--traco so vale em build, run e test";
    if (ModoEhDll(o) || o->suporte) return "--traco nao vale com --modo dll nem com --suporte";
    return NULL;
}

/* --rastreio (IA1-b12) enriquece a linha R do programa que roda: so build exe, run e test; dll e --suporte nao geram programa que rode (nos 2 backends em dev; o release a ignora). */
static const char *ValidarRastreio(const Opcoes *o)
{
    if (!o->rastreio) return NULL;
    if (!ComandoEh(o, "build") && !ComandoEh(o, "test") && !ComandoEh(o, "run")) return "--rastreio so vale em build, run e test";
    if (ModoEhDll(o) || o->suporte) return "--rastreio nao vale com --modo dll nem com --suporte";
    return NULL;
}

/* --guarda-memoria (M6e P93d, D-M6e-6): build, run e test em dev, backend x64 (implicito ou explicito) e exe; --release, --backend c, --modo c e --modo dll sao erro de opcao. */
static const char *ValidarGuarda(const Opcoes *o)
{
    bool comando_ok = ComandoEh(o, "build") || ComandoEh(o, "run") || ComandoEh(o, "test");
    bool backend_c = o->backend != NULL && strcmp(o->backend, "x64") != 0;

    if (!o->guarda_memoria) return NULL;
    if (!comando_ok || o->release || backend_c || ModoEhDll(o) || o->suporte) return "--guarda-memoria so vale em dev, backend x64, exe";
    return NULL;
}

/* --perfil (M8 P2, D-M8-5): build, run e test, em dev ou release, so exe (o backend x64 e o unico); --modo dll e --suporte sao erro de opcao. */
static const char *ValidarPerfil(const Opcoes *o)
{
    bool comando_ok = ComandoEh(o, "build") || ComandoEh(o, "run") || ComandoEh(o, "test");

    if (!o->perfil) return NULL;
    if (!comando_ok || ModoEhDll(o) || o->suporte) return "--perfil so vale em build, run e test, exe (nao com --modo dll nem --suporte)";
    return NULL;
}

/* --pie (P41) e o ELF static-pie: so vale com --target x86_64-linux (no Windows nao existe). */
static const char *ValidarPie(const Opcoes *o)
{
    if (!o->pie || AlvoEhLinux(o)) return NULL;
    return "--pie so vale com --target x86_64-linux (o static-pie e do ELF)";
}

/* --simbolos (M6 P66, D-M6-5): so sim ou nao. */
static const char *ValidarSimbolos(const Opcoes *o)
{
    if (o->simbolos == NULL || strcmp(o->simbolos, "sim") == 0 || strcmp(o->simbolos, "nao") == 0) return NULL;
    return "--simbolos aceita so sim ou nao";
}

bool OpcoesSimbolos(const Opcoes *o)
{
    if (o->simbolos == NULL) return !o->release;
    return strcmp(o->simbolos, "sim") == 0;
}

/* Padrao do release quando --checagem nao e passada: sim (M7a F9a: com o passe `checagem` o custo medido caiu para ~0,998 no x64; dev sempre confere; --checagem nao desliga por build). */
static const bool CHECAGEM_PADRAO_RELEASE = true;

/* --checagem (O0 P13): sim|nao em build, run e test; dev sempre confere (nao = erro) e --modo c nao a aceita (o C emitido confere, salvo -DFORJA_SEM_CHECAGEM). */
static const char *ValidarChecagem(const Opcoes *o)
{
    bool comando_ok = ComandoEh(o, "build") || ComandoEh(o, "run") || ComandoEh(o, "test");

    if (o->checagem == NULL) return NULL;
    if (strcmp(o->checagem, "sim") != 0 && strcmp(o->checagem, "nao") != 0) return "--checagem aceita so sim ou nao";
    if (!comando_ok) return "--checagem so vale em build, run e test";
    if (!o->release && strcmp(o->checagem, "nao") == 0) return "--checagem nao so vale com --release (o dev sempre confere indice e nulo)";
    return NULL;
}

bool OpcoesChecagem(const Opcoes *o)
{
    if (!o->release) return true;
    if (o->checagem == NULL) return CHECAGEM_PADRAO_RELEASE;
    return strcmp(o->checagem, "sim") == 0;
}

/* --sem-otim (M7a F0): passes de otimizacao da BIR desligados; so build, run e test, so --release, nao vale com --modo c; passo desconhecido lista os validos. */
static const char *ValidarSemOtim(const Opcoes *o)
{
    static char causa[300];
    bool comando_ok = ComandoEh(o, "build") || ComandoEh(o, "run") || ComandoEh(o, "test");

    if (o->sem_otim == NULL) return NULL;
    if (!comando_ok) return "--sem-otim so vale em build, run e test";
    if (!o->release) return "--sem-otim so vale com --release (o dev nunca roda passe de otimizacao)";
    if (!OtimConferirLista(o->sem_otim, causa, sizeof causa)) return causa;
    return NULL;
}

/* --entradas e --lib-pasta (P22-O1a) descrevem o que o build x64 le e grava: so o build os tem, e --suporte nao gera arquivo. */
static const char *ValidarEntradas(const Opcoes *o)
{
    if (o->entradas == NULL && o->lib_pasta == NULL) return NULL;
    if (!ComandoEh(o, "build")) return "--entradas e --lib-pasta so valem em build";
    if (o->suporte) return "--suporte nao gera arquivo: sem --entradas nem --lib-pasta";
    return NULL;
}

bool OpcoesConferirSoX64(const Opcoes *o)
{
    if ((o->entradas == NULL && o->lib_pasta == NULL) || BackendEhX64(o)) return true;
    DiagImprimirJsonSemFonte(X0003, "--entradas e --lib-pasta so existem com --backend x64", stderr);
    return false;
}

/* --modo c (emissao de C) e --prova-layout (prova contra o C) saem com o backend C, aposentado na 0.21.0: X0003. false = saiu o diagnostico.
 * Roda em PrepararOpcoes antes de OpcoesValidar: a validacao de build (-o) nao precisa ver o --modo c. */
bool OpcoesConferirAposentados(const Opcoes *o)
{
    if (ModoEhC(o)) {
        DiagImprimirJsonSemFonte(X0003, "o modo c (emissao de C portavel) foi aposentado na Forja 0.21.0; use --modo exe ou --modo dll", stderr);
        return false;
    }
    if (o->prova != NULL) {
        DiagImprimirJsonSemFonte(X0003, "--prova-layout dependia do backend C, aposentado na Forja 0.21.0", stderr);
        return false;
    }
    return true;
}

/* --backend (padrao: x64, tambem em --release desde a 0.21.0): so x64 existe; c foi aposentado na 0.21.0 e outro valor sai com X0003. false = saiu o diagnostico. */
static bool ConferirBackend(const Opcoes *o)
{
    char causa[256];
    const char *backend = o->backend;

    if (backend == NULL || strcmp(backend, "x64") == 0) return true;
    if (strcmp(backend, "c") == 0) {
        DiagImprimirJsonSemFonte(X0003, "o backend C foi aposentado na Forja 0.21.0: o unico backend e o x64 (o padrao); --backend c nao existe mais", stderr);
        return false;
    }
    snprintf(causa, sizeof causa, "backend desconhecido em --backend: '%s' (validos: x64)", backend);
    DiagImprimirJsonSemFonte(X0003, causa, stderr);
    return false;
}

/* --target x86_64-linux (D-M5-9): o ELF so sai do backend x64, em build e em test (que so constroi o exe de teste, em -o); cada combinacao fora disso e X0003 com o caminho certo. NULL = cabe. */
static const char *RecusaDoAlvoLinux(const Opcoes *o)
{
    if (!BackendEhX64(o)) return "o alvo x86_64-linux so existe no backend x64 (o padrao)";
    if (o->projeto) return "build --projeto so gera x86_64-windows nesta versao";
    if (ModoEhDll(o)) return NATIVO_X0003_SO;
    if (ComandoEh(o, "run")) return "o Windows nao executa ELF: build e rode no Linux";
    if (ComandoEh(o, "test") && o->saida == NULL) return "forja test no alvo x86_64-linux so constroi o exe de teste: passe -o <saida>";
    return NULL;
}

/* --target: x86_64-windows e o padrao; x86_64-linux vale nas combinacoes de RecusaDoAlvoLinux (X0003 nas outras) e outro valor sai com X0002. */
static bool ConferirAlvo(const Opcoes *o)
{
    char causa[256];
    const char *recusa;

    if (o->target == NULL || strcmp(o->target, OPCOES_ALVO_WINDOWS) == 0) return true;
    if (AlvoEhLinux(o)) {
        recusa = RecusaDoAlvoLinux(o);
        if (recusa != NULL) DiagImprimirJsonSemFonte(X0003, recusa, stderr);
        return recusa == NULL;
    }
    snprintf(causa, sizeof causa, "alvo desconhecido em --target: '%s' (validos: x86_64-windows|x86_64-linux)", o->target);
    DiagImprimirJsonSemFonte(X0002, causa, stderr);
    return false;
}

/* --debug-code descreve o que o backend x64 gerou (sem backend C: X0003). */
static bool ConferirDebugCode(const Opcoes *o)
{
    if (o->debug_code == NULL || BackendEhX64(o)) return true;
    DiagImprimirJsonSemFonte(X0003, "--debug-code so existe com --backend x64", stderr);
    return false;
}

bool OpcoesConferirBackendAlvo(const Opcoes *o)
{
    return ConferirBackend(o) && ConferirDebugCode(o) && OpcoesConferirSoX64(o) && ConferirAlvo(o);
}

/* --stdin le o .fj da entrada padrao: so o check (os outros comandos leem o arquivo do disco). */
static const char *ValidarStdin(const Opcoes *o)
{
    if (!o->stdin_fonte || ComandoEh(o, "check")) return NULL;
    return "--stdin so vale no comando check";
}

/* --filtro "<trecho>" (L16a) so tem o que filtrar no test; o runtime nativo guarda o trecho em 256 bytes. */
static const char *ValidarFiltro(const Opcoes *o)
{
    if (o->filtro == NULL) return NULL;
    if (!ComandoEh(o, "test")) return "--filtro so vale no comando test";
    if (strlen(o->filtro) > OPCOES_FILTRO_MAX) return "--filtro aceita no maximo 255 bytes";
    return NULL;
}

/* Os argumentos depois de '--' so tem para onde ir no run: nos outros comandos seriam ignorados em silencio. */
static const char *ValidarArgsPrograma(const Opcoes *o)
{
    if (o->nargs_programa > 0 && !ComandoEh(o, "run")) return "argumentos depois de -- so valem em run";
    return NULL;
}

/* --limite-tempo, --limite-mem e --limite-saida valem em run e test; --args-file so no run (como os argumentos depois de '--'); o formato de cada valor e conferido por LimitesLer. */
static const char *ValidarLimites(const Opcoes *o)
{
    Limites limites;
    bool tem_limite = o->limite_tempo != NULL || o->limite_mem != NULL || o->limite_saida != NULL;

    if (!tem_limite && o->args_file == NULL) return NULL;
    if (tem_limite && !ComandoEh(o, "run") && !ComandoEh(o, "test")) return "--limite-tempo, --limite-mem e --limite-saida so valem em run e test";
    if (o->args_file != NULL && !ComandoEh(o, "run")) return "--args-file so vale em run";
    return LimitesLer(o, &limites);
}

/* Alguma opcao que so faz sentido para UM arquivo .fj (saida, modo, tempos, --suporte, ...): nenhuma delas vale no build de projeto. */
static bool TemOpcaoDeArquivo(const Opcoes *o)
{
    const char *textos[] = {o->saida, o->modo, o->tempos, o->prova, o->debug_code, o->max_erros, o->filtro, o->entradas, o->lib_pasta,
                            o->limite_tempo, o->limite_mem, o->limite_saida, o->args_file, o->checagem, o->sem_otim};

    for (size_t k = 0; k < sizeof textos / sizeof textos[0]; k++) {
        if (textos[k] != NULL) return true;
    }
    return o->tipos || o->suporte || o->traco || o->rastreio || o->guarda_memoria || o->perfil || o->stdin_fonte || o->resumo || o->nargs_programa > 0;
}

/* --resumo (IA1-b02) troca a forma dos diagnosticos de compilacao: so o check, o build, o run e o test os emitem (o ast e o explicar nao). */
static const char *ValidarResumo(const Opcoes *o)
{
    if (!o->resumo || ComandoEh(o, "check") || ComandoEh(o, "build") || ComandoEh(o, "test") || ComandoEh(o, "run")) return NULL;
    return "--resumo so vale em check, build, run e test";
}

/* --projeto [forja.proj] [--release] [--plano] [--tudo] [--json]: so no build; o argumento e o manifesto, nunca um .fj. */
static const char *ValidarProjeto(const Opcoes *o)
{
    size_t tam = o->entrada != NULL ? strlen(o->entrada) : 0;
    uint32_t jobs;

    if (!ComandoEh(o, "build")) return "--projeto, --plano, --tudo e --jobs so valem em build";
    if (!o->projeto) return "--plano, --tudo e --jobs so valem com --projeto";
    if (o->jobs != NULL && !OpcoesJobsTexto(o->jobs, &jobs)) return "--jobs aceita um numero inteiro de 1 a 64";
    if (TemOpcaoDeArquivo(o)) return "--projeto nao aceita -o, --modo, --tempos, --suporte nem as opcoes de um arquivo .fj";
    if (tam >= 3 && _stricmp(o->entrada + tam - 3, ".fj") == 0) return "--projeto recebe o forja.proj, nao um arquivo .fj";
    return NULL;
}

const char *OpcoesValidar(const Opcoes *o)
{
    const char *erro;

    if (!ComandoConhecido(o)) return "comando desconhecido";
    if ((o->sem_pos || o->no != NULL) && !ComandoEh(o, "ast")) return "--sem-pos e --no so valem no comando ast";
    if (o->canonico && !ComandoEh(o, "check")) return "--canonico so vale no comando check";
    if ((o->projeto && !ComandoEh(o, "ast")) || o->plano || o->tudo || o->jobs != NULL) return ValidarProjeto(o);
    erro = ValidarArgsPrograma(o);
    if (erro != NULL) return erro;
    erro = ValidarLimites(o);
    if (erro != NULL) return erro;
    erro = ValidarTraco(o);
    if (erro != NULL) return erro;
    erro = ValidarRastreio(o);
    if (erro != NULL) return erro;
    erro = ValidarPie(o);
    if (erro != NULL) return erro;
    erro = ValidarGuarda(o);
    if (erro != NULL) return erro;
    erro = ValidarPerfil(o);
    if (erro != NULL) return erro;
    erro = ValidarSimbolos(o);
    if (erro != NULL) return erro;
    erro = ValidarChecagem(o);
    if (erro != NULL) return erro;
    erro = ValidarSemOtim(o);
    if (erro != NULL) return erro;
    erro = ValidarResumo(o);
    if (erro != NULL) return erro;
    erro = ValidarFiltro(o);
    if (erro != NULL) return erro;
    erro = ValidarStdin(o);
    if (erro != NULL) return erro;
    erro = ValidarEntradas(o);
    if (erro != NULL) return erro;
    erro = ValidarMaxErros(o);
    if (erro != NULL) return erro;
    erro = ValidarSuporte(o);
    if (erro != NULL) return erro;
    erro = ValidarDebugCode(o);
    if (erro != NULL) return erro;
    erro = ValidarBackendAlvo(o);
    if (erro != NULL) return erro;
    if (ComandoEh(o, "explicar")) return ValidarExplicar(o);
    if (ComandoEh(o, "build")) return ValidarBuild(o);
    if (ComandoEh(o, "test") || ComandoEh(o, "run")) return ValidarTestRun(o);
    return ValidarCheckAst(o);
}

/* Nome do alvo sem ':' e '/' (vira so o nome do arquivo virtual): std:text/parse -> std_text_parse. */
static void NomeDoAlvoSaneado(const char *alvo, char *saida, size_t n)
{
    snprintf(saida, n, "%s", alvo);
    for (char *p = saida; *p != '\0'; p++) {
        if (*p == ':' || *p == '/') *p = '_';
    }
}

/* forja test <alvo> (S4 P2): raiz sintetica em memoria (import "<alvo>" as alvo_teste + proc main) e caminho virtual <pasta>\<prefixo>_<nome>.teste.fj.
 * entrada passa a apontar para o caminho virtual (o exe temporario sai do nome dele); alvo_teste guarda o texto do import. NULL = ok ou nao e alvo. */
const char *OpcoesPrepararAlvoDoTeste(Opcoes *o)
{
    char pasta[256], nome[256];
    int n;

    if (!ComandoEh(o, "test") || o->entrada == NULL || !EntradaEhAlvo(o->entrada)) return NULL;
    if (_getcwd(pasta, sizeof pasta) == NULL) return "forja test std:<modulo>: nao consegui ler a pasta atual";
    n = snprintf(o->raiz_alvo, sizeof o->raiz_alvo, "import \"%s\" as alvo_teste\nproc main() {\n}\n", o->entrada);
    if (n < 0 || (size_t)n >= sizeof o->raiz_alvo) return "nome do modulo longo demais para forja test";
    NomeDoAlvoSaneado(o->entrada, nome, sizeof nome);
    n = snprintf(o->caminho_alvo, sizeof o->caminho_alvo, "%s\\%s.teste.fj", pasta, nome);
    if (n < 0 || (size_t)n >= sizeof o->caminho_alvo) return "caminho da pasta atual longo demais para forja test";
    o->alvo_teste = o->entrada;
    o->entrada = o->caminho_alvo;
    return NULL;
}
