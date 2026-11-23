/*
 * rt_legado.c - o que uma biblioteca estatica de terceiros (.a de gcc, .lib de MSVC) pede e a UCRT nao exporta, numa unidade propria do runtime nativo (M3b P23f).
 *
 * Versao: 0.3.1
 * Changelog:
 *   0.3.1 - M3b P23y-2: opendir(NULL) devolve NULL (desreferenciava o ponteiro nulo); o comentario dos imports lista os de verdade (eram so dois).
 *   0.3.0 - M3b P23g: printf, fprintf, vprintf, vfprintf (sobre __stdio_common_vfprintf) e snprintf; strcasecmp (_stricmp) e stat64i32 (_stat64i32); hypotf, sincosf e __popcountdi2 (libgcc);
 *           opendir, readdir e closedir sobre FindFirstFileA/FindNextFileA/FindClose com a struct dirent do mingw-w64 (d_name em +8). Fecham raylib, box2d e miniaudio no x64 sem msvcrt nem libmingwex.
 *   0.2.0 - M3b P23f: vsnprintf sobre __stdio_common_vsprintf com _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR (fecha com NUL e devolve o tamanho exigido, o contrato do C99); o libcurl.a
 *           (curl_version) pede vsnprintf e a ucrtbase nao o exporta. Mesma camada fina e mesma CRT do sprintf/sscanf.
 *   0.1.0 - M3b P23f: forja_entrada_ctor e forja_entrada_dll_ctor (as entradas do exe e da DLL quando um membro de estatica tem construtor: rodam a tabela forja_ctors_ini..forja_ctors_fim que o
 *           ligador monta e seguem na forja_entrada / forja_entrada_dll); stubs da toolchain (___chkstk_ms do gcc; __chkstk, _fltused, __security_cookie, __security_check_cookie e __GSHandlerCheck do MSVC) e a camada fina de
 *           sprintf e sscanf sobre __stdio_common_vsprintf e __stdio_common_vsscanf (a API documentada da ucrtbase, a tecnica do legacy_stdio_definitions.lib; a ucrtbase
 *           NAO exporta sprintf nem sscanf). Uma so CRT: nada de msvcrt.dll nem de libmingwex. O construir.bat compila esta unidade UMA vez, em rt_win_legado.o (flags de
 *           release do runtime); o forja.exe a embute uma vez e o ligador a junta ao programa so quando um membro de estatica pede um dos simbolos dela.
 *
 * Importa, pelas tabelas de import do ligador: da ucrtbase.dll __stdio_common_vsprintf, __stdio_common_vsscanf, __stdio_common_vfprintf, __acrt_iob_func, _stricmp, _stat64i32,
 * hypot, sinf, cosf, malloc e free; da kernel32.dll FindFirstFileA, FindNextFileA e FindClose (opendir, readdir e closedir). Sem estado mutavel.
 * Os stubs sao asm de topo de arquivo, cada um numa secao propria (.text$nome): o ligador descarta o que ninguem alcanca.
 */
#ifndef RT_ALVO_WIN
#error "rt_legado.c so compila para Windows: use -DRT_ALVO_WIN"
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define RT_LEGADO_PRINTF_TERMINA_COM_NUL 1ULL /* _CRT_INTERNAL_PRINTF_LEGACY_VSPRINTF_NULL_TERMINATION: sprintf sempre fecha o texto com NUL */
#define RT_LEGADO_PRINTF_SNPRINTF_PADRAO 2ULL /* _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR: vsnprintf fecha com NUL e devolve o tamanho exigido */
#define RT_LEGADO_PRINTF_PADRAO 0ULL         /* as opcoes iniciais do _CRT_INTERNAL_LOCAL_PRINTF_OPTIONS (printf/fprintf) */
#define RT_LEGADO_STDOUT 1U                  /* indice do stdout em __acrt_iob_func */
#define RT_LEGADO_SCANF_PADRAO 0ULL         /* as opcoes iniciais do _CRT_INTERNAL_LOCAL_SCANF_OPTIONS */
#define RT_LEGADO_SEM_LIMITE ((size_t)-1)     /* sprintf e sscanf nao sabem o tamanho do buffer */
#define RT_LEGADO_CONTINUAR_BUSCA 1           /* ExceptionContinueSearch */
#define RT_LEGADO_COOKIE 0x00002B992DDFA232ULL /* o valor inicial do cookie do MSVC (nunca muda aqui) */
#define RT_LEGADO_FLTUSED 0x9875
#define RT_LEGADO_DLL_ATTACH 1U                /* DLL_PROCESS_ATTACH */

