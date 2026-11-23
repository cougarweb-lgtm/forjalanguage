/*
 * rt_os_win.c - std:os no Windows x64: sleep_ms, time_ns e wall_time_ns (IA1-L6, L-07; IA1-LT).
 *
 * Versao: 0.10.0
 * Changelog:
 *   0.10.0 - I9-S1 (forja 0.43.0): forja_os_error_text ganha o 4o parametro local (int32): 0 = ingles (como antes, idioma neutro se faltar); != 0 = FormatMessageW com idioma 0 (idioma local da maquina, o mesmo do Rust).
 *   0.9.0 - D-G3-L2a (bloco A): forja_os_mem_available (GlobalMemoryStatusEx, ullAvailPhys), forja_os_pid_alive (OpenProcess + WaitForSingleObject(0); ACCESS_DENIED = vivo), forja_os_last_error (GetLastError), forja_os_error_text (FormatMessageW em ingles, idioma neutro se falhar), forja_os_write_stdout (rt_saida_stdout_cru) e forja_os_flush_stdout (rt_saida_descarregar); 3 imports novos no fim do ligar_tabelas.c (GlobalMemoryStatusEx, OpenProcess, FormatMessageW).
 *   0.8.0 - M8 K1 (D-M8-8): forja_os_pid (GetCurrentProcessId) e forja_os_cpu_count (GetActiveProcessorCount, todos os grupos; 0 ou erro -> 1); 73o e 74o imports do ligar_tabelas.c.
 *   0.7.0 - R2: forja_os_fuso_s (deslocamento do fuso local em segundos num instante UTC: SystemTimeToTzSpecificLocalTime de kernel32, 72o import do ligar_tabelas.c; so no exe que alcanca o simbolo).
 *   0.6.0 - M5 P45: forja_proc_multithread_ligar (liga o flag rt_multithread antes do CreateThread; std:os multithread_on).
 *   0.5.0 - M5 P28b-2b: forja_proc_escrever_erro (sys_escrever(2): WriteFile no handle de erro) e forja_proc_pico_memoria (K32GetProcessMemoryInfo de kernel32, 61o import do ligar.c: PeakWorkingSetSize); so entram no exe que os alcanca.
 *   0.4.0 - A52: forja_proc_pasta_atual, forja_proc_caminho_absoluto, forja_proc_caminho_exe, forja_proc_pasta_temp, forja_proc_definir_ambiente (GetCurrentDirectoryW, GetFullPathNameW, GetModuleFileNameW, GetTempPathW, SetEnvironmentVariableW: 56o ao 60o import de kernel32 do ligar.c) e forja_proc_bytes_aleatorios (SystemFunction036 da ADVAPI32.dll, 2a tabela do ligar.c); so entram no exe que os alcanca.
 *   0.3.0 - IA1-LT: forja_proc_hora_ns (hora de parede em ns desde 1970-01-01 UTC: GetSystemTimePreciseAsFileTime de kernel32, 28o import do ligar.c, so no exe que alcanca o simbolo).
 *   0.2.0 - IA1-LX: passa a ser incluido por rt_ryu.c (rt_win_ryu.o, so Windows) e nao mais por rt_curto.c: o rt_win_curto.o ficou neutro de alvo e liga tambem no ELF, onde o par vem de rt_os_linux.c (simbolos repetidos = X0903).
 *   0.1.0 - IA1-L6: primeira versao. forja_proc_dormir_ms (Sleep de kernel32, em fatias de 2^31-1 ms) e forja_proc_tempo_ns (o sys_tempo_ns do rt_win.o: QueryPerformanceCounter, monotonico).
 *
 * Usa sys_tempo_ns (rt_sys_win.c). Importa Sleep de kernel32 (o ligador acrescenta o __imp_Sleep so ao exe que alcanca forja_proc_dormir_ms).
 * Incluido por rt_ryu.c; nao compila sozinho.
 */
#include <stdint.h>

#define RT_OS_SLEEP_FATIA_MS 0x7FFFFFFFLL
/* FILETIME conta intervalos de 100 ns desde 1601-01-01; este e o valor em 1970-01-01. */
#define RT_OS_FILETIME_EM_1970 116444736000000000ULL
#define RT_OS_NS_POR_FILETIME 100LL

__declspec(dllimport) void Sleep(unsigned long ms);
/* O FILETIME (dois DWORD, baixo e alto) tem o mesmo layout de um uint64 little-endian. */
__declspec(dllimport) void GetSystemTimePreciseAsFileTime(uint64_t *filetime);
int64_t sys_tempo_ns(void);

