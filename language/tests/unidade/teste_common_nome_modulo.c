/* teste_common_nome_modulo.c - verificacoes de unidade da chave por modulo (common\nome_modulo.[ch]) e do pre-passo que preenche Decl.chave (semantic\sem_chave.c): IA1-L8 P1.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L8 P1: tags, chave injetiva (db+x_y contra db_x+y), tag repetida com sufixo, nome de exibicao e Decl.chave (raiz, export e foreign interno sem prefixo).
 * Verificacao: (em language\compiler) construir.bat && teste_common.exe
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common/nome_modulo.h"
#include "semantic/semantic.h"
#include "semantic/sem_interno.h"

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok) return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (nome_modulo) linha %d: %s\n", linha, texto);
}

#define NM_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

static bool ChaveIgual(const char *tag, const char *nome, const char *esperada)
{
    char saida[128];
    size_t n = NomeModuloChave(tag, nome, strlen(nome), saida, sizeof saida);
    return n == strlen(esperada) && strcmp(saida, esperada) == 0;
}

/* db + x_y contra db_x + y: a mesma concatenacao crua "db_x_y" tem duas leituras; com o tamanho da tag as chaves diferem. */
static void TestaChaveInjetiva(const Contagem *c)
{
    char a[128], b[128];
    size_t na = NomeModuloChave("db", "x_y", 3, a, sizeof a);
    size_t nb = NomeModuloChave("db_x", "y", 1, b, sizeof b);
    NM_VERIFICA(c, na > 0 && nb > 0 && strcmp(a, b) != 0);
    NM_VERIFICA(c, ChaveIgual("db", "x_y", "2db_x_y"));
    NM_VERIFICA(c, ChaveIgual("db_x", "y", "4db_x_y"));
    NM_VERIFICA(c, ChaveIgual("json", "close", "4json_close"));
    NM_VERIFICA(c, strcmp(NomeModuloExibicao(a), "x_y") == 0);
    NM_VERIFICA(c, strcmp(NomeModuloExibicao(b), "y") == 0);
    NM_VERIFICA(c, NomeModuloChave("db", "x_y", 3, a, 5) == 0); /* nao cabe: 0 */
}

static void TestaExibicao(const Contagem *c)
{
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("4json_close"), "close") == 0);
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("close"), "close") == 0);
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("0teste_3"), "0teste_3") == 0);
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("0lista_fixa_1"), "0lista_fixa_1") == 0);
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("12abcdefghijkl_f"), "f") == 0);
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("9ab"), "9ab") == 0);      /* prefixo que nao fecha: volta inteiro */
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("2dbx"), "2dbx") == 0);    /* falta o '_' depois da tag */
    NM_VERIFICA(c, strcmp(NomeModuloExibicao("4json_"), "") == 0);
}

static void TestaTags(const Contagem *c)
{
    const char *caminhos[] = {"vendor:db", "pasta/db", "std:file", "sub/outro/db", "a/db_1", "x-y.z"};
    char tags[6][NOME_MODULO_TAG_MAX];
    NomeModuloTags(caminhos, 6, tags);
    NM_VERIFICA(c, strcmp(tags[0], "db") == 0);
    NM_VERIFICA(c, strcmp(tags[1], "db_1") == 0);       /* tag repetida: sufixo _<indice> */
    NM_VERIFICA(c, strcmp(tags[2], "file") == 0);
    NM_VERIFICA(c, strcmp(tags[3], "db_3") == 0);
    NM_VERIFICA(c, strcmp(tags[4], "db_1_4") == 0);     /* "db_1" natural ja foi tomada pelo sufixo do indice 1 */
    NM_VERIFICA(c, strcmp(tags[5], "x_y_z") == 0);      /* fora de [A-Za-z0-9_] vira '_' */
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) NM_VERIFICA(c, strcmp(tags[i], tags[j]) != 0);
    }
}

/* ---- Decl.chave pelo pre-passo (SemChavesPreencher) ---- */

typedef struct { Arena arena; Interner nomes; Modulos mods; Programa prog; Semantica sem; } Cenario;

