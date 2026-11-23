/* lower_stmt.c - lowering dos comandos simples (let/var, atribuicao, return, expect, chamada) e dos blocos { } com defer.
 * Versao: 0.5.3
 * Changelog: 0.5.3 - M6e P93b: LwAtribuicao de `p = none` logo depois do none que o delete(p) gravou nao repete a escrita.
 *            0.5.2 - Linguagem v1.12 M3 P21v-b: LwChamadaComando baixa EX_CHAMADA_IND (obj.campo(x) solto e apos defer) por LwChamadaInd.
 *            0.5.1 -Linguagem v1.11 L12a: CombinarComposto de '+=' e '-=' com alvo [N]T usa LwContaVetor.
 *            0.5.0 -Linguagem v1.9 P8: LwGravacao desvia a[i] = s e a[i].campo = v de soa (LwGravacaoSoa).
 *            0.4.1 - Linguagem v1.8 P7: LwLetVar empilha o defer sintetico do em = escopo.
 *            0.4.0 - Linguagem v1.8 P4: LwFalhaStmt usa FonteLocalizarNoArquivo (caminho e linha do trecho).
 *            0.3.0 -Linguagem v1.7 P8: LwStmtLetMultiplo (IR_TUPLA_ELEMENTO + IR_LOCAL por nome); sai o provisorio do P1.
 *            0.2.0 - Linguagem v1.7 P1: COMANDOS[ST_LET_MULTIPLO + 1].
 *            0.1.1 - Linguagem v1.3: LwGravacao (IR_GRAVA_PONTEIRO) para alvo que nao e local; EstruturaDoCampo recebe o tipo da base.
 *            0.1.0 - Fase 5d: primeira versao (contrato em lower_interno.h; regras do arquiteto em MEM-LJ-ADR-38).
 * Verificacao: gcc -std=c11 -Wall -Wextra -Werror -I. -D__USE_MINGW_ANSI_STDIO=1 -fsyntax-only ir/lower_stmt.c;
 *              construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include "ir/lower_interno.h"

typedef void (*LowerComando)(LowerCtx *c, const Stmt *s);

/* Anotacao faltando na AST: diz onde (arquivo:linha:coluna) e para o compilador. */
_Noreturn void LwFalhaStmt(LowerCtx *c, const Stmt *s, const char *motivo)
{
    FontePosicao p = FonteLocalizarNoArquivo(c->sem->fonte, s->pos);
    const char *caminho = p.caminho != NULL ? p.caminho : "?";
    FalhaInterna("lowering: %s em %s:%u:%u", motivo, caminho, p.linha, p.coluna);
}

/* ---- let / var ---- */

static void LwLetVar(LowerCtx *c, const Stmt *s)
{
    if (s->tipo_local == TIPO_NAO_RESOLVIDO) LwFalhaStmt(c, s, "let/var sem tipo resolvido");
    Valor inicial = s->expr != NULL ? LwExpr(c, s->expr) : LwValorNenhum();
    IrInstr *i = LwNovaInstr(c, IR_LOCAL, s->pos);
    i->destino = s->local_id;
    i->tipo = s->tipo_local;
    i->a = inicial;
    i->mutavel = s->tipo_no == ST_VAR;
    if (s->corpo != NULL) LwStmtDefer(c, s->corpo); /* em = escopo: o delete sintetico sai no fim do bloco */
}

/* ---- atribuicao ---- */

/* Local que o alvo (nome simples ou base do campo) liga; o resto do compilador so aceita local/parametro aqui. */
static int32_t LocalDoAlvo(LowerCtx *c, const Stmt *s, const Expr *nome)
{
    if (nome->tipo_no != EX_NOME || (nome->ref != REF_LOCAL && nome->ref != REF_PARAM)) {
        LwFalhaStmt(c, s, "alvo da atribuicao nao liga a um local");
    }
    return nome->ref_id;
}

