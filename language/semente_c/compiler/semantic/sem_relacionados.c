/* sem_relacionados.c - relacionados do pacote do erro (contrato IA1, secao 1.1 item 4, passo b14): o que a IA precisa ver para resolver o erro sem abrir o arquivo.
 * SemRelacionar roda DEPOIS do DiagRelatar do E0201, E0202, E0301, E0310, E0311, E0502, E0503, E0504, E0607 e E1003 (SemErro e SemErroParecido chamam) e acrescenta ao
 * Diag.rel: declaracao do nome (let/var/parametro/const), tipo (struct, enum, union, com os campos ou membros), assinatura (proc chamado ou local de tipo proc),
 * parecido (ranqueado por distancia, vindo do SemErroParecido) e uso_anterior (a 1a ocorrencia do nome repetido, E0202). Codigo sem relacionado util sai com a lista vazia.
 * Tudo sai do texto na posicao do erro mais as tabelas da semantica (escopo, globais, programa): nenhum ponto de erro precisa passar contexto novo.
 * Sem relacionado especifico, o fallback e a declaracao que CONTEM o erro (assinatura do proc ou a declaracao de topo), nunca invencao.
 * Uma linha do fonte aparece uma vez so (o texto do relacionado e a linha inteira).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1a: ImportDoApelido e ImportCitado olham so os imports do arquivo (trecho) em analise, nao do pacote inteiro.
 *            0.1.0 - IA1-b14: primeira versao.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_relacionados_b14.py --forja compiler\forja.exe
 */
#include "semantic/sem_interno.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { TIPO_TEXTO_MAX = 40, LISTA_MAX = 56, CITADOS_MAX = 3, ALCANCE_DA_CHAMADA = 600 };
#define SEM_POS 0xFFFFFFFFu /* offset "nao achado" (o offset 0 e um offset valido) */

static bool CharDeNome(char c)
{
    return isalnum((unsigned char)c) != 0 || c == '_';
}

static bool EhEspaco(char c)
{
    return c == ' ' || c == '\t';
}

static uint32_t PularEspacos(const Semantica *s, uint32_t i)
{
    while (EhEspaco(s->fonte->texto[i])) i++;
    return i;
}

/* Id do identificador inteiro escrito em texto[pos..] (fim recebe o byte depois dele); -1 se nao e identificador ou o nome nunca foi internado. */
static int32_t NomeEm(const Semantica *s, uint32_t pos, uint32_t *fim)
{
    const char *t = s->fonte->texto;
    uint32_t i = pos;

    if (pos >= s->fonte->tam || !(isalpha((unsigned char)t[pos]) || t[pos] == '_')) return -1;
    while (CharDeNome(t[i])) i++;
    if (fim != NULL) *fim = i;
    return InternerAchar(s->nomes, t + pos, i - pos);
}

/* Pula os prefixos de tipo/endereco (^T, ?^T, &x): o nome que importa vem depois. */
static uint32_t PularPrefixos(const Semantica *s, uint32_t pos)
{
    while (strchr("^?&", s->fonte->texto[pos]) != NULL && s->fonte->texto[pos] != '\0') pos++;
    return pos;
}

/* Inicio da cadeia 'a.b.c' (so nomes e pontos) que termina em 'fim' (exclusivo, depois de espacos); fim se nao ha cadeia. */
static uint32_t InicioDaCadeiaAntes(const Semantica *s, uint32_t fim)
{
    const char *t = s->fonte->texto;
    uint32_t ini;

    while (fim > 0 && EhEspaco(t[fim - 1])) fim--;
    ini = fim;
    while (ini > 0 && (CharDeNome(t[ini - 1]) || t[ini - 1] == '.')) ini--;
    return ini;
}

/* Raiz da cadeia 'a.b.c' que termina antes de 'fim': o id de 'a'; -1 se nao ha cadeia. */
static int32_t RaizDaCadeiaAntes(const Semantica *s, uint32_t fim)
{
    uint32_t ini = InicioDaCadeiaAntes(s, fim);

    return ini < fim ? NomeEm(s, ini, NULL) : -1;
}

