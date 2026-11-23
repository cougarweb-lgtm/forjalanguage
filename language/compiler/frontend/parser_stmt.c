/* parser_stmt.c - parser dos comandos (let, var, atribuicao, if, while, for, switch, return, break, continue, defer,
 * expect, chamada) e dos blocos.
 * Versao: 0.15.0
 * Changelog: 0.15.0 - Linguagem v1.13 L1a: o apelido de import e consultado pelo trecho (arquivo_atual).
 *            0.14.0 - IA1-L3c2: AnalisarAlvoDoCaso guarda a posicao do membro em Caso.pos_membro.
 *            0.13.0 - IA1-L1: 'try {' vira E0110 (AnalisarTryComando).
 *            0.12.0 - IA1-b15: AnalisarBloco re-sincroniza no fim da linha/bloco do comando com erro (ParserRecuperarComando) e segue.
 *            0.11.0 - Linguagem v1.12 M3 P21v-a: o tipo proc(...) guarda os nomes dos parametros (alvo[nome + k]); chamada indireta (EX_CHAMADA_IND) vale como comando (AnalisarChamada e AnalisarAtribuicao) e apos defer.
 *            0.10.0 -Linguagem v1.11 L15a: '[dynamic, N]T' (TR_LISTA_FIXA, AnalisarListaFixa); LerTamanhoDoColchete e o N compartilhado com '[N]T'.
 *            0.9.0 - Linguagem v1.9 P2: map[K]V, bit_set[E], soa[N]S / soa[dynamic]S e matrix[R, C]T (AnalisarTipoV19, despacho
 *                    por sym atras de __builtin_expect; E0321 de dimensao no parser).
 *            0.8.0 - Linguagem v1.8b P2: tipo 'proc(nome: T, ...) -> R' (TR_PROC, AnalisarTipoProc) em AnalisarTipoRefNivel.
 *            0.7.1 - Linguagem v1.8 P3: 'apelido.proc(x)' solto (e apos defer) e chamada qualificada (ComecaChamadaQualificada; so com napelidos != 0).
 *            0.7.0 - Linguagem v1.8 P2a: tipo e alvo de case qualificados por 'apelido.' (TipoRef.modulo, Caso.modulo); E0101 em '[apelido.N]T'.
 *            0.6.0 - Linguagem v1.7 P1: '?T' (TR_OPCIONAL), 'let a, b = f()' (ST_LET_MULTIPLO), 'return a, b' (EX_TUPLA), 'case none:' e 'case ^T/[N]T/?T:' (Caso.tipo_escrito).
 *            0.5.0 - Linguagem v1.6 P1a: tipo '[dynamic]T' (TR_DINAMICO) em AnalisarTipoColchete.
 *            0.4.2 - Linguagem v1.5 P1c: AnalisarFor com 'for item', 'for &item', 'for i, item' e 'for i, &item' (colecao: expr2 NULL).
 *            0.4.1 - Linguagem v1.5 P1b: CadeiaDeLvalor anda tambem por EX_INDICE e EX_FATIA.
 *            0.4.0 - Linguagem v1.5 P1a: tipos '[N]T' e '[]T' (AnalisarTipoColchete).
 *            0.3.1 - Linguagem v1.3: CadeiaDeLvalor (lado esquerdo com '.campo' e '^' encadeados).
 *            0.3.0 -Linguagem v1.3: AnalisarTipoRef recursivo ('^T', '?^T', ate 8 niveis; sem o bool permite_ponteiro); COMANDOS[TK_QTD].
 *            0.2.1 - Fase 5d: AnalisarExpect grava Stmt.pos_fim = posicao do ')' final (texto da condicao do IR_EXPECT).
 *            0.2.0 - Fase 5c: 'try <chamada>' vale como comando solto (ST_CHAMADA cuja expressao e EX_TRY).
 *            0.1.0 - Fase 5b: comandos do prototipo + break/continue, defer de 1 comando simples, switch (com ligacao
 *                    'switch nome in expr'), expect(cond); despacho por tabela token -> funcao; 'for' aceita '..<' e '..='.
 * Verificacao: construir.bat && forja.exe ast <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/parser_interno.h"

typedef Stmt *(*AnalisaComando)(Parser *p);

static Stmt *AnalisarComando(Parser *p);

static Stmt *NovoStmt(Parser *p, StmtTipo tipo, uint32_t pos)
{
    Stmt *s = ArenaAlocar(p->arena, sizeof(Stmt));
    s->tipo_no = (uint8_t)tipo;
    s->pos = pos;
    s->nome = -1;
    return s;
}

/* v1.8: o IDENT atual abre 'apelido.nome' de um import deste arquivo (so com napelidos != 0): o comando e uma chamada qualificada. */
static bool ComecaChamadaQualificada(const Parser *p)
{
    return p->napelidos != 0 && TipoSeguinte(p) == TK_DOT &&
           ModulosApelido(p->modulos, p->arquivo_atual, TokenAtual(p)->sym) != 0;
}

