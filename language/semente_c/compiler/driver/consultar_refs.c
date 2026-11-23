/* consultar_refs.c - 'forja consultar refs <nome>': todos os usos resolvidos de uma declaracao (ver consultar_interno.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-I4: ConsultaAlvosDe, ConsultaUsoEhDoAlvo e ConsultaPosDoAlvo (antes estaticas) ficam visiveis ao 'forja editar'; AlvoRefs e ListaAlvos foram para o consultar_interno.h; a saida de 'refs' nao muda.
 *            0.1.0 - IA1-I2: primeira versao. <nome> = declaracao global (proc, struct, enum, union, const, unit; "<modulo>:<nome>" desambigua), "Tipo.campo" ou "Enum.membro",
 *            "proc.parametro" ou "proc.local" (todos os locais daquele nome no proc, um alvo por declaracao) ou um embutido (print, len...). Os usos sao os do "ref" do 'forja ast'
 *            (mesmas posicoes: "pos" e o inicio do no da AST; "pos_nome" e o token do nome).
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/consultar_interno.h"

bool ConsultaUsoEhDoAlvo(const IndUso *u, const AlvoRefs *a)
{
    if (u->alvo_tipo != a->tipo || u->alvo != a->decl) return false;
    switch (a->tipo) {
    case IND_CAMPO:
    case IND_MEMBRO:
    case IND_PARAM: return u->indice == a->indice;
    case IND_LOCAL: return u->decl_pos == a->decl_pos && u->nome == a->nome;
    case IND_EMBUTIDO: return u->nome == a->nome;
    default: return true;
    }
}

static void UsoJson(Consulta *q, const IndUso *u)
{
    ConsultaAbrirObj(q);
    ConsultaChaveArquivo(q, u->pos);
    ConsultaChavePos(q, "pos", u->pos);
    ConsultaChavePos(q, "pos_nome", u->pos_nome);
    ConsultaChaveNome(q, "nome", u->nome);
    ConsultaChaveBool(q, "chamada", u->chamada);
    if (u->dono != NULL) ConsultaChaveId(q, "em", u->dono);
    else ConsultaChaveTexto(q, "em", NULL, 0);
    ConsultaFecharObj(q);
}

/* Id do alvo no formato do AST 1.0: "<modulo>:<decl>", ".../campos[i]", ".../membros[i]", ".../params[i]"; local: ".../<nome>@linha:coluna". */
static void IdDoAlvo(Consulta *q, const AlvoRefs *a, Buf *id)
{
    const Interner *nomes = &q->c->nomes;

    if (a->decl == NULL) {
        BufAnexarN(id, InternerTexto(nomes, a->nome), InternerTam(nomes, a->nome));
        return;
    }
    AstIdBaseDaDecl(id, &q->c->modulos, nomes, a->decl);
    if (a->tipo == IND_CAMPO) BufFormatar(id, "/campos[%d]", a->indice);
    if (a->tipo == IND_MEMBRO) BufFormatar(id, "/membros[%d]", a->indice);
    if (a->tipo == IND_PARAM) BufFormatar(id, "/params[%d]", a->indice);
    if (a->tipo == IND_LOCAL) {
        FontePosicao pos = FonteLocalizarNoArquivo(&q->c->fonte, a->decl_pos);
        BufFormatar(id, "/%.*s@%u:%u", (int)InternerTam(nomes, a->nome), InternerTexto(nomes, a->nome), pos.linha, pos.coluna);
    }
}

/* Onde o alvo e declarado: o nome da declaracao, o campo, o membro, o parametro ou o local; UINT32_MAX no embutido. */
uint32_t ConsultaPosDoAlvo(Consulta *q, const AlvoRefs *a)
{
    if (a->decl == NULL) return AST_REF_SEM_POS;
    switch (a->tipo) {
    case IND_CAMPO: return a->decl->campos[a->indice].pos;
    case IND_MEMBRO: return a->decl->membros[a->indice].pos;
    case IND_PARAM: return a->decl->params[a->indice].pos;
    case IND_LOCAL: return a->decl_pos;
    default: return ConsultaInicioDecl(q, a->decl);
    }
}

static void AlvoJson(Consulta *q, const AlvoRefs *a)
{
    Buf id = {0};
    uint32_t pos = ConsultaPosDoAlvo(q, a);
    int64_t total = 0;

    IdDoAlvo(q, a, &id);
    ConsultaAbrirObj(q);
    ConsultaChaveTexto(q, "categoria", IndiceNomeDoAlvo(a->tipo), strlen(IndiceNomeDoAlvo(a->tipo)));
    ConsultaChaveNome(q, "nome", a->nome);
    ConsultaChaveTexto(q, "id", id.dados, id.tam);
    BufLiberar(&id);
    if (pos != AST_REF_SEM_POS) {
        ConsultaChaveModulo(q, "modulo", a->decl->modulo);
        ConsultaChaveArquivo(q, pos);
        ConsultaChavePos(q, "pos", pos);
    }
    ConsultaAbrirLista(q, "usos");
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        if (!ConsultaUsoEhDoAlvo(u, a)) continue;
        UsoJson(q, u);
        total++;
    }
    ConsultaFecharLista(q);
    ConsultaChaveInt(q, "total", total);
    ConsultaFecharObj(q);
}

/* ---------- resolucao do argumento ---------- */

