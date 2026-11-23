/* cmd_fmt_arquivo.c - o 'forja fmt' de UM arquivo: le, formata, mostra o diff (dry-run) ou grava e confere (--aplicar). Ver cmd_fmt_interno.h.
 * Versao: 0.1.3
 * Changelog: 0.1.3 - F1 P1b: sai a guarda do diff no fim de arquivo sem '\n' (o EdicoesDiff 0.2.1 limita a ultima linha nova ao novo texto); o diff sai de verdade.
 *            0.1.2 - F1 P1b: --aplicar recusa arquivo so-leitura antes de gravar (a gravacao atomica trocava o arquivo mesmo assim); exit 2 e o arquivo intacto.
 *            0.1.1 - F1 P1b: o diff de uma edicao no fim de arquivo sem '\n' sai como aviso (o EdicoesDiff do edicoes.c 0.2.0 estourava nesse caso; pendencia no motor).
 *            0.1.0 - F1 P1b: primeira versao. O caso degenerado (so espaco e linha em branco) sai como diff de apagamento total, porque o motor de edicoes nao expressa essa faixa.
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#include "cache/entradas_ler.h"
#include "common/fonte.h"
#include "driver/cmd_fmt_interno.h"
#include "driver/edicoes.h"

static FmtEstadoArquivo ClassificarArquivo(const FmtResultado *r)
{
    if (r->estado == FMT_NAO_ANALISADO) return FMT_ARQ_NAO_ANALISADO;
    return r->estado == FMT_CANONICO ? FMT_ARQ_CANONICO : FMT_ARQ_MUDARIA;
}

/* Linhas do texto: cada '\n' fecha uma, e o trecho depois do ultimo '\n' tambem conta. */
static uint32_t ContarLinhas(const char *txt, size_t tam)
{
    uint32_t n = 0;

    for (size_t k = 0; k < tam; k++) n += txt[k] == '\n';
    return n + ((tam > 0 && txt[tam - 1] != '\n') ? 1 : 0);
}

static void EmitirLinhaApagada(Buf *saida, const char *ini, size_t n, bool tem_fim)
{
    BufAnexar(saida, "-");
    BufAnexarN(saida, ini, n);
    BufAnexar(saida, tem_fim ? "\n" : "\n\\ No newline at end of file\n");
}

/* Diff de apagamento total: o texto inteiro some (o arquivo so tinha espaco e linha em branco). O motor de edicoes nao expressa essa faixa, por isso sai daqui. */
static void DiffApagamentoTotal(Buf *saida, const char *caminho, const FmtResultado *r)
{
    const char *txt = r->normalizado.dados;
    size_t tam = r->normalizado.tam, ini = 0;

    BufFormatar(saida, "--- a/%s\n+++ b/%s\n@@ -1,%u +0,0 @@\n", caminho, caminho, ContarLinhas(txt, tam));
    while (ini < tam) {
        const char *fim = memchr(txt + ini, '\n', tam - ini);
        size_t fim_idx = fim != NULL ? (size_t)(fim - txt) : tam;

        EmitirLinhaApagada(saida, txt + ini, fim_idx - ini, fim != NULL);
        ini = fim_idx + 1;
    }
}

/* O diff do arquivo anexado a saida; so BOM ou CRLF nao tem diff de texto e sai um comentario. false = o motor recusou as edicoes (erro interno). */
static bool DiffDoArquivo(Buf *saida, const char *caminho, const FmtResultado *r)
{
    if (r->formatado.tam == 0 && r->normalizado.tam > 0) {
        DiffApagamentoTotal(saida, caminho, r);
        return true;
    }
    if (r->edicoes.n == 0) {
        BufFormatar(saida, "# %s: so BOM e/ou fim de linha CRLF; o texto ja e canonico\n", caminho);
        return true;
    }
    return EdicoesDiff(r->normalizado.dados, r->normalizado.tam, r->edicoes.dados, (uint32_t)r->edicoes.n, caminho, saida) == EDICAO_OK;
}

static bool MostrarDiff(const char *caminho, const FmtResultado *r)
{
    Buf diff = {0};
    bool ok = DiffDoArquivo(&diff, caminho, r);

    if (ok && diff.tam > 0) fwrite(diff.dados, 1, diff.tam, stdout);
    if (!ok) fprintf(stderr, "forja: fmt: nao consegui montar o diff de '%s' (erro interno)\n", caminho);
    BufLiberar(&diff);
    return ok;
}

static void ImprimirNaoAnalisado(const char *caminho, const FmtResultado *r)
{
    fprintf(stderr, "forja: fmt: %s: nao analisado (%s, linha %u, coluna %u): %s\n", caminho, r->codigo[0] != '\0' ? r->codigo : "sem codigo", r->diag_linha, r->diag_coluna, r->motivo);
}

