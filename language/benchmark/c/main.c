/* main.c - linha de comando do forja: check e build --modo c, com --tempos.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { EXIT_OK = 0, EXIT_DIAG = 1, EXIT_USO = 2, EXIT_INTERNO = 3 };

typedef struct { bool build; const char *input, *output, *tempos; } Options;

static const char USO[] =
    "uso: forja check <arq.fj> [--tempos <t.json>]\n"
    "     forja build --modo c <arq.fj> -o <saida.c> [--tempos <t.json>]\n";

static bool ReadOptionValue(int argc, char **argv, int *i, const char **destino)
{
    if (*i + 1 >= argc) {
        fprintf(stderr, "forja: a opcao '%s' pede um valor\n%s", argv[*i], USO);
        return false;
    }
    *destino = argv[++*i];
    return true;
}

static bool ParseOption(int argc, char **argv, int *i, Options *o)
{
    const char *arg = argv[*i];
    const char *modo = NULL;
    if (strcmp(arg, "--tempos") == 0) return ReadOptionValue(argc, argv, i, &o->tempos);
    if (strcmp(arg, "-o") == 0) return ReadOptionValue(argc, argv, i, &o->output);
    if (strcmp(arg, "--modo") == 0) {
        if (!ReadOptionValue(argc, argv, i, &modo)) return false;
        if (strcmp(modo, "c") == 0) return true;
        fprintf(stderr, "forja: --modo '%s' nao suportado; so 'c'\n", modo);
        return false;
    }
    if (arg[0] != '-' && o->input == NULL) { o->input = arg; return true; }
    fprintf(stderr, "forja: argumento inesperado '%s'\n%s", arg, USO);
    return false;
}

static bool ParseArgs(int argc, char **argv, Options *o)
{
    memset(o, 0, sizeof *o);
    if (argc < 2 || (strcmp(argv[1], "check") != 0 && strcmp(argv[1], "build") != 0)) {
        fprintf(stderr, "forja: comando ausente ou desconhecido\n%s", USO);
        return false;
    }
    o->build = strcmp(argv[1], "build") == 0;
    for (int i = 2; i < argc; i++) {
        if (!ParseOption(argc, argv, &i, o)) return false;
    }
    if (o->input == NULL || (o->build && o->output == NULL)) {
        fprintf(stderr, "forja: faltou o arquivo de entrada%s\n%s", o->build ? " ou o -o" : "", USO);
        return false;
    }
    return true;
}

static bool RunStage(Ctx *c, bool (*stage)(Ctx *), uint64_t *ns)
{
    uint64_t inicio = NowNs();
    bool ok = stage(c);
    *ns = NowNs() - inicio;
    return ok;
}

static bool WriteTimes(const char *path, const Tempos *t, uint64_t linhas)
{
    char texto[400];
    int n = snprintf(texto, sizeof texto,
                     "{\"lex_ns\": %llu, \"parse_ns\": %llu, \"check_ns\": %llu, \"emit_ns\": %llu, "
                     "\"total_ns\": %llu, \"linhas\": %llu}\n",
                     (unsigned long long)t->lex, (unsigned long long)t->parse, (unsigned long long)t->check,
                     (unsigned long long)t->emit, (unsigned long long)t->total, (unsigned long long)linhas);
    return n > 0 && WriteWholeFile(path, texto, (size_t)n);
}

static int StageFailure(const Ctx *c)
{
    if (!c->diag.has) {
        fprintf(stderr, "forja: erro interno (sem diagnostico); veja a mensagem acima\n");
        return EXIT_INTERNO;
    }
    PrintDiag(c);
    return EXIT_DIAG;
}

/* Analise: lex -> parse -> check. Devolve o exit code ou -1 se tudo passou. */
static int Analyze(Ctx *c, Tempos *t)
{
    if (!InitSymbols(c)) return EXIT_INTERNO;
    if (!RunStage(c, LexFile, &t->lex)) return StageFailure(c);
    if (!RunStage(c, ParseFile, &t->parse)) return StageFailure(c);
    if (!RunStage(c, CheckFile, &t->check)) return StageFailure(c);
    return -1;
}

static int EmitOutput(Ctx *c, const Options *o, Tempos *t)
{
    uint64_t inicio = NowNs();
    bool ok = EmitFile(c) && WriteWholeFile(o->output, c->out.data, c->out.len);
    t->emit = NowNs() - inicio;
    if (ok) return EXIT_OK;
    return c->diag.has ? EXIT_DIAG : EXIT_USO;
}

static int Compile(const Options *o, Tempos *t, uint64_t *linhas)
{
    static Ctx ctx;
    uint64_t inicio = NowNs();
    char *src;
    size_t len;
    if (!ReadWholeFile(o->input, &src, &len)) return EXIT_USO;
    ctx.src = src;
    ctx.src_len = len;
    ctx.path = o->input;
    *linhas = CountLines(src, len);
    int code = Analyze(&ctx, t);
    if (code < 0) code = o->build ? EmitOutput(&ctx, o, t) : EXIT_OK;
    t->total = NowNs() - inicio;
    return code;
}

int main(int argc, char **argv)
{
    Options o;
    Tempos t = {0, 0, 0, 0, 0};
    uint64_t linhas = 0;
    if (!ParseArgs(argc, argv, &o)) return EXIT_USO;
    int code = Compile(&o, &t, &linhas);
    if (o.tempos != NULL && !WriteTimes(o.tempos, &t, linhas)) return EXIT_USO;
    return code;
}
