/* editar_renomear.c - a operacao 'renomear' do 'forja editar': a declaracao e TODOS os usos que o indice semantico acha no projeto (ver editar_interno.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I4: primeira versao. O resolvedor de nomes e o do 'forja consultar refs' (ConsultaAlvosDe); aqui entram so a posicao do nome na declaracao e dois usos que o indice
 *            nao guarda: o rotulo "nome = valor" de argumento nomeado (parametro) e de campo em literal de struct (campo). Nome dentro de string ou de comentario nunca muda.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/editar_interno.h"
#include "frontend/token.h"

enum { SEM_TOKEN = UINT32_MAX, DECL_TXT_MAX = 512 };

static const Token *TokenEm(const Consulta *q, uint32_t i)
{
    return (const Token *)VecEm(&q->fim.toks, i);
}

static uint32_t ParEm(const Consulta *q, uint32_t i)
{
    return *(const uint32_t *)VecEm(&q->fim.par, i);
}

/* Primeiro token com off >= 'off' (toks.n se nao ha). */
static uint32_t TokenDepois(const Consulta *q, uint32_t off)
{
    uint32_t lo = 0, hi = (uint32_t)q->fim.toks.n;

    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;

        if (TokenEm(q, meio)->off < off) lo = meio + 1;
        else hi = meio;
    }
    return lo;
}

static uint32_t TokenExato(const Consulta *q, uint32_t off)
{
    uint32_t i = TokenDepois(q, off);

    return i < q->fim.toks.n && TokenEm(q, i)->off == off ? i : SEM_TOKEN;
}

static bool NomeValido(const char *nome)
{
    if (nome[0] == '\0' || (nome[0] >= '0' && nome[0] <= '9')) return false;
    for (const char *c = nome; *c != '\0'; c++) {
        bool letra = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || (unsigned char)*c >= 0x80;

        if (!letra) return false;
    }
    return true;
}

/* ---------- o alvo ---------- */

typedef struct { const char *marca; IndAlvo tipo; } MarcaDeId;

static const MarcaDeId MARCAS_DE_ID[] = {{"/campos[", IND_CAMPO}, {"/membros[", IND_MEMBRO}, {"/params[", IND_PARAM}};

/* Quantos itens do tipo tem a declaracao e o nome do item i. */
static uint32_t TotalDoTipo(const Decl *d, IndAlvo tipo)
{
    if (tipo == IND_CAMPO) return d->tipo_no == D_ENUM ? 0 : d->ncampos;
    if (tipo == IND_MEMBRO) return d->tipo_no == D_ENUM ? d->nmembros : 0;
    return d->nparams;
}

static int32_t NomeDoItem(const Decl *d, IndAlvo tipo, uint32_t i)
{
    if (tipo == IND_CAMPO) return d->campos[i].nome;
    if (tipo == IND_MEMBRO) return d->membros[i].nome;
    return d->params[i].nome;
}

/* "<decl>/campos[i]", "<decl>/membros[i]" ou "<decl>/params[i]" (o "id" de 'consultar refs'). false = o texto nao tem essa forma; true com l->n == 0 = tem e nao existe. */
static bool AlvoPorMarca(Consulta *q, const char *texto, ListaAlvos *l)
{
    for (size_t k = 0; k < sizeof MARCAS_DE_ID / sizeof MARCAS_DE_ID[0]; k++) {
        const char *marca = strstr(texto, MARCAS_DE_ID[k].marca);
        char decl[DECL_TXT_MAX];
        const Decl *d;
        uint32_t indice;

        if (marca == NULL) continue;
        if ((size_t)(marca - texto) >= sizeof decl) return true;
        memcpy(decl, texto, (size_t)(marca - texto));
        decl[marca - texto] = '\0';
        d = ConsultaAcharDecl(q, decl);
        indice = (uint32_t)strtoul(marca + strlen(MARCAS_DE_ID[k].marca), NULL, 10);
        if (d == NULL || indice >= TotalDoTipo(d, MARCAS_DE_ID[k].tipo)) return true;
        l->itens[0] = (AlvoRefs){.tipo = MARCAS_DE_ID[k].tipo, .decl = d, .indice = (int32_t)indice, .nome = NomeDoItem(d, MARCAS_DE_ID[k].tipo, indice), .decl_pos = AST_REF_SEM_POS};
        l->n = 1;
        return true;
    }
    return false;
}

