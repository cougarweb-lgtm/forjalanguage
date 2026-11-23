/* estado.c - estado incremental do orquestrador de projeto, chave de cache do produto e motivo de recompilar (P22-O1b do M3).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - M3 F9a: LerLinha (LerTextos, LerNumeros) e EstadoMotivo (MotivoFixo) abaixo de 6 decisoes; comportamento igual.
 *            0.1.1 - M3 F8: EstadoGravarAtomico grava sem FlushFileBuffers/WRITE_THROUGH (ArquivoGravarAtomicoSemDescarga); continua temporario + rename.
 *            0.1.0- M3 P22-O1b: EstadoLer/EstadoGravarAtomico (8 colunas), EstadoAchar, texto canonico + EstadoChave/EstadoAmbiente/EstadoDepsTexto, EstadoMotivo.
 * Verificacao: construir.bat && teste_common.exe  (e python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache/entradas_ler.h"
#include "cache/estado.h"
#include "common/fonte.h"
#include "common/hash.h"

#define ESTADO_CABECALHO "# forja-estado 1\n"
#define ESTADO_COLUNAS 8

/* ---------- leitura e gravacao ---------- */

/* Divide a linha por TAB em exatamente ESTADO_COLUNAS campos (alterada no lugar). */
static bool DividirColunas(char *linha, char **campo)
{
    size_t n = 0;

    campo[n++] = linha;
    for (char *p = linha; *p != '\0'; p++) {
        if (*p != '\t') continue;
        if (n == ESTADO_COLUNAS) return false;
        *p = '\0';
        campo[n++] = p + 1;
    }
    return n == ESTADO_COLUNAS;
}

static bool Numero(const char *texto, int base, uint64_t *valor)
{
    char *fim;

    if (*texto == '\0') return false;
    *valor = strtoull(texto, &fim, base);
    return *fim == '\0';
}

/* Cabe no destino (com o NUL)? Copia. */
static bool Copiar(char *destino, size_t tam, const char *origem)
{
    if (strlen(origem) >= tam) return false;
    snprintf(destino, tam, "%s", origem);
    return true;
}

static bool LerInterface(const char *texto, EstadoLinha *l)
{
    l->tem_interface = strcmp(texto, "-") != 0;
    l->hash_interface = 0;
    return !l->tem_interface || Numero(texto, 16, &l->hash_interface);
}

static bool LerTextos(char **campo, EstadoLinha *l)
{
    const char *deps = strcmp(campo[7], "-") == 0 ? "" : campo[7];

    return Copiar(l->nome, sizeof l->nome, campo[0]) && Copiar(l->saida, sizeof l->saida, campo[3]) && Copiar(l->deps, sizeof l->deps, deps);
}

static bool LerNumeros(char **campo, EstadoLinha *l)
{
    return Numero(campo[1], 16, &l->chave) && LerInterface(campo[2], l) && Numero(campo[4], 10, &l->tamanho) && Numero(campo[5], 10, &l->mtime) &&
           Numero(campo[6], 16, &l->ambiente);
}

static bool LerLinha(char *linha, EstadoLinha *l)
{
    char *campo[ESTADO_COLUNAS];

    return DividirColunas(linha, campo) && LerTextos(campo, l) && LerNumeros(campo, l);
}

/* Linha torta sai do estado (vira "primeira vez"): o pior caso e recompilar. */
static void LerLinhasDoTexto(char *p, Estado *e)
{
    while (*p != '\0') {
        char *fim = strchr(p, '\n');

        if (fim != NULL) *fim = '\0';
        if (!LerLinha(p, VecEmpurrar(&e->linhas))) e->linhas.n--;
        if (fim == NULL) return;
        p = fim + 1;
    }
}

bool EstadoLer(const char *caminho, Estado *e)
{
    uint64_t tamanho, mtime;
    char *texto;
    size_t tam;

    VecIniciar(&e->linhas, sizeof(EstadoLinha));
    if (!ArquivoMedir(caminho, &tamanho, &mtime)) return true;
    if (!ArquivoLerInteiro(caminho, &texto, &tam)) return false;
    if (strncmp(texto, ESTADO_CABECALHO, strlen(ESTADO_CABECALHO)) == 0) LerLinhasDoTexto(texto + strlen(ESTADO_CABECALHO), e);
    free(texto);
    return true;
}

static void AnexarLinha(Buf *texto, const EstadoLinha *l)
{
    char interface[24];

    if (l->tem_interface)
        snprintf(interface, sizeof interface, "%016llx", (unsigned long long)l->hash_interface);
    else
        snprintf(interface, sizeof interface, "-");
    BufFormatar(texto, "%s\t%016llx\t%s\t%s\t%llu\t%llu\t%016llx\t%s\n", l->nome, (unsigned long long)l->chave, interface, l->saida, (unsigned long long)l->tamanho,
                (unsigned long long)l->mtime, (unsigned long long)l->ambiente, l->deps[0] != '\0' ? l->deps : "-");
}

