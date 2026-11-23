/* sem_idioma.c - idiomas de outras linguagens que so a analise semantica enxerga (contrato IA1, 3.4): o nome, o tipo, o proc ou o campo que a Forja nao tem
 * e que outra linguagem tem (nil/null/None, True, range, string, console.log/printf, x.len(), x.push(v)). Em vez do E0201 ou E0308 generico, relata o E0110
 * ("forma de outra linguagem; na Forja e X") com a sugestao aplicavel. A Forja NAO aceita nenhum deles: so o diagnostico muda.
 * Textos e ids na tabela de common/idiomas.c; cada SemIdioma* e chamado so no caminho de erro, ANTES do SemErro do E0201/E0308, e devolve true se relatou.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - IA1-L2 (conferencia do main): comentarios LIMITE: em LerArgumentos (cognitiva 16) e nas 4 funcoes de 5 parametros; codigo igual.
 *            0.1.0 - IA1-L2: primeira versao (SemIdiomaDeNome, SemIdiomaDeChamada, SemIdiomaDeTipo, SemIdiomaDeCampo).
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_m_d4.py --forja compiler\forja.exe
 */
#include "semantic/sem_interno.h"

#include "common/idiomas.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { NOME_ACHADO_MAX = 48, LIMITE_DE_FAIXA_MAX = 14 };

/* Nome que outra linguagem usa no lugar de uma palavra da Forja: o 'certo' troca o nome (nome de variavel ou de tipo nunca e prefixo de outro: casa inteiro). */
typedef struct { const char *nome; IdiomaId id; const char *certo; } NomeAlheio;

static const NomeAlheio NOMES_ALHEIOS[] = {
    {"nil", IDI_NULO, "none"},          {"null", IDI_NULO, "none"},   {"NULL", IDI_NULO, "none"},    {"nullptr", IDI_NULO, "none"},
    {"None", IDI_NULO, "none"},         {"undefined", IDI_NULO, "none"}, {"True", IDI_BOOL_ALHEIO, "true"}, {"TRUE", IDI_BOOL_ALHEIO, "true"},
    {"False", IDI_BOOL_ALHEIO, "false"}, {"FALSE", IDI_BOOL_ALHEIO, "false"},
};

static const NomeAlheio TIPOS_ALHEIOS[] = {{"string", IDI_TIPO_TEXTO, "str"}, {"String", IDI_TIPO_TEXTO, "str"}};

/* objeto.metodo( que imprime em outra linguagem; 'sozinha': uma linha e um argumento, como o print da Forja, entao a troca e mecanica. */
typedef struct { const char *objeto, *metodo; bool sozinha; } ImprimeAlheio;

static const ImprimeAlheio IMPRIMES[] = {
    {"console", "log", true}, {"fmt", "Println", true}, {"fmt", "Print", false}, {"fmt", "Printf", false}, {"Console", "WriteLine", true},
};

/* proc solto que imprime em outra linguagem (so quando o proc nao foi declarado). */
static const ImprimeAlheio IMPRIMES_SOLTOS[] = {{"println", "", true}, {"puts", "", true}, {"printf", "", false}};

static const char *const TAMANHOS[] = {"len", "length", "size", "count", NULL};
static const char *const ACRESCENTAS[] = {"append", "push", "push_back", NULL};

static bool CharDeNome(char c)
{
    return isalnum((unsigned char)c) != 0 || c == '_';
}

static uint32_t PularEspacos(const char *t, uint32_t i)
{
    while (t[i] == ' ' || t[i] == '\t') i++;
    return i;
}

/* A aspa de fechamento do texto que abre em t[i] (ou o fim da linha). */
static uint32_t FimDoTexto(const char *t, uint32_t i)
{
    for (i++; t[i] != '\0' && t[i] != '\n' && t[i] != '"'; i++) {
        if (t[i] == '\\' && t[i + 1] != '\0') i++;
    }
    return i;
}

/* Os argumentos de uma chamada: 'fecha' = o ')' que casa com o '(' (0 = nao fecha na linha); 'virgula1' e 'virgula2' = as duas primeiras virgulas do nivel dele (0 = nao tem). */
typedef struct { uint32_t fecha, virgula1, virgula2; } Argumentos;

