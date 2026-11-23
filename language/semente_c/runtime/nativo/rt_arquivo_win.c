/*
 * rt_arquivo_win.c - camada sys do std:arquivo no Windows x64: arquivo inteiro em modo binario, caminho UTF-8 (M5 P26c).
 *
 * Versao: 0.1.1
 * Changelog:
 *   0.1.1 - ERROR_INVALID_NAME (123) deixa de ser "nao existe" e vira "outro", como o _wfopen do oraculo C (EINVAL).
 *   0.1.0 - M5 P26c: sys_arquivo_abrir_ler, sys_arquivo_criar, sys_arquivo_tamanho, sys_arquivo_ler, sys_arquivo_gravar e sys_arquivo_fechar sobre CreateFileW, ReadFile, WriteFile,
 *           GetFileSizeEx, CloseHandle e GetLastError (do 22o ao 26o import de kernel32, no fim da tabela de ligar.c: so entram no .idata de quem importa o modulo; WriteFile e
 *           MultiByteToWideChar ja eram imports 2 e 20). Mora na unidade do rt_win_ryu.o (rt_ryu.c), NAO no rt_sys_win.c, pelo mesmo motivo do rt_proc_win.c: mais codigo no
 *           rt_win.o mudaria os bytes de exes release sem o modulo (D-M5-11).
 *
 * Modo binario: o Windows nao traduz nada em CreateFileW/ReadFile/WriteFile. Compartilhamento leitura+escrita, como o fopen da UCRT (o oraculo C usa _wfopen). Erros: ver
 * SysArqClassificar (a mesma tabela do _dosmaperr da UCRT para os casos que o oraculo distingue, para os dois backends darem o mesmo codigo neutro).
 * Prototipos a mao, sem windows.h. Incluido por rt_ryu.c depois de rt_proc_win.c (reusa MultiByteToWideChar e RT_PROC_PAGINA_UTF8); nao compila sozinho.
 */
#include <stdint.h>

#include "rt_sys.h"

__declspec(dllimport) void *CreateFileW(const unsigned short *nome, unsigned long acesso, unsigned long compartilha, void *seguranca, unsigned long cria,
                                        unsigned long atributos, void *modelo);
__declspec(dllimport) int ReadFile(void *h, void *buf, unsigned long n, unsigned long *lidos, void *sobreposto);
__declspec(dllimport) int WriteFile(void *h, const void *buf, unsigned long n, unsigned long *escritos, void *sobreposto);
__declspec(dllimport) int GetFileSizeEx(void *h, int64_t *tamanho);
__declspec(dllimport) int CloseHandle(void *h);
__declspec(dllimport) unsigned long GetLastError(void);

#define SYS_ARQ_LER 0x80000000UL                /* GENERIC_READ */
#define SYS_ARQ_GRAVAR 0x40000000UL             /* GENERIC_WRITE */
#define SYS_ARQ_COMPARTILHA 0x3UL               /* FILE_SHARE_READ | FILE_SHARE_WRITE */
#define SYS_ARQ_ABRIR_EXISTENTE 3UL             /* OPEN_EXISTING */
#define SYS_ARQ_CRIAR_ZERAR 2UL                 /* CREATE_ALWAYS */
#define SYS_ARQ_ATRIBUTO_NORMAL 0x80UL          /* FILE_ATTRIBUTE_NORMAL */
#define SYS_ARQ_ERRO_PIPE_FECHADO 109UL         /* ERROR_BROKEN_PIPE: o fim do arquivo de um cano */
#define SYS_ARQ_INVALIDO ((void *)(intptr_t)-1) /* INVALID_HANDLE_VALUE */
#define SYS_ARQ_PTR(a) ((void *)(uintptr_t)(a))