/* Ultimo nome da cadeia 'a.b.c' que termina antes de 'fim': o id de 'c' (o proc de 'geo.soma('); -1 se nao ha. */
static int32_t UltimoNomeAntes(const Semantica *s, uint32_t fim)
{
    const char *t = s->fonte->texto;
    uint32_t ini;

    while (fim > 0 && EhEspaco(t[fim - 1])) fim--;
    ini = fim;
    while (ini > 0 && CharDeNome(t[ini - 1])) ini--;
    return ini < fim ? NomeEm(s, ini, NULL) : -1;
}

static const Simbolo *SimboloDe(const Semantica *s, int32_t nome)
{
    return nome >= 0 && (uint32_t)nome < s->qtd_nomes ? SemAcharSimbolo(s, nome, 0) : NULL; /* IA1-L8: o simbolo do modulo em analise */
}

/* Nome do operando da direita do operador que comeca em 'pos': pula o operador e os espacos. */
static int32_t NomeDepoisDoOperador(const Semantica *s, uint32_t pos)
{
    const char *t = s->fonte->texto;

    while (t[pos] != '\0' && strchr("+-*/%<>=!&|~^. \t", t[pos]) != NULL) pos++;
    return NomeEm(s, pos, NULL);
}

static uint32_t InicioDaLinha(const Semantica *s, uint32_t off)
{
    while (off > 0 && s->fonte->texto[off - 1] != '\n') off--;
    return off;
}

/* Acrescenta o relacionado, exceto se a linha dele ja esta na lista (o texto do relacionado e a linha inteira). */
static void Adicionar(Semantica *s, DiagRelPapel papel, uint32_t off, const char *fmt, ...) __attribute__((format(gnu_printf, 4, 5)));

static void Adicionar(Semantica *s, DiagRelPapel papel, uint32_t off, const char *fmt, ...)
{
    char nota[DIAG_REL_NOTA_MAX];
    uint32_t linha = InicioDaLinha(s, off);
    va_list ap;

    for (uint8_t k = 0; k < s->diag->rel.n; k++) {
        if (InicioDaLinha(s, s->diag->rel.itens[k].off) == linha) return;
    }
    va_start(ap, fmt);
    vsnprintf(nota, sizeof nota, fmt, ap);
    va_end(ap);
    DiagRelacionar(s->diag, papel, off, "%s", nota);
}

/* ---------- declaracoes ---------- */

/* Nome do k-esimo membro (enum), campo (struct) ou variante (union, pelo tipo) da declaracao. */
static void NomeDoItem(const Semantica *s, const Decl *d, uint32_t k, char *saida, size_t cap)
{
    if (d->tipo_no == D_ENUM) snprintf(saida, cap, "%s", SemNomeTexto(s, d->membros[k].nome));
    else if (d->tipo_no == D_UNIAO) SemDescrever(s, d->campos[k].tipo.tipo, saida, cap);
    else snprintf(saida, cap, "%s", SemNomeTexto(s, d->campos[k].nome));
}

/* "a, b, c" com os itens da declaracao, cortado com "..." se passar de LISTA_MAX bytes. */
static void ListarItens(const Semantica *s, const Decl *d, char *saida)
{
    uint32_t qtd = d->tipo_no == D_ENUM ? d->nmembros : d->ncampos;
    size_t usado = 0;

    saida[0] = '\0';
    for (uint32_t k = 0; k < qtd; k++) {
        char item[TIPO_TEXTO_MAX];
        NomeDoItem(s, d, k, item, sizeof item);
        if (usado + strlen(item) + 5 > LISTA_MAX) {
            snprintf(saida + usado, LISTA_MAX - usado, "...");
            return;
        }
        usado += (size_t)snprintf(saida + usado, LISTA_MAX - usado, "%s%s", k > 0 ? ", " : "", item);
    }
}

static const char *RotuloDoTipo(const Decl *d)
{
    switch (d->tipo_no) {
    case D_STRUCT: return "struct";
    case D_ENUM: return "enum";
    case D_UNIAO: return "union";
    default: return "unidade";
    }
}

/* Tipo declarado pelo programa (struct, enum, union): a linha de declaracao e a lista dos campos, membros ou variantes. */
static void RelacionarTipoDecl(Semantica *s, const Decl *d)
{
    char itens[LISTA_MAX + 1];
    const char *sem_pub = d->modulo != s->modulo_atual && !d->publico ? " (sem pub)" : "";

    ListarItens(s, d, itens);
    Adicionar(s, DIAG_REL_TIPO, d->pos, "%s '%s'%s%s%s", RotuloDoTipo(d), SemNomeTexto(s, d->nome), itens[0] != '\0' ? ": " : "", itens, sem_pub);
}

