/*
 * rt_os_linux.c - std:os no Linux x86-64: sleep_ms, time_ns e wall_time_ns (IA1-L6, L-07; IA1-LT).
 *
 * Versao: 0.10.0
 * Changelog:
 *   0.10.0 - forja 0.44.0: forja_os_error_text devolve o texto do errno em ingles (tabela do musl em bloco NUL, errno 1..131; 0 e codigo sem texto = ""), igual ao Rust Linux estatico; local segue ignorado.
 *   0.9.0 - I9-S1 (forja 0.43.0): forja_os_error_text ganha o 4o parametro local (int32), ignorado: no Linux o texto segue "" (sem libc nao ha idioma local nem strerror).
 *   0.8.0 - D-G3-L2b: mem_available (sysinfo 99, freeram * mem_unit), pid_alive (kill(pid, 0); EPERM = vivo), last_error (rt_os_ultimo_erro, global gravada pelo std:file e std:process) e error_text (sempre "": sem libc).
 *   0.7.0 - D-G3-L2a (bloco A): stubs de falha de mem_available, pid_alive, last_error e error_text (sem syscall; o L2b faz o real) e write_stdout/flush_stdout reais pelo rt_saida.c comum.
 *   0.6.0 - M8 K1 (D-M8-8): forja_os_pid (getpid 39) e forja_os_cpu_count (sched_getaffinity 204, contando bits; erro ou zero -> 1).
 *   0.5.0 - R2: forja_os_fuso_s devolve INT32_MIN (o std:time usa o parser TZif em Forja pura no Linux).
 *   0.4.1 - M5c-b P63 (D-R7): forja_proc_pasta_temp le o TMPDIR vivo sob a trava do ambiente.
 *   0.4.0 - M5 P28b-2b: forja_proc_escrever_erro (sys_escrever(2)) e forja_proc_pico_memoria (getrusage 98, ru_maxrss * 1024).
 *   0.3.0 - M5 P28b-1 (A52): forja_proc_pasta_atual (getcwd 79), caminho_absoluto (lexico, sem disco), caminho_exe (AT_EXECFN do auxv, fallback argv[0] com "/"), pasta_temp (TMPDIR vivo ou /tmp),
 *           definir_ambiente (envp vivo no heap) e bytes_aleatorios (getrandom 318; fallback /dev/urandom). O contrato e o do rt_os_win.c.
 *   0.2.0 - IA1-LT: forja_proc_hora_ns (hora de parede: clock_gettime CLOCK_REALTIME, a mesma syscall 228 do monotonico, sem libc).
 *   0.1.0 - IA1-L6: primeira versao. forja_proc_dormir_ms (nanosleep, syscall 35, refeita com o resto quando um sinal a interrompe) e forja_proc_tempo_ns (o sys_tempo_ns do Linux:
 *           clock_gettime CLOCK_MONOTONIC). Compilado e embutido no rt_linux_*.o; a validacao no WSL fica para o P28 (D-M5-3: a lista de syscalls ganha nanosleep).
 *
 * Incluido por rt_linux.c depois de rt_proc.c; nao compila sozinho.
 */
#include <stdint.h>

#include "rt_syscall_linux.h"

/* SYSL_CLOCK_MONOTONIC e SYSL_NS_POR_S vem do rt_sys_linux.c (incluido antes). */
#define SYSL_CLOCK_REALTIME 0LL

int64_t sys_tempo_ns(void);

/* Dorme ms milissegundos (<= 0 volta na hora). */
void forja_proc_dormir_ms(int64_t ms)
{
    int64_t pedido[2];

    if (ms <= 0)
        return;
    pedido[0] = ms / 1000;
    pedido[1] = (ms % 1000) * 1000000;
    while (sysl_chamar2(SYSL_NANOSLEEP, (int64_t)pedido, (int64_t)pedido) == SYSL_EINTR) {
    }
}

