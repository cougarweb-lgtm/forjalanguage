/* edicoes.c - motor de edicao de texto do driver (ver edicoes.h): validacao, aplicacao e diff unificado.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - F1 P1b: LinhaNovaFinal limita a ultima linha nova ao tamanho do novo texto (o diff de edicao que poe '\n' no fim de arquivo sem '\n' final lia alem de ini[] e derrubava o forja com exit 139)
 *            0.2.0 - IA1-I4: faixa de varias linhas (Edicao.linha_fim: validacao, conflito, numero de linha novo, hunks do diff); EdicoesTemBom, EdicoesMontarArquivo, EdicoesDesfazer e a escrita JSON (EdicoesJsonTexto, EdicoesJsonTextoOuNulo, EdicoesJsonEdicao) vieram do cmd_corrigir.c, sem mudar o comportamento.
 *            0.1.0 - IA1-b05: primeira versao (nucleo do 'forja corrigir'; o 'forja editar' do I4 reaproveita).
 * Verificacao: construir.bat && teste_common.exe
 */
#include <stdlib.h>
#include <string.h>

#include "cache/entradas_ler.h"
#include "common/fonte.h"
#include "driver/edicoes.h"
#include "nativo/nativo_json.h"

enum { CONTEXTO_DIFF = 3 };

const char *EdicaoStatusTexto(EdicaoStatus status)
{
    switch (status) {
    case EDICAO_OK: return "edicao valida";
    case EDICAO_SEM_LINHA: return "a linha da edicao nao existe no arquivo";
    case EDICAO_FAIXA_INVALIDA: return "a faixa de colunas da edicao e invalida ou passa do fim da linha";
    case EDICAO_ANTIGO_DIFERE: return "o texto do arquivo na faixa nao e o 'antigo' da edicao";
    case EDICAO_SOBREPOSTA: return "duas edicoes tocam o mesmo trecho";
    }
    return "edicao invalida";
}

/* Linha onde a faixa da edicao termina (linha_fim 0 = a mesma em que comeca). */
static uint32_t LinhaFimDe(const Edicao *e)
{
    return e->linha_fim > e->linha ? e->linha_fim : e->linha;
}

/* (linha1, coluna1) vem antes de (linha2, coluna2)? */
static bool PosicaoAntes(uint32_t linha1, uint32_t coluna1, uint32_t linha2, uint32_t coluna2)
{
    return linha1 != linha2 ? linha1 < linha2 : coluna1 < coluna2;
}

bool EdicoesConflitam(const Edicao *a, const Edicao *b)
{
    const Edicao *antes, *depois;

    if (a->linha == b->linha && a->coluna == b->coluna) return true;
    antes = PosicaoAntes(a->linha, a->coluna, b->linha, b->coluna) ? a : b;
    depois = antes == a ? b : a;
    return PosicaoAntes(depois->linha, depois->coluna, LinhaFimDe(antes), antes->coluna_fim);
}

bool EdicoesIguais(const Edicao *a, const Edicao *b)
{
    return a->linha == b->linha && LinhaFimDe(a) == LinhaFimDe(b) && a->coluna == b->coluna && a->coluna_fim == b->coluna_fim && strcmp(a->antigo, b->antigo) == 0 &&
           strcmp(a->novo, b->novo) == 0;
}

uint32_t EdicoesLinhaNova(const Edicao *lista, uint32_t n, uint32_t linha)
{
    uint32_t nova = linha;

    for (uint32_t k = 0; k < n; k++) {
        if (lista[k].linha >= linha) continue;
        for (const char *c = lista[k].novo; *c != '\0'; c++) nova += *c == '\n';
        nova -= LinhaFimDe(&lista[k]) - lista[k].linha;
    }
    return nova;
}

/* ---------- ordenacao e validacao ---------- */

