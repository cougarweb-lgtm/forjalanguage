/* cmd_fmt.c - o subcomando 'forja fmt' (ver cmd_fmt.h): argumentos, a lista de arquivos, o rodape, o JSON e o --stdin. O arquivo em si e do cmd_fmt_arquivo.c.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - F1 P2: o texto de sugestao do F0001 deixa de levar o caminho (era cortado em 120 bytes); o comando completo fica so em forma_certa.
 *            0.2.0 - F1 P2: ConferirCanonicoSeHa (check --canonico): UM F0001 por arquivo fora da forma, com a 1a edicao como correcao quando cabe.
 *            0.1.0 - F1 P1b: RodarFmt completo. O --stdin (e o --stdin --regras do verificar_fmt.py) veio do main_despacho.c 0.3.0 sem mudar a saida.
 * Verificacao: construir.bat && python tests\fmt\verificar_fmt.py --forja compiler\forja.exe && python tests\cli\verificar_cli.py --forja compiler\forja.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "common/arena.h"
#include "common/base.h"
#include "common/buf.h"
#include "common/diag.h"
#include "common/fonte.h"
#include "common/vec.h"
#include "driver/cmd_fmt.h"
#include "driver/cmd_fmt_interno.h"
#include "driver/edicoes.h"
#include "driver/fmt_nucleo.h"
#include "driver/migrar_pasta.h"

enum { SAIDA_OK = 0, SAIDA_PENDENTE = 1, SAIDA_USO = 2 };

static int UsoErrado(const char *motivo)
{
    fprintf(stderr, "forja: fmt: %s\n", motivo);
    fputs("uso: forja fmt <arq.fj | pasta> ... [--aplicar] [--json] [--stdin]\n", stderr);
    return SAIDA_USO;
}

/* Confere a combinacao das opcoes; NULL = valida. */
static const char *ValidarArgumentos(const FmtPedido *p)
{
    if (p->stdin_padrao) {
        if (p->aplicar || p->json || p->nentradas > 0) return "--stdin nao vale com --aplicar, --json nem com caminho";
        return NULL;
    }
    if (p->regras) return "--regras so vale com --stdin";
    return p->nentradas == 0 ? "falta o arquivo .fj ou a pasta" : NULL;
}

/* Le as opcoes e as entradas; devolve o motivo do uso errado (NULL = argumentos validos). */
static const char *LerArgumentos(int argc, char **argv, FmtPedido *p)
{
    memset(p, 0, sizeof *p);
    p->entradas = calloc((size_t)argc + 1, sizeof *p->entradas);
    if (p->entradas == NULL) FalhaInterna("memoria esgotada lendo os argumentos do fmt");
    for (int k = 0; k < argc; k++) {
        if (strcmp(argv[k], "--aplicar") == 0) p->aplicar = true;
        else if (strcmp(argv[k], "--json") == 0) p->json = true;
        else if (strcmp(argv[k], "--stdin") == 0) p->stdin_padrao = true;
        else if (strcmp(argv[k], "--regras") == 0) p->regras = true;
        else if (argv[k][0] == '-') return "opcao desconhecida (valem --aplicar, --json e --stdin)";
        else p->entradas[p->nentradas++] = argv[k];
    }
    return ValidarArgumentos(p);
}

/* Junta os arquivos de todas as entradas; false = alguma entrada nao existe (mensagem no stderr). */
static bool ListarArquivos(const FmtPedido *p, Arena *arena, Vec *arquivos)
{
    for (int k = 0; k < p->nentradas; k++) {
        if (MigrarColetar(p->entradas[k], arena, arquivos)) continue;
        fprintf(stderr, "forja: fmt: nao achei '%s'\n", p->entradas[k]);
        return false;
    }
    return true;
}

static void ImprimirRodape(const FmtExecucao *x)
{
    const FmtTotais *t = &x->totais;

    if (x->pedido->aplicar) printf("%u gravados, %u nao analisados, %u canonicos\n", t->gravados, t->nao_analisados, t->canonicos);
    else printf("%u mudariam, %u nao analisados, %u canonicos\n", t->mudariam, t->nao_analisados, t->canonicos);
}

static void ImprimirDocumento(const FmtExecucao *x)
{
    Buf doc = {0};

    FmtJsonDocumento(&doc, x);
    fwrite(doc.dados, 1, doc.tam, stdout);
    BufLiberar(&doc);
}

