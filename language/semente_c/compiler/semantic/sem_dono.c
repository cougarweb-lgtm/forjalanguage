/* sem_dono.c - E0311 (dono nao se copia), contrato IA1 D-IA1-5/6, passo b08: a forma da ocorrencia (explicacao e forma_certa com os nomes do arquivo) e a sugestao de cada caso
 * que o b06 deixou sem: return de campo dono, item de append, 'for item' sobre listas, parametro [dynamic]T e campo dono com padrao.
 * Cada caso roda DEPOIS do SemErro do E0311 e escreve em s->diag (DiagForma, DiagSugerir/DiagEditar, coluna em BYTES).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-L4: sai o caso do resultado 'T ! E' com lista (agora o dono se move no return; DONO_RESULTADO sai de sem_interno.h).
 *            0.1.0 - IA1-b08: primeira versao.
 * Confianca: provavel = edicao que muda o tipo de algo que o corpo usa (o item do for vira ponteiro; o padrao some e o literal passa a pedir make); so_dica = descricao sem edicao
 *            (return, item de append e parametro mexem em mais de uma linha). Edicao que nao cabe rebaixa a sugestao a so_dica.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_forma_b08.py --forja compiler\forja.exe
 */
#include "semantic/sem_interno.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { TRECHO = 80, ONDE = 160, TEXTO_LOCAL = 1024 };

/* O que as frases precisam saber do caso. */
typedef struct {
    char onde[ONDE], linha[TRECHO], trecho[TRECHO];
    bool lista; /* o valor e [dynamic]T (so a lista tem fatia l[..]) */
} Cena;

static bool CharDeNome(char c)
{
    return isalnum((unsigned char)c) != 0 || c == '_';
}

static bool Espaco(char c)
{
    return c == ' ' || c == '\t';
}

static void MontarCena(const Semantica *s, uint32_t pos, TipoId tipo, Cena *c)
{
    DiagDescreverOnde(s->fonte, pos, c->onde, sizeof c->onde);
    SemLinhaDoFonte(s, pos, c->linha);
    SemTrechoDoFonte(s, pos, false, c->trecho);
    c->lista = TipoClasseDe(&s->tipos, tipo) == TC_DINAMICO;
}

/* Abre a sugestao do caso; o texto e cortado em DIAG_DESCRICAO_MAX - 1 bytes. */
static bool Abrir(Semantica *s, DiagConfianca confianca, const char *texto)
{
    return DiagSugerir(&s->diag->sug, confianca, "%s", texto);
}

/* A edicao [ini, fim) -> novo na sugestao aberta; se nao cabe, a sugestao vira so_dica (nunca meia correcao marcada como provavel). */
static void Editar(Semantica *s, uint32_t ini, uint32_t fim, const char *novo)
{
    DiagSugestao *aberta = &s->diag->sug.itens[s->diag->sug.n - 1];

    if (DiagEditar(&s->diag->sug, ini, fim, novo)) return;
    aberta->confianca = DIAG_SO_DICA;
    aberta->nedicoes = 0;
}

/* ---------- copia de dono (let/var, atribuicao, argumento, return, item de append) ---------- */

typedef enum { ONDE_QUALQUER, ONDE_RETURN, ONDE_ITEM } OndeDaCopia;

/* O que vem antes de pos na linha: a palavra 'return', o argumento nomeado 'item =' do append, ou outra coisa. */
static OndeDaCopia OndeDaCopiaEm(const char *t, uint32_t pos)
{
    uint32_t i = pos;

    while (i > 0 && Espaco(t[i - 1])) i--;
    if (i >= 6 && strncmp(t + i - 6, "return", 6) == 0 && (i == 6 || !CharDeNome(t[i - 7]))) return ONDE_RETURN;
    if (i == 0 || t[i - 1] != '=') return ONDE_QUALQUER;
    i--;
    while (i > 0 && Espaco(t[i - 1])) i--;
    return i >= 4 && strncmp(t + i - 4, "item", 4) == 0 && (i == 4 || !CharDeNome(t[i - 5])) ? ONDE_ITEM : ONDE_QUALQUER;
}

static void FormaDoReturn(Semantica *s, const Cena *c)
{
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL], dica[DIAG_DESCRICAO_MAX];

    snprintf(explicacao, sizeof explicacao,
             "Na %s o return da linha '%s' devolve '%s', que e dono: so um local se move para fora, e devolver um campo ou elemento copiaria o dono (duas donas da mesma memoria).",
             c->onde, c->linha, c->trecho);
    if (c->lista) {
        snprintf(forma, sizeof forma, "Devolva uma fatia emprestada: troque o retorno do proc por []T e escreva return %s[..] na linha '%s' (ou receba um ^[dynamic]T e preencha por ele).", c->trecho,
                 c->linha);
        snprintf(dica, sizeof dica, "devolva %.24s[..] com o retorno do proc como []T (empresta), ou receba ^[dynamic]T e preencha", c->trecho);
    } else {
        snprintf(forma, sizeof forma, "Receba um ponteiro para o dono (^T) e preencha ou leia por ele em vez de devolver '%s' na linha '%s'.", c->trecho, c->linha);
        snprintf(dica, sizeof dica, "receba ^T e escreva por ele em vez de devolver %.30s (um dono so se move de um local)", c->trecho);
    }
    DiagForma(s->diag, explicacao, forma);
    Abrir(s, DIAG_SO_DICA, dica);
}

