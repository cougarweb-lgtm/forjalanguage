/* parser_expr.c - parser das expressoes: or_else < or < and < comparacao < soma < mult < cast < unario/try < posfixo.
 * Versao: 0.17.1
 * Changelog: 0.17.1 - O0 P13: o '^' pos-fixo seguido de nome ou numero na linha chama ParserIdiomaXor (E0110: o xor e 'a ~ b').
 *            0.17.0 - B12: AnalisarLoad ('#load("arquivo")' em AnalisarPrimario: EX_TEXTO com carregar = true e nome = caminho; Parser.cargas guarda o no) e NovaExpr zera carregar.
 *            0.16.0 - F1 P1a: RegistrarChaveLiteral grava o offset da '{' de literal em Parser.chaves_literal (opcional; NULL = nada muda).
 *            0.15.0 - B01d: LerArgumento grava Arg.pos_fim (fim do ultimo token do argumento) para o forja migrar.
 *            0.14.0 - IA1-L2: '[' onde se espera expressao (literal de array de outra linguagem) vira E0110 (ParserIdiomaArray) em vez do E0101.
 *            0.13.0 - IA1-L1: '&&', '||' (AnalisarBinario) e '!x' (AnalisarUnario) viram E0110 (idioma_parser.c).
 *            0.12.0 - Linguagem v1.12 M3 P21v-a: posfixo '(' (AnalisarChamadaIndireta, EX_CHAMADA_IND).
 *            0.11.0 -Linguagem v1.11 L14a: 'quat{...}' (SYM_QUAT, minuscula) tambem e literal de struct.
 *            0.10.0 - Linguagem v1.11 L9: size_of(T) e align_of(T) levam um TIPO no 1o argumento (EX_TIPO), como new e make.
 *            0.9.0 - Linguagem v1.10 (A3): literais de struct e de array aceitam quebra de linha depois de '{', de ',' e antes de '}' (PularQuebras).
 *            0.8.0 - Linguagem v1.9 P2: indice duplo m[i, j] (EX_INDICE com args[0] e nargs = 1).
 *            0.7.0 - Linguagem v1.8 P2a: nome qualificado 'apelido.nome' em AnalisarNomeOuChamada (Expr.modulo; caminho rapido com napelidos == 0).
 *            0.6.0 - Linguagem v1.7 P1: f"..." (AnalisarFTexto, EX_FTEXTO) em AnalisarPrimario.
 *            0.5.0 - Linguagem v1.6 P1b: 'new(T, ...)' e 'make(T, ...)': o 1o argumento e um TIPO (EX_TIPO); LerArgumentosAte extraida.
 *            0.4.0 - Linguagem v1.5 P1b: literal de array '{a, b}' (EX_LITERAL_ARRAY), '[' posfixo (EX_INDICE, EX_FATIA) e E0101 em '[' primario.
 *            0.3.1 - Linguagem v1.3: 'none' (EX_NONE) e '^' posfixo (OP_DEREF) em AnalisarPosfixo.
 *            0.3.0 - Linguagem v1.3: o alvo do 'as' e um TipoRef (Expr.alvo); OPERADORES[TK_QTD].
 *            0.2.0 - Linguagem v1.2: operadores de bits (& | ~ << >>) na tabela OPERADORES, '~' unario, digitos do EX_INT em Expr.op.
 *            0.1.0 - Fase 5b: escalada de precedencia do prototipo + 'try' (prefixo, nivel dos unarios) e 'or_else'
 *                    (binario de menor precedencia, associativo a esquerda); EX_TEXTO; o campo 'flexivel' fica para a semantica.
 * Verificacao: construir.bat && forja.exe ast <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/parser_interno.h"

enum { NV_OR_ELSE, NV_OU, NV_E, NV_CMP, NV_SOMA, NV_MUL };

Expr *NovaExpr(Parser *p, ExprTipo tipo, uint32_t pos)
{
    Expr *e = ArenaAlocar(p->arena, sizeof(Expr));
    e->tipo_no = (uint8_t)tipo;
    e->pos = pos;
    e->pos_op = pos;
    e->nome = -1;
    e->carregar = false;
    return e;
}

/* Indexada pelo token: nivel+1 (0 = nao e operador binario) e o operador. */
static const struct { uint8_t nivel_mais1, op; } OPERADORES[TK_QTD] = {
    [KW_OR_ELSE] = {NV_OR_ELSE + 1, OP_OU}, [KW_OR] = {NV_OU + 1, OP_OU}, [KW_AND] = {NV_E + 1, OP_E},
    [TK_EQ] = {NV_CMP + 1, OP_IGUAL}, [TK_NE] = {NV_CMP + 1, OP_DIFERENTE}, [TK_LT] = {NV_CMP + 1, OP_MENOR},
    [TK_LE] = {NV_CMP + 1, OP_MENOR_IGUAL}, [TK_GT] = {NV_CMP + 1, OP_MAIOR}, [TK_GE] = {NV_CMP + 1, OP_MAIOR_IGUAL},
    [TK_PLUS] = {NV_SOMA + 1, OP_SOMA}, [TK_MINUS] = {NV_SOMA + 1, OP_SUB},
    [TK_STAR] = {NV_MUL + 1, OP_MUL}, [TK_SLASH] = {NV_MUL + 1, OP_DIV}, [TK_PERCENT] = {NV_MUL + 1, OP_RESTO},
    /* bits (Go/Odin): & << >> no nivel de * / %; | e ~ (ou-exclusivo) no nivel de + - */
    [TK_AMP] = {NV_MUL + 1, OP_BIT_E}, [TK_SHL] = {NV_MUL + 1, OP_DESLOCA_ESQ}, [TK_SHR] = {NV_MUL + 1, OP_DESLOCA_DIR},
    [TK_PIPE] = {NV_SOMA + 1, OP_BIT_OU}, [TK_TIL] = {NV_SOMA + 1, OP_BIT_XOU},
};

