/* consultar_escopo.c - 'forja consultar escopo <arq:L:C>': o que um ponto do fonte enxerga (ver consultar_interno.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b: o modulo do ponto vem do mapa trecho -> pacote (ConsultaModuloDoOffset); os imports visiveis sao os do arquivo do ponto (o modulo de pasta tem varios); "arquivo" em cada global so num pacote de 2+ arquivos.
 *            0.1.0 - IA1-I2: primeira versao. locais = parametros do proc/test que contem o ponto mais os let/var/for/switch ja declarados nos blocos que o contem (um let ainda
 *            nao e visivel dentro do proprio inicializador); globais = declaracoes do mesmo modulo e as pub dos modulos que ele importa, estas com o nome qualificado ('apelido.nome').
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/consultar_interno.h"

typedef struct {
    int32_t nome;
    const char *categoria;
    TipoId tipo;
    uint32_t pos;
} LocalVisivel;

static void LocalJson(Consulta *q, const LocalVisivel *l)
{
    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", l->nome);
    ConsultaChaveTexto(q, "categoria", l->categoria, strlen(l->categoria));
    ConsultaChaveTipo(q, "tipo", l->tipo);
    ConsultaChavePos(q, "pos", l->pos);
    ConsultaFecharObj(q);
}

static void LocaisDosStmts(Consulta *q, uint32_t off, const Stmt *s);

static void LocaisDoFor(Consulta *q, uint32_t off, const Stmt *s)
{
    const Expr *cabeca = s->expr2 != NULL ? s->expr2 : s->expr;

    if (cabeca != NULL && off >= AstFimExpr(&q->fim, cabeca)) {
        LocalJson(q, &(LocalVisivel){s->nome, "for", s->tipo_local, s->pos});
        if (s->nome_indice >= 0) LocalJson(q, &(LocalVisivel){s->nome_indice, "for_indice", TIPO_INT, s->pos});
    }
    LocaisDosStmts(q, off, s->corpo);
}

static void LocaisDoSwitch(Consulta *q, uint32_t off, const Stmt *s)
{
    for (uint32_t i = 0; i < s->ncasos; i++) {
        const Caso *k = &s->casos[i];
        if (k->pos > off || off >= AstFimCaso(&q->fim, k)) continue;
        if (s->nome >= 0 && k->local_id >= 0) LocalJson(q, &(LocalVisivel){s->nome, "switch", k->tipo, k->pos});
        LocaisDosStmts(q, off, k->corpo);
    }
}

/* let/var (simples ou multiplo) ja terminado antes do ponto. */
static void LocaisDoLet(Consulta *q, const Stmt *s)
{
    if (s->tipo_no == ST_LET || s->tipo_no == ST_VAR) {
        LocalJson(q, &(LocalVisivel){s->nome, s->tipo_no == ST_VAR ? "var" : "let", s->tipo_local, s->pos});
        return;
    }
    for (uint32_t i = 0; s->tipo_no == ST_LET_MULTIPLO && i < s->nnomes; i++) {
        if (s->nomes[i].local_id >= 0) LocalJson(q, &(LocalVisivel){s->nomes[i].nome, s->op == 1 ? "var" : "let", s->nomes[i].tipo, s->nomes[i].pos});
    }
}

/* O ponto esta dentro do comando 's': desce nos blocos dele. */
static void LocaisDentro(Consulta *q, uint32_t off, const Stmt *s)
{
    if (s->tipo_no == ST_FOR) LocaisDoFor(q, off, s);
    else if (s->tipo_no == ST_SWITCH) LocaisDoSwitch(q, off, s);
    else if (s->tipo_no != ST_LET && s->tipo_no != ST_VAR) {
        LocaisDosStmts(q, off, s->corpo);
        LocaisDosStmts(q, off, s->senao);
    }
}

/* let/var declarados antes do ponto (e ja terminados) e os nomes dos blocos que contem o ponto. */
static void LocaisDosStmts(Consulta *q, uint32_t off, const Stmt *s)
{
    for (; s != NULL && s->pos <= off; s = s->prox) {
        if (off < AstFimStmt(&q->fim, s)) LocaisDentro(q, off, s);
        else LocaisDoLet(q, s);
    }
}