/* ---- stubs da toolchain ---- */

/* ___chkstk_ms (gcc): rax = bytes que o prologo vai reservar; toca uma vez cada pagina de 4 KiB abaixo do rsp e preserva TODOS os registradores (rax e rcx inclusive).
 * __chkstk (MSVC): rax = bytes; sonda ate rsp-rax pelo limite da pilha da thread (gs:[0x10]) e preserva tudo menos rax e o flags; o rsp volta igual (quem chama faz o sub). */
__asm__(
    ".section .text$___chkstk_ms,\"xr\"\n"
    ".globl ___chkstk_ms\n"
    "___chkstk_ms:\n"
    "    pushq %rcx\n"
    "    pushq %rax\n"
    "    cmpq $0x1000, %rax\n"
    "    leaq 24(%rsp), %rcx\n"
    "    jb 2f\n"
    "1:  subq $0x1000, %rcx\n"
    "    orq $0, (%rcx)\n"
    "    subq $0x1000, %rax\n"
    "    cmpq $0x1000, %rax\n"
    "    ja 1b\n"
    "2:  subq %rax, %rcx\n"
    "    orq $0, (%rcx)\n"
    "    popq %rax\n"
    "    popq %rcx\n"
    "    ret\n"
    ".section .text$__chkstk,\"xr\"\n"
    ".globl __chkstk\n"
    "__chkstk:\n"
    "    subq $16, %rsp\n"
    "    movq %r10, (%rsp)\n"
    "    movq %r11, 8(%rsp)\n"
    "    xorq %r11, %r11\n"
    "    leaq 24(%rsp), %r10\n"
    "    subq %rax, %r10\n"
    "    cmovbq %r11, %r10\n"
    "    movq %gs:0x10, %r11\n"
    "    cmpq %r11, %r10\n"
    "    jae 4f\n"
    "    andw $0xf000, %r10w\n"
    "3:  leaq -0x1000(%r11), %r11\n"
    "    movb $0, (%r11)\n"
    "    cmpq %r11, %r10\n"
    "    jne 3b\n"
    "4:  movq (%rsp), %r10\n"
    "    movq 8(%rsp), %r11\n"
    "    addq $16, %rsp\n"
    "    ret\n"
    ".section .text$__security_check_cookie,\"xr\"\n"
    ".globl __security_check_cookie\n"
    "__security_check_cookie:\n"
    "    cmpq __security_cookie(%rip), %rcx\n"
    "    jne 5f\n"
    "    ret\n"
    "5:  movl $2, %ecx\n"
    "    int $0x29\n"
    ".text\n");

/* O cookie do /GS do MSVC: o valor fixo (sem inicializacao em tempo de execucao); __security_check_cookie derruba o processo (fastfail 2) se o do quadro mudou. */
const uint64_t __security_cookie = RT_LEGADO_COOKIE;

/* _fltused: o MSVC exige o simbolo em quem usa ponto flutuante; o valor nao importa. */
const int _fltused = RT_LEGADO_FLTUSED;

/* Manipulador de excecao de /GS (campo "language specific handler" do .xdata): nao trata nada, o desenrolar segue para o proximo. */
int __GSHandlerCheck(void *registro, void *frame, void *contexto, void *dispatcher)
{
    (void)registro;
    (void)frame;
    (void)contexto;
    (void)dispatcher;
    return RT_LEGADO_CONTINUAR_BUSCA;
}