/* LIMITE: cognitiva 16 (varredura de um caractere por volta com 5 ramos excludentes: texto, abre, fecha-do-nivel, fecha-interno e virgula; quebrar espalharia o estado fundo/virgulas). */
static Argumentos LerArgumentos(const char *t, uint32_t abre)
{
    Argumentos a = {0, 0, 0};
    int fundo = 0;

    for (uint32_t i = abre + 1; t[i] != '\0' && t[i] != '\n'; i++) {
        if (t[i] == '"') {
            i = FimDoTexto(t, i);
        } else if (t[i] == '(' || t[i] == '[' || t[i] == '{') {
            fundo++;
        } else if (t[i] == ')' && fundo == 0) {
            a.fecha = i;
            return a;
        } else if (t[i] == ')' || t[i] == ']' || t[i] == '}') {
            fundo--;
        } else if (t[i] == ',' && fundo == 0) {
            if (a.virgula1 == 0) a.virgula1 = i;
            else if (a.virgula2 == 0) a.virgula2 = i;
        }
    }
    return a;
}

/* Um argumento so, nao vazio. */
static bool ArgumentoUnico(const char *t, uint32_t abre, const Argumentos *a)
{
    return a->fecha != 0 && a->virgula1 == 0 && PularEspacos(t, abre + 1) < a->fecha;
}

/* Relata o idioma em [pos, pos + tam) e sugere trocar o trecho por 'novo' (confianca dada; so_dica se o fonte nao tem o 'achado' ali).
 * LIMITE: parametros 5 (Semantica e id sao os dois donos do diagnostico; pos, achado e novo descrevem uma troca so; agrupar criaria um tipo de uso unico). */
static bool RelatarTroca(Semantica *s, IdiomaId id, uint32_t pos, const char *achado, const char *novo)
{
    IdiomaTroca troca = IDIOMA_TROCA_VAZIA;
    char descricao[DIAG_DESCRICAO_MAX];
    size_t tam = strlen(achado);
    bool no_fonte = strncmp(s->fonte->texto + pos, achado, tam) == 0;

    IdiomaRelatar(s->diag, id, pos, achado);
    if (no_fonte) IdiomaTrocaSomar(&troca, s->fonte->texto, pos, pos + (uint32_t)tam, novo);
    snprintf(descricao, sizeof descricao, "troque '%s' por '%s'", achado, novo);
    IdiomaSugerir(s->diag, DIAG_CERTA, descricao, &troca);
    return true;
}

/* Relata o idioma em 'pos' so com a dica em texto (sem troca mecanica).
 * LIMITE: parametros 5 (mesmos da RelatarTroca; 'dica' no lugar do texto novo). */
static bool RelatarSoDica(Semantica *s, IdiomaId id, uint32_t pos, const char *achado, const char *dica)
{
    IdiomaRelatar(s->diag, id, pos, achado);
    IdiomaSugerir(s->diag, DIAG_SO_DICA, dica, NULL);
    return true;
}

static const NomeAlheio *AcharNomeAlheio(const NomeAlheio *tabela, size_t qtd, const char *nome)
{
    for (size_t i = 0; i < qtd; i++) {
        if (strcmp(tabela[i].nome, nome) == 0) return &tabela[i];
    }
    return NULL;
}

/* ---------- print: console.log(x), fmt.Println(x), println(x), printf(...) ---------- */

/* Depois do nome do objeto em 'pos' vem '.metodo(' ? Devolve o indice do '(' ou 0. */
static uint32_t AbreDoMetodo(const char *t, uint32_t depois_do_objeto, const char *metodo)
{
    uint32_t i = depois_do_objeto;
    size_t tam = strlen(metodo);

    if (t[i] != '.' || strncmp(t + i + 1, metodo, tam) != 0 || CharDeNome(t[i + 1 + tam])) return 0;
    i = PularEspacos(t, i + 1 + (uint32_t)tam);
    return t[i] == '(' ? i : 0;
}

/* Relata o print de outra linguagem cujo trecho 'achado' comeca em 'pos' e abre o '(' em 'abre'; a troca mecanica poe print( no lugar do trecho todo.
 * LIMITE: parametros 5 (pos, abre e achado localizam o trecho no fonte; 'sozinha' decide entre troca certa e so dica; agrupar criaria um tipo de uso unico). */
