/* nativo_suporte.c - forja build --backend x64 --suporte --json: o que o backend nativo cobre hoje, em JSON legivel por maquina.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5 P27a: suporte_versao 0.2.0; o campo "alvos" lista os alvos da tabela de alvo.c (nome, formato, o que gera e se o escritor existe); "alvo" segue sendo o padrao (x86_64-windows).
 *            0.1.0 - backend x64 P8b: NativoSuporteTabela (a tabela REGRAS_IR, dado e nao texto escrito a mao) e NativoSuportePrograma (cada X0001 do programa).
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 *
 * Formato (uma linha JSON no stdout, exit 0):
 *   tabela:   {"suporte_versao", "alvo", "alvos": [...], "regras": [{"op", "variante", "estado": "suportado"|"recusado", "marco", ["codigo": "X0001", "motivo"]}]}
 *   programa: {"suporte_versao", "alvo", "alvos": [...], "arquivo", "total", "ocorrencias": [{"codigo": "X0001", "arquivo", "linha", "coluna", "causa", "ir": {...}}]}
 *   alvos:    [{"nome": "x86_64-windows", "formato": "pe"|"elf", "gera": ["exe", "test"[, "dll"]], "escritor": true|false}]
 * Excecao deliberada a "parar no 1o erro": a varredura do programa junta TODAS as ocorrencias (o corpo de uma construcao recusada nao e varrido).
 */
#include <stdio.h>
#include <string.h>

#include "nativo/x64/alvo.h"
#include "nativo/bir/bir_interno.h"
#include "nativo/nativo.h"
#include "nativo/nativo_erro.h"
#include "nativo/nativo_json.h"

#define SUPORTE_VERSAO "0.2.0"

static void CampoTexto(Buf *b, const char *nome, const char *valor)
{
    BufFormatar(b, "\"%s\": ", nome);
    JsonTexto(b, valor, strlen(valor));
}

/* "alvos": [...] - um registro por alvo da tabela de alvo.c, na ordem dela (sem virgula antes nem depois: quem chama poe). */
static void AnexarAlvos(Buf *b)
{
    BufAnexar(b, "\"alvos\": [");
    for (uint32_t k = 0; k < AlvoQtd(); k++) {
        const Alvo *a = AlvoPorIndice(k);

        BufAnexar(b, k > 0 ? ", {" : "{");
        CampoTexto(b, "nome", a->nome);
        BufFormatar(b, ", \"formato\": \"%s\", \"gera\": [\"exe\", \"test\"%s], \"escritor\": %s}", a->formato == IMAGEM_ELF ? "elf" : "pe",
                    a->dll ? ", \"dll\"" : "", a->sem_escritor == NULL ? "true" : "false");
    }
    BufAnexarN(b, "]", 1);
}

static void AnexarRegra(Buf *b, const BirRegra *r)
{
    bool recusado = r->estado != BR_SUPORTADO;

    BufAnexarN(b, "{", 1);
    CampoTexto(b, "op", BirNomeDoOp(r->op));
    BufAnexar(b, ", ");
    CampoTexto(b, "variante", r->variante);
    BufFormatar(b, ", \"estado\": \"%s\", \"marco\": \"%s\"", recusado ? "recusado" : "suportado", r->marco);
    if (recusado)
        BufFormatar(b, ", \"codigo\": \"X0001\", \"motivo\": \"%s %s ainda nao suportado no backend nativo (marco %s)\"", BirNomeDoOp(r->op),
                    r->variante, r->marco);
    BufAnexarN(b, "}", 1);
}

int NativoSuporteTabela(void)
{
    Buf saida = {0};

    BufFormatar(&saida, "{\"suporte_versao\": \"%s\", \"alvo\": \"%s\", ", SUPORTE_VERSAO, NATIVO_ALVO_PADRAO);
    AnexarAlvos(&saida);
    BufAnexar(&saida, ", \"regras\": [");
    for (uint32_t k = 0; k < REGRAS_IR_QTD; k++) {
        if (k > 0)
            BufAnexar(&saida, ", ");
        AnexarRegra(&saida, &REGRAS_IR[k]);
    }
    BufAnexar(&saida, "]}\n");
    fputs(saida.dados, stdout);
    fflush(stdout);
    BufLiberar(&saida);
    return 0;
}

static void AnexarOcorrencia(Buf *b, const NativoPedido *p, const BirOcorrencia *o)
{
    FontePosicao pos = FonteLocalizarNoArquivo(p->fonte, o->pos);

    BufAnexar(b, "{\"codigo\": \"X0001\", \"arquivo\": ");
    JsonCaminho(b, pos.caminho);
    BufFormatar(b, ", \"linha\": %u, \"coluna\": %u, ", (unsigned)pos.linha, (unsigned)pos.coluna);
    CampoTexto(b, "causa", o->causa);
    NativoAnexarIr(b, p, o->pos, o->causa);
    BufAnexarN(b, "}", 1);
}

int NativoSuportePrograma(const NativoPedido *pedido)
{
    BirEntrada entrada = {.ir = pedido->ir, .tipos = pedido->tipos, .nomes = pedido->nomes, .fonte = pedido->fonte, .modo = pedido->modo};
    Vec ocorrencias;
    Buf saida = {0};

    VecIniciar(&ocorrencias, sizeof(BirOcorrencia));
    BirVarrerSuporte(&entrada, &ocorrencias);
    BufFormatar(&saida, "{\"suporte_versao\": \"%s\", \"alvo\": \"%s\", ", SUPORTE_VERSAO, NATIVO_ALVO_PADRAO);
    AnexarAlvos(&saida);
    BufAnexar(&saida, ", \"arquivo\": ");
    JsonCaminho(&saida, pedido->fonte->caminho);
    BufFormatar(&saida, ", \"total\": %zu, \"ocorrencias\": [", ocorrencias.n);
    for (size_t k = 0; k < ocorrencias.n; k++) {
        if (k > 0)
            BufAnexar(&saida, ", ");
        AnexarOcorrencia(&saida, pedido, VecEm(&ocorrencias, k));
    }
    BufAnexar(&saida, "]}\n");
    fputs(saida.dados, stdout);
    fflush(stdout);
    BufLiberar(&saida);
    VecLiberar(&ocorrencias);
    return 0;
}
