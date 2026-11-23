/* cmd_fmt_interno.h - tipos e funcoes compartilhados entre cmd_fmt.c, cmd_fmt_arquivo.c e cmd_fmt_json.c (o 'forja fmt' em pastas e arquivos). Nao e API: quem chama o comando usa cmd_fmt.h.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - F1 P1b: primeira versao (pedido, totais, execucao, estado do arquivo e as funcoes de um arquivo e do documento JSON).
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_CMD_FMT_INTERNO_H
#define FORJA_DRIVER_CMD_FMT_INTERNO_H

#include <stdbool.h>

#include "common/buf.h"
#include "driver/fmt_nucleo.h"

/* Os argumentos ja lidos: entradas = caminhos (arquivo ou pasta) na ordem da linha de comando. */
typedef struct {
    const char **entradas;
    int nentradas;
    bool aplicar, json, stdin_padrao, regras;
} FmtPedido;

/* O que o rodape e o documento resumem. */
typedef struct {
    uint32_t arquivos, canonicos, mudariam, gravados, nao_analisados;
} FmtTotais;

/* O estado de UM arquivo na execucao: mudaria so aparece no dry-run; com --aplicar o mesmo arquivo vira gravado. */
typedef enum { FMT_ARQ_CANONICO, FMT_ARQ_MUDARIA, FMT_ARQ_GRAVADO, FMT_ARQ_NAO_ANALISADO } FmtEstadoArquivo;

/* json: os objetos de arquivo ja emitidos, separados por ",\n" (vazio enquanto nenhum arquivo passou). */
typedef struct {
    const FmtPedido *pedido;
    FmtTotais totais;
    Buf json;
} FmtExecucao;

/* Le, formata e (com --aplicar) grava um arquivo; anexa o resultado ao texto ou ao JSON da execucao. false = erro de leitura, gravacao ou verificacao (mensagem no stderr; exit 2). */
bool FmtProcessarArquivo(const char *caminho, FmtExecucao *x);

/* Anexa o objeto de um arquivo a x->json. */
void FmtJsonArquivo(Buf *json, const char *caminho, FmtEstadoArquivo estado, const FmtResultado *r);

/* Documento completo da execucao: {"arquivos": [...], "totais": {...}}, com \n no fim. */
void FmtJsonDocumento(Buf *doc, const FmtExecucao *x);

#endif
