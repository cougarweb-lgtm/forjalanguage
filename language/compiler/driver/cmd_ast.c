/* cmd_ast.c - 'forja ast': AST 1.0 em JSON (ids estruturais, "fim", "ref"), --tipos, --sem-pos, --no e projeto.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I1: primeira versao. Sem --tipos a arvore impressa e a do parser; os "ref" vem de uma segunda compilacao analisada do mesmo arquivo.
 * Verificacao: construir.bat && python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "driver/cmd_ast.h"
#include "semantic/ast_json.h"
#include "nativo/nativo.h"

/* Escreve a AST da compilacao 'c' em stdout. sem != NULL: --tipos. refs: ligacoes (ou NULL). */
static int ImprimirAst(const Opcoes *o, Compilacao *c, const Semantica *sem, const AstRefs *refs)
{
    AstPedido pedido = {.programa = &c->parser.programa, .fonte = &c->fonte, .nomes = &c->nomes, .modulos = &c->modulos, .diag = &c->diag,
                        .sem = sem, .refs = refs, .no = o->no, .projeto = o->projeto, .sem_pos = o->sem_pos};
    Buf saida = {0};
    const char *erro = NULL;

    if (!AstImprimir(&pedido, &saida, &erro)) {
        fprintf(stderr, "forja: %s\n", erro);
        BufLiberar(&saida);
        return SAIDA_USO;
    }
    fwrite(saida.dados, 1, saida.tam, stdout);
    BufLiberar(&saida);
    return SAIDA_OK;
}

/* --tipos: a propria arvore analisada (com a reescrita da semantica), que tem de analisar sem erro. */
static int AstAnalisada(const Opcoes *o, Compilacao *c)
{
    AstRefs refs;
    int codigo;

    if (!AnalisarSintaxe(c, true) || !AnalisarSemantica(c)) return RelatarFalha(c);
    AstRefsMontar(&refs, &c->sem);
    codigo = ImprimirAst(o, c, &c->sem, &refs);
    AstRefsLiberar(&refs);
    return codigo;
}

/* Segunda compilacao do mesmo arquivo, com modulos e semantica, so para ler as ligacoes. NULL se a sintaxe ou a carga dos modulos falha (sem "ref"). */
static Compilacao *CompilarParaLigacoes(const Opcoes *o)
{
    Compilacao *a = calloc(1, sizeof *a);

    if (a == NULL) FalhaInterna("memoria esgotada alocando a compilacao das ligacoes");
    if (!FonteLer(&a->fonte, o->entrada)) {
        free(a);
        return NULL;
    }
    IniciarCompilacao(a);
    a->opcoes = o;
    a->sem.lista.teto = OpcoesMaxErros(o);
    if (!AnalisarSintaxe(a, true)) {
        LiberarCompilacao(a);
        free(a);
        return NULL;
    }
    AnalisarSemantica(a); /* o erro de semantica nao importa aqui: valem as ligacoes feitas antes dele */
    return a;
}

/* Sem --tipos: a AST do parser, sem a reescrita da semantica (embrulha, opc_teste, swizzle...), com as ligacoes da segunda compilacao. */
static int AstSintatica(const Opcoes *o, Compilacao *c)
{
    Compilacao *a;
    AstRefs refs;
    int codigo;

    if (!AnalisarSintaxe(c, o->projeto)) return RelatarFalha(c);
    a = CompilarParaLigacoes(o);
    if (a == NULL) return ImprimirAst(o, c, NULL, NULL);
    AstRefsMontar(&refs, &a->sem);
    codigo = ImprimirAst(o, c, NULL, &refs);
    AstRefsLiberar(&refs);
    LiberarCompilacao(a);
    free(a);
    return codigo;
}

int RodarAst(const Opcoes *o, Compilacao *c)
{
    return o->tipos ? AstAnalisada(o, c) : AstSintatica(o, c);
}

/* Caminho de buffer estatico (vive ate o fim do processo): <pasta>\main.fj. */
static char caminho_main[MAX_PATH + 16];

static bool EhPasta(const char *caminho)
{
    DWORD atributos = GetFileAttributesA(caminho);
    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool AstResolverEntrada(Opcoes *o)
{
    if (o->entrada == NULL) {
        fputs("forja: ast --projeto precisa do arquivo .fj de entrada ou da pasta do projeto\n", stderr);
        return false;
    }
    if (!EhPasta(o->entrada)) return true;
    snprintf(caminho_main, sizeof caminho_main, "%s\\main.fj", o->entrada);
    if (GetFileAttributesA(caminho_main) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "forja: a pasta %s nao tem main.fj (forja ast <pasta> usa o main.fj da pasta)\n", o->entrada);
        return false;
    }
    o->entrada = caminho_main;
    o->projeto = true;
    return true;
}
