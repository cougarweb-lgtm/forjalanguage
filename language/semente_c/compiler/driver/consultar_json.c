/* consultar_json.c - escrita da resposta JSON de 'forja consultar' e utilitarios comuns as perguntas (ver consultar_interno.h).
 * Versao: 0.1.4
 * Changelog: 0.1.4 - Linguagem v1.13 L1b: ConsultaItemCaminho (a lista "arquivos" do pacote).
 *            0.1.3 - M6d P92b: a assinatura mostra 'ends'.
 *0.1.2 - M6c P85: a assinatura mostra a clausula de posse do proc foreign (creates, destroys p, takes p).
 *0.1.1 - IA1-I3: ConsultaItemTexto e ConsultaItemModulo (elemento de lista que e so um texto, ou o caminho de um modulo).
 *            0.1.0 - IA1-I2: primeira versao. Uma linha de JSON por resposta, chaves na ordem de escrita (saida estavel); posicao [linha, coluna] local ao arquivo do modulo,
 *            como no 'forja ast'.
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#include <stdio.h>
#include <string.h>

#include "driver/consultar_interno.h"
#include "nativo/nativo_json.h"

enum { TRECHO_MAX = 160 };

static void Separar(Consulta *q)
{
    if (q->primeiro) q->primeiro = false;
    else BufAnexar(&q->saida, ", ");
}

static void Chave(Consulta *q, const char *chave)
{
    Separar(q);
    BufFormatar(&q->saida, "\"%s\": ", chave);
}

void ConsultaAbrir(Consulta *q, const char *pergunta)
{
    BufFormatar(&q->saida, "{\"consultar_versao\": \"%s\", \"pergunta\": \"%s\"", CONSULTAR_VERSAO, pergunta);
    q->primeiro = false;
}

void ConsultaFechar(Consulta *q)
{
    BufAnexar(&q->saida, "}\n");
}

void ConsultaAbrirObj(Consulta *q)
{
    Separar(q);
    BufAnexarN(&q->saida, "{", 1);
    q->primeiro = true;
}

void ConsultaFecharObj(Consulta *q)
{
    BufAnexarN(&q->saida, "}", 1);
    q->primeiro = false;
}

void ConsultaAbrirLista(Consulta *q, const char *chave)
{
    Chave(q, chave);
    BufAnexarN(&q->saida, "[", 1);
    q->primeiro = true;
}

void ConsultaFecharLista(Consulta *q)
{
    BufAnexarN(&q->saida, "]", 1);
    q->primeiro = false;
}

void ConsultaChaveTexto(Consulta *q, const char *chave, const char *texto, size_t n)
{
    Chave(q, chave);
    if (texto == NULL) BufAnexar(&q->saida, "null");
    else JsonTexto(&q->saida, texto, n);
}

void ConsultaChaveNome(Consulta *q, const char *chave, int32_t nome)
{
    const Interner *nomes = &q->c->nomes;
    if (nome < 0) ConsultaChaveTexto(q, chave, NULL, 0);
    else ConsultaChaveTexto(q, chave, InternerTexto(nomes, nome), InternerTam(nomes, nome));
}

void ConsultaChaveTipo(Consulta *q, const char *chave, TipoId tipo)
{
    Chave(q, chave);
    if (tipo == TIPO_NAO_RESOLVIDO) {
        BufAnexar(&q->saida, "null");
        return;
    }
    Buf texto = {0};
    TipoAnexarTexto(&q->c->sem.tipos, tipo, &texto);
    JsonTexto(&q->saida, texto.dados != NULL ? texto.dados : "", texto.tam);
    BufLiberar(&texto);
}

void ConsultaItemTexto(Consulta *q, const char *texto, size_t n)
{
    Separar(q);
    JsonTexto(&q->saida, texto, n);
}

void ConsultaItemCaminho(Consulta *q, const char *caminho)
{
    Separar(q);
    JsonCaminho(&q->saida, caminho);
}

void ConsultaItemModulo(Consulta *q, uint8_t modulo)
{
    int32_t caminho = q->c->modulos.itens[modulo].caminho;
    ConsultaItemTexto(q, InternerTexto(&q->c->nomes, caminho), InternerTam(&q->c->nomes, caminho));
}

void ConsultaChaveBool(Consulta *q, const char *chave, bool valor)
{
    Chave(q, chave);
    BufAnexar(&q->saida, valor ? "true" : "false");
}

void ConsultaChaveInt(Consulta *q, const char *chave, int64_t valor)
{
    Chave(q, chave);
    BufAnexarI64(&q->saida, valor);
}

void ConsultaChavePos(Consulta *q, const char *chave, uint32_t off)
{
    FontePosicao p = FonteLocalizarNoArquivo(&q->c->fonte, off);
    Chave(q, chave);
    BufFormatar(&q->saida, "[%u, %u]", p.linha, p.coluna);
}

void ConsultaChaveArquivo(Consulta *q, uint32_t off)
{
    FontePosicao p = FonteLocalizarNoArquivo(&q->c->fonte, off);
    Chave(q, "arquivo");
    JsonCaminho(&q->saida, p.caminho);
}

void ConsultaChaveId(Consulta *q, const char *chave, const Decl *d)
{
    Buf id = {0};
    AstIdBaseDaDecl(&id, &q->c->modulos, &q->c->nomes, d);
    ConsultaChaveTexto(q, chave, id.dados, id.tam);
    BufLiberar(&id);
}

void ConsultaChaveModulo(Consulta *q, const char *chave, uint8_t modulo)
{
    int32_t caminho = q->c->modulos.itens[modulo].caminho;
    ConsultaChaveTexto(q, chave, InternerTexto(&q->c->nomes, caminho), InternerTam(&q->c->nomes, caminho));
}

void ConsultaChaveTrecho(Consulta *q, const char *chave, uint32_t ini, uint32_t fim)
{
    size_t n = fim > ini ? fim - ini : 0;
    if (ini > q->c->fonte.tam) n = 0;
    else if (n > q->c->fonte.tam - ini) n = q->c->fonte.tam - ini;
    ConsultaChaveTexto(q, chave, q->c->fonte.texto + ini, n < TRECHO_MAX ? n : TRECHO_MAX);
}

int ConsultaUsoErrado(const char *pergunta, const char *argumento, const char *motivo)
{
    fprintf(stderr, "forja: consultar %s: '%s' %s\n", pergunta, argumento, motivo);
    return CONSULTAR_USO;
}

/* "<modulo>:test:<titulo>": o test com esse id (o titulo pode ter ':'). */
static const Decl *AcharTest(Consulta *q, const char *texto)
{
    const Decl *achado = NULL;

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL && achado == NULL; d = d->prox) {
        Buf id = {0};
        if (d->tipo_no != D_TEST) continue;
        AstIdBaseDaDecl(&id, &q->c->modulos, &q->c->nomes, d);
        if (id.tam == strlen(texto) && memcmp(id.dados, texto, id.tam) == 0) achado = d;
        BufLiberar(&id);
    }
    return achado;
}