static void FormaDoItem(Semantica *s, const Cena *c)
{
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL], dica[DIAG_DESCRICAO_MAX];

    snprintf(explicacao, sizeof explicacao, "Na %s o append da linha '%s' passa '%s' como item, mas '%s' ja e dono: copiar criaria duas donas da mesma memoria.",
             c->onde, c->linha, c->trecho, c->trecho);
    snprintf(forma, sizeof forma, "Passe um valor novo no item da linha '%s' (make(...)) ou guarde fatias: declare a lista de fora de []T e escreva item = %s[..].", c->linha, c->trecho);
    snprintf(dica, sizeof dica, "o item precisa ser valor novo ou fatia: guarde []T e passe %.30s[..]", c->trecho);
    DiagForma(s->diag, explicacao, forma);
    Abrir(s, DIAG_SO_DICA, dica);
}

static void FormaDaCopia(Semantica *s, const Cena *c)
{
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];

    snprintf(explicacao, sizeof explicacao,
             "Na %s a linha '%s' copia '%s', que e dono (lista, map, soa ou struct com lista): copiar criaria duas donas da mesma memoria.",
             c->onde, c->linha, c->trecho);
    if (c->lista) {
        snprintf(forma, sizeof forma, "Empreste em vez de copiar na linha '%s': escreva &%s (o nome novo vira ponteiro; use ^ para ler) ou %s[..] (fatia que le e altera os elementos).", c->linha,
                 c->trecho, c->trecho);
    } else {
        snprintf(forma, sizeof forma, "Empreste em vez de copiar na linha '%s': escreva &%s (o nome novo vira ponteiro; use ^ ou .campo para ler e alterar).", c->linha, c->trecho);
    }
    DiagForma(s->diag, explicacao, forma);
}

/* So a forma: a sugestao de emprestimo do let/var (SemSugerirEmprestimo) vem do comando, depois deste. */
static void CasoDaCopia(Semantica *s, uint32_t pos, TipoId tipo)
{
    Cena c;
    OndeDaCopia onde = OndeDaCopiaEm(s->fonte->texto, pos);

    MontarCena(s, pos, tipo, &c);
    if (onde == ONDE_RETURN) FormaDoReturn(s, &c);
    else if (onde == ONDE_ITEM && c.lista) FormaDoItem(s, &c);
    else FormaDaCopia(s, &c);
}

/* ---------- 'for item' sobre listas ---------- */

/* O nome do item que comeca em pos (depois de espacos): *ini..*fim; true se o 'in' vem logo depois dele (for item in, sem indice). */
static bool NomeDoItemSeguidoDeIn(const char *t, uint32_t pos, uint32_t *ini, uint32_t *fim)
{
    uint32_t i = pos;

    while (Espaco(t[i])) i++;
    *ini = i;
    while (CharDeNome(t[i])) i++;
    *fim = i;
    while (Espaco(t[i])) i++;
    return *fim > *ini && t[i] == 'i' && t[i + 1] == 'n' && !CharDeNome(t[i + 2]);
}

static void CasoDoFor(Semantica *s, uint32_t pos)
{
    Cena c;
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL], dica[DIAG_DESCRICAO_MAX], nome[TRECHO] = "";
    uint32_t ini, fim;
    bool simples = NomeDoItemSeguidoDeIn(s->fonte->texto, pos, &ini, &fim);

    MontarCena(s, pos, TIPO_VAZIO, &c);
    snprintf(explicacao, sizeof explicacao, "Na %s o for da linha '%s' copia cada elemento, e cada elemento e uma lista (dono): copiar criaria varias donas da mesma memoria.", c.onde, c.linha);
    if (simples && fim - ini < TRECHO) snprintf(nome, sizeof nome, "%.*s", (int)(fim - ini), s->fonte->texto + ini);
    if (nome[0] != '\0') {
        snprintf(forma, sizeof forma, "Escreva & antes do nome do item na linha '%s': for &%s in ...; o item vira ponteiro (use %s^ ou %s.campo no corpo).", c.linha, nome, nome, nome);
        snprintf(dica, sizeof dica, "escreva for &%s in ...: o item vira ponteiro (use %s^ no corpo)", nome, nome);
    } else {
        snprintf(forma, sizeof forma, "Empreste o item na linha '%s': escreva & antes do nome do item (a forma for &item in ...), que vira ponteiro.", c.linha);
        snprintf(dica, sizeof dica, "escreva & antes do nome do item no for: o item vira ponteiro");
    }
    DiagForma(s->diag, explicacao, forma);
    if (!Abrir(s, nome[0] != '\0' ? DIAG_PROVAVEL : DIAG_SO_DICA, dica) || nome[0] == '\0') return;
    Editar(s, ini, ini, "&");
}