/* Dorme ms milissegundos (<= 0 volta na hora). */
void forja_proc_dormir_ms(int64_t ms)
{
    while (ms > 0) {
        int64_t fatia = ms > RT_OS_SLEEP_FATIA_MS ? RT_OS_SLEEP_FATIA_MS : ms;

        Sleep((unsigned long)fatia);
        ms -= fatia;
    }
}

/* Relogio monotonico em nanossegundos desde um ponto fixo do sistema (so serve para medir diferencas). */
int64_t forja_proc_tempo_ns(void)
{
    return sys_tempo_ns();
}

/* Liga o flag rt_multithread (pegajoso; as travas do P44 passam a valer). Quem vai criar thread (foreign CreateThread; std:thread no P47) chama ANTES do CreateThread:
 * uma thread que ja esteja dentro de secao quando o flag liga segue sem lock. */
void sys_multithread_ligar(void);

void forja_proc_multithread_ligar(void)
{
    sys_multithread_ligar();
}

/* Hora de parede em nanossegundos desde 1970-01-01 UTC (NAO monotonica: o relogio do sistema pode ser acertado). */
int64_t forja_proc_hora_ns(void)
{
    uint64_t filetime = 0;

    GetSystemTimePreciseAsFileTime(&filetime);
    return ((int64_t)(filetime - RT_OS_FILETIME_EM_1970)) * RT_OS_NS_POR_FILETIME;
}

/* ---- R2: fuso local em std:time (kernel32 SystemTimeToTzSpecificLocalTime, nenhuma DLL alem de kernel32) ----
 * forja_os_fuso_s(unix_s) = deslocamento, em segundos, do fuso local do PC naquele instante UTC (negativo a oeste de Greenwich, ja com o horario de verao).
 * 0 se o instante cai fora da faixa do SYSTEMTIME (anos 1601..30827) ou o sistema recusa. */
#define RT_OS_SEG_POR_DIA 86400LL
#define RT_OS_DIAS_ERA 146097LL
#define RT_OS_DIAS_DA_ERA_A_1970 719468LL
#define RT_OS_ANOS_ERA 400LL
#define RT_OS_ANO_MIN 1601LL
#define RT_OS_ANO_MAX 30827LL

typedef struct {
    unsigned short ano, mes, dia_semana, dia, hora, minuto, segundo, milissegundo;
} RtOsSystemTime;

__declspec(dllimport) int SystemTimeToTzSpecificLocalTime(const void *fuso, const RtOsSystemTime *utc, RtOsSystemTime *local);

/* Dias desde 1970-01-01 do dia civil (Hinnant, days_from_civil). */
static int64_t rt_os_dias_de_civil(int64_t ano, int64_t mes, int64_t dia)
{
    int64_t a = mes <= 2 ? ano - 1 : ano;
    int64_t era = (a >= 0 ? a : a - (RT_OS_ANOS_ERA - 1)) / RT_OS_ANOS_ERA;
    int64_t ano_da_era = a - era * RT_OS_ANOS_ERA;
    int64_t mes_ajustado = mes > 2 ? mes - 3 : mes + 9;
    int64_t dia_do_ano = (153 * mes_ajustado + 2) / 5 + dia - 1;
    int64_t dia_da_era = ano_da_era * 365 + ano_da_era / 4 - ano_da_era / 100 + dia_do_ano;

    return era * RT_OS_DIAS_ERA + dia_da_era - RT_OS_DIAS_DA_ERA_A_1970;
}

/* Dia civil dos dias desde 1970-01-01 (Hinnant, civil_from_days). */
static void rt_os_civil_de_dias(int64_t dias, int64_t *ano, int64_t *mes, int64_t *dia)
{
    int64_t z = dias + RT_OS_DIAS_DA_ERA_A_1970;
    int64_t era = (z >= 0 ? z : z - (RT_OS_DIAS_ERA - 1)) / RT_OS_DIAS_ERA;
    int64_t dia_da_era = z - era * RT_OS_DIAS_ERA;
    int64_t ano_da_era = (dia_da_era - dia_da_era / 1460 + dia_da_era / 36524 - dia_da_era / (RT_OS_DIAS_ERA - 1)) / 365;
    int64_t dia_do_ano = dia_da_era - (365 * ano_da_era + ano_da_era / 4 - ano_da_era / 100);
    int64_t mes_ajustado = (5 * dia_do_ano + 2) / 153;

    *dia = dia_do_ano - (153 * mes_ajustado + 2) / 5 + 1;
    *mes = mes_ajustado < 10 ? mes_ajustado + 3 : mes_ajustado - 9;
    *ano = ano_da_era + era * RT_OS_ANOS_ERA + (*mes <= 2 ? 1 : 0);
}