static bool RelatarImprime(Semantica *s, uint32_t pos, uint32_t abre, const char *achado, bool sozinha)
{
    const char *t = s->fonte->texto;
    Argumentos a = LerArgumentos(t, abre);
    IdiomaTroca troca = IDIOMA_TROCA_VAZIA;

    IdiomaRelatar(s->diag, IDI_IMPRIME, pos, achado);
    if (sozinha && ArgumentoUnico(t, abre, &a)) IdiomaTrocaSomar(&troca, t, pos, abre + 1, "print(");
    IdiomaSugerir(s->diag, DIAG_CERTA, "troque pela forma da Forja: print(valor), e print(f\"texto {x}\") para juntar texto e valores", &troca);
    return true;
}

static bool ImprimeComoMetodo(Semantica *s, const Expr *e, const char *nome)
{
    const char *t = s->fonte->texto;
    uint32_t depois = e->pos_op + (uint32_t)strlen(nome);
    char achado[NOME_ACHADO_MAX];

    for (size_t i = 0; i < sizeof IMPRIMES / sizeof IMPRIMES[0]; i++) {
        uint32_t abre = strcmp(IMPRIMES[i].objeto, nome) == 0 ? AbreDoMetodo(t, depois, IMPRIMES[i].metodo) : 0;
        if (abre == 0) continue;
        snprintf(achado, sizeof achado, "%s.%s", nome, IMPRIMES[i].metodo);
        return RelatarImprime(s, e->pos_op, abre, achado, IMPRIMES[i].sozinha);
    }
    return false;
}

/* ---------- nome, tipo ---------- */

bool SemIdiomaDeNome(Semantica *s, const Expr *e)
{
    const char *nome = SemNomeTexto(s, e->nome);
    const NomeAlheio *x = AcharNomeAlheio(NOMES_ALHEIOS, sizeof NOMES_ALHEIOS / sizeof NOMES_ALHEIOS[0], nome);

    if (x != NULL) return RelatarTroca(s, x->id, e->pos_op, nome, x->certo);
    return ImprimeComoMetodo(s, e, nome);
}

bool SemIdiomaDeTipo(Semantica *s, int32_t nome, uint32_t pos)
{
    const char *texto = SemNomeTexto(s, nome);
    const NomeAlheio *x = AcharNomeAlheio(TIPOS_ALHEIOS, sizeof TIPOS_ALHEIOS / sizeof TIPOS_ALHEIOS[0], texto);

    return x != NULL && RelatarTroca(s, x->id, pos, texto, x->certo);
}

/* ---------- proc que nao existe: range(...), println(...), printf(...) ---------- */

/* O texto antes de 'pos' termina na palavra 'in' (cabecalho do for). */
static bool DepoisDoIn(const char *t, uint32_t pos)
{
    uint32_t i = pos;

    while (i > 0 && (t[i - 1] == ' ' || t[i - 1] == '\t')) i--;
    return i >= 2 && t[i - 2] == 'i' && t[i - 1] == 'n' && (i == 2 || !CharDeNome(t[i - 3]));
}

/* Argumento de range com so operandos e contas (nome, numero, len(x), a + 1): vira o limite de a..<b sem mudar de sentido. */
static bool ArgumentoSimples(const char *t, uint32_t ini, uint32_t fim)
{
    for (uint32_t i = ini; i < fim; i++) {
        if (!CharDeNome(t[i]) && strchr(".()[]+-*/% ", t[i]) == NULL) return false;
    }
    return fim > ini;
}

/* [ini, fim) sem os espacos das pontas, copiado em 'saida' (cap bytes); false se nao cabe.
 * LIMITE: parametros 5 (texto + faixa [ini, fim) + destino com capacidade: a forma de snprintf; agrupar criaria um tipo de uso unico). */
static bool CopiarSemEspacos(const char *t, uint32_t ini, uint32_t fim, char *saida, size_t cap)
{
    while (ini < fim && (t[ini] == ' ' || t[ini] == '\t')) ini++;
    while (fim > ini && (t[fim - 1] == ' ' || t[fim - 1] == '\t')) fim--;
    if (fim - ini >= cap) return false;
    memcpy(saida, t + ini, fim - ini);
    saida[fim - ini] = '\0';
    return true;
}

