/* processo.c - execucao de programas pela Forja: roda o programa (forja ligar, forja run e forja test), cita cada argumento e da os caminhos temporarios do run/test.
 * Versao: 0.10.0
 * Changelog: 0.10.0 - 0.21.0 F4 passo 2b: cc.c virou processo.c. Saem todas as funcoes que chamavam o gcc (CcCompilar, MontarLinha*, RodarGcc*, VersaoMaiorDoGcc,
 *                    DesviarFd/RestaurarFd, ligacao com B0004, CcCaminhoDoC) e as flags do erro do gcc 16; o driver nao tem mais nada de gcc.
 *                    Cc* viram Proc*; ProcExeDoRun e ProcApagarExeDoRun ficam (forja run chama com o exe do nativo x64).
 *            0.9.0 - O0 P13: -DFORJA_SEM_CHECAGEM quando CcPedido.sem_checagem (MontarLinha e MontarLinhaObjeto).
 *            0.8.0 - M6 P68: -g na compilacao do .c quando CcPedido.simbolos (MontarLinha e MontarLinhaObjeto, que recebe o pedido); a ligacao nao muda.
 *            0.7.3 - M5 P53: -lws2_32 na ligacao quando CcPedido.winsock (programa com std:net); fora dos -Wl,-Bdynamic, que so leva as 'link'.
 *            0.7.2 - POS-Z: com gcc >= 16 tambem -fno-tree-ch (segundo erro do gcc 16.1.0: laco aninhado com escrita perdida em -O2 -DFORJA_DEV; ver CC_FLAG_SEM_CH).
 *            0.7.1 - IA1-L9c: -Wno-array-bounds e -Wno-stringop-overflow (falso positivo do gcc 16 em memmove/memset de ordered_remove, insert_at e pop; ver FLAGS_BASE).
 *            0.7.0 - P-IA1-LG: contorna o erro do gcc 16.1.0 (tree-widening_mul alarga subtracao saturada com sinal): VersaoMaiorDoGcc (-dumpfullversion, uma vez por processo) e, com gcc >= 16, -fdisable-tree-widening_mul;
 *                    a nota que o gcc escreve por isso sai do stderr da compilacao (RodarGccSemNota); DesviarFd/RestaurarFd (stdout ou stderr para arquivo) servem a sonda e a ligacao.
 *            0.6.0 - IA1-b03: a ligacao roda com o stderr do gcc/ld desviado para um arquivo temporario (<obj>.ld.txt) e o B0004 leva essa saida em "ferramenta": {"nome","saida"}; nada do ld sai solto no stderr.
 *            0.5.0 - IA1-b02: -Wl,--no-insert-timestamp na ligacao (o carimbo do PE era a hora do build); CcCaminhoDoC (.c temporario por hash), CcExeDoRun e CcApagarExeDoRun (exe do run/test por hash do caminho, sem pid).
 *            0.4.1 - forja run -- args: CcExecutarPrograma saiu (virou orfa; o main.c chama CcExecutarComArgumentos com os argumentos do '--').
 *            0.4.0 -Linguagem v1.8b P6/P7: um -L por CcPedido.pastas_extra, depois do -L de pasta_libs (vendor:/engine:); CcExecutarComArgumentos (cita cada argumento como o gcc).
 *            0.3.0 - Linguagem v1.4 P5: CcPedido; Linha dinamica (sem CC_MAX_ARGS); build em 2 passos so com foreign/link
 *                    (-c e ligacao com -L, -Wl,-Bdynamic -l... -Wl,-Bstatic); B0004 em falha de ligacao (CC_LIGACAO).
 *            0.2.0 - Fase 5h: argv[0] sai citado (FORJA_CC com espaco no caminho; a busca no PATH segue com o nome cru);
 *                    CcExecutarPrograma roda o .exe de 'forja run' e 'forja test'; B0001/B0002 saem do catalogo (common/diag).
 *            0.1.0 - Fase 5d: gcc -std=c11 -O2 -static -Wall -Wextra -Werror -D__USE_MINGW_ANSI_STDIO=1 (+ -DFORJA_DEV no dev).
 *                    FORJA_CC troca o executavel (padrao gcc). Nao iniciar = CC_AUSENTE; gcc com codigo != 0 = CC_FALHOU.
 *                    O msvcrt junta o argv sem aspas, por isso cada argumento sai citado aqui (regra do argv do Windows).
 * Verificacao: python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <direct.h>
#include <errno.h>
#include <process.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/base.h"
#include "common/buf.h"
#include "common/hash.h"
#include "driver/processo.h"

#define PROC_CAMINHO_MAX 600

/* Cada argumento citado num Buf com NUL final: o argv aponta para o dados de cada um. Cresce sem limite. */
typedef struct { Buf *item; int n, cap; } Linha;

/* Regra do argv do Windows: barra so vira literal antes de aspa (dobrada, mais a propria aspa como \"). */
static void RepetirBarra(Buf *b, size_t quantas)
{
    for (size_t k = 0; k < quantas; k++)
        BufAnexarN(b, "\\", 1);
}

static void CitarEm(Buf *b, const char *arg)
{
    size_t barras = 0;

    BufAnexarN(b, "\"", 1);
    for (const char *p = arg; ; p++) {
        if (*p == '\\') {
            barras++;
            continue;
        }
        if (*p == '\0') {
            RepetirBarra(b, barras * 2);
            break;
        }
        if (*p == '"') {
            RepetirBarra(b, barras * 2 + 1);
            BufAnexarN(b, "\"", 1);
        } else {
            RepetirBarra(b, barras);
            BufAnexarN(b, p, 1);
        }
        barras = 0;
    }
    BufAnexarN(b, "\"", 1);
}