/* 1 = algum arquivo nao analisado, ou (dry-run) algum arquivo mudaria; 0 = tudo canonico ou gravado. */
static int CodigoDaExecucao(const FmtExecucao *x)
{
    if (x->totais.nao_analisados > 0) return SAIDA_PENDENTE;
    if (!x->pedido->aplicar && x->totais.mudariam > 0) return SAIDA_PENDENTE;
    return SAIDA_OK;
}

/* Arquivo ou pasta: processa cada .fj na ordem, com o relatorio (rodape ou documento JSON) no fim. Um erro de E/S para na hora (exit 2), como o migrar. */
static int Executar(const FmtPedido *p)
{
    Arena arena = {0};
    Vec arquivos;
    FmtExecucao x = {.pedido = p};
    int codigo = SAIDA_OK;

    VecIniciar(&arquivos, sizeof(const char *));
    if (!ListarArquivos(p, &arena, &arquivos)) codigo = SAIDA_USO;
    for (size_t k = 0; codigo == SAIDA_OK && k < arquivos.n; k++)
        if (!FmtProcessarArquivo(*(const char **)VecEm(&arquivos, k), &x)) codigo = SAIDA_USO;
    if (codigo == SAIDA_OK) {
        if (p->json) ImprimirDocumento(&x);
        else ImprimirRodape(&x);
        codigo = CodigoDaExecucao(&x);
    }
    BufLiberar(&x.json);
    VecLiberar(&arquivos);
    ArenaLiberar(&arena);
    return codigo;
}

/* A entrada padrao inteira (binario). false = erro de leitura. */
static bool LerEntradaPadrao(Buf *entrada)
{
    char bloco[65536];
    size_t n;

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    while ((n = fread(bloco, 1, sizeof bloco, stdin)) > 0) BufAnexarN(entrada, bloco, n);
    return !ferror(stdin);
}

static void ImprimirErroDeFmt(const FmtResultado *r)
{
    Buf b = {0};

    BufAnexar(&b, "{\"estado\": \"nao_analisado\", \"diagnostico\": ");
    EdicoesJsonTextoOuNulo(&b, r->codigo);
    BufFormatar(&b, ", \"linha\": %u, \"coluna\": %u, \"causa\": ", r->diag_linha, r->diag_coluna);
    EdicoesJsonTexto(&b, r->motivo, strlen(r->motivo));
    BufAnexar(&b, "}\n");
    fwrite(b.dados, 1, b.tam, stderr);
    BufLiberar(&b);
}

/* Auxilio de medicao do verificar_fmt.py (--stdin --regras): uma linha no stderr, "desvios=N primeiro=L:C:regra regras=nome:qtd,...". */
static void ImprimirRegrasDeFmt(const FmtResultado *r)
{
    static const char *const NOMES[] = {"bom", "fim_de_linha", "fim_de_arquivo", "linha_em_branco", "espaco_final", "indentacao", "espaco", "comentario"};
    const char *const *usadas = r->regras.dados;
    const char *separador = "";

    fprintf(stderr, "desvios=%u primeiro=%u:%u:%s regras=", r->desvios, r->primeiro.linha, r->primeiro.coluna, r->primeiro.regra != NULL ? r->primeiro.regra : "-");
    for (size_t k = 0; k < sizeof NOMES / sizeof NOMES[0]; k++) {
        uint32_t qtd = (k == 0 && r->tinha_bom) || (k == 1 && r->tinha_crlf) ? 1 : 0;

        for (size_t j = 0; j < r->regras.n; j++) qtd += strcmp(usadas[j], NOMES[k]) == 0;
        if (qtd > 0) fprintf(stderr, "%s%s:%u", separador, NOMES[k], qtd);
        if (qtd > 0) separador = ",";
    }
    fputc('\n', stderr);
}

/* 'forja fmt --stdin [--regras]': o fonte da entrada padrao sai formatado na saida padrao. Erro de sintaxe = JSON no stderr, exit 1, stdout vazio. */
static int RodarStdin(bool regras)
{
    Buf entrada = {0};
    FmtResultado r = {0};
    int codigo = SAIDA_OK;

    if (!LerEntradaPadrao(&entrada)) {
        fputs("forja fmt: nao consegui ler a entrada padrao\n", stderr);
        BufLiberar(&entrada);
        return SAIDA_USO;
    }
    FmtFormatar(entrada.dados != NULL ? entrada.dados : "", entrada.tam, &r);
    if (r.estado == FMT_NAO_ANALISADO) {
        ImprimirErroDeFmt(&r);
        codigo = SAIDA_PENDENTE;
    } else {
        fwrite(r.formatado.dados, 1, r.formatado.tam, stdout);
        if (regras) ImprimirRegrasDeFmt(&r);
    }
    FmtResultadoLiberar(&r);
    BufLiberar(&entrada);
    return codigo;
}

