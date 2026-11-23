/* consultar_decl.c - 'forja consultar simbolos | assinatura | campos': as perguntas que leem as declaracoes do projeto (ver consultar_interno.h).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - IA1-I3: EntradaDe vira ConsultaEntradaDe (a pergunta 'entrada' e o 'forja mapa' usam a mesma regra).
 *            0.1.0 - IA1-I2: primeira versao. simbolos = proc, foreign, struct, enum, union, const e unit de todos os modulos na ordem do programa (dependencias primeiro);
 *            assinatura = parametros com nome, tipo e padrao, retorno e erro; campos = campos de struct/union ou membros de enum.
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/consultar_interno.h"

bool ConsultaEhSimbolo(const Decl *d)
{
    switch ((DeclTipo)d->tipo_no) {
    case D_UNIDADE_BASE:
    case D_UNIDADE_DERIV:
    case D_CONST:
    case D_STRUCT:
    case D_ENUM:
    case D_PROC:
    case D_UNIAO: return true;
    default: return false;
    }
}

static bool ModuloCasa(const Consulta *q, const Decl *d, const char *modulo)
{
    int32_t caminho = q->c->modulos.itens[d->modulo].caminho;
    size_t n;

    if (modulo == NULL) return true;
    n = strlen(modulo);
    return InternerTam(&q->c->nomes, caminho) == n && memcmp(InternerTexto(&q->c->nomes, caminho), modulo, n) == 0;
}

static bool NomeEh(const Consulta *q, const Decl *d, const char *texto)
{
    return d->nome >= 0 && InternerTam(&q->c->nomes, d->nome) == strlen(texto) && memcmp(InternerTexto(&q->c->nomes, d->nome), texto, strlen(texto)) == 0;
}

const char *ConsultaEntradaDe(const Consulta *q, const Decl *d)
{
    if (d->tipo_no != D_PROC || d->externo) return NULL;
    if (d->exportado) return "export";
    return d->modulo == 0 && NomeEh(q, d, "main") ? "main" : NULL;
}

static void ChaveEntrada(Consulta *q, const Decl *d)
{
    const char *entrada = ConsultaEntradaDe(q, d);
    ConsultaChaveTexto(q, "entrada", entrada, entrada != NULL ? strlen(entrada) : 0);
}

static void ChaveLocalDaDecl(Consulta *q, const Decl *d)
{
    ConsultaChaveId(q, "id", d);
    ConsultaChaveModulo(q, "modulo", d->modulo);
    ConsultaChaveArquivo(q, d->pos);
    ConsultaChavePos(q, "pos", ConsultaInicioDecl(q, d));
    ConsultaChavePos(q, "pos_nome", d->pos);
    ConsultaChavePos(q, "fim", AstFimDecl(&q->fim, d));
    ConsultaChaveBool(q, "publico", d->publico);
    ConsultaChaveBool(q, "exportado", d->exportado);
}

static void SimboloJson(Consulta *q, const Decl *d)
{
    const char *categoria = IndiceCategoria(d);
    Buf assinatura = {0};

    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveTexto(q, "categoria", categoria, strlen(categoria));
    ChaveLocalDaDecl(q, d);
    ChaveEntrada(q, d);
    if (d->tipo_no == D_PROC) {
        ConsultaAssinaturaTexto(q, d, &assinatura);
        ConsultaChaveTexto(q, "assinatura", assinatura.dados, assinatura.tam);
        BufLiberar(&assinatura);
    } else if (d->tipo_no == D_CONST) {
        ConsultaChaveTipo(q, "tipo", d->tipo.tipo);
    }
    ConsultaFecharObj(q);
}

int ConsultarSimbolos(Consulta *q, const PedidoConsulta *p)
{
    bool achou_modulo = p->modulo == NULL;

    ConsultaAbrir(q, "simbolos");
    ConsultaAbrirLista(q, "simbolos");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (!ConsultaEhSimbolo(d) || !ModuloCasa(q, d, p->modulo)) continue;
        achou_modulo = true;
        SimboloJson(q, d);
    }
    ConsultaFecharLista(q);
    ConsultaFechar(q);
    if (!achou_modulo) return ConsultaUsoErrado("simbolos", p->modulo, "nao e modulo do projeto (use o caminho do import, ex.: std:mem)");
    return CONSULTAR_OK;
}

/* ---------- assinatura ---------- */

