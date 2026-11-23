/* fmt_espaco.c - o espacamento do 'forja fmt': papel de cada token e a TABELA FECHADA de pares (R6 do plano f1/f2).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - B12: TK_HASH tem PAPEL_UN (sem espaco depois do '#' de #load).
 *            0.1.0 - F1 P1a: primeira versao.
 * Verificacao: construir.bat && python tests\fmt\verificar_fmt.py --forja compiler\forja.exe
 *
 * Unario x binario: decide o token ANTERIOR (depois de nome, literal, ')' ']' '}' de literal ou '^' pos-fixo e fora do inicio da linha = binario). O '^' pos-fixo (p^.x) tambem
 * olha o seguinte: '^' seguido de nome de tipo, 'proc' ou '?' e prefixo ([]^T). Par fora da tabela ('?'): quem chama mantem o vao original.
 */
#include <stdlib.h>

#include "driver/fmt_interno.h"

/* Linha = papel do token de antes; coluna = papel do de depois; mesma ordem de FmtPapel:
 * W K PR | LP RP LB RB | LK LL RK RL | CM CO DT RG | BI UN PO.   '0' = nenhum espaco, '1' = um espaco, '?' = fora da tabela (mantem o vao). */
static const char TABELA_PARES[PAPEL_QTD][PAPEL_QTD + 1] = {
    [PAPEL_W] = "111" "0000" "1010" "0000" "1?0",
    [PAPEL_K] = "111" "1010" "1110" "00??" "11?",
    [PAPEL_PR] = "1??" "0???" "????" "????" "???",
    [PAPEL_LP] = "000" "0000" "?0?0" "??0?" "?0?",
    [PAPEL_RP] = "111" "0000" "1?10" "0000" "1?0",
    [PAPEL_LB] = "000" "0000" "?0?0" "??00" "?0?",
    [PAPEL_RB] = "010" "0000" "1010" "0000" "100",
    [PAPEL_LK] = "111" "1?1?" "?1??" "????" "?1?",
    [PAPEL_LL] = "000" "0?0?" "?0?0" "????" "?0?",
    [PAPEL_RK] = "?1?" "????" "????" "????" "???",
    [PAPEL_RL] = "111" "?0?0" "1?10" "0000" "1?0",
    [PAPEL_CM] = "111" "1010" "1110" "????" "?1?",
    [PAPEL_CO] = "111" "1010" "?1?0" "0???" "?1?",
    [PAPEL_DT] = "0??" "????" "????" "????" "???",
    [PAPEL_RG] = "0??" "0??0" "????" "????" "???",
    [PAPEL_BI] = "111" "1?1?" "?1??" "??1?" "?1?",
    [PAPEL_UN] = "0?0" "0?0?" "????" "????" "???",
    [PAPEL_PO] = "?1?" "0000" "1?10" "0000" "1?0",
};

int FmtEspacoDoPar(FmtPapel antes, FmtPapel depois)
{
    char c = TABELA_PARES[antes][depois];

    return c == '0' ? 0 : c == '1' ? 1 : -1;
}

bool FmtPapelFecha(FmtPapel p)
{
    return p == PAPEL_RP || p == PAPEL_RB || p == PAPEL_RK || p == PAPEL_RL;
}

/* Termina um operando: um '-' ou '&' depois dele e binario. */
static bool FimDeOperando(FmtPapel p)
{
    return p == PAPEL_W || p == PAPEL_RP || p == PAPEL_RB || p == PAPEL_RL || p == PAPEL_PO;
}

