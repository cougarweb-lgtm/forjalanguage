/* migrar_comentario.c - os comentarios do 'forja migrar --comentarios' (ver migrar_comentario.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L3d2: primeira versao.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "common/fonte.h"
#include "common/intern.h"
#include "driver/compilacao.h"
#include "driver/migrar_comentario.h"
#include "driver/migrar_nomes.h"
#include "frontend/lexer.h"

enum { TRECHO_MAX = 160, FORMAS_DE_PARSE = 3 };

/* Um '//' do arquivo: onde esta, em que linha e coluna, e se e a unica coisa da linha (so espaco antes). */
typedef struct { uint32_t inicio, fim, linha, coluna; bool sozinho; } Comentario;

/* Uma linha do bloco no texto sintetico: onde ela comeca la e onde o texto dela (depois do '//') comeca no arquivo. */
typedef struct { uint32_t off, linha, coluna; } LinhaDoBloco;

/* O texto sintetico de um bloco: as linhas depois do '//', separadas por LF (sem o CR do CRLF). */
typedef struct { Buf texto; Vec linhas; } Bloco;

typedef struct {
    const char *caminho;
    const Fonte *fonte;
    Arena *arena;
    Vec trocas, revisoes;       /* MigrarTroca e MigrarRevisao, na ordem do texto */
} Contexto;

/* O texto sintetico aceito pelo parser e onde o bloco comeca nele (depois da moldura). */
typedef struct { Compilacao *c; uint32_t base; } Analise;

/* As tres molduras, na ordem: o texto e o arquivo, o corpo de um proc, o valor de um let. */
static const char *const PREFIXOS[FORMAS_DE_PARSE] = {"", "proc forja_comentario() {\n", "proc forja_comentario() {\nlet forja_valor = "};
static const char *const SUFIXOS[FORMAS_DE_PARSE] = {"\n", "\n}\n", "\n}\n"};

/* ---------- os comentarios do arquivo ---------- */

/* A Fonte da analise: copia do texto em malloc (o lexer pede NUL em texto[tam]). */
static bool CarregarFonte(Fonte *f, const char *caminho, const char *texto, size_t tam)
{
    char *buf = malloc(tam + 1);

    if (buf == NULL) return false;
    memcpy(buf, texto, tam);
    buf[tam] = '\0';
    *f = (Fonte){.caminho = caminho, .texto = buf, .tam = tam};
    FonteIndexarLinhas(f);
    return true;
}

static bool SoEspacoAntes(const Fonte *f, uint32_t inicio, uint32_t coluna)
{
    for (uint32_t k = inicio - (coluna - 1); k < inicio; k++)
        if (f->texto[k] != ' ' && f->texto[k] != '\t') return false;
    return true;
}

/* O lexer da Forja acha os '//' (os que estao dentro de texto entre aspas nao contam). */
static void ListarComentarios(const Fonte *f, Vec *coms)
{
    Vec brutos;
    Arena arena = {0};
    Interner nomes;
    Diag diag = {0};
    Lexer lx;

    VecIniciar(&brutos, sizeof(LexerComentario));
    InternerIniciar(&nomes, &arena);
    LexerNomesIniciais(&nomes);
    lx = (Lexer){.fonte = f, .nomes = &nomes, .diag = &diag, .comentarios = &brutos};
    LexerAnalisar(&lx);
    for (size_t k = 0; k < brutos.n; k++) {
        const LexerComentario *b = VecEm(&brutos, k);
        Comentario c = {.inicio = b->inicio, .fim = b->fim};

        FonteLocalizar(f, b->inicio, &c.linha, &c.coluna);
        c.sozinho = SoEspacoAntes(f, b->inicio, c.coluna);
        *(Comentario *)VecEmpurrar(coms) = c;
    }
    LexerLiberar(&lx);
    InternerLiberar(&nomes);
    ArenaLiberar(&arena);
    VecLiberar(&brutos);
}

/* O texto do comentario (depois do '//', sem o CR do fim): inicio e tamanho no fonte. */
static const char *TextoDoComentario(const Fonte *f, const Comentario *c, size_t *n)
{
    const char *p = f->texto + c->inicio + 2;

    *n = c->fim - c->inicio - 2;
    if (*n > 0 && p[*n - 1] == '\r') (*n)--;
    return p;
}

/* ---------- um bloco ---------- */

static void MontarBloco(const Contexto *cx, const Comentario *coms, size_t ini, size_t fim, Bloco *b)
{
    VecIniciar(&b->linhas, sizeof(LinhaDoBloco));
    BufAnexar(&b->texto, "");
    for (size_t i = ini; i < fim; i++) {
        size_t n;
        const char *p = TextoDoComentario(cx->fonte, &coms[i], &n);

        *(LinhaDoBloco *)VecEmpurrar(&b->linhas) = (LinhaDoBloco){(uint32_t)b->texto.tam, coms[i].linha, coms[i].coluna + 2};
        BufAnexarN(&b->texto, p, n);
        if (i + 1 < fim) BufAnexar(&b->texto, "\n");
    }
}