/* Valor atual do alvo, para '+=' e '-=': o proprio local ou um IR_CAMPO sobre a base. */
static Valor LerAlvo(LowerCtx *c, const Stmt *s, int32_t base)
{
    const Expr *alvo = s->alvo;
    if (alvo->tipo_no == EX_NOME) return LwValorLocal(base, alvo->tipo);
    IrInstr *i = LwNovaInstr(c, IR_CAMPO, s->pos);
    i->destino = LwNovoTemp(c);
    i->tipo = alvo->tipo;
    i->a = LwValorLocal(base, alvo->a->tipo);
    i->nome = alvo->nome;
    return LwValorTemp(i->destino, alvo->tipo);
}

/* '+=' / '-=': atual op direita, no tipo do alvo. */
static Valor CombinarComposto(LowerCtx *c, const Stmt *s, Valor atual, Valor novo)
{
    if (TipoClasseDe(&c->sem->tipos, s->alvo->tipo) == TC_ARRAY) { /* v1.11 L12a: [N]T += [N]T, elemento a elemento */
        ContaVetor conta = {.op = s->op == ATRIB_SOMA ? OP_SOMA : OP_SUB, .tipo = s->alvo->tipo, .pos = s->pos, .a = atual, .b = novo};
        return LwContaVetor(c, &conta);
    }
    IrInstr *i = LwNovaInstr(c, IR_BINARIO, s->pos);
    i->destino = LwNovoTemp(c);
    i->tipo = s->alvo->tipo;
    i->op_aux = (uint8_t)(s->op == ATRIB_SOMA ? OP_SOMA : OP_SUB);
    i->a = atual;
    i->b = novo;
    return LwValorTemp(i->destino, i->tipo);
}

/* O valor a gravar: o da direita; em '+=' e '-=' vira atual op direita (a direita e calculada antes de ler o alvo). */
static Valor ValorAtribuido(LowerCtx *c, const Stmt *s, int32_t base)
{
    Valor novo = LwExpr(c, s->expr);
    if (s->op == 0) return novo;
    return CombinarComposto(c, s, LerAlvo(c, s, base), novo);
}

/* Decl da struct dona do campo: a base do campo e struct ou ponteiro para struct (desce um nivel so). */
static const Decl *EstruturaDoCampo(LowerCtx *c, const Stmt *s, TipoId t)
{
    if (TipoClasseDe(&c->sem->tipos, t) == TC_PONTEIRO) t = TipoDe(&c->sem->tipos, t)->a;
    if (TipoClasseDe(&c->sem->tipos, t) != TC_STRUCT) LwFalhaStmt(c, s, "campo de algo que nao e struct");
    return c->sem->globais[TipoDe(&c->sem->tipos, t)->a].decl;
}

/* Local, ou campo de local (a base e um nome): continua IR_ATRIBUICAO. O resto grava por ponteiro. */
static bool AlvoEhLocal(const Expr *alvo)
{
    if (alvo->tipo_no == EX_NOME) return true;
    return alvo->tipo_no == EX_CAMPO && alvo->a->tipo_no == EX_NOME;
}

static bool EhDeref(const Expr *e)
{
    return e->tipo_no == EX_UNARIO && e->op == OP_DEREF;
}

/* O ponteiro do alvo, calculado uma vez: 'E^' -> E; 'E^.campo' -> E; 'E.campo' com E ^Struct -> E. */
static Valor PonteiroDoAlvo(LowerCtx *c, const Stmt *s)
{
    const Expr *alvo = s->alvo;
    if (EhDeref(alvo)) return LwExpr(c, alvo->a);
    if (alvo->tipo_no == EX_INDICE) return LwEndereco(c, alvo);
    if (alvo->tipo_no != EX_CAMPO || alvo->ref != REF_CAMPO) LwFalhaStmt(c, s, "alvo de gravacao sem ponteiro");
    if (EhDeref(alvo->a)) return LwExpr(c, alvo->a->a);
    if (TipoClasseDe(&c->sem->tipos, alvo->a->tipo) == TC_PONTEIRO) return LwExpr(c, alvo->a);
    return LwEndereco(c, alvo->a);
}

