/*
 * forja_rt_proc.h - std:proc no backend C: argumentos, variavel de ambiente e saida com codigo (M5 P26b). Texto ASCII puro.
 *
 * Versao: 0.8.0
 * Changelog:
 *   0.8.0 - D-G3-L2a (bloco A): forja_os_mem_available, forja_os_pid_alive, forja_os_last_error, forja_os_error_text, forja_os_write_stdout e forja_os_flush_stdout no ramo Windows (mesmos nomes do rt_os_win.c); POSIX pendente, como no K1.
 *   0.7.0 - M8 K1 (D-M8-8): forja_os_pid e forja_os_cpu_count no ramo Windows (GetCurrentProcessId, GetActiveProcessorCount); POSIX pendente (stub no stubs_posix_a52.h se o oraculo precisar).
 *   0.6.0 - M5 P45: forja_proc_multithread_ligar (liga forja_rt_multithread de forja_rt.h, as travas do oraculo; chamar ANTES de criar thread; no POSIX a trava segue nula), o mesmo nome do runtime nativo (rt_os_win.c).
 *   0.5.0 - M5 P28b-2b: forja_proc_escrever_erro (stderr cru: WriteFile no handle de erro / write 2) e forja_proc_pico_memoria (Windows K32GetProcessMemoryInfo, PeakWorkingSetSize; POSIX getrusage ru_maxrss * 1024), os mesmos nomes do runtime nativo.
 *   0.4.0 - A52: forja_proc_pasta_atual, forja_proc_caminho_absoluto, forja_proc_caminho_exe, forja_proc_pasta_temp, forja_proc_definir_ambiente e forja_proc_bytes_aleatorios (so Windows: GetCurrentDirectoryW, GetFullPathNameW, GetModuleFileNameW, GetTempPathW, SetEnvironmentVariableW e SystemFunction036 da advapi32), mesmos nomes e contrato do rt_os_win.c.
 *   0.3.0 - IA1-LT: forja_proc_hora_ns (hora de parede em ns desde 1970 UTC: GetSystemTimePreciseAsFileTime / clock_gettime CLOCK_REALTIME), o mesmo nome do runtime nativo.
 *   0.2.0 - IA1-L6: forja_proc_dormir_ms (Sleep / nanosleep) e forja_proc_tempo_ns (QueryPerformanceCounter / CLOCK_MONOTONIC), os mesmos nomes do runtime nativo (rt_os_win.c / rt_os_linux.c).
 *   0.1.0 - M5 P26b: forja_proc_iniciar, forja_proc_argumentos, forja_proc_ambiente e forja_proc_sair, com as mesmas assinaturas e o mesmo comportamento do runtime nativo
 *           (rt_proc.c). Windows: GetCommandLineW + CommandLineToArgvW (o oraculo do parser proprio do nativo) + WideCharToMultiByte em UTF-8; ambiente por
 *           GetEnvironmentVariableW. Outros sistemas: argv do main e getenv.
 *
 * Entra no .c gerado SO quando o programa importa std:proc (emit_c_runtime.c, logo depois do forja_rt.h): programas sem o modulo nao mudam um byte.
 * Posse: argumentos e valores de ambiente sao emprestados ao programa e vivem ate o fim do processo (malloc sem free; o rastreador R0009 so ve o heap da Forja).
 * Cada forja_proc_ambiente guarda uma copia nova: chame uma vez e guarde o resultado. Codigo de saida fora de 0..255 e R0012 (exit 101).
 * Depende do forja_rt.h (forja_str, forja_falha, forja_falha_alocacao) e so de libc: stdio e stdlib.
 */
#define FORJA_R0012 "R0012"
#define FORJA_ARQ_PROC "std:proc"

static forja_str *forja_proc_lista;
static int64_t forja_proc_qtd;
static bool forja_proc_pronto;
static int forja_proc_argc_main;
static char **forja_proc_argv_main;

/* Guarda argc/argv do main (usados fora do Windows; no Windows a linha de comando vem do sistema, em UTF-16). */
static inline void forja_proc_iniciar(int argc, char **argv)
{
    forja_proc_argc_main = argc;
    forja_proc_argv_main = argv;
}