/* Linux ainda sem thread (M5c-b): nada a ligar. */
void forja_proc_multithread_ligar(void)
{
}

/* Relogio monotonico em nanossegundos desde um ponto fixo do sistema (so serve para medir diferencas). */
int64_t forja_proc_tempo_ns(void)
{
    return sys_tempo_ns();
}

/* Hora de parede em nanossegundos desde 1970-01-01 UTC (NAO monotonica: o relogio do sistema pode ser acertado). */
int64_t forja_proc_hora_ns(void)
{
    int64_t ts[2] = {0, 0}; /* struct timespec: segundos e nanossegundos */

    sysl_chamar2(SYSL_CLOCK_GETTIME, SYSL_CLOCK_REALTIME, (int64_t)ts);
    return ts[0] * SYSL_NS_POR_S + ts[1];
}

/* R2: o Linux nao resolve fuso no runtime. Devolve INT32_MIN ("sem fuso nativo"): o std:time cai no parser TZif em Forja pura (std\time\fuso.fj). */
int32_t forja_os_fuso_s(int64_t unix_s)
{
    (void)unix_s;
    return (int32_t)(-2147483647 - 1);
}

/* ---- A52: pasta atual, caminho absoluto, exe, pasta temporaria, ambiente e bytes aleatorios (P28b-1) ----
 * Contrato (o mesmo de rt_os_win.c): retorno >= 0 = bytes UTF-8 necessarios, sem o NUL, e so grava em destino quando cabem com o NUL (n > retorno);
 * retorno < 0 = -codigo (1 not_found, 2 no_permission, 3 other). O Linux ja entrega UTF-8: nao ha conversao. */
#define RT_OS_ERRO_NAO_ACHADO 1
#define RT_OS_ERRO_SEM_PERMISSAO 2
#define RT_OS_ERRO_OUTRO 3
#define RT_OS_ENOENT 2LL
#define RT_OS_EACCES 13LL
#define RT_OS_ERANGE 34LL
#define RT_OS_ENOSYS 38LL
#define RT_OS_EPERM 1LL
#define RT_OS_CAP_CWD_INICIAL 4096
#define RT_OS_CAP_CWD_MAXIMA (1 << 20)
#define RT_OS_FATIA_ALEATORIA (1 << 20)
#define RT_OS_O_LER_CLOEXEC 0x80000LL /* O_RDONLY | O_CLOEXEC */
#define RT_OS_AT_FDCWD (-100LL)

/* -errno do kernel -> codigo neutro (1, 2 ou 3). */
static int32_t rt_os_codigo_do_erro(int64_t retorno)
{
    if (retorno == -RT_OS_ENOENT)
        return RT_OS_ERRO_NAO_ACHADO;
    if (retorno == -RT_OS_EACCES)
        return RT_OS_ERRO_SEM_PERMISSAO;
    return RT_OS_ERRO_OUTRO;
}

static int64_t rt_os_tamanho(const char *texto)
{
    int64_t n = 0;

    while (texto[n] != '\0')
        n++;
    return n;
}

/* Entrega `tam` bytes pelo contrato: so grava (com NUL) quando n > tam. */
static int64_t rt_os_entregar(const char *texto, int64_t tam, char *destino, int64_t n)
{
    if (n > tam) {
        for (int64_t i = 0; i < tam; i++)
            destino[i] = texto[i];
        destino[tam] = '\0';
    }
    return tam;
}

/* getcwd no heap: ERANGE dobra o buffer ate o teto. Devolve o texto (dono: quem chama libera) e o tamanho sem o NUL em *tam, ou NULL com -codigo em *tam. */
static char *rt_os_ler_cwd(int64_t *tam)
{
    int64_t cap = RT_OS_CAP_CWD_INICIAL;

    while (cap <= RT_OS_CAP_CWD_MAXIMA) {
        char *buffer = sys_heap_alocar(cap);
        int64_t r;

        if (buffer == NULL) {
            *tam = -RT_OS_ERRO_OUTRO;
            return NULL;
        }
        r = sysl_chamar2(SYSL_GETCWD, (int64_t)buffer, cap);
        if (!sysl_falhou(r) && r > 0) {
            *tam = r - 1; /* o kernel conta o NUL */
            return buffer;
        }
        sys_heap_liberar(buffer);
        if (r != -RT_OS_ERANGE) {
            *tam = -(int64_t)rt_os_codigo_do_erro(r);
            return NULL;
        }
        cap *= 2;
    }
    *tam = -RT_OS_ERRO_OUTRO;
    return NULL;
}