/* ---------- parametro [dynamic]T ---------- */

static void CasoDoParametro(Semantica *s, uint32_t pos)
{
    Cena c;
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL], dica[DIAG_DESCRICAO_MAX], fatia[TRECHO];

    MontarCena(s, pos, TIPO_VAZIO, &c);
    snprintf(fatia, sizeof fatia, "[]%s", strncmp(c.trecho, "[dynamic]", 9) == 0 ? c.trecho + 9 : "T");
    snprintf(explicacao, sizeof explicacao, "Na %s o parametro da linha '%s' recebe '%s' por valor, o que copiaria a lista: parametro nao recebe lista por valor.", c.onde, c.linha, c.trecho);
    snprintf(forma, sizeof forma, "Troque '%s' por ^%s (o proc altera a lista; chame com &lista) ou por %s (o proc le e altera os elementos; chame com lista[..]).", c.trecho, c.trecho, fatia);
    snprintf(dica, sizeof dica, "troque o tipo por ^%.20s (altera a lista; chame f(&l)) ou %.20s (le; chame f(l[..]))", c.trecho, fatia);
    DiagForma(s->diag, explicacao, forma);
    Abrir(s, DIAG_SO_DICA, dica);
}

/* ---------- campo dono com padrao ---------- */

/* Onde o tipo do campo termina (recua de pos pulando espacos, o '=' e os espacos antes dele); 0 = nao ha '='. */
static uint32_t FimDoTipoDoCampo(const char *t, uint32_t pos)
{
    uint32_t i = pos;

    while (i > 0 && Espaco(t[i - 1])) i--;
    if (i == 0 || t[i - 1] != '=') return 0;
    i--;
    while (i > 0 && Espaco(t[i - 1])) i--;
    return i;
}

/* O fim do padrao que comeca em pos: o fim da linha sem comentario nem espacos; 0 se os parenteses, colchetes, chaves ou aspas nao fecham na linha. */
static uint32_t FimDoPadrao(const char *t, uint32_t pos)
{
    uint32_t fim = pos;
    int fundo = 0;
    bool texto = false;

    for (uint32_t i = pos; t[i] != '\0' && t[i] != '\n' && t[i] != '\r'; i++) {
        if (t[i] == '"') texto = !texto;
        if (!texto && t[i] == '/' && t[i + 1] == '/') break;
        if (!texto && strchr("([{", t[i]) != NULL) fundo++;
        if (!texto && strchr(")]}", t[i]) != NULL) fundo--;
        if (!Espaco(t[i])) fim = i + 1;
    }
    return fundo == 0 && !texto ? fim : 0;
}

static void CasoDoPadrao(Semantica *s, uint32_t pos)
{
    Cena c;
    char explicacao[TEXTO_LOCAL], forma[TEXTO_LOCAL];
    uint32_t ini = FimDoTipoDoCampo(s->fonte->texto, pos), fim = FimDoPadrao(s->fonte->texto, pos);
    bool cabe = ini > 0 && fim > pos;

    MontarCena(s, pos, TIPO_VAZIO, &c);
    snprintf(explicacao, sizeof explicacao, "Na %s o campo '%s' tem um padrao que aloca; um dono com padrao alocaria escondido a cada literal da struct.", c.onde, c.linha);
    snprintf(forma, sizeof forma, "Tire o padrao da linha '%s' (a lista nasce vazia, sem alocador) e escreva o campo = make(...) em cada literal que cria a struct.", c.linha);
    DiagForma(s->diag, explicacao, forma);
    if (!Abrir(s, cabe ? DIAG_PROVAVEL : DIAG_SO_DICA, "tire o padrao do campo (a lista nasce vazia) e passe make(...) no campo de cada literal da struct") || !cabe) return;
    Editar(s, ini, fim, "");
}

void SemFormaDoDono(Semantica *s, DonoCaso caso, uint32_t pos, TipoId tipo)
{
    if (!s->diag->tem || s->diag->codigo != E0311) return;
    switch (caso) {
    case DONO_COPIA: CasoDaCopia(s, pos, tipo); break;
    case DONO_FOR: CasoDoFor(s, pos); break;
    case DONO_PARAMETRO: CasoDoParametro(s, pos); break;
    case DONO_CAMPO_PADRAO: CasoDoPadrao(s, pos); break;
    }
}