int32_t forja_os_fuso_s(int64_t unix_s)
{
    int64_t dias = unix_s / RT_OS_SEG_POR_DIA;
    int64_t segundos_do_dia = unix_s - dias * RT_OS_SEG_POR_DIA;
    int64_t ano, mes, dia;
    RtOsSystemTime utc, local;

    if (segundos_do_dia < 0) {
        dias -= 1;
        segundos_do_dia += RT_OS_SEG_POR_DIA;
    }
    rt_os_civil_de_dias(dias, &ano, &mes, &dia);
    if (ano < RT_OS_ANO_MIN || ano > RT_OS_ANO_MAX)
        return 0;
    utc.ano = (unsigned short)ano;
    utc.mes = (unsigned short)mes;
    utc.dia_semana = 0;
    utc.dia = (unsigned short)dia;
    utc.hora = (unsigned short)(segundos_do_dia / 3600);
    utc.minuto = (unsigned short)(segundos_do_dia % 3600 / 60);
    utc.segundo = (unsigned short)(segundos_do_dia % 60);
    utc.milissegundo = 0;
    if (!SystemTimeToTzSpecificLocalTime(0, &utc, &local))
        return 0;
    return (int32_t)((rt_os_dias_de_civil(local.ano, local.mes, local.dia) * RT_OS_SEG_POR_DIA + local.hora * 3600 + local.minuto * 60 + local.segundo) - unix_s);
}

/* ---- A52: pasta atual, caminho absoluto, exe, pasta temporaria, ambiente e bytes aleatorios ----
 * Contrato (igual ao forja_rt_proc.h): retorno >= 0 = bytes UTF-8 necessarios, sem o NUL, e so grava em destino quando cabem com o NUL (n > retorno);
 * retorno < 0 = -codigo (1 not_found, 2 no_permission, 3 other). Usa sys_ambiente_converter, MultiByteToWideChar e as macros de rt_proc_win.c (mesma unidade). */
__declspec(dllimport) unsigned long GetCurrentDirectoryW(unsigned long n, unsigned short *destino);
__declspec(dllimport) unsigned long GetFullPathNameW(const unsigned short *nome, unsigned long n, unsigned short *destino, unsigned short **parte);
__declspec(dllimport) unsigned long GetModuleFileNameW(void *modulo, unsigned short *destino, unsigned long n);
__declspec(dllimport) unsigned long GetTempPathW(unsigned long n, unsigned short *destino);
__declspec(dllimport) int SetEnvironmentVariableW(const unsigned short *nome, const unsigned short *valor);
__declspec(dllimport) unsigned long GetLastError(void);
__declspec(dllimport) unsigned char SystemFunction036(void *destino, unsigned long n); /* ADVAPI32.dll: RtlGenRandom */

#define RT_OS_ERRO_OUTRO 3
#define RT_OS_MB_INVALIDO 8UL /* MB_ERR_INVALID_CHARS */
#define RT_OS_ERRO_VARIAVEL_NAO_ACHADA 203UL
#define RT_OS_TETO_CAMINHO 32768UL
#define RT_OS_FATIA_ALEATORIA 0x7FFFFFFFLL

/* qtd unidades UTF-16 (sem NUL) -> UTF-8 pelo contrato acima. */
static int64_t rt_os_entregar(const unsigned short *larga, unsigned long qtd, char *destino, int64_t n)
{
    int64_t r = sys_ambiente_converter(larga, qtd, destino, n);

    return r < 0 ? -RT_OS_ERRO_OUTRO : r;
}

/* UTF-8 com NUL -> UTF-16 no heap do runtime; 0 se o texto e invalido ou falta memoria. */
static unsigned short *rt_os_para_larga(const char *texto)
{
    int n = MultiByteToWideChar(RT_PROC_PAGINA_UTF8, RT_OS_MB_INVALIDO, texto, -1, 0, 0);
    unsigned short *larga;

    if (n <= 0)
        return 0;
    larga = sys_heap_alocar((int64_t)n * 2);
    if (larga != 0 && MultiByteToWideChar(RT_PROC_PAGINA_UTF8, RT_OS_MB_INVALIDO, texto, -1, larga, n) <= 0) {
        sys_heap_liberar(larga);
        larga = 0;
    }
    return larga;
}