#if defined(_WIN32)
/* kernel32 e shell32 sem windows.h (colidiria com nomes do programa): o rotulo liga o nome interno ao simbolo real. */
unsigned short *forja_proc_linha_larga(void) FORJA_ROTULO_C("GetCommandLineW");
unsigned short **forja_proc_dividir_larga(const unsigned short *linha, int *quantos) FORJA_ROTULO_C("CommandLineToArgvW");
void *forja_proc_liberar_local(void *p) FORJA_ROTULO_C("LocalFree");
int forja_proc_larga_para_utf8(unsigned int pagina, unsigned long flags, const unsigned short *larga, int nlarga, char *destino, int ndestino,
                               const char *padrao, int *usou_padrao) FORJA_ROTULO_C("WideCharToMultiByte");
int forja_proc_utf8_para_larga(unsigned int pagina, unsigned long flags, const char *origem, int norigem, unsigned short *larga,
                               int nlarga) FORJA_ROTULO_C("MultiByteToWideChar");
unsigned long forja_proc_ler_ambiente(const unsigned short *nome, unsigned short *destino, unsigned long n) FORJA_ROTULO_C("GetEnvironmentVariableW");

#define FORJA_PROC_UTF8 65001u

/* Copia UTF-8 (malloc, vive ate o fim) de n unidades UTF-16; n = -1 le ate o NUL. */
static inline char *forja_proc_copia_utf8(const unsigned short *larga, int n)
{
    int tam = n == 0 ? 0 : forja_proc_larga_para_utf8(FORJA_PROC_UTF8, 0, larga, n, NULL, 0, NULL, NULL);
    char *s = (char *)malloc((size_t)(tam > 0 ? tam : 0) + 1u);

    if (s == NULL)
        forja_falha_alocacao("sem memoria para o texto do processo em bytes", (long long)tam + 1, FORJA_ARQ_PROC, 0);
    if (tam > 0)
        forja_proc_larga_para_utf8(FORJA_PROC_UTF8, 0, larga, n, s, tam, NULL, NULL);
    s[tam > 0 ? (n == -1 ? tam - 1 : tam) : 0] = '\0';
    return s;
}

static inline void forja_proc_montar(void)
{
    int n = 0;
    unsigned short **larga = forja_proc_dividir_larga(forja_proc_linha_larga(), &n);

    if (larga == NULL)
        n = 0;
    if (n > 0) {
        forja_proc_lista = (forja_str *)malloc((size_t)n * sizeof(forja_str));
        if (forja_proc_lista == NULL)
            forja_falha_alocacao("sem memoria para os argumentos do processo em bytes", (long long)n * (long long)sizeof(forja_str), FORJA_ARQ_PROC, 0);
    }
    for (int i = 0; i < n; i++) {
        char *s = forja_proc_copia_utf8(larga[i], -1);
        forja_proc_lista[i] = (forja_str){s, (int64_t)__builtin_strlen(s)};
    }
    forja_proc_qtd = n;
    if (larga != NULL)
        forja_proc_liberar_local(larga);
}

static inline const char *forja_proc_ambiente(const char *nome)
{
    unsigned short nome_larga[512];
    unsigned long qtd;
    unsigned short *valor;
    char *copia;

    if (forja_proc_utf8_para_larga(FORJA_PROC_UTF8, 0, nome, -1, nome_larga, 512) <= 0)
        return NULL;
    qtd = forja_proc_ler_ambiente(nome_larga, NULL, 0); /* com o NUL; 0 = nao existe (variavel vazia da 1) */
    if (qtd == 0)
        return NULL;
    valor = (unsigned short *)malloc((size_t)qtd * sizeof(unsigned short));
    if (valor == NULL)
        forja_falha_alocacao("sem memoria para o valor da variavel de ambiente em bytes", (long long)qtd * 2, FORJA_ARQ_PROC, 0);
    qtd = forja_proc_ler_ambiente(nome_larga, valor, qtd);
    copia = forja_proc_copia_utf8(valor, (int)qtd);
    free(valor);
    return copia;
}
#else
static inline void forja_proc_montar(void)
{
    int n = forja_proc_argc_main;

    if (n > 0) {
        forja_proc_lista = (forja_str *)malloc((size_t)n * sizeof(forja_str));
        if (forja_proc_lista == NULL)
            forja_falha_alocacao("sem memoria para os argumentos do processo em bytes", (long long)n * (long long)sizeof(forja_str), FORJA_ARQ_PROC, 0);
    }
    for (int i = 0; i < n; i++)
        forja_proc_lista[i] = (forja_str){forja_proc_argv_main[i], (int64_t)__builtin_strlen(forja_proc_argv_main[i])};
    forja_proc_qtd = n;
}