int RodarFmt(int argc, char **argv)
{
    FmtPedido pedido;
    const char *erro = LerArgumentos(argc, argv, &pedido);
    int codigo;

    if (erro != NULL) {
        free(pedido.entradas);
        return UsoErrado(erro);
    }
    codigo = pedido.stdin_padrao ? RodarStdin(pedido.regras) : Executar(&pedido);
    free(pedido.entradas);
    return codigo;
}

/* ---------- check --canonico (F1 P2): o F0001 ---------- */

/* Offset no texto do fonte da posicao (linha e coluna em bytes a partir de 1, sem CR). Fora do texto vale o fim. */
static uint32_t OffsetDaPosicao(const Fonte *f, uint32_t linha, uint32_t coluna)
{
    uint32_t off = 0, l = 1;

    while (l < linha && off < f->tam) {
        if (f->texto[off] == '\n') l++;
        off++;
    }
    off += coluna > 0 ? coluna - 1 : 0;
    return off < f->tam ? off : (uint32_t)f->tam;
}

/* Coluna_fim do 1o desvio: a faixa da Edicao que comeca nele (uma linha so); sem Edicao (BOM ou CRLF, que nao sao Edicoes) vale a propria coluna. */
static uint32_t ColunaFimDoPrimeiro(const FmtResultado *r)
{
    const Edicao *e = (const Edicao *)r->edicoes.dados;

    for (size_t k = 0; k < r->edicoes.n; k++) {
        if (e[k].linha != r->primeiro.linha || e[k].coluna != r->primeiro.coluna) continue;
        return e[k].linha_fim <= e[k].linha ? e[k].coluna_fim : e[k].coluna;
    }
    return r->primeiro.coluna;
}

/* A 1a edicao vira a correcao 'certa' quando cabe no pacote (uma linha so, 'novo' e trecho antigo dentro dos limites do diagnostico); senao so dica. */
static void AbrirSugestaoDoFmt(Diag *d, const Fonte *f, const FmtResultado *r, const char *forma)
{
    const Edicao *e = (const Edicao *)r->edicoes.dados;
    bool cabe = r->edicoes.n > 0 && e->linha_fim <= e->linha && strchr(e->novo, '\n') == NULL
                && strlen(e->novo) < DIAG_NOVO_MAX && e->coluna_fim - e->coluna <= DIAG_ANTIGO_MAX;

    DiagSugerir(&d->sug, cabe ? DIAG_CERTA : DIAG_SO_DICA, "%s", forma);
    if (cabe) DiagEditar(&d->sug, OffsetDaPosicao(f, e->linha, e->coluna), OffsetDaPosicao(f, e->linha, e->coluna_fim), e->novo);
}

/* Monta e imprime o F0001 do 1o desvio (UM por arquivo): causa com a regra e o total, forma_certa com o caminho, coluna_fim e a correcao. */
static void ImprimirF0001(const Fonte *f, const FmtResultado *r)
{
    Diag d = {0};
    char forma[320], coluna_fim[16];

    DiagRelatar(&d, F0001, OffsetDaPosicao(f, r->primeiro.linha, r->primeiro.coluna), "fora da forma canonica: %s (%u desvios)", r->primeiro.regra, r->desvios);
    snprintf(forma, sizeof forma, "rode forja fmt --aplicar %s", f->caminho != NULL ? f->caminho : "");
    AbrirSugestaoDoFmt(&d, f, r, "rode forja fmt --aplicar neste arquivo"); /* sem caminho: a sugestao e cortada em DIAG_DESCRICAO_MAX; o comando inteiro fica em forma_certa */
    DiagForma(&d, NULL, forma);
    snprintf(coluna_fim, sizeof coluna_fim, "%u", ColunaFimDoPrimeiro(r));
    DiagAcrescentarExtra(&d, "coluna_fim", coluna_fim);
    DiagImprimirJson(&d, f, stdout);
}

bool ConferirCanonicoSeHa(bool canonico, const Fonte *f)
{
    FmtResultado r = {0};
    bool ok = true;

    if (!canonico) return true;
    FmtFormatar(f->texto != NULL ? f->texto : "", f->tam, &r);
    if (r.estado == FMT_MUDARIA) {
        ImprimirF0001(f, &r);
        ok = false;
    }
    FmtResultadoLiberar(&r);
    return ok;
}