int64_t forja_proc_pasta_atual(char *destino, int64_t n)
{
    unsigned long cap = GetCurrentDirectoryW(0, 0); /* com o NUL */
    unsigned short *larga;
    unsigned long qtd;
    int64_t r;

    if (cap == 0)
        return -RT_OS_ERRO_OUTRO;
    larga = sys_heap_alocar((int64_t)cap * 2);
    if (larga == 0)
        return -RT_OS_ERRO_OUTRO;
    qtd = GetCurrentDirectoryW(cap, larga);
    r = (qtd == 0 || qtd >= cap) ? -RT_OS_ERRO_OUTRO : rt_os_entregar(larga, qtd, destino, n);
    sys_heap_liberar(larga);
    return r;
}

int64_t forja_proc_caminho_absoluto(const char *caminho, char *destino, int64_t n)
{
    unsigned short *entrada = rt_os_para_larga(caminho);
    unsigned short *larga;
    unsigned long cap;
    unsigned long qtd;
    int64_t r;

    if (entrada == 0)
        return -RT_OS_ERRO_OUTRO;
    cap = GetFullPathNameW(entrada, 0, 0, 0); /* com o NUL */
    larga = cap == 0 ? 0 : sys_heap_alocar((int64_t)cap * 2);
    if (larga == 0) {
        sys_heap_liberar(entrada);
        return -RT_OS_ERRO_OUTRO;
    }
    qtd = GetFullPathNameW(entrada, cap, larga, 0);
    r = (qtd == 0 || qtd >= cap) ? -RT_OS_ERRO_OUTRO : rt_os_entregar(larga, qtd, destino, n);
    sys_heap_liberar(larga);
    sys_heap_liberar(entrada);
    return r;
}

/* GetModuleFileNameW devolve o tamanho do buffer quando trunca: dobra ate caber (teto 32768). */
int64_t forja_proc_caminho_exe(char *destino, int64_t n)
{
    unsigned long cap = 512;
    int64_t r = -RT_OS_ERRO_OUTRO;

    while (cap <= RT_OS_TETO_CAMINHO) {
        unsigned short *larga = sys_heap_alocar((int64_t)cap * 2);
        unsigned long qtd;

        if (larga == 0)
            return -RT_OS_ERRO_OUTRO;
        qtd = GetModuleFileNameW(0, larga, cap);
        if (qtd > 0 && qtd < cap)
            r = rt_os_entregar(larga, qtd, destino, n);
        sys_heap_liberar(larga);
        if (qtd == 0 || qtd < cap)
            return r;
        cap *= 2;
    }
    return -RT_OS_ERRO_OUTRO;
}

/* Sem a barra final, exceto na raiz (C:\). */
int64_t forja_proc_pasta_temp(char *destino, int64_t n)
{
    unsigned long cap = GetTempPathW(0, 0); /* com o NUL */
    unsigned short *larga;
    unsigned long qtd;
    int64_t r;

    if (cap == 0)
        return -RT_OS_ERRO_OUTRO;
    larga = sys_heap_alocar((int64_t)cap * 2);
    if (larga == 0)
        return -RT_OS_ERRO_OUTRO;
    qtd = GetTempPathW(cap, larga);
    if (qtd > 3 && qtd < cap && (larga[qtd - 1] == '\\' || larga[qtd - 1] == '/'))
        qtd--;
    r = (qtd == 0 || qtd >= cap) ? -RT_OS_ERRO_OUTRO : rt_os_entregar(larga, qtd, destino, n);
    sys_heap_liberar(larga);
    return r;
}

/* valor 0 (NULL) remove a variavel; remover a que nao existe e sucesso. 0 = ok, senao o codigo. */
int32_t forja_proc_definir_ambiente(const char *nome, const char *valor)
{
    unsigned short *nome_larga = rt_os_para_larga(nome);
    unsigned short *valor_larga = valor == 0 ? 0 : rt_os_para_larga(valor);
    int32_t r = RT_OS_ERRO_OUTRO;

    if (nome_larga != 0 && (valor == 0 || valor_larga != 0)) {
        if (SetEnvironmentVariableW(nome_larga, valor_larga) != 0)
            r = 0;
        else if (valor == 0 && GetLastError() == RT_OS_ERRO_VARIAVEL_NAO_ACHADA)
            r = 0;
    }
    if (nome_larga != 0)
        sys_heap_liberar(nome_larga);
    if (valor_larga != 0)
        sys_heap_liberar(valor_larga);
    return r;
}

