/* editar_mover.c - a operacao 'mover_decl' do 'forja editar' (ver editar_interno.h): tira a declaracao de um modulo e a poe no fim de outro.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b (modulo de pasta): mover de ou para um pacote (pasta com 2+ arquivos), inclusive entre os arquivos do mesmo pacote, e RECUSADO com diagnostico (ModuloSemPacote); os imports e as trocas de texto
 *            valem por arquivo (origem_arq, destino_arq), entao um uso em arquivo de pacote (fora da origem e do destino) e ajustado no arquivo certo.
 *            0.1.1 - G3 G0c: Retalho.ordem (indice de insercao) desempata CompararRetalhos: ordem total.
 *            0.1.0 - IA1-I5: primeira versao. O texto que muda de modulo (comentario colado + declaracao) passa por retalhos de token: no destino, o que a declaracao usa do modulo de origem ganha
 *            "apelido." (e o que usa torna-se pub), o apelido de um terceiro modulo vira o que o destino ja usa (ou um import novo), e "apelido." do proprio destino some. Fora dela, cada uso vira
 *            "apelido-do-destino.nome" (o modulo de origem usa sem qualificar; o destino passa a usar sem qualificar) e a declaracao ganha 'pub' se algum modulo de fora do destino a usa.
 *            Os imports que ficam sem uso saem e os que faltam entram (editar_imports.c). Ciclo de import, raiz importada e apelido ocupado sao recusados; o resto o check da copia decide.
 * Verificacao: construir.bat && python tests\editar\verificar_editar_i5.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/editar_interno.h"

/* Um trecho do texto movido que muda: [ini, fim) (offsets globais) vira 'novo'. */
typedef struct { uint32_t ini, fim; const char *novo; uint32_t ordem; } Retalho; /* ordem: indice de insercao (desempate) */

typedef struct {
    Plano *p;
    const Decl *d;
    uint8_t origem, destino;
    uint32_t origem_arq, destino_arq;   /* o arquivo (trecho da Fonte) de cada um: o import vale por arquivo; origem e destino tem um arquivo so (pacote de 2+ arquivos e recusado) */
    FaixaDecl faixa;
    uint32_t ini_decl, fim_decl;    /* a declaracao propriamente dita: do 'pub' ao fim */
    Imports *imp;
    Retalho *retalhos;
    uint32_t nretalhos, cap;
    bool precisa_pub;               /* algum modulo de fora do destino a usa */
} Movimento;

static void AdicionarRetalho(Movimento *m, uint32_t ini, uint32_t fim, const char *novo)
{
    if (m->nretalhos == m->cap) {
        m->cap = m->cap == 0 ? 16 : m->cap * 2;
        m->retalhos = realloc(m->retalhos, m->cap * sizeof *m->retalhos);
        if (m->retalhos == NULL) FalhaInterna("memoria esgotada guardando os retalhos do mover_decl");
    }
    m->retalhos[m->nretalhos] = (Retalho){ini, fim, novo, m->nretalhos};
    m->nretalhos++;
}

/* "apelido." copiado na arena. */
static const char *ComPonto(Plano *p, const char *apelido)
{
    char texto[160];
    int n = snprintf(texto, sizeof texto, "%s.", apelido);

    return ArenaCopiarTexto(p->arena, texto, n < 0 ? 0 : (size_t)n);
}

/* ---------- a declaracao e o destino ---------- */

static bool DeclMovivel(Plano *p, const Decl *d)
{
    if (!ConsultaEhSimbolo(d)) return PlanoErro(p, "so se move proc, struct, enum, union, const e unit");
    if (d->externo) return PlanoErro(p, "a declaracao e de um bloco 'foreign': o bloco inteiro nao se move com mover_decl");
    if (ConsultaEntradaDe(p->q, d) != NULL) return PlanoErro(p, "a declaracao e ponto de entrada (main ou pub export): fica no arquivo raiz");
    return EditarModuloEditavel(p, d->modulo);
}

/* Mover de ou para um pacote (pasta com 2+ arquivos) e recusado: o import vale por arquivo e o "para" nao diz o arquivo; e o que a declaracao usa dos irmaos do pacote nao tem import para qualificar.
 * O 1o arquivo do modulo fica em *arquivo. false = erro em p->erro. */
static bool ModuloSemPacote(Plano *p, uint8_t modulo, const char *papel, uint32_t *arquivo)
{
    const Consulta *q = p->q;
    uint32_t n = ModulosTrechosDoPacote(&q->c->modulos, q->c->fonte.ntrechos, modulo, arquivo);

    if (n < 2) return true;
    return PlanoErro(p, "mover_decl nao move de nem para um pacote (pasta com %u arquivos): o modulo \"%.*s\" e o %s; mova a mao ou com inserir_decl e remover_decl",
                     n, (int)InternerTam(&q->c->nomes, q->c->modulos.itens[modulo].caminho), InternerTexto(&q->c->nomes, q->c->modulos.itens[modulo].caminho), papel);
}