/* "<proc>/<nome>@linha:coluna" (o "id" de um local em 'consultar refs'). false = o texto nao tem essa forma. */
static bool AlvoLocalPorId(Consulta *q, const char *texto, ListaAlvos *l)
{
    const char *arroba = strchr(texto, '@'), *barra = arroba;
    char arg[DECL_TXT_MAX];
    ListaAlvos todos;
    char *fim;
    uint32_t linha, coluna;

    if (arroba == NULL) return false;
    while (barra > texto && *barra != '/') barra--;
    if (*barra != '/' || (size_t)(arroba - texto) >= sizeof arg) return true;
    snprintf(arg, sizeof arg, "%.*s.%.*s", (int)(barra - texto), texto, (int)(arroba - barra - 1), barra + 1);
    linha = (uint32_t)strtoul(arroba + 1, &fim, 10);
    coluna = *fim == ':' ? (uint32_t)strtoul(fim + 1, NULL, 10) : 0;
    ConsultaAlvosDe(q, arg, &todos);
    for (uint32_t k = 0; k < todos.n; k++) {
        FontePosicao pos = FonteLocalizarNoArquivo(&q->c->fonte, todos.itens[k].decl_pos);

        if (todos.itens[k].tipo == IND_LOCAL && pos.linha == linha && pos.coluna == coluna) l->itens[l->n++] = todos.itens[k];
    }
    return true;
}

/* Exatamente um alvo para o texto (nome como em 'consultar refs' ou o "id" dele). false = erro em p->erro. */
static bool ResolverAlvo(Plano *p, const char *texto, AlvoRefs *alvo)
{
    ListaAlvos l;

    l.n = 0;
    if (!AlvoLocalPorId(p->q, texto, &l) && !AlvoPorMarca(p->q, texto, &l)) ConsultaAlvosDe(p->q, texto, &l);
    if (l.n == 0) return PlanoErro(p, "'%s' nao e declaracao do projeto (use nome, Tipo.campo, Enum.membro, proc.parametro ou o id de 'forja consultar refs')", texto);
    if (l.n > 1) return PlanoErro(p, "'%s' designa %u alvos (locais de mesmo nome): use o id do 'forja consultar refs' (\"proc/nome@linha:coluna\")", texto, l.n);
    *alvo = l.itens[0];
    return true;
}

static bool NomeDaDeclEh(const Consulta *q, const Decl *d, const char *texto)
{
    return d->nome >= 0 && InternerTam(&q->c->nomes, d->nome) == strlen(texto) && memcmp(InternerTexto(&q->c->nomes, d->nome), texto, strlen(texto)) == 0;
}

/* O que o 'renomear' recusa: nome embutido, biblioteca padrao, test/import/link/static_assert, foreign (o nome e o simbolo C) e o main da raiz. */
static bool AlvoRenomeavel(Plano *p, const AlvoRefs *a)
{
    const Decl *d = a->decl;

    if (a->tipo == IND_EMBUTIDO || d == NULL) return PlanoErro(p, "e um nome embutido da linguagem: nao se renomeia");
    if (p->q->c->modulos.itens[d->modulo].origem == MOD_STD) return PlanoErro(p, "a declaracao e da biblioteca padrao embutida no forja: nao se renomeia");
    if (d->tipo_no == D_TEST || d->tipo_no == D_LINK || d->tipo_no == D_IMPORT || d->tipo_no == D_ASSERT) {
        return PlanoErro(p, "so se renomeia proc, struct, enum, union, const, unit, campo, membro, parametro e local");
    }
    if (d->externo) return PlanoErro(p, "a declaracao e de um bloco 'foreign': o nome dela e o simbolo C, nao se renomeia");
    if (a->tipo == IND_PROC && d->modulo == 0 && NomeDaDeclEh(p->q, d, "main")) return PlanoErro(p, "o main da raiz e a entrada do programa: nao se renomeia");
    return true;
}

/* Offset do nome do local: o 1o identificador com esse nome depois do no que o declara (let, var, for, switch...). */
static uint32_t OffsetDoLocal(const Consulta *q, const AlvoRefs *a)
{
    for (uint32_t i = TokenDepois(q, a->decl_pos); i < q->fim.toks.n; i++) {
        const Token *t = TokenEm(q, i);

        if (t->tipo == TK_IDENT && t->sym == a->nome) return t->off;
    }
    return AST_REF_SEM_POS;
}

/* Offset do nome do alvo na declaracao. */
static uint32_t OffsetNomeDoAlvo(const Consulta *q, const AlvoRefs *a)
{
    switch (a->tipo) {
    case IND_CAMPO: return a->decl->campos[a->indice].pos;
    case IND_MEMBRO: return a->decl->membros[a->indice].pos;
    case IND_PARAM: return a->decl->params[a->indice].pos;
    case IND_LOCAL: return OffsetDoLocal(q, a);
    default: return a->decl->pos;
    }
}

/* ---------- usos ---------- */

/* Em tipo qualificado (campo "centro: vet.Vetor") o indice guarda a posicao do 1o segmento: anda por "." ate o segmento que tem o nome do alvo. */
static uint32_t OffsetDoNomeNoUso(const Consulta *q, uint32_t pos, int32_t nome)
{
    uint32_t i = TokenExato(q, pos);

    if (i == SEM_TOKEN) return pos;
    while (TokenEm(q, i)->sym != nome && i + 2 < q->fim.toks.n && TokenEm(q, i + 1)->tipo == TK_DOT && TokenEm(q, i + 2)->tipo == TK_IDENT) i += 2;
    return TokenEm(q, i)->sym == nome ? TokenEm(q, i)->off : pos;
}

