/* consultar_grafo.c - 'forja consultar callers | callees': quem chama um proc e quem um proc ou test chama (ver consultar_interno.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I3: primeira versao. As chamadas sao os usos do indice (frontend\indice.c) marcados como chamada: a mesma ligacao do 'refs' e do "ref" do 'forja ast'.
 *            callers lista cada chamada (quem, onde) e os chamadores sem repeticao; callees agrupa por destino (proc, foreign ou embutido) na ordem da 1a chamada.
 * Verificacao: construir.bat && python tests\consultar\verificar_mapa.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/consultar_interno.h"

/* ---------- callers ---------- */

static bool ChamaProc(const IndUso *u, const Decl *proc)
{
    return u->chamada && u->alvo_tipo == IND_PROC && u->alvo == proc;
}

static void ChamadaJson(Consulta *q, const IndUso *u)
{
    ConsultaAbrirObj(q);
    if (u->dono != NULL) ConsultaChaveId(q, "em", u->dono);
    else ConsultaChaveTexto(q, "em", NULL, 0);
    if (u->dono != NULL) ConsultaChaveTexto(q, "categoria", IndiceCategoria(u->dono), strlen(IndiceCategoria(u->dono)));
    ConsultaChaveArquivo(q, u->pos);
    ConsultaChavePos(q, "pos", u->pos);
    ConsultaChavePos(q, "pos_nome", u->pos_nome);
    ConsultaFecharObj(q);
}

/* Chamadores sem repeticao, na ordem do fonte: os usos de um mesmo dono sao contiguos (as declaracoes nao se sobrepoem). */
static void ChamadoresJson(Consulta *q, const Decl *proc)
{
    const Decl *ultimo = NULL;

    ConsultaAbrirLista(q, "chamadores");
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        Buf id = {0};
        if (!ChamaProc(u, proc) || u->dono == NULL || u->dono == ultimo) continue;
        ultimo = u->dono;
        AstIdBaseDaDecl(&id, &q->c->modulos, &q->c->nomes, u->dono);
        ConsultaItemTexto(q, id.dados, id.tam);
        BufLiberar(&id);
    }
    ConsultaFecharLista(q);
}

int ConsultarCallers(Consulta *q, const PedidoConsulta *p)
{
    const Decl *proc = ConsultaAcharDecl(q, p->arg);
    int64_t total = 0;

    if (proc == NULL) return ConsultaUsoErrado("callers", p->arg, "nao existe no projeto");
    if (proc->tipo_no != D_PROC) return ConsultaUsoErrado("callers", p->arg, "nao e proc (callers vale para proc e foreign)");
    ConsultaAbrir(q, "callers");
    ConsultaChaveNome(q, "nome", proc->nome);
    ConsultaChaveId(q, "id", proc);
    ConsultaAbrirLista(q, "chamadas");
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        if (!ChamaProc(u, proc)) continue;
        ChamadaJson(q, u);
        total++;
    }
    ConsultaFecharLista(q);
    ChamadoresJson(q, proc);
    ConsultaChaveInt(q, "total", total);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}

/* ---------- callees ---------- */

typedef struct {
    const Decl *alvo; /* NULL = embutido */
    int32_t nome;
    uint32_t pos;     /* pos_nome da 1a chamada */
    uint32_t chamadas;
} Chamado;

static bool ChamadaDeDentro(const IndUso *u, const Decl *dono)
{
    return u->chamada && u->dono == dono && (u->alvo_tipo == IND_PROC || u->alvo_tipo == IND_EMBUTIDO);
}

/* Soma a chamada ao destino que ja existe ou abre um novo. */
static void SomarChamado(Vec *chamados, const IndUso *u)
{
    const Decl *alvo = u->alvo_tipo == IND_PROC ? u->alvo : NULL;
    Chamado *novo;

    for (size_t i = 0; i < chamados->n; i++) {
        Chamado *c = VecEm(chamados, i);
        if (c->alvo == alvo && (alvo != NULL || c->nome == u->nome)) {
            c->chamadas++;
            return;
        }
    }
    novo = VecEmpurrar(chamados);
    *novo = (Chamado){.alvo = alvo, .nome = u->nome, .pos = u->pos_nome, .chamadas = 1};
}

static void ChamadoJson(Consulta *q, const Chamado *c)
{
    const char *categoria = c->alvo != NULL ? IndiceCategoria(c->alvo) : "embutido";

    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", c->nome);
    ConsultaChaveTexto(q, "categoria", categoria, strlen(categoria));
    if (c->alvo != NULL) ConsultaChaveId(q, "id", c->alvo);
    else ConsultaChaveTexto(q, "id", NULL, 0);
    ConsultaChaveInt(q, "chamadas", c->chamadas);
    ConsultaChavePos(q, "pos", c->pos);
    ConsultaFecharObj(q);
}

int ConsultarCallees(Consulta *q, const PedidoConsulta *p)
{
    const Decl *dono = ConsultaAcharDecl(q, p->arg);
    Vec chamados;

    if (dono == NULL) return ConsultaUsoErrado("callees", p->arg, "nao existe no projeto");
    if (dono->tipo_no != D_PROC && dono->tipo_no != D_TEST) return ConsultaUsoErrado("callees", p->arg, "nao e proc nem test");
    VecIniciar(&chamados, sizeof(Chamado));
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        if (ChamadaDeDentro(u, dono)) SomarChamado(&chamados, u);
    }
    ConsultaAbrir(q, "callees");
    ConsultaChaveNome(q, "nome", dono->nome);
    ConsultaChaveId(q, "id", dono);
    ConsultaAbrirLista(q, "chamados");
    for (size_t i = 0; i < chamados.n; i++) ChamadoJson(q, VecEm(&chamados, i));
    ConsultaFecharLista(q);
    ConsultaChaveInt(q, "total", (int64_t)chamados.n);
    ConsultaFechar(q);
    VecLiberar(&chamados);
    return CONSULTAR_OK;
}
