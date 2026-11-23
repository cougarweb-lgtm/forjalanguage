/* sem_forma.c - forma por ocorrencia do E0301 (contrato IA1, D-IA1-5, passo b07): a explicacao e a forma_certa dizem o CONTEXTO da incompatibilidade (declaracao,
 * atribuicao, argumento, return, campo, conta, print, indice, condicao), com os dois tipos e os nomes da variavel, do proc ou do campo do arquivo; quando ha conversao
 * obvia, a forma_certa traz a linha ja corrigida ('as i32'). O catalogo guarda so o texto geral do codigo.
 * Cada funcao roda DEPOIS do SemErro do E0301 e escreve em s->diag com DiagForma; o E0301 que nenhuma delas cobre sai por SemFormaDaRegra (chamada pelo proprio SemErro),
 * que usa a causa como regra e o trecho do fonte onde o erro caiu.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - IA1-L5a: SemFormaDoPrint nao tem mais o caso do real (print de f32/f64 vale); a frase cita real.
 *            0.2.0 - IA1-b08: SemTrechoDoFonte e SemLinhaDoFonte exportados (o sem_dono.c usa); SemFormaContaDeTexto (E0306 de texto em conta: tipos reais e o caminho do f"...").
 *            0.1.0 - IA1-b07: primeira versao.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_forma_b07.py --forja compiler\forja.exe && python ..\tests\ia_first\verificar_forma_b08.py --forja compiler\forja.exe
 */
#include "semantic/sem_interno.h"

#include <stdio.h>
#include <string.h>

/* TEXTO_LOCAL: o buffer onde a frase e montada inteira (cabe o pior caso de campos cheios); DiagForma corta em DIAG_FORMA_MAX - 1. */
enum { TRECHO_MAX = 80, TIPO_TEXTO_MAX = 96, ONDE_MAX = 160, NOME_MAX = 64, PEDIDO_MAX = 160, DENTRO_MAX = NOME_MAX + 32, TEXTO_LOCAL = 1024, LINHA_MAX = 160 };

/* O que as frases precisam saber da ocorrencia. 'quem' = quem pede o tipo ("a declaracao de 'x'"). */
typedef struct {
    char onde[ONDE_MAX], quem[PEDIDO_MAX], esperado[TIPO_TEXTO_MAX], recebido[TIPO_TEXTO_MAX], trecho[TRECHO_MAX], dentro[DENTRO_MAX];
    ContextoDoTipo ctx;
} Cena;

/* ---------- trecho do fonte ---------- */

typedef struct { int fundo; bool texto, ate_espaco; } Varredura;

/* O caractere c (depois de 'antes', antes de 'depois') termina o trecho? Fecha-parenteses, virgula, '//' e '{' de bloco terminam no nivel 0; com ate_espaco, o espaco tambem. */
static bool TerminaTrecho(const Varredura *v, char antes, char c, char depois)
{
    if (v->texto) return false;
    if (c == '/' && depois == '/') return true;
    if (v->fundo > 0) return false;
    if (c == ',' || c == ')' || c == ']' || c == '}') return true;
    if (v->ate_espaco && (c == ' ' || c == '\t')) return true;
    return c == '{' && (antes == ' ' || antes == '\t');
}

static void AvancarVarredura(Varredura *v, char c)
{
    if (c == '"') v->texto = !v->texto;
    else if (v->texto) return;
    else if (c == '(' || c == '[' || c == '{') v->fundo++;
    else if (c == ')' || c == ']' || c == '}') v->fundo--;
}

/* O texto que comeca em pos: ate o fim do argumento (virgula, fecha-parenteses, '//', '{' de bloco ou fim de linha) ou, com ate_espaco, ate o 1o espaco fora de parenteses. */
void SemTrechoDoFonte(const Semantica *s, uint32_t pos, bool ate_espaco, char *saida)
{
    const char *t = s->fonte->texto;
    Varredura v = {0, false, ate_espaco};
    uint32_t n = 0;

    for (uint32_t i = pos; t[i] != '\0' && t[i] != '\n' && t[i] != '\r' && n < TRECHO_MAX - 1; i++) {
        if (TerminaTrecho(&v, i > pos ? t[i - 1] : ' ', t[i], t[i + 1])) break;
        AvancarVarredura(&v, t[i]);
        saida[n++] = t[i];
    }
    while (n > 0 && (saida[n - 1] == ' ' || saida[n - 1] == '\t')) n--;
    saida[n] = '\0';
}

