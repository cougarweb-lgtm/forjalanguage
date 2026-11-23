/* lexer.c - tokeniza o arquivo .fj inteiro (E0001 no primeiro erro).
 * Versao: 0.17.0
 * Changelog: 0.17.0 - B12: '#' vira TK_HASH (inicio de #load("arquivo")).
 *            0.16.0 - B01a: Allocator pre-internado (NOMES_EMBUTIDOS).
 *            0.15.0 - IA1-L3d2: Lexer.comentarios (opcional): cada '//' pulado grava {inicio, fim} (o forja migrar --comentarios le o texto dos comentarios).
 *            0.14.0 - IA1-L1: ';' (e o for de C) e a barra-asterisco viram E0110 (forma de outra linguagem; common/idiomas.c) em vez de E0001; ComentarioDeBlocoAlheio pula o bloco inteiro.
 *            0.13.0 - IA1-b15: varios E0001 por execucao (Lexer.erros; RecuperarDaLinha pula o resto da linha com erro).
 *            0.12.0 - IA1-b04: E0001 do ';' traz a sugestao aplicavel (PontoEVirgulaInvalido: remover o ';' do fim da linha = certa; no meio da linha = so dica) e a causa diz o que a Forja faz no lugar.
 *            0.11.0 - Linguagem v1.11 L14a: quat pre-internado (NOMES_EMBUTIDOS).
 *            0.10.0 - Linguagem v1.11 L9: size_of, align_of, offset_of e static_assert pre-internados (NOMES_EMBUTIDOS).
 *            0.9.0 - Linguagem v1.9 P2: map, bit_set, soa e matrix pre-internados (NOMES_EMBUTIDOS).
 *            0.8.1 - Linguagem v1.8 P9: LexerAnalisar com aligned(64) (o mesmo codigo corria ~0,35 ms mais devagar por causa do endereco).
 *            0.8.0 -Linguagem v1.8 P1: LexerAnalisar parte de lx->inicio (trecho anexado) e reaproveita o vetor de tokens.
 *            0.7.1 - Desempenho: o bloco f"..." sai para lexer_ftexto.c (LexerLerFTexto) e LerUm volta a ser estatica e sem recursao;
 *                    a expressao dentro de {...} usa LexerLerTokenSimples. O teste do 'f' fica em LerNome, depois do nome.
 *                    Causa da regressao (+2,5 ms no lex): LexerLerTokenSimples deu a LerUm um 2o chamador e o gcc deixou de
 *                    embutir FimDoNumero/CasarSimbolo/LerTexto/LerNome (o 'tipo' e o 'i' iam por memoria); por isso estes
 *                    ficam always_inline. Medido: lex v1.6 13,0 ms, 0.7.1 12,5 ms. Mesmos tokens.
 *            0.7.0 - Linguagem v1.7 P1: f"..." (LerFTexto, LerChaveDoFTexto): tokens TK_FTEXTO_* (D16); 'f' colado na aspa abre o texto formatado.
 *            0.6.0 - Linguagem v1.6 P1a: NOMES_EMBUTIDOS ganha "new", "make", "dynamic" (SYM_NEW, SYM_MAKE, SYM_DYNAMIC).
 *            0.5.0 - Linguagem v1.5 P1a: '[' e ']' viram TK_LCOLCHETE e TK_RCOLCHETE.
 *            0.4.0 - Linguagem v1.4 P1: NOMES_EMBUTIDOS ganha "link" (SYM_LINK).
 *            0.3.0 - Linguagem v1.3: '?' vira TK_INTERROGA; NOMES_EMBUTIDOS ganha str, cstr, rawptr.
 *            0.2.0 - Linguagem v1.2: '|', '~', '<<' e '>>' viram tokens (TK_PIPE, TK_TIL, TK_SHL, TK_SHR).
 *            0.1.3 - Linguagem v1.2: NOMES_EMBUTIDOS ganha i8 i16 i64 u8 u16 u32 u64 uint f64 (ids no fim dos SYM_*).
 *            0.1.2 - Fase 5i: guardas do contrato da Fonte (tamanho <= FONTE_TAM_MAX e NUL final); byte >= 0x80 fora de texto
 *                    explica que nome e ASCII (acento so dentro de aspas).
 *            0.1.1 - Fase 5i: vetor de tokens reservado de uma vez (n/2 + 64) e push sem memset (VecEmpurrarCru).
 *            0.1.0 - Fase 5b: reescrito sobre Fonte/Interner/Vec/Diag a partir do lexer do prototipo; texto "..." vira
 *                    TK_TEXTO (conteudo cru internado, escape \x pula 1 caractere); '!' vira TK_BANG; 34 palavras reservadas.
 * Verificacao: construir.bat && forja.exe check <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/lexer.h"
#include "common/buf.h"
#include "common/idiomas.h"

static const char *const PALAVRAS_CHAVE[KW_QTD] = {
    "const", "let", "var", "proc", "struct", "enum", "union", "unit", "pub", "export", "import", "as",
    "foreign", "test", "if", "else", "for", "in", "while", "switch", "case", "default", "break",
    "continue", "return", "defer", "try", "or_else", "and", "or", "not", "true", "false", "none",
};

static const char *const NOMES_EMBUTIDOS[SYM_QTD - KW_QTD] = {
    "i32", "int", "f32", "bool", "print", "main", "expect", "i8", "i16", "i64", "u8", "u16", "u32", "u64", "uint", "f64", "str", "cstr", "rawptr", "link",
    "new", "make", "dynamic", "map", "bit_set", "soa", "matrix", "size_of", "align_of", "offset_of", "static_assert", "quat", "Allocator",
};

void LexerNomesIniciais(Interner *nomes)
{
    for (int i = 0; i < KW_QTD; i++) Internar(nomes, PALAVRAS_CHAVE[i], (uint32_t)strlen(PALAVRAS_CHAVE[i]));
    for (int i = 0; i < SYM_QTD - KW_QTD; i++) Internar(nomes, NOMES_EMBUTIDOS[i], (uint32_t)strlen(NOMES_EMBUTIDOS[i]));
}

static bool EhDigito(char ch) { return ch >= '0' && ch <= '9'; }
static bool EhInicioDeNome(char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'; }
static bool EhParteDeNome(char ch) { return EhInicioDeNome(ch) || EhDigito(ch); }

static void EmpurrarToken(Lexer *lx, TokenTipo tipo, uint32_t off, uint32_t len, int32_t sym)
{
    Token *t = VecEmpurrarCru(&lx->tokens); /* os 4 campos abaixo sao todos os que existem: nao precisa zerar */
    t->off = off;
    t->len = (uint16_t)(len > UINT16_MAX ? UINT16_MAX : len);
    t->sym = sym;
    t->tipo = (uint8_t)tipo;
}