static inline const char *forja_proc_ambiente(const char *nome)
{
    const char *valor = getenv(nome);
    size_t tam;
    char *copia;

    if (valor == NULL)
        return NULL;
    tam = __builtin_strlen(valor);
    copia = (char *)malloc(tam + 1u);
    if (copia == NULL)
        forja_falha_alocacao("sem memoria para o valor da variavel de ambiente em bytes", (long long)tam + 1, FORJA_ARQ_PROC, 0);
    __builtin_memcpy(copia, valor, tam + 1u);
    return copia;
}
#endif

static const forja_str forja_proc_nenhum = {NULL, 0};

/* Vetor de forja_str dos argumentos (UTF-8) e a quantidade em *quantos; nunca NULL. */
static inline void *forja_proc_argumentos(int64_t *quantos)
{
    if (!forja_proc_pronto) {
        forja_proc_montar();
        forja_proc_pronto = true;
    }
    *quantos = forja_proc_qtd;
    return forja_proc_qtd > 0 ? (void *)forja_proc_lista : (void *)&forja_proc_nenhum;
}

/* exit(code): descarrega stdout e stderr e sai SEM rodar o atexit (nao ha relatorio R0009). */
static inline void forja_proc_sair(int64_t codigo)
{
    char causa[160];

    if (codigo < 0 || codigo > 255) {
        snprintf(causa, sizeof(causa), "exit com codigo %lld fora de 0..255", (long long)codigo);
        forja_falha(FORJA_R0012, causa, FORJA_ARQ_PROC, 0);
    }
    fflush(stdout);
    fflush(stderr);
    _Exit((int)codigo);
}

#if defined(_WIN32)
/* Sleep e o relogio de alta resolucao do kernel32, sem windows.h (o rotulo liga o nome interno ao simbolo real). */
void forja_proc_dormir_sistema(unsigned long ms) FORJA_ROTULO_C("Sleep");
int forja_proc_contador_sistema(int64_t *ticks) FORJA_ROTULO_C("QueryPerformanceCounter");
int forja_proc_frequencia_sistema(int64_t *ticks_por_s) FORJA_ROTULO_C("QueryPerformanceFrequency");
void forja_proc_hora_sistema(uint64_t *filetime) FORJA_ROTULO_C("GetSystemTimePreciseAsFileTime");

/* FILETIME: intervalos de 100 ns desde 1601-01-01; este e o valor em 1970-01-01. */
#define FORJA_PROC_FILETIME_EM_1970 116444736000000000ULL

/* Dorme ms milissegundos (<= 0 volta na hora), em fatias de 2^31-1 ms. */
static inline void forja_proc_dormir_ms(int64_t ms)
{
    while (ms > 0) {
        int64_t fatia = ms > 0x7FFFFFFFLL ? 0x7FFFFFFFLL : ms;

        forja_proc_dormir_sistema((unsigned long)fatia);
        ms -= fatia;
    }
}

/* Relogio monotonico em ns (contador / frequencia: segundos inteiros mais resto, para nao estourar o i64). */
static inline int64_t forja_proc_tempo_ns(void)
{
    int64_t contador = 0;
    int64_t frequencia = 1;

    forja_proc_contador_sistema(&contador);
    forja_proc_frequencia_sistema(&frequencia);
    return (contador / frequencia) * 1000000000LL + (contador % frequencia) * 1000000000LL / frequencia;
}

/* Hora de parede em ns desde 1970-01-01 UTC (nao monotonica). */
static inline int64_t forja_proc_hora_ns(void)
{
    uint64_t filetime = 0;

    forja_proc_hora_sistema(&filetime);
    return (int64_t)(filetime - FORJA_PROC_FILETIME_EM_1970) * 100LL;
}

/* A52: pasta atual, caminho absoluto, exe, pasta temporaria, ambiente e bytes aleatorios. Contrato (igual ao rt_os_win.c): retorno >= 0 = bytes UTF-8 necessarios, sem o NUL, e so grava
 * em destino quando cabem com o NUL (n > retorno); retorno < 0 = -codigo (1 not_found, 2 no_permission, 3 other). */
