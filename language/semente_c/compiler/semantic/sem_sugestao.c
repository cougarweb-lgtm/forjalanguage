/* sem_sugestao.c - sugestoes aplicaveis por maquina do lote 2 (contrato IA1, D-IA1-5/6): E0102 (caixa), E0201 e E1003 (parecido unico; E1003 sem
 * qualificar), E0202 (alias de foreign), E0307 ("a" + "b"), E0310 (append sem '&'), E0311 (let b = a) e E0502 (nome real do parametro).
 * Cada funcao roda DEPOIS do SemErro do seu codigo e abre a sugestao em s->diag->sug (DiagSugerir/DiagEditar, formato do b04); coluna em BYTES.
 * Versao: 0.1.2
 * Changelog: 0.1.2 - Linguagem v1.13 L1a: ApelidoDoModulo procura o apelido no arquivo (trecho) em analise, nao no modulo.
 *            0.1.1 - IA1-L3b: textos das sugestoes do E0502 para a regra V-06 = B (posicional depois de nomeado; argumento que sobra).
 *            0.1.0 - IA1-b06: primeira versao.
 * Confianca: certa = correcao unica que nao muda o sentido; provavel = parecido unico, renomeacao que pede ajustar os usos, emprestimo no lugar de copia;
 *            so_dica = descricao sem edicao. Falha ao montar uma edicao (duas linhas, texto longo, edicoes cheias) rebaixa a sugestao a so_dica sem edicao.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_sugestoes_l2.py --forja compiler\forja.exe
 */
#include "semantic/sem_interno.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { PEDACO_MAX = 40 };

static bool DiagEh(const Semantica *s, DiagCodigo codigo)
{
    return s->diag->tem && s->diag->codigo == codigo;
}

static bool CharDeNome(char c)
{
    return isalnum((unsigned char)c) != 0 || c == '_';
}

/* O nome 'nome' (tam bytes) esta escrito em t[pos], inteiro (nao e prefixo de um nome maior). */
static bool NomeNaPos(const char *t, uint32_t pos, const char *nome, size_t tam)
{
    return strncmp(t + pos, nome, tam) == 0 && !CharDeNome(t[pos + tam]);
}

static bool Abrir(Semantica *s, DiagConfianca confianca, const char *texto)
{
    return DiagSugerir(&s->diag->sug, confianca, "%s", texto);
}

static void Rebaixar(Semantica *s)
{
    DiagSugestao *aberta = &s->diag->sug.itens[s->diag->sug.n - 1];
    aberta->confianca = DIAG_SO_DICA;
    aberta->nedicoes = 0;
}

/* Edicao [ini, fim) -> novo na ultima sugestao aberta; so numa linha. Falha rebaixa a sugestao (nunca fica meia correcao marcada como certa). */
static void Editar(Semantica *s, uint32_t ini, uint32_t fim, const char *novo)
{
    const DiagSugestao *aberta = &s->diag->sug.itens[s->diag->sug.n - 1];
    bool uma_linha = fim >= ini && memchr(s->fonte->texto + ini, '\n', fim - ini) == NULL;

    if (aberta->confianca == DIAG_SO_DICA) return;
    if (uma_linha && DiagEditar(&s->diag->sug, ini, fim, novo)) return;
    Rebaixar(s);
}

void SemSugerirSoDica(Semantica *s, DiagCodigo codigo, const char *texto)
{
    if (DiagEh(s, codigo)) Abrir(s, DIAG_SO_DICA, texto);
}

/* ---------- nomes: caixa (E0102), parecido unico (E0201, E1003), qualificar (E1003), alias de foreign (E0202) ---------- */

void SemSugerirTroca(Semantica *s, const TrocaDeNome *troca, DiagConfianca confianca, const char *nota)
{
    char texto[DIAG_DESCRICAO_MAX];

    snprintf(texto, sizeof texto, "troque '%.*s' por '%s'%s", (int)troca->tam_antigo, troca->antigo, troca->novo, nota);
    if (!Abrir(s, confianca, texto)) return;
    if (!NomeNaPos(s->fonte->texto, troca->pos, troca->antigo, troca->tam_antigo)) {
        Rebaixar(s);
        return;
    }
    Editar(s, troca->pos, troca->pos + (uint32_t)troca->tam_antigo, troca->novo);
}

void SemSugerirCaixa(Semantica *s, int32_t nome, uint32_t pos, const char *certo)
{
    TrocaDeNome troca = {pos, SemNomeTexto(s, nome), InternerTam(s->nomes, nome), certo};

    if (!DiagEh(s, E0102) || strlen(certo) >= DIAG_NOVO_MAX) return;
    SemSugerirTroca(s, &troca, DIAG_PROVAVEL, "; renomeie tambem os usos");
}

