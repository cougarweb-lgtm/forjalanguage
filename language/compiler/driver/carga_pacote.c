/* carga_pacote.c - carga do modulo de pasta (ver carga_pacote.h): lista os .fj da pasta, anexa cada um como trecho da Fonte e o analisa como parte do mesmo modulo.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b: a lista dos .fj sai para common\pasta_fj.c (PastaListarFj, a mesma do cache) e a pasta carregada vai para Compilacao.pastas_carregadas (linha "pasta" do .entradas).
 *            0.1.0 - Linguagem v1.13 L1a: primeira versao (E1001 de pasta sem .fj; E1008 de pasta e arquivo com o mesmo caminho e de arquivo que entraria duas vezes).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "common/pasta_fj.h"
#include "driver/carga_pacote.h"

enum { CAMINHO_MAX = 1300 };

static bool EhPasta(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);
    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool EhArquivo(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);
    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool MesmoArquivo(const char *a, const char *b)
{
    char absoluto_a[CAMINHO_MAX], absoluto_b[CAMINHO_MAX];
    if (_fullpath(absoluto_a, a, sizeof absoluto_a) == NULL || _fullpath(absoluto_b, b, sizeof absoluto_b) == NULL) return false;
    return _stricmp(absoluto_a, absoluto_b) == 0;
}

bool CargaJaEntrou(const Compilacao *c, const char *caminho)
{
    const Fonte *f = &c->fonte;

    if (f->ntrechos == 0) return MesmoArquivo(f->caminho, caminho);
    for (uint32_t k = 0; k < f->ntrechos; k++) {
        if (MesmoArquivo(f->trechos[k].caminho, caminho)) return true;
    }
    return false;
}

bool CargaRelatarRepetido(Compilacao *c, const Modulo *mod, const char *caminho)
{
    return DiagRelatar(&c->diag, E1008, mod->pos_import,
                       "o arquivo '%s' entraria duas vezes no programa (o import \"%s\" e outro import trazem o mesmo arquivo); importe a pasta ou o arquivo, nao os dois",
                       caminho, InternerTexto(&c->nomes, mod->caminho));
}

/* Anexa o arquivo da pasta como trecho e o analisa (lexico e parser) como parte do modulo k. false = diagnostico em c->diag. */
static bool CarregarArquivoDaPasta(Compilacao *c, uint32_t k, const char *caminho)
{
    const Modulo *mod = &c->modulos.itens[k];

    if (CargaJaEntrou(c, caminho)) return CargaRelatarRepetido(c, mod, caminho);
    if (!FonteAnexarArquivo(&c->fonte, caminho)) {
        return DiagRelatar(&c->diag, E1001, mod->pos_import, "o arquivo '%s' da pasta nao pode ser lido", caminho);
    }
    return AnalisarModulo(c, k);
}

static bool CarregarPasta(Compilacao *c, uint32_t k, const Vec *caminhos)
{
    for (size_t i = 0; i < caminhos->n; i++) {
        if (!CarregarArquivoDaPasta(c, k, *(const char *const *)VecEm(caminhos, i))) return false;
    }
    return true;
}

/* Guarda a pasta carregada (caminho absoluto e hash da lista de nomes): vai ao .entradas do build, para um .fj que entra na pasta ou sai dela invalidar o cache. */
static void GuardarPasta(Compilacao *c, const char *pasta, const Vec *caminhos)
{
    char absoluto[CAMINHO_MAX];
    NativoPacote *p;

    if (_fullpath(absoluto, pasta, sizeof absoluto) == NULL) return;
    p = VecEmpurrar(&c->pastas_carregadas);
    p->caminho = ArenaCopiarTexto(&c->arena, absoluto, strlen(absoluto));
    p->hash = PastaHashNomes(caminhos);
}

CargaResultado CargaPacote(Compilacao *c, uint32_t k, const char *caminho_fj)
{
    const Modulo *mod = &c->modulos.itens[k];
    size_t tam = strlen(caminho_fj) - 3; /* sem o ".fj" */
    Vec caminhos;
    char pasta[CAMINHO_MAX];

    if (tam >= sizeof pasta) return CARGA_SEGUE_ARQUIVO;
    memcpy(pasta, caminho_fj, tam);
    pasta[tam] = '\0';
    if (!EhPasta(pasta)) return CARGA_SEGUE_ARQUIVO;
    VecIniciar(&caminhos, sizeof(const char *));
    PastaListarFj(&c->arena, pasta, &caminhos);
    CargaResultado r = CARGA_FALHA;
    if (EhArquivo(caminho_fj)) {
        if (caminhos.n == 0) r = CARGA_SEGUE_ARQUIVO;
        else DiagRelatar(&c->diag, E1008, mod->pos_import,
                         "conflito de caminho: existem o arquivo '%s' e a pasta '%s' com .fj; o import \"%s\" nao sabe qual vale (renomeie um dos dois)",
                         caminho_fj, pasta, InternerTexto(&c->nomes, mod->caminho));
    } else if (caminhos.n == 0) {
        DiagRelatar(&c->diag, E1001, mod->pos_import, "a pasta '%s' do import \"%s\" esta sem nenhum .fj", pasta, InternerTexto(&c->nomes, mod->caminho));
    } else if (CarregarPasta(c, k, &caminhos)) {
        GuardarPasta(c, pasta, &caminhos);
        r = CARGA_PACOTE_CARREGADO;
    }
    VecLiberar(&caminhos);
    return r;
}