unsigned long forja_proc_k_pasta_atual(unsigned long n, unsigned short *destino) FORJA_ROTULO_C("GetCurrentDirectoryW");
unsigned long forja_proc_k_caminho_absoluto(const unsigned short *nome, unsigned long n, unsigned short *destino, unsigned short **parte)
    FORJA_ROTULO_C("GetFullPathNameW");
unsigned long forja_proc_k_modulo(void *modulo, unsigned short *destino, unsigned long n) FORJA_ROTULO_C("GetModuleFileNameW");
unsigned long forja_proc_k_pasta_temp(unsigned long n, unsigned short *destino) FORJA_ROTULO_C("GetTempPathW");
int forja_proc_k_definir_ambiente(const unsigned short *nome, const unsigned short *valor) FORJA_ROTULO_C("SetEnvironmentVariableW");
unsigned long forja_proc_k_ultimo_erro(void) FORJA_ROTULO_C("GetLastError");
/* advapi32: RtlGenRandom. O gcc do mingw liga a advapi32 por padrao. */
unsigned char forja_proc_k_aleatorio(void *destino, unsigned long n) FORJA_ROTULO_C("SystemFunction036");

#define FORJA_PROC_OS_OUTRO 3
#define FORJA_PROC_MB_INVALIDO 8u
#define FORJA_PROC_ERRO_VARIAVEL_NAO_ACHADA 203u
#define FORJA_PROC_TETO_CAMINHO 32768u
#define FORJA_PROC_FATIA_ALEATORIA 0x7FFFFFFFLL

/* UTF-8 (NUL no fim) -> UTF-16 (malloc); NULL se o texto e invalido ou falta memoria. */
static inline unsigned short *forja_proc_para_larga(const char *texto)
{
    int n = forja_proc_utf8_para_larga(FORJA_PROC_UTF8, FORJA_PROC_MB_INVALIDO, texto, -1, NULL, 0);
    unsigned short *larga;

    if (n <= 0)
        return NULL;
    larga = (unsigned short *)malloc((size_t)n * sizeof(unsigned short));
    if (larga != NULL && forja_proc_utf8_para_larga(FORJA_PROC_UTF8, FORJA_PROC_MB_INVALIDO, texto, -1, larga, n) <= 0) {
        free(larga);
        larga = NULL;
    }
    return larga;
}

/* qtd unidades UTF-16 (sem NUL) -> bytes UTF-8 necessarios; grava so se couber com o NUL. -3 se a conversao falha. */
static inline int64_t forja_proc_entregar_larga(const unsigned short *larga, int64_t qtd, uint8_t *destino, int64_t n)
{
    int tam = qtd == 0 ? 0 : forja_proc_larga_para_utf8(FORJA_PROC_UTF8, 0, larga, (int)qtd, NULL, 0, NULL, NULL);

    if (tam < 0 || (qtd > 0 && tam == 0))
        return -FORJA_PROC_OS_OUTRO;
    if (n > tam) {
        if (tam > 0)
            forja_proc_larga_para_utf8(FORJA_PROC_UTF8, 0, larga, (int)qtd, (char *)destino, tam, NULL, NULL);
        destino[tam] = 0;
    }
    return tam;
}

static inline int64_t forja_proc_pasta_atual(uint8_t *destino, int64_t n)
{
    unsigned long cap = forja_proc_k_pasta_atual(0, NULL); /* com o NUL */
    unsigned short *larga;
    unsigned long qtd;
    int64_t r;

    if (cap == 0)
        return -FORJA_PROC_OS_OUTRO;
    larga = (unsigned short *)malloc((size_t)cap * sizeof(unsigned short));
    if (larga == NULL)
        return -FORJA_PROC_OS_OUTRO;
    qtd = forja_proc_k_pasta_atual(cap, larga);
    r = (qtd == 0 || qtd >= cap) ? -FORJA_PROC_OS_OUTRO : forja_proc_entregar_larga(larga, qtd, destino, n);
    free(larga);
    return r;
}

