/* consultar_pos.c - posicoes 'arq:L:C' e a pergunta 'forja consultar tipo <arq:L:C>' (ver consultar_interno.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I2: primeira versao. O tipo da posicao e o da menor expressao da AST analisada que a contem ([pos, fim) das posicoes finais de ast_fim.h); sem expressao ali,
 *            o tipo do parametro, campo, let ou var cujo NOME esta na posicao.
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "driver/consultar_interno.h"
#include "semantic/ast_json.h"

/* ---------- arq:L:C ---------- */

typedef struct {
    char arq[MAX_PATH + 16];
    uint32_t linha, coluna;
} PosicaoTexto;

/* "arq:L:C" -> arq (copiado), L e C; false se nao tem a forma (L e C so digitos). */
static bool PartirPosicao(const char *texto, PosicaoTexto *saida)
{
    const char *c2 = strrchr(texto, ':');
    const char *c1;
    char *fim;

    if (c2 == NULL || c2 == texto) return false;
    for (c1 = c2 - 1; c1 > texto && *c1 != ':'; c1--) {}
    if (*c1 != ':' || c1 == texto) return false;
    saida->coluna = (uint32_t)strtoul(c2 + 1, &fim, 10);
    if (c2[1] == '\0' || *fim != '\0') return false;
    saida->linha = (uint32_t)strtoul(c1 + 1, &fim, 10);
    if (c1[1] == '\0' || fim != c2) return false;
    if ((size_t)(c1 - texto) >= sizeof saida->arq) return false;
    memcpy(saida->arq, texto, (size_t)(c1 - texto));
    saida->arq[c1 - texto] = '\0';
    return true;
}

bool ConsultaArquivoDaPosicao(const char *texto, char *destino, size_t tam)
{
    PosicaoTexto partida;

    if (!PartirPosicao(texto, &partida) || strlen(partida.arq) >= tam) return false;
    memcpy(destino, partida.arq, strlen(partida.arq) + 1); /* cabe: tamanho conferido acima */
    return true;
}

/* Caminho em minusculas com '\' (e o caminho completo se o arquivo existe): dois nomes do mesmo arquivo dao o mesmo texto. */
static void Normalizar(const char *caminho, char *destino, size_t tam)
{
    char completo[MAX_PATH];
    DWORD n = strncmp(caminho, "std:", 4) == 0 ? 0 : GetFullPathNameA(caminho, sizeof completo, completo, NULL); /* std:<modulo> nao e arquivo */
    const char *origem = (n > 0 && n < sizeof completo) ? completo : caminho;
    size_t i = 0;

    for (; origem[i] != '\0' && i + 1 < tam; i++) destino[i] = origem[i] == '/' ? '\\' : (char)tolower((unsigned char)origem[i]);
    destino[i] = '\0';
}

static bool MesmoCaminho(const char *a, const char *b)
{
    char na[MAX_PATH + 16], nb[MAX_PATH + 16];

    Normalizar(a, na, sizeof na);
    Normalizar(b, nb, sizeof nb);
    return strcmp(na, nb) == 0;
}

bool ConsultaLerPosicao(Consulta *q, const char *texto, PosicaoPedida *pos)
{
    const Fonte *f = &q->c->fonte;
    PosicaoTexto partida;
    uint32_t nt = f->ntrechos > 0 ? f->ntrechos : 1;

    if (!PartirPosicao(texto, &partida) || partida.linha == 0 || partida.coluna == 0) {
        ConsultaUsoErrado("posicao", texto, "nao tem a forma arquivo:linha:coluna (linha e coluna a partir de 1)");
        return false;
    }
    const char *arq = partida.arq;
    uint32_t linha = partida.linha, coluna = partida.coluna;

    for (uint32_t k = 0; k < nt; k++) {
        const char *caminho = f->ntrechos > 0 ? f->trechos[k].caminho : f->caminho;
        uint32_t base = f->ntrechos > 0 ? f->trechos[k].linha_base : 0;
        uint32_t fim = f->ntrechos > 0 ? f->trechos[k].fim : (uint32_t)f->tam;
        uint64_t global = (uint64_t)linha + base;

        if (!MesmoCaminho(caminho, arq)) continue;
        if (f->inicio_linha == NULL || global > f->ninicios || f->inicio_linha[global - 1] + coluna - 1 >= fim) break;
        *pos = (PosicaoPedida){.off = f->inicio_linha[global - 1] + coluna - 1, .linha = linha, .coluna = coluna, .arquivo = caminho};
        return true;
    }
    ConsultaUsoErrado("posicao", texto, "esta fora dos arquivos do projeto (arquivo desconhecido ou linha/coluna alem do fim)");
    return false;
}

const Decl *ConsultaDeclEm(const Consulta *q, uint32_t off)
{
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_IMPORT || d->tipo_no == D_LINK) continue;
        if (ConsultaInicioDecl(q, d) <= off && off < AstFimDecl(&q->fim, d)) return d;
    }
    return NULL;
}

/* ---------- tipo ---------- */

/* O nome declarado cujo texto esta em [pos, pos + tam): parametro, campo, let ou var. */
typedef struct { uint32_t pos, tam; TipoId tipo; const char *no; } NomeDeclarado;

typedef struct {
    const Consulta *q;
    uint32_t off;
    const Expr *expr;
    NomeDeclarado nome;
} BuscaTipo;

static void VisitarStmts(BuscaTipo *b, const Stmt *s);

static bool EhIndiceSintetico(const Expr *e)
{
    return e->tipo_no == EX_INT && e->ref == REF_COMPONENTE;
}