/* Nivel do operador binario no token atual (-1 se nao for), e o operador em *op. */
static int NivelAtual(const Parser *p, uint8_t *op)
{
    TokenTipo t = TipoAtual(p);
    *op = OPERADORES[t].op;
    return (int)OPERADORES[t].nivel_mais1 - 1;
}

/* Argumento "[nome =] expr"; so_nomeado vale para literal de struct. */
static bool LerArgumento(Parser *p, Arg *a, bool so_nomeado)
{
    bool nomeado = TipoAtual(p) == TK_IDENT && TipoSeguinte(p) == TK_ASSIGN;
    a->nome = -1;
    a->indice = -1;
    if (so_nomeado && !nomeado) return ErroSintaxe(p, "nome do campo seguido de '='");
    if (nomeado) {
        a->nome = TokenAtual(p)->sym;
        a->pos_nome = TokenAtual(p)->off;
        p->cur += 2;
    }
    a->valor = AnalisarExpr(p);
    if (a->valor == NULL) return false;
    const Token *ultimo = &p->toks[p->cur - 1];
    a->pos_fim = ultimo->off + ultimo->len; /* B01d: onde o argumento termina (inicio do ',' ou ')' em geral) */
    return true;
}

/* v1.10 (A3): os literais de struct e de array (fechador '}') aceitam quebra de linha depois de '{', depois de ',' e antes de '}';
 * a virgula entre os itens continua obrigatoria. */
static void PularQuebras(Parser *p, bool pula)
{
    while (pula && TipoAtual(p) == TK_NL) p->cur++;
}

/* Argumentos separados por ',' ate o fechador (sem consumi-lo). */
static bool LerArgumentosAte(Parser *p, TokenTipo fecha, bool so_nomeado)
{
    bool multilinha = fecha == TK_RBRACE;
    PularQuebras(p, multilinha);
    while (TipoAtual(p) != fecha) {
        Arg a = {0};
        if (!LerArgumento(p, &a, so_nomeado)) return false;
        *(Arg *)VecEmpurrar(&p->pilha_args) = a;
        PularQuebras(p, multilinha);
        if (TipoAtual(p) != TK_COMMA) break;
        p->cur++;
        PularQuebras(p, multilinha);
    }
    return true;
}