/* Papel do token sem olhar o contexto; os ambiguos ('-' '&' '~' '^' '{' '}') voltam como PAPEL_UN e o contexto decide. */
static FmtPapel PapelDoTipo(uint8_t tipo)
{
    switch (tipo) {
    case TK_IDENT: case TK_INT: case TK_REAL: case TK_TEXTO: case TK_FTEXTO_INICIO: case KW_TRUE: case KW_FALSE: case KW_NONE: return PAPEL_W;
    case KW_PROC: return PAPEL_PR;
    case TK_LPAREN: return PAPEL_LP;
    case TK_RPAREN: return PAPEL_RP;
    case TK_LCOLCHETE: return PAPEL_LB;
    case TK_RCOLCHETE: return PAPEL_RB;
    case TK_COMMA: return PAPEL_CM;
    case TK_COLON: return PAPEL_CO;
    case TK_DOT: return PAPEL_DT;
    case TK_DOTDOT: case TK_DOTDOTLT: case TK_DOTDOTEQ: return PAPEL_RG;
    case TK_ASSIGN: case TK_PLUSEQ: case TK_MINUSEQ: case TK_ARROW: case TK_PLUS: case TK_STAR: case TK_SLASH: case TK_PERCENT: case TK_EQ: case TK_NE:
    case TK_LT: case TK_LE: case TK_GT: case TK_GE: case TK_PIPE: case TK_SHL: case TK_SHR: case TK_BANG: case KW_AND: case KW_OR: case KW_AS: case KW_IN:
    case KW_OR_ELSE: return PAPEL_BI;
    case TK_MINUS: case TK_AMP: case TK_TIL: case TK_INTERROGA: case TK_CARET: case TK_LBRACE: case TK_RBRACE: case TK_HASH: return PAPEL_UN;
    default: return PAPEL_K;
    }
}

/* O '^' e pos-fixo (p^.x) quando vem depois de operando e o seguinte nao e um nome de tipo, 'proc' ou '?'. */
static bool CaretPosFixo(const FmtToken *t, size_t i, size_t n)
{
    bool tipo_depois;

    if (t[i].primeiro || i == 0 || !FimDeOperando((FmtPapel)t[i - 1].papel)) return false;
    tipo_depois = i + 1 < n && !t[i + 1].primeiro && (t[i + 1].tipo == TK_IDENT || t[i + 1].tipo == KW_PROC || t[i + 1].tipo == TK_INTERROGA);
    return !tipo_depois;
}

/* A '{' que comeca em 'off' esta na lista de literais? A lista e a posicao dela avancam juntas (os dois crescem). */
static bool ChaveEhLiteral(const Vec *chaves, size_t *pos, uint32_t off)
{
    const uint32_t *lista = chaves->dados;

    while (*pos < chaves->n && lista[*pos] < off) (*pos)++;
    return *pos < chaves->n && lista[*pos] == off;
}

/* Papel dos tokens de ambiguidade resolvida pelo anterior: '-' '&' '~' (binario depois de operando) e '^'. */
static FmtPapel PapelAmbiguo(const FmtToken *t, size_t i, size_t n)
{
    bool depois_de_operando = !t[i].primeiro && i > 0 && FimDeOperando((FmtPapel)t[i - 1].papel);

    if (t[i].tipo == TK_CARET) return CaretPosFixo(t, i, n) ? PAPEL_PO : PAPEL_UN;
    if (t[i].tipo == TK_INTERROGA) return PAPEL_UN;
    return depois_de_operando ? PAPEL_BI : PAPEL_UN;
}

void FmtAtribuirPapeis(FmtToken *t, size_t n, const Vec *chaves_literal)
{
    bool *abertas = calloc(n + 1, sizeof *abertas); /* por '{' aberta: era de literal? */
    size_t topo = 0, pos_literal = 0;

    if (abertas == NULL) FalhaInterna("memoria esgotada atribuindo os papeis do fmt");
    for (size_t i = 0; i < n; i++) {
        FmtPapel p = PapelDoTipo(t[i].tipo);

        if (t[i].tipo == TK_LBRACE) {
            abertas[topo] = chaves_literal != NULL && ChaveEhLiteral(chaves_literal, &pos_literal, t[i].ini);
            p = abertas[topo++] ? PAPEL_LL : PAPEL_LK;
        } else if (t[i].tipo == TK_RBRACE) {
            p = topo > 0 && abertas[--topo] ? PAPEL_RL : PAPEL_RK;
        } else if (p == PAPEL_UN) {
            p = PapelAmbiguo(t, i, n);
        }
        t[i].papel = (uint8_t)p;
    }
    free(abertas);
}