int64_t forja_proc_pasta_atual(char *destino, int64_t n)
{
    int64_t tam;
    char *cwd = rt_os_ler_cwd(&tam);
    int64_t r;

    if (cwd == NULL)
        return tam;
    r = rt_os_entregar(cwd, tam, destino, n);
    sys_heap_liberar(cwd);
    return r;
}

/* Acrescenta os componentes de `texto` a `saida` ("/a/b", vazio = raiz), resolvendo ".", ".." e "//" so pelo texto (sem disco, sem symlink). Texto absoluto recomeca da raiz.
 * LIMITE: cognitiva um unico laco de componentes com 4 casos ("", ".", ".." e nome); partir em funcoes espalha o estado de saida/len. */
static int64_t rt_os_juntar(char *saida, int64_t len, const char *texto)
{
    const char *p = texto;

    if (*p == '/')
        len = 0;
    while (*p != '\0') {
        const char *inicio;
        int64_t n;

        while (*p == '/')
            p++;
        inicio = p;
        while (*p != '\0' && *p != '/')
            p++;
        n = p - inicio;
        if (n == 0 || (n == 1 && inicio[0] == '.'))
            continue;
        if (n == 2 && inicio[0] == '.' && inicio[1] == '.') {
            while (len > 0 && saida[len - 1] != '/')
                len--;
            if (len > 0)
                len--;
            continue;
        }
        saida[len++] = '/';
        for (int64_t i = 0; i < n; i++)
            saida[len++] = inicio[i];
    }
    return len;
}

/* Caminho lexico absoluto de `caminho` (relativo parte da pasta atual) entregue pelo contrato; sem barra final, "/" na raiz. */
static int64_t rt_os_absoluto(const char *caminho, char *destino, int64_t n)
{
    int64_t tam_cwd = 0;
    char *cwd = NULL;
    char *saida;
    int64_t len = 0;
    int64_t r;

    if (caminho[0] != '/') {
        cwd = rt_os_ler_cwd(&tam_cwd);
        if (cwd == NULL)
            return tam_cwd;
    }
    saida = sys_heap_alocar(tam_cwd + rt_os_tamanho(caminho) + 2);
    if (saida == NULL) {
        if (cwd != NULL)
            sys_heap_liberar(cwd);
        return -RT_OS_ERRO_OUTRO;
    }
    if (cwd != NULL) {
        len = rt_os_juntar(saida, 0, cwd);
        sys_heap_liberar(cwd);
    }
    len = rt_os_juntar(saida, len, caminho);
    if (len == 0)
        saida[len++] = '/';
    r = rt_os_entregar(saida, len, destino, n);
    sys_heap_liberar(saida);
    return r;
}

int64_t forja_proc_caminho_absoluto(const char *caminho, char *destino, int64_t n)
{
    return rt_os_absoluto(caminho, destino, n);
}

static bool rt_os_tem_barra(const char *texto)
{
    for (; *texto != '\0'; texto++) {
        if (*texto == '/')
            return true;
    }
    return false;
}

/* O executavel: AT_EXECFN do auxv (o que o kernel recebeu no execve; relativo resolve contra a pasta atual) ou, sem ele, o argv[0] SE tiver "/". Sem "/" = nao achado.
 * Nao resolve symlink (sem readlink de /proc: diferenca documentada do Windows). */