/* Nome global com declaracao: proc vira assinatura, tipo vira tipo, const vira declaracao. false = sem declaracao (embutido ou inexistente). */
static bool RelacionarSimbolo(Semantica *s, int32_t nome)
{
    const Simbolo *y = SimboloDe(s, nome);
    const Decl *d = y != NULL ? y->decl : NULL;
    const char *sem_pub;

    if (d == NULL) return false;
    sem_pub = d->modulo != s->modulo_atual && !d->publico ? " (sem pub)" : "";
    switch (y->categoria) {
    case SIM_PROC:
        Adicionar(s, DIAG_REL_ASSINATURA, d->pos, "assinatura do proc '%s'%s", SemNomeTexto(s, nome), sem_pub);
        return true;
    case SIM_STRUCT: case SIM_ENUM: case SIM_UNIAO: case SIM_UNIDADE:
        RelacionarTipoDecl(s, d);
        return true;
    case SIM_CONST:
        Adicionar(s, DIAG_REL_DECLARACAO, d->pos, "constante '%s'%s", SemNomeTexto(s, nome), sem_pub);
        return true;
    default: return false;
    }
}

static void RelacionarLocal(Semantica *s, const LocalInfo *l)
{
    char tipo[TIPO_TEXTO_MAX];
    DiagRelPapel papel = TipoClasseDe(&s->tipos, l->tipo) == TC_PROC ? DIAG_REL_ASSINATURA : DIAG_REL_DECLARACAO;

    SemDescrever(s, l->tipo, tipo, sizeof tipo);
    Adicionar(s, papel, l->pos, "%s '%s' declarado aqui como %s", l->natureza == LOC_PARAM ? "parametro" : "local", SemNomeTexto(s, l->nome), tipo);
}

/* Declaracao do nome visivel: o local (de um proc) tem prioridade, depois o global. false = nada a mostrar. */
static bool RelacionarNome(Semantica *s, int32_t nome)
{
    const LocalInfo *l;

    if (SimboloDe(s, nome) == NULL) return false;
    l = EscopoBuscar(&s->escopo, nome);
    if (l != NULL) {
        RelacionarLocal(s, l);
        return true;
    }
    return RelacionarSimbolo(s, nome);
}

/* ---------- onde o erro mora ---------- */

/* Declaracao de topo que contem o offset (o proc, a struct, o enum...); a const nao guarda o fim, entao sem contenedor vale a declaracao mais proxima antes do offset. */
static const Decl *DeclQueContem(const Semantica *s, uint32_t off)
{
    const Decl *anterior = NULL;

    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_IMPORT || d->tipo_no == D_LINK) continue;
        if (d->pos <= off && off < d->pos_fim) return d;
        if (d->pos <= off && (anterior == NULL || d->pos > anterior->pos)) anterior = d;
    }
    return anterior;
}

/* Sem relacionado especifico: a declaracao que contem o erro (assinatura do proc com parametros e retorno, ou a linha da declaracao de topo). */
static void RelacionarUnidade(Semantica *s)
{
    const Decl *d = DeclQueContem(s, s->diag->off);

    if (d == NULL) return;
    if (d->tipo_no == D_PROC || d->tipo_no == D_TEST) Adicionar(s, DIAG_REL_ASSINATURA, d->pos, "proc que contem o erro");
    else Adicionar(s, DIAG_REL_DECLARACAO, d->pos, "declaracao que contem o erro");
}

/* Nomes entre aspas simples na causa ('Cor', 'azul'), so os que o programa internou. */
static uint32_t NomesCitados(const Semantica *s, int32_t *saida, uint32_t max)
{
    const char *c = s->diag->causa;
    uint32_t n = 0;

    while (n < max && (c = strchr(c, '\'')) != NULL) {
        const char *ini = ++c;
        while (CharDeNome(*c)) c++;
        if (*c != '\'' || c == ini) continue;
        int32_t id = InternerAchar(s->nomes, ini, (uint32_t)(c - ini));
        if (id >= 0) saida[n++] = id;
        c++;
    }
    return n;
}

