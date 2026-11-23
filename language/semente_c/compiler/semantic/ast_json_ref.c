/* ast_json_ref.c - ids estruturais, ligacoes ("ref") e recorte por id da AST 1.0 em JSON.
 * Versao: 0.2.4
 * Changelog: 0.2.4 - G3 G0c: CompararEntradas desempata por AstRefEntrada.ordem (indice de coleta): ordem total.
 *            0.2.3 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            0.2.2 - B01a: EhDeclOculta inclui a struct oculta do Allocator (Semantica.decl_allocator).
 *            0.2.1 - IA1-L3c: 'scope' (forma nova de escopo) tambem e palavra de alocador.
 *            0.2.0 - IA1-I2: a coleta tambem guarda o dono de cada uso e os usos de nome de tipo (AstRefs.tipos); AstRefsDeclDoTipo (era DeclDoTipo) fica publica.
 *            0.1.0 - IA1-I1: primeira versao (ver ast_json_ref.h). Id = "<modulo>:<decl>/<campo>[i]/..." (D-IA1-20), derivado da estrutura e nunca de linha ou contador.
 * Verificacao: construir.bat && python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 */
#include <ctype.h>
#include <stdlib.h>

#include "semantic/ast_json_interno.h"
#include "semantic/tipos.h"

/* ---------- ids ---------- */

size_t IdEmpurrar(Json *j, const char *chave, int indice)
{
    size_t antes = j->id.tam;
    BufFormatar(&j->id, "/%s", chave);
    if (indice >= 0) BufFormatar(&j->id, "[%d]", indice);
    return antes;
}

void IdRestaurar(Json *j, size_t tam)
{
    j->id.tam = tam;
    if (j->id.dados != NULL) j->id.dados[tam] = '\0';
}

void AstIdBaseDaDecl(Buf *saida, const Modulos *modulos, const Interner *nomes, const Decl *d)
{
    int32_t caminho = modulos->itens[d->modulo].caminho;
    BufAnexarN(saida, InternerTexto(nomes, caminho), InternerTam(nomes, caminho));
    BufAnexarN(saida, ":", 1);
    switch ((DeclTipo)d->tipo_no) {
    case D_TEST: BufAnexar(saida, "test:"); break;
    case D_IMPORT: BufAnexar(saida, "import:"); break;
    case D_LINK: BufAnexar(saida, "link:"); break;
    case D_ASSERT: BufAnexar(saida, "static_assert"); return;
    default: break;
    }
    if (d->nome >= 0) BufAnexarN(saida, InternerTexto(nomes, d->nome), InternerTam(nomes, d->nome));
}

/* ---------- nomes declarados no que esta sendo escrito ---------- */

static void Guardar(Json *j, Vec *tabela, uint32_t chave)
{
    AstGuardado *g = VecEmpurrar(tabela);
    g->chave = chave;
    g->off = (uint32_t)j->guardados.tam;
    g->tam = (uint32_t)j->id.tam;
    BufAnexarN(&j->guardados, j->id.dados, j->id.tam);
}

static const AstGuardado *Achar(const Vec *tabela, uint32_t chave)
{
    for (size_t i = tabela->n; i > 0; i--) {
        const AstGuardado *g = VecEm(tabela, i - 1);
        if (g->chave == chave) return g;
    }
    return NULL;
}

void LocalRegistrar(Json *j, uint32_t pos)
{
    if (j->refs != NULL) Guardar(j, &j->locais, pos);
}

void ImportRegistrar(Json *j, int32_t apelido)
{
    if (j->refs != NULL) Guardar(j, &j->imports, (uint32_t)apelido);
}

/* ---------- tabela de usos resolvidos ---------- */

typedef struct { Vec *entradas, *tipos; Vec locais; const Decl *dono; } Coletor; /* locais: uint32_t por local_id = offset do no que o declara; dono: a Decl em coleta */

static bool EhUsoDeNome(const Expr *e)
{
    bool nomeia = e->tipo_no == EX_NOME || e->tipo_no == EX_CHAMADA || e->tipo_no == EX_CAMPO || e->tipo_no == EX_LITERAL_STRUCT;
    return nomeia && e->ref != REF_NENHUM;
}

static uint32_t DeclaracaoDoLocal(const Coletor *c, const Expr *e)
{
    bool local = e->ref == REF_LOCAL || e->ref == REF_PARAM;
    if (!local || e->ref_id < 0 || (size_t)e->ref_id >= c->locais.n) return AST_REF_SEM_POS;
    return *(const uint32_t *)VecEm(&c->locais, (size_t)e->ref_id);
}