enum { PROFUNDIDADE_MAX_PONTEIRO = 8 };

static bool AnalisarTipoRefNivel(Parser *p, TipoRef *t, uint32_t nivel);

/* Depois de '^' (ou '?^'): o tipo apontado vira um TipoRef novo da arena. */
static bool AnalisarAlvoDoPonteiro(Parser *p, TipoRef *t, uint32_t nivel)
{
    if (nivel >= PROFUNDIDADE_MAX_PONTEIRO) return ErroSintaxe(p, "ponteiro aninhado demais (maximo 8 niveis de '^')");
    t->alvo = ArenaAlocar(p->arena, sizeof(TipoRef));
    return AnalisarTipoRefNivel(p, t->alvo, nivel + 1);
}

/* O N de '[N]T' e de '[dynamic, N]T' (literal inteiro ou nome de const): grava pos_tamanho e nome e avanca o token. 'esperado' e o texto do erro de sintaxe. */
static bool LerTamanhoDoColchete(Parser *p, TipoRef *t, const char *esperado)
{
    TokenTipo k = TipoAtual(p);
    if (k != TK_INT && k != TK_IDENT) return ErroSintaxe(p, esperado);
    if (k == TK_IDENT && p->napelidos != 0 && TipoSeguinte(p) == TK_DOT && ModulosApelido(p->modulos, p->arquivo_atual, TokenAtual(p)->sym) != 0) {
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off,
                           "tamanho de array com nome de outro modulo chega depois: use literal ou const deste arquivo");
    }
    t->pos_tamanho = TokenAtual(p)->off;
    if (k == TK_IDENT) t->nome = TokenAtual(p)->sym;
    p->cur++;
    return true;
}

/* v1.11 L15a: '[dynamic, N]T' (o token atual e a ','); mesmo N do array, forma propria (TR_LISTA_FIXA). */
static bool AnalisarListaFixa(Parser *p, TipoRef *t, uint32_t nivel)
{
    p->cur++;
    t->forma = TR_LISTA_FIXA;
    if (!LerTamanhoDoColchete(p, t, "capacidade da lista fixa: literal inteiro ou nome de const ('[dynamic, 64]T', '[dynamic, N]T')")) return false;
    if (!Esperar(p, TK_RCOLCHETE, "']'")) return false;
    return AnalisarAlvoDoPonteiro(p, t, nivel);
}

/* '[N]T' (N literal inteiro ou nome de const), '[]T', '[dynamic]T' e '[dynamic, N]T': o elemento vira o alvo, com o mesmo limite de aninhamento do '^'. */
static bool AnalisarTipoColchete(Parser *p, TipoRef *t, uint32_t nivel)
{
    p->cur++;
    t->nome = -1;
    if (TipoAtual(p) == TK_IDENT && TokenAtual(p)->sym == SYM_DYNAMIC) {
        t->forma = TR_DINAMICO;
        t->pos_tamanho = TokenAtual(p)->off;
        p->cur++;
        if (TipoAtual(p) == TK_COMMA) return AnalisarListaFixa(p, t, nivel);
        if (!Esperar(p, TK_RCOLCHETE, "']'")) return false;
        return AnalisarAlvoDoPonteiro(p, t, nivel);
    }
    if (TipoAtual(p) == TK_RCOLCHETE) {
        p->cur++;
        t->forma = TR_FATIA;
        return AnalisarAlvoDoPonteiro(p, t, nivel);
    }
    t->forma = TR_ARRAY;
    if (!LerTamanhoDoColchete(p, t, "tamanho do array: literal inteiro ou nome de const ('[4]T', '[N]T') ou ']' para fatia")) return false;
    if (!Esperar(p, TK_RCOLCHETE, "']'")) return false;
    return AnalisarAlvoDoPonteiro(p, t, nivel);
}

enum { PARAMS_MAX_CALLBACK = 16 };