/* O texto formatado de novo e canonico? (a conferencia de um arquivo gravado) */
static bool TextoEhCanonico(const char *dados, size_t tam)
{
    FmtResultado r = {0};
    bool canonico;

    FmtFormatar(dados, tam, &r);
    canonico = r.estado == FMT_CANONICO;
    FmtResultadoLiberar(&r);
    return canonico;
}

static void DesfazerGravacao(const char *caminho, const char *original, size_t tam)
{
    if (EdicoesDesfazer(caminho, original, tam)) fprintf(stderr, "forja: fmt: a conferencia de '%s' falhou; o arquivo voltou ao original\n", caminho);
    else fprintf(stderr, "forja: fmt: a conferencia de '%s' falhou e nao consegui devolver o original (confira o arquivo)\n", caminho);
}

/* Relê o arquivo gravado: tem de dar canonico; senao volta ao original. */
static bool ConferirGravado(const char *caminho, const char *original, size_t tam)
{
    char *novo = NULL;
    size_t tam_novo = 0;
    bool ok = false;

    if (ArquivoLerInteiro(caminho, &novo, &tam_novo)) ok = TextoEhCanonico(novo != NULL ? novo : "", tam_novo);
    else fprintf(stderr, "forja: fmt: nao consegui reler '%s' depois de gravar\n", caminho);
    free(novo);
    if (!ok) DesfazerGravacao(caminho, original, tam);
    return ok;
}

/* O atributo so-leitura do Windows vale: a gravacao atomica trocaria o arquivo mesmo assim, por isso a permissao e conferida antes (false = nao grava). */
static bool GravacaoPermitida(const char *caminho)
{
    if (_access(caminho, 2) == 0) return true;
    fprintf(stderr, "forja: fmt: '%s' e so-leitura; nao gravei (o arquivo ficou como estava)\n", caminho);
    return false;
}

/* Grava o texto canonico (LF, sem BOM) de forma atomica e confere a gravacao. false = nao gravou (o arquivo fica como estava) ou a conferencia falhou. */
static bool GravarFormatado(const char *caminho, const char *original, size_t tam, const FmtResultado *r)
{
    int erro = 0;
    const char *dados = r->formatado.dados != NULL ? r->formatado.dados : "";

    if (!GravacaoPermitida(caminho)) return false;
    if (!ArquivoGravarAtomico(caminho, dados, r->formatado.tam, &erro)) {
        fprintf(stderr, "forja: fmt: nao consegui gravar '%s' (errno %d); o arquivo ficou como estava\n", caminho, erro);
        return false;
    }
    return ConferirGravado(caminho, original, tam);
}

/* A saida padrao de um arquivo (so sem --json): o diff no dry-run, a linha 'gravado:' com --aplicar, a causa do nao analisado no stderr. */
static bool MostrarArquivo(const FmtExecucao *x, const char *caminho, FmtEstadoArquivo estado, const FmtResultado *r)
{
    if (x->pedido->json) return true;
    switch (estado) {
    case FMT_ARQ_MUDARIA: return MostrarDiff(caminho, r);
    case FMT_ARQ_GRAVADO: printf("gravado: %s\n", caminho); return true;
    case FMT_ARQ_NAO_ANALISADO: ImprimirNaoAnalisado(caminho, r); return true;
    case FMT_ARQ_CANONICO: return true;
    }
    return true;
}

static void ContarArquivo(FmtTotais *totais, FmtEstadoArquivo estado)
{
    totais->arquivos++;
    switch (estado) {
    case FMT_ARQ_CANONICO: totais->canonicos++; break;
    case FMT_ARQ_MUDARIA: totais->mudariam++; break;
    case FMT_ARQ_GRAVADO: totais->gravados++; break;
    case FMT_ARQ_NAO_ANALISADO: totais->nao_analisados++; break;
    }
}

bool FmtProcessarArquivo(const char *caminho, FmtExecucao *x)
{
    char *lido = NULL;
    size_t tam = 0;
    FmtResultado r = {0};
    FmtEstadoArquivo estado;
    const char *original;
    bool ok = true;

    if (!ArquivoLerInteiro(caminho, &lido, &tam)) {
        fprintf(stderr, "forja: fmt: nao consegui ler '%s'\n", caminho);
        return false;
    }
    original = lido != NULL ? lido : "";
    FmtFormatar(original, tam, &r);
    estado = ClassificarArquivo(&r);
    if (estado == FMT_ARQ_MUDARIA && x->pedido->aplicar) {
        ok = GravarFormatado(caminho, original, tam, &r);
        if (ok) estado = FMT_ARQ_GRAVADO;
    }
    if (ok) ok = MostrarArquivo(x, caminho, estado, &r);
    if (ok) {
        ContarArquivo(&x->totais, estado);
        if (x->pedido->json) FmtJsonArquivo(&x->json, caminho, estado, &r);
    }
    FmtResultadoLiberar(&r);
    free(lido);
    return ok;
}