/* range(n) vira 0..<n (duas edicoes: "range(" e ")"); range(a, b) vira a..<b (uma edicao); a forma com passo e so dica. */
static bool RelatarRange(Semantica *s, uint32_t pos, uint32_t abre)
{
    const char *t = s->fonte->texto;
    Argumentos a = LerArgumentos(t, abre);
    IdiomaTroca troca = IDIOMA_TROCA_VAZIA;
    char de[LIMITE_DE_FAIXA_MAX], ate[LIMITE_DE_FAIXA_MAX], novo[DIAG_NOVO_MAX]; /* 13 + "..<" + 13 cabe em DIAG_NOVO_MAX */
    bool simples = a.fecha != 0 && DepoisDoIn(t, pos) && a.virgula2 == 0 &&
                   ArgumentoSimples(t, abre + 1, a.virgula1 != 0 ? a.virgula1 : a.fecha) && (a.virgula1 == 0 || ArgumentoSimples(t, a.virgula1 + 1, a.fecha));

    IdiomaRelatar(s->diag, IDI_RANGE, pos, "range");
    if (simples && a.virgula1 == 0) {
        IdiomaTrocaSomar(&troca, t, pos, abre + 1, "0..<");
        IdiomaTrocaSomar(&troca, t, a.fecha, a.fecha + 1, "");
    } else if (simples && CopiarSemEspacos(t, abre + 1, a.virgula1, de, sizeof de) && CopiarSemEspacos(t, a.virgula1 + 1, a.fecha, ate, sizeof ate)) {
        snprintf(novo, sizeof novo, "%s..<%s", de, ate);
        IdiomaTrocaSomar(&troca, t, pos, a.fecha + 1, novo);
    }
    IdiomaSugerir(s->diag, DIAG_CERTA, "troque range(n) por 0..<n e range(a, b) por a..<b", &troca);
    return true;
}

bool SemIdiomaDeChamada(Semantica *s, const Expr *e)
{
    const char *t = s->fonte->texto, *nome = SemNomeTexto(s, e->nome);
    uint32_t abre = PularEspacos(t, e->pos_op + (uint32_t)strlen(nome));

    if (t[abre] != '(') return false;
    if (strcmp(nome, "range") == 0) return RelatarRange(s, e->pos_op, abre);
    for (size_t i = 0; i < sizeof IMPRIMES_SOLTOS / sizeof IMPRIMES_SOLTOS[0]; i++) {
        if (strcmp(IMPRIMES_SOLTOS[i].objeto, nome) != 0) continue;
        if (!IMPRIMES_SOLTOS[i].sozinha) return RelatarSoDica(s, IDI_IMPRIME, e->pos_op, nome, "troque printf por print(f\"texto {x}\"): o texto e os valores vao dentro do f\"...\"");
        return RelatarImprime(s, e->pos_op, abre, nome, true);
    }
    return false;
}

/* ---------- campo: x.len(), x.length, x.push(v) ---------- */

static bool NomeEntre(const char *nome, const char *const *lista)
{
    for (; *lista != NULL; lista++) {
        if (strcmp(*lista, nome) == 0) return true;
    }
    return false;
}

/* Colecao ([N]T, []T, [dynamic]T, [dynamic, N]T, ponteiro para elas) ou texto: o que len(x) mede. */
static bool TemTamanho(const Semantica *s, TipoId tipo)
{
    ColecaoForma forma;

    SemElementoDe(s, tipo, &forma);
    return forma != FORMA_NENHUMA || SemEhListaFixaOuPonteiro(s, tipo) || tipo == TIPO_TEXTO;
}

/* O '.' que antecede o campo (pos_op), pulando espacos; 0 = nao achou. */
static uint32_t PontoDoCampo(const char *t, uint32_t pos_op)
{
    uint32_t i = pos_op;

    while (i > 0 && (t[i - 1] == ' ' || t[i - 1] == '\t')) i--;
    return i > 0 && t[i - 1] == '.' ? i - 1 : 0;
}