/* 'uso' traz pos, nome, membro, classe e modulo; o dono e o da declaracao que o coletor percorre. */
static void EmpurrarUsoDeTipo(Coletor *c, AstRefTipo uso)
{
    uso.dono = c->dono;
    *(AstRefTipo *)VecEmpurrar(c->tipos) = uso;
}

/* Cada TipoRef que a AST imprime: o nome (TR_NOME) vira uso; os tipos de dentro (alvo, chave/valor, parametros e retorno de proc) tambem. */
static void ColetarTipo(Coletor *c, const TipoRef *t)
{
    if (t == NULL || t->forma == TR_TUPLA) return;
    if (t->forma == TR_NOME) {
        if (t->nome >= 0) EmpurrarUsoDeTipo(c, (AstRefTipo){.pos = t->pos, .nome = t->nome, .membro = -1, .classe = AST_USO_TIPO_ESCRITO, .modulo = t->modulo});
        return;
    }
    if (t->forma == TR_PROC) {
        if (t->pos_tamanho == 1) ColetarTipo(c, &t->alvo[0]);
        for (int32_t i = 1; i <= t->nome; i++) ColetarTipo(c, &t->alvo[i]);
        return;
    }
    ColetarTipo(c, &t->alvo[0]);
    if (t->forma == TR_MAPA) ColetarTipo(c, &t->alvo[1]);
}

static void ColetarExpr(Coletor *c, const Expr *e)
{
    if (e == NULL) return;
    if (EhUsoDeNome(e)) {
        AstRefEntrada *r = VecEmpurrar(c->entradas);
        r->pos_op = e->pos_op;
        r->e = e;
        r->decl_pos = DeclaracaoDoLocal(c, e);
        r->dono = c->dono;
        r->ordem = (uint32_t)(c->entradas->n - 1);
    } else if (e->tipo_no == EX_NOME) {
        EmpurrarUsoDeTipo(c, (AstRefTipo){.pos = e->pos, .nome = e->nome, .membro = -1, .classe = AST_USO_NOME_DE_TIPO, .modulo = e->modulo}); /* talvez seja nome de tipo (transmute(x, para = T)); o indice decide */
    }
    ColetarExpr(c, e->a);
    ColetarExpr(c, e->b);
    if (e->tipo_no == EX_CONVERSAO || e->tipo_no == EX_TIPO) { /* args e alvo dividem a uniao */
        ColetarTipo(c, e->alvo);
        return;
    }
    for (uint32_t i = 0; i < e->nargs; i++) ColetarExpr(c, e->args[i].valor);
}

static void DeclararLocal(Coletor *c, int32_t id, uint32_t pos)
{
    if (id >= 0 && (size_t)id < c->locais.n) *(uint32_t *)VecEm(&c->locais, (size_t)id) = pos;
}

/* Quem declara cada local do proc/test: let, var, for (item e indice), let multiplo e o nome ligado de um case. */
static void DeclararLocais(Coletor *c, const Stmt *s)
{
    for (; s != NULL; s = s->prox) {
        if (s->tipo_no == ST_LET || s->tipo_no == ST_VAR || s->tipo_no == ST_FOR) DeclararLocal(c, s->local_id, s->pos);
        if (s->tipo_no == ST_FOR) DeclararLocal(c, s->local_indice, s->pos);
        for (uint32_t i = 0; s->tipo_no == ST_LET_MULTIPLO && i < s->nnomes; i++) DeclararLocal(c, s->nomes[i].local_id, s->pos);
        for (uint32_t i = 0; s->tipo_no == ST_SWITCH && i < s->ncasos; i++) {
            DeclararLocal(c, s->casos[i].local_id, s->casos[i].pos);
            DeclararLocais(c, s->casos[i].corpo);
        }
        DeclararLocais(c, s->corpo);
        DeclararLocais(c, s->senao);
    }
}

static void ColetarUsos(Coletor *c, const Stmt *s);

/* 'case Tipo[.membro]:' ou 'case ^Tipo:' (tipo escrito); o default nao usa nome. */
static void ColetarCaso(Coletor *c, const Caso *k)
{
    if (k->tipo_escrito != NULL) ColetarTipo(c, k->tipo_escrito);
    else if (!k->padrao) EmpurrarUsoDeTipo(c, (AstRefTipo){.pos = k->pos, .nome = k->tipo_nome, .membro = k->membro, .classe = AST_USO_CASO, .modulo = k->modulo});
    ColetarUsos(c, k->corpo);
}

