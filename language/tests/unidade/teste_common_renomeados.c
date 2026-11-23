/* teste_common_renomeados.c - verificacoes de unidade da tabela de renomeados (common\renomeados.[ch]) e da secao RENOMEADOS da migracao.txt embutida (RenomeadosVerificacoes,
 * chamada pelo teste_common): IA1-L3a.
 * Versao: 0.7.0
 * Changelog: 0.7.0 - S3 P4 (D10): REN_LIBM fica fora de "antigo != novo" (sqrt -> sqrt e o embutido, sem renome) e de "recusar = true" (a forma de libm e codigo valido: nao e E0110).
 *            0.6.0 - S2g-json: TestaParamsDoJson (indice, valor, texto, caminho -> index, value, text, path so no escopo vendor:json/json); o caso "caminho" de TestaClassesDoL3c2 passa para um modulo que nao existe.
 *            0.5.0 - IA1-L3f: recusar = true em todas as linhas; TestaRecusa (RenomeadoRecusa e RenomeadoTolerarAntiga).
 *            0.4.0 - IA1-L3d1: TestaTrocas (RenTrocas, RenomeadoNotar, RenomeadoNotarTabela).
 *            0.3.0 - IA1-L3c2: TestaClassesDoL3c2 (RenomeadoAcharEm, REN_PARAM, REN_PACOTE, REN_MODULO de vendor).
 *            0.2.0 - IA1-L3c: TestaDuplaAceitacao (RenomeadoBuscarNovo, RenomeadoAchar com escopo, recusar false em todas).
 *            0.1.0 - IA1-L3a: campos validos (nao vazios, sem tabulacao, antigo != novo), sem antigo repetido na mesma classe e escopo, API (contagem, indice, busca, classe) e a
 *            secao da migracao.txt embutida igual a tabela, entrada por entrada e na ordem (se a tabela muda sem regerar o doc, falha aqui).
 * Verificacao: (em language\compiler) construir.bat && teste_common.exe
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common/doc_embutida.h"
#include "common/renomeados.h"

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok) return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (renomeados) linha %d: %s\n", linha, texto);
}

#define REN_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

#define CABECALHO_SECAO "classe\tescopo\tantigo\tnovo\tnota\n"
#define MARCA_SECAO "\nRENOMEADOS (Emenda 4)"

/* O campo tem texto e nenhum caractere que quebre a linha da migracao.txt. */
static bool CampoValido(const char *campo)
{
    return campo != NULL && campo[0] != '\0' && strpbrk(campo, "\t\n\r\\\"") == NULL;
}

/* Campos validos e antigo != novo em toda entrada; a classe esta na faixa e tem texto. */
static void TestaCampos(const Contagem *c)
{
    size_t invalidas = 0, iguais = 0, classes = 0;

    for (size_t k = 0; k < RenomeadosContagem(); k++) {
        const Renomeado *r = RenomeadoPorIndice(k);

        if (!CampoValido(r->escopo) || !CampoValido(r->antigo) || !CampoValido(r->novo) || !CampoValido(r->nota)) invalidas++;
        if (strcmp(r->antigo, r->novo) == 0 && r->classe != REN_LIBM) iguais++; /* S3 P4: sqrt -> sqrt (embutido) nao e renome */
        if ((unsigned)r->classe >= REN_CLASSE_QTD || strcmp(RenomeadoClasseTexto(r->classe), "?") == 0) classes++;
    }
    REN_VERIFICA(c, RenomeadosContagem() > 0);
    REN_VERIFICA(c, invalidas == 0);
    REN_VERIFICA(c, iguais == 0);
    REN_VERIFICA(c, classes == 0);
}