/* 'nome: T' de um parametro do tipo proc(...): o tipo vai para 't' e o nome (v1.12: vale nos argumentos nomeados da chamada indireta) para 'nome'. */
static bool AnalisarParamDoTipoProc(Parser *p, TipoRef *t, TipoRef *nome, uint32_t nivel)
{
    if (!LerNome(p, &nome->nome, &nome->pos, "nome do parametro do callback (nome: T)")) return false;
    if (!Esperar(p, TK_COLON, "':'")) return false;
    return AnalisarTipoRefNivel(p, t, nivel + 1);
}

/* v1.8b: 'proc(nome: T, ...) -> R' (callback de foreign). alvo[0] = retorno, alvo[1..nome] = parametros, alvo[nome + k] = nome do parametro k. */
static bool AnalisarTipoProc(Parser *p, TipoRef *t, uint32_t nivel)
{
    TipoRef tmp[PARAMS_MAX_CALLBACK + 1] = {0}, nomes[PARAMS_MAX_CALLBACK + 1] = {0};
    uint32_t n = 0;
    t->pos = TokenAtual(p)->off;
    t->forma = TR_PROC;
    t->erro = -1;
    t->pos_tamanho = 0;
    p->cur++;
    if (!Esperar(p, TK_LPAREN, "'(' depois de proc no tipo")) return false;
    while (TipoAtual(p) != TK_RPAREN) {
        if (n == PARAMS_MAX_CALLBACK) {
            return DiagRelatar(p->diag, E0101, TokenAtual(p)->off, "callback com mais de 16 parametros");
        }
        if (!AnalisarParamDoTipoProc(p, &tmp[n + 1], &nomes[n + 1], nivel)) return false;
        n++;
        if (TipoAtual(p) != TK_COMMA) break;
        p->cur++;
    }
    if (!Esperar(p, TK_RPAREN, "')'")) return false;
    if (TipoAtual(p) == TK_ARROW) {
        p->cur++;
        t->pos_tamanho = 1;
        if (!AnalisarTipoRefNivel(p, &tmp[0], nivel + 1)) return false;
    }
    t->nome = (int32_t)n;
    t->alvo = ArenaAlocar(p->arena, (2 * n + 1) * sizeof(TipoRef));
    memcpy(t->alvo, tmp, (n + 1) * sizeof(TipoRef));
    memcpy(t->alvo + n + 1, nomes + 1, n * sizeof(TipoRef));
    return true;
}

/* v1.9: map[K]V (alvo[0] = K, alvo[1] = V). O token atual e o 'map'. */
static bool AnalisarTipoMapa(Parser *p, TipoRef *t, uint32_t nivel)
{
    p->cur++;
    if (!Esperar(p, TK_LCOLCHETE, "'['")) return false;
    t->forma = TR_MAPA;
    t->alvo = ArenaAlocar(p->arena, 2 * sizeof(TipoRef));
    if (!AnalisarTipoRefNivel(p, &t->alvo[0], nivel + 1)) return false;
    if (!Esperar(p, TK_RCOLCHETE, "']'")) return false;
    return AnalisarTipoRefNivel(p, &t->alvo[1], nivel + 1);
}

/* v1.9: bit_set[E]. O token atual e o 'bit_set'. */
static bool AnalisarTipoBitSet(Parser *p, TipoRef *t, uint32_t nivel)
{
    p->cur++;
    if (!Esperar(p, TK_LCOLCHETE, "'['")) return false;
    t->forma = TR_BIT_SET;
    t->alvo = ArenaAlocar(p->arena, sizeof(TipoRef));
    if (!AnalisarTipoRefNivel(p, t->alvo, nivel + 1)) return false;
    return Esperar(p, TK_RCOLCHETE, "']'");
}

/* v1.9: soa[N]S e soa[dynamic]S. O token atual e o 'soa'; o '[' vem garantido pelo despacho. */
static bool AnalisarTipoSoa(Parser *p, TipoRef *t, uint32_t nivel)
{
    p->cur++;
    t->forma = TR_SOA;
    t->alvo = ArenaAlocar(p->arena, sizeof(TipoRef));
    t->alvo->erro = -1;
    t->alvo->pos = TokenAtual(p)->off;
    return AnalisarTipoColchete(p, t->alvo, nivel + 1);
}

/* v1.9: dimensao de matrix[R, C]T: literal de um digito, de 2 a 4. */
static bool LerDimensaoDaMatriz(Parser *p, uint32_t *v)
{
    const Token *tk = TokenAtual(p);
    if (tk->tipo != TK_INT) return DiagRelatar(p->diag, E0101, tk->off, "matrix[R, C]T: R e C sao literais inteiros de 2 a 4");
    char digito = p->fonte->texto[tk->off];
    char depois = p->fonte->texto[tk->off + 1];
    bool continua = (depois >= '0' && depois <= '9') || (depois >= 'a' && depois <= 'z') || (depois >= 'A' && depois <= 'Z') || depois == '_';
    if (digito < '2' || digito > '4' || continua) return DiagRelatar(p->diag, E0321, tk->off, "matrix[R, C]T: R e C vao de 2 a 4");
    *v = (uint32_t)(digito - '0');
    p->cur++;
    return true;
}