static void LinhaAdicionar(Linha *l, const char *arg)
{
    Buf *b;

    if (l->n >= l->cap) {
        int nova = l->cap == 0 ? 16 : l->cap * 2;
        Buf *novo = realloc(l->item, (size_t)nova * sizeof *novo);

        if (novo == NULL)
            FalhaInterna("proc: memoria esgotada ao montar a linha de argumentos");
        l->item = novo;
        l->cap = nova;
    }
    b = &l->item[l->n++];
    *b = (Buf){0};
    CitarEm(b, arg);
    BufAnexarN(b, "", 1);
}

static void LinhaLiberar(Linha *l)
{
    for (int k = 0; k < l->n; k++)
        BufLiberar(&l->item[k]);
    free(l->item);
}

/* Roda 'programa' com os argumentos da linha. argv[0] sai citado, porque o msvcrt o escreve cru na linha de comando
 * (caminho com espaco quebraria); a busca no PATH usa o nome sem aspas. Devolve -1 se nao iniciou. */
static intptr_t ExecutarLinha(const char *programa, const Linha *l)
{
    Buf nome = {0};
    const char **argv = malloc(((size_t)l->n + 2) * sizeof *argv);
    intptr_t rc;

    if (argv == NULL)
        FalhaInterna("proc: memoria esgotada ao montar o argv do programa");
    CitarEm(&nome, programa);
    BufAnexarN(&nome, "", 1);
    argv[0] = nome.dados;
    for (int k = 0; k < l->n; k++)
        argv[k + 1] = l->item[k].dados;
    argv[l->n + 1] = NULL;
    rc = _spawnvp(_P_WAIT, programa, argv);
    BufLiberar(&nome);
    free(argv);
    return rc;
}

bool ProcExecutarComArgumentos(const char *programa, const char *const *args, int nargs, int *codigo)
{
    Linha l = {0};
    intptr_t rc;

    for (int k = 0; k < nargs; k++)
        LinhaAdicionar(&l, args[k]);
    rc = ExecutarLinha(programa, &l);
    LinhaLiberar(&l);
    if (rc == -1)
        return false;
    *codigo = (int)rc;
    return true;
}

bool ProcArquivoTemporario(char *destino, size_t tam, const char *extensao)
{
    const char *pasta = getenv("TEMP");
    int n;

    if (pasta == NULL || pasta[0] == '\0')
        pasta = ".";
    n = snprintf(destino, tam, "%s\\forja_%d%s", pasta, (int)_getpid(), extensao);
    return n > 0 && (size_t)n < tam;
}

/* Pasta temporaria do usuario (%TEMP%, ou a atual). */
static const char *PastaTemporaria(void)
{
    const char *pasta = getenv("TEMP");

    return pasta != NULL && pasta[0] != '\0' ? pasta : ".";
}

/* Caminho absoluto de 'caminho' em 'destino'; se nao resolver, o proprio texto (o hash so precisa de um valor estavel). */
static void CaminhoAbsoluto(char *destino, size_t tam, const char *caminho)
{
    if (_fullpath(destino, caminho, tam) == NULL)
        snprintf(destino, tam, "%s", caminho);
}

/* "a\b\prog.fj" -> "prog" (nome do arquivo sem a extensao .fj), no maximo 60 bytes. */
static void NomeDoFonte(char *destino, size_t tam, const char *fonte)
{
    const char *base = strrchr(fonte, '\\');
    const char *barra = strrchr(fonte, '/');
    size_t n;

    if (base == NULL || (barra != NULL && barra > base))
        base = barra;
    base = base != NULL ? base + 1 : fonte;
    n = strlen(base);
    if (n > 3 && _stricmp(base + n - 3, ".fj") == 0)
        n -= 3;
    if (n >= tam)
        n = tam - 1;
    memcpy(destino, base, n);
    destino[n] = '\0';
}

/* Cria a pasta (existir ja e ok). */
static bool CriarPasta(const char *pasta)
{
    return _mkdir(pasta) == 0 || errno == EEXIST;
}

bool ProcExeDoRun(char *destino, size_t tam, const char *fonte, const char *variante)
{
    char absoluto[PROC_CAMINHO_MAX], nome[64], raiz[PROC_CAMINHO_MAX], pasta[PROC_CAMINHO_MAX];
    uint64_t h;
    int n;

    CaminhoAbsoluto(absoluto, sizeof absoluto, fonte);
    h = Hash64(absoluto, strlen(absoluto), HASH64_SEMENTE);
    h = Hash64(variante, strlen(variante), h);
    NomeDoFonte(nome, sizeof nome, fonte);
    snprintf(raiz, sizeof raiz, "%s\\forja_run", PastaTemporaria());
    n = snprintf(pasta, sizeof pasta, "%s\\%016llx", raiz, (unsigned long long)h);
    if (n <= 0 || (size_t)n >= sizeof pasta || !CriarPasta(raiz) || !CriarPasta(pasta))
        return false;
    n = snprintf(destino, tam, "%s\\%s.exe", pasta, nome);
    return n > 0 && (size_t)n < tam;
}

void ProcApagarExeDoRun(const char *arq_exe)
{
    char pasta[PROC_CAMINHO_MAX];
    char *corte;

    remove(arq_exe);
    snprintf(pasta, sizeof pasta, "%s", arq_exe);
    corte = strrchr(pasta, '\\');
    if (corte == NULL)
        return;
    *corte = '\0';
    _rmdir(pasta);
}