/* A linha do fonte com [c->ini, c->fim) trocado por c->novo, sem o recuo; false se o trecho passa de uma linha ou a linha nao cabe em 'cap'. */
static bool LinhaCorrigida(const Semantica *s, const Conversao *c, char *saida, size_t cap)
{
    const char *t = s->fonte->texto;
    uint32_t ini = c->ini, fim = c->fim, inicio = ini, final = fim;
    int n;

    if (memchr(t + ini, '\n', fim - ini) != NULL) return false;
    while (inicio > 0 && t[inicio - 1] != '\n') inicio--;
    while (inicio < ini && (t[inicio] == ' ' || t[inicio] == '\t')) inicio++;
    while (t[final] != '\0' && t[final] != '\n' && t[final] != '\r') final++;
    n = snprintf(saida, cap, "%.*s%s%.*s", (int)(ini - inicio), t + inicio, c->novo, (int)(final - fim), t + fim);
    return n > 0 && (size_t)n < cap;
}

/* ---------- cena ---------- */

static const char *NomeOuInterrogacao(const Semantica *s, int32_t nome)
{
    return nome >= 0 ? SemNomeTexto(s, nome) : "?";
}

/* " (dentro do proc 'p')" quando o erro cai dentro do corpo do proc em analise; senao vazio. */
static void DentroDoProc(const Semantica *s, uint32_t pos, char *saida)
{
    const Decl *proc = s->proc;

    saida[0] = '\0';
    if (proc == NULL || proc->tipo_no != D_PROC || proc->nome < 0 || pos < proc->pos || pos > proc->pos_fim) return;
    snprintf(saida, DENTRO_MAX, " (dentro do proc '%.*s')", NOME_MAX - 1, SemNomeTexto(s, proc->nome));
}

/* Quem pede o tipo, pelo contexto da coercao ("a declaracao de 'x'"); sem contexto, "esta expressao". */
static void QuemPede(const Semantica *s, const ContextoDoTipo *c, char *saida)
{
    const char *nome = NomeOuInterrogacao(s, c->nome), *dono = NomeOuInterrogacao(s, c->dono);

    switch (c->onde) {
    case CTX_DECLARACAO: snprintf(saida, PEDIDO_MAX, "a declaracao de '%s'", nome); break;
    case CTX_ATRIBUICAO: snprintf(saida, PEDIDO_MAX, c->nome >= 0 ? "a atribuicao a '%s'" : "a atribuicao", nome); break;
    case CTX_ARGUMENTO: snprintf(saida, PEDIDO_MAX, "o argumento '%s' da chamada de '%s'", nome, dono); break;
    case CTX_RETORNO: snprintf(saida, PEDIDO_MAX, "o return do proc '%s'", dono); break;
    case CTX_CAMPO: snprintf(saida, PEDIDO_MAX, "o campo '%s' do literal '%s'", nome, dono); break;
    default: snprintf(saida, PEDIDO_MAX, "esta expressao"); break;
    }
}

static void MontarCena(const Semantica *s, const Expr *valor, TipoId esperado, Cena *c)
{
    memset(c, 0, sizeof *c);
    if (s->ctx_tipo.valor == valor) c->ctx = s->ctx_tipo; /* contexto de outra expressao nao vale */
    DiagDescreverOnde(s->fonte, valor->pos, c->onde, sizeof c->onde);
    QuemPede(s, &c->ctx, c->quem);
    SemDescrever(s, esperado, c->esperado, sizeof c->esperado);
    SemDescrever(s, valor->tipo, c->recebido, sizeof c->recebido);
    SemTrechoDoFonte(s, valor->pos, false, c->trecho);
    if (c->ctx.onde != CTX_RETORNO) DentroDoProc(s, valor->pos, c->dentro); /* o return ja diz o proc */
}

/* ---------- frases ---------- */

static const char *PorQueNaoConverte(const Semantica *s, TipoId esperado, TipoId recebido)
{
    bool numeros = TipoEhNumerico(&s->tipos, esperado) && TipoEhNumerico(&s->tipos, recebido);

    if (numeros && TipoEhInteiro(esperado) != TipoEhInteiro(recebido)) return "A Forja nao converte inteiro em real nem real em inteiro sozinha.";
    if (numeros) return "A Forja nao mistura numeros de tipos diferentes sem 'as'.";
    if (esperado == TIPO_BOOL || recebido == TIPO_BOOL) return "bool nao vira numero e numero nao vira bool: compare com ==, != ou <.";
    return "Os dois tipos sao de categorias diferentes e nao ha conversao automatica.";
}