/* Valor atual do apontado, para '+=' e '-=': IR_CAMPO pelo ponteiro ou deref dele. */
static Valor LerApontado(LowerCtx *c, const Stmt *s, Valor ponteiro)
{
    bool campo = s->alvo->tipo_no == EX_CAMPO;
    IrInstr *i = LwNovaInstr(c, campo ? IR_CAMPO : IR_UNARIO, s->pos);
    i->destino = LwNovoTemp(c);
    i->tipo = s->alvo->tipo;
    i->a = ponteiro;
    if (campo) i->nome = s->alvo->nome;
    else i->op_aux = OP_DEREF;
    return LwValorTemp(i->destino, i->tipo);
}

/* Alvo na soa: a[i] = s ou a[i].campo = v / op=, com a uma soa (a struct nao existe inteira na memoria). */
static bool LwAlvoNaSoa(const LowerCtx *c, const Expr *alvo)
{
    if (alvo->tipo_no == EX_INDICE) return TipoClasseDe(&c->sem->tipos, alvo->a->tipo) == TC_SOA;
    return alvo->tipo_no == EX_CAMPO && alvo->a->tipo_no == EX_INDICE &&
           TipoClasseDe(&c->sem->tipos, alvo->a->a->tipo) == TC_SOA;
}

/* a[i] = s: espalha a struct pelas colunas (IR_SOA_GRAVA). Ordem: endereco da soa, indice, valor. */
static void LwGravacaoSoaInteira(LowerCtx *c, const Stmt *s)
{
    Valor ponteiro = LwEndereco(c, s->alvo->a);
    Valor indice = LwIndiceInt(c, s->alvo->b);
    Valor novo = LwExpr(c, s->expr);
    IrInstr *i = LwNovaInstr(c, IR_SOA_GRAVA, s->pos);
    i->a = ponteiro;
    i->b = indice;
    i->args = LwArgs(c, 1);
    i->args[0] = novo;
    i->nargs = 1;
}

/* a[i].campo = v / op=: ponteiro (base, indice) -> valor -> atual -> faixa -> grava, como o LwGravacao de hoje. */
static void LwGravacaoSoa(LowerCtx *c, const Stmt *s)
{
    if (s->alvo->tipo_no == EX_INDICE) {
        LwGravacaoSoaInteira(c, s);
        return;
    }
    Valor ponteiro = LwEnderecoCampoSoa(c, s->alvo);
    Valor novo = LwExpr(c, s->expr);
    if (s->op != 0) {
        IrInstr *atual = LwNovaInstr(c, IR_UNARIO, s->pos);
        atual->destino = LwNovoTemp(c);
        atual->tipo = s->alvo->tipo;
        atual->op_aux = OP_DEREF;
        atual->a = ponteiro;
        novo = CombinarComposto(c, s, LwValorTemp(atual->destino, atual->tipo), novo);
    }
    LwVerificaFaixa(c, EstruturaDoCampo(c, s, s->alvo->a->tipo), s->alvo->ref_id, novo, s->pos);
    IrInstr *i = LwNovaInstr(c, IR_GRAVA_PONTEIRO, s->pos);
    i->tipo = s->alvo->tipo;
    i->a = ponteiro;
    i->b = novo;
    i->nome = -1;
}

/* Gravacao por ponteiro: ponteiro (1 vez) -> valor da direita -> (+=/-=) atual op direita -> faixa -> IR_GRAVA_PONTEIRO. */
static void LwGravacao(LowerCtx *c, const Stmt *s)
{
    if (__builtin_expect(c->sem->tipos.tem_soa, 0) && LwAlvoNaSoa(c, s->alvo)) {
        LwGravacaoSoa(c, s);
        return;
    }
    bool campo = s->alvo->tipo_no == EX_CAMPO;
    Valor ponteiro = PonteiroDoAlvo(c, s);
    Valor novo = LwExpr(c, s->expr);
    if (s->op != 0) novo = CombinarComposto(c, s, LerApontado(c, s, ponteiro), novo);
    if (campo) {
        LwVerificaFaixa(c, EstruturaDoCampo(c, s, s->alvo->a->tipo), s->alvo->ref_id, novo, s->pos);
    }
    IrInstr *i = LwNovaInstr(c, IR_GRAVA_PONTEIRO, s->pos);
    i->tipo = s->alvo->tipo;
    i->a = ponteiro;
    i->b = novo;
    i->nome = campo ? s->alvo->nome : -1;
}