int64_t forja_proc_caminho_exe(char *destino, int64_t n)
{
    const char *exe = sysl_execfn();

    if (exe == NULL && sysl_argc > 0 && rt_os_tem_barra(sysl_argv[0]))
        exe = sysl_argv[0];
    if (exe == NULL || exe[0] == '\0')
        return -RT_OS_ERRO_NAO_ACHADO;
    return rt_os_absoluto(exe, destino, n);
}

/* TMPDIR vivo se nao vazio, senao /tmp; sem a barra final (exceto a raiz). */
int64_t forja_proc_pasta_temp(char *destino, int64_t n)
{
    bool tomou = sys_ambiente_travar();
    const char *pasta = sysl_valor_ambiente("TMPDIR");
    int64_t tam;
    int64_t r;

    if (pasta == NULL || pasta[0] == '\0')
        pasta = "/tmp";
    tam = rt_os_tamanho(pasta);
    while (tam > 1 && pasta[tam - 1] == '/')
        tam--;
    r = rt_os_entregar(pasta, tam, destino, n);
    sys_ambiente_destravar(tomou);
    return r;
}

/* valor 0 (NULL) remove a variavel; remover a que nao existe e sucesso. 0 = ok, senao o codigo. */
int32_t forja_proc_definir_ambiente(const char *nome, const char *valor)
{
    return sysl_ambiente_definir(nome, valor);
}

/* Um read repetido no EINTR. */
static int64_t rt_os_ler_fd(int64_t fd, void *destino, int64_t n)
{
    int64_t r;

    do {
        r = sysl_chamar3(SYSL_READ, fd, (int64_t)destino, n);
    } while (r == SYSL_EINTR);
    return r;
}

/* Fallback sem getrandom (kernel < 3.17 ou seccomp): /dev/urandom. fd >= 0 ou o -errno. */
static int64_t rt_os_abrir_urandom(void)
{
    int64_t v[7] = {SYSL_OPENAT, RT_OS_AT_FDCWD, (int64_t)"/dev/urandom", RT_OS_O_LER_CLOEXEC, 0, 0, 0};

    return sysl_chamar6(v);
}

/* Enche destino com n bytes: getrandom 318 em fatias (EINTR repete); ENOSYS/EPERM trocam para /dev/urandom. 0 = ok.
 * LIMITE: cognitiva laco em fatias com EINTR + queda para o urandom no mesmo estado (restante, posicao).
 * LIMITE: niveis laco, tentativa de EINTR e troca de fonte no mesmo fluxo. */
int32_t forja_proc_bytes_aleatorios(unsigned char *destino, int64_t n)
{
    int64_t fd = -1;

    while (n > 0) {
        int64_t fatia = n > RT_OS_FATIA_ALEATORIA ? RT_OS_FATIA_ALEATORIA : n;
        int64_t r;

        if (fd >= 0) {
            r = rt_os_ler_fd(fd, destino, fatia);
        } else {
            do {
                r = sysl_chamar3(SYSL_GETRANDOM, (int64_t)destino, fatia, 0);
            } while (r == SYSL_EINTR);
            if (r == -RT_OS_ENOSYS || r == -RT_OS_EPERM) {
                fd = rt_os_abrir_urandom();
                if (sysl_falhou(fd))
                    return RT_OS_ERRO_OUTRO;
                continue;
            }
        }
        if (sysl_falhou(r) || r == 0)
            break;
        destino += r;
        n -= r;
    }
    if (fd >= 0)
        sysl_chamar1(SYSL_CLOSE, fd);
    return n > 0 ? RT_OS_ERRO_OUTRO : 0;
}

/* ---- P28b-2b: stderr cru e pico de memoria ---- */
#define RT_OS_RUSAGE_SELF 0LL
#define RT_OS_RUSAGE_PALAVRAS 18 /* struct rusage = 144 bytes: 2 timeval (4 palavras) e depois ru_maxrss */
#define RT_OS_RUSAGE_MAXRSS 4    /* indice de ru_maxrss em palavras de 8 bytes (deslocamento 32), em KiB */
#define RT_OS_BYTES_POR_KIB 1024
#define RT_OS_MASCARA_PALAVRAS 64 /* 64 x 64 bits = 4096 CPUs (o teto de std:os.cpu_count) */