static const char *EfeitoDoAs(const Semantica *s, TipoId esperado, TipoId recebido)
{
    if (TipoEhInteiro(esperado) && TipoEhReal(&s->tipos, recebido)) return "o 'as' trunca a parte fracionaria";
    if (TipoEhReal(&s->tipos, esperado) && TipoEhInteiro(recebido)) return "o 'as' pode perder precisao em inteiro muito grande";
    return "confira se o valor cabe no tipo novo";
}

/* "Troque a linha por: <linha com o 'as'> (<efeito>)." ; false = sem conversao obvia ou a linha nao cabe. */
static bool FormaComConversao(const Semantica *s, const Expr *valor, TipoId esperado, char *forma)
{
    Conversao c;
    char linha[LINHA_MAX];

    if (!SemConversaoObvia(s, valor, esperado, &c) || !LinhaCorrigida(s, &c, linha, sizeof linha)) return false;
    snprintf(forma, TEXTO_LOCAL, "Troque a linha por: %s (%s).", linha, EfeitoDoAs(s, esperado, valor->tipo));
    return true;
}

/* Sem conversao: o que escrever no lugar, conforme quem pede o tipo. */
static void FormaSemConversao(const Semantica *s, const Cena *c, char *forma)
{
    const char *nome = NomeOuInterrogacao(s, c->ctx.nome), *dono = NomeOuInterrogacao(s, c->ctx.dono);

    switch (c->ctx.onde) {
    case CTX_DECLARACAO:
        snprintf(forma, TEXTO_LOCAL, "Escreva em '%s' um valor %s, ou troque o tipo declarado de '%s' para %s.", nome, c->esperado, nome, c->recebido);
        break;
    case CTX_ATRIBUICAO:
        snprintf(forma, TEXTO_LOCAL, "Atribua%s um valor %s, ou declare a variavel com o tipo %s.", c->ctx.nome >= 0 ? " a variavel" : "", c->esperado, c->recebido);
        break;
    case CTX_ARGUMENTO:
        snprintf(forma, TEXTO_LOCAL, "Passe em '%s' um valor %s (a chamada de '%s' recebeu %s).", nome, c->esperado, dono, c->recebido);
        break;
    case CTX_RETORNO:
        snprintf(forma, TEXTO_LOCAL, "Devolva um valor %s em '%s', ou troque o tipo de retorno do proc para %s.", c->esperado, dono, c->recebido);
        break;
    case CTX_CAMPO:
        snprintf(forma, TEXTO_LOCAL, "Escreva no campo '%s' um valor %s no lugar de '%s'.", nome, c->esperado, c->trecho);
        break;
    default:
        snprintf(forma, TEXTO_LOCAL, "Use um valor %s no lugar de '%s' (%s).", c->esperado, c->trecho, c->recebido);
        break;
    }
}

void SemFormaTipoDiferente(Semantica *s, const Expr *valor, TipoId esperado)
{
    Cena c;
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];

    MontarCena(s, valor, esperado, &c);
    snprintf(explicacao, sizeof explicacao, "Na %s, %s%s pede %s mas o valor '%s' e %s. %s", c.onde, c.quem, c.dentro, c.esperado, c.trecho, c.recebido,
             PorQueNaoConverte(s, esperado, valor->tipo));
    if (!FormaComConversao(s, valor, esperado, forma)) FormaSemConversao(s, &c, forma);
    DiagForma(s->diag, explicacao, forma);
}

/* ---------- papel fixo: indice, limite, condicao ---------- */

void SemFormaDoValor(Semantica *s, const Expr *valor, TipoId esperado, const char *papel)
{
    Cena c;
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];
    const char *pede;

    MontarCena(s, valor, esperado, &c);
    pede = TipoEhInteiro(esperado) ? "inteiro" : c.esperado;
    snprintf(explicacao, sizeof explicacao, "Na %s, %s precisa ser %s, mas '%s' e %s%s. %s", c.onde, papel, pede, c.trecho, c.recebido, c.dentro,
             PorQueNaoConverte(s, esperado, valor->tipo));
    if (!FormaComConversao(s, valor, esperado, forma)) {
        if (esperado == TIPO_BOOL) snprintf(forma, sizeof forma, "Escreva uma comparacao (==, !=, <, >) ou um valor bool no lugar de '%s' (%s).", c.trecho, c.recebido);
        else snprintf(forma, sizeof forma, "Use um valor %s no lugar de '%s' (%s).", pede, c.trecho, c.recebido);
    }
    DiagForma(s->diag, explicacao, forma);
}