/* Um candidato so, ou o 1o estritamente mais perto que o 2o: o desempate nao e ambiguo. */
void SemSugerirParecido(Semantica *s, const SemParecido *sp)
{
    const Parecidos *busca = &sp->busca;
    char novo[DIAG_NOVO_MAX];
    TrocaDeNome troca = {sp->pos, busca->alvo, busca->tam_alvo, novo};

    if ((sp->codigo != E0201 && sp->codigo != E1003) || busca->qtd == 0 || !DiagEh(s, sp->codigo)) return;
    if (busca->qtd > 1 && busca->nomes[0].distancia == busca->nomes[1].distancia) return;
    if (busca->nomes[0].tam >= sizeof novo) return;
    snprintf(novo, sizeof novo, "%.*s", (int)busca->nomes[0].tam, busca->nomes[0].texto);
    SemSugerirTroca(s, &troca, DIAG_PROVAVEL, "");
}

/* Apelido com que o arquivo atual importou o modulo; NULL = este arquivo nao importa o modulo. */
static const char *ApelidoDoModulo(const Semantica *s, uint8_t modulo)
{
    const Vec *apelidos = &s->modulos->apelidos;

    for (size_t i = 0; i < apelidos->n; i++) {
        const ApelidoDeArquivo *a = VecEm(apelidos, i);
        if (a->arquivo == s->arquivo_atual && a->modulo == modulo) return SemNomeTexto(s, a->nome);
    }
    return NULL;
}

/* E1003 de nome do modulo 'dono' usado sem apelido: escreva apelido.nome (certa se o nome e pub; senao falta o pub la). */
void SemSugerirQualificar(Semantica *s, const NomeUsado *uso, const Decl *dono)
{
    const char *apelido = ApelidoDoModulo(s, dono->modulo);
    char novo[DIAG_NOVO_MAX], texto[DIAG_DESCRICAO_MAX];

    if (!DiagEh(s, E1003)) return;
    if (apelido == NULL || strlen(apelido) + 2 >= sizeof novo) {
        Abrir(s, DIAG_SO_DICA, "importe o modulo do nome e escreva apelido.nome");
        return;
    }
    snprintf(novo, sizeof novo, "%s.", apelido);
    snprintf(texto, sizeof texto, "escreva %s%s: o nome e do modulo importado como '%s'", novo, SemNomeTexto(s, uso->nome), apelido);
    if (Abrir(s, dono->publico ? DIAG_CERTA : DIAG_PROVAVEL, texto)) Editar(s, uso->pos, uso->pos, novo);
}

/* E0202 de proc foreign sem alias cujo nome a linguagem reserva (remove, get...): vira 'proc c_nome = "nome"(...)'; as chamadas passam a c_nome. */
void SemSugerirAliasDeForeign(Semantica *s, const Decl *d)
{
    const char *nome = SemNomeTexto(s, d->nome);
    uint32_t tam = InternerTam(s->nomes, d->nome);
    char alias[DIAG_NOVO_MAX], texto[DIAG_DESCRICAO_MAX];

    if (!DiagEh(s, E0202) || !d->externo || d->simbolo >= 0 || s->globais[d->nome].categoria != SIM_PROC_EMBUTIDO) return;
    if (tam + 4 >= sizeof alias || !NomeNaPos(s->fonte->texto, d->pos, nome, tam)) return;
    snprintf(alias, sizeof alias, " = \"%s\"", nome);
    snprintf(texto, sizeof texto, "declare com alias: proc c_%s = \"%s\"(...) e chame c_%s", nome, nome, nome);
    if (!Abrir(s, DIAG_PROVAVEL, texto)) return;
    Editar(s, d->pos, d->pos, "c_");
    Editar(s, d->pos + tam, d->pos + tam, alias);
}

/* E0202 do proc da Forja cujo nome e o simbolo C de um alias foreign: outro nome para o proc (as chamadas acompanham). */
void SemSugerirRenomearProc(Semantica *s, int32_t nome, uint32_t pos)
{
    char novo[DIAG_NOVO_MAX];
    const char *texto = SemNomeTexto(s, nome);
    TrocaDeNome troca = {pos, texto, InternerTam(s->nomes, nome), novo};

    if (!DiagEh(s, E0202) || troca.tam_antigo + 4 >= sizeof novo) return;
    snprintf(novo, sizeof novo, "%s_fj", texto);
    SemSugerirTroca(s, &troca, DIAG_PROVAVEL, "; renomeie tambem as chamadas");
}

/* ---------- chamada: E0502 ---------- */

static uint32_t PularEspacos(const char *t, uint32_t i)
{
    while (t[i] == ' ' || t[i] == '\t') i++;
    return i;
}