static void LwAtribuicao(LowerCtx *c, const Stmt *s)
{
    if (!AlvoEhLocal(s->alvo)) {
        LwGravacao(c, s);
        return;
    }
    bool campo = s->alvo->tipo_no == EX_CAMPO;
    if (campo && s->alvo->ref != REF_CAMPO) LwFalhaStmt(c, s, "alvo de campo sem ref de campo");
    int32_t base = LocalDoAlvo(c, s, campo ? s->alvo->a : s->alvo);
    Valor novo = ValorAtribuido(c, s, base);
    if (campo) LwVerificaFaixa(c, EstruturaDoCampo(c, s, s->alvo->a->tipo), s->alvo->ref_id, novo, s->pos);
    const IrInstr *ultima = c->atual->ultima;
    if (!campo && novo.tipo_no == V_NULO && ultima != NULL && ultima->op == IR_ATRIBUICAO && ultima->destino == base && ultima->a.tipo_no == V_NULO) {
        return; /* M6e P93b: `delete(p); p = none` - o delete ja gravou o none no nome (release byte a byte igual a `delete(p)`) */
    }
    IrInstr *i = LwNovaInstr(c, IR_ATRIBUICAO, s->pos);
    i->destino = base;
    i->tipo = s->alvo->tipo;
    i->a = novo;
    i->nome = campo ? s->alvo->nome : -1;
}

/* ---- chamada como comando ---- */

/* Resultado descartado (a semantica ja barrou resultado ignorado). 'try chamada' chega como EX_TRY. */
static void LwChamadaComando(LowerCtx *c, const Stmt *s)
{
    if (s->expr->tipo_no == EX_TRY) (void)LwTry(c, s->expr);
    else if (s->expr->tipo_no == EX_CHAMADA_IND) (void)LwChamadaInd(c, s->expr);
    else (void)LwChamada(c, s->expr);
}

/* ---- return ---- */

/* Embrulha o valor no resultado do proc: T -> ok, E -> erro, o proprio resultado passa direto. */
static Valor EmbrulharResultado(LowerCtx *c, const Stmt *s, Valor v, TipoId retorno)
{
    const TipoInfo info = *TipoDe(&c->sem->tipos, retorno);
    TipoId t = s->expr->tipo;
    IrOp op;
    if (t == info.b) op = IR_RES_ERRO;
    else if (t == info.a && t != TIPO_VAZIO) op = IR_RES_OK;
    else return v;
    IrInstr *i = LwNovaInstr(c, op, s->pos);
    i->destino = LwNovoTemp(c);
    i->tipo = retorno;
    i->a = v;
    return LwValorTemp(i->destino, retorno);
}

/* Valor do return ja no tipo de retorno do proc; 'return' sem valor em '! E' vira ok vazio. */
static Valor ValorDoReturn(LowerCtx *c, const Stmt *s)
{
    TipoId retorno = c->decl->retorno.tipo;
    bool resultado = TipoClasseDe(&c->sem->tipos, retorno) == TC_RESULTADO;
    if (s->expr == NULL) {
        if (!resultado) return LwValorNenhum();
        IrInstr *ok = LwNovaInstr(c, IR_RES_OK, s->pos);
        ok->destino = LwNovoTemp(c);
        ok->tipo = retorno;
        ok->a = LwValorNenhum();
        return LwValorTemp(ok->destino, retorno);
    }
    Valor v = LwExpr(c, s->expr);
    return resultado ? EmbrulharResultado(c, s, v, retorno) : v;
}

/* O defer pode mudar o local devolvido: com defer pendente, o valor sai para um local sintetico antes dele. */
static Valor ProtegerDoDefer(LowerCtx *c, Valor v, uint32_t pos)
{
    if (v.tipo_no != V_LOCAL || c->defers.n == 0) return v;
    IrInstr *i = LwNovaInstr(c, IR_LOCAL, pos);
    i->destino = LwNovoLocal(c);
    i->tipo = v.tipo;
    i->a = v;
    return LwValorLocal(i->destino, v.tipo);
}

static void LwReturn(LowerCtx *c, const Stmt *s)
{
    Valor v = ProtegerDoDefer(c, ValorDoReturn(c, s), s->pos);
    LwExpandirDefers(c, 0);
    IrInstr *i = LwNovaInstr(c, IR_RETORNA, s->pos);
    i->a = v;
}