static void RelacionarCitados(Semantica *s)
{
    int32_t citados[CITADOS_MAX];
    uint32_t n = NomesCitados(s, citados, CITADOS_MAX);

    for (uint32_t k = 0; k < n; k++) RelacionarNome(s, citados[k]);
}

/* ---------- E0201: nome, campo ou membro que nao existe ---------- */

/* 'base.nome' com 'nome' em pos: o tipo (struct/enum/union) ou a variavel da base, e a struct da variavel. */
static void RelacionarBase(Semantica *s, int32_t base)
{
    const LocalInfo *l = SimboloDe(s, base) != NULL ? EscopoBuscar(&s->escopo, base) : NULL;
    const Decl *dono = l != NULL ? SemDeclDaStruct(s, l->tipo) : NULL;

    if (!RelacionarNome(s, base)) return;
    if (dono != NULL) RelacionarTipoDecl(s, dono);
}

static void RelE0201(Semantica *s)
{
    uint32_t pos = s->diag->off;
    int32_t nome;

    if (pos > 0 && s->fonte->texto[pos - 1] == '.') {
        RelacionarBase(s, RaizDaCadeiaAntes(s, pos - 1));
        return;
    }
    nome = NomeEm(s, PularPrefixos(s, pos), NULL);
    if (nome >= 0) RelacionarNome(s, nome);
}

/* ---------- E0202: nome repetido ---------- */

/* Campo, membro ou parametro de 'd' com esse nome em outro offset que nao o do erro. */
static uint32_t PosDoItemRepetido(const Decl *d, int32_t nome, uint32_t off)
{
    for (uint32_t k = 0; k < d->nmembros; k++) {
        if (d->membros[k].nome == nome && d->membros[k].pos != off) return d->membros[k].pos;
    }
    for (uint32_t k = 0; k < d->ncampos; k++) {
        if (d->campos[k].nome == nome && d->campos[k].pos != off) return d->campos[k].pos;
    }
    for (uint32_t k = 0; k < d->nparams; k++) {
        if (d->params[k].nome == nome && d->params[k].pos != off) return d->params[k].pos;
    }
    return SEM_POS;
}

/* Proc foreign cujo simbolo C (alias) e o nome, em outro offset que nao o do erro. */
static const Decl *DeclDoAlias(const Semantica *s, int32_t nome, uint32_t off)
{
    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_PROC && d->externo && d->simbolo == nome && d->pos_simbolo != off && d->pos != off) return d;
    }
    return NULL;
}

/* Import deste arquivo cujo apelido (escrito ou o ultimo segmento do caminho) e o nome. */
static const Decl *ImportDoApelido(const Semantica *s, int32_t nome)
{
    if (s->modulos == NULL) return NULL;
    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no != D_IMPORT || d->modulo != s->modulo_atual || FonteTrechoDoOffset(s->fonte, d->pos) != s->arquivo_atual) continue;
        if ((d->nome_a >= 0 ? d->nome_a : s->modulos->itens[d->indice].apelido) == nome) return d;
    }
    return NULL;
}

/* Offset da 1a ocorrencia do nome: local, item da declaracao que contem o erro, global, alias de foreign ou apelido de import; SEM_POS = nenhuma. */
static uint32_t PosDaPrimeiraOcorrencia(const Semantica *s, int32_t nome)
{
    uint32_t off = s->diag->off, pos;
    const Simbolo *y = SimboloDe(s, nome);
    const LocalInfo *l = y != NULL ? EscopoBuscar(&s->escopo, nome) : NULL;
    const Decl *contem = DeclQueContem(s, off);
    const Decl *outra;

    if (l != NULL && l->pos != off) return l->pos;
    pos = contem != NULL ? PosDoItemRepetido(contem, nome, off) : SEM_POS;
    if (pos != SEM_POS) return pos;
    if (y != NULL && y->decl != NULL && y->decl->pos != off) return y->decl->pos;
    outra = DeclDoAlias(s, nome, off);
    if (outra == NULL) outra = ImportDoApelido(s, nome);
    return outra != NULL ? outra->pos : SEM_POS;
}