static bool TrocarUsos(Plano *p, const AlvoRefs *a, const char *novo)
{
    Consulta *q = p->q;
    size_t tam = InternerTam(&q->c->nomes, a->nome);

    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        uint32_t pos;

        if (!ConsultaUsoEhDoAlvo(u, a)) continue;
        pos = OffsetDoNomeNoUso(q, u->pos_nome, a->nome);
        if (!PlanoTrocarOffsets(p, pos, pos + (uint32_t)tam, novo)) return false;
    }
    return true;
}

/* O nome qualificado que termina no token i (a.b.nome) vem logo depois de '->' ou '!'? Entao e tipo de retorno ou de erro, e o '{' que segue e o corpo do proc. */
static bool EhTipoDeRetorno(const Consulta *q, uint32_t i)
{
    while (i >= 2 && TokenEm(q, i - 1)->tipo == TK_DOT && TokenEm(q, i - 2)->tipo == TK_IDENT) i -= 2;
    return i >= 1 && (TokenEm(q, i - 1)->tipo == TK_ARROW || TokenEm(q, i - 1)->tipo == TK_BANG);
}

/* Troca o rotulo "nome =" que esta direto (profundidade 1) no par aberto logo depois do nome em 'pos_nome': a chamada f(...) ou o literal T{...}.
 * LIMITE: parametros 5 (o plano, a posicao e o tipo de par dizem onde olhar; alvo e nome novo dizem o que trocar; dois chamadores, struct nao ganha clareza). */
static bool RotulosNoPar(Plano *p, uint32_t pos_nome, TokenTipo abre, const AlvoRefs *a, const char *novo)
{
    const Consulta *q = p->q;
    uint32_t nome = TokenExato(q, pos_nome), fecha;
    size_t tam = InternerTam(&q->c->nomes, a->nome);

    if (nome == SEM_TOKEN || nome + 1 >= q->fim.toks.n || TokenEm(q, nome + 1)->tipo != abre) return true;
    if (abre == TK_LBRACE && EhTipoDeRetorno(q, nome)) return true;
    fecha = ParEm(q, nome + 1);
    for (uint32_t j = nome + 2; fecha != AST_FIM_NENHUM && j < fecha; j++) {
        const Token *t = TokenEm(q, j);

        if (ParEm(q, j) != AST_FIM_NENHUM && ParEm(q, j) > j) {
            j = ParEm(q, j);
            continue;
        }
        if (t->tipo != TK_IDENT || t->sym != a->nome || TokenEm(q, j + 1)->tipo != TK_ASSIGN) continue;
        if (!PlanoTrocarOffsets(p, t->off, t->off + (uint32_t)tam, novo)) return false;
    }
    return true;
}

/* Parametro: o argumento nomeado das chamadas do proc. Campo: o rotulo dos literais da struct. O indice guarda a chamada e o literal, nao o rotulo. */
static bool TrocarRotulos(Plano *p, const AlvoRefs *a, const char *novo)
{
    Consulta *q = p->q;
    bool param = a->tipo == IND_PARAM;

    if (!param && a->tipo != IND_CAMPO) return true;
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        bool serve = param ? (u->alvo_tipo == IND_PROC && u->chamada) : (u->alvo_tipo == IND_STRUCT || u->alvo_tipo == IND_UNIAO);

        if (u->alvo != a->decl || !serve) continue;
        if (!RotulosNoPar(p, u->pos_nome, param ? TK_LPAREN : TK_LBRACE, a, novo)) return false;
    }
    return true;
}

bool EditarRenomear(Plano *p, const char *texto, const char *novo)
{
    AlvoRefs alvo;
    const char *antigo;
    uint32_t off;

    if (!NomeValido(novo)) return PlanoErro(p, "'%s' nao e um nome valido (letras, digitos e '_', sem comecar por digito)", novo);
    if (!ResolverAlvo(p, texto, &alvo) || !AlvoRenomeavel(p, &alvo)) return false;
    antigo = InternerTexto(&p->q->c->nomes, alvo.nome);
    if (InternerTam(&p->q->c->nomes, alvo.nome) == strlen(novo) && memcmp(antigo, novo, strlen(novo)) == 0) return PlanoErro(p, "o nome novo '%s' e igual ao atual", novo);
    off = OffsetNomeDoAlvo(p->q, &alvo);
    if (off == AST_REF_SEM_POS) return PlanoErro(p, "nao achei onde '%s' e declarado", texto);
    return PlanoTrocarOffsets(p, off, off + (uint32_t)InternerTam(&p->q->c->nomes, alvo.nome), novo) && TrocarUsos(p, &alvo, novo) && TrocarRotulos(p, &alvo, novo);
}
