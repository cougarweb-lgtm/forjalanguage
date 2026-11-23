/*
 * rt_perfil_linux.c - a parte Linux x86-64 do perfil (M8 P1, D-M8-5/D-M8-6): id da thread, abrir/gravar/fechar o arquivo de saida, por syscalls cruas.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M8 P1: primeira versao. gettid 186 (nova na lista fechada do D-M5-3, auditar_runtime 0.8.0), openat 257, write 1 e close 3; o caminho-base e FORJA_PERFIL_SAIDA
 *           (sys_ambiente) ou o argv[0] do programa (sys_argumentos; o runtime nao le /proc).
 *
 * Incluido por rt_perfil.c (que o escolhe por RT_ALVO_LINUX); nao compila sozinho.
 */
#include "rt_syscall_linux.h"

#define PERFIL_CAMINHO_MAX 2048
#define PERFIL_AT_FDCWD (-100LL)
#define PERFIL_ABRIR_GRAVAR 0x241LL /* O_WRONLY | O_CREAT | O_TRUNC */
#define PERFIL_MODO_ARQUIVO 0644LL

/* Id da thread atual (gettid). So e chamado com mais de uma thread (rt_multithread) e uma vez na partida. */
static uint32_t perfil_so_tid(void)
{
    return (uint32_t)sysl_chamar1(SYSL_GETTID, 0);
}

/* Copia o texto (com NUL) para destino[usado..]; devolve o novo usado ou -1 se nao cabe em PERFIL_CAMINHO_MAX. */
static int64_t perfil_so_anexar(char *destino, int64_t usado, const char *texto)
{
    int64_t i;

    for (i = 0; texto[i] != '\0'; i++) {
        if (usado + i + 1 >= PERFIL_CAMINHO_MAX)
            return -1;
        destino[usado + i] = texto[i];
    }
    destino[usado + i] = '\0';
    return usado + i;
}

/* A base do caminho: FORJA_PERFIL_SAIDA ou argv[0]. Devolve o tamanho gravado em caminho (sem NUL) ou -1. */
static int64_t perfil_so_base(char *caminho)
{
    int64_t n = sys_ambiente("FORJA_PERFIL_SAIDA", caminho, PERFIL_CAMINHO_MAX - 16);
    int64_t argc = 0;
    char **argv = NULL;

    if (n > 0)
        return n;
    sys_argumentos(&argc, &argv);
    if (argc < 1 || argv == NULL)
        return -1;
    return perfil_so_anexar(caminho, 0, argv[0]);
}

/* Abre (cria ou zera) "<base><sufixo>" para gravar. 0 = falhou. O fd vai somado de 1 (0 = falha, como o handle do Windows). */
static uintptr_t perfil_so_abrir(const char *sufixo)
{
    char caminho[PERFIL_CAMINHO_MAX];
    int64_t usado = perfil_so_base(caminho);
    int64_t v[7] = {SYSL_OPENAT, PERFIL_AT_FDCWD, (int64_t)caminho, PERFIL_ABRIR_GRAVAR, PERFIL_MODO_ARQUIVO, 0, 0};
    int64_t fd;

    if (usado < 0)
        return 0;
    usado = perfil_so_anexar(caminho, usado, sufixo);
    if (usado < 0)
        return 0;
    fd = sysl_chamar6(v);
    return sysl_falhou(fd) ? 0 : (uintptr_t)fd + 1u;
}

/* Grava os n bytes (repete ate sair tudo e refaz com EINTR); false em falha. */
static bool perfil_so_gravar(uintptr_t arquivo, const void *dados, int64_t n)
{
    const uint8_t *p = (const uint8_t *)dados;
    int64_t r;

    while (n > 0) {
        r = sysl_chamar3(SYSL_WRITE, (int64_t)(arquivo - 1u), (int64_t)p, n);
        if (r == SYSL_EINTR)
            continue;
        if (sysl_falhou(r) || r == 0)
            return false;
        p += r;
        n -= r;
    }
    return true;
}

static void perfil_so_fechar(uintptr_t arquivo)
{
    sysl_chamar1(SYSL_CLOSE, (int64_t)(arquivo - 1u));
}