bool SemErroDoInteiro(Semantica *s, const Expr *valor, const char *papel)
{
    SemErro(s, E0301, valor->pos, "%s precisa ser inteiro", papel);
    SemFormaDoValor(s, valor, TIPO_INT, papel);
    return false;
}

/* ---------- conta com dois lados ---------- */

/* O operador escrito em pos (nome como 'and' ou simbolo de ate 3 caracteres). */
static void TextoDoOperador(const Semantica *s, uint32_t pos, char *saida)
{
    const char *t = s->fonte->texto + pos;
    size_t n = 0;
    bool palavra = t[0] >= 'a' && t[0] <= 'z';

    for (; n < 8; n++) {
        bool letra = t[n] >= 'a' && t[n] <= 'z';
        bool simbolo = n < 3 && t[n] != '\0' && strchr("+-*/%<>=!&|^~", t[n]) != NULL;
        if (!(palavra ? letra : simbolo)) break;
        saida[n] = t[n];
    }
    saida[n] = '\0';
}

void SemFormaOperandos(Semantica *s, const Expr *e)
{
    char onde[ONDE_MAX], op[12], esq[TRECHO_MAX], dir[TRECHO_MAX], ta[TIPO_TEXTO_MAX], tb[TIPO_TEXTO_MAX], linha[LINHA_MAX];
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];
    Conversao c;

    DiagDescreverOnde(s->fonte, e->pos_op, onde, sizeof onde);
    TextoDoOperador(s, e->pos_op, op);
    SemTrechoDoFonte(s, e->a->pos, true, esq);
    SemTrechoDoFonte(s, e->b->pos, true, dir);
    SemDescrever(s, e->a->tipo, ta, sizeof ta);
    SemDescrever(s, e->b->tipo, tb, sizeof tb);
    snprintf(explicacao, sizeof explicacao, "Na %s a conta '%s %s %s' junta %s com %s, e o operador '%s' pede os dois lados do mesmo tipo. %s", onde, esq, op, dir, ta, tb, op,
             PorQueNaoConverte(s, e->a->tipo, e->b->tipo));
    if (SemConversaoObvia(s, e->b, e->a->tipo, &c) && LinhaCorrigida(s, &c, linha, sizeof linha)) {
        snprintf(forma, sizeof forma, "Converta o lado direito para %s: troque a linha por: %s (ou converta '%s' para %s).", ta, linha, esq, tb);
    } else {
        snprintf(forma, sizeof forma, "Deixe '%s' e '%s' com o mesmo tipo (%s ou %s) antes de usar '%s'.", esq, dir, ta, tb, op);
    }
    DiagForma(s->diag, explicacao, forma);
}

/* ---------- print ---------- */

void SemFormaDoPrint(Semantica *s, const Expr *valor)
{
    char onde[ONDE_MAX], trecho[TRECHO_MAX], tipo[TIPO_TEXTO_MAX], explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];

    DiagDescreverOnde(s->fonte, valor->pos, onde, sizeof onde);
    SemTrechoDoFonte(s, valor->pos, false, trecho);
    SemDescrever(s, valor->tipo, tipo, sizeof tipo);
    snprintf(explicacao, sizeof explicacao, "Na %s o print recebeu '%s', que e %s; print so imprime inteiro (i8..u64, int, uint), real, bool, str ou cstr.", onde, trecho, tipo);
    if (TipoClasseDe(&s->tipos, valor->tipo) == TC_RESULTADO)
        snprintf(forma, sizeof forma, "Tire o valor do resultado antes de imprimir: escreva 'or_else' e um valor padrao depois de '%s' (ou use 'try' num proc que devolve erro).", trecho);
    else snprintf(forma, sizeof forma, "Imprima um inteiro, bool, str ou cstr: troque '%s' (%s) por um desses ou por um campo dele.", trecho, tipo);
    DiagForma(s->diag, explicacao, forma);
}

/* ---------- E0301 sem forma propria ---------- */