static bool PrimeiraOcorrencia(Semantica *s, int32_t nome)
{
    uint32_t pos = PosDaPrimeiraOcorrencia(s, nome);

    if (pos == SEM_POS) return false;
    Adicionar(s, DIAG_REL_USO_ANTERIOR, pos, "primeira ocorrencia de '%s'", SemNomeTexto(s, nome));
    return true;
}

/* Nome no offset do erro: o identificador, ou o texto entre aspas do alias de foreign ("labs"). */
static int32_t NomeDoErro(const Semantica *s)
{
    const char *t = s->fonte->texto;
    uint32_t pos = s->diag->off;
    const char *fim;

    if (t[pos] != '"') return NomeEm(s, pos, NULL);
    fim = strchr(t + pos + 1, '"');
    return fim != NULL ? InternerAchar(s->nomes, t + pos + 1, (uint32_t)(fim - t - pos - 1)) : -1;
}

static void RelE0202(Semantica *s)
{
    int32_t nomes[1 + CITADOS_MAX];
    uint32_t n = 0;
    int32_t aqui = NomeDoErro(s);

    if (aqui >= 0) nomes[n++] = aqui;
    n += NomesCitados(s, nomes + n, CITADOS_MAX);
    for (uint32_t k = 0; k < n; k++) {
        if (PrimeiraOcorrencia(s, nomes[k])) return;
    }
}

/* ---------- E0301, E0310, E0311: o valor ou o tipo que o erro aponta ---------- */

/* Operador em pos: os nomes dos dois operandos (a cadeia da esquerda e o nome da direita). */
static void RelacionarOperandos(Semantica *s, uint32_t pos)
{
    RelacionarNome(s, RaizDaCadeiaAntes(s, pos));
    RelacionarNome(s, NomeDepoisDoOperador(s, pos));
}

static void RelDoUso(Semantica *s)
{
    uint32_t pos = PularPrefixos(s, s->diag->off);
    char c = s->fonte->texto[pos];
    int32_t nome = NomeEm(s, pos, NULL);

    if (nome >= 0) RelacionarNome(s, nome);
    else if (c != '\0' && strchr("+-*/%<>=!&|~^", c) != NULL) RelacionarOperandos(s, pos);
    if (s->diag->rel.n == 0) RelacionarCitados(s);
}

/* ---------- E0502, E0503, E0504: a chamada ---------- */

/* Nome do proc da chamada que contem 'pos' entre os parenteses; -1 se nao ha. */
static int32_t ChamadaAoRedor(const Semantica *s, uint32_t pos)
{
    const char *t = s->fonte->texto;
    uint32_t fundo = 0;

    for (uint32_t i = pos; i > 0 && pos - i < ALCANCE_DA_CHAMADA; i--) {
        char c = t[i - 1];
        if (c == ')' || c == '}') fundo++;
        else if ((c == '(' || c == '{') && fundo > 0) fundo--;
        else if (c == '(') return UltimoNomeAntes(s, i - 1);
        else if (c == '{') return -1; /* abre um bloco, nao uma chamada */
    }
    return -1;
}

/* O campo citado na causa ('dono') entre os campos da struct: a linha dele mostra o tipo que nao tem valor zero. */
static void RelacionarCampoCitado(Semantica *s, const Decl *estrutura)
{
    int32_t citados[CITADOS_MAX];
    uint32_t n = NomesCitados(s, citados, CITADOS_MAX);

    for (uint32_t k = 0; k < n; k++) {
        for (uint32_t c = 0; c < estrutura->ncampos; c++) {
            if (estrutura->campos[c].nome == citados[k]) {
                Adicionar(s, DIAG_REL_DECLARACAO, estrutura->campos[c].pos, "campo '%s' da struct '%s'", SemNomeTexto(s, citados[k]), SemNomeTexto(s, estrutura->nome));
                return;
            }
        }
    }
}

/* Nome em pos seguido de '(' (chamada) ou '{' (literal de struct). */
static bool EhChamadaOuLiteral(const Semantica *s, uint32_t pos, int32_t nome)
{
    uint32_t fim;
    char c;

    if (nome < 0) return false;
    NomeEm(s, pos, &fim);
    c = s->fonte->texto[PularEspacos(s, fim)];
    return c == '(' || c == '{';
}