/* Enche destino com n bytes do gerador do sistema, em fatias de 0x7FFFFFFF. 0 = ok. */
int32_t forja_proc_bytes_aleatorios(unsigned char *destino, int64_t n)
{
    while (n > 0) {
        int64_t fatia = n > RT_OS_FATIA_ALEATORIA ? RT_OS_FATIA_ALEATORIA : n;

        if (SystemFunction036(destino, (unsigned long)fatia) == 0)
            return RT_OS_ERRO_OUTRO;
        destino += fatia;
        n -= fatia;
    }
    return 0;
}

/* ---- P28b-2b: stderr cru e pico de memoria ---- */
void sys_escrever(int fd, const void *buf, int64_t n);
__declspec(dllimport) int K32GetProcessMemoryInfo(void *processo, void *contadores, unsigned long cb); /* kernel32: 61o import do ligar.c */

/* PROCESS_MEMORY_COUNTERS (72 bytes no x64): PeakWorkingSetSize e o 1o campo depois de cb e PageFaultCount. */
typedef struct {
    uint32_t cb;
    uint32_t faltas_de_pagina;
    uint64_t pico_conjunto_de_trabalho;
    uint64_t resto[7];
} RtOsMemoria;

/* Bytes crus em stderr (WriteFile, sem CRLF e sem buffer): o sys_escrever do runtime, o mesmo do print de erro. */
void forja_proc_escrever_erro(const unsigned char *dados, int64_t n)
{
    sys_escrever(2, dados, n);
}

/* >= 0 = pico do conjunto de trabalho do proprio processo em bytes (pseudo handle -1); < 0 = -codigo (3 other). */
int64_t forja_proc_pico_memoria(void)
{
    RtOsMemoria m = {(uint32_t)sizeof(RtOsMemoria), 0, 0, {0}};

    if (K32GetProcessMemoryInfo((void *)(intptr_t)-1, &m, m.cb) == 0)
        return -RT_OS_ERRO_OUTRO;
    return (int64_t)m.pico_conjunto_de_trabalho;
}

/* ---- M8 K1: pid e numero de processadores (std:os.pid, std:os.cpu_count) ---- */
__declspec(dllimport) unsigned long GetCurrentProcessId(void); /* kernel32: 73o import do ligar_tabelas.c */
__declspec(dllimport) unsigned long GetActiveProcessorCount(unsigned short grupo); /* kernel32: 74o import; grupo 0xFFFF = ALL_PROCESSOR_GROUPS */
#define RT_OS_TODOS_OS_GRUPOS 0xFFFFU

/* Pid do proprio processo (GetCurrentProcessId nunca falha). */
int64_t forja_os_pid(void)
{
    return (int64_t)GetCurrentProcessId();
}

/* >= 1 = processadores ativos em todos os grupos; 0 (erro) vira 1. */
int64_t forja_os_cpu_count(void)
{
    unsigned long qtd = GetActiveProcessorCount(RT_OS_TODOS_OS_GRUPOS);

    return qtd == 0 ? 1 : (int64_t)qtd;
}

/* ---- D-G3-L2a (bloco A): memoria livre, viveza de pid, erro do sistema e stdout cru ---- */
/* MEMORYSTATUSEX (64 bytes): dwLength, dwMemoryLoad, ullTotalPhys, ullAvailPhys e 5 campos que nao usamos. */
typedef struct {
    unsigned long tamanho;
    unsigned long carga;
    uint64_t total_fisica;
    uint64_t livre_fisica;
    uint64_t resto[5];
} RtOsMemoriaGlobal;

__declspec(dllimport) int GlobalMemoryStatusEx(RtOsMemoriaGlobal *estado); /* kernel32: 1o dos 8 imports do bloco A */
__declspec(dllimport) void *OpenProcess(unsigned long acesso, int herda, unsigned long pid); /* kernel32 */
__declspec(dllimport) int CloseHandle(void *h); /* kernel32 (ja importado pelo ligar_tabelas.c) */
__declspec(dllimport) unsigned long WaitForSingleObject(void *h, unsigned long ms); /* kernel32 */
__declspec(dllimport) unsigned long FormatMessageW(unsigned long flags, const void *fonte, unsigned long codigo, unsigned long idioma,
                                                   unsigned short *destino, unsigned long n, void *argumentos); /* kernel32 */
void rt_saida_stdout_cru(const void *p, int64_t n);
void rt_saida_descarregar(void);