/* "<caminho do modulo>:<nome>": o nome (id) declarado no modulo cujo caminho e o prefixo de n_modulo bytes de 'texto'. IA1-L8: o mesmo nome pode ser de varios modulos. */
static const Decl *AcharDeclNoModulo(Consulta *q, int32_t id, const char *texto, size_t n_modulo)
{
    for (uint32_t k = 0; k < q->c->modulos.n; k++) {
        int32_t caminho = q->c->modulos.itens[k].caminho;
        if (InternerTam(&q->c->nomes, caminho) != n_modulo || memcmp(InternerTexto(&q->c->nomes, caminho), texto, n_modulo) != 0) continue;
        const Decl *d = IndiceDeclDoNome(&q->indice, id, (uint8_t)k);
        return d != NULL && d->modulo == k ? d : NULL;
    }
    return NULL;
}

const Decl *ConsultaAcharDecl(Consulta *q, const char *texto)
{
    const char *dois_pontos = strrchr(texto, ':');
    const Decl *test = strstr(texto, ":test:") != NULL ? AcharTest(q, texto) : NULL;

    if (test != NULL) return test;
    const char *nome = dois_pontos != NULL ? dois_pontos + 1 : texto;
    int32_t id = InternerAchar(&q->c->nomes, nome, (uint32_t)strlen(nome));
    if (id < 0) return NULL;
    if (dois_pontos == NULL) return IndiceDeclDoNome(&q->indice, id, 0);
    return AcharDeclNoModulo(q, id, texto, (size_t)(dois_pontos - texto));
}

uint32_t ConsultaInicioDecl(const Consulta *q, const Decl *d)
{
    return AstInicioDecl(&q->fim, d);
}

/* Trecho do fonte [ini, fim) como texto de uma linha (quebras viram espaco). */
static void AnexarTrecho(const Consulta *q, Buf *saida, uint32_t ini, uint32_t fim)
{
    for (uint32_t k = ini; k < fim && k < q->c->fonte.tam; k++) {
        char ch = q->c->fonte.texto[k];
        BufAnexarN(saida, (ch == '\n' || ch == '\r') ? " " : &ch, 1);
    }
}

static void AssinaturaParametros(Consulta *q, const Decl *d, Buf *s)
{
    const Interner *nomes = &q->c->nomes;

    for (uint32_t i = 0; i < d->nparams; i++) {
        const Param *p = &d->params[i];
        if (i > 0) BufAnexar(s, ", ");
        BufFormatar(s, "%.*s: ", (int)InternerTam(nomes, p->nome), InternerTexto(nomes, p->nome));
        TipoAnexarTexto(&q->c->sem.tipos, p->tipo.tipo, s);
        if (p->padrao == NULL) continue;
        BufAnexar(s, " = ");
        AnexarTrecho(q, s, p->padrao->pos, AstFimExpr(&q->fim, p->padrao));
    }
}

void ConsultaAssinaturaTexto(Consulta *q, const Decl *d, Buf *s)
{
    const Interner *nomes = &q->c->nomes;

    BufFormatar(s, "proc %.*s(", (int)InternerTam(nomes, d->nome), InternerTexto(nomes, d->nome));
    AssinaturaParametros(q, d, s);
    BufAnexarN(s, ")", 1);
    if (d->tem_retorno) {
        BufAnexar(s, " -> ");
        TipoAnexarTexto(&q->c->sem.tipos, d->retorno.tipo, s);
    }
    if (d->posse_c == POSSE_C_NENHUMA) return;
    /* M6c P85: clausula de posse do proc foreign (creates, creates p, destroys p, takes p) */
    static const char *const PALAVRAS_POSSE[] = {"", "creates", "creates", "destroys", "takes", "ends"};
    BufFormatar(s, " %s", PALAVRAS_POSSE[d->posse_c]);
    if (d->posse_param < 0) return;
    const Param *alvo = &d->params[d->posse_param];
    BufFormatar(s, " %.*s", (int)InternerTam(nomes, alvo->nome), InternerTexto(nomes, alvo->nome));
}