static void LocaisJson(Consulta *q, uint32_t off, const Decl *d)
{
    ConsultaAbrirLista(q, "locais");
    if (d != NULL && (d->tipo_no == D_PROC || d->tipo_no == D_TEST)) {
        for (uint32_t i = 0; i < d->nparams; i++) LocalJson(q, &(LocalVisivel){d->params[i].nome, "param", d->params[i].tipo.tipo, d->params[i].pos});
        LocaisDosStmts(q, off, d->corpo);
    }
    ConsultaFecharLista(q);
}

/* ---------- globais ---------- */

static void GlobalJson(Consulta *q, const Decl *d, const Buf *qualificado)
{
    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveTexto(q, "categoria", IndiceCategoria(d), strlen(IndiceCategoria(d)));
    ConsultaChaveModulo(q, "modulo", d->modulo);
    ConsultaChaveTexto(q, "qualificado", qualificado != NULL ? qualificado->dados : NULL, qualificado != NULL ? qualificado->tam : 0);
    ConsultaChaveBool(q, "publico", d->publico);
    ConsultaChaveArquivoSePacote(q, d->pos);
    ConsultaChavePos(q, "pos", d->pos);
    ConsultaFecharObj(q);
}

/* Apelido com que o modulo 'importado' entra no arquivo de 'imp': o 'as' escrito ou o ultimo segmento do caminho. */
static int32_t ApelidoDoImport(const Consulta *q, const Decl *imp)
{
    return imp->nome_a >= 0 ? imp->nome_a : q->c->modulos.itens[imp->indice].apelido;
}

static void GlobaisImportados(Consulta *q, const Decl *imp)
{
    const Interner *nomes = &q->c->nomes;
    int32_t apelido = ApelidoDoImport(q, imp);

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        Buf nome = {0};
        if (d->modulo != imp->indice || !d->publico || !ConsultaEhSimbolo(d)) continue;
        BufFormatar(&nome, "%.*s.%.*s", (int)InternerTam(nomes, apelido), InternerTexto(nomes, apelido), (int)InternerTam(nomes, d->nome), InternerTexto(nomes, d->nome));
        GlobalJson(q, d, &nome);
        BufLiberar(&nome);
    }
}

/* O import 'd' esta no mesmo arquivo do ponto 'off'? (v1.13: os imports do modulo de pasta valem por arquivo; os nomes do pacote valem em todos). */
static bool ImportDoArquivoDoPonto(const Consulta *q, const Decl *d, uint32_t off)
{
    return d->tipo_no == D_IMPORT && ConsultaTrechoDoOffset(q, d->pos) == ConsultaTrechoDoOffset(q, off);
}

static void GlobaisJson(Consulta *q, uint8_t modulo, uint32_t off)
{
    ConsultaAbrirLista(q, "globais");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo == modulo && ConsultaEhSimbolo(d)) GlobalJson(q, d, NULL);
    }
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo == modulo && ImportDoArquivoDoPonto(q, d, off)) GlobaisImportados(q, d);
    }
    ConsultaFecharLista(q);
}

static void ImportadosJson(Consulta *q, uint8_t modulo, uint32_t off)
{
    ConsultaAbrirLista(q, "importados");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo != modulo || !ImportDoArquivoDoPonto(q, d, off)) continue;
        ConsultaAbrirObj(q);
        ConsultaChaveNome(q, "apelido", ApelidoDoImport(q, d));
        ConsultaChaveModulo(q, "modulo", (uint8_t)d->indice);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

int ConsultarEscopo(Consulta *q, const PedidoConsulta *p)
{
    PosicaoPedida pos;
    const Decl *d;
    uint8_t modulo;

    if (!ConsultaLerPosicao(q, p->arg, &pos)) return CONSULTAR_USO;
    d = ConsultaDeclEm(q, pos.off);
    modulo = ConsultaModuloDoOffset(q, pos.off);
    ConsultaAbrir(q, "escopo");
    ConsultaChaveTexto(q, "arquivo", pos.arquivo, strlen(pos.arquivo));
    ConsultaChaveInt(q, "linha", pos.linha);
    ConsultaChaveInt(q, "coluna", pos.coluna);
    ConsultaChaveModulo(q, "modulo", modulo);
    if (d != NULL) ConsultaChaveId(q, "em", d);
    else ConsultaChaveTexto(q, "em", NULL, 0);
    LocaisJson(q, pos.off, d);
    GlobaisJson(q, modulo, pos.off);
    ImportadosJson(q, modulo, pos.off);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}