#define RT_OS_CONSULTA_LIMITADA 0x1000UL /* PROCESS_QUERY_LIMITED_INFORMATION */
#define RT_OS_SINCRONIZAR 0x00100000UL /* SYNCHRONIZE */
#define RT_OS_ESPERA_AGORA 0UL
#define RT_OS_ESPERA_EXPIROU 0x102UL /* WAIT_TIMEOUT: o processo ainda roda */
#define RT_OS_ERRO_ACESSO_NEGADO 5UL
#define RT_OS_PID_MAXIMO 0xFFFFFFFFLL
#define RT_OS_VIVO 1
#define RT_OS_MORTO 0
/* FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS; idioma 0x0409 = ingles (en-US). */
#define RT_OS_MSG_FLAGS 0x1200UL
#define RT_OS_IDIOMA_INGLES 0x0409UL
#define RT_OS_IDIOMA_NEUTRO 0UL
#define RT_OS_MSG_TETO 1024

int64_t forja_os_mem_available(void)
{
    RtOsMemoriaGlobal estado = {0};

    estado.tamanho = (unsigned long)sizeof(RtOsMemoriaGlobal);
    if (GlobalMemoryStatusEx(&estado) == 0)
        return -RT_OS_ERRO_OUTRO;
    return (int64_t)estado.livre_fisica;
}

/* 1 = o processo existe (ACCESS_DENIED conta como vivo: o sistema nao deixa olhar, mas ele roda); 0 = saiu, nao existe ou pid fora de 1..2^32-1. */
int32_t forja_os_pid_alive(int64_t pid)
{
    void *h;
    int32_t vivo;

    if (pid <= 0 || pid > RT_OS_PID_MAXIMO)
        return RT_OS_MORTO;
    h = OpenProcess(RT_OS_CONSULTA_LIMITADA | RT_OS_SINCRONIZAR, 0, (unsigned long)pid);
    if (h == 0)
        return GetLastError() == RT_OS_ERRO_ACESSO_NEGADO ? RT_OS_VIVO : RT_OS_MORTO;
    vivo = WaitForSingleObject(h, RT_OS_ESPERA_AGORA) == RT_OS_ESPERA_EXPIROU ? RT_OS_VIVO : RT_OS_MORTO;
    CloseHandle(h);
    return vivo;
}

int64_t forja_os_last_error(void)
{
    return (int64_t)GetLastError();
}

/* Texto do erro em UTF-16 no destino (RT_OS_MSG_TETO unidades); 0 = codigo sem texto. Tenta o ingles e, se faltar, o idioma neutro. */
static unsigned long rt_os_mensagem_larga(unsigned long codigo, unsigned short *destino, int32_t local)
{
    unsigned long qtd = 0;

    /* I9-S1: local != 0 pula o ingles e vai direto ao idioma 0 (o do usuario/sistema da maquina, o mesmo da std do Rust). */
    if (local == 0)
        qtd = FormatMessageW(RT_OS_MSG_FLAGS, 0, codigo, RT_OS_IDIOMA_INGLES, destino, RT_OS_MSG_TETO, 0);
    if (qtd == 0)
        qtd = FormatMessageW(RT_OS_MSG_FLAGS, 0, codigo, RT_OS_IDIOMA_NEUTRO, destino, RT_OS_MSG_TETO, 0);
    return qtd;
}

/* Mesmo contrato de tamanho do os_ler_texto: >= 0 = bytes UTF-8 necessarios (sem NUL), grava so se couber com o NUL. Apara CR, LF, espaco e tab do fim. */
int64_t forja_os_error_text(int64_t codigo, char *destino, int64_t n, int32_t local)
{
    unsigned short larga[RT_OS_MSG_TETO];
    unsigned long qtd;

    if (codigo < 0 || codigo > RT_OS_PID_MAXIMO)
        return 0;
    qtd = rt_os_mensagem_larga((unsigned long)codigo, larga, local);
    while (qtd > 0 && (larga[qtd - 1] == ' ' || larga[qtd - 1] == '\r' || larga[qtd - 1] == '\n' || larga[qtd - 1] == '\t'))
        qtd--;
    if (qtd == 0)
        return 0;
    return rt_os_entregar(larga, qtd, destino, n);
}

/* std:os.write_stdout e flush_stdout: o buffer do stdout e o do print (rt_saida.c). */
void forja_os_write_stdout(const unsigned char *dados, int64_t n)
{
    rt_saida_stdout_cru(dados, n);
}

void forja_os_flush_stdout(void)
{
    rt_saida_descarregar();
}
