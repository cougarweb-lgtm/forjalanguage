/* entradas_ler.c - leitura do .entradas e hash atual dos arquivos que o build leu, com o atalho tamanho+mtime (P22-O1b do M3).
 * Versao: 0.1.3
 * Changelog: 0.1.3 - Linguagem v1.13 L1b: o tipo "pasta" (a pasta de um modulo de pasta); o hash de hoje e o da lista dos .fj dela (PastaHashDeHoje).
 *            0.1.2 - B12: TipoValido aceita "load" (#load, bytes crus: so o "fj" tira o BOM).
 *            0.1.1 - M3 F9a: LerLinha (TipoValido) e EntradaHashAtual (AtalhoValido, TemBomDoFj) abaixo de 6 decisoes; comportamento igual.
 *            0.1.0 - M3 P22-O1b: ArquivoLerInteiro, ArquivoMedir, EntradasLer, EntradaHashAtual (BOM do .fj fora do hash; atalho so com mtime < inicio do build).
 * Verificacao: construir.bat && teste_common.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "cache/entradas_ler.h"
#include "common/hash.h"
#include "common/pasta_fj.h"

#define ENTRADAS_CABECALHO "# forja-entradas 1\n"
#define ENTRADAS_CAMPOS 5
#define ENTRADAS_ARQUIVO_MAX 0x7fffffffLL     /* maior arquivo que o orquestrador le inteiro */

/* Tamanho do arquivo aberto e volta ao inicio; -1 se nao mediu ou passa de ENTRADAS_ARQUIVO_MAX. */
static long long MedirEVoltar(FILE *arq)
{
    long long total = (_fseeki64(arq, 0, SEEK_END) == 0) ? _ftelli64(arq) : -1;

    if (total < 0 || total > ENTRADAS_ARQUIVO_MAX || _fseeki64(arq, 0, SEEK_SET) != 0) return -1;
    return total;
}

bool ArquivoLerInteiro(const char *caminho, char **dados, size_t *tam)
{
    FILE *arq = fopen(caminho, "rb");
    long long total;
    char *bloco;

    if (arq == NULL) return false;
    total = MedirEVoltar(arq);
    bloco = total >= 0 ? malloc((size_t)total + 1) : NULL;
    if (bloco == NULL || fread(bloco, 1, (size_t)total, arq) != (size_t)total) {
        free(bloco);
        fclose(arq);
        return false;
    }
    fclose(arq);
    bloco[total] = '\0';
    *dados = bloco;
    *tam = (size_t)total;
    return true;
}

bool ArquivoMedir(const char *caminho, uint64_t *tamanho, uint64_t *mtime)
{
    WIN32_FILE_ATTRIBUTE_DATA dados;

    if (!GetFileAttributesExA(caminho, GetFileExInfoStandard, &dados)) return false;
    *tamanho = ((uint64_t)dados.nFileSizeHigh << 32) | dados.nFileSizeLow;
    *mtime = ((uint64_t)dados.ftLastWriteTime.dwHighDateTime << 32) | dados.ftLastWriteTime.dwLowDateTime;
    return true;
}

/* Divide a linha por TAB em exatamente ENTRADAS_CAMPOS campos (alterada no lugar). false = campos a mais ou a menos. */
static bool DividirTabs(char *linha, char **campo)
{
    size_t n = 0;

    campo[n++] = linha;
    for (char *p = linha; *p != '\0'; p++) {
        if (*p != '\t') continue;
        if (n == ENTRADAS_CAMPOS) return false;
        *p = '\0';
        campo[n++] = p + 1;
    }
    return n == ENTRADAS_CAMPOS;
}

/* Numero inteiro sem sinal, todo o texto, na base dada. */
static bool Numero(const char *texto, int base, uint64_t *valor)
{
    char *fim;

    if (*texto == '\0') return false;
    *valor = strtoull(texto, &fim, base);
    return *fim == '\0';
}

static bool TipoValido(const char *tipo)
{
    return strcmp(tipo, "fj") == 0 || strcmp(tipo, "lib") == 0 || strcmp(tipo, "load") == 0 || strcmp(tipo, "pasta") == 0;
}

static bool LerLinha(char *linha, EntradaGravada *e)
{
    char *campo[ENTRADAS_CAMPOS];

    if (!DividirTabs(linha, campo)) return false;
    if (!TipoValido(campo[0])) return false;
    if (strlen(campo[1]) >= sizeof e->caminho) return false;
    snprintf(e->tipo, sizeof e->tipo, "%s", campo[0]);
    snprintf(e->caminho, sizeof e->caminho, "%s", campo[1]);
    return Numero(campo[2], 10, &e->tamanho) && Numero(campo[3], 10, &e->mtime) && Numero(campo[4], 16, &e->hash);
}

/* Linhas do texto (depois do cabecalho) para o Vec; false na 1a linha torta. */
static bool LerLinhasDoTexto(char *p, Vec *linhas)
{
    while (*p != '\0') {
        char *fim = strchr(p, '\n');

        if (fim == NULL) return false;
        *fim = '\0';
        if (!LerLinha(p, VecEmpurrar(linhas))) return false;
        p = fim + 1;
    }
    return true;
}

bool EntradasLer(const char *caminho, Vec *linhas)
{
    char *texto;
    size_t tam;
    bool ok;

    VecIniciar(linhas, sizeof(EntradaGravada));
    if (!ArquivoLerInteiro(caminho, &texto, &tam)) return false;
    ok = strncmp(texto, ENTRADAS_CABECALHO, strlen(ENTRADAS_CABECALHO)) == 0 && LerLinhasDoTexto(texto + strlen(ENTRADAS_CABECALHO), linhas);
    free(texto);
    if (!ok) {
        VecLiberar(linhas);
        VecIniciar(linhas, sizeof(EntradaGravada));
    }
    return ok;
}

/* Tamanho e mtime iguais aos gravados e mtime anterior ao inicio do build: o hash gravado vale. */
static bool AtalhoValido(const EntradaGravada *e, uint64_t tamanho, uint64_t mtime, uint64_t inicio_build)
{
    return tamanho == e->tamanho && mtime == e->mtime && mtime < inicio_build;
}

/* .fj que comeca com BOM UTF-8. */
static bool TemBomDoFj(const EntradaGravada *e, const char *dados, size_t tam)
{
    return strcmp(e->tipo, "fj") == 0 && tam >= 3 && memcmp(dados, "\xEF\xBB\xBF", 3) == 0;
}

bool EntradaHashAtual(const EntradaGravada *e, uint64_t inicio_build, uint64_t *hash)
{
    uint64_t tamanho, mtime;
    char *dados;
    size_t tam, pula = 0;

    if (!ArquivoMedir(e->caminho, &tamanho, &mtime)) return false;
    if (strcmp(e->tipo, "pasta") == 0) { /* L1b: a lista dos .fj da pasta de hoje (sem atalho: o mtime da pasta nao diz que arquivo entrou) */
        *hash = PastaHashDeHoje(e->caminho);
        return true;
    }
    if (AtalhoValido(e, tamanho, mtime, inicio_build)) {
        *hash = e->hash;
        return true;
    }
    if (!ArquivoLerInteiro(e->caminho, &dados, &tam)) return false;
    if (TemBomDoFj(e, dados, tam)) pula = 3;
    *hash = Hash64(dados + pula, tam - pula, HASH64_SEMENTE);
    free(dados);
    return true;
}
