/* nativo_entradas.c - o arquivo .entradas do build x64 (--entradas): tudo o que o build leu, com tamanho, mtime e hash (P22-O1a do M3).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - Linguagem v1.13 L1b: ColetarPacotes grava uma linha "pasta" por modulo de pasta (NativoPedido.pacotes): o hash da lista dos .fj, para o cache ver .fj acrescentado ou removido.
 *            0.2.0 - B12: ColetarCargas grava uma linha "load" por #load (NativoPedido.cargas: caminho absoluto e Hash64 do conteudo, sem BOM tirado).
 *            0.1.0 - M3 P22-O1a: NativoEntradasGravar (fj = raiz e modulos de arquivo da Fonte; lib = arquivos lidos pelo resolvedor de bibliotecas, fora do System32).
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "common/buf.h"
#include "common/fonte.h"
#include "common/hash.h"
#include "common/vec.h"
#include "nativo/nativo_entradas.h"

#define ENTRADAS_TAM_CAMINHO 1100
#define ENTRADAS_PREFIXO_STD "std:"
#define ENTRADAS_CABECALHO "# forja-entradas 1\n"

typedef struct {
    char caminho[ENTRADAS_TAM_CAMINHO];
    const char *tipo;   /* "fj" ou "lib" */
    uint64_t tamanho, mtime, hash;
} Entrada;

/* Tamanho e mtime (FILETIME, 100 ns) do arquivo; zeros se sumiu (o orquestrador re-hasheia e ve que mudou). */
static void EntradaMedirArquivo(Entrada *e)
{
    WIN32_FILE_ATTRIBUTE_DATA dados;

    e->tamanho = 0;
    e->mtime = 0;
    if (!GetFileAttributesExA(e->caminho, GetFileExInfoStandard, &dados))
        return;
    e->tamanho = ((uint64_t)dados.nFileSizeHigh << 32) | dados.nFileSizeLow;
    e->mtime = ((uint64_t)dados.ftLastWriteTime.dwHighDateTime << 32) | dados.ftLastWriteTime.dwLowDateTime;
}

/* Acrescenta a entrada com o caminho completo. false = o caminho nao coube. */
static bool EntradaAcrescentar(Vec *entradas, const char *tipo, const char *caminho, uint64_t hash)
{
    Entrada *e = VecEmpurrar(entradas);

    if (_fullpath(e->caminho, caminho, sizeof e->caminho) == NULL)
        return false;
    e->tipo = tipo;
    e->hash = hash;
    EntradaMedirArquivo(e);
    return true;
}

/* O k-esimo trecho de texto da Fonte (sem trechos = o arquivo todo). */
static FonteTrecho TrechoDaFonte(const Fonte *f, uint32_t k)
{
    if (f->ntrechos == 0)
        return (FonteTrecho){0, (uint32_t)f->tam, 0, f->caminho};
    return f->trechos[k];
}

static bool ColetarFontes(const Fonte *f, Vec *entradas)
{
    uint32_t n = f->ntrechos == 0 ? 1 : f->ntrechos;

    for (uint32_t k = 0; k < n; k++) {
        FonteTrecho t = TrechoDaFonte(f, k);

        if (strncmp(t.caminho, ENTRADAS_PREFIXO_STD, strlen(ENTRADAS_PREFIXO_STD)) == 0)
            continue;
        if (!EntradaAcrescentar(entradas, "fj", t.caminho, Hash64(f->texto + t.inicio, t.fim - t.inicio, HASH64_SEMENTE)))
            return false;
    }
    return true;
}

/* O arquivo e do Windows (%SystemRoot%\...): fica fora, para uma atualizacao do sistema nao recompilar todos os projetos. */
static bool EhDoSistema(const char *caminho)
{
    const char *raiz = getenv("SystemRoot");
    size_t tam = raiz != NULL ? strlen(raiz) : 0;

    return tam > 0 && _strnicmp(caminho, raiz, tam) == 0 && (caminho[tam] == '\\' || caminho[tam] == '/');
}

static bool ColetarLibs(const BibliotecasResolvidas *libs, Vec *entradas)
{
    for (size_t k = 0; libs != NULL && k < libs->lidas.n; k++) {
        const BibliotecaLida *lida = VecEm(&libs->lidas, k);

        if (EhDoSistema(lida->caminho))
            continue;
        if (!EntradaAcrescentar(entradas, "lib", lida->caminho, lida->hash))
            return false;
    }
    return true;
}

/* B12: cada #load lido vira uma linha "load" (o caminho ja e absoluto, vindo do driver). */
static bool ColetarCargas(const Vec *cargas, Vec *entradas)
{
    for (size_t k = 0; cargas != NULL && k < cargas->n; k++) {
        const NativoCarga *carga = VecEm(cargas, k);

        if (!EntradaAcrescentar(entradas, "load", carga->caminho, carga->hash))
            return false;
    }
    return true;
}

/* L1b: cada pasta de modulo de pasta vira uma linha "pasta" (caminho absoluto; o hash e o da lista dos nomes dos .fj que a carga viu): .fj novo ou removido na pasta muda a linha. */
static bool ColetarPacotes(const Vec *pacotes, Vec *entradas)
{
    for (size_t k = 0; pacotes != NULL && k < pacotes->n; k++) {
        const NativoPacote *pacote = VecEm(pacotes, k);

        if (!EntradaAcrescentar(entradas, "pasta", pacote->caminho, pacote->hash))
            return false;
    }
    return true;
}

static int CompararEntradas(const void *a, const void *b)
{
    return strcmp(((const Entrada *)a)->caminho, ((const Entrada *)b)->caminho);
}

/* Texto do arquivo: cabecalho e uma linha por entrada, em ordem de caminho; o mesmo arquivo lido duas vezes (link repetido) sai uma vez. */
static void MontarTexto(Vec *entradas, Buf *texto)
{
    BufAnexar(texto, ENTRADAS_CABECALHO);
    qsort(entradas->dados, entradas->n, sizeof(Entrada), CompararEntradas);
    for (size_t k = 0; k < entradas->n; k++) {
        const Entrada *e = VecEm(entradas, k);

        if (k > 0 && CompararEntradas(e, VecEm(entradas, k - 1)) == 0)
            continue;
        BufFormatar(texto, "%s\t%s\t%llu\t%llu\t%016llx\n", e->tipo, e->caminho, (unsigned long long)e->tamanho, (unsigned long long)e->mtime,
                    (unsigned long long)e->hash);
    }
}

bool NativoEntradasGravar(const NativoPedido *pedido, const BibliotecasResolvidas *libs, int *errno_saida)
{
    Vec entradas;
    Buf texto = {0};
    bool ok;

    VecIniciar(&entradas, sizeof(Entrada));
    ok = ColetarFontes(pedido->fonte, &entradas) && ColetarLibs(libs, &entradas) && ColetarCargas(pedido->cargas, &entradas) && ColetarPacotes(pedido->pacotes, &entradas);
    if (!ok && errno_saida != NULL)
        *errno_saida = ENAMETOOLONG;
    if (ok) {
        MontarTexto(&entradas, &texto);
        ok = ArquivoGravarAtomico(pedido->entradas, texto.dados, texto.tam, errno_saida);
    }
    BufLiberar(&texto);
    VecLiberar(&entradas);
    return ok;
}
