/* filho_win.c - o programa do usuario no forja run/test: pipes, Job Object e limites (ver filho_win.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b10: primeira versao. O job so existe com limite (KILL_ON_JOB_CLOSE e teto duro de memoria de 2x o limite + 64 MiB, so para um estouro entre duas leituras
 *            nao devorar a maquina; o veredito e sempre o pico lido, nunca a falha de alocacao do programa).
 * Verificacao: construir.bat && python -I ..\tests\ia_first\verificar_limites_b10.py --forja .\forja.exe
 * Texto ANSI (CreateProcessA): o resto do forja ja usa a pagina de codigo ativa nos caminhos que vem do argv.
 */
#include "driver/filho_win.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "common/buf.h"
#include "driver/processo_win.h"
#include "driver/rcode_runtime.h"

#define PIPE_TAM 65536
#define LEITURA_TAM 16384
#define PASSO_MEMORIA_MS 5
#define PASSO_TEMPO_MAX_MS 1000
#define ESPERA_LEITORES_MS 2000
#define TETO_FOLGA_BYTES (64ull << 20)
#define CANAL_STDOUT 0
#define CANAL_STDERR 1
#define CANAIS 2

/* Quanto da saida ainda pode sair. Os dois leitores dividem o mesmo orcamento (stdout + stderr). */
typedef struct {
    CRITICAL_SECTION trava;
    uint64_t limite, usado, descartados;
    bool estourou;
    HANDLE evento;                  /* ligado no 1o byte descartado */
} Orcamento;

typedef struct {
    HANDLE leitura, destino, thread;
    Orcamento *orcamento;
    bool filtrar;
} Canal;

typedef struct {
    Orcamento orcamento;
    Canal canal[CANAIS];
    HANDLE job;
    PROCESS_INFORMATION pi;
    LARGE_INTEGER inicio, frequencia;
} Execucao;

/* ---------- orcamento de saida e leitores ---------- */

/* Quantos dos 'n' bytes lidos ainda cabem no limite; o resto e contado como descartado. */
static size_t Reservar(Orcamento *o, size_t n)
{
    uint64_t livre;
    size_t cabe;

    EnterCriticalSection(&o->trava);
    livre = o->limite > o->usado ? o->limite - o->usado : 0;
    cabe = n <= livre ? n : (size_t)livre;
    o->usado += cabe;
    o->descartados += n - cabe;
    if (cabe < n && !o->estourou) {
        o->estourou = true;
        SetEvent(o->evento);
    }
    LeaveCriticalSection(&o->trava);
    return cabe;
}

static void Emitir(const Canal *c, const char *dados, size_t n)
{
    DWORD escrito;

    if (c->destino == NULL || c->destino == INVALID_HANDLE_VALUE) return;
    while (n > 0 && WriteFile(c->destino, dados, (DWORD)n, &escrito, NULL) && escrito > 0) {
        dados += escrito;
        n -= escrito;
    }
}

static DWORD WINAPI LerCanal(LPVOID argumento)
{
    Canal *c = argumento;
    char dados[LEITURA_TAM];
    RcodeFiltro filtro;
    Buf saida = {0};
    DWORD lidos;
    size_t cabe;

    RcodeFiltroIniciar(&filtro);
    while (ReadFile(c->leitura, dados, sizeof dados, &lidos, NULL) && lidos > 0) {
        cabe = Reservar(c->orcamento, lidos);
        if (cabe == 0) continue;
        if (!c->filtrar) {
            Emitir(c, dados, cabe);
            continue;
        }
        saida.tam = 0;
        RcodeFiltroAlimentar(&filtro, dados, cabe, &saida);
        Emitir(c, saida.dados, saida.tam);
    }
    if (c->filtrar) {
        saida.tam = 0;
        RcodeFiltroFechar(&filtro, &saida);
        Emitir(c, saida.dados, saida.tam);
    }
    BufLiberar(&saida);
    return 0;
}

