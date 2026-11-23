/* gerar_bir_alcance.c - verificacoes de unidade da poda do inalcancavel (BirAlcance, B07, M4 P24p): ciclo, recursao, endereco tomado e export. Chamada pelo teste_nativo.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M4 P24p: primeira versao (+4 verificacoes).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 *
 * Cada verificacao constroi a BIR com BirEntrada.poda e olha o dump (BirImprimir): "funcao <simbolo>(" presente = o proc entrou; ausente = foi podado.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/bir/bir.h"
#include "gerar_bir_frente.h"

typedef struct { int *total; int *falhas; } Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (alcance) linha %d: %s\n", linha, texto);
}

#define ALCANCE_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* Fonte em memoria -> dump da BIR com poda (dll = --modo dll). false = o programa nao passou. */
static bool DumpComPoda(const char *texto, bool dll, Buf *dump)
{
    Frente *f = calloc(1, sizeof *f);
    BirPrograma programa;
    bool ok;

    f->fonte = FonteDeTexto("memoria.fj", texto);
    IniciarFrente(f);
    ok = AnalisarFrente(f);
    if (ok) {
        BirEntrada entrada = EntradaDaFrente(f, BM_DEV);

        entrada.poda = true;
        entrada.dll = dll;
        ok = BirConstruir(&entrada, &programa, &f->diag) && BirValidar(&programa, &f->diag);
        if (ok)
            BirImprimir(&programa, dump);
        BirLiberar(&programa);
    }
    LiberarFrente(f, false);
    free(f);
    return ok;
}

static bool Tem(const Buf *dump, const char *trecho)
{
    return dump->dados != NULL && strstr(dump->dados, trecho) != NULL;
}

/* main -> a <-> b e um ciclo vivo; morto1 <-> morto2 e um ciclo que ninguem alcanca. */
static const char FONTE_CICLO[] =
    "proc a(x: i32) -> i32 {\n    if x <= 0 {\n        return 0\n    }\n    return b(x - 1)\n}\n\n"
    "proc b(x: i32) -> i32 {\n    return a(x)\n}\n\n"
    "proc morto1(x: i32) -> i32 {\n    return morto2(x)\n}\n\n"
    "proc morto2(x: i32) -> i32 {\n    return morto1(x)\n}\n\n"
    "proc main() {\n    print(a(3))\n}\n";

/* fat e recursiva e viva; morta_rec chama a si mesma e ninguem a chama. */
static const char FONTE_RECURSAO[] =
    "proc fat(n: i32) -> i32 {\n    if n <= 1 {\n        return 1\n    }\n    return n * fat(n - 1)\n}\n\n"
    "proc morta_rec(n: i32) -> i32 {\n    if n <= 0 {\n        return 0\n    }\n    return morta_rec(n - 1)\n}\n\n"
    "proc main() {\n    print(fat(5))\n}\n";

/* alvo so aparece como valor (let f = alvo); sobra_viva toma o endereco de so_no_morto dentro de um proc que ninguem chama. */
static const char FONTE_ENDERECO[] =
    "proc alvo(x: i32) -> i32 {\n    return x + 1\n}\n\n"
    "proc so_no_morto(x: i32) -> i32 {\n    return x\n}\n\n"
    "proc morto_com_endereco() -> i32 {\n    let g = so_no_morto\n    return g(1)\n}\n\n"
    "proc main() {\n    let f = alvo\n    print(f(1))\n}\n";

/* exporta e a raiz da DLL, aux so e chamado por ela; solta nao e export nem e chamado. */
static const char FONTE_EXPORT[] =
    "proc aux(x: i64) -> i64 {\n    return x * 2\n}\n\n"
    "proc solta(x: i64) -> i64 {\n    return aux(x)\n}\n\n"
    "pub export proc exporta(x: i64) -> i64 {\n    return aux(x)\n}\n\n"
    "proc main() {\n}\n";

static void VerificaCiclo(const Contagem *c)
{
    Buf dump = {0};

    ALCANCE_VERIFICA(c, DumpComPoda(FONTE_CICLO, false, &dump) && Tem(&dump, "funcao fj_a(") && Tem(&dump, "funcao fj_b(") && Tem(&dump, "funcao fj_main(")
                            && !Tem(&dump, "funcao fj_morto1(") && !Tem(&dump, "funcao fj_morto2("));
    BufLiberar(&dump);
}

static void VerificaRecursao(const Contagem *c)
{
    Buf dump = {0};

    ALCANCE_VERIFICA(c, DumpComPoda(FONTE_RECURSAO, false, &dump) && Tem(&dump, "funcao fj_fat(") && Tem(&dump, "funcao fj_main(") && !Tem(&dump, "funcao fj_morta_rec("));
    BufLiberar(&dump);
}

static void VerificaEnderecoTomado(const Contagem *c)
{
    Buf dump = {0};

    ALCANCE_VERIFICA(c, DumpComPoda(FONTE_ENDERECO, false, &dump) && Tem(&dump, "funcao fj_alvo(") && Tem(&dump, "funcao fjt_alvo(") && Tem(&dump, "end @fjt_alvo")
                            && !Tem(&dump, "funcao fj_so_no_morto(") && !Tem(&dump, "fjt_so_no_morto") && !Tem(&dump, "funcao fj_morto_com_endereco("));
    BufLiberar(&dump);
}

static void VerificaExport(const Contagem *c)
{
    Buf dump = {0};

    ALCANCE_VERIFICA(c, DumpComPoda(FONTE_EXPORT, true, &dump) && Tem(&dump, "funcao exporta(") && Tem(&dump, "funcao fj_aux(") && Tem(&dump, "funcao fj_main(")
                            && !Tem(&dump, "funcao fj_solta("));
    BufLiberar(&dump);
}

void AlcanceVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    VerificaCiclo(&c);
    VerificaRecursao(&c);
    VerificaEnderecoTomado(&c);
    VerificaExport(&c);
}