static int CompararEdicoes(const void *x, const void *y)
{
    const Edicao *a = *(const Edicao *const *)x, *b = *(const Edicao *const *)y;

    if (a->linha != b->linha) return a->linha < b->linha ? -1 : 1;
    if (a->coluna != b->coluna) return a->coluna < b->coluna ? -1 : 1;
    if (a->coluna_fim != b->coluna_fim) return a->coluna_fim < b->coluna_fim ? -1 : 1;
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* Ponteiros para as edicoes, por (linha, coluna). Quem chama libera com free. */
static const Edicao **Ordenar(const Edicao *lista, uint32_t n)
{
    const Edicao **ord = malloc((n > 0 ? n : 1) * sizeof *ord);

    if (ord == NULL) FalhaInterna("memoria esgotada ordenando as edicoes");
    for (uint32_t k = 0; k < n; k++) ord[k] = &lista[k];
    qsort(ord, n, sizeof *ord, CompararEdicoes);
    return ord;
}

/* Vizinhas na ordem: se alguma sobrepoe, ha sobreposicao (a de inicio mais cedo cobre a seguinte). */
static EdicaoStatus ConferirSobreposicao(const Edicao *lista, const Edicao *const *ord, uint32_t n, uint32_t *ruim)
{
    for (uint32_t k = 1; k < n; k++) {
        if (!EdicoesConflitam(ord[k - 1], ord[k])) continue;
        *ruim = (uint32_t)(ord[k] - lista);
        return EDICAO_SOBREPOSTA;
    }
    return EDICAO_OK;
}

/* Fim do texto util da linha que comeca em 'ini': sem o '\n' e sem o '\r' que o precede. */
static size_t FimUtilDaLinha(const char *texto, size_t tam, size_t ini)
{
    const char *nl = memchr(texto + ini, '\n', tam - ini);
    size_t fim = nl != NULL ? (size_t)(nl - texto) : tam;

    if (nl != NULL && fim > ini && texto[fim - 1] == '\r') fim--;
    return fim;
}

/* ---------- aplicar ---------- */

/* Anda de linha em linha ate a de numero 'alvo'; *linha e *ini guardam onde parou. false = o texto acaba antes.
 * LIMITE: parametros 5 (texto+tam sao um buffer; *linha e *ini sao o cursor que o chamador reaproveita de uma edicao para a outra). */
static bool IrParaLinha(const char *texto, size_t tam, uint32_t alvo, uint32_t *linha, size_t *ini)
{
    while (*linha < alvo) {
        const char *nl = memchr(texto + *ini, '\n', tam - *ini);

        if (nl == NULL) return false;
        *ini = (size_t)(nl - texto) + 1;
        (*linha)++;
    }
    return *ini < tam || (*ini == tam && alvo == 1);
}

/* A faixa da edicao cabe no texto e o texto dela e o 'antigo'; a linha inicial comeca em 'ini_linha'. Devolve os offsets [*de, *ate) da faixa. Faixa de varias linhas
 * (linha_fim > linha): a coluna final conta na ultima linha, e a faixa leva os fins de linha do meio.
 * LIMITE: parametros 6 (texto+tam sao um buffer; os dois ultimos sao as saidas da faixa). */
static EdicaoStatus ConferirFaixa(const char *texto, size_t tam, size_t ini_linha, const Edicao *e, size_t *de, size_t *ate)
{
    uint32_t linha = e->linha;
    size_t ini_fim = ini_linha;

    if (!IrParaLinha(texto, tam, LinhaFimDe(e), &linha, &ini_fim)) return EDICAO_SEM_LINHA;
    *de = ini_linha + e->coluna - 1;
    *ate = ini_fim + e->coluna_fim - 1;
    if (e->coluna < 1 || e->coluna_fim < 1 || *ate > FimUtilDaLinha(texto, tam, ini_fim)) return EDICAO_FAIXA_INVALIDA;
    if (ini_fim == ini_linha ? e->coluna_fim < e->coluna : *de > FimUtilDaLinha(texto, tam, ini_linha)) return EDICAO_FAIXA_INVALIDA;
    if (strlen(e->antigo) != *ate - *de || memcmp(texto + *de, e->antigo, *ate - *de) != 0) return EDICAO_ANTIGO_DIFERE;
    return EDICAO_OK;
}

/* LIMITE: parametros 6 (texto+tam e lista+n sao pares de buffer da fronteira publica; saida e indice_ruim sao as duas saidas; agrupar em struct so criaria uma copia). */
EdicaoStatus EdicoesAplicar(const char *texto, size_t tam, const Edicao *lista, uint32_t n, Buf *saida, uint32_t *indice_ruim)
{
    const Edicao **ord = Ordenar(lista, n);
    EdicaoStatus st;
    uint32_t ruim = 0, linha = 1;
    size_t ini = 0, copiado = 0, de = 0, ate = 0;
    Buf novo = {0};

    st = ConferirSobreposicao(lista, ord, n, &ruim);
    for (uint32_t k = 0; k < n && st == EDICAO_OK; k++) {
        const Edicao *e = ord[k];

        ruim = (uint32_t)(e - lista);
        if (!IrParaLinha(texto, tam, e->linha, &linha, &ini)) {
            st = EDICAO_SEM_LINHA;
            break;
        }
        st = ConferirFaixa(texto, tam, ini, e, &de, &ate);
        if (st != EDICAO_OK) break;
        BufAnexarN(&novo, texto + copiado, de - copiado);
        BufAnexar(&novo, e->novo);
        copiado = ate;
    }
    if (st == EDICAO_OK) {
        BufAnexarN(&novo, texto + copiado, tam - copiado);
        BufAnexarN(saida, novo.dados != NULL ? novo.dados : "", novo.tam);
    } else if (indice_ruim != NULL) {
        *indice_ruim = ruim;
    }
    BufLiberar(&novo);
    free(ord);
    return st;
}

/* ---------- diff unificado ---------- */

/* Texto dividido em linhas: ini[k-1] e o inicio da linha k (k de 1 a nlinhas); ini[nlinhas] = tam. */
typedef struct { const char *texto; size_t tam; size_t *ini; uint32_t nlinhas; } Linhas;

static void LinhasIndexar(Linhas *l, const char *texto, size_t tam)
{
    uint32_t n = 0;

    l->texto = texto;
    l->tam = tam;
    for (size_t k = 0; k < tam; k++) n += texto[k] == '\n';
    if (tam > 0 && texto[tam - 1] != '\n') n++;
    l->nlinhas = n;
    l->ini = malloc(((size_t)n + 1) * sizeof *l->ini);
    if (l->ini == NULL) FalhaInterna("memoria esgotada indexando as linhas do diff");
    l->ini[0] = 0;
    for (uint32_t k = 1; k <= n; k++) {
        const char *nl = memchr(texto + l->ini[k - 1], '\n', tam - l->ini[k - 1]);

        l->ini[k] = nl != NULL ? (size_t)(nl - texto) + 1 : tam;
    }
}

/* A linha k (a partir de 1) com o prefixo do diff (' ', '-' ou '+'); sem '\n' no fim do arquivo, a marca do patch. */
static void EmitirLinha(Buf *saida, char prefixo, const Linhas *l, uint32_t k)
{
    size_t ini = l->ini[k - 1], fim = l->ini[k];
    bool tem_nl = fim > ini && l->texto[fim - 1] == '\n';

    BufAnexarN(saida, &prefixo, 1);
    BufAnexarN(saida, l->texto + ini, fim - ini - (tem_nl ? 1 : 0));
    BufAnexarN(saida, "\n", 1);
    if (!tem_nl) BufAnexar(saida, "\\ No newline at end of file\n");
}

/* Linhas originais a..b (inclusive) que as edicoes tocam, em sequencia sem lacuna. */
typedef struct { uint32_t a, b; } FaixaDeLinhas;

static uint32_t AgruparFaixas(const Edicao *const *ord, uint32_t n, FaixaDeLinhas *faixas)
{
    uint32_t total = 0;

    for (uint32_t k = 0; k < n; k++) {
        uint32_t linha = ord[k]->linha, ultima = LinhaFimDe(ord[k]);

        if (total > 0 && linha <= faixas[total - 1].b + 1) {
            if (ultima > faixas[total - 1].b) faixas[total - 1].b = ultima;
            continue;
        }
        faixas[total].a = linha;
        faixas[total].b = ultima;
        total++;
    }
    return total;
}

typedef struct {
    Linhas velho, novo;
    const Edicao *lista;
    uint32_t n;
    Buf *saida;
} Diff;

/* Ultima linha NOVA que a faixa terminada na linha velha `b` ocupa. Limitada ao novo texto: uma edicao que poe '\n' no fim de um arquivo sem '\n' final soma uma linha na conta
 * das edicoes, mas LinhasIndexar nao conta a linha vazia depois do ultimo '\n' (sem o limite, EmitirLinha leria ini[] alem do alocado: F1 P1b). */
static uint32_t LinhaNovaFinal(const Diff *d, uint32_t b)
{
    uint32_t nb = EdicoesLinhaNova(d->lista, d->n, b + 1) - 1;

    return nb > d->novo.nlinhas ? d->novo.nlinhas : nb;
}

/* Faixas fx[de..ate] formam um hunk: contexto antes, -antigas +novas de cada faixa com o contexto entre elas, contexto depois. */
static void EmitirHunk(const Diff *d, const FaixaDeLinhas *fx, uint32_t de, uint32_t ate)
{
    uint32_t ini = fx[de].a > CONTEXTO_DIFF ? fx[de].a - CONTEXTO_DIFF : 1;
    uint32_t fim = fx[ate].b + CONTEXTO_DIFF < d->velho.nlinhas ? fx[ate].b + CONTEXTO_DIFF : d->velho.nlinhas;
    uint32_t qtd_velho = fim - ini + 1, qtd_novo = qtd_velho, cursor = ini;

    for (uint32_t f = de; f <= ate; f++) {
        uint32_t na = EdicoesLinhaNova(d->lista, d->n, fx[f].a), nb = LinhaNovaFinal(d, fx[f].b);

        qtd_novo = qtd_novo - (fx[f].b - fx[f].a + 1) + (nb + 1 - na);
    }
    BufFormatar(d->saida, "@@ -%u,%u +%u,%u @@\n", ini, qtd_velho, EdicoesLinhaNova(d->lista, d->n, ini), qtd_novo);
    for (uint32_t f = de; f <= ate; f++) {
        uint32_t na = EdicoesLinhaNova(d->lista, d->n, fx[f].a), nb = LinhaNovaFinal(d, fx[f].b);

        for (; cursor < fx[f].a; cursor++) EmitirLinha(d->saida, ' ', &d->velho, cursor);
        for (uint32_t k = fx[f].a; k <= fx[f].b; k++) EmitirLinha(d->saida, '-', &d->velho, k);
        for (uint32_t k = na; k <= nb; k++) EmitirLinha(d->saida, '+', &d->novo, k);
        cursor = fx[f].b + 1;
    }
    for (; cursor <= fim; cursor++) EmitirLinha(d->saida, ' ', &d->velho, cursor);
}

/* Todos os hunks: faixas separadas por no maximo 2 * contexto linhas iguais dividem o mesmo hunk. */
static void EmitirHunks(const Diff *d, const FaixaDeLinhas *fx, uint32_t nfaixas)
{
    uint32_t de = 0;

    for (uint32_t k = 1; k <= nfaixas; k++) {
        if (k < nfaixas && fx[k].a - fx[k - 1].b - 1 <= 2 * CONTEXTO_DIFF) continue;
        EmitirHunk(d, fx, de, k - 1);
        de = k;
    }
}

/* LIMITE: parametros 6 (mesmos pares de buffer do EdicoesAplicar, mais o nome do arquivo e a saida do diff). */
EdicaoStatus EdicoesDiff(const char *texto, size_t tam, const Edicao *lista, uint32_t n, const char *nome, Buf *saida)
{
    Buf aplicado = {0};
    EdicaoStatus st = EdicoesAplicar(texto, tam, lista, n, &aplicado, NULL);
    const Edicao **ord;
    FaixaDeLinhas *faixas;
    Diff d = {.lista = lista, .n = n, .saida = saida};

    if (st != EDICAO_OK || n == 0) {
        BufLiberar(&aplicado);
        return st;
    }
    ord = Ordenar(lista, n);
    faixas = malloc((size_t)n * sizeof *faixas);
    if (faixas == NULL) FalhaInterna("memoria esgotada agrupando as linhas do diff");
    LinhasIndexar(&d.velho, texto, tam);
    LinhasIndexar(&d.novo, aplicado.dados, aplicado.tam);
    BufFormatar(saida, "--- a/%s\n+++ b/%s\n", nome, nome);
    EmitirHunks(&d, faixas, AgruparFaixas(ord, n, faixas));
    free(d.velho.ini);
    free(d.novo.ini);
    free(faixas);
    free(ord);
    BufLiberar(&aplicado);
    return EDICAO_OK;
}

/* ---------- arquivo: BOM, montar, desfazer ---------- */

bool EdicoesTemBom(const char *dados, size_t tam)
{
    return tam >= EDICOES_BOM_TAM && (unsigned char)dados[0] == 0xEF && (unsigned char)dados[1] == 0xBB && (unsigned char)dados[2] == 0xBF;
}

/* LIMITE: parametros 5 (original+tam e lista+n sao pares de buffer; novo e a saida). */
bool EdicoesMontarArquivo(const char *original, size_t tam, const Edicao *lista, uint32_t n, Buf *novo)
{
    size_t bom = EdicoesTemBom(original, tam) ? EDICOES_BOM_TAM : 0;

    BufAnexarN(novo, original, bom);
    return EdicoesAplicar(original + bom, tam - bom, lista, n, novo, NULL) == EDICAO_OK;
}

bool EdicoesDesfazer(const char *arquivo, const char *original, size_t tam)
{
    char *atual = NULL;
    size_t tam_atual = 0;
    bool ok = ArquivoGravarAtomico(arquivo, original, tam, NULL) && ArquivoLerInteiro(arquivo, &atual, &tam_atual) && tam_atual == tam && memcmp(atual, original, tam) == 0;

    free(atual);
    return ok;
}

/* ---------- JSON ---------- */

/* Escape de uma letra so (aspas, barra, quebra de linha, retorno de carro, tabulacao); NULL = o byte sai como esta ou como \u00xx. */
static const char *EscapeCurto(unsigned char ch)
{
    switch (ch) {
    case '"': return "\\\"";
    case '\\': return "\\\\";
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\t': return "\\t";
    default: return NULL;
    }
}

void EdicoesJsonTexto(Buf *b, const char *texto, size_t n)
{
    BufAnexarN(b, "\"", 1);
    for (size_t k = 0; k < n; k++) {
        unsigned char ch = (unsigned char)texto[k];
        const char *curto = EscapeCurto(ch);

        if (curto != NULL) {
            BufAnexar(b, curto);
            continue;
        }
        if (ch < 0x20) BufFormatar(b, "\\u%04x", (unsigned)ch);
        else BufAnexarN(b, (const char *)&ch, 1);
    }
    BufAnexarN(b, "\"", 1);
}

void EdicoesJsonTextoOuNulo(Buf *b, const char *texto)
{
    if (texto == NULL || texto[0] == '\0') BufAnexar(b, "null");
    else EdicoesJsonTexto(b, texto, strlen(texto));
}

void EdicoesJsonEdicao(Buf *b, const Edicao *e, const char *arquivo)
{
    BufAnexar(b, "{\"arquivo\": ");
    JsonCaminho(b, arquivo);
    BufFormatar(b, ", \"linha\": %u, \"coluna\": %u, ", e->linha, e->coluna);
    if (e->linha_fim > e->linha) BufFormatar(b, "\"linha_fim\": %u, ", e->linha_fim);
    BufFormatar(b, "\"coluna_fim\": %u, \"antigo\": ", e->coluna_fim);
    EdicoesJsonTexto(b, e->antigo, strlen(e->antigo));
    BufAnexar(b, ", \"novo\": ");
    EdicoesJsonTexto(b, e->novo, strlen(e->novo));
    BufAnexar(b, "}");
}