/* Pipe do filho para o forja: a ponta de leitura fica so com o forja, a de escrita (herdavel) vai ao filho. */
static bool CriarCanal(Canal *c, HANDLE *escrita, DWORD destino_padrao, bool filtrar)
{
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};

    if (!CreatePipe(&c->leitura, escrita, &sa, PIPE_TAM)) return false;
    SetHandleInformation(c->leitura, HANDLE_FLAG_INHERIT, 0);
    c->destino = GetStdHandle(destino_padrao);
    c->filtrar = filtrar;
    return true;
}

/* Copia herdavel de um handle padrao do forja (stdin, ou stdout sem limite de saida); sem handle ou sem copia, o NUL. */
static HANDLE HerdavelDoPadrao(DWORD id, bool escrita)
{
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE original = GetStdHandle(id), copia = NULL;

    if (original != NULL && original != INVALID_HANDLE_VALUE &&
        DuplicateHandle(GetCurrentProcess(), original, GetCurrentProcess(), &copia, 0, TRUE, DUPLICATE_SAME_ACCESS))
        return copia;
    return CreateFileA("NUL", escrita ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

/* ---------- job e processo ---------- */

static HANDLE CriarJob(const FilhoPedido *p)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {0};
    HANDLE job = CreateJobObjectA(NULL, NULL);

    if (job == NULL) return NULL;
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (p->limite_mem > 0) {
        info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
        info.JobMemoryLimit = (SIZE_T)(p->limite_mem * 2 + TETO_FOLGA_BYTES);
    }
    if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof info)) return job;
    CloseHandle(job);
    return NULL;
}

/* Lista de atributos com HANDLE_LIST: so os 3 handles padrao passam para o filho. */
static LPPROC_THREAD_ATTRIBUTE_LIST ListaDeHandles(HANDLE *h)
{
    SIZE_T tam = 0;
    LPPROC_THREAD_ATTRIBUTE_LIST lista;

    InitializeProcThreadAttributeList(NULL, 1, 0, &tam);
    lista = malloc(tam);
    if (lista == NULL) return NULL;
    if (!InitializeProcThreadAttributeList(lista, 1, 0, &tam)) {
        free(lista);
        return NULL;
    }
    if (UpdateProcThreadAttribute(lista, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, h, 3 * sizeof *h, NULL, NULL)) return lista;
    DeleteProcThreadAttributeList(lista);
    free(lista);
    return NULL;
}

/* CreateProcess suspenso com stdin, stdout e stderr nos handles dados (h[0..2]). */
static bool CriarSuspenso(const FilhoPedido *p, HANDLE *h, PROCESS_INFORMATION *pi)
{
    STARTUPINFOEXA si = {0};
    ProcessoPedido pedido = {.exe = p->exe, .args = p->args};
    char *linha = malloc(PROCESSO_LINHA_MAX);
    bool ok = false;

    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = h[0];
    si.StartupInfo.hStdOutput = h[1];
    si.StartupInfo.hStdError = h[2];
    si.lpAttributeList = ListaDeHandles(h);
    if (linha != NULL && si.lpAttributeList != NULL && ProcessoMontarLinha(&pedido, linha))
        ok = CreateProcessA(p->exe, linha, NULL, NULL, TRUE, CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, pi) != 0;
    if (!ok && GetLastError() == 0) SetLastError(ERROR_INVALID_PARAMETER);
    if (si.lpAttributeList != NULL) {
        DeleteProcThreadAttributeList(si.lpAttributeList);
        free(si.lpAttributeList);
    }
    free(linha);
    return ok;
}

/* Prende o filho suspenso ao job (se ha) e o solta. Falha = mata o filho. */
static bool PrenderESoltar(Execucao *e)
{
    bool ok = e->job == NULL || AssignProcessToJobObject(e->job, e->pi.hProcess);

    ok = ok && ResumeThread(e->pi.hThread) != (DWORD)-1;
    if (!ok) {
        DWORD erro = GetLastError();

        TerminateProcess(e->pi.hProcess, 1);
        SetLastError(erro);
    }
    QueryPerformanceCounter(&e->inicio);
    return ok;
}

