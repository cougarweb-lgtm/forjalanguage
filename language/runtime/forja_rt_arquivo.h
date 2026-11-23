/*
 * forja_rt_arquivo.h - std:arquivo no backend C: arquivo inteiro em modo binario, caminho UTF-8 (M5 P26c). Texto ASCII puro.
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M5 P28: ENOTDIR e ENAMETOOLONG = nao existe; EISDIR = sem permissao (mesma tabela do rt_arquivo_linux.c 0.2.0); no ramo POSIX, pasta aberta para leitura (fgetc + EISDIR) = sem permissao. O ramo _WIN32 nao muda.
 *   0.1.1 - caminho vazio = "nao existe" no Windows (o _wfopen daria EINVAL = "outro"; o nativo e o POSIX dao "nao existe").
 *   0.1.0 - M5 P26c: forja_arquivo_abrir_ler, _criar, _tamanho, _ler, _gravar e _fechar, com as mesmas assinaturas e o mesmo comportamento do runtime nativo (rt_arquivo.c).
 *           Windows: _wfopen (caminho UTF-8 -> UTF-16 por MultiByteToWideChar; o fopen com caminho acentuado falha no Windows) em modo "rb"/"wb", _fseeki64/_ftelli64 para o
 *           tamanho. Outros sistemas: fopen e fseek/ftell (long de 64 bits).
 *
 * Entra no .c gerado SO quando o programa importa std:arquivo (emit_c_runtime.c, depois do forja_rt.h e do forja_rt_proc.h): programas sem o modulo nao mudam um byte.
 * Modo binario: nenhuma traducao de fim de linha. Erro em codigo neutro: 1 = nao existe (ENOENT), 2 = sem permissao (EACCES, EPERM, EROFS), 3 = outro.
 * O identificador devolvido e o FILE* como inteiro de 64 bits (nunca 0). Todo tamanho e int64_t. O buffer e do programa Forja: posse e R0009 nao passam por aqui.
 * Depende do forja_rt.h (stdint, stdio, stdlib) e de errno.h.
 */
#include <errno.h>

#define FORJA_ARQ_LOTE_MAX 0x40000000LL /* 1 GiB por fread/fwrite, como o rt nativo */
#define FORJA_ARQ_NAO_EXISTE 1
#define FORJA_ARQ_SEM_PERMISSAO 2
#define FORJA_ARQ_OUTRO 3

static inline int32_t forja_arquivo_classificar(int codigo)
{
    if (codigo == ENOENT || codigo == ENOTDIR || codigo == ENAMETOOLONG)
        return FORJA_ARQ_NAO_EXISTE;
    if (codigo == EACCES || codigo == EPERM || codigo == EROFS || codigo == EISDIR)
        return FORJA_ARQ_SEM_PERMISSAO;
    return FORJA_ARQ_OUTRO;
}

#if defined(_WIN32)
/* MultiByteToWideChar sem windows.h (colidiria com nomes do programa): o rotulo liga o nome interno ao simbolo real. */
int forja_arquivo_utf8_para_larga(unsigned int pagina, unsigned long flags, const char *origem, int norigem, unsigned short *larga,
                                  int nlarga) FORJA_ROTULO_C("MultiByteToWideChar");

#define FORJA_ARQ_PAGINA_UTF8 65001u

/* fopen com caminho UTF-8: converte para UTF-16 e usa _wfopen. NULL com *erro preenchido em falha. */
static inline FILE *forja_arquivo_fopen(const char *caminho, bool gravar, int32_t *erro)
{
    int n = caminho[0] == '\0' ? 0 : forja_arquivo_utf8_para_larga(FORJA_ARQ_PAGINA_UTF8, 0, caminho, -1, NULL, 0);
    unsigned short *larga = n > 0 ? (unsigned short *)malloc((size_t)n * sizeof(unsigned short)) : NULL;
    FILE *f = NULL;
    int falha = 0;

    if (caminho[0] == '\0') { /* o _wfopen devolve EINVAL para o caminho vazio; o nativo (CreateFileW) e o POSIX dizem "nao existe" */
        *erro = FORJA_ARQ_NAO_EXISTE;
        return NULL;
    }
    if (larga == NULL || forja_arquivo_utf8_para_larga(FORJA_ARQ_PAGINA_UTF8, 0, caminho, -1, larga, n) <= 0) {
        free(larga);
        *erro = FORJA_ARQ_OUTRO;
        return NULL;
    }
    f = _wfopen((const wchar_t *)larga, gravar ? L"wb" : L"rb");
    falha = errno;
    free(larga);
    if (f == NULL)
        *erro = forja_arquivo_classificar(falha);
    return f;
}