static void ChavePadrao(Consulta *q, const Expr *padrao)
{
    if (padrao == NULL) ConsultaChaveTexto(q, "padrao", NULL, 0);
    else ConsultaChaveTrecho(q, "padrao", padrao->pos, AstFimExpr(&q->fim, padrao));
}

static void ParametrosJson(Consulta *q, const Decl *d)
{
    ConsultaAbrirLista(q, "params");
    for (uint32_t i = 0; i < d->nparams; i++) {
        const Param *p = &d->params[i];
        ConsultaAbrirObj(q);
        ConsultaChaveNome(q, "nome", p->nome);
        ConsultaChaveTipo(q, "tipo", p->tipo.tipo);
        ChavePadrao(q, p->padrao);
        ConsultaChaveInt(q, "indice", i);
        ConsultaChavePos(q, "pos", p->pos);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

static void RetornosJson(Consulta *q, const Decl *d)
{
    bool multiplo = d->tem_retorno && d->retorno.forma == TR_TUPLA;

    ConsultaAbrirLista(q, "retornos");
    for (uint32_t i = 0; multiplo && i < d->nretornos; i++) {
        ConsultaAbrirObj(q);
        ConsultaChaveNome(q, "nome", d->retornos[i].nome);
        ConsultaChaveTipo(q, "tipo", d->retornos[i].tipo.tipo);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

int ConsultarAssinatura(Consulta *q, const PedidoConsulta *p)
{
    const Decl *d = ConsultaAcharDecl(q, p->arg);
    Buf texto = {0};

    if (d == NULL) return ConsultaUsoErrado("assinatura", p->arg, "nao existe no projeto");
    if (d->tipo_no != D_PROC) return ConsultaUsoErrado("assinatura", p->arg, "nao e proc (struct, union e enum: forja consultar campos)");
    ConsultaAbrir(q, "assinatura");
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveTexto(q, "categoria", IndiceCategoria(d), strlen(IndiceCategoria(d)));
    ChaveLocalDaDecl(q, d);
    ChaveEntrada(q, d);
    ConsultaChaveBool(q, "externo", d->externo);
    ParametrosJson(q, d);
    ConsultaChaveTipo(q, "retorno", d->tem_retorno ? d->retorno.tipo : TIPO_NAO_RESOLVIDO);
    ConsultaChaveNome(q, "erro", d->tem_retorno ? d->retorno.erro : -1);
    RetornosJson(q, d);
    ConsultaAssinaturaTexto(q, d, &texto);
    ConsultaChaveTexto(q, "texto", texto.dados, texto.tam);
    BufLiberar(&texto);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}

/* ---------- campos ---------- */

static void CamposJson(Consulta *q, const Decl *d)
{
    ConsultaAbrirLista(q, "campos");
    for (uint32_t i = 0; i < d->ncampos; i++) {
        const Campo *c = &d->campos[i];
        ConsultaAbrirObj(q);
        ConsultaChaveNome(q, "nome", c->nome);
        ConsultaChaveTipo(q, "tipo", c->tipo.tipo);
        ChavePadrao(q, c->padrao);
        ConsultaChaveInt(q, "indice", i);
        ConsultaChavePos(q, "pos", c->pos);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

static void MembrosJson(Consulta *q, const Decl *d)
{
    ConsultaAbrirLista(q, "membros");
    for (uint32_t i = 0; i < d->nmembros; i++) {
        const Membro *m = &d->membros[i];
        ConsultaAbrirObj(q);
        ConsultaChaveNome(q, "nome", m->nome);
        ConsultaChaveInt(q, "indice", i);
        if (d->op == 1) ConsultaChaveInt(q, "valor", m->valor);
        else ConsultaChaveTexto(q, "valor", NULL, 0);
        ConsultaChavePos(q, "pos", m->pos);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

int ConsultarCampos(Consulta *q, const PedidoConsulta *p)
{
    const Decl *d = ConsultaAcharDecl(q, p->arg);

    if (d == NULL) return ConsultaUsoErrado("campos", p->arg, "nao existe no projeto");
    if (d->tipo_no != D_STRUCT && d->tipo_no != D_UNIAO && d->tipo_no != D_ENUM) {
        return ConsultaUsoErrado("campos", p->arg, "nao e struct, union nem enum (campos vale so para esses)");
    }
    ConsultaAbrir(q, "campos");
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveTexto(q, "categoria", IndiceCategoria(d), strlen(IndiceCategoria(d)));
    ChaveLocalDaDecl(q, d);
    if (d->tipo_no == D_ENUM) MembrosJson(q, d);
    else CamposJson(q, d);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}