static inline int64_t forja_proc_caminho_absoluto(const char *caminho, uint8_t *destino, int64_t n)
{
    unsigned short *entrada = forja_proc_para_larga(caminho);
    unsigned short *larga;
    unsigned long cap;
    unsigned long qtd;
    int64_t r;

    if (entrada == NULL)
        return -FORJA_PROC_OS_OUTRO;
    cap = forja_proc_k_caminho_absoluto(entrada, 0, NULL, NULL); /* com o NUL */
    larga = cap == 0 ? NULL : (unsigned short *)malloc((size_t)cap * sizeof(unsigned short));
    if (larga == NULL) {
        free(entrada);
        return -FORJA_PROC_OS_OUTRO;
    }
    qtd = forja_proc_k_caminho_absoluto(entrada, cap, larga, NULL);
    r = (qtd == 0 || qtd >= cap) ? -FORJA_PROC_OS_OUTRO : forja_proc_entregar_larga(larga, qtd, destino, n);
    free(larga);
    free(entrada);
    return r;
}

/* GetModuleFileNameW devolve o tamanho do buffer quando trunca: dobra ate caber (teto 32768). */
static inline int64_t forja_proc_caminho_exe(uint8_t *destino, int64_t n)
{
    unsigned long cap = 512;
    int64_t r = -FORJA_PROC_OS_OUTRO;

    while (cap <= FORJA_PROC_TETO_CAMINHO) {
        unsigned short *larga = (unsigned short *)malloc((size_t)cap * sizeof(unsigned short));
        unsigned long qtd;

        if (larga == NULL)
            return -FORJA_PROC_OS_OUTRO;
        qtd = forja_proc_k_modulo(NULL, larga, cap);
        if (qtd > 0 && qtd < cap)
            r = forja_proc_entregar_larga(larga, qtd, destino, n);
        free(larga);
        if (qtd == 0 || qtd < cap)
            return r;
        cap *= 2;
    }
    return -FORJA_PROC_OS_OUTRO;
}

/* Sem a barra final, exceto na raiz (C:\). */
static inline int64_t forja_proc_pasta_temp(uint8_t *destino, int64_t n)
{
    unsigned long cap = forja_proc_k_pasta_temp(0, NULL); /* com o NUL */
    unsigned short *larga;
    unsigned long qtd;
    int64_t r;

    if (cap == 0)
        return -FORJA_PROC_OS_OUTRO;
    larga = (unsigned short *)malloc((size_t)cap * sizeof(unsigned short));
    if (larga == NULL)
        return -FORJA_PROC_OS_OUTRO;
    qtd = forja_proc_k_pasta_temp(cap, larga);
    if (qtd > 3 && qtd < cap && (larga[qtd - 1] == '\\' || larga[qtd - 1] == '/'))
        qtd--;
    r = (qtd == 0 || qtd >= cap) ? -FORJA_PROC_OS_OUTRO : forja_proc_entregar_larga(larga, qtd, destino, n);
    free(larga);
    return r;
}

/* valor NULL remove a variavel; remover a que nao existe e sucesso. 0 = ok, senao o codigo. */
static inline int32_t forja_proc_definir_ambiente(const char *nome, const char *valor)
{
    unsigned short *nome_larga = forja_proc_para_larga(nome);
    unsigned short *valor_larga = valor == NULL ? NULL : forja_proc_para_larga(valor);
    int32_t r = FORJA_PROC_OS_OUTRO;

    if (nome_larga != NULL && (valor == NULL || valor_larga != NULL)) {
        if (forja_proc_k_definir_ambiente(nome_larga, valor_larga) != 0)
            r = 0;
        else if (valor == NULL && forja_proc_k_ultimo_erro() == FORJA_PROC_ERRO_VARIAVEL_NAO_ACHADA)
            r = 0;
    }
    free(nome_larga);
    free(valor_larga);
    return r;
}

/* Enche destino com n bytes do gerador do sistema, em fatias de 0x7FFFFFFF. 0 = ok. */
static inline int32_t forja_proc_bytes_aleatorios(uint8_t *destino, int64_t n)
{
    while (n > 0) {
        int64_t fatia = n > FORJA_PROC_FATIA_ALEATORIA ? FORJA_PROC_FATIA_ALEATORIA : n;

        if (forja_proc_k_aleatorio(destino, (unsigned long)fatia) == 0)
            return FORJA_PROC_OS_OUTRO;
        destino += fatia;
        n -= fatia;
    }
    return 0;
}