static bool IniciarMovimento(Movimento *m, const char *para)
{
    Plano *p = m->p;
    int destino = EditarAcharModulo(p, para);

    if (destino < 0 || !EditarModuloEditavel(p, (uint8_t)destino)) return false;
    if (!ModuloSemPacote(p, m->d->modulo, "de origem", &m->origem_arq) || !ModuloSemPacote(p, (uint8_t)destino, "de destino", &m->destino_arq)) return false;
    if (destino == m->d->modulo) return PlanoErro(p, "a declaracao ja esta no modulo '%s'", para);
    m->origem = m->d->modulo;
    m->destino = (uint8_t)destino;
    m->ini_decl = ConsultaInicioDecl(p->q, m->d);
    m->fim_decl = AstFimDecl(&p->q->fim, m->d);
    return EditarFaixaDaDecl(p, m->d, &m->faixa);
}

/* ---------- 'pub' ---------- */

/* O que a declaracao movida usa de irmao dela tem de ser visivel do destino. */
static bool TornarPublico(Movimento *m, const Decl *alvo)
{
    uint32_t ini;

    if (alvo->publico) return true;
    if (alvo->externo) return PlanoErro(m->p, "a declaracao usa um proc de bloco 'foreign' que nao e pub: torne o bloco pub e tente de novo");
    ini = ConsultaInicioDecl(m->p->q, alvo);
    return EditarTrocarNoArquivo(m->p, ini, ini, "pub ");
}

/* ---------- usos de dentro da declaracao ---------- */

/* Uso de um irmao (mesmo modulo de origem, sem qualificar): no destino precisa de "apelido-da-origem.". */
static bool QualificarIrmao(Movimento *m, const IndUso *u, uint32_t token)
{
    const char *apelido = ImportsApelido(m->imp, m->destino_arq, m->origem, -1);
    uint32_t off = EditarToken(m->p->q, token)->off;

    if (apelido == NULL) return false;
    ImportsContar(m->imp, m->destino_arq, m->origem, 1);
    AdicionarRetalho(m, off, off, ComPonto(m->p, apelido));
    return TornarPublico(m, u->alvo);
}

/* Uso qualificado de um terceiro modulo k: o destino o chama pelo apelido que ja usa, ou por um import novo com o mesmo apelido de antes. */
static bool TrocarApelidoInterno(Movimento *m, uint32_t nome, uint8_t k)
{
    const Consulta *q = m->p->q;
    const Token *apelido = EditarToken(q, nome - 2);
    size_t tam = InternerTam(&q->c->nomes, apelido->sym);
    const char *novo = ImportsApelido(m->imp, m->destino_arq, k, apelido->sym);

    if (novo == NULL) return false;
    ImportsContar(m->imp, m->destino_arq, k, 1);
    if (strlen(novo) != tam || memcmp(novo, InternerTexto(&q->c->nomes, apelido->sym), tam) != 0) AdicionarRetalho(m, apelido->off, apelido->off + (uint32_t)tam, novo);
    return true;
}

static bool UsoInterno(Movimento *m, const IndUso *u, uint32_t token)
{
    const Consulta *q = m->p->q;
    uint8_t k = u->alvo->modulo;
    uint32_t nome;

    if (k == m->origem) return QualificarIrmao(m, u, token);
    if (!EditarUsoQualificado(q, u, &nome)) return true;
    if (k == m->destino) {
        AdicionarRetalho(m, EditarToken(q, nome - 2)->off, EditarToken(q, nome)->off, "");
        return true;
    }
    return TrocarApelidoInterno(m, nome, k);
}

/* ---------- usos de fora da declaracao ---------- */

/* Uso sem qualificar dentro do modulo de origem: passa a "apelido-do-destino.nome". */
static bool QualificarNaOrigem(Movimento *m, uint32_t token)
{
    const char *apelido = ImportsApelido(m->imp, m->origem_arq, m->destino, -1);
    uint32_t off = EditarToken(m->p->q, token)->off;

    if (apelido == NULL) return false;
    ImportsContar(m->imp, m->origem_arq, m->destino, 1);
    m->precisa_pub = true;
    return EditarTrocarNoArquivo(m->p, off, off, ComPonto(m->p, apelido));
}

