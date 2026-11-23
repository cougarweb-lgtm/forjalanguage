/*
 * rt_sys_win.c - camada sys do runtime nativo no Windows x64 (P2a).
 *
 * Versao: 0.7.1
 * Changelog:
 *   0.7.1 - M6e P93f: sys_guarda_limite_mapas (sempre false no Windows).
 *   0.7.0 - M6e P93e: sys_falha_ligar (pre-tratador da guarda de memoria, antes do tratador do sys_excecao_ligar; SysFalha do EXCEPTION_RECORD/CONTEXT pelos offsets do censo c5) e sys_guarda_reservar/commit/decommit (VirtualAlloc/VirtualFree, sem import novo). O tratador vetorial e ligado uma vez so (sys_veh_garantir).
 *   0.6.0 - M5 P44: rt_multithread (flag pegajoso), sys_multithread_ligar, sys_trava_pegar_srw e sys_trava_soltar_srw (AcquireSRWLockExclusive e ReleaseSRWLockExclusive: 65o e 66o imports de kernel32).
 *   0.5.0 - M5 P43: sys_tls_alocar, sys_tls_ler e sys_tls_gravar (TlsAlloc, TlsGetValue e TlsSetValue: 62o ao 64o import de kernel32, so de quem usa o RtFio; o ligador descarta o resto).
 *   0.4.0 - Linguagem v1.11 L16b: sys_excecao_ligar (AddVectoredExceptionHandler, o 17o import de kernel32). So o rt_teste.c a usa: o ligador descarta a funcao e o __imp_ em quem nao roda 'forja test'.
 *   0.3.0 -Linguagem v1.11 L16a: sys_ambiente (GetEnvironmentVariableA, o 16o import de kernel32). So o rt_teste.c o usa: o ligador descarta a funcao e o __imp_ em quem nao roda 'forja test'.
 *   0.2.1 - M2 P9a: o noreturn de ExitProcess vem de RT_NORETORNO (rt_compat.h); o objeto sai igual.
 *   0.2.0 - console em UTF-8: GetConsoleMode, GetConsoleOutputCP e SetConsoleOutputCP (12 -> 15 imports);
 *           a pagina original volta em sys_sair. Sem console nada e chamado.
 *   0.1.0 - primeira versao: os 12 imports de kernel32 que o runtime usa.
 *
 * Os prototipos sao a mao, sem windows.h: assim o .o importa so os 15 simbolos
 * __imp_ de kernel32 e nada mais. Incluido por rt_win.c; nao compila sozinho.
 */
#include "rt_sys.h"
#include "rt_compat.h"

#ifndef RT_ALVO_WIN
#error "rt_sys_win.c exige -DRT_ALVO_WIN"
#endif

/* Limite por WriteFile: DWORD de 32 bits, lotes de 1 GiB para ficar longe do teto. */
#define SYS_LOTE_ESCRITA 0x40000000ULL

#define SYS_MEM_RESERVAR_CONFIRMAR 0x3000UL /* MEM_RESERVE | MEM_COMMIT */
#define SYS_MEM_LIBERAR 0x8000UL            /* MEM_RELEASE */
#define SYS_MEM_RESERVAR 0x2000UL           /* MEM_RESERVE */
#define SYS_MEM_CONFIRMAR 0x1000UL          /* MEM_COMMIT */
#define SYS_MEM_DESCONFIRMAR 0x4000UL       /* MEM_DECOMMIT */
#define SYS_PAGINA_SEM_ACESSO 0x01UL        /* PAGE_NOACCESS */
#define SYS_PAGINA_LEITURA_ESCRITA 0x04UL   /* PAGE_READWRITE */
#define SYS_TIPO_ARQUIVO_CHAR 0x0002UL      /* FILE_TYPE_CHAR */
#define SYS_NUMERO_SAIDA_PADRAO 0xFFFFFFF5UL /* STD_OUTPUT_HANDLE (-11) */
#define SYS_NUMERO_SAIDA_ERRO 0xFFFFFFF4UL   /* STD_ERROR_HANDLE (-12) */
#define SYS_PAGINA_UTF8 65001U               /* CP_UTF8 */

typedef void *SysHandle;

__declspec(dllimport) SysHandle GetStdHandle(unsigned long nivel);
__declspec(dllimport) int WriteFile(SysHandle h, const void *buf, unsigned long n, unsigned long *escritos, void *sobreposto);
__declspec(dllimport) unsigned long GetFileType(SysHandle h);
__declspec(dllimport) RT_NORETORNO void ExitProcess(unsigned int codigo);
__declspec(dllimport) SysHandle GetProcessHeap(void);
__declspec(dllimport) void *HeapAlloc(SysHandle h, unsigned long flags, uint64_t n);
__declspec(dllimport) void *HeapReAlloc(SysHandle h, unsigned long flags, void *p, uint64_t n);
__declspec(dllimport) int HeapFree(SysHandle h, unsigned long flags, void *p);
__declspec(dllimport) void *VirtualAlloc(void *endereco, uint64_t n, unsigned long tipo, unsigned long protecao);
__declspec(dllimport) int VirtualFree(void *endereco, uint64_t n, unsigned long tipo);
__declspec(dllimport) int QueryPerformanceCounter(int64_t *valor);
__declspec(dllimport) int QueryPerformanceFrequency(int64_t *valor);
__declspec(dllimport) int GetConsoleMode(SysHandle h, unsigned long *modo);
__declspec(dllimport) unsigned int GetConsoleOutputCP(void);
__declspec(dllimport) int SetConsoleOutputCP(unsigned int pagina);
__declspec(dllimport) unsigned long GetEnvironmentVariableA(const char *nome, char *destino, unsigned long n);

