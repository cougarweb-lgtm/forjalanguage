/* migrar_pasta.c - a lista de arquivos do 'forja migrar' (ver migrar_pasta.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L3d1: primeira versao.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "driver/migrar_pasta.h"

enum { CAMINHO_MAX = 1024 };

static void ListarPasta(const char *pasta, Arena *arena, Vec *lista);

static bool PastaIgnorada(const char *nome)
{
    static const char *const PREFIXOS[] = {"language_", ".git", "backup"};

    for (size_t k = 0; k < sizeof PREFIXOS / sizeof PREFIXOS[0]; k++)
        if (_strnicmp(nome, PREFIXOS[k], strlen(PREFIXOS[k])) == 0) return true;
    return false;
}

static bool TemExtensaoFj(const char *nome)
{
    size_t n = strlen(nome);

    return n > 3 && _stricmp(nome + n - 3, ".fj") == 0;
}

/* Ordem por nome sem diferenciar maiuscula; o empate (so a caixa muda) fica na ordem de strcmp, para a lista nao depender do disco. */
static int ComparaNomes(const void *a, const void *b)
{
    const char *x = ((const WIN32_FIND_DATAA *)a)->cFileName, *y = ((const WIN32_FIND_DATAA *)b)->cFileName;
    int c = _stricmp(x, y);

    return c != 0 ? c : strcmp(x, y);
}

/* "pasta\nome" (sem barra dobrada se a pasta ja termina em barra); false = nao coube. */
static bool JuntarCaminho(char *destino, size_t tam, const char *pasta, const char *nome)
{
    size_t n = strlen(pasta);
    bool barra = n > 0 && (pasta[n - 1] == '\\' || pasta[n - 1] == '/');

    return snprintf(destino, tam, barra ? "%s%s" : "%s\\%s", pasta, nome) < (int)tam;
}

static void VisitarEntrada(const char *pasta, const WIN32_FIND_DATAA *fd, Arena *arena, Vec *lista)
{
    char caminho[CAMINHO_MAX];
    bool eh_pasta = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    bool atalho = (fd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

    if (!JuntarCaminho(caminho, sizeof caminho, pasta, fd->cFileName)) return;
    if (eh_pasta) {
        if (!atalho && !PastaIgnorada(fd->cFileName)) ListarPasta(caminho, arena, lista);
        return;
    }
    if (TemExtensaoFj(fd->cFileName)) *(const char **)VecEmpurrar(lista) = ArenaCopiarTexto(arena, caminho, strlen(caminho));
}

static void ListarPasta(const char *pasta, Arena *arena, Vec *lista)
{
    char padrao[CAMINHO_MAX];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    Vec itens;

    if (!JuntarCaminho(padrao, sizeof padrao, pasta, "*")) return;
    h = FindFirstFileA(padrao, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    VecIniciar(&itens, sizeof fd);
    do {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) *(WIN32_FIND_DATAA *)VecEmpurrar(&itens) = fd;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (itens.n > 1) qsort(itens.dados, itens.n, sizeof fd, ComparaNomes);
    for (size_t k = 0; k < itens.n; k++) VisitarEntrada(pasta, VecEm(&itens, k), arena, lista);
    VecLiberar(&itens);
}

bool MigrarColetar(const char *entrada, Arena *arena, Vec *lista)
{
    DWORD atributos = GetFileAttributesA(entrada);

    if (atributos == INVALID_FILE_ATTRIBUTES) return false;
    if ((atributos & FILE_ATTRIBUTE_DIRECTORY) != 0) ListarPasta(entrada, arena, lista);
    else *(const char **)VecEmpurrar(lista) = ArenaCopiarTexto(arena, entrada, strlen(entrada));
    return true;
}