/* Uso "apelido-da-origem.nome" no arquivo x (de outro modulo): vira "apelido-do-destino.nome", ou so "nome" se x e o proprio destino. */
static bool TrocarApelidoExterno(Movimento *m, uint32_t x, uint32_t nome)
{
    const Consulta *q = m->p->q;
    const Token *apelido = EditarToken(q, nome - 2);
    uint32_t fim_apelido = apelido->off + (uint32_t)InternerTam(&q->c->nomes, apelido->sym);
    const char *novo;

    ImportsContar(m->imp, x, m->origem, -1);
    if (ModulosPacoteDoArquivo(&q->c->modulos, x) == m->destino) return EditarTrocarNoArquivo(m->p, apelido->off, EditarToken(q, nome)->off, "");
    novo = ImportsApelido(m->imp, x, m->destino, -1);
    if (novo == NULL) return false;
    ImportsContar(m->imp, x, m->destino, 1);
    m->precisa_pub = true;
    return EditarTrocarNoArquivo(m->p, apelido->off, fim_apelido, novo);
}

static bool UsoExterno(Movimento *m, const IndUso *u, uint32_t token)
{
    uint8_t x = EditarModuloDoOffset(m->p->q, u->pos_nome);
    uint32_t nome;

    if (x == m->origem) return QualificarNaOrigem(m, token);
    if (!EditarModuloEditavel(m->p, x)) return false;
    if (!EditarUsoQualificado(m->p->q, u, &nome)) return true;
    return TrocarApelidoExterno(m, EditarArquivoDoOffset(m->p->q, u->pos_nome), nome);
}

/* Cada uso do indice: o de dentro da declaracao (que aponta para outra) e o de fora (que aponta para ela). */
static bool TratarUsos(Movimento *m)
{
    const Consulta *q = m->p->q;

    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        bool dentro = u->pos_nome >= m->ini_decl && u->pos_nome < m->fim_decl;
        uint32_t token = EditarUsoEhDeDecl(u) ? EditarTokenDoNome(q, u) : TOKEN_AUSENTE;

        if (token == TOKEN_AUSENTE) continue;
        if (u->alvo == m->d && !dentro && !UsoExterno(m, u, token)) return false;
        if (u->alvo != m->d && dentro && !UsoInterno(m, u, token)) return false;
    }
    return true;
}

/* ---------- o texto que muda de modulo ---------- */

static int CompararRetalhos(const void *x, const void *y)
{
    const Retalho *a = x, *b = y;

    if (a->ini != b->ini) return a->ini < b->ini ? -1 : 1;
    if (a->fim != b->fim) return a->fim < b->fim ? -1 : 1;
    return a->ordem < b->ordem ? -1 : (a->ordem > b->ordem ? 1 : 0);
}

/* Anexa o texto do fonte em [ini, fim) sem os CR (o destino poe o fim de linha dele). */
static void AnexarSemCr(Buf *saida, const char *texto, uint32_t ini, uint32_t fim)
{
    for (uint32_t k = ini; k < fim; k++) {
        if (texto[k] != '\r') BufAnexarN(saida, texto + k, 1);
    }
}

static void MontarTexto(const Movimento *m, Buf *saida)
{
    const char *texto = m->p->q->c->fonte.texto;
    uint32_t pos = m->faixa.ini_texto;

    qsort(m->retalhos, m->nretalhos, sizeof *m->retalhos, CompararRetalhos);
    for (uint32_t k = 0; k < m->nretalhos; k++) {
        AnexarSemCr(saida, texto, pos, m->retalhos[k].ini);
        BufAnexar(saida, m->retalhos[k].novo);
        pos = m->retalhos[k].fim;
    }
    AnexarSemCr(saida, texto, pos, m->faixa.fim_texto);
}

/* Tira a declaracao da origem, poe o texto novo no fim do destino e ajusta os imports. */
static bool GravarMovimento(Movimento *m)
{
    Buf texto = {0};
    bool ok;

    if (m->precisa_pub && !m->d->publico) AdicionarRetalho(m, m->ini_decl, m->ini_decl, "pub ");
    MontarTexto(m, &texto);
    ok = EditarTrocarNoArquivo(m->p, m->faixa.ini_remocao, m->faixa.fim_remocao, "") &&
         EditarInserirTexto(m->p, m->destino_arq, LUGAR_NO_FIM, NULL, texto.dados) && ImportsAplicar(m->imp);
    BufLiberar(&texto);
    return ok;
}

bool EditarMoverDecl(Plano *p, const char *id, const char *para)
{
    Movimento m = {.p = p, .d = ConsultaAcharDecl(p->q, id)};
    bool ok;

    if (m.d == NULL) return PlanoErro(p, "'%s' nao e declaracao do projeto (use o nome ou o id de 'forja consultar simbolos')", id);
    if (!DeclMovivel(p, m.d) || !IniciarMovimento(&m, para)) return false;
    m.imp = ImportsIniciar(p);
    ImportsDescontarDecl(m.imp, m.d);
    ok = TratarUsos(&m) && GravarMovimento(&m);
    ImportsLiberar(m.imp);
    free(m.retalhos);
    return ok;
}
