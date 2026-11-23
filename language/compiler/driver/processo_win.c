/* processo_win.c - filhos do orquestrador de projeto no Windows (P22-O2 do M3): CreateProcessA suspenso, preso a um Job Object com KILL_ON_JOB_CLOSE e solto por ResumeThread.
 * Versao: 0.2.2
 * Changelog: 0.2.2 - IA1-b10: MontarLinha vira ProcessoMontarLinha (publica; o forja run/test monta a linha do programa do usuario com a mesma regra do argv).
 *            0.2.1 - M3 F9b-1: decisoes extraidas de AnexarEntreAspas.
 *            0.2.0 - M3 F6: ProcessoDisparar marca o tempo de cada fase em pedido->fases (so quando pedido; QueryPerformanceCounter), ProcessoTempos (GetProcessTimes) e o aviso de
 *            "sem job" diz a causa do sistema (erro_sistema.c).
 *            0.1.1- M3 P23z (revisor): ProcessoDisparar confere o retorno de ResumeThread (falhou = mata o filho suspenso, fecha os handles e devolve false com o erro).
 *            0.1.0 - M3 P22-O2: ProcessoJobCriar/Fechar, ProcessoDisparar (handles herdados por lista explicita: NUL, .out e .err), ProcessoEsperarQualquer, ProcessoConcluir, ProcessoVivo.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 * Texto ANSI (CreateProcessA, CreateFileA): o resto do forja ja usa a pagina de codigo ativa nos caminhos que vem do argv.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "driver/erro_sistema.h"
#include "driver/processo_win.h"

#define PROCESSO_CANAIS 3           /* stdin (NUL), stdout, stderr */

typedef struct { HANDLE h[PROCESSO_CANAIS]; } Canais;

/* ---------- linha de comando ---------- */

static bool Caractere(char *linha, size_t *n, char c)
{
    if (*n + 1 >= PROCESSO_LINHA_MAX) return false;
    linha[(*n)++] = c;
    return true;
}

static bool Repetir(char *linha, size_t *n, char c, size_t vezes)
{
    for (size_t k = 0; k < vezes; k++) {
        if (!Caractere(linha, n, c)) return false;
    }
    return true;
}

static bool Texto(char *linha, size_t *n, const char *texto)
{
    for (const char *p = texto; *p != '\0'; p++) {
        if (!Caractere(linha, n, *p)) return false;
    }
    return true;
}

/* Regra do CommandLineToArgvW: barras antes de uma aspa dobram e a aspa ganha mais uma; barras no fim (antes da aspa de fechar) dobram. */
static bool AnexarCaractereDoArgumento(char *linha, size_t *n, char c, size_t *barras)
{
    if (c == '"' && !Repetir(linha, n, '\\', *barras + 1)) return false;
    *barras = c == '\\' ? *barras + 1 : 0;
    return Caractere(linha, n, c);
}

static bool AnexarEntreAspas(char *linha, size_t *n, const char *arg)
{
    size_t barras = 0;

    if (!Caractere(linha, n, '"')) return false;
    for (const char *p = arg; *p != '\0'; p++) {
        if (!AnexarCaractereDoArgumento(linha, n, *p, &barras)) return false;
    }
    return Repetir(linha, n, '\\', barras) && Caractere(linha, n, '"');
}

static bool PrecisaDeAspas(const char *arg)
{
    return arg[0] == '\0' || strpbrk(arg, " \t\"") != NULL;
}

/* " arg" entre aspas so se precisar. */
static bool AnexarArgumento(char *linha, size_t *n, const char *arg)
{
    if (!Caractere(linha, n, ' ')) return false;
    return PrecisaDeAspas(arg) ? AnexarEntreAspas(linha, n, arg) : Texto(linha, n, arg);
}

/* argv[0] sempre entre aspas (caminho com espaco), depois os args ate o NULL. */
bool ProcessoMontarLinha(const ProcessoPedido *p, char *linha)
{
    size_t n = 0;

    if (!AnexarEntreAspas(linha, &n, p->exe)) return false;
    for (const char *const *a = p->args; *a != NULL; a++) {
        if (!AnexarArgumento(linha, &n, *a)) return false;
    }
    linha[n] = '\0';
    return true;
}