/* v1.9: matrix[R, C]T. O token atual e o 'matrix'. */
static bool AnalisarTipoMatriz(Parser *p, TipoRef *t, uint32_t nivel)
{
    uint32_t r = 0, c = 0;
    p->cur++;
    if (!Esperar(p, TK_LCOLCHETE, "'['")) return false;
    t->forma = TR_MATRIZ;
    t->pos_tamanho = TokenAtual(p)->off;
    if (!LerDimensaoDaMatriz(p, &r)) return false;
    if (!Esperar(p, TK_COMMA, "','")) return false;
    if (!LerDimensaoDaMatriz(p, &c)) return false;
    if (!Esperar(p, TK_RCOLCHETE, "']'")) return false;
    t->nome = (int32_t)((r << 4) | c);
    t->alvo = ArenaAlocar(p->arena, sizeof(TipoRef));
    return AnalisarTipoRefNivel(p, t->alvo, nivel + 1);
}

/* v1.9: despacho das 4 palavras contextuais de tipo (so entra com 'map'/'bit_set'/'soa'/'matrix' seguido de '['). */
static bool AnalisarTipoV19(Parser *p, TipoRef *t, uint32_t nivel)
{
    if (nivel >= PROFUNDIDADE_MAX_PONTEIRO) return ErroSintaxe(p, "ponteiro aninhado demais (maximo 8 niveis de '^')");
    int32_t sym = TokenAtual(p)->sym;
    t->pos = TokenAtual(p)->off;
    t->nome = -1;
    if (sym == SYM_MAP) return AnalisarTipoMapa(p, t, nivel);
    if (sym == SYM_BIT_SET) return AnalisarTipoBitSet(p, t, nivel);
    if (sym == SYM_SOA) return AnalisarTipoSoa(p, t, nivel);
    return AnalisarTipoMatriz(p, t, nivel);
}

static bool AnalisarTipoRefNivel(Parser *p, TipoRef *t, uint32_t nivel)
{
    t->erro = -1;
    t->alvo = NULL;
    t->pos = TokenAtual(p)->off;
    if (TipoAtual(p) == TK_LCOLCHETE) return AnalisarTipoColchete(p, t, nivel);
    if (TipoAtual(p) == TK_INTERROGA) {
        p->cur++;
        if (TipoAtual(p) != TK_CARET) {
            t->forma = TR_OPCIONAL;
            return AnalisarAlvoDoPonteiro(p, t, nivel);
        }
        p->cur++;
        t->forma = TR_ANULAVEL;
        return AnalisarAlvoDoPonteiro(p, t, nivel);
    }
    if (TipoAtual(p) == TK_CARET) {
        p->cur++;
        t->forma = TR_PONTEIRO;
        return AnalisarAlvoDoPonteiro(p, t, nivel);
    }
    if (__builtin_expect(TipoAtual(p) == KW_PROC, 0)) return AnalisarTipoProc(p, t, nivel);
    if (__builtin_expect((uint32_t)(TokenAtual(p)->sym - SYM_MAP) <= (uint32_t)(SYM_MATRIX - SYM_MAP), 0) &&
        TipoAtual(p) == TK_IDENT && TipoSeguinte(p) == TK_LCOLCHETE)
        return AnalisarTipoV19(p, t, nivel);
    t->forma = TR_NOME;
    if (p->napelidos == 0) return LerNome(p, &t->nome, &t->pos, "nome de tipo");
    uint32_t pos_nome;
    t->modulo = LerApelido(p, &pos_nome);
    if (!LerNome(p, &t->nome, &pos_nome, "nome de tipo")) return false;
    if (t->modulo != 0) t->pos_tamanho = pos_nome; else t->pos = pos_nome;
    return true;
}

/* "T", "^T", "?^T" (recursivo); erro = -1 porque a arena zera e 0 e um id valido. */
bool AnalisarTipoRef(Parser *p, TipoRef *t)
{
    return AnalisarTipoRefNivel(p, t, 0);
}