/* Nenhum par de entradas repete (classe, escopo, antigo). */
static void TestaSemRepeticao(const Contagem *c)
{
    size_t repetidas = 0;

    for (size_t a = 0; a < RenomeadosContagem(); a++) {
        for (size_t b = a + 1; b < RenomeadosContagem(); b++) {
            const Renomeado *x = RenomeadoPorIndice(a), *y = RenomeadoPorIndice(b);

            if (x->classe == y->classe && strcmp(x->escopo, y->escopo) == 0 && strcmp(x->antigo, y->antigo) == 0) repetidas++;
        }
    }
    REN_VERIFICA(c, repetidas == 0);
}

/* Contagem, acesso por indice, busca por (classe, antigo) e texto da classe. */
static void TestaApi(const Contagem *c)
{
    size_t n = RenomeadosContagem();
    const Renomeado *em = RenomeadoBuscar(REN_ROTULO, "em");
    const Renomeado *banco = RenomeadoBuscar(REN_TIPO, "ErroBanco");

    REN_VERIFICA(c, RenomeadoPorIndice(n) == NULL);
    REN_VERIFICA(c, RenomeadoPorIndice(0) != NULL);
    REN_VERIFICA(c, em != NULL && strcmp(em->novo, "alloc") == 0);
    REN_VERIFICA(c, banco != NULL && strcmp(banco->novo, "DbError") == 0);
    REN_VERIFICA(c, RenomeadoBuscar(REN_ROTULO, "ErroBanco") == NULL);
    REN_VERIFICA(c, RenomeadoBuscar(REN_PACOTE, "em") == NULL);
    REN_VERIFICA(c, strcmp(RenomeadoClasseTexto(REN_VALOR_ALOCADOR), "valor-alocador") == 0);
    REN_VERIFICA(c, strcmp(RenomeadoClasseTexto(REN_CLASSE_QTD), "?") == 0);
}

/* L3c: busca pelo nome novo, busca com escopo (antigo OU novo, palavra inteira do escopo) e o flag recusar (false em todas: a forma antiga vale em silencio). */
static void TestaDuplaAceitacao(const Contagem *c)
{
    bool era_antigo = true;
    size_t recusadas = 0;
    const Renomeado *novo = RenomeadoBuscarNovo(REN_ROTULO, "alloc");
    const Renomeado *por_novo = RenomeadoAchar(REN_ROTULO, "make", "alloc", &era_antigo);
    const Renomeado *por_antigo;

    REN_VERIFICA(c, novo != NULL && strcmp(novo->antigo, "em") == 0);
    REN_VERIFICA(c, por_novo == novo && !era_antigo);
    por_antigo = RenomeadoAchar(REN_ROTULO, "format_c", "em", &era_antigo);
    REN_VERIFICA(c, por_antigo == novo && era_antigo);
    REN_VERIFICA(c, RenomeadoAchar(REN_ROTULO, "format", "em", &era_antigo) == novo);
    REN_VERIFICA(c, RenomeadoAchar(REN_ROTULO, "forma", "em", &era_antigo) == NULL);
    REN_VERIFICA(c, RenomeadoAchar(REN_ROTULO, "transmute", "em", &era_antigo) == NULL);
    REN_VERIFICA(c, RenomeadoAchar(REN_ROTULO, "transmute", "para", &era_antigo) != NULL && era_antigo);
    REN_VERIFICA(c, RenomeadoAchar(REN_PROC, "vendor:db/db", "abrir", &era_antigo) != NULL);
    REN_VERIFICA(c, RenomeadoAchar(REN_PROC, "vendor:db/outro", "abrir", &era_antigo) == NULL);
    REN_VERIFICA(c, RenomeadoAchar(REN_QUALIFICADOR, NULL, "arquivo", &era_antigo) != NULL);
    REN_VERIFICA(c, RenomeadoBuscarNovo(REN_MODULO, "std:file") != NULL && RenomeadoBuscarNovo(REN_MODULO, "std:arquivo") == NULL);
    for (size_t k = 0; k < RenomeadosContagem(); k++) {
        if (!RenomeadoPorIndice(k)->recusar && RenomeadoPorIndice(k)->classe != REN_LIBM) recusadas++; /* S3 P4: libm e codigo valido, nao E0110 */
    }
    REN_VERIFICA(c, recusadas == 0);
}

