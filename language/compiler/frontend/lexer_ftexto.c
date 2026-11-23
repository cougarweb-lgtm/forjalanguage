/* lexer_ftexto.c - f"..." (Linguagem v1.7, D16): pedacos de texto cru e, para cada '{', os tokens normais da expressao.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Separado de lexer.c 0.7.0 para tirar o bloco f"..." do layout do laco quente do lexer (a recursao e o codigo frio
 *                    pioravam o lex em ~2 ms no corpus de 200 mil linhas). Tokens emitidos identicos.
 * Verificacao: construir.bat && python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/lexer.h"
#include "common/buf.h"

static void EmpurrarToken(Lexer *lx, TokenTipo tipo, uint32_t off, uint32_t len, int32_t sym)
{
    Token *t = VecEmpurrarCru(&lx->tokens);
    t->off = off;
    t->len = (uint16_t)(len > UINT16_MAX ? UINT16_MAX : len);
    t->sym = sym;
    t->tipo = (uint8_t)tipo;
}

/* Estado de um f"...": inicio = offset do 'f'; j = leitura; ini_pedaco = onde comecou o pedaco de texto atual. */
typedef struct { Lexer *lx; Buf buf; uint32_t inicio, j, ini_pedaco; } FTexto;

static bool ErroFTextoSemFechar(Lexer *lx, uint32_t inicio)
{
    return DiagRelatar(lx->diag, E0001, inicio, "f\"...\" sem fechar na linha: falta a aspa de fechamento");
}

/* Fecha o pedaco de texto: sem bytes acumulados nao gera token. */
static void FecharPedacoDoFTexto(FTexto *f, uint32_t fim)
{
    if (f->buf.tam == 0) return;
    int32_t sym = Internar(f->lx->nomes, f->buf.dados, (uint32_t)f->buf.tam);
    EmpurrarToken(f->lx, TK_FTEXTO_PEDACO, f->ini_pedaco, fim - f->ini_pedaco, sym);
    f->buf.tam = 0;
}

/* ':formato' dentro de {...}: k aponta o ':'; o texto do formato (ate o '}') e internado sem validar (E0315 e da semantica). */
static bool LerFormatoDaChave(Lexer *lx, uint32_t *k)
{
    const char *s = lx->fonte->texto;
    uint32_t m = *k + 1;
    while (s[m] != '}' && s[m] != '"' && s[m] != '\n' && s[m] != '\0') m++;
    if (s[m] != '}') return DiagRelatar(lx->diag, E0001, *k, "formato sem '}' no f\"...\"");
    EmpurrarToken(lx, TK_FTEXTO_FORMATO, *k, m - *k, Internar(lx->nomes, s + *k + 1, m - *k - 1));
    *k = m;
    return true;
}

/* Um passo dentro de {...}: 0 = erro, 1 = continua, 2 = fechou o '}' (k aponta o '}'). */
static int PassoDaChave(Lexer *lx, uint32_t *k, uint32_t inicio)
{
    const char *s = lx->fonte->texto;
    char ch = s[*k];
    if (ch == '}') return 2;
    if (ch == ':') return LerFormatoDaChave(lx, k) ? 1 : 0;
    if (ch == '"' || (ch == 'f' && s[*k + 1] == '"')) {
        return DiagRelatar(lx->diag, E0001, *k + (ch == '"' ? 0 : 1),
                           "aspas dentro de {...} no f\"...\": guarde o texto numa variavel antes") ? 1 : 0;
    }
    if (ch == '{') {
        return DiagRelatar(lx->diag, E0001, *k, "'{' dentro de {...} no f\"...\": guarde o literal numa variavel antes") ? 1 : 0;
    }
    if (ch == '\n' || ch == '\0') return ErroFTextoSemFechar(lx, inicio) ? 1 : 0;
    return LexerLerTokenSimples(lx, k) ? 1 : 0;
}

/* '{' expr [':' formato] '}': empurra CHAVE, os tokens da expressao, FORMATO e FIM_CHAVE; *j termina depois do '}'. */
static bool LerChaveDoFTexto(Lexer *lx, uint32_t *j, uint32_t inicio)
{
    const char *s = lx->fonte->texto;
    uint32_t abre = *j, k = abre + 1;
    EmpurrarToken(lx, TK_FTEXTO_CHAVE, abre, 1, -1);
    while (s[k] == ' ' || s[k] == '\t') k++;
    if (s[k] == '}') return DiagRelatar(lx->diag, E0001, abre, "{} vazio no f\"...\": ponha uma expressao entre as chaves");
    for (;;) {
        while (s[k] == ' ' || s[k] == '\t') k++;
        int passo = PassoDaChave(lx, &k, inicio);
        if (passo == 0) return false;
        if (passo == 2) break;
    }
    EmpurrarToken(lx, TK_FTEXTO_FIM_CHAVE, k, 1, -1);
    *j = k + 1;
    return true;
}

/* Copia um byte (ou o par '\x', '{{', '}}') do texto para o pedaco; '}' sozinho e erro. */
static bool CopiarDoFTexto(FTexto *f)
{
    const char *s = f->lx->fonte->texto;
    uint32_t j = f->j;
    if (s[j] == '\\' && s[j + 1] != '\n') {
        BufAnexarN(&f->buf, s + j, 2);
        f->j += 2;
    } else if ((s[j] == '{' || s[j] == '}') && s[j + 1] == s[j]) {
        BufAnexarN(&f->buf, s + j, 1);
        f->j += 2;
    } else if (s[j] == '}') {
        return DiagRelatar(f->lx->diag, E0001, j, "'}' sozinho no f\"...\": escreva '}}' para uma chave");
    } else {
        BufAnexarN(&f->buf, s + j, 1);
        f->j++;
    }
    return true;
}

/* Um passo do texto: 0 = erro, 1 = continua, 2 = achou a aspa de fechamento. */
static int PassoDoFTexto(FTexto *f)
{
    const char *s = f->lx->fonte->texto;
    if (f->j >= (uint32_t)f->lx->fonte->tam || s[f->j] == '\n') return ErroFTextoSemFechar(f->lx, f->inicio) ? 1 : 0;
    if (s[f->j] == '"') return 2;
    if (s[f->j] == '{' && s[f->j + 1] != '{') {
        FecharPedacoDoFTexto(f, f->j);
        if (!LerChaveDoFTexto(f->lx, &f->j, f->inicio)) return 0;
        f->ini_pedaco = f->j;
        return 1;
    }
    return CopiarDoFTexto(f) ? 1 : 0;
}

/* f"...": INICIO, PEDACO/CHAVE..FIM_CHAVE em ordem, FIM. Sem fechar na linha = E0001 no 'f'. */
bool LexerLerFTexto(Lexer *lx, uint32_t *i)
{
    FTexto f = {.lx = lx, .inicio = *i, .j = *i + 2, .ini_pedaco = *i + 2};
    int passo = 1;
    EmpurrarToken(lx, TK_FTEXTO_INICIO, *i, 2, -1);
    while (passo == 1) passo = PassoDoFTexto(&f);
    if (passo == 2) {
        FecharPedacoDoFTexto(&f, f.j);
        EmpurrarToken(lx, TK_FTEXTO_FIM, f.j, 1, -1);
        *i = f.j + 1;
    }
    BufLiberar(&f.buf);
    return passo == 2;
}