__declspec(dllimport) unsigned long TlsAlloc(void);
__declspec(dllimport) void *TlsGetValue(unsigned long indice);
__declspec(dllimport) int TlsSetValue(unsigned long indice, void *valor);
__declspec(dllimport) void AcquireSRWLockExclusive(void *trava);
__declspec(dllimport) void ReleaseSRWLockExclusive(void *trava);

/* O pedaco do EXCEPTION_POINTERS que interessa: o EXCEPTION_RECORD comeca pelo ExceptionCode (DWORD). */
typedef struct { unsigned long codigo; } SysRegistroExcecao;
typedef struct { SysRegistroExcecao *registro; void *contexto; } SysInfoExcecao;
__declspec(dllimport) void *AddVectoredExceptionHandler(unsigned long primeiro, long (*tratador)(SysInfoExcecao *));

/* Pagina de saida que o console tinha antes de sys_console_ligar_utf8 trocar; 0 = nada a restaurar. */
static unsigned int sys_pagina_original;

static unsigned long sys_nivel_std(int fd)
{
    return fd == 2 ? SYS_NUMERO_SAIDA_ERRO : SYS_NUMERO_SAIDA_PADRAO;
}

void sys_escrever(int fd, const void *buf, int64_t n)
{
    const unsigned char *p = (const unsigned char *)buf;
    SysHandle h = GetStdHandle(sys_nivel_std(fd));

    /* Falha de WriteFile (fd fechado, pipe quebrado) para sem erro: igual ao stdio, que ignora erro de printf. */
    while (n > 0) {
        unsigned long pedido = n > (int64_t)SYS_LOTE_ESCRITA ? (unsigned long)SYS_LOTE_ESCRITA : (unsigned long)n;
        unsigned long escritos = 0;

        if (!WriteFile(h, p, pedido, &escritos, 0) || escritos == 0)
            return;
        p += escritos;
        n -= (int64_t)escritos;
    }
}

bool sys_e_console(int fd)
{
    /* GetFileType devolve 0 (FILE_TYPE_UNKNOWN) para handle invalido: nesse caso nao e console. */
    return GetFileType(GetStdHandle(sys_nivel_std(fd))) == SYS_TIPO_ARQUIVO_CHAR;
}

static bool sys_tem_console(void)
{
    unsigned long modo = 0;

    /* GetConsoleMode so tem sucesso em handle de console de verdade; pipe, arquivo e NUL falham. */
    return GetConsoleMode(GetStdHandle(SYS_NUMERO_SAIDA_PADRAO), &modo) != 0
        || GetConsoleMode(GetStdHandle(SYS_NUMERO_SAIDA_ERRO), &modo) != 0;
}

void sys_console_ligar_utf8(void)
{
    unsigned int atual;

    if (!sys_tem_console())
        return;
    atual = GetConsoleOutputCP();
    if (atual == 0 || atual == SYS_PAGINA_UTF8)
        return;
    if (SetConsoleOutputCP(SYS_PAGINA_UTF8) != 0)
        sys_pagina_original = atual;
}

void sys_console_restaurar(void)
{
    unsigned int original = sys_pagina_original;

    sys_pagina_original = 0;
    if (original != 0)
        SetConsoleOutputCP(original);
}

_Noreturn void sys_sair(int codigo)
{
    sys_console_restaurar();
    ExitProcess((unsigned int)codigo);
}

void *sys_heap_alocar(int64_t n)
{
    return HeapAlloc(GetProcessHeap(), 0, (uint64_t)n);
}

void *sys_heap_realocar(void *p, int64_t n)
{
    return HeapReAlloc(GetProcessHeap(), 0, p, (uint64_t)n);
}

void sys_heap_liberar(void *p)
{
    HeapFree(GetProcessHeap(), 0, p);
}

void *sys_paginas_reservar(int64_t n)
{
    return VirtualAlloc(0, (uint64_t)n, SYS_MEM_RESERVAR_CONFIRMAR, SYS_PAGINA_LEITURA_ESCRITA);
}

void sys_paginas_liberar(void *p, int64_t n)
{
    /* MEM_RELEASE exige tamanho 0: libera a regiao inteira que VirtualAlloc reservou. */
    (void)n;
    VirtualFree(p, 0, SYS_MEM_LIBERAR);
}