/* Lista de argumentos ate o fechador (consumido). Dentro dela o literal de struct volta a valer. */
static bool AnalisarListaArgs(Parser *p, Expr *e, TokenTipo fecha, bool so_nomeado)
{
    bool salvo = p->sem_literal_struct;
    size_t base = p->pilha_args.n;
    p->sem_literal_struct = false;
    if (!LerArgumentosAte(p, fecha, so_nomeado)) return false;
    p->sem_literal_struct = salvo;
    if (!Esperar(p, fecha, fecha == TK_RPAREN ? "')'" : "'}'")) return false;
    e->nargs = (uint32_t)(p->pilha_args.n - base);
    e->args = CopiarLista(p, &p->pilha_args, base);
    return true;
}

/* Argumentos de new/make: o 1o e um TIPO (EX_TIPO), o resto como em AnalisarListaArgs. */
static bool AnalisarListaArgsDeTipo(Parser *p, Expr *e)
{
    bool salvo = p->sem_literal_struct;
    size_t base = p->pilha_args.n;
    p->sem_literal_struct = false;
    if (TipoAtual(p) != TK_RPAREN) {
        TipoRef *t = ArenaAlocar(p->arena, sizeof(TipoRef));
        if (!AnalisarTipoRef(p, t)) return false;
        Expr *alvo = NovaExpr(p, EX_TIPO, t->pos);
        alvo->alvo = t;
        Arg primeiro = {0};
        primeiro.nome = -1;
        primeiro.indice = -1;
        primeiro.valor = alvo;
        *(Arg *)VecEmpurrar(&p->pilha_args) = primeiro;
        if (TipoAtual(p) != TK_COMMA && TipoAtual(p) != TK_RPAREN)
            return ErroSintaxe(p, "o 1o argumento de new, make, size_of e align_of e um tipo: new(Grifo, alloc = heap)");
        if (TipoAtual(p) == TK_COMMA) p->cur++;
        if (!LerArgumentosAte(p, TK_RPAREN, false)) return false;
    }
    p->sem_literal_struct = salvo;
    if (!Esperar(p, TK_RPAREN, "')'")) return false;
    e->nargs = (uint32_t)(p->pilha_args.n - base);
    e->args = CopiarLista(p, &p->pilha_args, base);
    return true;
}

/* F1: grava o offset da '{' de literal (token atual) quando o chamador pediu (forja fmt); sem o Vec nada acontece. */
static void RegistrarChaveLiteral(Parser *p)
{
    if (p->chaves_literal != NULL) *(uint32_t *)VecEmpurrar(p->chaves_literal) = TokenAtual(p)->off;
}

static Expr *AnalisarNomeOuChamada(Parser *p)
{
    uint32_t pos_apelido = 0;
    uint8_t modulo = p->napelidos != 0 ? LerApelido(p, &pos_apelido) : 0;
    const Token *t = TokenAtual(p);
    Expr *e = NovaExpr(p, EX_NOME, modulo != 0 ? pos_apelido : t->off);
    bool maiuscula = (p->fonte->texto[t->off] >= 'A' && p->fonte->texto[t->off] <= 'Z') || t->sym == SYM_QUAT;
    e->pos_op = t->off;
    e->modulo = modulo;
    e->nome = t->sym;
    p->cur++;
    if (TipoAtual(p) == TK_LPAREN) {
        e->tipo_no = EX_CHAMADA;
        p->cur++;
        bool de_tipo = t->sym == SYM_NEW || t->sym == SYM_MAKE || t->sym == SYM_SIZE_OF || t->sym == SYM_ALIGN_OF;
        if (de_tipo) return AnalisarListaArgsDeTipo(p, e) ? e : NULL;
        return AnalisarListaArgs(p, e, TK_RPAREN, false) ? e : NULL;
    }
    if (TipoAtual(p) == TK_LBRACE && maiuscula && !p->sem_literal_struct) {
        e->tipo_no = EX_LITERAL_STRUCT;
        RegistrarChaveLiteral(p);
        p->cur++;
        return AnalisarListaArgs(p, e, TK_RBRACE, true) ? e : NULL;
    }
    return e;
}