/* Bytes crus em stderr: o sys_escrever do runtime (write 2 em laco; EINTR repete). */
void forja_proc_escrever_erro(const unsigned char *dados, int64_t n)
{
    sys_escrever(2, dados, n);
}

/* >= 0 = pico do conjunto residente do proprio processo em bytes (getrusage 98: ru_maxrss em KiB); < 0 = -codigo (3 other). */
int64_t forja_proc_pico_memoria(void)
{
    int64_t uso[RT_OS_RUSAGE_PALAVRAS] = {0};

    if (sysl_falhou(sysl_chamar2(SYSL_GETRUSAGE, RT_OS_RUSAGE_SELF, (int64_t)uso)))
        return -RT_OS_ERRO_OUTRO;
    return uso[RT_OS_RUSAGE_MAXRSS] * RT_OS_BYTES_POR_KIB;
}

/* M8 K1: pid do proprio processo (getpid 39 nao falha; devolve o pid). */
int64_t forja_os_pid(void)
{
    return sysl_chamar1(SYSL_GETPID, 0);
}

/* >= 1 = bits do conjunto de afinidade (sched_getaffinity 204, mascara de 4096 bits); erro ou zero vira 1. */
int64_t forja_os_cpu_count(void)
{
    uint64_t mascara[RT_OS_MASCARA_PALAVRAS] = {0};
    int64_t bits = 0;
    int64_t r = sysl_chamar3(SYSL_SCHED_GETAFFINITY, 0, (int64_t)sizeof mascara, (int64_t)mascara);

    if (sysl_falhou(r))
        return 1;
    for (int64_t i = 0; i < RT_OS_MASCARA_PALAVRAS; i++) {
        uint64_t palavra = mascara[i];

        while (palavra != 0) {
            palavra &= palavra - 1;
            bits++;
        }
    }
    return bits == 0 ? 1 : bits;
}

/* D-G3-L2b: std:os mem_available, pid_alive, last_error e error_text por syscall direta (sem libc). */
#define RT_OS_SYSINFO_PALAVRAS 16 /* struct sysinfo = 112 bytes: freeram na palavra 5 (deslocamento 40) e mem_unit (u32) na 13 (deslocamento 104) */
#define RT_OS_SYSINFO_FREERAM 5
#define RT_OS_SYSINFO_MEM_UNIT 13
#define RT_OS_EPERM_KILL 1LL

/* Ultimo errno (positivo) do runtime: gravado pelas falhas de open (std:file) e de spawn (std:process), lido por last_error. 0 = nenhuma falha ainda.
 * Global do objeto (nao static): o rt_linux_es.o o grava (extern em rt_es_linux.c). Sem libc nao ha errno por thread: e UM valor do processo, como o SetLastError do Windows nao e. */
int64_t rt_os_ultimo_erro = 0; /* compartilhado: so o ultimo erro gravado vale, sem trava */

/* RAM livre em bytes: sysinfo 99, freeram * mem_unit. NAO e o MemAvailable do /proc (que soma o cache recuperavel): o freeram so conta paginas realmente livres, entao
 * o valor e MENOR que o ullAvailPhys do Windows (que inclui a lista standby). Serve igual para limitar o paralelismo, com folga a favor da seguranca. */
int64_t forja_os_mem_available(void)
{
    int64_t info[RT_OS_SYSINFO_PALAVRAS] = {0};
    uint64_t unidade;

    if (sysl_falhou(sysl_chamar1(SYSL_SYSINFO, (int64_t)info)))
        return -RT_OS_ERRO_OUTRO;
    unidade = (uint32_t)info[RT_OS_SYSINFO_MEM_UNIT];
    return info[RT_OS_SYSINFO_FREERAM] * (int64_t)(unidade == 0 ? 1 : unidade);
}