/* ---------- canais herdados ---------- */

static HANDLE AbrirHerdavel(const char *caminho, bool escrita)
{
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};

    if (escrita) return CreateFileA(caminho, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return CreateFileA(caminho, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void FecharCanais(Canais *c)
{
    for (int k = 0; k < PROCESSO_CANAIS; k++) {
        if (c->h[k] != INVALID_HANDLE_VALUE) CloseHandle(c->h[k]);
        c->h[k] = INVALID_HANDLE_VALUE;
    }
}

/* stdin = NUL, stdout e stderr = os arquivos (criados e truncados). false = erro em *erro, nada fica aberto. */
static bool AbrirCanais(const ProcessoPedido *p, Canais *c, DWORD *erro)
{
    c->h[0] = AbrirHerdavel("NUL", false);
    c->h[1] = c->h[0] != INVALID_HANDLE_VALUE ? AbrirHerdavel(p->saida_out, true) : INVALID_HANDLE_VALUE;
    c->h[2] = c->h[1] != INVALID_HANDLE_VALUE ? AbrirHerdavel(p->saida_err, true) : INVALID_HANDLE_VALUE;
    if (c->h[2] != INVALID_HANDLE_VALUE) return true;
    *erro = GetLastError();
    FecharCanais(c);
    return false;
}

/* Lista de atributos com HANDLE_LIST: so esses 3 handles passam para o filho. Os handles de 'c' tem de viver ate o CreateProcess. */
static LPPROC_THREAD_ATTRIBUTE_LIST ListaDeHandles(const Canais *c)
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
    if (UpdateProcThreadAttribute(lista, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, (PVOID)c->h, sizeof c->h, NULL, NULL)) return lista;
    DeleteProcThreadAttributeList(lista);
    free(lista);
    return NULL;
}

/* ---------- disparo ---------- */

/* O erro do Windows do passo que falhou; ERROR_INVALID_PARAMETER quando a falha foi nossa (linha de comando longa, sem memoria) e nao deixou erro. */
static DWORD ErroDaFalha(void)
{
    DWORD erro = GetLastError();

    return erro != 0 ? erro : ERROR_INVALID_PARAMETER;
}

static bool CriarSuspenso(const ProcessoPedido *p, const Canais *c, PROCESS_INFORMATION *pi, DWORD *erro)
{
    STARTUPINFOEXA si = {0};
    char *linha = malloc(PROCESSO_LINHA_MAX);
    bool ok = false;

    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = c->h[0];
    si.StartupInfo.hStdOutput = c->h[1];
    si.StartupInfo.hStdError = c->h[2];
    si.lpAttributeList = ListaDeHandles(c);
    if (linha != NULL && si.lpAttributeList != NULL && ProcessoMontarLinha(p, linha))
        ok = CreateProcessA(p->exe, linha, NULL, NULL, TRUE, CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, pi) != 0;
    *erro = ok ? 0 : ErroDaFalha();
    if (si.lpAttributeList != NULL) {
        DeleteProcThreadAttributeList(si.lpAttributeList);
        free(si.lpAttributeList);
    }
    free(linha);
    return ok;
}

static void PrenderAoJob(ProcessoJob *job, HANDLE processo)
{
    char causa[ERRO_TEXTO_MAX];

    if (job == NULL || job->handle == NULL || AssignProcessToJobObject(job->handle, processo)) return;
    if (job->avisou) return;
    job->avisou = true;
    ErroWinTexto(GetLastError(), causa, sizeof causa);
    fprintf(stderr, "forja: aviso: nao consegui prender o filho ao job object (%s); ele nao morre sozinho se o orquestrador morrer\n", causa);
}

/* Marca o instante da fase 'i' em f (QueryPerformanceCounter); f NULL = nao mede. */
static void Marcar(ProcessoFases *f, int i)
{
    LARGE_INTEGER agora;

    if (f == NULL) return;
    QueryPerformanceCounter(&agora);
    f->marca[i] = (uint64_t)agora.QuadPart;
}

/* Solta o filho que nasceu suspenso. Falha = mata o filho e fecha os handles: sem retomar, a espera por ele nunca acabaria. */
static bool Retomar(PROCESS_INFORMATION *pi, Processo *saida)
{
    if (ResumeThread(pi->hThread) != (DWORD)-1) {
        CloseHandle(pi->hThread);
        return true;
    }
    saida->erro = ErroDaFalha();
    TerminateProcess(pi->hProcess, 1);
    CloseHandle(pi->hThread);
    CloseHandle(pi->hProcess);
    return false;
}

bool ProcessoDisparar(const ProcessoPedido *pedido, Processo *saida)
{
    Canais canais;
    PROCESS_INFORMATION pi;
    DWORD erro = 0;
    bool ok;

    memset(saida, 0, sizeof *saida);
    Marcar(pedido->fases, 0);
    if (!AbrirCanais(pedido, &canais, &erro)) {
        saida->erro = erro;
        return false;
    }
    Marcar(pedido->fases, 1);
    ok = CriarSuspenso(pedido, &canais, &pi, &erro);
    Marcar(pedido->fases, 2);
    FecharCanais(&canais);
    Marcar(pedido->fases, 3);
    if (!ok) {
        saida->erro = erro;
        return false;
    }
    PrenderAoJob(pedido->job, pi.hProcess);
    Marcar(pedido->fases, 4);
    if (!Retomar(&pi, saida)) return false;
    Marcar(pedido->fases, 5);
    saida->handle = pi.hProcess;
    saida->pid = pi.dwProcessId;
    return true;
}

/* ---------- job, espera, fim ---------- */

bool ProcessoJobCriar(ProcessoJob *job)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limite = {0};

    job->avisou = false;
    job->handle = CreateJobObjectA(NULL, NULL);
    if (job->handle == NULL) return false;
    limite.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (SetInformationJobObject(job->handle, JobObjectExtendedLimitInformation, &limite, sizeof limite)) return true;
    CloseHandle(job->handle);
    job->handle = NULL;
    return false;
}