bool EstadoGravarAtomico(const char *caminho, const Estado *e, int *errno_saida)
{
    Buf texto = {0};
    bool ok;

    BufAnexar(&texto, ESTADO_CABECALHO);
    for (size_t k = 0; k < e->linhas.n; k++) AnexarLinha(&texto, VecEm(&e->linhas, k));
    /* sem descarga em disco: o estado torto/vazio depois de uma queda e descartado por EstadoLer e o produto so recompila */
    ok = ArquivoGravarAtomicoSemDescarga(caminho, texto.dados, texto.tam, errno_saida);
    BufLiberar(&texto);
    return ok;
}

const EstadoLinha *EstadoAchar(const Estado *e, const char *nome)
{
    for (size_t k = 0; k < e->linhas.n; k++) {
        const EstadoLinha *l = VecEm(&e->linhas, k);

        if (strcmp(l->nome, nome) == 0) return l;
    }
    return NULL;
}

void EstadoLiberar(Estado *e)
{
    VecLiberar(&e->linhas);
}

/* ---------- chave ---------- */

static void TextoDoCabecalho(const ChavePedido *p, Buf *texto)
{
    BufFormatar(texto, "forja=%016llx\nalvo=%s\nmodo=%s\nnivel=%u\ntipo=%s\nentrada=%s\nlib-pasta=%s\n", (unsigned long long)p->forja, p->alvo, p->modo,
                (unsigned)p->nivel, p->tipo, p->entrada, p->lib_pasta);
}

void EstadoTextoCanonico(const ChavePedido *p, Buf *texto)
{
    TextoDoCabecalho(p, texto);
    for (size_t k = 0; k < p->ndeps; k++) {
        if (p->deps[k].tem)
            BufFormatar(texto, "dep:%s=%016llx\n", p->deps[k].nome, (unsigned long long)p->deps[k].hash);
        else
            BufFormatar(texto, "dep:%s=-\n", p->deps[k].nome);
    }
    for (size_t k = 0; k < p->narquivos; k++)
        BufFormatar(texto, "%s=%016llx\n", p->arquivos[k].caminho, (unsigned long long)p->arquivos[k].hash);
}

uint64_t EstadoChave(const ChavePedido *p)
{
    Buf texto = {0};
    uint64_t chave;

    EstadoTextoCanonico(p, &texto);
    chave = Hash64(texto.dados, texto.tam, HASH64_SEMENTE);
    BufLiberar(&texto);
    return chave;
}

uint64_t EstadoAmbiente(const ChavePedido *p)
{
    Buf texto = {0};
    uint64_t ambiente;

    TextoDoCabecalho(p, &texto);
    ambiente = Hash64(texto.dados, texto.tam, HASH64_SEMENTE);
    BufLiberar(&texto);
    return ambiente;
}

void EstadoDepsTexto(const ChavePedido *p, char *destino, size_t tam)
{
    Buf texto = {0};

    for (size_t k = 0; k < p->ndeps; k++) {
        const EstadoDep *d = &p->deps[k];

        if (d->tem)
            BufFormatar(&texto, "%s%s=%016llx", k > 0 ? "," : "", d->nome, (unsigned long long)d->hash);
        else
            BufFormatar(&texto, "%s%s=-", k > 0 ? "," : "", d->nome);
    }
    snprintf(destino, tam, "%s", p->ndeps > 0 ? texto.dados : "-");
    BufLiberar(&texto);
}

/* ---------- motivo ---------- */

static bool Escrever(char *texto, size_t n, const char *fixo)
{
    snprintf(texto, n, "%s", fixo);
    return true;
}

/* Motivo de texto fixo, na ordem de prioridade; "" = nao recompila (chave igual e saida ok); NULL = o motivo depende de dep/arquivo mudado. */
static const char *MotivoFixo(const MotivoPedido *p)
{
    if (p->forcado) return "forcado (--tudo)";
    if (p->primeira_vez) return "primeira vez";
    if (p->chave_igual) return p->saida_ok ? "" : "saida ausente ou alterada";
    if (!p->ambiente_igual) return "forja.exe, modo ou opcoes mudaram";
    return NULL;
}

bool EstadoMotivo(const MotivoPedido *p, char *texto, size_t n)
{
    const char *fixo = MotivoFixo(p);

    if (fixo != NULL) return fixo[0] != '\0' && Escrever(texto, n, fixo);
    if (p->dep_mudou[0] != '\0') {
        snprintf(texto, n, "interface de %s mudou", p->dep_mudou);
        return true;
    }
    if (p->arquivo_mudou[0] != '\0') {
        snprintf(texto, n, "%s mudou: %s", p->arquivo_e_lib ? "biblioteca" : "fonte", p->arquivo_mudou);
        return true;
    }
    return Escrever(texto, n, "forja.exe, modo ou opcoes mudaram");
}