static int32_t Internado(Cenario *k, const char *texto) { return Internar(&k->nomes, texto, (uint32_t)strlen(texto)); }

static Decl *Declarar(Cenario *k, uint8_t tipo_no, uint8_t modulo, const char *nome)
{
    Decl *d = ArenaAlocar(&k->arena, sizeof *d);
    d->tipo_no = tipo_no;
    d->modulo = modulo;
    d->nome = Internado(k, nome);
    d->chave = -7;
    d->simbolo = -1;
    if (k->prog.ultima != NULL) k->prog.ultima->prox = d;
    else k->prog.primeira = d;
    k->prog.ultima = d;
    return d;
}

static void IniciarCenario(Cenario *k)
{
    memset(k, 0, sizeof *k);
    InternerIniciar(&k->nomes, &k->arena);
    k->mods.nomes = &k->nomes;
    k->mods.n = 4;
    k->mods.itens[0] = (Modulo){.caminho = Internado(k, "raiz"), .origem = MOD_RAIZ};
    k->mods.itens[1] = (Modulo){.caminho = Internado(k, "vendor:db"), .origem = MOD_VENDOR};
    k->mods.itens[2] = (Modulo){.caminho = Internado(k, "std:os"), .origem = MOD_STD};
    k->mods.itens[3] = (Modulo){.caminho = Internado(k, "pasta/db"), .origem = MOD_ARQUIVO};
    k->sem = (Semantica){.modulos = &k->mods, .qtd_modulos = 4, .nomes = &k->nomes, .arena = &k->arena, .programa = &k->prog};
}

static bool ChaveTexto(Cenario *k, const Decl *d, const char *esperada)
{
    return strcmp(InternerTexto(&k->nomes, d->chave), esperada) == 0;
}

static void TestaDeclChave(const Contagem *c)
{
    Cenario k;
    IniciarCenario(&k);
    Decl *raiz = Declarar(&k, D_PROC, 0, "close");
    Decl *principal = Declarar(&k, D_PROC, 0, "main");
    Decl *db_close = Declarar(&k, D_PROC, 1, "close");
    Decl *db_struct = Declarar(&k, D_STRUCT, 1, "Config");
    Decl *db_export = Declarar(&k, D_PROC, 1, "abrir_banco");
    Decl *std_interno = Declarar(&k, D_PROC, 2, "forja_ler");
    Decl *std_foreign = Declarar(&k, D_PROC, 2, "puts");
    Decl *arq_close = Declarar(&k, D_PROC, 3, "close");
    Decl *teste = Declarar(&k, D_TEST, 1, "x");
    db_export->exportado = true;
    std_interno->externo = true;
    std_foreign->externo = true;
    SemChavesPreencher(&k.sem);
    NM_VERIFICA(c, raiz->chave == raiz->nome && principal->chave == principal->nome);   /* raiz e main: sem prefixo */
    NM_VERIFICA(c, ChaveTexto(&k, db_close, "2db_close"));
    NM_VERIFICA(c, ChaveTexto(&k, db_struct, "2db_Config"));
    NM_VERIFICA(c, db_export->chave == db_export->nome);                                 /* proc export: sem prefixo */
    NM_VERIFICA(c, std_interno->chave == std_interno->nome);                             /* foreign interno da std (forja_*): sem prefixo */
    NM_VERIFICA(c, ChaveTexto(&k, std_foreign, "2os_puts"));                             /* foreign de modulo: o wrapper leva a chave */
    NM_VERIFICA(c, ChaveTexto(&k, arq_close, "4db_3_close"));                            /* tag "db" repetida: o modulo 3 ganha "db_3" */
    NM_VERIFICA(c, db_close->chave != arq_close->chave && db_close->chave != raiz->chave);
    NM_VERIFICA(c, teste->chave == teste->nome);                                         /* test nao e simbolo de topo */
    ArenaLiberar(&k.arena);
}

void NomeModuloVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};
    TestaChaveInjetiva(&c);
    TestaExibicao(&c);
    TestaTags(&c);
    TestaDeclChave(&c);
}