/* ---------- espera ---------- */

static double SegundosDesde(const Execucao *e)
{
    LARGE_INTEGER agora;

    QueryPerformanceCounter(&agora);
    return (double)(agora.QuadPart - e->inicio.QuadPart) / (double)e->frequencia.QuadPart;
}

static uint64_t PicoDeMemoria(HANDLE job)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {0};

    if (job == NULL || !QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof info, NULL)) return 0;
    return (uint64_t)info.PeakJobMemoryUsed;
}

/* Quanto esperar ate a proxima conferencia: 5 ms com limite de memoria; sem ele, o que falta do tempo (no maximo 1 s); sem limite nenhum, sem prazo. */
static DWORD ProximoPasso(const Execucao *e, const FilhoPedido *p)
{
    double falta;

    if (p->limite_mem > 0) return PASSO_MEMORIA_MS;
    if (p->limite_tempo_s <= 0) return INFINITE;
    falta = (p->limite_tempo_s - SegundosDesde(e)) * 1000.0;
    if (falta < 1) return 1;
    return falta > PASSO_TEMPO_MAX_MS ? PASSO_TEMPO_MAX_MS : (DWORD)falta + 1;
}

/* Espera o filho terminar ou um limite estourar (o de saida chega pelo evento do orcamento). */
static FilhoFim Esperar(const Execucao *e, const FilhoPedido *p)
{
    HANDLE h[2] = {e->pi.hProcess, e->orcamento.evento};
    DWORD r;

    for (;;) {
        r = WaitForMultipleObjects(2, h, FALSE, ProximoPasso(e, p));
        if (r == WAIT_OBJECT_0 + 1) return FILHO_ESTOUROU_SAIDA;
        if (r != WAIT_TIMEOUT) return FILHO_TERMINOU;
        if (p->limite_tempo_s > 0 && SegundosDesde(e) >= p->limite_tempo_s) return FILHO_ESTOUROU_TEMPO;
        if (p->limite_mem > 0 && PicoDeMemoria(e->job) > p->limite_mem) return FILHO_ESTOUROU_MEMORIA;
    }
}

/* Espera os leitores acabarem (o EOF do pipe vem quando o ultimo dono da ponta de escrita morre); quem segurar a ponta alem do prazo tem a leitura cancelada. */
static void JuntarLeitores(Execucao *e)
{
    HANDLE t[CANAIS];
    DWORD n = 0;

    for (int k = 0; k < CANAIS; k++) {
        if (e->canal[k].thread != NULL) t[n++] = e->canal[k].thread;
    }
    if (n == 0) return;
    if (WaitForMultipleObjects(n, t, TRUE, ESPERA_LEITORES_MS) == WAIT_TIMEOUT) {
        for (DWORD k = 0; k < n; k++) CancelSynchronousIo(t[k]);
        WaitForMultipleObjects(n, t, TRUE, ESPERA_LEITORES_MS);
    }
    for (DWORD k = 0; k < n; k++) CloseHandle(t[k]);
}

/* ---------- orquestracao ---------- */

static void FecharSeAberto(HANDLE *h)
{
    if (*h != NULL && *h != INVALID_HANDLE_VALUE) CloseHandle(*h);
    *h = NULL;
}

static void IniciarExecucao(Execucao *e, const FilhoPedido *p)
{
    memset(e, 0, sizeof *e);
    InitializeCriticalSection(&e->orcamento.trava);
    e->orcamento.evento = CreateEventA(NULL, TRUE, FALSE, NULL);
    e->orcamento.limite = p->limite_saida > 0 ? p->limite_saida : UINT64_MAX;
    QueryPerformanceFrequency(&e->frequencia);
}