static void ColetarUsos(Coletor *c, const Stmt *s)
{
    for (; s != NULL; s = s->prox) {
        if ((s->tipo_no == ST_LET || s->tipo_no == ST_VAR) && s->tem_tipo) ColetarTipo(c, &s->tipo);
        ColetarExpr(c, s->expr);
        ColetarExpr(c, s->expr2);
        ColetarExpr(c, s->alvo);
        for (uint32_t i = 0; s->tipo_no == ST_SWITCH && i < s->ncasos; i++) ColetarCaso(c, &s->casos[i]);
        ColetarUsos(c, s->corpo);
        ColetarUsos(c, s->senao);
    }
}

/* Tipos escritos na assinatura: const, parametros, retorno (ou os retornos multiplos) e campos/variantes. */
static void ColetarTiposDaDecl(Coletor *c, const Decl *d)
{
    if (d->tipo_no == D_CONST) ColetarTipo(c, &d->tipo);
    for (uint32_t i = 0; i < d->nparams; i++) ColetarTipo(c, &d->params[i].tipo);
    if (d->tem_retorno && d->retorno.forma == TR_TUPLA) {
        for (uint32_t i = 0; i < d->nretornos; i++) ColetarTipo(c, &d->retornos[i].tipo);
    } else if (d->tem_retorno) {
        ColetarTipo(c, &d->retorno);
    }
    for (uint32_t i = 0; i < d->ncampos; i++) ColetarTipo(c, &d->campos[i].tipo);
}

static void ColetarDecl(AstRefs *refs, const Decl *d)
{
    Coletor c = {&refs->entradas, &refs->tipos, {0}, d};
    if (d->com_erro) return; /* a analise parou nesta declaracao: os locais que sobraram nao foram numerados */
    ColetarTiposDaDecl(&c, d);
    VecIniciar(&c.locais, sizeof(uint32_t));
    for (uint32_t i = 0; i < d->nlocais; i++) *(uint32_t *)VecEmpurrar(&c.locais) = AST_REF_SEM_POS;
    for (uint32_t i = 0; i < d->nparams; i++) DeclararLocal(&c, (int32_t)i, d->params[i].pos);
    DeclararLocais(&c, d->corpo);
    ColetarExpr(&c, d->init);
    for (uint32_t i = 0; i < d->nparams; i++) ColetarExpr(&c, d->params[i].padrao);
    for (uint32_t i = 0; i < d->ncampos; i++) ColetarExpr(&c, d->campos[i].padrao);
    ColetarUsos(&c, d->corpo);
    VecLiberar(&c.locais);
}

static int CompararEntradas(const void *a, const void *b)
{
    const AstRefEntrada *p = a, *q = b;

    if (p->pos_op != q->pos_op) return p->pos_op < q->pos_op ? -1 : 1;
    return p->ordem < q->ordem ? -1 : (p->ordem > q->ordem ? 1 : 0);
}

void AstRefsMontar(AstRefs *refs, const Semantica *sem)
{
    refs->sem = sem;
    VecIniciar(&refs->entradas, sizeof(AstRefEntrada));
    VecIniciar(&refs->tipos, sizeof(AstRefTipo));
    for (const Decl *d = sem->programa->primeira; d != NULL; d = d->prox) ColetarDecl(refs, d);
    qsort(refs->entradas.dados, refs->entradas.n, sizeof(AstRefEntrada), CompararEntradas);
}

void AstRefsLiberar(AstRefs *refs)
{
    VecLiberar(&refs->entradas);
    VecLiberar(&refs->tipos);
}

static const AstRefEntrada *AcharEntrada(const AstRefs *refs, uint32_t pos_op, uint8_t tipo_no)
{
    size_t lo = 0, hi = refs->entradas.n;
    while (lo < hi) {
        size_t meio = lo + (hi - lo) / 2;
        if (((const AstRefEntrada *)VecEm(&refs->entradas, meio))->pos_op < pos_op) lo = meio + 1; else hi = meio;
    }
    if (lo >= refs->entradas.n) return NULL;
    const AstRefEntrada *primeira = VecEm(&refs->entradas, lo);
    if (primeira->pos_op != pos_op) return NULL;
    /* a semantica pode embrulhar o no (a -> a.id) e deixar dois com o mesmo pos_op: vale o da mesma especie que o impresso */
    for (size_t k = lo; k < refs->entradas.n; k++) {
        const AstRefEntrada *r = VecEm(&refs->entradas, k);
        if (r->pos_op != pos_op) break;
        if (r->e->tipo_no == tipo_no) return r;
    }
    return primeira;
}

