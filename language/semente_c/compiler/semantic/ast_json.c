/* ast_json.c - imprime a AST em JSON, um documento de uma linha (comando 'forja ast'): tipos, expressoes e comandos. Declaracoes e documento: ast_json_decl.c.
 * Versao: 0.17.1
 * Changelog: 0.17.1 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            0.17.0 - B12: ExTexto marca "load": true no EX_TEXTO de #load (Expr.carregar).
 *            0.16.0 - IA1-I2: AstNomeDaExpr (acessor de NOMES_EXPR para 'forja consultar tipo').
 *            0.15.0 - IA1-I1: AST 1.0 (ast_versao 1.0.0): "id" estrutural e "fim" em todo no, "ref" nos usos resolvidos (ast_json_ref.c), --sem-pos; Json foi para ast_json_interno.h e as declaracoes para ast_json_decl.c.
 *            0.14.0 - Linguagem v1.12 M3 P21v-a: no "chamada_ind" ("chamado", "args") e "nomes_params" no tipo proc; ast_versao continua 0.11.0 (so acrescenta um no e uma chave).
 *            0.13.0 -Linguagem v1.11 L15a: forma "lista_fixa" ([dynamic, N]T; "tamanho" como no array); ast_versao continua 0.11.0.
 *            0.12.0 - Linguagem v1.11 L9: no "static_assert" (D_ASSERT, chave "chamada"); ast_versao continua 0.11.0 (so acrescenta um no).
 *            0.11.0 - Linguagem v1.10: "simbolo" no proc foreign com alias (A6); literal 0x/0b sai com o texto como escrito (A2); ast_versao 0.11.0.
 *            0.10.0 -Linguagem v1.9 P2: TipoRefJson mapa/bit_set/soa/matriz (chave, valor, linhas, colunas) e indice2 do m[i, j]; ast_versao 0.10.0.
 *            0.9.0 - Linguagem v1.8b P2: TipoRefJson "proc" ("params", "retorno"), "externo" no struct opaco, "valor" nos membros de enum com valor; ast_versao 0.9.0.
 *            0.8.0 - Linguagem v1.8 P2a: no "import" ("caminho", "apelido"), chave "modulo" em Expr/TipoRef/Caso qualificados, so Decl do modulo 0; ast_versao 0.8.0.
 *            0.7.0 - Linguagem v1.7 P1: TipoRefJson "opcional", proc com "retornos", no "union", "let_multiplo", "tupla", "ftexto", "embrulha", "opc_teste", Caso "tipo_escrito"; ast_versao 0.7.0.
 *            0.6.0 - Linguagem v1.6 P1a: TipoRefJson forma "dinamico", no "tipo" (EX_TIPO, "tipo_escrito"); ast_versao 0.6.0.
 *            0.5.2 - Linguagem v1.5 P1c: for de colecao ("item", "por_ref", "indice", "colecao").
 *            0.5.1 - Linguagem v1.5 P1b: nos "indice", "fatia" e "literal_array".
 *            0.5.0 - Linguagem v1.5 P1a: TipoRefJson com "array" ("tamanho") e "fatia"; ast_versao 0.5.0.
 *            0.4.0 - Linguagem v1.4 P1: proc foreign ("externo": true, sem "corpo"), no "link" ("biblioteca"), ast_versao 0.4.0.
 *            0.3.0 - Linguagem v1.3: TipoRefJson recursivo ("forma"/"alvo" no lugar de "ponteiro"), "alvo" do 'as', EX_NONE, "deref", ast_versao 0.3.0.
 *            0.2.2 - Linguagem v1.2: nomes dos operadores de bits em NOMES_OP.
 *            0.2.1 - Linguagem v1.1: "publico" e "exportado" na declaracao (so quando verdadeiros).
 *            0.2.0 - Fase 5c: modo --tipos (AstImprimirJsonTipos): "tipo":"<texto>" em expressoes, let/var/for, parametros, campos e
 *                    const; a chave "tipo" que ja carregava o tipo ESCRITO vira "tipo_escrito" nesse modo (sem colisao).
 *            0.1.0 - Fase 5b: nos de expressao, comando e declaracao por tabela de funcoes; posicao [linha, coluna] via
 *                    tabela de inicios de linha + busca binaria (FonteLocalizar seria quadratico no arquivo todo).
 * Verificacao: construir.bat && forja.exe ast <arq.fj> | python -c "import json,sys; json.load(sys.stdin)"
 */
