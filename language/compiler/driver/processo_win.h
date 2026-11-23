/* processo_win.h - filhos do orquestrador de projeto no Windows: CreateProcess suspenso preso a um Job Object (KILL_ON_JOB_CLOSE), saida em arquivo e espera por WaitForMultipleObjects (P22-O2 do M3).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - IA1-b10: PROCESSO_LINHA_MAX e ProcessoMontarLinha (publicos).
 *            0.2.0 - M3 F6: ProcessoFases (marcas de tempo das fases do disparo, so quando pedidas) e ProcessoTempos (criacao e saida do filho); aviso de "sem job" com a causa do sistema.
 *            0.1.0- M3 P22-O2: ProcessoJobCriar/Fechar, ProcessoDisparar, ProcessoEsperarQualquer, ProcessoConcluir e ProcessoVivo.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#ifndef FORJA_DRIVER_PROCESSO_WIN_H
#define FORJA_DRIVER_PROCESSO_WIN_H

#include <stdbool.h>
#include <stdint.h>

#define PROCESSO_ESPERA_MAX 64      /* MAXIMUM_WAIT_OBJECTS */
#define PROCESSO_LINHA_MAX 32768    /* limite do lpCommandLine do CreateProcess */

/* O job do orquestrador: os filhos que ele recebe morrem quando o ultimo handle do job fecha (o pai morreu, levou Ctrl+C ou terminou). handle e HANDLE. */
typedef struct {
    void *handle;
    bool avisou;                    /* o aviso de "sem job" ja saiu uma vez */
} ProcessoJob;

/* Um filho em andamento. handle e HANDLE do processo; erro = GetLastError() quando ProcessoDisparar falha. */
typedef struct {
    void *handle;
    uint32_t pid;
    uint32_t erro;
} Processo;

/* Marcas (QueryPerformanceCounter) do ProcessoDisparar: 0 inicio, 1 canais abertos, 2 filho criado, 3 canais fechados, 4 filho preso ao job, 5 filho retomado. */
#define PROCESSO_MARCAS 6
typedef struct { uint64_t marca[PROCESSO_MARCAS]; } ProcessoFases;

typedef struct {
    const char *exe;                  /* caminho do executavel (vira argv[0] entre aspas) */
    const char *const *args;            /* argv[1..], terminado por NULL; cada um e citado se precisar */
    const char *saida_out, *saida_err;  /* arquivos que recebem o stdout e o stderr do filho (criados/truncados); stdin = NUL */
    ProcessoJob *job;                   /* NULL ou sem handle = filho solto */
    ProcessoFases *fases;               /* NULL = nao mede; senao recebe as marcas do disparo */
} ProcessoPedido;

/* A linha de comando do filho (exe entre aspas e cada arg citado so se precisar, regra do argv do Windows) em linha[PROCESSO_LINHA_MAX]; usa so pedido->exe e pedido->args.
 * false = nao coube. */
bool ProcessoMontarLinha(const ProcessoPedido *pedido, char *linha);

/* Cria o job com KILL_ON_JOB_CLOSE. false = nao criou (job->handle fica NULL); o chamador avisa e segue sem job. */
bool ProcessoJobCriar(ProcessoJob *job);

void ProcessoJobFechar(ProcessoJob *job);

/* Dispara o filho: so os 3 handles de entrada/saida sao herdados (lista explicita), CREATE_SUSPENDED, AssignProcessToJobObject, ResumeThread. Se o Assign falhar (pai
 * dentro de um job sem aninhamento) segue sem job e avisa 1x no stderr. false = nao disparou (saida->erro). */
bool ProcessoDisparar(const ProcessoPedido *pedido, Processo *saida);

/* Espera o primeiro dos n filhos (1..64) terminar; *qual = indice na lista. false = a espera falhou. */
bool ProcessoEsperarQualquer(const Processo *lista, uint32_t n, uint32_t *qual);

/* FILETIME (100 ns) de criacao e de saida de um filho que ja terminou (GetProcessTimes); chamar ANTES de ProcessoConcluir. false = nao leu. */
bool ProcessoTempos(const Processo *p, uint64_t *criacao, uint64_t *saida);

/* Codigo de saida de um filho que ja terminou, e fecha o handle. */
int ProcessoConcluir(Processo *p);

/* O pid ainda e de um processo vivo? (pid reciclado conta como vivo: o pior caso e deixar um .tmp para a proxima faxina.) */
bool ProcessoVivo(uint32_t pid);

#endif