/* ---- construtores de terceiros ---- */

typedef void (*RtLegadoConstrutor)(void);

/* Definidos pelo ligador no objeto dos membros de estatica (terceiros_ctors.c): a tabela de ponteiros dos .CRT$XC* e .ctors, ja na ordem de execucao. */
extern const RtLegadoConstrutor forja_ctors_ini[];
extern const RtLegadoConstrutor forja_ctors_fim[];

_Noreturn void forja_entrada(void);
int forja_entrada_dll(void *instancia, uint32_t motivo, void *reservado);

static void rt_legado_construir(void)
{
    for (const RtLegadoConstrutor *p = forja_ctors_ini; p < forja_ctors_fim; p++)
        (*p)();
}

/* Entrada do exe quando um membro de estatica tem construtor (o ligador a escolhe no lugar da forja_entrada): roda os construtores e segue como a forja_entrada, antes do fj_main. */
_Noreturn void forja_entrada_ctor(void)
{
    rt_legado_construir();
    forja_entrada();
}

/* O mesmo na DLL: os construtores rodam no DLL_PROCESS_ATTACH, antes do resto do DllMain da Forja. */
int forja_entrada_dll_ctor(void *instancia, uint32_t motivo, void *reservado)
{
    if (motivo == RT_LEGADO_DLL_ATTACH)
        rt_legado_construir();
    return forja_entrada_dll(instancia, motivo, reservado);
}

/* ---- sprintf, vsnprintf e sscanf sobre a ucrtbase ---- */

__declspec(dllimport) int __stdio_common_vsprintf(uint64_t opcoes, char *destino, size_t tamanho, const char *formato, void *locale, va_list argumentos);
__declspec(dllimport) int __stdio_common_vsscanf(uint64_t opcoes, const char *entrada, size_t tamanho, const char *formato, void *locale, va_list argumentos);

/* sprintf do C: escreve em destino (sem limite de tamanho), fecha com NUL e devolve o numero de caracteres (-1 em erro), como o legacy_stdio_definitions. */
/* lint-ok: c-funcoes-inseguras e a DEFINICAO do sprintf que o terceiro (cJSON) chama; o tamanho do destino e problema do chamador, como na libc */
int sprintf(char *destino, const char *formato, ...)
{
    va_list argumentos;
    int escritos;

    va_start(argumentos, formato);
    escritos = __stdio_common_vsprintf(RT_LEGADO_PRINTF_TERMINA_COM_NUL, destino, RT_LEGADO_SEM_LIMITE, formato, NULL, argumentos);
    va_end(argumentos);
    return escritos < 0 ? -1 : escritos;
}

/* vsnprintf do C99: escreve ate tamanho-1 caracteres, fecha com NUL e devolve o tamanho que o texto inteiro exigiria (-1 em erro), como o vsnprintf do stdio.h da UCRT. */
int vsnprintf(char *destino, size_t tamanho, const char *formato, va_list argumentos)
{
    int exigidos = __stdio_common_vsprintf(RT_LEGADO_PRINTF_SNPRINTF_PADRAO, destino, tamanho, formato, NULL, argumentos);

    return exigidos < 0 ? -1 : exigidos;
}

/* sscanf do C: le de entrada (texto terminado em NUL) e devolve o numero de campos lidos (EOF antes do primeiro campo), como o legacy_stdio_definitions. */
int sscanf(const char *entrada, const char *formato, ...)
{
    va_list argumentos;
    int lidos;

    va_start(argumentos, formato);
    lidos = __stdio_common_vsscanf(RT_LEGADO_SCANF_PADRAO, entrada, RT_LEGADO_SEM_LIMITE, formato, NULL, argumentos);
    va_end(argumentos);
    return lidos;
}

/* ---- printf e familia sobre a ucrtbase (P23g) ---- */