static void AcrescentarAlvo(ListaAlvos *l, AlvoRefs a)
{
    for (uint32_t i = 0; i < l->n; i++) {
        if (l->itens[i].tipo == a.tipo && l->itens[i].decl_pos == a.decl_pos && l->itens[i].indice == a.indice && l->itens[i].nome == a.nome) return;
    }
    if (l->n < sizeof l->itens / sizeof l->itens[0]) l->itens[l->n++] = a;
}

static AlvoRefs AlvoBase(IndAlvo tipo, const Decl *decl, int32_t nome)
{
    return (AlvoRefs){.tipo = tipo, .decl = decl, .indice = -1, .nome = nome, .decl_pos = AST_REF_SEM_POS};
}

static void AlvosDeCampoOuMembro(const Decl *d, int32_t nome, ListaAlvos *l)
{
    for (uint32_t i = 0; d->tipo_no != D_ENUM && i < d->ncampos; i++) {
        AlvoRefs a = AlvoBase(IND_CAMPO, d, nome);
        a.indice = (int32_t)i;
        if (d->campos[i].nome == nome) AcrescentarAlvo(l, a);
    }
    for (uint32_t i = 0; d->tipo_no == D_ENUM && i < d->nmembros; i++) {
        AlvoRefs a = AlvoBase(IND_MEMBRO, d, nome);
        a.indice = (int32_t)i;
        if (d->membros[i].nome == nome) AcrescentarAlvo(l, a);
    }
}

/* "proc.nome": o parametro (mesmo sem uso) e cada local de mesmo nome que algum uso alcanca. */
static void AlvosDeLocal(const Consulta *q, const Decl *d, int32_t nome, ListaAlvos *l)
{
    for (uint32_t i = 0; i < d->nparams; i++) {
        AlvoRefs a = AlvoBase(IND_PARAM, d, nome);
        a.indice = (int32_t)i;
        a.decl_pos = d->params[i].pos;
        if (d->params[i].nome == nome) AcrescentarAlvo(l, a);
    }
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        AlvoRefs a = AlvoBase(IND_LOCAL, d, nome);
        a.decl_pos = u->decl_pos;
        if (u->alvo_tipo == IND_LOCAL && u->alvo == d && u->nome == nome) AcrescentarAlvo(l, a);
    }
}

/* Nome sem declaracao global: se algum uso o liga a um embutido (print, len...), o alvo e o embutido. */
static void AlvosEmbutidos(const Consulta *q, int32_t nome, ListaAlvos *l)
{
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        if (u->alvo_tipo == IND_EMBUTIDO && u->nome == nome) {
            AcrescentarAlvo(l, AlvoBase(IND_EMBUTIDO, NULL, nome));
            return;
        }
    }
}

static void AlvosComPonto(Consulta *q, const char *arg, const char *ponto, ListaAlvos *l)
{
    char esquerda[512];
    size_t n = (size_t)(ponto - arg);
    const Decl *d;
    int32_t nome;

    if (n >= sizeof esquerda) return;
    memcpy(esquerda, arg, n);
    esquerda[n] = '\0';
    d = ConsultaAcharDecl(q, esquerda);
    nome = InternerAchar(&q->c->nomes, ponto + 1, (uint32_t)strlen(ponto + 1));
    if (d == NULL || nome < 0) return;
    if (d->tipo_no == D_PROC || d->tipo_no == D_TEST) AlvosDeLocal(q, d, nome, l);
    else if (d->tipo_no == D_STRUCT || d->tipo_no == D_UNIAO || d->tipo_no == D_ENUM) AlvosDeCampoOuMembro(d, nome, l);
}

static void AlvosSemPonto(Consulta *q, const char *arg, ListaAlvos *l)
{
    const Decl *d = ConsultaAcharDecl(q, arg);
    const char *dois = strrchr(arg, ':');
    int32_t nome = InternerAchar(&q->c->nomes, dois != NULL ? dois + 1 : arg, (uint32_t)strlen(dois != NULL ? dois + 1 : arg));

    if (d != NULL) AcrescentarAlvo(l, AlvoBase(IndiceAlvoDaDecl(d), d, d->nome));
    else if (nome >= 0) AlvosEmbutidos(q, nome, l);
}

void ConsultaAlvosDe(Consulta *q, const char *arg, ListaAlvos *alvos)
{
    const char *ponto = strrchr(arg, '.'); /* titulo de test pode ter '.', nome nunca */

    alvos->n = 0;
    if (ponto != NULL) AlvosComPonto(q, arg, ponto, alvos);
    else AlvosSemPonto(q, arg, alvos);
}

int ConsultarRefs(Consulta *q, const PedidoConsulta *p)
{
    ListaAlvos alvos;

    ConsultaAlvosDe(q, p->arg, &alvos);
    if (alvos.n == 0) {
        return ConsultaUsoErrado("refs", p->arg, "nao e declaracao do projeto (use nome, Tipo.campo, Enum.membro, proc.parametro ou um embutido)");
    }
    ConsultaAbrir(q, "refs");
    ConsultaChaveTexto(q, "nome", p->arg, strlen(p->arg));
    ConsultaAbrirLista(q, "alvos");
    for (uint32_t i = 0; i < alvos.n; i++) AlvoJson(q, &alvos.itens[i]);
    ConsultaFecharLista(q);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}
