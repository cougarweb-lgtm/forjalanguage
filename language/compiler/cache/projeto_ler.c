/* projeto_ler.c - leitor do manifesto forja.proj (subconjunto 0.19.0 do orquestrador de projeto): produtos, dependencias e saida (P22-O1b do M3).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M3 F9a: Dividir, NomeValido, CaminhoAbsoluto, ConferirProduto, TratarProduto, TratarSaida, LimparLinha, TratarLinha e MontarSaidas abaixo de 6 decisoes (extracao de EhBranco, PularBrancos, ConferirForma etc.); comportamento igual.
 *            0.1.0 - M3 P22-O1b: ProjetoLer (B0005 com arquivo, linha e causa), ProjetoLiberar e ProjetoProduto. Produtos ordenados por nome; dependencias resolvidas para indices.
 * Verificacao: construir.bat && teste_common.exe  (e python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "cache/projeto_ler.h"
#include "common/fonte.h"

#define LEITOR_LINHA_MAX 1024
#define LEITOR_SAIDA_PADRAO "bin"
#define LEITOR_MAX_PALAVRAS (5 + PROJETO_DEPENDE_MAX)   /* produto <nome> <tipo> <entrada> depende + as dependencias */

typedef struct {
    Projeto *projeto;
    Diag *erro;
    const char *caminho;                    /* como o usuario o escreveu: vai na causa do B0005 */
    bool saida_declarada;
    char saida_texto[PROJETO_CAMINHO_MAX];
} Leitor;

/* B0005: "<caminho> linha N: <causa> '<detalhe>'" (sem linha quando linha == 0, sem detalhe quando NULL). Sempre false, para "return Invalido(...)". */
static bool Invalido(Leitor *l, uint32_t linha, const char *causa, const char *detalhe)
{
    char onde[PROJETO_CAMINHO_MAX + 32];

    if (linha > 0)
        snprintf(onde, sizeof onde, "%s linha %u", l->caminho, linha);
    else
        snprintf(onde, sizeof onde, "%s", l->caminho);
    if (detalhe != NULL)
        DiagRelatar(l->erro, B0005, 0, "%s: %s '%s'", onde, causa, detalhe);
    else
        DiagRelatar(l->erro, B0005, 0, "%s: %s", onde, causa);
    return false;
}

static bool EhBranco(char c)
{
    return c == ' ' || c == '\t';
}

/* Passa os brancos do inicio sem alterar o texto. */
static const char *PularBrancos(const char *texto)
{
    while (EhBranco(*texto)) texto++;
    return texto;
}

/* Troca os brancos do inicio por '\0' e devolve o primeiro caractere depois deles. */
static char *ZerarBrancos(char *p)
{
    while (EhBranco(*p)) *p++ = '\0';
    return p;
}

static char *FimDaPalavra(char *p)
{
    while (*p != '\0' && !EhBranco(*p)) p++;
    return p;
}

/* Divide a linha, alterada no lugar, em palavras separadas por espaco ou tab. Devolve quantas; max + 1 se passou do maximo (a de sobra nao e guardada). */
static size_t Dividir(char *linha, char **palavras, size_t max)
{
    size_t n = 0;
    char *p = linha;

    while (*p != '\0') {
        p = ZerarBrancos(p);
        if (*p == '\0') break;
        if (n == max) return max + 1;
        palavras[n++] = p;
        p = FimDaPalavra(p);
    }
    return n;
}