static Expr *AnalisarParenteses(Parser *p)
{
    uint32_t pos = TokenAtual(p)->off;
    bool salvo = p->sem_literal_struct;
    p->cur++;
    p->sem_literal_struct = false;
    Expr *e = AnalisarExpr(p);
    p->sem_literal_struct = salvo;
    if (e == NULL || !Esperar(p, TK_RPAREN, "')'")) return NULL;
    e->pos = pos;
    return e;
}

/* Literal de um token so: numero, texto ou bool (op = valor do bool). */
static Expr *AnalisarLiteral(Parser *p, ExprTipo tipo, uint8_t op)
{
    Expr *e = NovaExpr(p, tipo, TokenAtual(p)->off);
    e->op = op;
    if (tipo == EX_INT) {
        const Token *tk = TokenAtual(p);
        e->op = (uint8_t)(tk->len < 255 ? tk->len : 255); /* digitos: caminho rapido do E0420 */
        if (tk->len >= 2 && p->fonte->texto[tk->off + 1] > '9') e->op = 255; /* 0x.../0b...: a conta de digitos decimais nao vale, sempre le o valor */
    }
    if (tipo == EX_TEXTO) e->nome = TokenAtual(p)->sym;
    p->cur++;
    return e;
}

/* '{a, b}' (so valores; o tipo vem do contexto na semantica). No cabecalho o '{' abre o bloco (D22). */
static Expr *AnalisarLiteralArray(Parser *p)
{
    if (p->sem_literal_struct) {
        ErroSintaxe(p, "expressao (literal de array no cabecalho: declare o array antes)");
        return NULL;
    }
    Expr *e = NovaExpr(p, EX_LITERAL_ARRAY, TokenAtual(p)->off);
    RegistrarChaveLiteral(p);
    p->cur++;
    if (!AnalisarListaArgs(p, e, TK_RBRACE, false)) return NULL;
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (e->args[i].nome < 0) continue;
        DiagRelatar(p->diag, E0101, e->args[i].pos_nome,
                    "literal de array so tem valores; literal de struct leva o nome do tipo: Tipo{campo = valor}");
        return NULL;
    }
    return e;
}

/* Parte de texto do f"...": valor = EX_TEXTO com o pedaco internado; Arg.nome = -1. */
static bool AnalisarPedacoDoFTexto(Parser *p)
{
    Expr *texto = NovaExpr(p, EX_TEXTO, TokenAtual(p)->off);
    Arg a = {.nome = -1, .indice = -1, .valor = texto};
    texto->nome = TokenAtual(p)->sym;
    p->cur++;
    *(Arg *)VecEmpurrar(&p->pilha_args) = a;
    return true;
}

/* Parte '{expr[:formato]}': Arg.nome = texto do formato (ou -1), pos_nome = o ':' (ou o inicio da expressao), indice = 0. */
static bool AnalisarChaveDoFTexto(Parser *p)
{
    bool salvo = p->sem_literal_struct;
    Arg a = {.nome = -1, .indice = 0};
    p->cur++;
    p->sem_literal_struct = false;
    a.valor = AnalisarExpr(p);
    p->sem_literal_struct = salvo;
    if (a.valor == NULL) return false;
    a.pos_nome = a.valor->pos;
    if (TipoAtual(p) == TK_FTEXTO_FORMATO) {
        a.nome = TokenAtual(p)->sym;
        a.pos_nome = TokenAtual(p)->off;
        p->cur++;
    }
    if (!Esperar(p, TK_FTEXTO_FIM_CHAVE, "'}' no fim de {expressao} do f\"...\"")) return false;
    *(Arg *)VecEmpurrar(&p->pilha_args) = a;
    return true;
}

/* f"...": partes (texto e {expr}) no acumulador de argumentos; f"" tem 0 partes. */
static Expr *AnalisarFTexto(Parser *p)
{
    size_t base = p->pilha_args.n;
    Expr *e = NovaExpr(p, EX_FTEXTO, TokenAtual(p)->off);
    p->cur++;
    while (TipoAtual(p) != TK_FTEXTO_FIM) {
        bool ok = TipoAtual(p) == TK_FTEXTO_PEDACO ? AnalisarPedacoDoFTexto(p) : TipoAtual(p) == TK_FTEXTO_CHAVE ? AnalisarChaveDoFTexto(p) : ErroSintaxe(p, "texto ou '{' no f\"...\"");
        if (!ok) return NULL;
    }
    p->cur++;
    e->nargs = (uint32_t)(p->pilha_args.n - base);
    e->args = CopiarLista(p, &p->pilha_args, base);
    return e;
}