__declspec(dllimport) int __stdio_common_vfprintf(uint64_t opcoes, void *arquivo, const char *formato, void *locale, va_list argumentos);
__declspec(dllimport) void *__acrt_iob_func(unsigned int indice);

/* vfprintf do C: escreve no FILE dado e devolve o numero de caracteres (-1 em erro). O stdout do programa que liga uma estatica e o FILE stdout do UCRT (rt_saida.c, variante CRT): a ordem com o print se mantem. */
int vfprintf(void *arquivo, const char *formato, va_list argumentos)
{
    int escritos = __stdio_common_vfprintf(RT_LEGADO_PRINTF_PADRAO, arquivo, formato, NULL, argumentos);

    return escritos < 0 ? -1 : escritos;
}

int fprintf(void *arquivo, const char *formato, ...)
{
    va_list argumentos;
    int escritos;

    va_start(argumentos, formato);
    escritos = vfprintf(arquivo, formato, argumentos);
    va_end(argumentos);
    return escritos;
}

int vprintf(const char *formato, va_list argumentos)
{
    return vfprintf(__acrt_iob_func(RT_LEGADO_STDOUT), formato, argumentos);
}

int printf(const char *formato, ...)
{
    va_list argumentos;
    int escritos;

    va_start(argumentos, formato);
    escritos = vprintf(formato, argumentos);
    va_end(argumentos);
    return escritos;
}

/* snprintf do C99: o vsnprintf acima com os argumentos variaveis (tamanho exigido, fecha com NUL). */
int snprintf(char *destino, size_t tamanho, const char *formato, ...)
{
    va_list argumentos;
    int exigidos;

    va_start(argumentos, formato);
    exigidos = vsnprintf(destino, tamanho, formato, argumentos);
    va_end(argumentos);
    return exigidos;
}

/* ---- nomes antigos e matematica que a ucrtbase tem com outro nome ou nao tem ---- */

__declspec(dllimport) int _stricmp(const char *a, const char *b);
__declspec(dllimport) int _stat64i32(const char *caminho, void *destino);
__declspec(dllimport) double hypot(double x, double y);
__declspec(dllimport) float sinf(float x);
__declspec(dllimport) float cosf(float x);

/* strcasecmp (POSIX) e a _stricmp da UCRT: compara sem distinguir caixa. */
int strcasecmp(const char *a, const char *b)
{
    return _stricmp(a, b);
}

/* stat64i32: o nome sem sublinhado do _stat64i32 (struct _stat64i32 e a mesma que o stat do mingw-w64 usa). */
int stat64i32(const char *caminho, void *destino)
{
    return _stat64i32(caminho, destino);
}

/* hypotf: o hypot em double arredondado para float (a precisao do float sai exata). */
float hypotf(float x, float y)
{
    return (float)hypot((double)x, (double)y);
}

/* sincosf: seno e cosseno de uma vez, nos dois ponteiros. */
void sincosf(float x, float *seno, float *cosseno)
{
    *seno = sinf(x);
    *cosseno = cosf(x);
}

/* __popcountdi2 (libgcc): quantos bits 1 tem o inteiro de 64 bits (algoritmo de Kernighan: um bit por volta). */
int __popcountdi2(int64_t valor)
{
    uint64_t resto = (uint64_t)valor;
    int bits = 0;

    for (; resto != 0; resto &= resto - 1)
        bits++;
    return bits;
}

/* ---- opendir, readdir e closedir sobre FindFirstFileA (P23g) ---- */

#define RT_LEGADO_NOME_MAX 260          /* MAX_PATH: tamanho de d_name */
#define RT_LEGADO_SEM_BUSCA ((void *)-1) /* INVALID_HANDLE_VALUE */
#define RT_LEGADO_PADRAO_EXTRA 3         /* "\\*" e o NUL */