/* Os handles padrao do filho: stdin herdado, stderr no pipe (filtrado) e stdout no pipe so se ha limite de saida. Os da ponta do filho ficam em h[]. */
static bool PrepararCanais(Execucao *e, const FilhoPedido *p, HANDLE *h)
{
    h[0] = HerdavelDoPadrao(STD_INPUT_HANDLE, false);
    if (!CriarCanal(&e->canal[CANAL_STDERR], &h[2], STD_ERROR_HANDLE, p->enriquecer)) return false;
    e->canal[CANAL_STDERR].orcamento = &e->orcamento;
    if (p->limite_saida == 0) {
        h[1] = HerdavelDoPadrao(STD_OUTPUT_HANDLE, true);
        return true;
    }
    if (!CriarCanal(&e->canal[CANAL_STDOUT], &h[1], STD_OUTPUT_HANDLE, false)) return false;
    e->canal[CANAL_STDOUT].orcamento = &e->orcamento;
    return true;
}

static void IniciarLeitores(Execucao *e)
{
    for (int k = 0; k < CANAIS; k++) {
        if (e->canal[k].leitura != NULL) e->canal[k].thread = CreateThread(NULL, 0, LerCanal, &e->canal[k], 0, NULL);
    }
}

/* Cria o job (com limite), o filho suspenso e solta os leitores. false = nada ficou rodando. */
static bool Disparar(Execucao *e, const FilhoPedido *p, HANDLE *h)
{
    bool com_limite = p->limite_tempo_s > 0 || p->limite_mem > 0 || p->limite_saida > 0;

    if (com_limite) {
        e->job = CriarJob(p);
        if (e->job == NULL) return false;
    }
    if (!CriarSuspenso(p, h, &e->pi)) return false;
    for (int k = 0; k < 3; k++) FecharSeAberto(&h[k]); /* so o filho fica com as pontas dele: o EOF dos pipes depende disso */
    if (!PrenderESoltar(e)) return false;
    IniciarLeitores(e);
    return true;
}

static void Concluir(Execucao *e, const FilhoPedido *p, FilhoFim fim, FilhoResultado *r)
{
    DWORD codigo = 0;

    if (e->job != NULL) TerminateJobObject(e->job, 1);  /* estouro, ou sobra de neto: nada fica vivo segurando o pipe */
    else if (fim != FILHO_TERMINOU) TerminateProcess(e->pi.hProcess, 1);
    WaitForSingleObject(e->pi.hProcess, INFINITE);
    GetExitCodeProcess(e->pi.hProcess, &codigo);
    JuntarLeitores(e);
    r->fim = fim;
    r->codigo = codigo;
    r->pico_mem = p->limite_mem > 0 ? PicoDeMemoria(e->job) : 0;
    r->descartados = e->orcamento.descartados;
    if (fim != FILHO_TERMINOU) return;
    if (e->orcamento.estourou) r->fim = FILHO_ESTOUROU_SAIDA;
    else if (p->limite_mem > 0 && r->pico_mem > p->limite_mem) r->fim = FILHO_ESTOUROU_MEMORIA;
}

static void Liberar(Execucao *e)
{
    FecharSeAberto(&e->pi.hProcess);
    FecharSeAberto(&e->pi.hThread);
    FecharSeAberto(&e->job);
    FecharSeAberto(&e->orcamento.evento);
    for (int k = 0; k < CANAIS; k++) {
        FecharSeAberto(&e->canal[k].leitura);
    }
    DeleteCriticalSection(&e->orcamento.trava);
}

bool FilhoExecutar(const FilhoPedido *pedido, FilhoResultado *resultado, uint32_t *erro)
{
    Execucao e;
    HANDLE h[3] = {NULL, NULL, NULL};
    bool ok;

    IniciarExecucao(&e, pedido);
    ok = PrepararCanais(&e, pedido, h) && Disparar(&e, pedido, h);
    *erro = ok ? 0 : (uint32_t)GetLastError();
    for (int k = 0; k < 3; k++) FecharSeAberto(&h[k]);
    if (ok) Concluir(&e, pedido, Esperar(&e, pedido), resultado);
    Liberar(&e);
    return ok;
}