/* B12: o nome depois do '#' e exatamente 'load'. */
static bool TokenEhLoad(Parser *p)
{
    const Token *tk = TokenAtual(p);
    return TipoAtual(p) == TK_IDENT && tk->len == 4 && memcmp(p->fonte->texto + tk->off, "load", 4) == 0;
}

/* B12: '#load("arquivo")' e EX_TEXTO com carregar = true e nome = caminho cru (sem aspas); o no vai para Parser.cargas e o driver troca o nome
 * pelo conteudo do arquivo antes da semantica (ResolverLoads). Argumento que nao e literal, ou '#' sem 'load', = erro de sintaxe. */
static Expr *AnalisarLoad(Parser *p)
{
    Expr *e;
    if (p->cargas == NULL) { ErroSintaxe(p, "expressao"); return NULL; }
    e = NovaExpr(p, EX_TEXTO, TokenAtual(p)->off);
    p->cur++;
    if (!TokenEhLoad(p)) { ErroSintaxe(p, "#load(\"arquivo\")"); return NULL; }
    p->cur++;
    if (!Esperar(p, TK_LPAREN, "'(' depois de #load")) return NULL;
    if (TipoAtual(p) != TK_TEXTO) { ErroSintaxe(p, "caminho entre aspas em #load"); return NULL; }
    e->nome = TokenAtual(p)->sym;
    e->carregar = true;
    p->cur++;
    if (!Esperar(p, TK_RPAREN, "')' em #load")) return NULL;
    *(Expr **)VecEmpurrar(p->cargas) = e;
    return e;
}

static Expr *AnalisarPrimario(Parser *p)
{
    switch (TipoAtual(p)) {
    case TK_FTEXTO_INICIO: return AnalisarFTexto(p);
    case TK_HASH: return AnalisarLoad(p);
    case TK_LBRACE: return AnalisarLiteralArray(p);
    case TK_LCOLCHETE:
        ParserIdiomaArray(p); /* IA1-L2: '[1, 2]' e idioma de Python, JS e Rust */
        return NULL;
    case TK_INT: return AnalisarLiteral(p, EX_INT, 0);
    case TK_REAL: return AnalisarLiteral(p, EX_REAL, 0);
    case TK_TEXTO: return AnalisarLiteral(p, EX_TEXTO, 0);
    case KW_TRUE: return AnalisarLiteral(p, EX_BOOL, 1);
    case KW_FALSE: return AnalisarLiteral(p, EX_BOOL, 0);
    case KW_NONE: return AnalisarLiteral(p, EX_NONE, 0);
    case TK_IDENT: return AnalisarNomeOuChamada(p);
    case TK_LPAREN: return AnalisarParenteses(p);
    default: ErroSintaxe(p, "expressao"); return NULL;
    }
}

/* Limite de fatia: falta dele ('a[1..<]', 'a[..<3]') aponta o token onde devia estar. */
static Expr *AnalisarLimiteDaFatia(Parser *p)
{
    TokenTipo t = TipoAtual(p);
    if (t == TK_RCOLCHETE || t == TK_DOTDOTLT || t == TK_DOTDOTEQ) {
        ErroSintaxe(p, "limite da fatia: a[lo..<hi] ou a[..]");
        return NULL;
    }
    return AnalisarExpr(p);
}

/* 'a[lo..<hi]' / 'a[lo..=hi]' com o 1o limite ja lido e o token atual na faixa. */
static Expr *AnalisarFatiaComFaixa(Parser *p, Expr *base, Expr *inicio, uint32_t pos_op)
{
    Expr *e = NovaExpr(p, EX_FATIA, base->pos);
    size_t pilha_base = p->pilha_args.n;
    e->op = TipoAtual(p) == TK_DOTDOTLT ? FATIA_EXCL : FATIA_INCL;
    p->cur++;
    Expr *fim = AnalisarLimiteDaFatia(p);
    if (fim == NULL) return NULL;
    Arg a = {.nome = -1, .indice = -1, .valor = inicio};
    *(Arg *)VecEmpurrar(&p->pilha_args) = a;
    a.valor = fim;
    *(Arg *)VecEmpurrar(&p->pilha_args) = a;
    e->a = base;
    e->pos_op = pos_op;
    e->nargs = 2;
    e->args = CopiarLista(p, &p->pilha_args, pilha_base);
    return e;
}

