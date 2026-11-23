/* pasta_fj.c - a lista dos .fj de uma pasta e o hash dela (ver pasta_fj.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.13 L1b: primeira versao (ListarFj do driver\carga_pacote.c 0.1.0, mais PastaHashNomes e PastaHashDeHoje).
 * Verificacao: construir.bat && python ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "common/hash.h"
#include "common/pasta_fj.h"

enum { PASTA_CAMINHO_MAX = 1300 };

static bool TemExtensaoFj(const char *nome)
{
    size_t n = strlen(nome);
    return n > 3 && _stricmp(nome + n - 3, ".fj") == 0;
}

/* Ordem de byte do caminho (a pasta e a mesma: vale o nome do arquivo). */
static int ComparaCaminhos(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

void PastaListarFj(Arena *arena, const char *pasta, Vec *caminhos)
{
    char padrao[PASTA_CAMINHO_MAX + 4], completo[PASTA_CAMINHO_MAX + MAX_PATH + 2]; /* pasta + "\*" e pasta + "\" + nome */
    WIN32_FIND_DATAA achado;

    snprintf(padrao, sizeof padrao, "%s\\*", pasta); /* o filtro "*.fj" do sistema casa tambem nome curto 8.3: a extensao se confere abaixo */
    HANDLE busca = FindFirstFileA(padrao, &achado);
    if (busca == INVALID_HANDLE_VALUE) return;
    do {
        if ((achado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 || !TemExtensaoFj(achado.cFileName)) continue;
        int n = snprintf(completo, sizeof completo, "%s\\%s", pasta, achado.cFileName);
        if (n < 0 || (size_t)n >= sizeof completo) continue;
        const char **slot = VecEmpurrar(caminhos);
        *slot = ArenaCopiarTexto(arena, completo, (size_t)n);
    } while (FindNextFileA(busca, &achado));
    FindClose(busca);
    qsort(caminhos->dados, caminhos->n, sizeof(const char *), ComparaCaminhos);
}

/* O nome do arquivo: o que vem depois da ultima barra. */
static const char *NomeDoArquivo(const char *caminho)
{
    const char *nome = caminho;

    for (const char *p = caminho; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') nome = p + 1;
    }
    return nome;
}

uint64_t PastaHashNomes(const Vec *caminhos)
{
    uint64_t hash = HASH64_SEMENTE;

    for (size_t i = 0; i < caminhos->n; i++) {
        const char *nome = NomeDoArquivo(*(const char *const *)VecEm(caminhos, i));

        hash = Hash64(nome, strlen(nome), hash);
        hash = Hash64("\n", 1, hash);
    }
    return hash;
}

uint64_t PastaHashDeHoje(const char *pasta)
{
    Arena arena = {0};
    Vec caminhos;
    uint64_t hash;

    VecIniciar(&caminhos, sizeof(const char *));
    PastaListarFj(&arena, pasta, &caminhos);
    hash = PastaHashNomes(&caminhos);
    VecLiberar(&caminhos);
    ArenaLiberar(&arena);
    return hash;
}