/* L3f: toda forma antiga e recusada (recusar = true); o modo "tolerar forma antiga" do forja migrar desliga a recusa e volta ao desligar. */
static void TestaRecusa(const Contagem *c)
{
    const Renomeado *em = RenomeadoBuscar(REN_ROTULO, "em");

    REN_VERIFICA(c, em != NULL && RenomeadoRecusa(em));
    RenomeadoTolerarAntiga(true);
    REN_VERIFICA(c, !RenomeadoRecusa(em));
    RenomeadoTolerarAntiga(false);
    REN_VERIFICA(c, RenomeadoRecusa(em));
}

/* L3c2: escopo com mais de uma palavra (membro = modulo + enum), param por modulo inteiro, pacote por pasta e modulo por caminho inteiro. */
static void TestaClassesDoL3c2(const Contagem *c)
{
    static const char *const DB_ERRO[] = {"vendor:db/db", "DbError", NULL}, *const DB_OUTRO[] = {"vendor:db/db", "ZlibError", NULL};
    static const char *const OUTRO_MODULO[] = {"vendor:compression/compression", "DbError", NULL};
    bool era_antigo = false;
    const Renomeado *m = RenomeadoAcharEm(REN_MEMBRO, DB_ERRO, "nao_abriu", &era_antigo);

    REN_VERIFICA(c, m != NULL && era_antigo && strcmp(m->novo, "not_opened") == 0);
    REN_VERIFICA(c, RenomeadoAcharEm(REN_MEMBRO, DB_ERRO, "not_opened", &era_antigo) == m && !era_antigo);
    REN_VERIFICA(c, RenomeadoAcharEm(REN_MEMBRO, DB_OUTRO, "sem_memoria", &era_antigo) == NULL);
    REN_VERIFICA(c, RenomeadoAcharEm(REN_MEMBRO, OUTRO_MODULO, "sem_memoria", &era_antigo) == NULL);
    REN_VERIFICA(c, RenomeadoAchar(REN_PARAM, "vendor:db/db", "caminho", &era_antigo) != NULL && era_antigo);
    REN_VERIFICA(c, RenomeadoAchar(REN_PARAM, "vendor:json/outro", "caminho", &era_antigo) == NULL);
    REN_VERIFICA(c, RenomeadoAchar(REN_PACOTE, "import", "banco_servidor", &era_antigo) != NULL && era_antigo);
    REN_VERIFICA(c, RenomeadoBuscarNovo(REN_PACOTE, "physics2d") != NULL && RenomeadoBuscar(REN_PACOTE, "physics2d") == NULL);
    REN_VERIFICA(c, RenomeadoBuscar(REN_MODULO, "vendor:banco/banco") != NULL && RenomeadoBuscarNovo(REN_MODULO, "vendor:db/db") != NULL);
    REN_VERIFICA(c, strcmp(RenomeadoClasseTexto(REN_PARAM), "param") == 0);
}

/* S2g-json: os 4 rotulos de parametro do vendor:json (indice, valor, texto, caminho) entram na tabela, so no escopo do json, pelas duas formas, e sao E0110 (recusar). */
static void TestaParamsDoJson(const Contagem *c)
{
    static const char *const ANTIGOS[] = {"indice", "valor", "texto", "caminho"};
    static const char *const NOVOS[] = {"index", "value", "text", "path"};
    bool era_antigo = false;

    for (size_t k = 0; k < sizeof ANTIGOS / sizeof ANTIGOS[0]; k++) {
        const Renomeado *r = RenomeadoAchar(REN_PARAM, "vendor:json/json", ANTIGOS[k], &era_antigo);

        REN_VERIFICA(c, r != NULL && era_antigo && strcmp(r->novo, NOVOS[k]) == 0 && RenomeadoRecusa(r));
        REN_VERIFICA(c, RenomeadoAchar(REN_PARAM, "vendor:json/json", NOVOS[k], &era_antigo) == r && !era_antigo);
        REN_VERIFICA(c, RenomeadoAchar(REN_PARAM, "vendor:net/net", ANTIGOS[k], &era_antigo) == NULL);
    }
    REN_VERIFICA(c, RenomeadoAchar(REN_PROC, "vendor:json/json", "texto", &era_antigo) != NULL);
    REN_VERIFICA(c, RenomeadoAchar(REN_PARAM, "vendor:db/db", "valor", &era_antigo) != NULL);
}