/* P28b-2b: stderr cru (WriteFile no handle de erro, sem a traducao de LF do stdio) e pico de memoria (K32GetProcessMemoryInfo do kernel32, do proprio processo: -1 = pseudo handle). */
void *forja_proc_k_handle_padrao(unsigned long nivel) FORJA_ROTULO_C("GetStdHandle");
int forja_proc_k_escrever(void *h, const void *buf, unsigned long n, unsigned long *escritos, void *sobreposto) FORJA_ROTULO_C("WriteFile");
int forja_proc_k_memoria(void *processo, void *contadores, unsigned long cb) FORJA_ROTULO_C("K32GetProcessMemoryInfo");
unsigned long forja_proc_k_pid(void) FORJA_ROTULO_C("GetCurrentProcessId");
unsigned long forja_proc_k_processadores(unsigned short grupo) FORJA_ROTULO_C("GetActiveProcessorCount");
int forja_proc_k_memoria_global(void *estado) FORJA_ROTULO_C("GlobalMemoryStatusEx");
void *forja_proc_k_abrir_processo(unsigned long acesso, int herda, unsigned long pid) FORJA_ROTULO_C("OpenProcess");
int forja_proc_k_fechar(void *h) FORJA_ROTULO_C("CloseHandle");
unsigned long forja_proc_k_esperar(void *h, unsigned long ms) FORJA_ROTULO_C("WaitForSingleObject");
unsigned long forja_proc_k_mensagem(unsigned long flags, const void *fonte, unsigned long codigo, unsigned long idioma, unsigned short *destino,
                                    unsigned long n, void *argumentos) FORJA_ROTULO_C("FormatMessageW");

#define FORJA_PROC_NIVEL_ERRO 0xFFFFFFF4UL /* STD_ERROR_HANDLE (-12) */
#define FORJA_PROC_LOTE_ESCRITA 0x40000000LL

/* PROCESS_MEMORY_COUNTERS (72 bytes no x64): PeakWorkingSetSize e o 1o campo depois de cb e PageFaultCount. */
typedef struct {
    uint32_t cb;
    uint32_t faltas_de_pagina;
    uint64_t pico_conjunto_de_trabalho;
    uint64_t resto[7];
} ForjaProcMemoria;

/* Falha de WriteFile (fd fechado, pipe quebrado) para sem erro, como o stdio. */
static inline void forja_proc_escrever_erro(const uint8_t *dados, int64_t n)
{
    void *h = forja_proc_k_handle_padrao(FORJA_PROC_NIVEL_ERRO);

    while (n > 0) {
        unsigned long pedido = n > FORJA_PROC_LOTE_ESCRITA ? (unsigned long)FORJA_PROC_LOTE_ESCRITA : (unsigned long)n;
        unsigned long escritos = 0;

        if (!forja_proc_k_escrever(h, dados, pedido, &escritos, NULL) || escritos == 0)
            return;
        dados += escritos;
        n -= (int64_t)escritos;
    }
}

/* >= 0 = pico em bytes; < 0 = -codigo (3 other). */
static inline int64_t forja_proc_pico_memoria(void)
{
    ForjaProcMemoria m = {(uint32_t)sizeof(ForjaProcMemoria), 0, 0, {0}};

    if (forja_proc_k_memoria((void *)(intptr_t)-1, &m, m.cb) == 0)
        return -FORJA_PROC_OS_OUTRO;
    return (int64_t)m.pico_conjunto_de_trabalho;
}

/* M8 K1: pid e processadores (mesmos nomes do rt_os_win.c). So Windows; o oraculo POSIX ainda nao tem estes dois. */
static inline int64_t forja_os_pid(void)
{
    return (int64_t)forja_proc_k_pid();
}

static inline int64_t forja_os_cpu_count(void)
{
    unsigned long qtd = forja_proc_k_processadores(0xFFFFU);

    return qtd == 0 ? 1 : (int64_t)qtd;
}

/* D-G3-L2a (bloco A): memoria livre, viveza de pid, erro do sistema e stdout cru. MEMORYSTATUSEX de 64 bytes, como em rt_os_win.c. */
typedef struct {
    unsigned long tamanho;
    unsigned long carga;
    uint64_t total;
    uint64_t livre;
    uint64_t resto[5];
} ForjaProcMemoriaGlobal;

#define FORJA_PROC_NIVEL_SAIDA 0xFFFFFFF5UL /* STD_OUTPUT_HANDLE (-11) */

static inline int64_t forja_os_mem_available(void)
{
    ForjaProcMemoriaGlobal m = {0};

    m.tamanho = (unsigned long)sizeof(ForjaProcMemoriaGlobal);
    if (forja_proc_k_memoria_global(&m) == 0)
        return -FORJA_PROC_OS_OUTRO;
    return (int64_t)m.livre;
}