/* O receptor e uma cadeia simples nome(.nome)*: so assim a troca monta a chamada sem reescrever o receptor. */
static bool ReceptorSimples(const char *t, uint32_t ini, uint32_t ponto)
{
    for (uint32_t i = ini; i < ponto; i++) {
        if (!CharDeNome(t[i]) && t[i] != '.') return false;
    }
    return ponto > ini;
}

/* x.len() | x.length | x.size() | x.count: troca por len(x) (insere "len(" antes do receptor e troca o ponto e o campo por ")"). */
static bool RelatarTamanho(Semantica *s, const Expr *e, uint32_t ponto, const char *achado)
{
    const char *t = s->fonte->texto;
    uint32_t fim = e->pos_op + (uint32_t)strlen(SemNomeTexto(s, e->nome)), abre = PularEspacos(t, fim);
    IdiomaTroca troca = IDIOMA_TROCA_VAZIA;
    bool chamada = t[abre] == '(', fecha_logo = chamada && t[PularEspacos(t, abre + 1)] == ')';

    if (chamada && fecha_logo) fim = PularEspacos(t, abre + 1) + 1;
    IdiomaRelatar(s->diag, IDI_TAMANHO, e->pos_op, achado);
    if (ReceptorSimples(t, e->pos, ponto) && (!chamada || fecha_logo)) {
        IdiomaTrocaSomar(&troca, t, e->pos, e->pos, "len(");
        IdiomaTrocaSomar(&troca, t, ponto, fim, ")");
    }
    IdiomaSugerir(s->diag, DIAG_CERTA, "troque pelo proc len(x): o tamanho nao e metodo nem campo", &troca);
    return true;
}

/* A lista do receptor e var (raiz da cadeia e local var): so entao o append da troca compila. */
static bool ReceptorEhVar(const Semantica *s, const Expr *receptor)
{
    const Expr *raiz = SemRaizDaCadeia(s, receptor);
    const LocalInfo *local = raiz != NULL && raiz->tipo_no == EX_NOME ? EscopoBuscar(&s->escopo, raiz->nome) : NULL;

    return local != NULL && local->natureza == LOC_VAR;
}

/* x.append(v) | x.push(v): append(&x, item = v) (insere "append(&" antes do receptor e troca ".push(" por ", item = "; o ')' do fim fica). */
static bool RelatarAcrescentar(Semantica *s, const Expr *e, uint32_t ponto, const char *achado)
{
    const char *t = s->fonte->texto;
    uint32_t abre = PularEspacos(t, e->pos_op + (uint32_t)strlen(SemNomeTexto(s, e->nome)));
    Argumentos a = t[abre] == '(' ? LerArgumentos(t, abre) : (Argumentos){0, 0, 0};
    IdiomaTroca troca = IDIOMA_TROCA_VAZIA;

    IdiomaRelatar(s->diag, IDI_ACRESCENTAR, e->pos_op, achado);
    if (t[abre] == '(' && ArgumentoUnico(t, abre, &a) && ReceptorSimples(t, e->pos, ponto) && ReceptorEhVar(s, e->a)) {
        IdiomaTrocaSomar(&troca, t, e->pos, e->pos, "append(&");
        IdiomaTrocaSomar(&troca, t, ponto, abre + 1, ", item = ");
    }
    IdiomaSugerir(s->diag, DIAG_CERTA, "troque pelo proc append(&x, item = v): lista nao tem metodo", &troca);
    return true;
}

bool SemIdiomaDeCampo(Semantica *s, const Expr *e)
{
    const char *nome = SemNomeTexto(s, e->nome);
    uint32_t ponto = PontoDoCampo(s->fonte->texto, e->pos_op);
    char achado[NOME_ACHADO_MAX];
    bool tamanho = NomeEntre(nome, TAMANHOS), acrescenta = NomeEntre(nome, ACRESCENTAS);

    if ((!tamanho && !acrescenta) || ponto == 0 || !TemTamanho(s, e->a->tipo)) return false;
    snprintf(achado, sizeof achado, "x.%s", nome);
    return tamanho ? RelatarTamanho(s, e, ponto, achado) : RelatarAcrescentar(s, e, ponto, achado);
}
