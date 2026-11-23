/*
 * rt_arquivo_linux.c - camada sys do std:arquivo no Linux x86-64: arquivo inteiro em modo binario por openat/fstat/read/write/close (M5 P26d).
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - forja 0.44.0: toda falha (open, pasta aberta para leitura = EISDIR 21, read, write, close) grava o errno positivo em rt_os_ultimo_erro (std:os.last_error), como o rt_es_linux.c ja
 *           fazia no file.open. Sem isso read_all/write_all saiam com last_error = 0 e o app caia num codigo do Windows. Write que devolve 0 nao grava; sucesso nao zera. A classificacao neutra nao mudou.
 *   0.2.0 - M5 P28: ENOTDIR e ENAMETOOLONG = nao existe; EISDIR = sem permissao; abrir pasta para leitura (fstat apos o open) = sem permissao. Igual ao Windows e ao forja_rt_arquivo.h 0.2.0.
 *   0.1.0 - M5 P26d: sys_arquivo_abrir_ler, sys_arquivo_criar, sys_arquivo_tamanho, sys_arquivo_ler, sys_arquivo_gravar e sys_arquivo_fechar. O caminho UTF-8 vai ao kernel como esta;
 *           nao ha traducao de fim de linha. Erros (errno -> codigo neutro) como o forja_arquivo_classificar do forja_rt_arquivo.h: ENOENT = nao existe; EACCES, EPERM e EROFS = sem
 *           permissao; o resto = outro. O identificador devolvido e fd + 1 (nunca 0, porque 0 e "falhou" no contrato de rt_sys.h e o fd 0 existe).
 *
 * Incluido por rt_linux.c depois de rt_sys_linux.c; nao compila sozinho.
 */
#include <stddef.h>

#include "rt_sys.h"

#define SYSL_AT_FDCWD (-100LL)
#define SYSL_O_SOMENTE_LEITURA 0x80000LL      /* O_RDONLY | O_CLOEXEC */
#define SYSL_O_CRIAR_ZERAR 0x80241LL          /* O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC */
#define SYSL_MODO_NOVO_ARQUIVO 0666LL         /* o umask do processo recorta */
#define SYSL_ENOENT 2LL
#define SYSL_EPERM 1LL
#define SYSL_EACCES 13LL
#define SYSL_EROFS 30LL
#define SYSL_ENOTDIR 20LL
#define SYSL_EISDIR 21LL
#define SYSL_ENAMETOOLONG 36LL
#define SYSL_S_IFDIR 0040000LL
#define SYSL_STAT_INDICE_MODO 3               /* st_mode: bytes 24..27 do struct stat (x86-64) */
#define SYSL_STAT_INDICE_TAMANHO 6            /* st_size: bytes 48..55 */
#define SYSL_STAT_PALAVRAS 18                 /* struct stat tem 144 bytes */
#define SYSL_S_IFMT 0170000LL
#define SYSL_S_IFREG 0100000LL

extern int64_t rt_os_ultimo_erro; /* rt_os_linux.c (mesmo objeto, incluido depois): o errno que std:os.last_error devolve */

/* Grava o errno positivo da chamada que falhou (retorno = -errno do kernel). */
static void sysl_arquivo_gravar_erro(int64_t retorno)
{
    rt_os_ultimo_erro = 0 - retorno;
}

/* -errno do kernel -> codigo neutro. */
static int32_t sysl_arquivo_classificar(int64_t retorno)
{
    int64_t errno_kernel = 0 - retorno;

    if (errno_kernel == SYSL_ENOENT || errno_kernel == SYSL_ENOTDIR || errno_kernel == SYSL_ENAMETOOLONG)
        return SYS_ARQ_NAO_EXISTE;
    if (errno_kernel == SYSL_EACCES || errno_kernel == SYSL_EPERM || errno_kernel == SYSL_EROFS || errno_kernel == SYSL_EISDIR)
        return SYS_ARQ_SEM_PERMISSAO;
    return SYS_ARQ_OUTRO;
}

static uint64_t sysl_arquivo_abrir(const char *caminho, int64_t flags, int32_t *erro)
{
    int64_t v[7] = {SYSL_OPENAT, SYSL_AT_FDCWD, (int64_t)caminho, flags, SYSL_MODO_NOVO_ARQUIVO, 0, 0};
    int64_t r = sysl_chamar6(v);

    if (sysl_falhou(r)) {
        sysl_arquivo_gravar_erro(r);
        *erro = sysl_arquivo_classificar(r);
        return 0;
    }
    return (uint64_t)r + 1;
}

/* Abrir pasta com O_RDONLY funciona (so o read da EISDIR): o fstat logo apos o open a recusa como o Windows (sem permissao). */
uint64_t sys_arquivo_abrir_ler(const char *caminho, int32_t *erro)
{
    int64_t st[SYSL_STAT_PALAVRAS];
    uint64_t id = sysl_arquivo_abrir(caminho, SYSL_O_SOMENTE_LEITURA, erro);

    if (id == 0)
        return 0;
    if (sysl_chamar2(SYSL_FSTAT, (int64_t)(id - 1), (int64_t)st) == 0 && (st[SYSL_STAT_INDICE_MODO] & SYSL_S_IFMT) == SYSL_S_IFDIR) {
        sysl_chamar1(SYSL_CLOSE, (int64_t)(id - 1));
        rt_os_ultimo_erro = SYSL_EISDIR;
        *erro = SYS_ARQ_SEM_PERMISSAO;
        return 0;
    }
    return id;
}

uint64_t sys_arquivo_criar(const char *caminho, int32_t *erro)
{
    return sysl_arquivo_abrir(caminho, SYSL_O_CRIAR_ZERAR, erro);
}

/* Tamanho de arquivo regular; qualquer outra coisa (pasta, dispositivo, pipe) ou falha = -1 ("nao sei", so uma dica de capacidade). */
int64_t sys_arquivo_tamanho(uint64_t arquivo)
{
    int64_t st[SYSL_STAT_PALAVRAS];

    if (sysl_chamar2(SYSL_FSTAT, (int64_t)(arquivo - 1), (int64_t)st) != 0)
        return -1;
    if ((st[SYSL_STAT_INDICE_MODO] & SYSL_S_IFMT) != SYSL_S_IFREG)
        return -1;
    return st[SYSL_STAT_INDICE_TAMANHO];
}

int64_t sys_arquivo_ler(uint64_t arquivo, void *destino, int64_t n)
{
    int64_t r;

    do {
        r = sysl_chamar3(SYSL_READ, (int64_t)(arquivo - 1), (int64_t)destino, n);
    } while (r == SYSL_EINTR);
    if (sysl_falhou(r)) {
        sysl_arquivo_gravar_erro(r);
        return -1;
    }
    return r;
}

int64_t sys_arquivo_gravar(uint64_t arquivo, const void *origem, int64_t n)
{
    const char *p = (const char *)origem;
    int64_t feito = 0;

    while (feito < n) {
        int64_t r = sysl_chamar3(SYSL_WRITE, (int64_t)(arquivo - 1), (int64_t)(p + feito), n - feito);

        if (r == SYSL_EINTR)
            continue;
        if (sysl_falhou(r)) {
            sysl_arquivo_gravar_erro(r);
            return -1;
        }
        if (r == 0)
            return -1;
        feito += r;
    }
    return feito;
}

int32_t sys_arquivo_fechar(uint64_t arquivo)
{
    int64_t r = sysl_chamar1(SYSL_CLOSE, (int64_t)(arquivo - 1));

    if (r == 0)
        return 0;
    sysl_arquivo_gravar_erro(r);
    return -1;
}