/* ---------- declaracao de um nome ou de um tipo ---------- */

static bool EhDeclOculta(const Semantica *sem, const Decl *d)
{
    if (d == sem->decl_quat || d == sem->decl_allocator) return true;
    for (uint32_t i = 0; i < sem->qtd_listas_fixas; i++) {
        if (sem->decls_listas_fixas[i] == d) return true;
    }
    return false;
}

static const Decl *DeclDoSimbolo(const Semantica *sem, const Simbolo *y)
{
    const Decl *d = y->decl;
    if (d == NULL || y->categoria == SIM_PROC_EMBUTIDO || EhDeclOculta(sem, d)) return NULL;
    return d;
}

const Decl *AstRefsDeclDoNome(const AstRefs *refs, int32_t nome, uint8_t qualificador, uint8_t modulo_do_uso)
{
    const Semantica *sem = refs->sem;
    if (nome < 0 || (uint32_t)nome >= sem->qtd_nomes || sem->globais == NULL) return NULL;
    return DeclDoSimbolo(sem, SemAcharSimboloEm(sem, nome, qualificador, modulo_do_uso));
}

/* A chave (TipoInfo.a de struct, enum e union) ja e o indice de Semantica.globais. */
static const Decl *DeclDaChave(const AstRefs *refs, int32_t chave)
{
    const Semantica *sem = refs->sem;
    if (chave < 0 || (uint32_t)chave >= sem->qtd_nomes || sem->globais == NULL) return NULL;
    return DeclDoSimbolo(sem, &sem->globais[chave]);
}

/* struct, enum ou union do tipo (atras de um '^'); NULL nos demais. */
const Decl *AstRefsDeclDoTipo(const AstRefs *refs, TipoId t)
{
    const Tipos *tipos = &refs->sem->tipos;
    const TipoInfo *k;
    if (t == TIPO_NAO_RESOLVIDO) return NULL;
    k = TipoDe(tipos, t);
    if (k->classe == TC_PONTEIRO) k = TipoDe(tipos, k->a);
    if (k->classe != TC_STRUCT && k->classe != TC_ENUM && k->classe != TC_UNIAO) return NULL;
    return DeclDaChave(refs, (int32_t)k->a);
}

static const char *ModuloDaDecl(const AstRefs *refs, const Decl *d, size_t *tam)
{
    int32_t caminho = refs->sem->modulos->itens[d->modulo].caminho;
    *tam = InternerTam(refs->sem->nomes, caminho);
    return InternerTexto(refs->sem->nomes, caminho);
}

static const char *CategoriaDaDecl(const Decl *d)
{
    switch ((DeclTipo)d->tipo_no) {
    case D_CONST: return "const";
    case D_STRUCT: return "struct";
    case D_ENUM: return "enum";
    case D_UNIAO: return "union";
    case D_PROC: return "proc";
    default: return "unit";
    }
}

/* ---------- escrita do "ref" ---------- */

static void EmitirRef(Json *j, const char *tipo, const char *id, size_t n_id, const char *modulo, size_t n_modulo)
{
    Chave(j, "ref");
    TextoJson(j, id, n_id, false);
    Chave(j, "ref_modulo");
    TextoJson(j, modulo, n_modulo, false);
    BufFormatar(j->b, ", \"ref_tipo\": \"%s\"", tipo);
}