void ProcessoJobFechar(ProcessoJob *job)
{
    if (job->handle != NULL) CloseHandle(job->handle);
    job->handle = NULL;
}

bool ProcessoEsperarQualquer(const Processo *lista, uint32_t n, uint32_t *qual)
{
    HANDLE h[PROCESSO_ESPERA_MAX];
    DWORD r;

    if (n == 0 || n > PROCESSO_ESPERA_MAX) return false;
    for (uint32_t k = 0; k < n; k++) h[k] = lista[k].handle;
    r = WaitForMultipleObjects(n, h, FALSE, INFINITE);
    if (r >= WAIT_OBJECT_0 + n) return false;
    *qual = r - WAIT_OBJECT_0;
    return true;
}

static uint64_t FiletimeParaInteiro(FILETIME ft)
{
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

bool ProcessoTempos(const Processo *p, uint64_t *criacao, uint64_t *saida)
{
    FILETIME criado, saiu, nucleo, usuario;

    if (p->handle == NULL || !GetProcessTimes(p->handle, &criado, &saiu, &nucleo, &usuario)) return false;
    *criacao = FiletimeParaInteiro(criado);
    *saida = FiletimeParaInteiro(saiu);
    return true;
}

int ProcessoConcluir(Processo *p)
{
    DWORD codigo = 3;   /* SAIDA_INTERNO se nem o codigo de saida se obtem */

    if (p->handle == NULL) return (int)codigo;
    GetExitCodeProcess(p->handle, &codigo);
    CloseHandle(p->handle);
    p->handle = NULL;
    return (int)codigo;
}

bool ProcessoVivo(uint32_t pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    bool vivo;

    if (h == NULL) return GetLastError() == ERROR_ACCESS_DENIED;
    vivo = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return vivo;
}