/* v1.9: 'm[i, j]': o 2o indice vai em args[0].valor (nargs = 1). O token atual e a ','. */
static bool AnalisarSegundoIndice(Parser *p, Expr *e)
{
    p->cur++;
    Expr *segundo = AnalisarLimiteDaFatia(p);
    if (segundo == NULL) return false;
    e->args = ArenaAlocar(p->arena, sizeof(Arg));
    e->args[0] = (Arg){.nome = -1, .pos_nome = segundo->pos, .valor = segundo, .indice = -1};
    e->nargs = 1;
    return true;
}

/* Depois do '[': '..' ']' (fatia inteira), expr ']' (indice) ou expr ('..<'|'..=') expr ']' (fatia). */
static Expr *AnalisarDentroDoColchete(Parser *p, Expr *base, uint32_t pos_op)
{
    Expr *e;
    if (TipoAtual(p) == TK_DOTDOT) {
        p->cur++;
        e = NovaExpr(p, EX_FATIA, base->pos);
        e->op = FATIA_INTEIRA;
    } else {
        Expr *primeiro = AnalisarLimiteDaFatia(p);
        if (primeiro == NULL) return NULL;
        if (TipoAtual(p) == TK_DOTDOTLT || TipoAtual(p) == TK_DOTDOTEQ) {
            e = AnalisarFatiaComFaixa(p, base, primeiro, pos_op);
            if (e == NULL) return NULL;
        } else {
            e = NovaExpr(p, EX_INDICE, base->pos);
            e->b = primeiro;
            if (__builtin_expect(TipoAtual(p) == TK_COMMA, 0) && !AnalisarSegundoIndice(p, e)) return NULL;
        }
    }
    e->a = base;
    e->pos_op = pos_op;
    return Esperar(p, TK_RCOLCHETE, "']'") ? e : NULL;
}

/* Posfixo '[': dentro dos colchetes o literal de struct volta a valer (como nos parenteses). */
static Expr *AnalisarColchete(Parser *p, Expr *base)
{
    bool salvo = p->sem_literal_struct;
    uint32_t pos_op = TokenAtual(p)->off;
    p->cur++;
    p->sem_literal_struct = false;
    Expr *e = AnalisarDentroDoColchete(p, base, pos_op);
    p->sem_literal_struct = salvo;
    return e;
}

/* v1.12 P21v-a: posfixo '(': chamada sobre uma expressao de tipo proc (campo, resultado de chamada, '(f as proc(...))(x)'). */
static Expr *AnalisarChamadaIndireta(Parser *p, Expr *chamado)
{
    Expr *e = NovaExpr(p, EX_CHAMADA_IND, chamado->pos);
    e->a = chamado;
    e->pos_op = TokenAtual(p)->off;
    p->cur++;
    return AnalisarListaArgs(p, e, TK_RPAREN, false) ? e : NULL;
}

Expr *AnalisarPosfixo(Parser *p)
{
    Expr *e = AnalisarPrimario(p);
    while (e != NULL && (TipoAtual(p) == TK_DOT || TipoAtual(p) == TK_CARET || TipoAtual(p) == TK_LCOLCHETE || TipoAtual(p) == TK_LPAREN)) {
        if (TipoAtual(p) == TK_LPAREN) {
            e = AnalisarChamadaIndireta(p, e);
            continue;
        }
        if (TipoAtual(p) == TK_LCOLCHETE) {
            e = AnalisarColchete(p, e);
            continue;
        }
        if (TipoAtual(p) == TK_CARET) { /* 'p^': desreferencia */
            if (ParserIdiomaXor(p)) return NULL; /* O0 P13: 'a ^ b' e o xor de C; na Forja e 'a ~ b' */
            Expr *deref = NovaExpr(p, EX_UNARIO, e->pos);
            deref->op = OP_DEREF;
            deref->a = e;
            deref->pos_op = TokenAtual(p)->off;
            p->cur++;
            e = deref;
            continue;
        }
        p->cur++;
        Expr *campo = NovaExpr(p, EX_CAMPO, e->pos);
        campo->a = e;
        if (!LerNome(p, &campo->nome, &campo->pos_op, "nome do campo")) return NULL;
        e = campo;
    }
    return e;
}

