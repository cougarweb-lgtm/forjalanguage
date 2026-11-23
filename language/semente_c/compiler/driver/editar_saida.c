/* editar_saida.c - a resposta JSON do 'forja editar' e o caminho relativo que ela mostra (ver editar_interno.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I4: primeira versao. Uma linha JSON, chaves em ordem fixa (a mesma entrada da os mesmos bytes), caminhos relativos a pasta da raiz com barras normais.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/editar_interno.h"
#include "nativo/nativo_json.h"

enum { DIAGNOSTICOS_MAX = 5 };

void EditarCaminhoRelativo(const Plano *p, const char *caminho, char *saida, size_t tam)
{
    char raiz[EDITAR_CAMINHO_MAX], absoluto[EDITAR_CAMINHO_MAX], *barra;
    size_t n;
    const char *origem = caminho;

    if (_fullpath(absoluto, caminho, sizeof absoluto) != NULL && _fullpath(raiz, p->q->c->fonte.caminho, sizeof raiz) != NULL) {
        barra = strrchr(raiz, '\\');
        if (barra != NULL) *barra = '\0';
        n = strlen(raiz);
        origem = _strnicmp(absoluto, raiz, n) == 0 && absoluto[n] == '\\' ? absoluto + n + 1 : absoluto;
    }
    snprintf(saida, tam, "%s", origem);
    for (char *c = saida; *c != '\0'; c++) {
        if (*c == '\\') *c = '/';
    }
}

/* O JSON cru de um diagnostico do check da copia, com a pasta da copia tirada dos caminhos. */
static void AnexarSemPrefixo(Buf *b, const char *texto, const char *prefixo)
{
    size_t n = strlen(prefixo);
    const char *achou;

    while (n > 0 && (achou = strstr(texto, prefixo)) != NULL) {
        BufAnexarN(b, texto, (size_t)(achou - texto));
        texto = achou + n;
    }
    BufAnexar(b, texto);
}

static void EmitirOperacoes(Buf *b, const Plano *p)
{
    BufAnexar(b, ", \"operacoes\": [");
    for (uint32_t k = 0; k < p->noperacoes; k++) {
        const Operacao *o = &p->operacoes[k];

        BufAnexar(b, k > 0 ? ", {\"op\": " : "{\"op\": ");
        EdicoesJsonTexto(b, o->op, strlen(o->op));
        BufAnexar(b, ", \"alvo\": ");
        EdicoesJsonTextoOuNulo(b, o->alvo);
        BufAnexar(b, ", \"novo\": ");
        EdicoesJsonTextoOuNulo(b, o->novo);
        BufFormatar(b, ", \"edicoes\": %u}", o->edicoes);
    }
    BufAnexar(b, "]");
}

static void EmitirEdicoes(Buf *b, const Plano *p)
{
    bool primeira = true;

    BufAnexar(b, ", \"edicoes\": [");
    for (uint32_t k = 0; k < p->narquivos; k++) {
        for (uint32_t j = 0; j < p->arquivos[k].n; j++) {
            if (!primeira) BufAnexar(b, ", ");
            EdicoesJsonEdicao(b, &p->arquivos[k].edicoes[j], p->arquivos[k].relativo);
            primeira = false;
        }
    }
    BufAnexar(b, "]");
}

static void EmitirDiagnosticos(Buf *b, const Resposta *r)
{
    BufAnexar(b, ", \"diagnosticos\": [");
    for (uint32_t k = 0; r->depois != NULL && k < r->depois->n && k < DIAGNOSTICOS_MAX; k++) {
        if (k > 0) BufAnexar(b, ", ");
        AnexarSemPrefixo(b, r->depois->itens[k].bruto, r->prefixo_copia != NULL ? r->prefixo_copia : "");
    }
    BufAnexar(b, "]");
}

void EditarEmitirResposta(const Plano *p, const Resposta *r)
{
    Buf b = {0};
    bool desfeito = r->depois != NULL && !r->valido;

    BufFormatar(&b, "{\"editar_versao\": \"%s\", \"comando\": \"editar\", \"alvo\": ", EDITAR_VERSAO);
    JsonCaminho(&b, r->alvo);
    BufFormatar(&b, ", \"aplicar\": %s", r->aplicar ? "true" : "false");
    EmitirOperacoes(&b, p);
    EmitirEdicoes(&b, p);
    BufAnexar(&b, ", \"diff\": ");
    EdicoesJsonTexto(&b, r->diff != NULL && r->diff->dados != NULL ? r->diff->dados : "", r->diff != NULL ? r->diff->tam : 0);
    BufAnexar(&b, ", \"erros_antes\": 0, \"erros_depois\": ");
    if (r->erros_depois < 0) BufAnexar(&b, "null");
    else BufFormatar(&b, "%lld", (long long)r->erros_depois);
    EmitirDiagnosticos(&b, r);
    BufFormatar(&b, ", \"valido\": %s, \"aplicado\": %s, \"desfeito\": %s, \"motivo\": ", r->valido ? "true" : "false", r->aplicado ? "true" : "false", desfeito ? "true" : "false");
    EdicoesJsonTextoOuNulo(&b, r->motivo);
    BufAnexar(&b, "}\n");
    fwrite(b.dados, 1, b.tam, stdout);
    BufLiberar(&b);
}