static void RelDaChamada(Semantica *s)
{
    uint32_t pos = s->diag->off;
    int32_t aqui = NomeEm(s, pos, NULL);
    int32_t alvo = EhChamadaOuLiteral(s, pos, aqui) ? aqui : ChamadaAoRedor(s, pos);
    const Simbolo *y;

    if (!RelacionarNome(s, alvo)) {
        alvo = aqui; /* new(No) / make(...): o tipo em pos; frame_reset(1): nenhum (embutido sem declaracao) */
        RelacionarNome(s, alvo);
    }
    y = SimboloDe(s, alvo);
    if (y != NULL && y->categoria == SIM_STRUCT && y->decl != NULL) RelacionarCampoCitado(s, y->decl);
}

/* ---------- E0607: switch que nao cobre ---------- */

/* Nome do valor do switch: depois de 'switch', ou depois do 'in' em 'switch r in valor'. */
static int32_t NomeDoValorDoSwitch(const Semantica *s, uint32_t pos)
{
    const char *t = s->fonte->texto;
    uint32_t i = SemPosDepoisDaPalavra(s, pos), fim;

    if (NomeEm(s, i, &fim) < 0) return -1;
    fim = PularEspacos(s, fim);
    if (t[fim] == 'i' && t[fim + 1] == 'n' && !CharDeNome(t[fim + 2])) i = PularEspacos(s, fim + 2);
    return NomeEm(s, i, NULL);
}

static void RelDoSwitch(Semantica *s)
{
    int32_t citados[CITADOS_MAX];
    uint32_t n = NomesCitados(s, citados, CITADOS_MAX);

    for (uint32_t k = 0; k < n; k++) {
        const Simbolo *y = SimboloDe(s, citados[k]);
        if (y != NULL && y->decl != NULL && (y->categoria == SIM_ENUM || y->categoria == SIM_UNIAO)) RelacionarTipoDecl(s, y->decl);
    }
    RelacionarNome(s, NomeDoValorDoSwitch(s, s->diag->off));
}

/* ---------- E1003: nome de outro modulo ---------- */

/* O import deste arquivo cujo caminho vem entre aspas na causa ("mods/geo"). */
static const Decl *ImportCitado(const Semantica *s)
{
    const char *ini = strchr(s->diag->causa, '"');
    const char *fim = ini != NULL ? strchr(ini + 1, '"') : NULL;
    int32_t caminho = fim != NULL ? InternerAchar(s->nomes, ini + 1, (uint32_t)(fim - ini - 1)) : -1;

    if (caminho < 0) return NULL;
    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_IMPORT && d->nome == caminho && d->modulo == s->modulo_atual && FonteTrechoDoOffset(s->fonte, d->pos) == s->arquivo_atual) return d;
    }
    return NULL;
}

static void RelE1003(Semantica *s)
{
    int32_t nome = NomeEm(s, s->diag->off, NULL);
    const Decl *importado;

    RelacionarSimbolo(s, nome);
    if (s->diag->rel.n > 0) return;
    importado = ImportCitado(s);
    if (importado != NULL) Adicionar(s, DIAG_REL_DECLARACAO, importado->pos, "import do modulo");
}

/* ---------- entradas ---------- */

void SemRelacionar(Semantica *s)
{
    switch (s->diag->codigo) {
    case E0201: RelE0201(s); break;
    case E0202: RelE0202(s); break;
    case E0301: case E0310: case E0311: RelDoUso(s); break;
    case E0502: case E0503: case E0504: RelDaChamada(s); break;
    case E0607: RelDoSwitch(s); break;
    case E1003: RelE1003(s); break;
    default: return;
    }
    if (s->diag->rel.n == 0) RelacionarUnidade(s);
}

void SemRelacionarParecidos(Semantica *s, const Parecidos *busca)
{
    for (uint32_t k = 0; k < busca->qtd; k++) {
        const ParecidoNome *p = &busca->nomes[k];
        bool tem_pos = p->pos != PARECIDO_SEM_POS && p->pos != 0; /* pos 0: parametro de tipo proc, sem posicao no fonte */

        if (tem_pos) Adicionar(s, DIAG_REL_PARECIDO, p->pos, "parecido #%u: '%.*s' (distancia %u)", k + 1, (int)p->tam, p->texto, p->distancia);
    }
}