static Expr *AnalisarUnario(Parser *p);

/* Prefixo (-, not, &, try): consome o token e o operando, que e outro unario. */
static Expr *AnalisarPrefixo(Parser *p, ExprTipo tipo, uint8_t op)
{
    Expr *e = NovaExpr(p, tipo, TokenAtual(p)->off);
    e->op = op;
    p->cur++;
    e->a = AnalisarUnario(p);
    return e->a == NULL ? NULL : e;
}

static Expr *AnalisarUnario(Parser *p)
{
    switch (TipoAtual(p)) {
    case TK_MINUS: return AnalisarPrefixo(p, EX_UNARIO, OP_NEGAR);
    case KW_NOT: return AnalisarPrefixo(p, EX_UNARIO, OP_NAO);
    case TK_AMP: return AnalisarPrefixo(p, EX_UNARIO, OP_ENDERECO);
    case TK_TIL: return AnalisarPrefixo(p, EX_UNARIO, OP_BIT_NAO);
    case KW_TRY: return AnalisarPrefixo(p, EX_TRY, 0);
    case TK_BANG: ParserIdiomaNegacao(p); return NULL; /* IA1-L1: '!x' e idioma de C, Java, JS e Go */
    default: return AnalisarPosfixo(p);
    }
}

static Expr *AnalisarConversao(Parser *p)
{
    Expr *e = AnalisarUnario(p);
    while (e != NULL && TipoAtual(p) == KW_AS) {
        p->cur++;
        Expr *conv = NovaExpr(p, EX_CONVERSAO, e->pos);
        conv->a = e;
        conv->alvo = ArenaAlocar(p->arena, sizeof(TipoRef));
        if (!AnalisarTipoRef(p, conv->alvo)) return NULL;
        conv->pos_op = conv->alvo->pos;
        conv->nome = conv->alvo->forma == TR_NOME ? conv->alvo->nome : -1;
        e = conv;
    }
    return e;
}

static Expr *NovoBinario(Parser *p, TokenTipo token, uint8_t op, Expr *a, Expr *b)
{
    Expr *e = NovaExpr(p, token == KW_OR_ELSE ? EX_OR_ELSE : EX_BINARIO, a->pos);
    e->op = op;
    e->a = a;
    e->b = b;
    return e;
}

/* Escalada de precedencia: so consome operadores de nivel >= minimo; comparacao nao encadeia. */
static Expr *AnalisarBinario(Parser *p, int minimo)
{
    Expr *esq = AnalisarConversao(p);
    uint8_t op;
    int nivel;
    while (esq != NULL && (nivel = NivelAtual(p, &op)) >= minimo) {
        TokenTipo token = TipoAtual(p);
        uint32_t pos_op = TokenAtual(p)->off;
        if (__builtin_expect(token == TK_AMP || token == TK_PIPE, 0) && ParserIdiomaOperador(p)) return NULL; /* IA1-L1: '&&' e '||' */
        p->cur++;
        Expr *dir = AnalisarBinario(p, nivel + 1);
        if (dir == NULL) return NULL;
        esq = NovoBinario(p, token, op, esq, dir);
        esq->pos_op = pos_op;
        if (nivel == NV_CMP && NivelAtual(p, &op) == NV_CMP) break;
    }
    return esq;
}

Expr *AnalisarExpr(Parser *p)
{
    return AnalisarBinario(p, NV_OR_ELSE);
}

/* Cabecalho de if/while/for/switch: '{' termina a expressao em vez de abrir literal de struct. */
Expr *AnalisarCabecalho(Parser *p)
{
    p->sem_literal_struct = true;
    Expr *e = AnalisarExpr(p);
    p->sem_literal_struct = false;
    return e;
}