/* Parser sobre prefixo + corpo + sufixo da forma 'forma'; a Compilacao (viva) se o parser aceitou, NULL se nao. */
static Compilacao *Parsear(const Contexto *cx, const Buf *corpo, int forma)
{
    Opcoes opcoes = {.comando = "check", .entrada = cx->caminho};
    Compilacao *c = calloc(1, sizeof *c);
    Buf tudo = {0};
    bool ok;

    if (c == NULL) FalhaInterna("memoria esgotada alocando a compilacao do comentario");
    BufAnexar(&tudo, PREFIXOS[forma]);
    BufAnexarN(&tudo, corpo->dados, corpo->tam);
    BufAnexar(&tudo, SUFIXOS[forma]);
    ok = CarregarFonte(&c->fonte, cx->caminho, tudo.dados, tudo.tam);
    BufLiberar(&tudo);
    if (!ok) {
        free(c);
        return NULL;
    }
    IniciarCompilacao(c);
    c->opcoes = &opcoes;
    c->sem.lista.teto = OpcoesMaxErros(&opcoes);
    ok = AnalisarSintaxe(c, false);
    c->opcoes = NULL;
    if (ok) return c;
    LiberarCompilacao(c);
    free(c);
    return NULL;
}

static bool AnalisarBloco(const Contexto *cx, const Buf *corpo, Analise *a)
{
    for (int forma = 0; forma < FORMAS_DE_PARSE; forma++) {
        Compilacao *c = Parsear(cx, corpo, forma);

        if (c == NULL) continue;
        a->c = c;
        a->base = (uint32_t)strlen(PREFIXOS[forma]);
        return true;
    }
    return false;
}

/* Linha e coluna, no arquivo, do byte u do texto sintetico do bloco. */
static void PosicaoNoArquivo(const Bloco *b, uint32_t u, uint32_t *linha, uint32_t *coluna)
{
    const LinhaDoBloco *l = VecEm(&b->linhas, 0);

    for (size_t k = 1; k < b->linhas.n; k++) {
        const LinhaDoBloco *prox = VecEm(&b->linhas, k);

        if (prox->off > u) break;
        l = prox;
    }
    *linha = l->linha;
    *coluna = l->coluna + (u - l->off);
}

/* O texto cortado em TRECHO_MAX bytes (na fronteira de um caractere UTF-8) e sem espaco nas pontas. */
static const char *TrechoCortado(Arena *arena, const char *texto, size_t n)
{
    while (n > 0 && (texto[0] == ' ' || texto[0] == '\t')) {
        texto++;
        n--;
    }
    if (n > TRECHO_MAX) {
        n = TRECHO_MAX;
        while (n > 0 && ((unsigned char)texto[n] & 0xC0) == 0x80) n--;
    }
    while (n > 0 && (texto[n - 1] == ' ' || texto[n - 1] == '\t' || texto[n - 1] == '\r')) n--;
    return ArenaCopiarTexto(arena, texto, n);
}

/* Uma revisao: o comentario em (linha, coluna), o trecho dele e a lista de formas (Vec de const char *). */
static void SomarRevisao(Contexto *cx, uint32_t linha, uint32_t coluna, const char *trecho, size_t n, const Vec *formas)
{
    MigrarRevisao r = {.linha = linha, .coluna = coluna, .trecho = TrechoCortado(cx->arena, trecho, n), .nformas = (uint32_t)formas->n};

    r.formas = ArenaAlocar(cx->arena, (formas->n + 1) * sizeof *r.formas);
    memcpy(r.formas, formas->dados, formas->n * sizeof *r.formas);
    *(MigrarRevisao *)VecEmpurrar(&cx->revisoes) = r;
}

/* Uma forma antiga no texto: troca na linha e coluna dela. Se o texto sintetico nao confere com o que a regra viu, nao troca. */
static void SomarTroca(Contexto *cx, const Bloco *b, const NomeTroca *t, uint32_t base)
{
    MigrarTroca m = {.classe = t->classe, .antigo = t->antigo, .novo = t->novo, .grupo = -1, .comentario = true};
    uint32_t u = t->off - base;

    if (memcmp(b->texto.dados + u, t->antigo, t->tam) != 0) return;
    PosicaoNoArquivo(b, u, &m.linha, &m.coluna);
    m.coluna_fim = m.coluna + t->tam;
    *(MigrarTroca *)VecEmpurrar(&cx->trocas) = m;
}

/* Um qualificador antigo duvidoso (pode ser variavel): vai para a lista com o trecho dele como unica forma. */
static void SomarDuvida(Contexto *cx, const Bloco *b, const NomeTroca *d, uint32_t base)
{
    Vec formas;
    uint32_t linha, coluna, u = d->off - base;
    const char *forma = ArenaCopiarTexto(cx->arena, b->texto.dados + u, d->tam);

    VecIniciar(&formas, sizeof forma);
    *(const char **)VecEmpurrar(&formas) = forma;
    PosicaoNoArquivo(b, u, &linha, &coluna);
    SomarRevisao(cx, linha, coluna, forma, d->tam, &formas);
    VecLiberar(&formas);
}