/* Fim (exclusivo) do texto entre aspas que abre em t[i]; devolve a posicao da aspa de fechamento (ou a do fim da linha). */
static uint32_t FimDoTexto(const char *t, uint32_t i)
{
    for (i++; t[i] != '\0' && t[i] != '\n' && t[i] != '"'; i++) {
        if (t[i] == '\\' && t[i + 1] != '\0') i++;
    }
    return i;
}

/* Fim do argumento que comeca em 'ini': a virgula ou o fecha-parenteses do nivel dele, sem os espacos do fim. 0 = nao achou na linha. */
static uint32_t FimDoArgumento(const char *t, uint32_t ini)
{
    int fundo = 0;

    for (uint32_t i = ini; t[i] != '\0' && t[i] != '\n'; i++) {
        switch (t[i]) {
        case '"': i = FimDoTexto(t, i); break;
        case '(': case '[': case '{': fundo++; break;
        case ')': case ']': case '}':
            if (fundo > 0) { fundo--; break; }
            while (i > ini && t[i - 1] == ' ') i--;
            return i;
        case ',':
            if (fundo > 0) break;
            while (i > ini && t[i - 1] == ' ') i--;
            return i;
        default: break;
        }
    }
    return 0;
}

/* Depois de 'nome', espacos, '=' e espacos a partir de 'ini'; 0 = o argumento nao esta escrito assim. */
static uint32_t DepoisDoNomeEIgual(const char *t, uint32_t ini)
{
    uint32_t i = ini;

    while (CharDeNome(t[i])) i++;
    i = PularEspacos(t, i);
    if (t[i] != '=' || t[i + 1] == '=') return 0;
    return PularEspacos(t, i + 1);
}

/* 1o argumento escrito 'nome = valor': tire "nome = ". certa se nome e o 1o parametro (param0); senao o valor passa a ligar ao 1o parametro (provavel). */
void SemSugerirTirarNome(Semantica *s, const Arg *g, int32_t param0)
{
    uint32_t fim = DepoisDoNomeEIgual(s->fonte->texto, g->pos_nome);
    char texto[DIAG_DESCRICAO_MAX];

    if (!DiagEh(s, E0502)) return;
    snprintf(texto, sizeof texto, "o 1o argumento e posicional: tire '%s = '", SemNomeTexto(s, g->nome));
    if (!Abrir(s, g->nome == param0 ? DIAG_CERTA : DIAG_PROVAVEL, texto)) return;
    if (fim == 0) Rebaixar(s);
    else Editar(s, g->pos_nome, fim, "");
}

/* Argumento posicional onde o proc quer 'nome = valor': escreva "nome = " antes do valor (o nome REAL do parametro, lido da assinatura). */
void SemSugerirNomearArg(Semantica *s, const Arg *g, const char *nome)
{
    char novo[DIAG_NOVO_MAX], texto[DIAG_DESCRICAO_MAX];

    if (!DiagEh(s, E0502) || strlen(nome) + 4 >= sizeof novo) return;
    snprintf(novo, sizeof novo, "%s = ", nome);
    snprintf(texto, sizeof texto, "escreva '%s' antes do valor: o argumento depois de um nomeado tambem e nomeado", novo);
    if (Abrir(s, DIAG_CERTA, texto)) Editar(s, g->valor->pos, g->valor->pos, novo);
}

/* Proc sem parametros recebeu um argumento: tire-o (provavel: o valor some, com o efeito que ele tivesse). So quando e o ultimo da chamada. */
void SemSugerirTirarArg(Semantica *s, const Arg *g)
{
    const char *t = s->fonte->texto;
    uint32_t fim = FimDoArgumento(t, g->valor->pos);

    if (!DiagEh(s, E0502) || !Abrir(s, DIAG_PROVAVEL, "o argumento sobra (o proc nao tem parametro para ele): tire-o")) return;
    if (fim == 0 || t[PularEspacos(t, fim)] != ')') Rebaixar(s);
    else Editar(s, g->valor->pos, fim, "");
}

/* ---------- memoria: E0310 (append sem '&'), E0311 (let b = a) ---------- */

/* append(l, ...): insira '&'. certa se a lista e um local var (ou chega por ponteiro/fatia); senao falta o 'var' (provavel). */
void SemSugerirEnderecoDoAppend(Semantica *s, const Expr *lista)
{
    const Expr *raiz = SemRaizDaCadeia(s, lista);
    const LocalInfo *local = raiz != NULL && raiz->tipo_no == EX_NOME ? EscopoBuscar(&s->escopo, raiz->nome) : NULL;
    bool mutavel = raiz == NULL || (local != NULL && local->natureza == LOC_VAR);

    if (!DiagEh(s, E0310) || (lista->tipo_no != EX_NOME && lista->tipo_no != EX_CAMPO)) return;
    if (Abrir(s, mutavel ? DIAG_CERTA : DIAG_PROVAVEL, "append recebe o endereco da colecao: escreva &lista (a lista precisa ser var)")) {
        Editar(s, lista->pos, lista->pos, "&");
    }
}