bool AnalisarBloco(Parser *p, Stmt **primeiro, bool ate_caso)
{
    Stmt *ultimo = NULL;
    *primeiro = NULL;
    for (;;) {
        TokenTipo t = TipoAtual(p);
        if (t == TK_RBRACE || (ate_caso && (t == KW_CASE || t == KW_DEFAULT))) return true;
        uint32_t ini = p->cur;
        Stmt *s = AnalisarComando(p);
        if (s == NULL) {
            if (!ParserRecuperarComando(p, ini)) return false; /* b15: guarda o erro, pula a linha (e o bloco que ela abre) e segue */
            continue;
        }
        if (ultimo == NULL) *primeiro = s; else ultimo->prox = s;
        ultimo = s;
    }
}

bool AnalisarCorpoComChaves(Parser *p, Stmt **corpo, uint32_t *pos_fecha)
{
    if (!EsperarAbreBloco(p) || !AnalisarBloco(p, corpo, false)) return false;
    *pos_fecha = TokenAtual(p)->off;
    p->cur++;
    return true;
}

/* 'let q, r = f()' (v1.7): o 1o nome ja foi lido e o token atual e a ','; '_' e um nome comum aqui (a semantica o trata). */
static bool LerNomesDoLetMultiplo(Parser *p, Stmt *s, int32_t primeiro, uint32_t pos_primeiro)
{
    size_t base = p->pilha_membros.n; /* o Membro {nome, pos} serve de acumulador; vira NomeLet na arena */
    Membro m = {.nome = primeiro, .pos = pos_primeiro};
    *(Membro *)VecEmpurrar(&p->pilha_membros) = m;
    while (TipoAtual(p) == TK_COMMA) {
        p->cur++;
        if (!LerNome(p, &m.nome, &m.pos, "nome da variavel")) return false;
        *(Membro *)VecEmpurrar(&p->pilha_membros) = m;
    }
    s->nnomes = (uint32_t)(p->pilha_membros.n - base);
    s->nomes = ArenaAlocar(p->arena, s->nnomes * sizeof(NomeLet));
    for (uint32_t i = 0; i < s->nnomes; i++) {
        const Membro *lido = VecEm(&p->pilha_membros, base + i);
        s->nomes[i] = (NomeLet){.nome = lido->nome, .pos = lido->pos, .local_id = -1};
    }
    p->pilha_membros.n = base;
    return true;
}

static Stmt *AnalisarLetMultiplo(Parser *p, Stmt *s, uint32_t pos_primeiro)
{
    s->op = (uint8_t)(s->tipo_no == ST_VAR);
    s->tipo_no = ST_LET_MULTIPLO;
    if (!LerNomesDoLetMultiplo(p, s, s->nome, pos_primeiro) || !Esperar(p, TK_ASSIGN, "'='")) return NULL;
    s->nome = -1;
    s->expr = AnalisarExpr(p);
    return s->expr != NULL && EsperarFimDeLinha(p) ? s : NULL;
}

static Stmt *AnalisarLetVar(Parser *p)
{
    Stmt *s = NovoStmt(p, TipoAtual(p) == KW_LET ? ST_LET : ST_VAR, TokenAtual(p)->off);
    uint32_t pos_nome;
    p->cur++;
    if (!LerNome(p, &s->nome, &pos_nome, "nome da variavel")) return NULL;
    if (TipoAtual(p) == TK_COMMA) return AnalisarLetMultiplo(p, s, pos_nome);
    if (TipoAtual(p) == TK_COLON) {
        p->cur++;
        s->tem_tipo = true;
        if (!AnalisarTipoRef(p, &s->tipo)) return NULL;
    }
    if (!Esperar(p, TK_ASSIGN, "'='")) return NULL;
    s->expr = AnalisarExpr(p);
    return s->expr != NULL && EsperarFimDeLinha(p) ? s : NULL;
}

static Stmt *AnalisarIf(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_IF, TokenAtual(p)->off);
    p->cur++;
    s->expr = AnalisarCabecalho(p);
    if (s->expr == NULL || !AnalisarCorpoComChaves(p, &s->corpo, &s->pos_fim)) return NULL;
    if (TipoAtual(p) != KW_ELSE) return EsperarFimDeLinha(p) ? s : NULL;
    p->cur++;
    if (TipoAtual(p) == KW_IF) {
        s->senao = AnalisarIf(p);
        return s->senao != NULL ? s : NULL;
    }
    uint32_t pos_fecha;
    if (!AnalisarCorpoComChaves(p, &s->senao, &pos_fecha)) return NULL;
    return EsperarFimDeLinha(p) ? s : NULL;
}

