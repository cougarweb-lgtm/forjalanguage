/* diag_spec.c - o campo "spec" do diagnostico (ver diag_spec.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao. Sai do diag_pacote.c a decisao de spec x spec_ref (antes o spec era a forma_certa do catalogo, so no diagnostico com sugestao).
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/diag_spec.h"

#include <string.h>

#include "common/diag_pacote.h"
#include "common/spec_doc.h"
#include "common/spec_exemplos.h"

static bool PrimeiraVezDoCodigo;
static bool CodigoJaVisto[DIAG_QTD];

void DiagSpecIniciarLinha(DiagCodigo codigo)
{
    PrimeiraVezDoCodigo = !CodigoJaVisto[codigo];
    CodigoJaVisto[codigo] = true;
}

/* Acrescenta o texto ao fim de saida sem passar de cap - 1 bytes. */
static void Acrescentar(char *saida, size_t cap, const char *texto, size_t tam)
{
    size_t usado = strlen(saida);

    if (tam > cap - 1 - usado) tam = cap - 1 - usado;
    memcpy(saida + usado, texto, tam);
    saida[usado + tam] = '\0';
}

/* "\nexemplo certo:\n<linhas do conserto>" quando o exemplo da bateria tem conserto verificado. */
static void AcrescentarExemploCerto(const char *codigo, char *saida, size_t cap)
{
    SpecExemplo e;
    DocTexto resto, linha;
    const char *separador = "\nexemplo certo:\n";

    if (!SpecExemploDoCodigo(codigo, &e) || e.certo.tam == 0) return;
    resto = e.certo;
    while (SpecProximaLinha(&resto, &linha)) {
        size_t pular = linha.tam >= 2 ? 2 : linha.tam;

        Acrescentar(saida, cap, separador, strlen(separador));
        Acrescentar(saida, cap, linha.texto + pular, linha.tam - pular);
        separador = "\n";
    }
}

void DiagSpecTexto(const char *codigo, char *saida, size_t cap)
{
    char secao[SPEC_NOME_MAX];
    char trecho[SPEC_TRECHO_MAX + 8];

    saida[0] = '\0';
    if (!SpecSecaoDoCodigo(codigo, secao) || SpecTrechoDoCodigo(codigo, secao, trecho, sizeof trecho) == 0) {
        Acrescentar(saida, cap, "forja spec", 10);
        return;
    }
    Acrescentar(saida, cap, secao, strlen(secao));
    Acrescentar(saida, cap, ": ", 2);
    Acrescentar(saida, cap, trecho, strlen(trecho));
    AcrescentarExemploCerto(codigo, saida, cap);
}

void DiagSpecImprimirCompleto(DiagCodigo codigo, FILE *saida)
{
    char texto[DIAG_SPEC_MAX];

    DiagSpecTexto(DiagCodigoTexto(codigo), texto, sizeof texto);
    fputs(", \"spec\": ", saida);
    ImprimirTextoJson(saida, texto);
}

void DiagSpecImprimir(DiagCodigo codigo, FILE *saida)
{
    char ref[64];

    if (PrimeiraVezDoCodigo) {
        DiagSpecImprimirCompleto(codigo, saida);
        return;
    }
    snprintf(ref, sizeof ref, "forja explicar %s", DiagCodigoTexto(codigo));
    fputs(", \"spec_ref\": ", saida);
    ImprimirTextoJson(saida, ref);
}