/* L3d1: a lista de trocas do forja migrar: sem lista ligada em Diag.trocas nada e registrado; com ela, RenomeadoNotarTabela guarda posicao, tamanho, classe e os dois textos. */
static void TestaTrocas(const Contagem *c)
{
    Diag d = {0};
    RenTrocas reg;
    const RenTroca *t;

    RenTrocasIniciar(&reg);
    RenomeadoNotar(&d, &(RenTroca){.off = 10, .tam = 2, .classe = REN_ROTULO, .antigo = "em", .novo = "alloc"});
    REN_VERIFICA(c, reg.itens.n == 0);
    d.trocas = &reg;
    RenomeadoNotarTabela(&d, 10, 2, RenomeadoBuscar(REN_ROTULO, "em"));
    RenomeadoNotar(&d, &(RenTroca){.off = 30, .tam = 4, .classe = REN_QUALIFICADOR, .antigo = "arquivo", .novo = "file"});
    t = reg.itens.dados;
    REN_VERIFICA(c, reg.itens.n == 2 && t[0].off == 10 && t[0].tam == 2 && t[0].classe == REN_ROTULO);
    REN_VERIFICA(c, strcmp(t[0].antigo, "em") == 0 && strcmp(t[0].novo, "alloc") == 0);
    REN_VERIFICA(c, t[1].off == 30 && t[1].classe == REN_QUALIFICADOR && strcmp(t[1].novo, "file") == 0);
    RenTrocasLiberar(&reg);
}

/* A secao RENOMEADOS da migracao.txt embutida: a marca uma vez, o cabecalho e uma linha por entrada, na ordem da tabela, ate o fim do arquivo. */
static void TestaDocIgualATabela(const Contagem *c)
{
    DocTexto doc = DocEmbutida(DOC_MIGRACAO);
    const char *marca = strstr(doc.texto, MARCA_SECAO);
    const char *p = marca != NULL ? strstr(marca, CABECALHO_SECAO) : NULL;
    size_t divergentes = 0;

    REN_VERIFICA(c, marca != NULL && strstr(marca + 1, MARCA_SECAO) == NULL);
    REN_VERIFICA(c, p != NULL);
    if (p == NULL) return;
    p += strlen(CABECALHO_SECAO);
    for (size_t k = 0; k < RenomeadosContagem(); k++) {
        const Renomeado *r = RenomeadoPorIndice(k);
        char linha[512];
        int n = snprintf(linha, sizeof linha, "%s\t%s\t%s\t%s\t%s\n", RenomeadoClasseTexto(r->classe), r->escopo, r->antigo, r->novo, r->nota);

        if (n <= 0 || (size_t)n >= sizeof linha || strncmp(p, linha, (size_t)n) != 0) {
            divergentes++;
            break;
        }
        p += n;
    }
    REN_VERIFICA(c, divergentes == 0);
    REN_VERIFICA(c, *p == '\0');
}

void RenomeadosVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    TestaCampos(&c);
    TestaSemRepeticao(&c);
    TestaApi(&c);
    TestaDuplaAceitacao(&c);
    TestaClassesDoL3c2(&c);
    TestaParamsDoJson(&c);
    TestaTrocas(&c);
    TestaRecusa(&c);
    TestaDocIgualATabela(&c);
}