/* Fim da cadeia 'nome' ou 'a.b.c' que comeca em i. */
static uint32_t FimDaCadeia(const char *t, uint32_t i)
{
    while (CharDeNome(t[i]) || (t[i] == '.' && CharDeNome(t[i + 1]))) i++;
    return i;
}

/* let/var b = a com 'a' dono: empreste. Principal &a (b passa a ser ponteiro); lista tambem ganha a[..] (fatia). Sempre provavel: o tipo de b muda. */
void SemSugerirEmprestimo(Semantica *s, const Expr *e)
{
    const char *t = s->fonte->texto;
    uint32_t fim = FimDaCadeia(t, e->pos);

    if (!DiagEh(s, E0311) || (e->tipo_no != EX_NOME && e->tipo_no != EX_CAMPO)) return;
    if (fim == e->pos || t[fim] == '(' || t[fim] == '[') return;
    if (Abrir(s, DIAG_PROVAVEL, "empreste em vez de copiar: &x (b vira ponteiro; use b^ ou b.campo)")) Editar(s, e->pos, e->pos, "&");
    if (TipoClasseDe(&s->tipos, e->tipo) != TC_DINAMICO) return;
    if (Abrir(s, DIAG_PROVAVEL, "ou empreste uma fatia: x[..] (b vira []T, le e altera os elementos)")) Editar(s, fim, fim, "[..]");
}

/* ---------- texto: E0307 em "a" + "b" ---------- */

/* Pedaco do f"...": {nome} para str local; o proprio texto do literal (sem '{', '}', '\' e aspas dentro). false = forma que o f"" nao cobre aqui. */
static bool PedacoDoFTexto(const Semantica *s, const Expr *x, char *saida, uint32_t *fim)
{
    const char *t = s->fonte->texto;

    if (x->tipo_no == EX_NOME && x->modulo == 0 && x->tipo == TIPO_TEXTO) {
        *fim = x->pos_op + InternerTam(s->nomes, x->nome);
        return snprintf(saida, PEDACO_MAX, "{%s}", SemNomeTexto(s, x->nome)) < PEDACO_MAX;
    }
    if (x->tipo_no != EX_TEXTO || t[x->pos] != '"') return false;
    uint32_t aspa = FimDoTexto(t, x->pos);
    uint32_t tam = aspa - x->pos - 1;
    const char *ruim = strpbrk(t + x->pos + 1, "{}\\");
    if (t[aspa] != '"' || tam >= PEDACO_MAX || (ruim != NULL && ruim < t + aspa)) return false;
    memcpy(saida, t + x->pos + 1, tam);
    saida[tam] = '\0';
    *fim = aspa + 1;
    return true;
}

/* A soma e o unico argumento de um print( ... ): so ai o f"" substitui a conta (f"" vale so em print, format e format_c). */
static bool SomaEhOArgumentoDoPrint(const char *t, uint32_t ini, uint32_t fim)
{
    uint32_t i = ini;

    while (i > 0 && t[i - 1] == ' ') i--;
    return i >= 6 && strncmp(t + i - 6, "print(", 6) == 0 && (i == 6 || !CharDeNome(t[i - 7])) && t[PularEspacos(t, fim)] == ')';
}

void SemSugerirConcatenar(Semantica *s, const Expr *soma)
{
    char a[PEDACO_MAX], b[PEDACO_MAX], novo[DIAG_NOVO_MAX];
    uint32_t fim_a = 0, fim_b = 0;
    bool cobre = PedacoDoFTexto(s, soma->a, a, &fim_a) && PedacoDoFTexto(s, soma->b, b, &fim_b);

    if (!DiagEh(s, E0307) || soma->op != OP_SOMA) return;
    if (cobre && SomaEhOArgumentoDoPrint(s->fonte->texto, soma->a->pos, fim_b) && snprintf(novo, sizeof novo, "f\"%s%s\"", a, b) < (int)sizeof novo) {
        if (Abrir(s, DIAG_CERTA, "em print, junte os textos num f\"...\": f\"{a}{b}\"")) Editar(s, soma->a->pos, fim_b, novo);
        return;
    }
    Abrir(s, DIAG_SO_DICA, "str nao tem '+': use f\"{a}{b}\" dentro de print, ou format(f\"{a}{b}\", alloc = heap) para guardar o texto (pede delete)");
}