#include <ctype.h>

#include "semantic/ast_json_interno.h"

static const char *const NOMES_EXPR[] = {
    "int", "real", "bool", "texto", "nome", "campo", "chamada", "literal_struct", "unario", "binario",
    "conversao", "try", "or_else", "none", "indice", "fatia", "literal_array",
    "tipo",
    "embrulha", "opc_teste", "tupla", "ftexto", "chamada_ind",
};
const char *AstNomeDaExpr(uint8_t tipo_no)
{
    return NOMES_EXPR[tipo_no];
}

static const char *const NOMES_OP[] = {
    "ou", "e", "igual", "diferente", "menor", "menor_igual", "maior", "maior_igual", "soma", "sub", "mul",
    "div", "resto", "negar", "nao", "endereco", "bit_e", "bit_ou", "bit_xou", "desloca_esq", "desloca_dir", "bit_nao",
    "deref",
};
static const char *const NOMES_STMT[] = {
    "let", "var", "atribuicao", "if", "while", "for", "return", "chamada", "break", "continue", "defer",
    "switch", "expect", "let_multiplo",
};
static void ExprJson(Json *j, const Expr *e);
static void StmtJson(Json *j, const Stmt *s);

/* ---------- pecas basicas ---------- */

void MapearLinhas(Json *j)
{
    const Fonte *f = j->fonte;
    VecIniciar(&j->linhas, sizeof(uint32_t));
    VecEmpurrar(&j->linhas);
    for (size_t i = 0; i < f->tam; i++) {
        if (f->texto[i] == '\n') *(uint32_t *)VecEmpurrar(&j->linhas) = (uint32_t)i + 1;
    }
}

static void PosJson(Json *j, uint32_t off)
{
    const uint32_t *inicio = j->linhas.dados;
    size_t lo = 0, hi = j->linhas.n;
    while (hi - lo > 1) {
        size_t meio = (lo + hi) / 2;
        if (inicio[meio] <= off) lo = meio; else hi = meio;
    }
    BufFormatar(j->b, "[%zu, %u]", lo + 1 - j->linha_base, off - inicio[lo] + 1); /* linha local ao arquivo do modulo */
}

/* Texto entre aspas, com escape; '\' vira '/' quando e caminho de arquivo. */
void TextoJson(Json *j, const char *s, size_t n, bool caminho)
{
    BufAnexarN(j->b, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (caminho && ch == '\\') BufAnexarN(j->b, "/", 1);
        else if (ch == '"' || ch == '\\') { BufAnexarN(j->b, "\\", 1); BufAnexarN(j->b, s + i, 1); }
        else if (ch < 0x20) BufFormatar(j->b, "\\u%04x", ch);
        else BufAnexarN(j->b, s + i, 1);
    }
    BufAnexarN(j->b, "\"", 1);
}

void Chave(Json *j, const char *chave)
{
    BufFormatar(j->b, ", \"%s\": ", chave);
}

void NomeJson(Json *j, int32_t id)
{
    if (id < 0) BufAnexar(j->b, "null");
    else TextoJson(j, InternerTexto(j->nomes, id), InternerTam(j->nomes, id), false);
}

void ChaveNome(Json *j, const char *chave, int32_t id)
{
    Chave(j, chave);
    NomeJson(j, id);
}

void ChaveBool(Json *j, const char *chave, bool valor)
{
    Chave(j, chave);
    BufAnexar(j->b, valor ? "true" : "false");
}

/* ", "<chave>": [l, c]"; em --sem-pos nenhuma posicao sai. */
void ChavePos(Json *j, const char *chave, uint32_t off)
{
    if (j->sem_pos) return;
    Chave(j, chave);
    PosJson(j, off);
}

/* "id" do no em escrita (caminho estrutural, D-IA1-20), "pos" e "fim" [l, c] (fim exclusivo, D-IA1-21). */
static void IdPosFim(Json *j, uint32_t off, uint32_t fim)
{
    BufAnexar(j->b, "\"id\": ");
    TextoJson(j, j->id.dados, j->id.tam, false);
    ChavePos(j, "pos", off);
    ChavePos(j, "fim", fim);
}