/* 1 = o processo existe (kill(pid, 0): EPERM conta como vivo, como o ACCESS_DENIED do Windows); 0 = nao existe (ESRCH) ou pid <= 0 (kill(0) e kill(-n) mirariam um grupo).
 * Zumbi que o pai ainda nao colheu conta como vivo (kill o aceita). */
int32_t forja_os_pid_alive(int64_t pid)
{
    int64_t r;

    if (pid <= 0)
        return 0;
    r = sysl_chamar2(SYSL_KILL, pid, 0);
    return r == 0 || r == -RT_OS_EPERM_KILL ? 1 : 0;
}

int64_t forja_os_last_error(void)
{
    return rt_os_ultimo_erro;
}

/* forja 0.44.0: textos de errno do musl (strerror do Linux x86-64, o mesmo do Rust Linux estatico), em ingles. Um unico bloco de textos terminados em NUL, na ordem do errno
 * (indice 0 = errno 0): sem tabela de ponteiros, entao nao entra relocacao de dado no rt_linux (vale tambem na variante PIE). Errno sem texto no musl ("No error information") fica "". */
#define RT_OS_ERRNO_MAXIMO 131
static const char rt_os_textos_errno[] =
    "\0" /* 0 */
    "Operation not permitted\0" /* 1 */
    "No such file or directory\0" /* 2 */
    "No such process\0" /* 3 */
    "Interrupted system call\0" /* 4 */
    "I/O error\0" /* 5 */
    "No such device or address\0" /* 6 */
    "Argument list too long\0" /* 7 */
    "Exec format error\0" /* 8 */
    "Bad file descriptor\0" /* 9 */
    "No child process\0" /* 10 */
    "Resource temporarily unavailable\0" /* 11 */
    "Out of memory\0" /* 12 */
    "Permission denied\0" /* 13 */
    "Bad address\0" /* 14 */
    "Block device required\0" /* 15 */
    "Resource busy\0" /* 16 */
    "File exists\0" /* 17 */
    "Cross-device link\0" /* 18 */
    "No such device\0" /* 19 */
    "Not a directory\0" /* 20 */
    "Is a directory\0" /* 21 */
    "Invalid argument\0" /* 22 */
    "Too many open files in system\0" /* 23 */
    "No file descriptors available\0" /* 24 */
    "Not a tty\0" /* 25 */
    "Text file busy\0" /* 26 */
    "File too large\0" /* 27 */
    "No space left on device\0" /* 28 */
    "Invalid seek\0" /* 29 */
    "Read-only file system\0" /* 30 */
    "Too many links\0" /* 31 */
    "Broken pipe\0" /* 32 */
    "Domain error\0" /* 33 */
    "Result not representable\0" /* 34 */
    "Resource deadlock would occur\0" /* 35 */
    "Filename too long\0" /* 36 */
    "No locks available\0" /* 37 */
    "Function not implemented\0" /* 38 */
    "Directory not empty\0" /* 39 */
    "Symbolic link loop\0" /* 40 */
    "\0" /* 41 */
    "No message of desired type\0" /* 42 */
    "Identifier removed\0" /* 43 */
    "\0" /* 44 */
    "\0" /* 45 */
    "\0" /* 46 */
    "\0" /* 47 */
    "\0" /* 48 */
    "\0" /* 49 */
    "\0" /* 50 */
    "\0" /* 51 */
    "\0" /* 52 */
    "\0" /* 53 */
    "\0" /* 54 */
    "\0" /* 55 */
    "\0" /* 56 */
    "\0" /* 57 */
    "\0" /* 58 */
    "\0" /* 59 */
    "Device not a stream\0" /* 60 */
    "No data available\0" /* 61 */
    "Device timeout\0" /* 62 */
    "Out of streams resources\0" /* 63 */
    "\0" /* 64 */
    "\0" /* 65 */
    "\0" /* 66 */
    "Link has been severed\0" /* 67 */
    "\0" /* 68 */
    "\0" /* 69 */
    "\0" /* 70 */
    "Protocol error\0" /* 71 */
    "Multihop attempted\0" /* 72 */
    "\0" /* 73 */
    "Bad message\0" /* 74 */
    "Value too large for data type\0" /* 75 */
    "\0" /* 76 */
    "File descriptor in bad state\0" /* 77 */
    "\0" /* 78 */
    "\0" /* 79 */
    "\0" /* 80 */
    "\0" /* 81 */
    "\0" /* 82 */
    "\0" /* 83 */
    "Illegal byte sequence\0" /* 84 */
    "\0" /* 85 */
    "\0" /* 86 */
    "\0" /* 87 */
    "Not a socket\0" /* 88 */
    "Destination address required\0" /* 89 */
    "Message too large\0" /* 90 */
    "Protocol wrong type for socket\0" /* 91 */
    "Protocol not available\0" /* 92 */
    "Protocol not supported\0" /* 93 */
    "Socket type not supported\0" /* 94 */
    "Not supported\0" /* 95 */
    "Protocol family not supported\0" /* 96 */
    "Address family not supported by protocol\0" /* 97 */
    "Address in use\0" /* 98 */
    "Address not available\0" /* 99 */
    "Network is down\0" /* 100 */
    "Network unreachable\0" /* 101 */
    "Connection reset by network\0" /* 102 */
    "Connection aborted\0" /* 103 */
    "Connection reset by peer\0" /* 104 */
    "No buffer space available\0" /* 105 */
    "Socket is connected\0" /* 106 */
    "Socket not connected\0" /* 107 */
    "Cannot send after socket shutdown\0" /* 108 */
    "\0" /* 109 */
    "Operation timed out\0" /* 110 */
    "Connection refused\0" /* 111 */
    "Host is down\0" /* 112 */
    "Host is unreachable\0" /* 113 */
    "Operation already in progress\0" /* 114 */
    "Operation in progress\0" /* 115 */
    "Stale file handle\0" /* 116 */
    "Data consistency error\0" /* 117 */
    "\0" /* 118 */
    "Resource not available\0" /* 119 */
    "\0" /* 120 */
    "Remote I/O error\0" /* 121 */
    "Quota exceeded\0" /* 122 */
    "No medium found\0" /* 123 */
    "Wrong medium type\0" /* 124 */
    "Operation canceled\0" /* 125 */
    "Required key not available\0" /* 126 */
    "Key has expired\0" /* 127 */
    "Key has been revoked\0" /* 128 */
    "Key was rejected by service\0" /* 129 */
    "Previous owner died\0" /* 130 */
    "State not recoverable\0" /* 131 */;

/* Texto do errno (musl, ingles; "" para 0, codigo sem texto ou fora de 1..131: contrato do Windows). O parametro local e ignorado: o Linux nao tem idioma local. */
int64_t forja_os_error_text(int64_t codigo, char *destino, int64_t n, int32_t local)
{
    const char *texto = rt_os_textos_errno;
    int64_t tam = 0;
    int64_t i;

    (void)local;
    if (codigo <= 0 || codigo > RT_OS_ERRNO_MAXIMO)
        return rt_os_entregar("", 0, destino, n);
    for (i = 0; i < codigo; i++) {
        while (*texto != 0)
            texto++;
        texto++;
    }
    while (texto[tam] != 0)
        tam++;
    return rt_os_entregar(texto, tam, destino, n);
}

/* write_stdout e flush_stdout nao usam syscall: o buffer do stdout e o do print (rt_saida.c, comum aos dois alvos). */
void forja_os_write_stdout(const unsigned char *dados, int64_t n)
{
    rt_saida_stdout_cru(dados, n);
}

void forja_os_flush_stdout(void)
{
    rt_saida_descarregar();
}