static Stmt *AnalisarWhile(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_WHILE, TokenAtual(p)->off);
    p->cur++;
    s->expr = AnalisarCabecalho(p);
    if (s->expr == NULL || !AnalisarCorpoComChaves(p, &s->corpo, &s->pos_fim)) return NULL;
    return EsperarFimDeLinha(p) ? s : NULL;
}

/* ['&'] nome: o item (ou o indice) do for. */
static bool LerNomeDoFor(Parser *p, int32_t *nome, bool *ref)
{
    uint32_t pos_nome;
    *ref = TipoAtual(p) == TK_AMP;
    if (*ref) p->cur++;
    return LerNome(p, nome, &pos_nome, "nome da variavel do laco");
}

/* 'for [&]item' ou 'for i, [&]item' (indice primeiro; o '&' vai so no item). */
static bool AnalisarNomesDoFor(Parser *p, Stmt *s)
{
    uint32_t inicio = p->cur;
    int32_t primeiro;
    bool ref_primeiro;
    s->nome_indice = -1;
    s->local_indice = -1;
    if (!LerNomeDoFor(p, &primeiro, &ref_primeiro)) return false;
    if (TipoAtual(p) != TK_COMMA) {
        s->nome = primeiro;
        s->item_ref = ref_primeiro;
        return true;
    }
    if (ref_primeiro) {
        p->cur = inicio;
        return ErroSintaxe(p, "o indice do for e um inteiro; '&' vai no item");
    }
    p->cur++;
    s->nome_indice = primeiro;
    return LerNomeDoFor(p, &s->nome, &s->item_ref);
}

/* Depois do cabecalho: '..<' ou '..=' so vale no for de faixa (1 nome, sem '&'); senao o cabecalho e a colecao. */
static bool AnalisarFimDoFor(Parser *p, Stmt *s)
{
    TokenTipo t = TipoAtual(p);
    if (t != TK_DOTDOTLT && t != TK_DOTDOTEQ) return true;
    if (s->nome_indice >= 0 || s->item_ref) {
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off,
                           "for com indice ou '&' percorre array ou fatia, nao faixa: for i in a..<b");
    }
    s->faixa_excl = t == TK_DOTDOTLT;
    p->cur++;
    s->expr2 = AnalisarCabecalho(p);
    return s->expr2 != NULL;
}

/* for [&]nome [',' [&]nome] in cabecalho [('..<'|'..=') cabecalho] bloco; sem a faixa, expr2 == NULL (colecao). */
static Stmt *AnalisarFor(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_FOR, TokenAtual(p)->off);
    p->cur++;
    if (!AnalisarNomesDoFor(p, s) || !Esperar(p, KW_IN, "'in'")) return NULL;
    s->expr = AnalisarCabecalho(p);
    if (s->expr == NULL || !AnalisarFimDoFor(p, s)) return NULL;
    if (!AnalisarCorpoComChaves(p, &s->corpo, &s->pos_fim)) return NULL;
    return EsperarFimDeLinha(p) ? s : NULL;
}

/* 'return a, b' (v1.7): a 1a expressao ja foi lida e o token atual e a ','; vira EX_TUPLA com os valores na ordem. */
static Expr *AnalisarTuplaDoReturn(Parser *p, Expr *primeiro)
{
    size_t base = p->pilha_args.n;
    Expr *t = NovaExpr(p, EX_TUPLA, primeiro->pos);
    Arg a = {.nome = -1, .indice = -1, .valor = primeiro};
    *(Arg *)VecEmpurrar(&p->pilha_args) = a;
    while (TipoAtual(p) == TK_COMMA) {
        p->cur++;
        a.valor = AnalisarExpr(p);
        if (a.valor == NULL) return NULL;
        *(Arg *)VecEmpurrar(&p->pilha_args) = a;
    }
    t->nargs = (uint32_t)(p->pilha_args.n - base);
    t->args = CopiarLista(p, &p->pilha_args, base);
    return t;
}

static Stmt *AnalisarReturn(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_RETURN, TokenAtual(p)->off);
    p->cur++;
    if (TipoAtual(p) != TK_NL && TipoAtual(p) != TK_EOF) {
        s->expr = AnalisarExpr(p);
        if (s->expr != NULL && TipoAtual(p) == TK_COMMA) s->expr = AnalisarTuplaDoReturn(p, s->expr);
        if (s->expr == NULL) return NULL;
    }
    return EsperarFimDeLinha(p) ? s : NULL;
}