static inline int32_t forja_os_pid_alive(int64_t pid)
{
    void *h;
    int32_t vivo;

    if (pid <= 0 || pid > 0xFFFFFFFFLL)
        return 0;
    h = forja_proc_k_abrir_processo(0x1000UL | 0x00100000UL, 0, (unsigned long)pid);
    if (h == NULL)
        return forja_proc_k_ultimo_erro() == 5UL ? 1 : 0;
    vivo = forja_proc_k_esperar(h, 0) == 0x102UL ? 1 : 0;
    forja_proc_k_fechar(h);
    return vivo;
}

static inline int64_t forja_os_last_error(void)
{
    return (int64_t)forja_proc_k_ultimo_erro();
}

/* Mesmo contrato do rt_os_win.c: ingles e, se faltar, idioma neutro; codigo sem texto = "". */
static inline int64_t forja_os_error_text(int64_t codigo, uint8_t *destino, int64_t n)
{
    unsigned short larga[1024];
    unsigned long qtd;

    if (codigo < 0 || codigo > 0xFFFFFFFFLL)
        return 0;
    qtd = forja_proc_k_mensagem(0x1200UL, NULL, (unsigned long)codigo, 0x0409UL, larga, 1024UL, NULL);
    if (qtd == 0)
        qtd = forja_proc_k_mensagem(0x1200UL, NULL, (unsigned long)codigo, 0UL, larga, 1024UL, NULL);
    while (qtd > 0 && (larga[qtd - 1] == ' ' || larga[qtd - 1] == '\r' || larga[qtd - 1] == '\n' || larga[qtd - 1] == '\t'))
        qtd--;
    if (qtd == 0)
        return 0;
    return forja_proc_entregar_larga(larga, (int64_t)qtd, destino, n);
}

/* O stdio vai primeiro (fflush) para a ordem com o print; depois WriteFile cru no handle de saida. */
static inline void forja_os_write_stdout(const uint8_t *dados, int64_t n)
{
    void *h;

    fflush(stdout);
    h = forja_proc_k_handle_padrao(FORJA_PROC_NIVEL_SAIDA);
    while (n > 0) {
        unsigned long pedido = n > FORJA_PROC_LOTE_ESCRITA ? (unsigned long)FORJA_PROC_LOTE_ESCRITA : (unsigned long)n;
        unsigned long escritos = 0;

        if (!forja_proc_k_escrever(h, dados, pedido, &escritos, NULL) || escritos == 0)
            return;
        dados += escritos;
        n -= (int64_t)escritos;
    }
}

static inline void forja_os_flush_stdout(void)
{
    fflush(stdout);
}
#else
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>

static inline void forja_proc_dormir_ms(int64_t ms)
{
    struct timespec pedido;

    if (ms <= 0)
        return;
    pedido.tv_sec = (time_t)(ms / 1000);
    pedido.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&pedido, &pedido) != 0) {
    }
}

static inline int64_t forja_proc_tempo_ns(void)
{
    struct timespec agora;

    clock_gettime(CLOCK_MONOTONIC, &agora);
    return (int64_t)agora.tv_sec * 1000000000LL + (int64_t)agora.tv_nsec;
}

/* Hora de parede em ns desde 1970-01-01 UTC (nao monotonica). */
static inline int64_t forja_proc_hora_ns(void)
{
    struct timespec agora;

    clock_gettime(CLOCK_REALTIME, &agora);
    return (int64_t)agora.tv_sec * 1000000000LL + (int64_t)agora.tv_nsec;
}

/* P28b-2b: stderr cru por write(2) e pico de memoria por getrusage (ru_maxrss em KiB). */
static inline void forja_proc_escrever_erro(const uint8_t *dados, int64_t n)
{
    while (n > 0) {
        ssize_t r = write(2, dados, (size_t)n);

        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            return;
        dados += r;
        n -= r;
    }
}

static inline int64_t forja_proc_pico_memoria(void)
{
    struct rusage uso;

    if (getrusage(RUSAGE_SELF, &uso) != 0)
        return -3;
    return (int64_t)uso.ru_maxrss * 1024;
}
#endif

static inline void forja_proc_multithread_ligar(void)
{
    forja_rt_multithread = 1;
}