/* O bloco parseou: as regras por nome sobre os tokens dele. */
static void TrocasDoBloco(Contexto *cx, const Bloco *b, const Analise *a)
{
    NomesEntrada e = {.texto = a->c->fonte.texto, .tokens = a->c->lexer.tokens.dados, .ntokens = a->c->lexer.tokens.n, .ini = a->base, .fim = a->base + (uint32_t)b->texto.tam};
    Vec trocas, duvidas;

    VecIniciar(&trocas, sizeof(NomeTroca));
    VecIniciar(&duvidas, sizeof(NomeTroca));
    NomesDoCodigo(&e, cx->arena, &trocas, &duvidas);
    for (size_t k = 0; k < trocas.n; k++) SomarTroca(cx, b, VecEm(&trocas, k), a->base);
    for (size_t k = 0; k < duvidas.n; k++) SomarDuvida(cx, b, VecEm(&duvidas, k), a->base);
    VecLiberar(&trocas);
    VecLiberar(&duvidas);
}

/* O comentario nao parseia: prosa ou mistura. So a lista das formas antigas, se ha. */
static void RevisarComentario(Contexto *cx, const Comentario *c)
{
    size_t n;
    const char *p = TextoDoComentario(cx->fonte, c, &n);
    const char *copia = ArenaCopiarTexto(cx->arena, p, n);
    Vec formas;

    VecIniciar(&formas, sizeof copia);
    NomesSoltos(copia, n, cx->arena, &formas);
    if (formas.n > 0) SomarRevisao(cx, c->linha, c->coluna, copia, n, &formas);
    VecLiberar(&formas);
}

/* Comentarios [ini, fim) como um texto so. relatar: se nao parseia, vai para a revisao (a tentativa com o bloco inteiro nao relata: as linhas tem a vez delas). */
static bool ProcessarLinhas(Contexto *cx, const Comentario *coms, size_t ini, size_t fim, bool relatar)
{
    Bloco b = {0};
    Analise a = {0};
    bool ok;

    MontarBloco(cx, coms, ini, fim, &b);
    ok = AnalisarBloco(cx, &b.texto, &a);
    if (ok) {
        TrocasDoBloco(cx, &b, &a);
        LiberarCompilacao(a.c);
        free(a.c);
    } else if (relatar) {
        RevisarComentario(cx, &coms[ini]);
    }
    BufLiberar(&b.texto);
    VecLiberar(&b.linhas);
    return ok;
}

static void ProcessarUnidade(Contexto *cx, const Comentario *coms, size_t ini, size_t fim)
{
    if (fim - ini > 1 && ProcessarLinhas(cx, coms, ini, fim, false)) return;
    for (size_t i = ini; i < fim; i++) ProcessarLinhas(cx, coms, i, i + 1, true);
}

/* Comentarios de linha inteira em linhas seguidas formam um bloco; o resto e um bloco por comentario. */
static void ProcessarTodos(Contexto *cx, const Vec *coms)
{
    const Comentario *c = coms->dados;
    size_t ini = 0;

    while (ini < coms->n) {
        size_t fim = ini + 1;

        while (c[ini].sozinho && fim < coms->n && c[fim].sozinho && c[fim].linha == c[fim - 1].linha + 1) fim++;
        ProcessarUnidade(cx, c, ini, fim);
        ini = fim;
    }
}

static void Exportar(const Contexto *cx, ComentariosSaida *saida)
{
    saida->ntrocas = (uint32_t)cx->trocas.n;
    saida->trocas = ArenaAlocar(cx->arena, (cx->trocas.n + 1) * sizeof *saida->trocas);
    if (cx->trocas.n > 0) memcpy(saida->trocas, cx->trocas.dados, cx->trocas.n * sizeof *saida->trocas);
    saida->nrevisoes = (uint32_t)cx->revisoes.n;
    saida->revisoes = ArenaAlocar(cx->arena, (cx->revisoes.n + 1) * sizeof *saida->revisoes);
    if (cx->revisoes.n > 0) memcpy(saida->revisoes, cx->revisoes.dados, cx->revisoes.n * sizeof *saida->revisoes);
}

void MigrarComentarios(const char *caminho, const char *texto, size_t tam, Arena *arena, ComentariosSaida *saida)
{
    Fonte f;
    Vec coms;
    Contexto cx = {.caminho = caminho, .fonte = &f, .arena = arena};

    memset(saida, 0, sizeof *saida);
    if (!CarregarFonte(&f, caminho, texto, tam)) return;
    VecIniciar(&coms, sizeof(Comentario));
    VecIniciar(&cx.trocas, sizeof(MigrarTroca));
    VecIniciar(&cx.revisoes, sizeof(MigrarRevisao));
    ListarComentarios(&f, &coms);
    ProcessarTodos(&cx, &coms);
    Exportar(&cx, saida);
    VecLiberar(&coms);
    VecLiberar(&cx.trocas);
    VecLiberar(&cx.revisoes);
    FonteLiberar(&f);
}