static inline int64_t forja_arquivo_posicao_fim(FILE *f)
{
    if (_fseeki64(f, 0, SEEK_END) != 0)
        return -1;
    return (int64_t)_ftelli64(f);
}

static inline bool forja_arquivo_voltar_ao_inicio(FILE *f)
{
    return _fseeki64(f, 0, SEEK_SET) == 0;
}
#else
static inline FILE *forja_arquivo_fopen(const char *caminho, bool gravar, int32_t *erro)
{
    FILE *f = fopen(caminho, gravar ? "wb" : "rb");

    if (f == NULL) {
        *erro = forja_arquivo_classificar(errno);
        return NULL;
    }
    if (!gravar) { /* pasta abre com "rb" e so o primeiro read da EISDIR: sonda C11 pura (fileno/fstat ficam escondidos com -std=c11 na musl) */
        int primeiro;

        errno = 0;
        primeiro = fgetc(f);
        if (primeiro == EOF && ferror(f) && errno == EISDIR) {
            fclose(f);
            *erro = FORJA_ARQ_SEM_PERMISSAO;
            return NULL;
        }
        if (primeiro == EOF)
            clearerr(f);
        else
            ungetc(primeiro, f);
    }
    return f;
}

static inline int64_t forja_arquivo_posicao_fim(FILE *f)
{
    if (fseek(f, 0, SEEK_END) != 0)
        return -1;
    return (int64_t)ftell(f);
}

static inline bool forja_arquivo_voltar_ao_inicio(FILE *f)
{
    return fseek(f, 0, SEEK_SET) == 0;
}
#endif

static inline uint64_t forja_arquivo_abrir_ler(const char *caminho, int32_t *erro)
{
    return (uint64_t)(uintptr_t)forja_arquivo_fopen(caminho, false, erro);
}

static inline uint64_t forja_arquivo_criar(const char *caminho, int32_t *erro)
{
    return (uint64_t)(uintptr_t)forja_arquivo_fopen(caminho, true, erro);
}

/* Tamanho em bytes; negativo = desconhecido. A posicao volta ao inicio. */
static inline int64_t forja_arquivo_tamanho(uint64_t arquivo)
{
    FILE *f = (FILE *)(uintptr_t)arquivo;
    int64_t tamanho = forja_arquivo_posicao_fim(f);

    return forja_arquivo_voltar_ao_inicio(f) ? tamanho : -1;
}

/* Um lote de leitura (no maximo 1 GiB): quantos bytes leu, 0 no fim do arquivo ou -1 em falha. */
static inline int64_t forja_arquivo_ler(uint64_t arquivo, uint8_t *destino, int64_t quantos)
{
    FILE *f = (FILE *)(uintptr_t)arquivo;
    size_t lidos;

    if (quantos <= 0)
        return 0;
    lidos = fread(destino, 1, (size_t)(quantos > FORJA_ARQ_LOTE_MAX ? FORJA_ARQ_LOTE_MAX : quantos), f);
    if (lidos == 0 && ferror(f))
        return -1;
    return (int64_t)lidos;
}

/* Grava os quantos bytes, em lotes de 1 GiB: devolve quantos gravou (= quantos) ou -1 na primeira falha. */
static inline int64_t forja_arquivo_gravar(uint64_t arquivo, const uint8_t *origem, int64_t quantos)
{
    FILE *f = (FILE *)(uintptr_t)arquivo;
    int64_t feito = 0;

    while (feito < quantos) {
        int64_t lote = quantos - feito > FORJA_ARQ_LOTE_MAX ? FORJA_ARQ_LOTE_MAX : quantos - feito;

        if (fwrite(origem + feito, 1, (size_t)lote, f) != (size_t)lote)
            return -1;
        feito += lote;
    }
    return feito;
}

static inline int32_t forja_arquivo_fechar(uint64_t arquivo)
{
    return fclose((FILE *)(uintptr_t)arquivo) == 0 ? 0 : -1;
}
