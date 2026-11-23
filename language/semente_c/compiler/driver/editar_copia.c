/* editar_copia.c - o check de "depois" do 'forja editar', numa copia do projeto em %TEMP% (ver editar_interno.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b: a copia leva um arquivo por trecho da Fonte (o modulo de pasta tem varios), nao um por modulo.
 *            0.1.0 - IA1-I4: primeira versao. A copia leva todo arquivo de modulo do projeto (a biblioteca padrao embutida e os modulos de engine: ficam de fora) sob a menor pasta que contem todos,
 *            com o texto novo nos editados; o "forja check" roda na raiz da copia. Assim um dry-run e uma operacao desfeita nao tocam nenhum arquivo do projeto.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe>
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "cache/entradas_ler.h"
#include "driver/editar_interno.h"

/* Um arquivo de modulo a copiar: o caminho absoluto (barras invertidas) e o que o plano diz dele. */
typedef struct { char abs[EDITAR_CAMINHO_MAX]; int editado; } ArquivoDaCopia;

/* Indice da '\' que fecha a maior pasta que 'a' e 'b' dividem, olhando ate o indice 'limite' (0 = nenhuma). */
static size_t PastaComum(const char *a, const char *b, size_t limite)
{
    size_t comum = 0;

    for (size_t k = 0; k <= limite && a[k] != '\0' && b[k] != '\0'; k++) {
        if (tolower((unsigned char)a[k]) != tolower((unsigned char)b[k])) break;
        if (a[k] == '\\') comum = k;
    }
    return comum;
}

/* Os arquivos do projeto (menos os de std e engine) com o caminho absoluto; devolve quantos. Um por trecho da Fonte: o modulo de pasta tem varios. */
static uint32_t ListarArquivos(const Plano *p, ArquivoDaCopia *lista)
{
    const Compilacao *c = p->q->c;
    uint32_t n = 0, nt = c->fonte.ntrechos > 0 ? c->fonte.ntrechos : 1;

    for (uint32_t k = 0; k < nt && n < 4096; k++) {
        const char *caminho = c->fonte.ntrechos > 0 ? c->fonte.trechos[k].caminho : c->fonte.caminho;
        const Modulo *dono = &c->modulos.itens[ModulosPacoteDoArquivo(&c->modulos, k)];

        if (dono->origem == MOD_STD || dono->origem == MOD_ENGINE) continue;
        if (_fullpath(lista[n].abs, caminho, sizeof lista[n].abs) == NULL) continue;
        lista[n].editado = -1;
        for (uint32_t j = 0; j < p->narquivos; j++)
            if (strcmp(p->arquivos[j].caminho, caminho) == 0) lista[n].editado = (int)j;
        n++;
    }
    return n;
}

/* Indice da '\' que fecha a pasta que contem todos os arquivos. */
static size_t PastaRaizDaCopia(const ArquivoDaCopia *lista, uint32_t n)
{
    size_t raiz = strlen(lista[0].abs);

    for (uint32_t k = 0; k < n; k++) raiz = PastaComum(lista[0].abs, lista[k].abs, raiz);
    return raiz;
}

/* ---------- pasta temporaria ---------- */

static bool PastaTemporaria(char *pasta, size_t tam)
{
    char base[MAX_PATH];
    DWORD n = GetTempPathA(sizeof base, base);

    if (n == 0 || n >= sizeof base) return false;
    snprintf(pasta, tam, "%sforja_editar_%lu", base, (unsigned long)GetCurrentProcessId());
    while (pasta[strlen(pasta) - 1] == '\\') pasta[strlen(pasta) - 1] = '\0';
    return CreateDirectoryA(pasta, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

/* Apaga a pasta e tudo que ha dentro. */
static void RemoverArvore(const char *pasta)
{
    char padrao[EDITAR_CAMINHO_MAX], filho[EDITAR_CAMINHO_MAX];
    WIN32_FIND_DATAA dados;
    HANDLE h;

    snprintf(padrao, sizeof padrao, "%s\\*", pasta);
    h = FindFirstFileA(padrao, &dados);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(dados.cFileName, ".") == 0 || strcmp(dados.cFileName, "..") == 0) continue;
            snprintf(filho, sizeof filho, "%s\\%s", pasta, dados.cFileName);
            if ((dados.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) RemoverArvore(filho);
            else DeleteFileA(filho);
        } while (FindNextFileA(h, &dados));
        FindClose(h);
    }
    RemoveDirectoryA(pasta);
}