/* 'apelido.nome': o id do import que trouxe o apelido (texto do apelido lido cru da fonte em 'off'). */
static void RefDoImport(Json *j, uint8_t modulo, uint32_t off)
{
    const char *s = j->fonte->texto + off;
    size_t n = 0;
    const AstGuardado *g;
    int32_t apelido;
    if (modulo == 0) return;
    while ((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '_') n++;
    apelido = InternerAchar(j->nomes, s, (uint32_t)n);
    g = apelido < 0 ? NULL : Achar(&j->imports, (uint32_t)apelido);
    if (g == NULL) return;
    Chave(j, "ref_import");
    TextoJson(j, j->guardados.dados + g->off, g->tam, false);
}

static void EmitirRefDaDecl(Json *j, const Decl *d, const char *tipo, const char *sufixo, int32_t indice)
{
    Buf id = {0};
    size_t n_modulo;
    const char *modulo = ModuloDaDecl(j->refs, d, &n_modulo);
    AstIdBaseDaDecl(&id, j->refs->sem->modulos, j->refs->sem->nomes, d);
    if (sufixo != NULL) BufFormatar(&id, "/%s[%d]", sufixo, indice);
    EmitirRef(j, tipo, id.dados, id.tam, modulo, n_modulo);
    BufLiberar(&id);
}

static void EmitirRefDoLocal(Json *j, const AstRefEntrada *r)
{
    const AstGuardado *g = r->decl_pos == AST_REF_SEM_POS ? NULL : Achar(&j->locais, r->decl_pos);
    const char *modulo;
    if (g == NULL) return;
    modulo = InternerTexto(j->nomes, j->modulos->itens[j->modulo].caminho);
    EmitirRef(j, r->e->ref == REF_PARAM ? "param" : "local", j->guardados.dados + g->off, g->tam, modulo,
              InternerTam(j->nomes, j->modulos->itens[j->modulo].caminho));
}

static void EmitirRefGlobal(Json *j, const Expr *e)
{
    const AstRefs *refs = j->refs;
    switch ((RefTipo)e->ref) {
    case REF_CONST:
    case REF_ENUM:
    case REF_PROC:
    case REF_STRUCT: {
        const Decl *d = AstRefsDeclDoNome(refs, e->nome, e->modulo, j->modulo);
        if (d != NULL) EmitirRefDaDecl(j, d, CategoriaDaDecl(d), NULL, 0);
        break;
    }
    case REF_MEMBRO: {
        const Decl *d = AstRefsDeclDoTipo(refs, e->tipo);
        if (d != NULL) EmitirRefDaDecl(j, d, "membro", "membros", e->ref_id);
        break;
    }
    case REF_CAMPO: {
        const Decl *d = AstRefsDeclDoTipo(refs, e->a->tipo);
        if (d != NULL) EmitirRefDaDecl(j, d, "campo", "campos", e->ref_id);
        break;
    }
    default: break;
    }
}

static const Decl *DeclDoNomeDeTipo(Json *j, int32_t nome, uint8_t qualificador);

/* Tipos da linguagem (i32, f64...) e as palavras de alocador ('alloc = heap | frame | scope'; escopo e a forma antiga, L3c). */
static bool NomeEhPalavraDaLinguagem(const char *texto, size_t n)
{
    static const char *const alocadores[] = {"heap", "frame", "scope", "escopo"};
    for (size_t i = 0; i < sizeof alocadores / sizeof *alocadores; i++) {
        if (strlen(alocadores[i]) == n && memcmp(alocadores[i], texto, n) == 0) return true;
    }
    for (TipoId t = 1; t <= 16; t++) {
        const char *nome = TipoNomeFixo(t);
        if (nome != NULL && nome[0] != '(' && strlen(nome) == n && memcmp(nome, texto, n) == 0) return true;
    }
    return false;
}

/* Nome usado como tipo ou palavra de alocador (transmute(x, para = T), make(..., em = heap)): a semantica nao o liga a nada. Struct, enum, union e unit declarados viram ref; i32, heap... sao embutidos. */
static void RefDoNomeDeTipo(Json *j, const Expr *e)
{
    const Decl *d = DeclDoNomeDeTipo(j, e->nome, e->modulo);
    if (d != NULL && (d->tipo_no == D_STRUCT || d->tipo_no == D_ENUM || d->tipo_no == D_UNIAO || d->tipo_no == D_UNIDADE_BASE || d->tipo_no == D_UNIDADE_DERIV)) {
        EmitirRefDaDecl(j, d, CategoriaDaDecl(d), NULL, 0);
        RefDoImport(j, e->modulo, e->pos);
        return;
    }
    if (NomeEhPalavraDaLinguagem(InternerTexto(j->nomes, e->nome), InternerTam(j->nomes, e->nome))) BufAnexar(j->b, ", \"ref_tipo\": \"embutido\"");
}

void RefDaExpr(Json *j, const Expr *e)
{
    const AstRefEntrada *r;
    bool nomeia = e->tipo_no == EX_NOME || e->tipo_no == EX_CHAMADA || e->tipo_no == EX_CAMPO || e->tipo_no == EX_LITERAL_STRUCT;
    if (j->refs == NULL || !nomeia) return;
    r = AcharEntrada(j->refs, e->pos_op, e->tipo_no);
    if (r == NULL) {
        if (e->tipo_no == EX_NOME) RefDoNomeDeTipo(j, e);
        return;
    }
    if (r->e->ref == REF_EMBUTIDO) {
        BufAnexar(j->b, ", \"ref_tipo\": \"embutido\"");
        return;
    }
    if (r->e->ref == REF_LOCAL || r->e->ref == REF_PARAM) EmitirRefDoLocal(j, r);
    else EmitirRefGlobal(j, r->e);
    RefDoImport(j, e->modulo, e->pos);
}

/* Nome de tipo (id do Interner de j): a declaracao global dele, procurada pelo texto na semantica que analisou. */
static const Decl *DeclDoNomeDeTipo(Json *j, int32_t nome, uint8_t qualificador)
{
    int32_t na_semantica;
    if (j->refs == NULL || nome < 0) return NULL;
    na_semantica = InternerAchar(j->refs->sem->nomes, InternerTexto(j->nomes, nome), InternerTam(j->nomes, nome));
    return AstRefsDeclDoNome(j->refs, na_semantica, qualificador, j->modulo);
}

void RefDoTipo(Json *j, const TipoRef *t)
{
    const Decl *d;
    if (t->forma != TR_NOME) return;
    d = DeclDoNomeDeTipo(j, t->nome, t->modulo);
    if (d == NULL) return;
    EmitirRefDaDecl(j, d, CategoriaDaDecl(d), NULL, 0);
    RefDoImport(j, t->modulo, t->pos);
}

/* 'case Tipo:' e 'case Enum.membro:' */
void RefDoCaso(Json *j, const Caso *c)
{
    const Decl *d;
    if (c->padrao || c->tipo_escrito != NULL) return;
    d = DeclDoNomeDeTipo(j, c->tipo_nome, c->modulo);
    if (d == NULL) return;
    if (c->membro >= 0 && d->tipo_no == D_ENUM) {
        int32_t membro = InternerAchar(j->refs->sem->nomes, InternerTexto(j->nomes, c->membro), InternerTam(j->nomes, c->membro));
        for (uint32_t k = 0; k < d->nmembros; k++) {
            if (d->membros[k].nome == membro) {
                EmitirRefDaDecl(j, d, "membro", "membros", (int32_t)k);
                return;
            }
        }
    }
    EmitirRefDaDecl(j, d, CategoriaDaDecl(d), NULL, 0);
}

/* ---------- recorte de um no por id ---------- */

/* Texto do id como TextoJson o escreve (aspas, barra e controles com escape). */
static void EscaparId(Buf *saida, const char *id)
{
    for (const unsigned char *p = (const unsigned char *)id; *p != '\0'; p++) {
        if (*p == '"' || *p == '\\') {
            BufAnexarN(saida, "\\", 1);
            BufAnexarN(saida, (const char *)p, 1);
        } else if (*p < 0x20) {
            BufFormatar(saida, "\\u%04x", *p);
        } else {
            BufAnexarN(saida, (const char *)p, 1);
        }
    }
}

/* Abertura '{' do objeto cujo "id" comeca em 'achado': '{"id"' ou '{"no": "<tipo>", "id"'. */
static size_t AberturaDoObjeto(const char *json, size_t achado)
{
    size_t p = achado;
    if (p > 0 && json[p - 1] == '{') return p - 1;
    while (p > 0) {
        p--;
        if (json[p] == '{' && strncmp(json + p, "{\"no\": \"", 8) == 0) return p;
    }
    return 0;
}

/* Fim (exclusivo) do objeto que abre em 'ini': conta {} e [] fora de texto. */
static size_t FimDoObjeto(const char *json, size_t tam, size_t ini)
{
    int profundidade = 0;
    bool em_texto = false;
    for (size_t k = ini; k < tam; k++) {
        char ch = json[k];
        if (em_texto) {
            if (ch == '\\') k++;
            else if (ch == '"') em_texto = false;
        } else if (ch == '"') {
            em_texto = true;
        } else if (ch == '{' || ch == '[') {
            profundidade++;
        } else if ((ch == '}' || ch == ']') && --profundidade == 0) {
            return k + 1;
        }
    }
    return tam;
}

bool AstNoExtrair(const char *json, size_t tam, const char *id, size_t *ini, size_t *fim)
{
    Buf padrao = {0};
    const char *achado;
    BufAnexar(&padrao, "\"id\": \"");
    EscaparId(&padrao, id);
    BufAnexarN(&padrao, "\"", 1);
    achado = strstr(json, padrao.dados);
    BufLiberar(&padrao);
    if (achado == NULL) return false;
    *ini = AberturaDoObjeto(json, (size_t)(achado - json));
    *fim = FimDoObjeto(json, tam, *ini);
    return true;
}