/* Abre um no: {"no": "<tipo>", "id": "...", "pos": [l, c], "fim": [l, c] ; quem chama fecha com '}'. */
void AbreNo(Json *j, const char *tipo, uint32_t off, uint32_t fim)
{
    BufFormatar(j->b, "{\"no\": \"%s\", ", tipo);
    IdPosFim(j, off, fim);
}

void AbreObjeto(Json *j, uint32_t off, uint32_t fim)
{
    BufAnexarN(j->b, "{", 1);
    IdPosFim(j, off, fim);
}

void ChaveExpr(Json *j, const char *chave, const Expr *e)
{
    size_t antes = IdEmpurrar(j, chave, -1);
    Chave(j, chave);
    ExprJson(j, e);
    IdRestaurar(j, antes);
}

/* "modulo": texto do apelido (v1.8), lido cru da fonte em 'off' (so [a-z0-9_]). */
static void ChaveModuloEm(Json *j, uint32_t off)
{
    const char *s = j->fonte->texto + off;
    size_t n = 0;
    while ((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '_') n++;
    Chave(j, "modulo");
    TextoJson(j, s, n, false);
}

/* Cada comando leva o indice no id: "corpo[2]". */
static void ListaStmtJson(Json *j, const char *chave, const Stmt *s)
{
    int indice = 0;
    BufAnexarN(j->b, "[", 1);
    for (const Stmt *x = s; x != NULL; x = x->prox, indice++) {
        size_t antes = IdEmpurrar(j, chave, indice);
        if (x != s) BufAnexar(j->b, ", ");
        StmtJson(j, x);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
}

void ChaveCorpo(Json *j, const char *chave, const Stmt *s)
{
    Chave(j, chave);
    ListaStmtJson(j, chave, s);
}

static const char *const NOMES_FORMA[] = {"nome", "ponteiro", "anulavel", "array", "fatia", "dinamico", "opcional", "tupla", "proc", "mapa", "bit_set", "soa", "matriz", "lista_fixa"};

/* "tamanho": texto escrito do N do array (digitos ou nome de const), recortado da fonte em pos_tamanho. */
static void TamanhoDoArrayJson(Json *j, const TipoRef *t)
{
    const char *s = j->fonte->texto + t->pos_tamanho;
    size_t n = 0;
    while ((s[n] >= '0' && s[n] <= '9') || (s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= 'a' && s[n] <= 'z') || s[n] == '_') n++;
    Chave(j, "tamanho");
    TextoJson(j, s, n, false);
}

/* Tipo dentro de outro tipo: o segmento entra no id (".../alvo"). */
static void FilhoTipo(Json *j, const char *segmento, const TipoRef *t)
{
    size_t antes = IdEmpurrar(j, segmento, -1);
    TipoRefJson(j, t);
    IdRestaurar(j, antes);
}

/* v1.8b: TR_PROC: "params": [TipoRef de alvo[1..nome]], "retorno": TipoRef de alvo[0] ou null. */
static void ProcDoTipoJson(Json *j, const TipoRef *t)
{
    Chave(j, "params");
    BufAnexarN(j->b, "[", 1);
    for (int32_t i = 1; i <= t->nome; i++) {
        size_t antes = IdEmpurrar(j, "params", i - 1);
        if (i > 1) BufAnexar(j->b, ", ");
        TipoRefJson(j, &t->alvo[i]);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
    Chave(j, "retorno");
    if (t->pos_tamanho == 1) FilhoTipo(j, "retorno", &t->alvo[0]); else BufAnexar(j->b, "null");
    Chave(j, "nomes_params"); /* v1.12: alvo[nome + k] */
    BufAnexarN(j->b, "[", 1);
    for (int32_t i = 1; i <= t->nome; i++) {
        if (i > 1) BufAnexar(j->b, ", ");
        NomeJson(j, t->alvo[t->nome + i].nome);
    }
    BufAnexarN(j->b, "]", 1);
}

/* v1.9: TR_MATRIZ: "linhas": R, "colunas": C (numeros JSON, de t->nome = (R << 4) | C). */
static void DimensoesDaMatrizJson(Json *j, const TipoRef *t)
{
    BufFormatar(j->b, ", \"linhas\": %d, \"colunas\": %d", t->nome >> 4, t->nome & 15);
}

/* {"id", "pos", "fim", "forma"?: "ponteiro"|"anulavel"|"array"|"fatia"|"dinamico", "tamanho"?, "alvo"?: {...}, "nome"?, "erro"?} (recursivo em "alvo") */
void TipoRefJson(Json *j, const TipoRef *t)
{
    AbreObjeto(j, t->pos, AstFimTipo(&j->fim, t));
    if (t->modulo != 0) ChaveModuloEm(j, t->pos);
    RefDoTipo(j, t);
    if (t->forma != TR_NOME) {
        Chave(j, "forma");
        BufFormatar(j->b, "\"%s\"", NOMES_FORMA[t->forma]);
        if (t->forma == TR_PROC) {
            ProcDoTipoJson(j, t);
        } else if (t->forma == TR_MAPA) {
            Chave(j, "chave");
            FilhoTipo(j, "chave", &t->alvo[0]);
            Chave(j, "valor");
            FilhoTipo(j, "valor", &t->alvo[1]);
        } else {
            if (t->forma == TR_MATRIZ) DimensoesDaMatrizJson(j, t);
            if (t->forma == TR_ARRAY || t->forma == TR_LISTA_FIXA) TamanhoDoArrayJson(j, t);
            Chave(j, "alvo");
            FilhoTipo(j, "alvo", t->alvo);
        }
    }
    if (t->forma == TR_NOME && t->nome >= 0) ChaveNome(j, "nome", t->nome);
    if (t->erro >= 0) ChaveNome(j, "erro", t->erro);
    BufAnexarN(j->b, "}", 1);
}

/* No modo --tipos, "tipo" e o tipo resolvido (texto); o tipo escrito passa a "tipo_escrito". */
static const char *ChaveDoTipoEscrito(const Json *j)
{
    return j->tipos ? "tipo_escrito" : "tipo";
}

/* O segmento do id e sempre "tipo": o id do tipo escrito nao muda entre o modo comum e o --tipos. */
void ChaveTipo(Json *j, const TipoRef *t)
{
    Chave(j, ChaveDoTipoEscrito(j));
    FilhoTipo(j, "tipo", t);
}

/* "<chave>": "<texto do tipo>" so no modo --tipos e se a semantica resolveu o tipo. */
void ChaveTipoTexto(Json *j, const char *chave, TipoId tipo)
{
    if (!j->tipos || tipo == TIPO_NAO_RESOLVIDO) return;
    Chave(j, chave);
    BufAnexarN(j->b, "\"", 1);
    TipoAnexarTexto(&j->sem->tipos, tipo, j->b);
    BufAnexarN(j->b, "\"", 1);
}

/* ---------- expressoes ---------- */

static void ExNumero(Json *j, const Expr *e)
{
    const char *s = j->fonte->texto + e->pos_op;
    size_t n = 0;
    if (e->tipo_no == EX_INT && s[0] == '0' && (s[1] == 'x' || s[1] == 'b')) { /* v1.10: o texto do literal vai como foi escrito */
        n = 2;
        while (isalnum((unsigned char)s[n])) n++;
    }
    while (s[n] >= '0' && s[n] <= '9') n++;
    if (s[n] == '.' && s[n + 1] >= '0' && s[n + 1] <= '9') {
        n++;
        while (s[n] >= '0' && s[n] <= '9') n++;
    }
    Chave(j, "texto");
    TextoJson(j, s, n, false);
}

static void ExBool(Json *j, const Expr *e)
{
    ChaveBool(j, "valor", e->op != 0);
}

/* B12: o #load traz "load": true; "texto" e o caminho antes de ResolverLoads e o conteudo depois (forja ast --tipos). */
static void ExTexto(Json *j, const Expr *e)
{
    ChaveNome(j, "texto", e->nome);
    if (e->carregar) ChaveBool(j, "load", true);
}

static void ExNome(Json *j, const Expr *e)
{
    ChaveNome(j, "nome", e->nome);
}

static void ExCampo(Json *j, const Expr *e)
{
    ChaveNome(j, "nome", e->nome);
    ChaveExpr(j, "base", e->a);
}

/* Expressao que e o elemento 'indice' de uma lista: o id leva ".../chave[i]". */
static void ElementoExpr(Json *j, const char *chave, int indice, const Expr *e)
{
    size_t antes = IdEmpurrar(j, chave, indice);
    ExprJson(j, e);
    IdRestaurar(j, antes);
}

static void ArgsListaJson(Json *j, const Expr *e, const char *chave)
{
    Chave(j, chave);
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < e->nargs; i++) {
        const Arg *a = &e->args[i];
        size_t antes = IdEmpurrar(j, chave, (int)i);
        BufAnexar(j->b, i ? ", {" : "{");
        if (a->nome >= 0) {
            BufAnexar(j->b, "\"nome\": ");
            NomeJson(j, a->nome);
            BufAnexar(j->b, ", ");
        }
        BufAnexar(j->b, "\"valor\": ");
        ElementoExpr(j, "valor", -1, a->valor);
        BufAnexarN(j->b, "}", 1);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
}

static void ArgsJson(Json *j, const Expr *e, const char *chave)
{
    ChaveNome(j, "nome", e->nome);
    ArgsListaJson(j, e, chave);
}

static void ExChamada(Json *j, const Expr *e)
{
    ArgsJson(j, e, "args");
}

/* v1.12: chamada sobre expressao de tipo proc: "chamado" e "args". */
static void ExChamadaInd(Json *j, const Expr *e)
{
    ChaveExpr(j, "chamado", e->a);
    ArgsListaJson(j, e, "args");
}

static void ExLiteralStruct(Json *j, const Expr *e)
{
    ArgsJson(j, e, "campos");
}

static void ExUnario(Json *j, const Expr *e)
{
    ChaveExpr(j, "operando", e->a);
}

static void ExBinario(Json *j, const Expr *e)
{
    ChavePos(j, "op_pos", e->pos_op);
    ChaveExpr(j, "a", e->a);
    ChaveExpr(j, "b", e->b);
}

static void ExConversao(Json *j, const Expr *e)
{
    Chave(j, "alvo");
    FilhoTipo(j, "alvo", e->alvo);
    ChaveExpr(j, "valor", e->a);
}

static void ExNone(Json *j, const Expr *e)
{
    (void)j;
    (void)e;
}

static void ExTry(Json *j, const Expr *e)
{
    ChaveExpr(j, "valor", e->a);
}

static void ExOrElse(Json *j, const Expr *e)
{
    ChaveExpr(j, "a", e->a);
    ChaveExpr(j, "b", e->b);
}

static void ExIndice(Json *j, const Expr *e)
{
    ChaveExpr(j, "base", e->a);
    ChaveExpr(j, "indice", e->b);
    if (e->nargs == 1) ChaveExpr(j, "indice2", e->args[0].valor);
}

static void ExFatia(Json *j, const Expr *e)
{
    static const char *const LIMITES[] = {"exclusivo", "inclusivo", "inteira"};
    ChaveExpr(j, "base", e->a);
    if (e->nargs == 2) {
        ChaveExpr(j, "inicio", e->args[0].valor);
        ChaveExpr(j, "ate", e->args[1].valor); /* AST 1.0: era "fim", que agora e a posicao final de todo no */
    }
    Chave(j, "limite");
    BufFormatar(j->b, "\"%s\"", LIMITES[e->op]);
}

static void ExLiteralArray(Json *j, const Expr *e)
{
    Chave(j, "elementos");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (i) BufAnexar(j->b, ", ");
        ElementoExpr(j, "elementos", (int)i, e->args[i].valor);
    }
    BufAnexarN(j->b, "]", 1);
}

static void ExTipo(Json *j, const Expr *e)
{
    Chave(j, "tipo_escrito");
    FilhoTipo(j, "tipo", e->alvo);
}

static void ExEmbrulha(Json *j, const Expr *e)
{
    ChaveExpr(j, "valor", e->a);
}

static void ExOpcTeste(Json *j, const Expr *e)
{
    ChaveBool(j, "e_none", e->op != 0);
    ChaveExpr(j, "valor", e->a);
}

static void ExTupla(Json *j, const Expr *e)
{
    Chave(j, "valores");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (i) BufAnexar(j->b, ", ");
        ElementoExpr(j, "valores", (int)i, e->args[i].valor);
    }
    BufAnexarN(j->b, "]", 1);
}

/* "partes": [{"texto": "..."} | {"valor": {...}, "formato"?: "08.3"}] */
static void ExFTexto(Json *j, const Expr *e)
{
    Chave(j, "partes");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < e->nargs; i++) {
        const Arg *a = &e->args[i];
        size_t antes = IdEmpurrar(j, "partes", (int)i);
        if (i) BufAnexar(j->b, ", ");
        if (a->valor->tipo_no == EX_TEXTO && a->nome < 0 && a->indice < 0) {
            BufAnexar(j->b, "{\"texto\": ");
            NomeJson(j, a->valor->nome);
        } else {
            BufAnexar(j->b, "{\"valor\": ");
            ElementoExpr(j, "valor", -1, a->valor);
            if (a->nome >= 0) ChaveNome(j, "formato", a->nome);
        }
        BufAnexarN(j->b, "}", 1);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
}

typedef void (*ImprimeExpr)(Json *j, const Expr *e);

static const ImprimeExpr EXPRESSOES[EX_CHAMADA_IND + 1] = {
    [EX_CHAMADA_IND] = ExChamadaInd, [EX_EMBRULHA] = ExEmbrulha, [EX_OPC_TESTE] = ExOpcTeste, [EX_TUPLA] = ExTupla, [EX_FTEXTO] = ExFTexto,
    [EX_TIPO] = ExTipo,
    [EX_INDICE] = ExIndice, [EX_FATIA] = ExFatia, [EX_LITERAL_ARRAY] = ExLiteralArray,
    [EX_INT] = ExNumero, [EX_REAL] = ExNumero, [EX_BOOL] = ExBool, [EX_TEXTO] = ExTexto, [EX_NOME] = ExNome,
    [EX_CAMPO] = ExCampo, [EX_CHAMADA] = ExChamada, [EX_LITERAL_STRUCT] = ExLiteralStruct, [EX_UNARIO] = ExUnario,
    [EX_BINARIO] = ExBinario, [EX_CONVERSAO] = ExConversao, [EX_TRY] = ExTry, [EX_OR_ELSE] = ExOrElse, [EX_NONE] = ExNone,
};

/* Operador por nome: unario e binario escrevem "op" antes dos filhos. */
static void OperadorJson(Json *j, const Expr *e)
{
    Chave(j, "op");
    BufFormatar(j->b, "\"%s\"", NOMES_OP[e->op]);
}

static void ExprJson(Json *j, const Expr *e)
{
    AbreNo(j, NOMES_EXPR[e->tipo_no], e->pos, AstFimExpr(&j->fim, e));
    ChaveTipoTexto(j, "tipo", e->tipo);
    if (e->modulo != 0) ChaveModuloEm(j, e->pos);
    RefDaExpr(j, e);
    if (e->tipo_no == EX_UNARIO || e->tipo_no == EX_BINARIO) OperadorJson(j, e);
    EXPRESSOES[e->tipo_no](j, e);
    BufAnexarN(j->b, "}", 1);
}

/* ---------- comandos ---------- */

typedef void (*ImprimeStmt)(Json *j, const Stmt *s);

static void StLetVar(Json *j, const Stmt *s)
{
    LocalRegistrar(j, s->pos);
    ChaveNome(j, "nome", s->nome);
    ChaveTipoTexto(j, "tipo", s->tipo_local);
    if (s->tem_tipo) ChaveTipo(j, &s->tipo);
    ChaveExpr(j, "valor", s->expr);
}

static void StAtribuicao(Json *j, const Stmt *s)
{
    static const char *const OPS[] = {"=", "+=", "-="};
    Chave(j, "op");
    BufFormatar(j->b, "\"%s\"", OPS[s->op]);
    ChaveExpr(j, "alvo", s->alvo);
    ChaveExpr(j, "valor", s->expr);
}

static void StIf(Json *j, const Stmt *s)
{
    ChaveExpr(j, "cond", s->expr);
    ChaveCorpo(j, "entao", s->corpo);
    if (s->senao != NULL) ChaveCorpo(j, "senao", s->senao);
}

static void StWhile(Json *j, const Stmt *s)
{
    ChaveExpr(j, "cond", s->expr);
    ChaveCorpo(j, "corpo", s->corpo);
}

/* for de colecao: {"item", "por_ref", "indice"?, "colecao", "tipo"?, "corpo"} */
static void StForColecao(Json *j, const Stmt *s)
{
    ChaveNome(j, "item", s->nome);
    ChaveBool(j, "por_ref", s->item_ref);
    if (s->nome_indice >= 0) ChaveNome(j, "indice", s->nome_indice);
    ChaveExpr(j, "colecao", s->expr);
    ChaveTipoTexto(j, "tipo", s->tipo_local);
    ChaveCorpo(j, "corpo", s->corpo);
}

static void StFor(Json *j, const Stmt *s)
{
    LocalRegistrar(j, s->pos);
    if (s->expr2 == NULL) {
        StForColecao(j, s);
        return;
    }
    ChaveNome(j, "var", s->nome);
    ChaveTipoTexto(j, "tipo", s->tipo_local);
    ChaveExpr(j, "de", s->expr);
    ChaveExpr(j, "ate", s->expr2);
    ChaveBool(j, "inclusivo", !s->faixa_excl);
    ChaveCorpo(j, "corpo", s->corpo);
}

static void StReturn(Json *j, const Stmt *s)
{
    if (s->expr != NULL) ChaveExpr(j, "valor", s->expr);
}

static void StChamada(Json *j, const Stmt *s)
{
    ChaveExpr(j, "expr", s->expr);
}

static void StSemCampos(Json *j, const Stmt *s)
{
    (void)j;
    (void)s;
}

static void StDefer(Json *j, const Stmt *s)
{
    size_t antes = IdEmpurrar(j, "comando", -1);
    Chave(j, "comando");
    StmtJson(j, s->corpo);
    IdRestaurar(j, antes);
}

static void CasoJson(Json *j, const Caso *c)
{
    AbreObjeto(j, c->pos, AstFimCaso(&j->fim, c));
    LocalRegistrar(j, c->pos);
    RefDoCaso(j, c);
    ChaveBool(j, "padrao", c->padrao);
    ChaveTipoTexto(j, "tipo", c->tipo);
    if (c->modulo != 0) { /* o apelido vem logo depois do 'case' */
        const char *s = j->fonte->texto + c->pos + 4;
        while (*s == ' ' || *s == '\t') s++;
        ChaveModuloEm(j, (uint32_t)(s - j->fonte->texto));
    }
    if (c->tipo_escrito != NULL) {
        Chave(j, "tipo_escrito");
        FilhoTipo(j, "tipo", c->tipo_escrito);
    } else if (!c->padrao) {
        ChaveNome(j, ChaveDoTipoEscrito(j), c->tipo_nome);
    }
    if (c->membro >= 0) ChaveNome(j, "membro", c->membro);
    ChaveCorpo(j, "corpo", c->corpo);
    BufAnexarN(j->b, "}", 1);
}

static void StSwitch(Json *j, const Stmt *s)
{
    ChaveExpr(j, "valor", s->expr);
    if (s->nome >= 0) ChaveNome(j, "liga", s->nome);
    Chave(j, "casos");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < s->ncasos; i++) {
        size_t antes = IdEmpurrar(j, "casos", (int)i);
        if (i) BufAnexar(j->b, ", ");
        CasoJson(j, &s->casos[i]);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
}

static void StExpect(Json *j, const Stmt *s)
{
    ChaveExpr(j, "cond", s->expr);
}

/* {"mutavel": bool, "nomes": ["q", "_"], "valor": {...}} */
static void StLetMultiplo(Json *j, const Stmt *s)
{
    LocalRegistrar(j, s->pos);
    ChaveBool(j, "mutavel", s->op != 0);
    Chave(j, "nomes");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < s->nnomes; i++) {
        if (i) BufAnexar(j->b, ", ");
        NomeJson(j, s->nomes[i].nome);
    }
    BufAnexarN(j->b, "]", 1);
    ChaveExpr(j, "valor", s->expr);
}

static const ImprimeStmt COMANDOS[ST_LET_MULTIPLO + 1] = {
    [ST_LET_MULTIPLO] = StLetMultiplo,
    [ST_LET] = StLetVar, [ST_VAR] = StLetVar, [ST_ATRIBUICAO] = StAtribuicao, [ST_IF] = StIf,
    [ST_WHILE] = StWhile, [ST_FOR] = StFor, [ST_RETURN] = StReturn, [ST_CHAMADA] = StChamada,
    [ST_BREAK] = StSemCampos, [ST_CONTINUE] = StSemCampos, [ST_DEFER] = StDefer, [ST_SWITCH] = StSwitch,
    [ST_EXPECT] = StExpect,
};

static void StmtJson(Json *j, const Stmt *s)
{
    AbreNo(j, NOMES_STMT[s->tipo_no], s->pos, AstFimStmt(&j->fim, s));
    COMANDOS[s->tipo_no](j, s);
    BufAnexarN(j->b, "}", 1);
}