/* Cria as pastas que faltam entre 'base' (que ja existe) e o arquivo 'destino'. */
static void CriarPastasDe(const char *destino, size_t base)
{
    char parcial[EDITAR_CAMINHO_MAX];

    snprintf(parcial, sizeof parcial, "%s", destino);
    for (size_t k = base + 1; parcial[k] != '\0'; k++) {
        if (parcial[k] != '\\') continue;
        parcial[k] = '\0';
        CreateDirectoryA(parcial, NULL);
        parcial[k] = '\\';
    }
}

/* ---------- copia ---------- */

/* Grava no destino o texto novo do arquivo (se foi editado) ou o conteudo atual dele. false = nao conseguiu (erro em p->erro). */
static bool CopiarUm(Plano *p, const ArquivoDaCopia *a, const Buf *novos, const char *destino)
{
    char *dados = NULL;
    size_t tam = 0;
    bool ok;

    if (a->editado >= 0) return ArquivoGravar(destino, novos[a->editado].dados, novos[a->editado].tam) || PlanoErro(p, "nao consegui gravar a copia '%s'", destino);
    if (!ArquivoLerInteiro(a->abs, &dados, &tam)) return PlanoErro(p, "nao consegui ler '%s' para a copia", a->abs);
    ok = ArquivoGravar(destino, dados, tam);
    free(dados);
    return ok || PlanoErro(p, "nao consegui gravar a copia '%s'", destino);
}

/* LIMITE: parametros 6 (lista+n e um buffer; raiz+pasta dizem de onde e para onde copia; novos traz o texto editado; funcao de uso unico, struct nao ganha clareza). */
static bool CopiarTodos(Plano *p, const ArquivoDaCopia *lista, uint32_t n, size_t raiz, const char *pasta, const Buf *novos)
{
    char destino[EDITAR_CAMINHO_MAX];

    for (uint32_t k = 0; k < n; k++) {
        snprintf(destino, sizeof destino, "%s%s", pasta, lista[k].abs + raiz);
        CriarPastasDe(destino, strlen(pasta));
        if (!CopiarUm(p, &lista[k], novos, destino)) return false;
    }
    return true;
}

/* LIMITE: parametros 5 (plano e novos sao a entrada; arena, depois e prefixo sao as tres saidas da fronteira; um chamador so). */
bool EditarVerificarCopia(Plano *p, const Buf *novos, Arena *arena, ResultadoCheck *depois, const char **prefixo)
{
    ArquivoDaCopia *lista = calloc(4096, sizeof *lista);
    char pasta[EDITAR_CAMINHO_MAX], raiz_copia[EDITAR_CAMINHO_MAX], erro[EDITAR_TEXTO_MAX];
    uint32_t n;
    size_t raiz;
    bool ok;

    if (lista == NULL) FalhaInterna("memoria esgotada listando os arquivos da copia");
    n = ListarArquivos(p, lista);
    if (n == 0 || !PastaTemporaria(pasta, sizeof pasta)) {
        free(lista);
        return PlanoErro(p, "nao consegui criar a pasta temporaria para o check");
    }
    raiz = PastaRaizDaCopia(lista, n);
    if (raiz < 3) {
        RemoverArvore(pasta);
        free(lista);
        return PlanoErro(p, "os arquivos do projeto nao dividem nenhuma pasta (unidades diferentes): nao ha como conferir numa copia");
    }
    snprintf(raiz_copia, sizeof raiz_copia, "%s%s", pasta, lista[0].abs + raiz);
    ok = CopiarTodos(p, lista, n, raiz, pasta, novos);
    if (ok && !CorrigirRodarCheck(arena, raiz_copia, depois, erro, sizeof erro)) ok = PlanoErro(p, "o check depois da edicao falhou: %.300s", erro);
    RemoverArvore(pasta);
    for (char *c = pasta; *c != '\0'; c++) *c = *c == '\\' ? '/' : *c;
    snprintf(raiz_copia, sizeof raiz_copia, "%s/", pasta);
    *prefixo = ArenaCopiarTexto(arena, raiz_copia, strlen(raiz_copia));
    free(lista);
    return ok;
}