/* Codigo do GetLastError -> codigo neutro. 2/3/15/161/206 viram ENOENT e 5/19..36 viram EACCES na UCRT (_dosmaperr); o nome invalido (123) fica de fora: na UCRT vira EINVAL. */
static int32_t SysArqClassificar(unsigned long codigo)
{
    switch (codigo) {
    case 2UL:
    case 3UL:
    case 15UL:
    case 161UL:
    case 206UL:
        return SYS_ARQ_NAO_EXISTE;
    case 5UL:
        return SYS_ARQ_SEM_PERMISSAO;
    default:
        return codigo >= 19UL && codigo <= 36UL ? SYS_ARQ_SEM_PERMISSAO : SYS_ARQ_OUTRO;
    }
}

/* UTF-8 -> UTF-16 com NUL, no heap do sistema (quem chama libera). NULL = sem memoria ou texto invalido. */
static unsigned short *SysArqCaminhoLargo(const char *caminho)
{
    int n = MultiByteToWideChar(RT_PROC_PAGINA_UTF8, 0, caminho, -1, 0, 0);
    unsigned short *larga;

    if (n <= 0)
        return 0;
    larga = sys_heap_alocar((int64_t)n * 2);
    if (larga == 0)
        return 0;
    if (MultiByteToWideChar(RT_PROC_PAGINA_UTF8, 0, caminho, -1, larga, n) <= 0) {
        sys_heap_liberar(larga);
        return 0;
    }
    return larga;
}

static uint64_t SysArqAbrir(const char *caminho, unsigned long acesso, unsigned long cria, int32_t *erro)
{
    unsigned short *larga = SysArqCaminhoLargo(caminho);
    void *h;
    unsigned long falha;

    if (larga == 0) {
        *erro = SYS_ARQ_OUTRO;
        return 0;
    }
    h = CreateFileW(larga, acesso, SYS_ARQ_COMPARTILHA, 0, cria, SYS_ARQ_ATRIBUTO_NORMAL, 0);
    falha = h == SYS_ARQ_INVALIDO ? GetLastError() : 0;
    sys_heap_liberar(larga);
    if (h == SYS_ARQ_INVALIDO) {
        *erro = SysArqClassificar(falha);
        return 0;
    }
    return (uint64_t)(uintptr_t)h;
}

uint64_t sys_arquivo_abrir_ler(const char *caminho, int32_t *erro)
{
    return SysArqAbrir(caminho, SYS_ARQ_LER, SYS_ARQ_ABRIR_EXISTENTE, erro);
}

uint64_t sys_arquivo_criar(const char *caminho, int32_t *erro)
{
    return SysArqAbrir(caminho, SYS_ARQ_GRAVAR, SYS_ARQ_CRIAR_ZERAR, erro);
}

int64_t sys_arquivo_tamanho(uint64_t arquivo)
{
    int64_t tamanho = 0;

    return GetFileSizeEx(SYS_ARQ_PTR(arquivo), &tamanho) ? tamanho : -1;
}

int64_t sys_arquivo_ler(uint64_t arquivo, void *destino, int64_t n)
{
    unsigned long lidos = 0;

    if (ReadFile(SYS_ARQ_PTR(arquivo), destino, (unsigned long)n, &lidos, 0))
        return (int64_t)lidos;
    return GetLastError() == SYS_ARQ_ERRO_PIPE_FECHADO ? 0 : -1;
}

int64_t sys_arquivo_gravar(uint64_t arquivo, const void *origem, int64_t n)
{
    const char *p = origem;
    int64_t feito = 0;

    while (feito < n) {
        unsigned long escritos = 0;

        if (!WriteFile(SYS_ARQ_PTR(arquivo), p + feito, (unsigned long)(n - feito), &escritos, 0) || escritos == 0)
            return -1;
        feito += (int64_t)escritos;
    }
    return feito;
}

int32_t sys_arquivo_fechar(uint64_t arquivo)
{
    return CloseHandle(SYS_ARQ_PTR(arquivo)) ? 0 : -1;
}