/* WIN32_FIND_DATAA do Windows (320 bytes). */
typedef struct {
    uint32_t atributos;
    uint32_t tempos[6]; /* 3 FILETIME de 2 x 32 bits (alinhados em 4, como no Windows; uint64_t empurraria o resto 4 bytes) */
    uint32_t tamanho_alto, tamanho_baixo, reservado0, reservado1;
    char nome[RT_LEGADO_NOME_MAX];
    char nome_curto[14];
} RtLegadoAchado;
_Static_assert(sizeof(RtLegadoAchado) == 320, "WIN32_FIND_DATAA tem 320 bytes");

/* struct dirent do mingw-w64 (d_name em +8, o que o rcore.c do raylib le). */
struct dirent {
    int32_t d_ino;
    uint16_t d_reclen;
    uint16_t d_namlen;
    char d_name[RT_LEGADO_NOME_MAX];
};

/* DIR: a entrada devolvida, a busca aberta e o primeiro achado (que o FindFirstFileA ja trouxe) ainda nao entregue. */
typedef struct {
    struct dirent entrada;
    void *busca;
    int achado_pendente;
    RtLegadoAchado achado;
} RtLegadoDir;

__declspec(dllimport) void *FindFirstFileA(const char *padrao, RtLegadoAchado *achado);
__declspec(dllimport) int FindNextFileA(void *busca, RtLegadoAchado *achado);
__declspec(dllimport) int FindClose(void *busca);
__declspec(dllimport) void *malloc(size_t tamanho);
__declspec(dllimport) void free(void *memoria);

/* Copia ate tamanho-1 caracteres de origem para destino, fecha com NUL e devolve quantos copiou. */
static size_t rt_legado_copiar(const char *origem, char *destino, size_t tamanho)
{
    size_t n = 0;

    for (; origem[n] != '\0' && n < tamanho - 1; n++)
        destino[n] = origem[n];
    destino[n] = '\0';
    return n;
}

/* Escreve "<caminho>\*" em padrao (RT_LEGADO_NOME_MAX bytes); falso se nao cabe. */
static int rt_legado_padrao(const char *caminho, char *padrao)
{
    size_t n = rt_legado_copiar(caminho, padrao, RT_LEGADO_NOME_MAX - RT_LEGADO_PADRAO_EXTRA + 1);

    if (caminho[n] != '\0')
        return 0;
    if (n > 0 && padrao[n - 1] != '\\' && padrao[n - 1] != '/')
        padrao[n++] = '\\';
    padrao[n++] = '*';
    padrao[n] = '\0';
    return 1;
}

/* opendir: abre a listagem de um diretorio (NULL se o caminho e NULL, grande demais ou nao existe). */
RtLegadoDir *opendir(const char *caminho)
{
    char padrao[RT_LEGADO_NOME_MAX];
    RtLegadoDir *dir;

    if (caminho == NULL || !rt_legado_padrao(caminho, padrao) || (dir = malloc(sizeof *dir)) == NULL)
        return NULL;
    dir->busca = FindFirstFileA(padrao, &dir->achado);
    if (dir->busca == RT_LEGADO_SEM_BUSCA) {
        free(dir);
        return NULL;
    }
    dir->achado_pendente = 1;
    return dir;
}

/* readdir: a proxima entrada (o ponteiro vale ate a proxima chamada) ou NULL no fim. */
struct dirent *readdir(RtLegadoDir *dir)
{
    if (dir == NULL || (!dir->achado_pendente && !FindNextFileA(dir->busca, &dir->achado)))
        return NULL;
    dir->achado_pendente = 0;
    dir->entrada.d_namlen = (uint16_t)rt_legado_copiar(dir->achado.nome, dir->entrada.d_name, RT_LEGADO_NOME_MAX);
    dir->entrada.d_reclen = (uint16_t)sizeof dir->entrada;
    dir->entrada.d_ino = 0;
    return &dir->entrada;
}

/* closedir: fecha a busca e libera o DIR (0 em sucesso, -1 sem DIR). */
int closedir(RtLegadoDir *dir)
{
    if (dir == NULL)
        return -1;
    FindClose(dir->busca);
    free(dir);
    return 0;
}