int64_t sys_ambiente(const char *nome, char *destino, int64_t n)
{
    unsigned long copiados;

    if (n <= 0)
        return -1;
    copiados = GetEnvironmentVariableA(nome, destino, (unsigned long)n);
    /* Devolve o tamanho SEM o NUL quando coube; quando nao coube devolve o necessario COM o NUL (>= n). Ausente e 0. */
    if (copiados >= (unsigned long)n) {
        destino[0] = '\0';
        return -1;
    }
    return (int64_t)copiados;
}

static int (*sys_tratador_excecao)(unsigned long);
static int (*sys_pre_falha)(const SysFalha *); /* compartilhado: gravado 1x pela 1a alocacao com guarda, sob rt_rastreio_trava */
static bool sys_veh_ligado; /* compartilhado: idem sys_pre_falha (o forja test liga o seu numa thread so) */

/* Offsets do censo c5 (tests\nativo\m6e\CENSO.txt, por programa C compilado): EXCEPTION_RECORD.NumberParameters 24, ExceptionInformation[1] 40; CONTEXT.Rsp 152, Rbp 160, Rip 248. */
#define SYS_REG_NUM_PARAMETROS 24
#define SYS_REG_ENDERECO 40
#define SYS_CTX_RSP 152
#define SYS_CTX_RBP 160
#define SYS_CTX_RIP 248

static uint64_t sys_ler64(const void *base, int desloc)
{
    return *(const uint64_t *)((const unsigned char *)base + desloc);
}

/* O tratador vetorial do Windows: a guarda (se ligada) ve a falha antes, com os registradores do contexto; depois o tratador do runtime recebe so o codigo; 0 = EXCEPTION_CONTINUE_SEARCH. */
static long sys_excecao_despachar(SysInfoExcecao *info)
{
    if (sys_pre_falha != 0) {
        SysFalha f = {info->registro->codigo, 0, sys_ler64(info->contexto, SYS_CTX_RIP), sys_ler64(info->contexto, SYS_CTX_RSP), sys_ler64(info->contexto, SYS_CTX_RBP)};

        if (*(const unsigned long *)((const unsigned char *)info->registro + SYS_REG_NUM_PARAMETROS) >= 2)
            f.endereco = sys_ler64(info->registro, SYS_REG_ENDERECO);
        if (sys_pre_falha(&f) != 0)
            return -1; /* EXCEPTION_CONTINUE_EXECUTION: o pre-tratador so devolve 1 se resolveu a falha (a guarda sai do processo e nao volta) */
    }
    return sys_tratador_excecao != 0 ? sys_tratador_excecao(info->registro->codigo) : 0;
}

static void sys_veh_garantir(void)
{
    if (sys_veh_ligado)
        return;
    sys_veh_ligado = true;
    AddVectoredExceptionHandler(1, sys_excecao_despachar);
}

void sys_excecao_ligar(int (*tratador)(unsigned long codigo))
{
    sys_tratador_excecao = tratador;
    sys_veh_garantir();
}

void sys_falha_ligar(int (*pre)(const SysFalha *f))
{
    sys_pre_falha = pre;
    sys_veh_garantir();
}

void *sys_guarda_reservar(int64_t n)
{
    return VirtualAlloc(0, (uint64_t)n, SYS_MEM_RESERVAR, SYS_PAGINA_SEM_ACESSO);
}

bool sys_guarda_commit(void *p, int64_t n)
{
    return VirtualAlloc(p, (uint64_t)n, SYS_MEM_CONFIRMAR, SYS_PAGINA_LEITURA_ESCRITA) != 0;
}

void sys_guarda_decommit(void *p, int64_t n)
{
    VirtualFree(p, (uint64_t)n, SYS_MEM_DESCONFIRMAR);
}

bool sys_guarda_limite_mapas(void)
{
    return false;
}

uint32_t sys_tls_alocar(void)
{
    return TlsAlloc();
}

void *sys_tls_ler(uint32_t indice)
{
    return TlsGetValue(indice);
}

bool sys_tls_gravar(uint32_t indice, void *valor)
{
    return TlsSetValue(indice, valor) != 0;
}

volatile uint8_t rt_multithread; /* compartilhado: flag pegajoso, so vai de 0 para 1 (sys_multithread_ligar) */

void sys_multithread_ligar(void)
{
    rt_multithread = 1;
}

void sys_trava_pegar_srw(SysTrava *t)
{
    AcquireSRWLockExclusive(t);
}

void sys_trava_soltar_srw(SysTrava *t)
{
    ReleaseSRWLockExclusive(t);
}

int64_t sys_tempo_ns(void)
{
    int64_t contador = 0;
    int64_t frequencia = 0;

    QueryPerformanceCounter(&contador);
    QueryPerformanceFrequency(&frequencia);
    /* Divide em segundos inteiros mais resto: contador * 1e9 estouraria i64 em poucas horas.
     * O resto < frequencia, e com frequencia tipica de 10 MHz o resto * 1e9 cabe em i64. */
    return (contador / frequencia) * 1000000000LL + (contador % frequencia) * 1000000000LL / frequencia;
}