/* Quebra de linha vira 1 token NL; linhas em branco e comentarios colapsam no primeiro. */
static void EmpurrarQuebra(Lexer *lx, uint32_t off)
{
    const Vec *v = &lx->tokens;
    if (v->n == 0 || ((const Token *)VecEm(v, v->n - 1))->tipo == TK_NL) return;
    EmpurrarToken(lx, TK_NL, off, 1, -1);
}

/* Fim do numero que comeca em i; *real = tem parte fracionaria ("1.5"; "1." e ".5" nao existem). */
static inline __attribute__((always_inline)) uint32_t FimDoNumero(const char *s, uint32_t i, bool *real)
{
    uint32_t j = i;
    while (EhDigito(s[j])) j++;
    *real = s[j] == '.' && EhDigito(s[j + 1]);
    if (!*real) return j;
    j++;
    while (EhDigito(s[j])) j++;
    return j;
}

static bool EhPrefixoDeNumero(char a, char b) { return a == '0' && (b == 'x' || b == 'b'); }

static bool EhDigitoHex(char ch) { return EhDigito(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }

/* v1.10 (A2): 0x... e 0b... A partir de texto[i] = '0'. Le todo o corte [A-Za-z0-9_] colado e confere os digitos da base: um
 * digito de fora ('0b12', '0xFG') ou nenhum digito ('0x') e E0001 no proprio literal, em vez de virar dois tokens e um E0101 enganoso. */
static __attribute__((noinline)) bool LerNumeroPrefixado(Lexer *lx, uint32_t *i)
{
    const char *s = lx->fonte->texto;
    bool hex = s[*i + 1] == 'x';
    uint32_t j = *i + 2;
    while (EhParteDeNome(s[j])) j++;
    if (j == *i + 2) return DiagRelatar(lx->diag, E0001, *i, "faltam digitos depois de '0%c'", s[*i + 1]);
    for (uint32_t k = *i + 2; k < j; k++) {
        if (hex ? EhDigitoHex(s[k]) : (s[k] == '0' || s[k] == '1')) continue;
        return DiagRelatar(lx->diag, E0001, k, "'%c' nao e digito %s (use %s)", s[k], hex ? "hexadecimal" : "binario",
                           hex ? "0-9 e a-f" : "0 e 1");
    }
    EmpurrarToken(lx, TK_INT, *i, j - *i, -1);
    *i = j;
    return true;
}

/* Valor de um literal inteiro: decimal, 0x... ou 0b... (minusculo). Le ate o 1o caractere que nao for digito da base.
 * false = passa de 64 bits sem sinal. Unico leitor: sem_expr, sem_call, lower_const, sem_tipo e o parser (limite de faixa) usam este. */
bool LexerLerInteiroU64(const char *s, uint64_t *valor)
{
    uint64_t v = 0;
    if (EhPrefixoDeNumero(s[0], s[1])) {
        bool hex = s[1] == 'x';
        for (s += 2; hex ? EhDigitoHex(*s) : (*s == '0' || *s == '1'); s++) {
            uint64_t d = EhDigito(*s) ? (uint64_t)(*s - '0') : (uint64_t)((*s | 0x20) - 'a' + 10);
            if (v >> (hex ? 60 : 63) != 0) return false;
            v = (v << (hex ? 4 : 1)) | d;
        }
        *valor = v;
        return true;
    }
    for (; EhDigito(*s); s++) {
        uint64_t d = (uint64_t)(*s - '0');
        if (v > (UINT64_MAX - d) / 10u) return false;
        v = v * 10u + d;
    }
    *valor = v;
    return true;
}

/* Tamanho do simbolo em s (0 = nao e simbolo valido); tipo recebe o token.
 * Passa de 30 linhas e de 5 casos de proposito: e uma tabela caractere -> token, sem aninhamento nem regra de negocio. */
static inline __attribute__((always_inline)) uint32_t CasarSimbolo(const char *s, TokenTipo *tipo)
{
    char a = s[0], b = s[1];
    switch (a) {
    case '(': *tipo = TK_LPAREN; return 1;
    case ')': *tipo = TK_RPAREN; return 1;
    case '{': *tipo = TK_LBRACE; return 1;
    case '}': *tipo = TK_RBRACE; return 1;
    case ',': *tipo = TK_COMMA; return 1;
    case ':': *tipo = TK_COLON; return 1;
    case '*': *tipo = TK_STAR; return 1;
    case '/': *tipo = TK_SLASH; return 1;
    case '%': *tipo = TK_PERCENT; return 1;
    case '^': *tipo = TK_CARET; return 1;
    case '&': *tipo = TK_AMP; return 1;
    case '=': if (b == '=') { *tipo = TK_EQ; return 2; } *tipo = TK_ASSIGN; return 1;
    case '!': if (b == '=') { *tipo = TK_NE; return 2; } *tipo = TK_BANG; return 1;
    case '|': *tipo = TK_PIPE; return 1;
    case '~': *tipo = TK_TIL; return 1;
    case '#': *tipo = TK_HASH; return 1;
    case '?': *tipo = TK_INTERROGA; return 1;
    case '[': *tipo = TK_LCOLCHETE; return 1;
    case ']': *tipo = TK_RCOLCHETE; return 1;
    case '<':
        if (b == '<') { *tipo = TK_SHL; return 2; }
        if (b == '=') { *tipo = TK_LE; return 2; }
        *tipo = TK_LT; return 1;
    case '>':
        if (b == '>') { *tipo = TK_SHR; return 2; }
        if (b == '=') { *tipo = TK_GE; return 2; }
        *tipo = TK_GT; return 1;
    case '+': if (b == '=') { *tipo = TK_PLUSEQ; return 2; } *tipo = TK_PLUS; return 1;
    case '-':
        if (b == '=') { *tipo = TK_MINUSEQ; return 2; }
        if (b == '>') { *tipo = TK_ARROW; return 2; }
        *tipo = TK_MINUS; return 1;
    case '.':
        if (b == '.' && s[2] == '<') { *tipo = TK_DOTDOTLT; return 3; }
        if (b == '.' && s[2] == '=') { *tipo = TK_DOTDOTEQ; return 3; }
        if (b == '.') { *tipo = TK_DOTDOT; return 2; }
        *tipo = TK_DOT; return 1;
    default: return 0;
    }
}

/* "..." numa linha so; \x pula o caractere seguinte (o escape fica como escrito). Sem fechar = E0001 na aspa de abertura. */
static inline __attribute__((always_inline)) bool LerTexto(Lexer *lx, uint32_t *i)
{
    const char *s = lx->fonte->texto;
    uint32_t n = (uint32_t)lx->fonte->tam, j = *i + 1;
    while (j < n && s[j] != '"' && s[j] != '\n') j += (s[j] == '\\' && s[j + 1] != '\n') ? 2 : 1;
    if (j >= n || s[j] != '"') return DiagRelatar(lx->diag, E0001, *i, "texto sem fechar na linha: falta a aspa de fechamento");
    int32_t sym = Internar(lx->nomes, s + *i + 1, j - *i - 1);
    EmpurrarToken(lx, TK_TEXTO, *i, j + 1 - *i, sym);
    *i = j + 1;
    return true;
}

static inline __attribute__((always_inline)) bool LerNome(Lexer *lx, uint32_t *i)
{
    const char *s = lx->fonte->texto;
    uint32_t j = *i + 1;
    while (EhParteDeNome(s[j])) j++;
    /* 'f' colado na aspa abre f"...": testado depois do nome (aspa logo apos nome quase nunca ocorre: ramo previsivel). */
    if (s[j] == '"' && j == *i + 1 && s[*i] == 'f') return LexerLerFTexto(lx, i);
    int32_t sym = Internar(lx->nomes, s + *i, j - *i);
    EmpurrarToken(lx, sym < KW_QTD ? (TokenTipo)(KW_CONST + sym) : TK_IDENT, *i, j - *i, sym);
    *i = j;
    return true;
}

/* IA1-L1: a linha que tem o offset i comeca com "for (" (o for de C, Java e JS: o ';' do cabecalho e o primeiro a aparecer). */
static bool LinhaComecaComForC(const char *s, uint32_t i)
{
    uint32_t k = i;

    while (k > 0 && s[k - 1] != '\n') k--;
    while (s[k] == ' ' || s[k] == '\t') k++;
    if (strncmp(s + k, "for", 3) != 0) return false;
    k += 3;
    while (s[k] == ' ' || s[k] == '\t') k++;
    return s[k] == '(';
}

/* IA1-b04: ';' e idioma de C/Go/JS (a Forja separa comandos por linha). Sobrando no fim da linha (so espaco ou comentario depois), tirar o ';' e a unica
 * correcao e nao muda o sentido (certa); no meio da linha falta decidir onde quebrar (so dica, sem edicao). */
static bool PontoEVirgulaInvalido(Lexer *lx, uint32_t i)
{
    const char *s = lx->fonte->texto;
    uint32_t k = i + 1;

    while (s[k] == ' ' || s[k] == '\t' || s[k] == '\r') k++;
    bool fim_da_linha = s[k] == '\n' || s[k] == '\0' || (s[k] == '/' && s[k + 1] == '/');
    if (LinhaComecaComForC(s, i)) {
        IdiomaRelatar(lx->diag, IDI_FOR_ESTILO_C, i, "for (;;)");
        DiagSugerir(&lx->diag->sug, DIAG_SO_DICA, "troque o cabecalho pelo for da Forja: for i in 0..<n {");
        return false;
    }
    IdiomaRelatar(lx->diag, IDI_PONTO_E_VIRGULA, i, ";");
    if (fim_da_linha) {
        DiagSugerir(&lx->diag->sug, DIAG_CERTA, "remova o ';' do fim da linha");
        DiagEditar(&lx->diag->sug, i, i + 1, "");
    } else {
        DiagSugerir(&lx->diag->sug, DIAG_SO_DICA, "um comando por linha: troque o ';' por uma quebra de linha");
    }
    return false;
}

static bool CaractereInvalido(Lexer *lx, uint32_t i)
{
    unsigned char ch = (unsigned char)lx->fonte->texto[i];
    if (ch == ';') return PontoEVirgulaInvalido(lx, i);
    if (ch >= 0x21 && ch < 0x7F) return DiagRelatar(lx->diag, E0001, i, "caractere invalido '%c'", ch);
    if (ch >= 0x80)
        return DiagRelatar(lx->diag, E0001, i,
                           "caractere fora do ASCII (byte 0x%02X): nome, tipo e comando so usam a-z, A-Z, 0-9 e _ (sem acento); "
                           "acento so vale dentro de texto entre aspas", ch);
    return DiagRelatar(lx->diag, E0001, i, "caractere invalido (byte 0x%02X)", ch);
}

/* Trata o token que comeca em texto[*i]; avanca *i. */
static inline __attribute__((always_inline)) bool LerUm(Lexer *lx, uint32_t *i)
{
    const char *s = lx->fonte->texto;
    TokenTipo tipo;
    if (EhInicioDeNome(s[*i])) return LerNome(lx, i);
    if (s[*i] == '"') return LerTexto(lx, i);
    if (EhDigito(s[*i])) {
        if (EhPrefixoDeNumero(s[*i], s[*i + 1])) return LerNumeroPrefixado(lx, i);
        bool real;
        uint32_t fim = FimDoNumero(s, *i, &real);
        EmpurrarToken(lx, real ? TK_REAL : TK_INT, *i, fim - *i, -1);
        *i = fim;
        return true;
    }
    uint32_t n = CasarSimbolo(s + *i, &tipo);
    if (n == 0) return CaractereInvalido(lx, *i);
    EmpurrarToken(lx, tipo, *i, n, -1);
    *i += n;
    return true;
}

/* Um token dentro de {...} de f"...": o chamador (lexer_ftexto.c) ja barrou aspas e f"...", entao nao recursa. */
bool LexerLerTokenSimples(Lexer *lx, uint32_t *i)
{
    return LerUm(lx, i);
}

/* IA1-b15: o token da posicao *i falhou (E0001 pendente): com lista guarda o erro e pula o resto da linha (os tokens de uma linha com erro nao servem).
 * false = sem lista ou lista cheia (o erro sai como sempre). */
__attribute__((cold, noinline)) static bool RecuperarDaLinha(Lexer *lx, uint32_t *i, uint32_t n)
{
    const char *s = lx->fonte->texto;
    if (lx->erros == NULL || !DiagListaGuardar(lx->erros, lx->diag, 0)) return false;
    while (*i < n && s[*i] != '\n') (*i)++;
    return true;
}

/* IA1-L1: barra-asterisco (comentario de bloco de C, Java e JS; '*' nunca e prefixo na Forja, entao ela nao tem outro sentido). A sugestao e certa quando o comentario abre e fecha
 * na mesma linha e nada vem depois do fecho: a abertura vira '//' e o fecho sai. Em varias linhas so a dica (cada linha precisa do seu '//'). */
static bool BlocoDeUmaLinha(const char *s, uint32_t ini, uint32_t fecho, bool fecha)
{
    uint32_t k = fecho + 2;

    if (!fecha || memchr(s + ini, '\n', fecho - ini) != NULL) return false;
    while (s[k] == ' ' || s[k] == '\t' || s[k] == '\r') k++;
    return s[k] == '\n' || s[k] == '\0';
}

static void SugerirComentarioDeLinha(Lexer *lx, uint32_t ini, uint32_t fecho, bool fecha)
{
    const char *s = lx->fonte->texto;

    if (!BlocoDeUmaLinha(s, ini, fecho, fecha)) {
        DiagSugerir(&lx->diag->sug, DIAG_SO_DICA, "escreva // no comeco de cada linha do comentario e tire o fecho do bloco");
        return;
    }
    uint32_t inicio_do_fecho = fecho;
    while (inicio_do_fecho > ini + 2 && (s[inicio_do_fecho - 1] == ' ' || s[inicio_do_fecho - 1] == '\t')) inicio_do_fecho--;
    DiagSugerir(&lx->diag->sug, DIAG_CERTA, "troque o comentario de bloco pelo comentario de linha //");
    DiagEditar(&lx->diag->sug, ini, ini + 2, "//");
    DiagEditar(&lx->diag->sug, inicio_do_fecho, fecho + 2, "");
}

static bool ComentarioDeBlocoAlheio(Lexer *lx, uint32_t *i, uint32_t n)
{
    const char *s = lx->fonte->texto;
    uint32_t fecho = *i + 2;

    while (fecho < n && !(s[fecho] == '*' && s[fecho + 1] == '/')) fecho++;
    IdiomaRelatar(lx->diag, IDI_COMENTARIO_DE_BLOCO, *i, "/*");
    SugerirComentarioDeLinha(lx, *i, fecho, fecho < n);
    if (lx->erros == NULL || !DiagListaGuardar(lx->erros, lx->diag, 0)) return false;
    *i = fecho < n ? fecho + 2 : n; /* pula o bloco inteiro: as linhas de dentro nao sao codigo */
    return true;
}

/* '//' (pula ate o fim da linha) ou barra-asterisco (E0110) em s[*i]; false = o lexer para (erro pendente sem lista, ou lista cheia). */
static bool SaltarComentario(Lexer *lx, uint32_t *i, uint32_t n)
{
    const char *s = lx->fonte->texto;

    uint32_t inicio = *i;

    if (s[*i + 1] == '*') return ComentarioDeBlocoAlheio(lx, i, n);
    while (*i < n && s[*i] != '\n') (*i)++;
    if (lx->comentarios != NULL) *(LexerComentario *)VecEmpurrar(lx->comentarios) = (LexerComentario){inicio, *i};
    return true;
}

/* aligned(64): o laco do lexer rende ~0,35 ms mais com a funcao no inicio de um bloco de 64 bytes (medido, v1.8 P9).
 * LIMITE: cognitiva 20 (laco de despacho por caractere: quebra, espaco, comentario e token na mesma cadeia; cada ramo ja delega a uma funcao). */
__attribute__((aligned(64))) bool LexerAnalisar(Lexer *lx)
{
    const char *s = lx->fonte->texto;
    /* Contrato da Fonte: posicoes de 32 bits e NUL em texto[tam]; LerNome, FimDoNumero e LerTexto leem 1 caractere alem do fim. */
    if (lx->fonte->tam > FONTE_TAM_MAX)
        FalhaInterna("lexer: fonte de %zu bytes passa do limite de %u (posicoes de 32 bits)", lx->fonte->tam, (unsigned)FONTE_TAM_MAX);
    if (s[lx->fonte->tam] != '\0')
        FalhaInterna("lexer: a fonte nao termina em NUL (contrato de FonteLer e FonteDeTexto)");
    uint32_t i = lx->inicio, n = (uint32_t)lx->fonte->tam;
    if (lx->tokens.tam != 0) lx->tokens.n = 0; /* lexer reaproveitado (modulos): mesmo vetor, tokens do trecho novo */
    else VecIniciar(&lx->tokens, sizeof(Token));
    VecReservar(&lx->tokens, (n - i) / 2 + 64); /* fonte real tem ~0,35 token por byte; acima disso o vetor dobra normalmente */
    while (i < n) {
        char ch = s[i];
        if (ch == '\n') {
            EmpurrarQuebra(lx, i);
            i++;
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            i++;
        } else if (ch == '/' && (s[i + 1] == '/' || s[i + 1] == '*')) {
            if (!SaltarComentario(lx, &i, n)) return false;
        } else if (!LerUm(lx, &i)) {
            if (!RecuperarDaLinha(lx, &i, n)) return false;
        }
    }
    EmpurrarToken(lx, TK_EOF, n, 0, -1);
    return lx->erros == NULL || lx->erros->n == 0;
}

void LexerLiberar(Lexer *lx)
{
    VecLiberar(&lx->tokens);
}