/* A linha do fonte que contem pos, sem o recuo e sem comentario de fim de linha (no maximo TRECHO_MAX - 1 bytes). */
void SemLinhaDoFonte(const Semantica *s, uint32_t pos, char *saida)
{
    const char *t = s->fonte->texto;
    uint32_t i = pos, n = 0;

    while (i > 0 && t[i - 1] != '\n') i--;
    while (t[i] == ' ' || t[i] == '\t') i++;
    for (; t[i] != '\0' && t[i] != '\n' && t[i] != '\r' && n < TRECHO_MAX - 1; i++) {
        if (t[i] == '/' && t[i + 1] == '/') break;
        saida[n++] = t[i];
    }
    while (n > 0 && (saida[n - 1] == ' ' || saida[n - 1] == '\t')) n--;
    saida[n] = '\0';
}

void SemFormaDaRegra(Semantica *s)
{
    char onde[ONDE_MAX], linha[TRECHO_MAX], explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];
    const char *causa = s->diag->causa;
    const char *regra = strstr(causa, ": ");

    DiagDescreverOnde(s->fonte, s->diag->off, onde, sizeof onde);
    SemLinhaDoFonte(s, s->diag->off, linha);
    snprintf(explicacao, sizeof explicacao, "Na %s a Forja recusou a linha '%s' porque a regra deste ponto e: %.100s.", onde, linha, causa);
    if (regra != NULL) snprintf(forma, sizeof forma, "Ajuste a linha '%s' conforme a regra: %.120s.", linha, regra + 2);
    else snprintf(forma, sizeof forma, "Reescreva a linha '%s' de modo que obedeca a regra: %.120s.", linha, causa);
    DiagForma(s->diag, explicacao, forma);
}

/* ---------- conta com texto (E0306 enganoso: "a" + 1, c + 1) ---------- */

static bool EhTextoNaConta(const Expr *x)
{
    return x->tipo_no == EX_TEXTO || x->tipo == TIPO_TEXTO || x->tipo == TIPO_CSTR;
}

/* O pedaco de f"...": o texto do literal (sem '{', '}', '\' nem aspas dentro) ou {trecho}. */
static void PedacoDoFTexto(const Expr *x, const char *trecho, char *saida, size_t cap)
{
    size_t n = strlen(trecho);
    bool literal = x->tipo_no == EX_TEXTO && n >= 2 && trecho[0] == '"' && trecho[n - 1] == '"';

    for (size_t i = 1; literal && i + 1 < n; i++) literal = strchr("{}\\\"", trecho[i]) == NULL;
    if (literal) snprintf(saida, cap, "%.*s", (int)(n - 2), trecho + 1);
    else snprintf(saida, cap, "{%s}", trecho);
}

void SemFormaContaDeTexto(Semantica *s, const Expr *e)
{
    char onde[ONDE_MAX], op[12], esq[TRECHO_MAX], dir[TRECHO_MAX], ta[TIPO_TEXTO_MAX], tb[TIPO_TEXTO_MAX], pa[TRECHO_MAX + 2], pb[TRECHO_MAX + 2];
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];

    if (!EhTextoNaConta(e->a) && !EhTextoNaConta(e->b)) return; /* ponteiro de verdade: vale a forma da tabela */
    DiagDescreverOnde(s->fonte, e->pos_op, onde, sizeof onde);
    TextoDoOperador(s, e->pos_op, op);
    SemTrechoDoFonte(s, e->a->pos, true, esq);
    SemTrechoDoFonte(s, e->b->pos, true, dir);
    SemDescrever(s, e->a->tipo, ta, sizeof ta);
    SemDescrever(s, e->b->tipo, tb, sizeof tb);
    snprintf(explicacao, sizeof explicacao, "Na %s a conta '%s %s %s' junta %s com %s: texto (str, cstr ou literal) nao faz conta, e a Forja nao converte o numero em texto sozinha.", onde, esq,
             op, dir, ta, tb);
    PedacoDoFTexto(e->a, esq, pa, sizeof pa);
    PedacoDoFTexto(e->b, dir, pb, sizeof pb);
    if (e->op == OP_SOMA) {
        snprintf(forma, sizeof forma, "Troque a conta por f\"...\": print(f\"%s%s\") em print, ou format(f\"%s%s\", alloc = heap) para guardar o texto (e delete depois).", pa, pb, pa, pb);
    } else {
        snprintf(forma, sizeof forma, "Texto nao tem o operador '%s': monte o texto com f\"...\" ou converta o outro lado antes da conta com '%s'.", op, op);
    }
    DiagForma(s->diag, explicacao, forma);
}