/* ---- expect ---- */

/* Texto da condicao como escrito no fonte: de Expr.pos ate o ')' final (Stmt.pos_fim), sem espaco no fim. */
static int32_t TextoDaCondicao(LowerCtx *c, const Stmt *s)
{
    const Fonte *f = c->sem->fonte;
    uint32_t ini = s->expr->pos, fim = s->pos_fim;
    if (fim <= ini || fim > f->tam) LwFalhaStmt(c, s, "expect sem posicao do ')' final");
    while (fim > ini && (f->texto[fim - 1] == ' ' || f->texto[fim - 1] == '\t')) fim--;
    return Internar(c->sem->nomes, f->texto + ini, fim - ini);
}

static void LwExpect(LowerCtx *c, const Stmt *s)
{
    Valor condicao = LwExpr(c, s->expr);
    IrInstr *i = LwNovaInstr(c, IR_EXPECT, s->pos);
    i->a = condicao;
    i->nome = TextoDaCondicao(c, s);
}

/* ---- despacho e blocos ---- */

/* let/var a, b = f(): a tupla e calculada uma vez; cada nome (menos '_') le o seu elemento. */
static void LwStmtLetMultiplo(LowerCtx *c, const Stmt *s)
{
    Valor tupla = LwExpr(c, s->expr);
    for (uint32_t k = 0; k < s->nnomes; k++) {
        const NomeLet *nome = &s->nomes[k];
        if (nome->local_id < 0) continue;
        IrInstr *elemento = LwNovaInstr(c, IR_TUPLA_ELEMENTO, nome->pos);
        elemento->destino = LwNovoTemp(c);
        elemento->tipo = nome->tipo;
        elemento->a = tupla;
        elemento->b = LwValorInt(k, TIPO_U32);
        IrInstr *local = LwNovaInstr(c, IR_LOCAL, nome->pos);
        local->destino = nome->local_id;
        local->tipo = nome->tipo;
        local->a = LwValorTemp(elemento->destino, elemento->tipo);
        local->mutavel = s->op == 1;
    }
}

static const LowerComando COMANDOS[ST_LET_MULTIPLO + 1] = {
    [ST_LET_MULTIPLO] = LwStmtLetMultiplo,
    [ST_LET] = LwLetVar, [ST_VAR] = LwLetVar, [ST_ATRIBUICAO] = LwAtribuicao, [ST_IF] = LwStmtIf,
    [ST_WHILE] = LwStmtWhile, [ST_FOR] = LwStmtFor, [ST_RETURN] = LwReturn, [ST_CHAMADA] = LwChamadaComando,
    [ST_BREAK] = LwStmtBreak, [ST_CONTINUE] = LwStmtContinue, [ST_DEFER] = LwStmtDefer, [ST_SWITCH] = LwStmtSwitch,
    [ST_EXPECT] = LwExpect,
};

void LwStmt(LowerCtx *c, const Stmt *s)
{
    if (s->tipo_no > ST_LET_MULTIPLO || COMANDOS[s->tipo_no] == NULL) LwFalhaStmt(c, s, "comando desconhecido");
    COMANDOS[s->tipo_no](c, s);
}

static bool EhSaida(const Stmt *s)
{
    return s->tipo_no == ST_RETURN || s->tipo_no == ST_BREAK || s->tipo_no == ST_CONTINUE;
}

void LwBloco(LowerCtx *c, const Stmt *lista, bool e_laco)
{
    LwEscopo *escopo = VecEmpurrar(&c->escopos);
    escopo->inicio = (uint32_t)c->defers.n;
    escopo->e_laco = e_laco;
    uint32_t inicio = escopo->inicio;
    const Stmt *ultimo = NULL;
    for (const Stmt *s = lista; s != NULL; s = s->prox) {
        LwStmt(c, s);
        ultimo = s;
    }
    if (ultimo == NULL || !EhSaida(ultimo)) LwExpandirDefers(c, (uint32_t)c->escopos.n - 1);
    c->defers.n = inicio;
    c->escopos.n--;
}