static void VisitarExpr(BuscaTipo *b, const Expr *e)
{
    if (e == NULL || EhIndiceSintetico(e)) return;
    if (e->pos <= b->off && b->off < AstFimExpr(&b->q->fim, e)) b->expr = e;
    VisitarExpr(b, e->a);
    VisitarExpr(b, e->b);
    if (e->tipo_no == EX_CONVERSAO || e->tipo_no == EX_TIPO) return; /* args e alvo dividem a uniao */
    for (uint32_t i = 0; i < e->nargs; i++) VisitarExpr(b, e->args[i].valor);
}

/* O nome comeca depois da palavra-chave e dos espacos que o seguem. */
static void NomeDoLet(BuscaTipo *b, const Stmt *s)
{
    const Fonte *f = &b->q->c->fonte;
    const Interner *nomes = &b->q->c->nomes;
    uint32_t p = s->pos;

    while (p < f->tam && isalpha((unsigned char)f->texto[p])) p++;
    while (p < f->tam && (f->texto[p] == ' ' || f->texto[p] == '\t')) p++;
    if (s->nome < 0 || p + InternerTam(nomes, s->nome) > f->tam || memcmp(f->texto + p, InternerTexto(nomes, s->nome), InternerTam(nomes, s->nome)) != 0) return;
    if (b->off >= p && b->off < p + InternerTam(nomes, s->nome)) b->nome = (NomeDeclarado){p, InternerTam(nomes, s->nome), s->tipo_local, s->tipo_no == ST_VAR ? "var" : "let"};
}

static void VisitarCasos(BuscaTipo *b, const Stmt *s)
{
    for (uint32_t i = 0; s->tipo_no == ST_SWITCH && i < s->ncasos; i++) VisitarStmts(b, s->casos[i].corpo);
}

static void VisitarStmts(BuscaTipo *b, const Stmt *s)
{
    for (; s != NULL; s = s->prox) {
        if (b->off < s->pos || b->off >= AstFimStmt(&b->q->fim, s)) continue;
        if (s->tipo_no == ST_LET || s->tipo_no == ST_VAR) NomeDoLet(b, s);
        VisitarExpr(b, s->expr);
        VisitarExpr(b, s->expr2);
        VisitarExpr(b, s->alvo);
        VisitarCasos(b, s);
        VisitarStmts(b, s->corpo);
        VisitarStmts(b, s->senao);
    }
}

/* 'cand' traz pos, tipo e rotulo do param/campo; o tamanho vem do nome. */
static void NomeDeParamOuCampo(BuscaTipo *b, int32_t nome, NomeDeclarado cand)
{
    cand.tam = nome >= 0 ? InternerTam(&b->q->c->nomes, nome) : 0;
    if (b->off >= cand.pos && b->off < cand.pos + cand.tam) b->nome = cand;
}

static void VisitarDecl(BuscaTipo *b, const Decl *d)
{
    VisitarExpr(b, d->init);
    for (uint32_t i = 0; i < d->nparams; i++) {
        NomeDeParamOuCampo(b, d->params[i].nome, (NomeDeclarado){d->params[i].pos, 0, d->params[i].tipo.tipo, "param"});
        VisitarExpr(b, d->params[i].padrao);
    }
    for (uint32_t i = 0; i < d->ncampos; i++) {
        NomeDeParamOuCampo(b, d->campos[i].nome, (NomeDeclarado){d->campos[i].pos, 0, d->campos[i].tipo.tipo, "campo"});
        VisitarExpr(b, d->campos[i].padrao);
    }
    VisitarStmts(b, d->corpo);
}

static void RespostaDoNome(Consulta *q, const NomeDeclarado *n)
{
    ConsultaChaveTexto(q, "no", n->no, strlen(n->no));
    ConsultaChavePos(q, "pos", n->pos);
    ConsultaChavePos(q, "fim", n->pos + n->tam);
    ConsultaChaveTrecho(q, "texto", n->pos, n->pos + n->tam);
    ConsultaChaveTipo(q, "tipo", n->tipo);
}

static void RespostaDaExpr(Consulta *q, const Expr *e)
{
    uint32_t fim = AstFimExpr(&q->fim, e);

    ConsultaChaveTexto(q, "no", AstNomeDaExpr(e->tipo_no), strlen(AstNomeDaExpr(e->tipo_no)));
    ConsultaChavePos(q, "pos", e->pos);
    ConsultaChavePos(q, "fim", fim);
    ConsultaChaveTrecho(q, "texto", e->pos, fim);
    ConsultaChaveTipo(q, "tipo", e->tipo);
}

int ConsultarTipo(Consulta *q, const PedidoConsulta *p)
{
    PosicaoPedida pos;
    const Decl *d;
    BuscaTipo busca = {.q = q};

    if (!ConsultaLerPosicao(q, p->arg, &pos)) return CONSULTAR_USO;
    busca.off = pos.off;
    d = ConsultaDeclEm(q, pos.off);
    if (d != NULL) VisitarDecl(&busca, d);
    ConsultaAbrir(q, "tipo");
    ConsultaChaveTexto(q, "arquivo", pos.arquivo, strlen(pos.arquivo));
    ConsultaChaveInt(q, "linha", pos.linha);
    ConsultaChaveInt(q, "coluna", pos.coluna);
    if (busca.expr != NULL) {
        RespostaDaExpr(q, busca.expr);
    } else if (busca.nome.no != NULL) {
        RespostaDoNome(q, &busca.nome);
    } else {
        static const char motivo[] = "nenhuma expressao nem nome declarado nessa posicao";
        ConsultaChaveTexto(q, "no", NULL, 0);
        ConsultaChaveTipo(q, "tipo", TIPO_NAO_RESOLVIDO);
        ConsultaChaveTexto(q, "motivo", motivo, sizeof motivo - 1);
    }
    ConsultaFechar(q);
    return CONSULTAR_OK;
}