static bool CaractereDeNome(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

static bool NomeValido(const char *nome)
{
    size_t tam = strlen(nome);

    if (tam == 0 || tam >= PROJETO_NOME_MAX) return false;
    for (size_t k = 0; k < tam; k++) {
        if (!CaractereDeNome(nome[k])) return false;
    }
    return true;
}

/* Indice do produto de nome 'nome' (-1 = nao existe). */
static int64_t IndiceDoProduto(const Projeto *p, const char *nome)
{
    for (size_t k = 0; k < p->produtos.n; k++) {
        if (strcmp(((const Produto *)VecEm(&p->produtos, k))->nome, nome) == 0) return (int64_t)k;
    }
    return -1;
}

static bool CaminhoJaAbsoluto(const char *resto)
{
    return resto[0] == '/' || resto[0] == '\\' || (resto[0] != '\0' && resto[1] == ':');
}

/* resto relativo a 'pasta' (ou ja absoluto) -> caminho absoluto normalizado em destino[PROJETO_CAMINHO_MAX]. false = nao coube. */
static bool CaminhoAbsoluto(const char *pasta, const char *resto, char *destino)
{
    char bruto[PROJETO_CAMINHO_MAX];
    bool absoluto = CaminhoJaAbsoluto(resto);
    int n = absoluto ? snprintf(bruto, sizeof bruto, "%s", resto) : snprintf(bruto, sizeof bruto, "%s\\%s", pasta, resto);

    return n > 0 && (size_t)n < sizeof bruto && _fullpath(destino, bruto, PROJETO_CAMINHO_MAX) != NULL;
}

static bool ArquivoExiste(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);

    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/* Forma do 'produto': palavras, 'depende' e limite de dependencias. */
static bool ConferirForma(Leitor *l, uint32_t linha, char **pal, size_t n)
{
    if (n < 4 || (n > 4 && (strcmp(pal[4], "depende") != 0 || n < 6)))
        return Invalido(l, linha, "esperado: produto <nome> <exe|dll> <entrada.fj> [depende <nome> ...]", NULL);
    if (n > LEITOR_MAX_PALAVRAS) return Invalido(l, linha, "produto depende de produtos demais", NULL);
    return true;
}

/* Nome, repeticao e tipo do 'produto'. */
static bool ConferirIdentidade(Leitor *l, uint32_t linha, char **pal)
{
    if (!NomeValido(pal[1])) return Invalido(l, linha, "nome de produto invalido (use [a-z0-9_]+)", pal[1]);
    if (IndiceDoProduto(l->projeto, pal[1]) >= 0) return Invalido(l, linha, "nome repetido", pal[1]);
    if (strcmp(pal[2], "exe") != 0 && strcmp(pal[2], "dll") != 0) return Invalido(l, linha, "tipo desconhecido (use exe ou dll)", pal[2]);
    return true;
}

static bool ConferirDependencias(Leitor *l, uint32_t linha, char **pal, size_t n)
{
    for (size_t k = 5; k < n; k++) {
        if (!NomeValido(pal[k])) return Invalido(l, linha, "nome de dependencia invalido", pal[k]);
    }
    return true;
}

/* Forma, nome, repeticao e tipo do 'produto'. */
static bool ConferirProduto(Leitor *l, uint32_t linha, char **pal, size_t n)
{
    return ConferirForma(l, linha, pal, n) && ConferirIdentidade(l, linha, pal) && ConferirDependencias(l, linha, pal, n);
}

static void PreencherProduto(Produto *pr, char **pal, size_t n, const char *entrada)
{
    snprintf(pr->nome, sizeof pr->nome, "%s", pal[1]);
    pr->tipo = strcmp(pal[2], "dll") == 0 ? PRODUTO_DLL : PRODUTO_EXE;
    snprintf(pr->entrada, sizeof pr->entrada, "%s", entrada);
    pr->ndep = n > 5 ? (uint32_t)(n - 5) : 0;     /* so ate o ResolverDependencias: aqui conta os nomes de nomes_dep */
    for (size_t k = 5; k < n; k++) snprintf(pr->nomes_dep[k - 5], sizeof pr->nomes_dep[0], "%s", pal[k]);
}

static bool TratarProduto(Leitor *l, uint32_t linha, char **pal, size_t n)
{
    char entrada[PROJETO_CAMINHO_MAX];
    Produto *pr;

    if (!ConferirProduto(l, linha, pal, n)) return false;
    if (!CaminhoAbsoluto(l->projeto->pasta, pal[3], entrada)) return Invalido(l, linha, "caminho de entrada longo demais", pal[3]);
    if (!ArquivoExiste(entrada)) return Invalido(l, linha, "entrada ausente", pal[3]);
    pr = VecEmpurrar(&l->projeto->produtos);
    PreencherProduto(pr, pal, n, entrada);
    pr->linha = linha;
    return true;
}

/* "saida = <pasta>" (o texto comeca depois da palavra saida). */
static bool TratarSaida(Leitor *l, uint32_t linha, const char *texto)
{
    texto = PularBrancos(texto);
    if (*texto != '=') return Invalido(l, linha, "esperado: saida = <pasta>", NULL);
    texto = PularBrancos(texto + 1);
    if (*texto == '\0') return Invalido(l, linha, "saida sem pasta", NULL);
    if (l->saida_declarada) return Invalido(l, linha, "saida declarada duas vezes", NULL);
    if (strlen(texto) >= sizeof l->saida_texto) return Invalido(l, linha, "pasta de saida longa demais", NULL);
    snprintf(l->saida_texto, sizeof l->saida_texto, "%s", texto);
    l->saida_declarada = true;
    return true;
}

static bool BrancoDoFim(char c)
{
    return EhBranco(c) || c == '\r';
}

/* Tira o comentario ('#') e o branco do fim; devolve o primeiro caractere util. */
static char *LimparLinha(char *texto)
{
    char *comentario = strchr(texto, '#');
    size_t tam;

    if (comentario != NULL) *comentario = '\0';
    tam = strlen(texto);
    while (tam > 0 && BrancoDoFim(texto[tam - 1])) texto[--tam] = '\0';
    while (EhBranco(*texto)) texto++;
    return texto;
}

/* A linha comeca pela palavra 'saida' (seguida de branco ou '='). */
static bool EhDeclaracaoSaida(const char *texto)
{
    return strncmp(texto, "saida", 5) == 0 && (EhBranco(texto[5]) || texto[5] == '=');
}

static bool TratarLinha(Leitor *l, uint32_t linha, char *bruto)
{
    char *texto = LimparLinha(bruto);
    char *pal[LEITOR_MAX_PALAVRAS] = {0};
    size_t n;

    if (*texto == '\0') return true;
    if (EhDeclaracaoSaida(texto)) return TratarSaida(l, linha, texto + 5);
    n = Dividir(texto, pal, LEITOR_MAX_PALAVRAS);
    if (n > LEITOR_MAX_PALAVRAS) return Invalido(l, linha, "produto depende de produtos demais", NULL);
    if (strcmp(pal[0], "produto") == 0) return TratarProduto(l, linha, pal, n);
    return Invalido(l, linha, "declaracao desconhecida", pal[0]);
}

static bool LerLinhas(Leitor *l, const Fonte *fonte)
{
    const char *p = fonte->texto;
    uint32_t linha = 1;

    while (*p != '\0') {
        const char *fim = strchr(p, '\n');
        size_t tam = fim != NULL ? (size_t)(fim - p) : strlen(p);
        char bruto[LEITOR_LINHA_MAX];

        if (tam >= sizeof bruto) return Invalido(l, linha, "linha longa demais", NULL);
        memcpy(bruto, p, tam);
        bruto[tam] = '\0';
        if (!TratarLinha(l, linha, bruto)) return false;
        if (fim == NULL) break;
        p = fim + 1;
        linha++;
    }
    return true;
}

/* Poe o indice no fim de dep[] em ordem crescente (= ordem de nome); repetido nao entra. */
static void InserirDependencia(Produto *pr, uint32_t indice)
{
    uint32_t pos = 0;

    while (pos < pr->ndep && pr->dep[pos] < indice) pos++;
    if (pos < pr->ndep && pr->dep[pos] == indice) return;
    memmove(&pr->dep[pos + 1], &pr->dep[pos], (pr->ndep - pos) * sizeof pr->dep[0]);
    pr->dep[pos] = indice;
    pr->ndep++;
}

static bool ResolverDependencias(Leitor *l)
{
    Projeto *p = l->projeto;

    for (size_t k = 0; k < p->produtos.n; k++) {
        Produto *pr = VecEm(&p->produtos, k);
        uint32_t nomes = pr->ndep;

        pr->ndep = 0;
        for (uint32_t j = 0; j < nomes; j++) {
            int64_t indice = IndiceDoProduto(p, pr->nomes_dep[j]);

            if (indice < 0) return Invalido(l, pr->linha, "depende de produto inexistente", pr->nomes_dep[j]);
            InserirDependencia(pr, (uint32_t)indice);
        }
    }
    return true;
}

static int CompararPorNome(const void *a, const void *b)
{
    return strcmp(((const Produto *)a)->nome, ((const Produto *)b)->nome);
}

/* snprintf devolveu n e coube em cap bytes (n == 0 cabe: produto exe nao tem cabecalho). */
static bool CabeEm(int n, size_t cap)
{
    return n >= 0 && (size_t)n < cap;
}

/* <saida>\<nome>.exe|.dll (+ <nome>.h na dll) de um produto. false = caminho longo demais. */
static bool MontarSaidaDoProduto(const Projeto *p, Produto *pr)
{
    const char *extensao = pr->tipo == PRODUTO_DLL ? "dll" : "exe";
    int a = snprintf(pr->saida, sizeof pr->saida, "%s\\%s.%s", p->saida, pr->nome, extensao);
    int b = pr->tipo == PRODUTO_DLL ? snprintf(pr->cabecalho, sizeof pr->cabecalho, "%s\\%s.h", p->saida, pr->nome) : 0;

    return a > 0 && CabeEm(a, sizeof pr->saida) && CabeEm(b, sizeof pr->cabecalho);
}

/* <saida>\<nome>.exe|.dll (+ <nome>.h na dll). */
static bool MontarSaidas(Leitor *l)
{
    Projeto *p = l->projeto;

    for (size_t k = 0; k < p->produtos.n; k++) {
        Produto *pr = VecEm(&p->produtos, k);

        if (!MontarSaidaDoProduto(p, pr))
            return Invalido(l, pr->linha, "caminho de saida longo demais", pr->nome);
    }
    return true;
}

/* Depois da ultima linha: pasta de saida, ordem por nome, dependencias em indices e caminhos de saida. */
static bool Finalizar(Leitor *l)
{
    Projeto *p = l->projeto;
    const char *saida = l->saida_declarada ? l->saida_texto : LEITOR_SAIDA_PADRAO;

    if (p->produtos.n == 0) return Invalido(l, 0, "nenhum produto declarado", NULL);
    if (!CaminhoAbsoluto(p->pasta, saida, p->saida)) return Invalido(l, 0, "pasta de saida longa demais", saida);
    qsort(p->produtos.dados, p->produtos.n, sizeof(Produto), CompararPorNome);
    return ResolverDependencias(l) && MontarSaidas(l);
}

/* Caminho absoluto do manifesto e a pasta dele (sem a barra final). false = nao coube. */
static bool PastaDoManifesto(const char *caminho, char *absoluto, char *pasta)
{
    char *barra;

    if (_fullpath(absoluto, caminho, PROJETO_CAMINHO_MAX) == NULL) return false;
    snprintf(pasta, PROJETO_CAMINHO_MAX, "%s", absoluto);
    barra = strrchr(pasta, '\\');
    if (barra == NULL) return false;
    *barra = '\0';
    return true;
}

bool ProjetoLer(const char *caminho, Projeto *saida, Diag *erro)
{
    Leitor l = {saida, erro, caminho, false, ""};
    char absoluto[PROJETO_CAMINHO_MAX];
    Fonte fonte;
    bool ok;

    memset(saida, 0, sizeof *saida);
    VecIniciar(&saida->produtos, sizeof(Produto));
    erro->tem = false;
    if (!PastaDoManifesto(caminho, absoluto, saida->pasta)) {
        ProjetoLiberar(saida);
        return Invalido(&l, 0, "caminho do manifesto longo demais", NULL);
    }
    if (!FonteLer(&fonte, absoluto)) {
        ProjetoLiberar(saida);
        return false;
    }
    ok = LerLinhas(&l, &fonte) && Finalizar(&l);
    FonteLiberar(&fonte);
    if (!ok) ProjetoLiberar(saida);
    return ok;
}

void ProjetoLiberar(Projeto *p)
{
    VecLiberar(&p->produtos);
}

const Produto *ProjetoProduto(const Projeto *p, uint32_t k)
{
    return VecEm(&p->produtos, k);
}