static Stmt *AnalisarSemArgumentos(Parser *p)
{
    Stmt *s = NovoStmt(p, TipoAtual(p) == KW_BREAK ? ST_BREAK : ST_CONTINUE, TokenAtual(p)->off);
    p->cur++;
    return EsperarFimDeLinha(p) ? s : NULL;
}

static Stmt *AnalisarChamada(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_CHAMADA, TokenAtual(p)->off);
    s->expr = AnalisarPosfixo(p);
    if (s->expr == NULL) return NULL;
    if (s->expr->tipo_no != EX_CHAMADA && s->expr->tipo_no != EX_CHAMADA_IND) { ErroSintaxe(p, "chamada de proc"); return NULL; }
    return EsperarFimDeLinha(p) ? s : NULL;
}

/* try <chamada> como comando: repassa o erro e descarta o valor (EX_TRY sobre a chamada, sem mexer na precedencia dos unarios). */
static Stmt *AnalisarTryComando(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_CHAMADA, TokenAtual(p)->off);
    Expr *t = NovaExpr(p, EX_TRY, TokenAtual(p)->off);
    p->cur++;
    if (TipoAtual(p) == TK_LBRACE) { /* IA1-L1: 'try { } catch' e excecao */
        ParserIdiomaTryBloco(p);
        return NULL;
    }
    t->a = AnalisarPosfixo(p);
    if (t->a == NULL) return NULL;
    if (t->a->tipo_no != EX_CHAMADA) { ErroSintaxe(p, "chamada de proc depois de try"); return NULL; }
    s->expr = t;
    return EsperarFimDeLinha(p) ? s : NULL;
}

/* A cadeia ate a raiz so tem '.campo', '^', '[i]' e '[lo..<hi]', e a raiz e um nome (sem chamada no meio). */
static bool CadeiaDeLvalor(const Expr *e)
{
    while (e->tipo_no == EX_CAMPO || e->tipo_no == EX_INDICE || e->tipo_no == EX_FATIA ||
           (e->tipo_no == EX_UNARIO && e->op == OP_DEREF)) {
        e = e->a;
    }
    return e->tipo_no == EX_NOME;
}

/* lvalor := nome ("." nome | "^" | "[" ... "]")* seguido de '=', '+=' ou '-=' */
static Stmt *AnalisarAtribuicao(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_ATRIBUICAO, TokenAtual(p)->off);
    s->alvo = AnalisarPosfixo(p);
    if (s->alvo == NULL) return NULL;
    if (s->alvo->tipo_no == EX_CHAMADA_IND) { /* v1.12: 'obj.campo(a)' solto (e apos defer) e a chamada indireta como comando */
        s->tipo_no = ST_CHAMADA;
        s->expr = s->alvo;
        s->alvo = NULL;
        return EsperarFimDeLinha(p) ? s : NULL;
    }
    bool lvalor = CadeiaDeLvalor(s->alvo);
    TokenTipo t = TipoAtual(p);
    if (!lvalor || (t != TK_ASSIGN && t != TK_PLUSEQ && t != TK_MINUSEQ)) {
        ErroSintaxe(p, "'=', '+=' ou '-='");
        return NULL;
    }
    s->op = (uint8_t)(t == TK_ASSIGN ? 0 : (t == TK_PLUSEQ ? 1 : 2));
    p->cur++;
    s->expr = AnalisarExpr(p);
    return s->expr != NULL && EsperarFimDeLinha(p) ? s : NULL;
}

/* expect(cond): comando proprio, a condicao e a unica expressao. */
static Stmt *AnalisarExpect(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_EXPECT, TokenAtual(p)->off);
    p->cur += 2;
    s->expr = AnalisarExpr(p);
    if (s->expr == NULL) return NULL;
    s->pos_fim = TokenAtual(p)->off; /* posicao do ')' final: o lowering recorta o texto da condicao ate aqui */
    if (!Esperar(p, TK_RPAREN, "')' (expect recebe so a condicao)")) return NULL;
    return EsperarFimDeLinha(p) ? s : NULL;
}

/* Comando que comeca em nome: expect(...), chamada ou atribuicao. */
static Stmt *AnalisarPorNome(Parser *p)
{
    if (TipoSeguinte(p) != TK_LPAREN) return ComecaChamadaQualificada(p) ? AnalisarChamada(p) : AnalisarAtribuicao(p);
    if (TokenAtual(p)->sym == SYM_EXPECT) return AnalisarExpect(p);
    return AnalisarChamada(p);
}

/* defer <chamada ou atribuicao>: um comando simples na mesma linha, nunca bloco. */
static Stmt *AnalisarDefer(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_DEFER, TokenAtual(p)->off);
    p->cur++;
    if (TipoAtual(p) != TK_IDENT || TokenAtual(p)->sym == SYM_EXPECT) {
        ErroSintaxe(p, "chamada ou atribuicao na mesma linha (defer nao aceita bloco)");
        return NULL;
    }
    s->corpo = TipoSeguinte(p) == TK_LPAREN || ComecaChamadaQualificada(p) ? AnalisarChamada(p) : AnalisarAtribuicao(p);
    return s->corpo != NULL ? s : NULL;
}

/* Alvo do case: Tipo ou Enum.membro. */
static bool AnalisarAlvoDoCaso(Parser *p, Caso *c)
{
    uint32_t pos_nome;
    TokenTipo k = TipoAtual(p);
    if (k == KW_NONE) { /* 'case none:' (v1.7) */
        c->tipo_nome = KW_NONE - KW_CONST;
        p->cur++;
        return true;
    }
    if (k == TK_CARET || k == TK_LCOLCHETE || k == TK_INTERROGA) { /* tipo escrito (v1.7): ^T, [N]T, ?T */
        c->tipo_escrito = ArenaAlocar(p->arena, sizeof(TipoRef));
        return AnalisarTipoRef(p, c->tipo_escrito);
    }
    if (p->napelidos != 0) c->modulo = LerApelido(p, &pos_nome);
    if (!LerNome(p, &c->tipo_nome, &pos_nome, "tipo ou enum do case")) return false;
    if (TipoAtual(p) != TK_DOT) return true;
    p->cur++;
    return LerNome(p, &c->membro, &c->pos_membro, "membro do enum");
}

/* case Alvo: | default: ; o corpo vai ate o proximo case/default ou '}'. */
static bool AnalisarCaso(Parser *p, Caso *c)
{
    c->pos = TokenAtual(p)->off;
    c->tipo_nome = c->membro = -1;
    c->padrao = TipoAtual(p) == KW_DEFAULT;
    if (!c->padrao && TipoAtual(p) != KW_CASE) return ErroSintaxe(p, "'case', 'default' ou '}'");
    p->cur++;
    if (!c->padrao && !AnalisarAlvoDoCaso(p, c)) return false;
    if (!Esperar(p, TK_COLON, "':'")) return false;
    if (TipoAtual(p) == TK_NL) p->cur++;
    return AnalisarBloco(p, &c->corpo, true);
}

static bool AnalisarCasos(Parser *p, Stmt *s)
{
    size_t base = p->pilha_casos.n;
    while (TipoAtual(p) != TK_RBRACE) {
        Caso c = {0};
        if (!AnalisarCaso(p, &c)) return false;
        *(Caso *)VecEmpurrar(&p->pilha_casos) = c;
    }
    s->ncasos = (uint32_t)(p->pilha_casos.n - base);
    s->casos = CopiarLista(p, &p->pilha_casos, base);
    return true;
}

/* switch expr { ... } ou switch nome in expr { ... } */
static Stmt *AnalisarSwitch(Parser *p)
{
    Stmt *s = NovoStmt(p, ST_SWITCH, TokenAtual(p)->off);
    p->cur++;
    if (TipoAtual(p) == TK_IDENT && TipoSeguinte(p) == KW_IN) {
        s->nome = TokenAtual(p)->sym;
        p->cur += 2;
    }
    s->expr = AnalisarCabecalho(p);
    if (s->expr == NULL || !EsperarAbreBloco(p) || !AnalisarCasos(p, s)) return NULL;
    s->pos_fim = TokenAtual(p)->off;
    p->cur++;
    return EsperarFimDeLinha(p) ? s : NULL;
}

static const AnalisaComando COMANDOS[TK_QTD] = {
    [KW_LET] = AnalisarLetVar, [KW_VAR] = AnalisarLetVar, [KW_IF] = AnalisarIf, [KW_WHILE] = AnalisarWhile,
    [KW_FOR] = AnalisarFor, [KW_RETURN] = AnalisarReturn, [KW_BREAK] = AnalisarSemArgumentos,
    [KW_CONTINUE] = AnalisarSemArgumentos, [KW_DEFER] = AnalisarDefer, [KW_SWITCH] = AnalisarSwitch,
    [TK_IDENT] = AnalisarPorNome, [KW_TRY] = AnalisarTryComando,
};

static Stmt *AnalisarComando(Parser *p)
{
    AnalisaComando analisa = COMANDOS[TipoAtual(p)];
    if (analisa == NULL) {
        ErroSintaxe(p, "comando");
        return NULL;
    }
    return analisa(p);
}
