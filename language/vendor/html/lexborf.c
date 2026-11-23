/*
 * lexborf.c - cola C99 entre o lexbor 3.0.0 e a ponte Forja. Contrato em lexborf.h.
 *
 * Fluxo: parse HTML -> DOM plano (pre-ordem, iterativo: sem recursao, arvore
 * profunda nao estoura a pilha) -> consultas por indice. CSS: serializa cada
 * regra e declaracao para o buffer do proprio sheet; consultas devolvem
 * ponteiros para esse buffer.
 */
#include <stdlib.h>
#include <string.h>

#include "lexborf.h"
#include "lexbor/html/html.h"
#include "lexbor/dom/dom.h"
#include "lexbor/css/css.h"
#include "lexbor/selectors/selectors.h"

#define LEXBORF_VAZIO ((const uint8_t *) "")
#define LEXBORF_MASCARA_ESP 0x07FFFFFFu
#define LEXBORF_SUFIXO_IMPORTANTE " !important"

/* ---- utilitarios internos ---- */

/* Garante capacidade para `need` elementos de `tam` bytes; 0 = falta de memoria. */
static int lexborf_cresce(void **arr, size_t *cap, size_t need, size_t tam)
{
    size_t novo;
    void *tmp;

    if (need <= *cap) {
        return 1;
    }
    novo = *cap ? *cap : 64;
    while (novo < need) {
        novo *= 2;
    }
    tmp = realloc(*arr, novo * tam);
    if (tmp == NULL) {
        return 0;
    }
    *arr = tmp;
    *cap = novo;
    return 1;
}

static void lexborf_set_len(size_t *len, size_t valor)
{
    if (len != NULL) {
        *len = valor;
    }
}

/* ---- DOM plano ---- */

struct lexborf_no {
    lxb_dom_node_t *no;
    int32_t pai;
    int32_t primeiro;
    int32_t proximo;
    int32_t ultimo;
};

struct lexborf_doc {
    lxb_html_document_t *doc;
    struct lexborf_no *nos;
    size_t n;
    size_t cap;
    /* cache de atributos: acesso sequencial a k nao vira O(k^2) */
    int32_t ac_elem;
    int32_t ac_k;
    lxb_dom_attr_t *ac_attr;
    /* copias terminadas em NUL que devolvemos (texto sem NUL no lexbor); vivem ate o free */
    uint8_t **vivos;
    size_t nvivos;
    size_t capvivos;
};

/* Copia n bytes para memoria do documento, com NUL no fim; NULL = falta de memoria. */
static const uint8_t *lexborf_copia_texto(const lexborf_doc *d, const lxb_char_t *p, size_t n)
{
    lexborf_doc *w = (lexborf_doc *) d;
    uint8_t *c = malloc(n + 1);

    if (c == NULL) {
        return NULL;
    }
    if (n > 0) {
        memcpy(c, p, n);
    }
    c[n] = 0;
    if (!lexborf_cresce((void **) &w->vivos, &w->capvivos, w->nvivos + 1, sizeof(*w->vivos))) {
        free(c);
        return NULL;
    }
    w->vivos[w->nvivos++] = c;
    return c;
}

static int32_t lexborf_visita(lexborf_doc *d, lxb_dom_node_t *no, int32_t pai)
{
    size_t i = d->n;
    struct lexborf_no *e;

    if (!lexborf_cresce((void **) &d->nos, &d->cap, i + 1, sizeof(*d->nos))) {
        return -1;
    }
    e = &d->nos[i];
    e->no = no;
    e->pai = pai;
    e->primeiro = -1;
    e->proximo = -1;
    e->ultimo = -1;
    if (pai >= 0) {
        struct lexborf_no *p = &d->nos[pai];

        if (p->primeiro < 0) {
            p->primeiro = (int32_t) i;
        } else {
            d->nos[p->ultimo].proximo = (int32_t) i;
        }
        p->ultimo = (int32_t) i;
    }
    d->n = i + 1;
    return (int32_t) i;
}

static int lexborf_empilha(int32_t **pilha, size_t *cap, size_t *n, int32_t valor)
{
    if (!lexborf_cresce((void **) pilha, cap, *n + 1, sizeof(int32_t))) {
        return 0;
    }
    (*pilha)[(*n)++] = valor;
    return 1;
}

/* Sobe a partir de um no sem irmao seguinte; devolve o proximo irmao ou NULL ao sair da raiz. */
static lxb_dom_node_t *lexborf_proximo_no(lxb_dom_node_t *no, lxb_dom_node_t *raiz,
                                          size_t *plen)
{
    while (no->next == NULL) {
        no = no->parent;
        if (no == raiz) {
            return NULL;
        }
        (*plen)--;
    }
    return no->next;
}

static int lexborf_constroi(lexborf_doc *d, lxb_dom_node_t *raiz)
{
    int32_t *caminho = NULL;
    size_t plen = 0;
    size_t capc = 0;
    lxb_dom_node_t *no;
    int32_t idx;
    int ok = 0;

    if (lexborf_visita(d, raiz, -1) < 0) {
        return 0;
    }
    if (!lexborf_empilha(&caminho, &capc, &plen, 0)) {
        goto fim;
    }
    for (no = raiz->first_child; no != NULL; ) {
        idx = lexborf_visita(d, no, caminho[plen - 1]);
        if (idx < 0) {
            goto fim;
        }
        if (no->first_child != NULL) {
            if (!lexborf_empilha(&caminho, &capc, &plen, idx)) {
                goto fim;
            }
            no = no->first_child;
            continue;
        }
        no = lexborf_proximo_no(no, raiz, &plen);
    }
    ok = 1;
fim:
    free(caminho);
    return ok;
}

void lexborf_free(lexborf_doc *d)
{
    if (d == NULL) {
        return;
    }
    if (d->doc != NULL) {
        (void) lxb_html_document_destroy(d->doc);
    }
    for (size_t k = 0; k < d->nvivos; k++) {
        free(d->vivos[k]);
    }
    free(d->vivos);
    free(d->nos);
    free(d);
}

lexborf_doc *lexborf_parse(const uint8_t *html, size_t len)
{
    lexborf_doc *d;

    if (html == NULL && len > 0) {
        return NULL;
    }
    d = calloc(1, sizeof(*d));
    if (d == NULL) {
        return NULL;
    }
    d->ac_elem = -1;
    d->doc = lxb_html_document_create();
    if (d->doc == NULL) {
        lexborf_free(d);
        return NULL;
    }
    if (lxb_html_document_parse(d->doc, html ? html : LEXBORF_VAZIO, len) != LXB_STATUS_OK) {
        lexborf_free(d);
        return NULL;
    }
    if (!lexborf_constroi(d, lxb_dom_interface_node(d->doc))) {
        lexborf_free(d);
        return NULL;
    }
    return d;
}

static int lexborf_valido(const lexborf_doc *d, int32_t i)
{
    return d != NULL && i >= 0 && (size_t) i < d->n;
}

int32_t lexborf_node_count(const lexborf_doc *d)
{
    return d != NULL ? (int32_t) d->n : 0;
}

int32_t lexborf_node_type(const lexborf_doc *d, int32_t i)
{
    if (!lexborf_valido(d, i)) {
        return -1;
    }
    return (int32_t) d->nos[i].no->type;
}

int32_t lexborf_parent(const lexborf_doc *d, int32_t i)
{
    return lexborf_valido(d, i) ? d->nos[i].pai : -1;
}

int32_t lexborf_first_child(const lexborf_doc *d, int32_t i)
{
    return lexborf_valido(d, i) ? d->nos[i].primeiro : -1;
}

int32_t lexborf_next_sibling(const lexborf_doc *d, int32_t i)
{
    return lexborf_valido(d, i) ? d->nos[i].proximo : -1;
}

const uint8_t *lexborf_tag_name(const lexborf_doc *d, int32_t i, size_t *len)
{
    lxb_dom_node_t *no;
    const lxb_char_t *nome = NULL;
    size_t n = 0;

    lexborf_set_len(len, 0);
    if (!lexborf_valido(d, i)) {
        return NULL;
    }
    no = d->nos[i].no;
    if (no->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        nome = lxb_dom_element_local_name((lxb_dom_element_t *) no, &n);
    } else if (no->type == LXB_DOM_NODE_TYPE_DOCUMENT_TYPE) {
        nome = lxb_dom_document_type_name((lxb_dom_document_type_t *) no, &n);
    } else {
        return NULL;
    }
    if (nome == NULL) {
        return LEXBORF_VAZIO;
    }
    lexborf_set_len(len, n);
    return lexborf_copia_texto(d, nome, n);
}

int32_t lexborf_namespace(const lexborf_doc *d, int32_t i)
{
    if (!lexborf_valido(d, i) || d->nos[i].no->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return 0;
    }
    switch (d->nos[i].no->ns) {
    case LXB_NS_HTML:
        return 1;
    case LXB_NS_MATH:
        return 2;
    case LXB_NS_SVG:
        return 3;
    default:
        return 0;
    }
}

/* Atributo k do elemento i; reaproveita o cache quando o pedido avanca. */
static lxb_dom_attr_t *lexborf_attr_k(const lexborf_doc *d, int32_t i, int32_t k)
{
    lexborf_doc *w = (lexborf_doc *) d;
    lxb_dom_attr_t *a;
    int32_t c;

    if (w->ac_elem != i || w->ac_k > k || w->ac_attr == NULL) {
        a = lxb_dom_element_first_attribute((lxb_dom_element_t *) w->nos[i].no);
        c = 0;
    } else {
        a = w->ac_attr;
        c = w->ac_k;
    }
    for (; a != NULL && c < k; c++) {
        a = a->next;
    }
    if (a != NULL) {
        w->ac_elem = i;
        w->ac_k = c;
        w->ac_attr = a;
    }
    return a;
}

int32_t lexborf_attr_count(const lexborf_doc *d, int32_t i)
{
    lxb_dom_attr_t *a;
    int32_t total = 0;

    if (!lexborf_valido(d, i) || d->nos[i].no->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return 0;
    }
    for (a = lxb_dom_element_first_attribute((lxb_dom_element_t *) d->nos[i].no);
         a != NULL; a = a->next) {
        total++;
    }
    return total;
}

const uint8_t *lexborf_attr_name(const lexborf_doc *d, int32_t i, int32_t k, size_t *len)
{
    lxb_dom_attr_t *a;
    const lxb_char_t *nome;
    size_t n = 0;

    lexborf_set_len(len, 0);
    if (!lexborf_valido(d, i) || k < 0) {
        return NULL;
    }
    a = lexborf_attr_k(d, i, k);
    if (a == NULL) {
        return NULL;
    }
    nome = lxb_dom_attr_qualified_name(a, &n);
    if (nome == NULL) {
        return LEXBORF_VAZIO;
    }
    lexborf_set_len(len, n);
    return lexborf_copia_texto(d, nome, n);
}

const uint8_t *lexborf_attr_value(const lexborf_doc *d, int32_t i, int32_t k, size_t *len)
{
    lxb_dom_attr_t *a;
    const lxb_char_t *valor;
    size_t n = 0;

    lexborf_set_len(len, 0);
    if (!lexborf_valido(d, i) || k < 0) {
        return NULL;
    }
    a = lexborf_attr_k(d, i, k);
    if (a == NULL) {
        return NULL;
    }
    valor = lxb_dom_attr_value(a, &n);
    if (valor == NULL) {
        return LEXBORF_VAZIO;
    }
    lexborf_set_len(len, n);
    return lexborf_copia_texto(d, valor, n);
}

const uint8_t *lexborf_text(const lexborf_doc *d, int32_t i, size_t *len)
{
    lxb_dom_node_t *no;
    lxb_dom_character_data_t *cd;

    lexborf_set_len(len, 0);
    if (!lexborf_valido(d, i)) {
        return NULL;
    }
    no = d->nos[i].no;
    if (no->type != LXB_DOM_NODE_TYPE_TEXT && no->type != LXB_DOM_NODE_TYPE_COMMENT) {
        return NULL;
    }
    cd = (lxb_dom_character_data_t *) no;
    if (cd->data.data == NULL) {
        return LEXBORF_VAZIO;
    }
    lexborf_set_len(len, cd->data.length);
    return lexborf_copia_texto(d, cd->data.data, cd->data.length);
}

/* ---- seletores CSS sobre o DOM ---- */

typedef struct {
    lxb_css_parser_t *parser;
    lxb_css_memory_t *memoria;
    lxb_css_selectors_t *seletores_css;
    lxb_selectors_t *casador;
    lxb_css_selector_list_t *lista;
} lexborf_busca;

static void lexborf_busca_libera(lexborf_busca *b)
{
    if (b->casador != NULL) {
        (void) lxb_selectors_destroy(b->casador, true);
    }
    if (b->seletores_css != NULL) {
        (void) lxb_css_selectors_destroy(b->seletores_css, true);
    }
    if (b->parser != NULL) {
        (void) lxb_css_parser_destroy(b->parser, true);
    }
    if (b->memoria != NULL) {
        (void) lxb_css_memory_destroy(b->memoria, true);
    }
}

static lxb_status_t lexborf_cb_acerto(lxb_dom_node_t *no, lxb_css_selector_specificity_t spec,
                                      void *ctx)
{
    (void) no;
    (void) spec;
    *(int *) ctx = 1;
    return LXB_STATUS_OK;
}

/* Compila o seletor; 0 = seletor invalido ou falta de memoria. */
static int lexborf_compila_seletor(lexborf_busca *b, const uint8_t *sel, size_t n)
{
    b->memoria = lxb_css_memory_create();
    if (b->memoria == NULL || lxb_css_memory_init(b->memoria, 128) != LXB_STATUS_OK) {
        return 0;
    }
    b->parser = lxb_css_parser_create();
    if (b->parser == NULL || lxb_css_parser_init(b->parser, NULL) != LXB_STATUS_OK) {
        return 0;
    }
    lxb_css_parser_memory_set(b->parser, b->memoria);
    b->seletores_css = lxb_css_selectors_create();
    if (b->seletores_css == NULL || lxb_css_selectors_init(b->seletores_css) != LXB_STATUS_OK) {
        return 0;
    }
    lxb_css_parser_selectors_set(b->parser, b->seletores_css);
    b->lista = lxb_css_selectors_parse(b->parser, sel, n);
    if (b->parser->status != LXB_STATUS_OK || b->lista == NULL) {
        return 0;
    }
    b->casador = lxb_selectors_create();
    return b->casador != NULL && lxb_selectors_init(b->casador) == LXB_STATUS_OK;
}

/* Proximo no em pre-ordem dentro da subarvore de raiz; NULL ao fim. */
static lxb_dom_node_t *lexborf_proximo_pre_ordem(lxb_dom_node_t *no, lxb_dom_node_t *raiz)
{
    if (no->first_child != NULL) {
        return no->first_child;
    }
    while (no != raiz) {
        if (no->next != NULL) {
            return no->next;
        }
        no = no->parent;
    }
    return NULL;
}

/* Percorre a subarvore de raiz; o contador k do no e o indice raiz+k no vetor plano. */
static int32_t lexborf_percorre(const lexborf_doc *d, int32_t raiz, lexborf_busca *b,
                                int32_t *out, int32_t cap)
{
    lxb_dom_node_t *raiz_no = d->nos[raiz].no;
    lxb_dom_node_t *no = raiz_no->first_child;
    int32_t idx = raiz;
    int32_t total = 0;

    while (no != NULL) {
        int acertou = 0;

        idx++;
        if (no->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            (void) lxb_selectors_match_node(b->casador, no, b->lista,
                                            lexborf_cb_acerto, &acertou);
            lxb_selectors_clean(b->casador);
        }
        if (acertou) {
            if (out != NULL && total < cap) {
                out[total] = idx;
            }
            total++;
        }
        no = lexborf_proximo_pre_ordem(no, raiz_no);
    }
    return total;
}

int32_t lexborf_find_all(lexborf_doc *d, int32_t root, const uint8_t *selector,
                         size_t sel_len, int32_t *out, int32_t cap)
{
    lexborf_busca b;
    int32_t total;

    if (!lexborf_valido(d, root) || selector == NULL) {
        return -1;
    }
    memset(&b, 0, sizeof(b));
    if (!lexborf_compila_seletor(&b, selector, sel_len)) {
        lexborf_busca_libera(&b);
        return -1;
    }
    total = lexborf_percorre(d, root, &b, out, cap);
    lexborf_busca_libera(&b);
    return total;
}

/* ---- CSS ---- */

typedef struct {
    size_t sel_ini;
    size_t sel_n;
    uint32_t espec;
    int32_t decl_ini;
    int32_t decl_n;
} lexborf_regra;

typedef struct {
    size_t nome_ini;
    size_t nome_n;
    size_t val_ini;
    size_t val_n;
    int32_t importante;
} lexborf_decl;

struct lexborf_sheet {
    uint8_t *buf;
    size_t buf_n;
    size_t buf_cap;
    lexborf_regra *regras;
    size_t regras_n;
    size_t regras_cap;
    lexborf_decl *decls;
    size_t decls_n;
    size_t decls_cap;
    int32_t atrules;
};

static lxb_status_t lexborf_cb_anexa(const lxb_char_t *dados, size_t len, void *ctx)
{
    lexborf_sheet *s = (lexborf_sheet *) ctx;

    if (!lexborf_cresce((void **) &s->buf, &s->buf_cap, s->buf_n + len, sizeof(uint8_t))) {
        return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }
    memcpy(s->buf + s->buf_n, dados, len);
    s->buf_n += len;
    return LXB_STATUS_OK;
}

static int lexborf_anexa_nul(lexborf_sheet *s)
{
    uint8_t zero = 0;

    return lexborf_cb_anexa((const lxb_char_t *) &zero, 1, s) == LXB_STATUS_OK;
}

/* Separa o valor do texto "nome: valor[ !important]" serializado pelo lexbor. */
static void lexborf_recorta_valor(const lexborf_sheet *s, size_t nome_ini, size_t nome_n,
                                  size_t tot_ini, size_t tot_n, int importante,
                                  size_t *vi, size_t *vn)
{
    size_t prefixo = nome_n + 2;
    size_t sn = sizeof(LEXBORF_SUFIXO_IMPORTANTE) - 1;
    int tem_prefixo = tot_n >= prefixo
        && memcmp(s->buf + nome_ini, s->buf + tot_ini, nome_n) == 0
        && s->buf[tot_ini + nome_n] == ':'
        && s->buf[tot_ini + nome_n + 1] == ' ';

    *vi = tem_prefixo ? tot_ini + prefixo : tot_ini;
    *vn = tem_prefixo ? tot_n - prefixo : tot_n;
    if (importante && *vn >= sn
        && memcmp(s->buf + *vi + *vn - sn, LEXBORF_SUFIXO_IMPORTANTE, sn) == 0) {
        *vn -= sn;
    }
}

static int lexborf_coleta_decl(lexborf_sheet *s, lxb_css_rule_declaration_t *dec)
{
    size_t nome_ini = s->buf_n;
    size_t nome_n;
    size_t tot_ini;
    size_t tot_n;
    size_t vi;
    size_t vn;
    lexborf_decl dd;

    if (lxb_css_rule_declaration_serialize_name(dec, lexborf_cb_anexa, s) != LXB_STATUS_OK) {
        return 0;
    }
    nome_n = s->buf_n - nome_ini;
    if (!lexborf_anexa_nul(s)) {
        return 0;
    }
    tot_ini = s->buf_n;
    if (lxb_css_rule_declaration_serialize(dec, lexborf_cb_anexa, s) != LXB_STATUS_OK) {
        return 0;
    }
    tot_n = s->buf_n - tot_ini;
    dd.importante = dec->important ? 1 : 0;
    lexborf_recorta_valor(s, nome_ini, nome_n, tot_ini, tot_n, dd.importante, &vi, &vn);
    /* o valor ganha copia propria terminada em NUL: o texto serializado nao tem NUL */
    if (!lexborf_cresce((void **) &s->buf, &s->buf_cap, s->buf_n + vn + 1, sizeof(uint8_t))) {
        return 0;
    }
    memcpy(s->buf + s->buf_n, s->buf + vi, vn);
    s->buf[s->buf_n + vn] = 0;
    dd.nome_ini = nome_ini;
    dd.nome_n = nome_n;
    dd.val_ini = s->buf_n;
    dd.val_n = vn;
    s->buf_n += vn + 1;
    if (!lexborf_cresce((void **) &s->decls, &s->decls_cap, s->decls_n + 1, sizeof(*s->decls))) {
        return 0;
    }
    s->decls[s->decls_n++] = dd;
    return 1;
}

static int lexborf_coleta_regra(lexborf_sheet *s, lxb_css_rule_style_t *estilo)
{
    lexborf_regra reg;
    lxb_css_rule_t *r;

    memset(&reg, 0, sizeof(reg));
    reg.sel_ini = s->buf_n;
    if (estilo->selector != NULL) {
        if (lxb_css_selector_serialize_list_chain(estilo->selector, lexborf_cb_anexa, s)
            != LXB_STATUS_OK) {
            return 0;
        }
        reg.espec = estilo->selector->specificity & LEXBORF_MASCARA_ESP;
    }
    reg.sel_n = s->buf_n - reg.sel_ini;
    if (!lexborf_anexa_nul(s)) {
        return 0;
    }
    reg.decl_ini = (int32_t) s->decls_n;
    if (estilo->declarations != NULL) {
        for (r = estilo->declarations->first; r != NULL; r = r->next) {
            if (r->type == LXB_CSS_RULE_DECLARATION) {
                if (!lexborf_coleta_decl(s, (lxb_css_rule_declaration_t *) r)) {
                    return 0;
                }
                reg.decl_n++;
            }
        }
    }
    if (!lexborf_cresce((void **) &s->regras, &s->regras_cap, s->regras_n + 1, sizeof(*s->regras))) {
        return 0;
    }
    s->regras[s->regras_n++] = reg;
    return 1;
}

static int lexborf_coleta(lexborf_sheet *s, lxb_css_stylesheet_t *sst)
{
    lxb_css_rule_t *r;

    if (sst->root == NULL) {
        return 1;
    }
    for (r = ((lxb_css_rule_list_t *) sst->root)->first; r != NULL; r = r->next) {
        if (r->type == LXB_CSS_RULE_AT_RULE) {
            s->atrules++;
        } else if (r->type == LXB_CSS_RULE_STYLE) {
            if (!lexborf_coleta_regra(s, (lxb_css_rule_style_t *) r)) {
                return 0;
            }
        }
    }
    return 1;
}

void lexborf_css_free(lexborf_sheet *s)
{
    if (s == NULL) {
        return;
    }
    free(s->buf);
    free(s->regras);
    free(s->decls);
    free(s);
}

lexborf_sheet *lexborf_css_parse(const uint8_t *css, size_t len)
{
    lexborf_sheet *s;
    lxb_css_parser_t *p = NULL;
    lxb_css_stylesheet_t *sst = NULL;
    lxb_status_t st = LXB_STATUS_ERROR;
    int ok = 0;

    if (css == NULL && len > 0) {
        return NULL;
    }
    s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    p = lxb_css_parser_create();
    if (p == NULL || lxb_css_parser_init(p, NULL) != LXB_STATUS_OK) {
        goto fim;
    }
    sst = lxb_css_stylesheet_create(NULL);
    if (sst == NULL) {
        goto fim;
    }
    st = lxb_css_stylesheet_parse(sst, p, css ? css : LEXBORF_VAZIO, len);
    if (st == LXB_STATUS_OK) {
        ok = lexborf_coleta(s, sst);
    }
fim:
    if (p != NULL) {
        (void) lxb_css_parser_destroy(p, true);
    }
    if (sst != NULL) {
        (void) lxb_css_stylesheet_destroy(sst, true);
    }
    if (!ok) {
        lexborf_css_free(s);
        return NULL;
    }
    return s;
}

static const lexborf_regra *lexborf_regra_em(const lexborf_sheet *s, int32_t r)
{
    if (s == NULL || r < 0 || (size_t) r >= s->regras_n) {
        return NULL;
    }
    return &s->regras[r];
}

static const lexborf_decl *lexborf_decl_em(const lexborf_sheet *s, int32_t r, int32_t k)
{
    const lexborf_regra *g = lexborf_regra_em(s, r);

    if (g == NULL || k < 0 || k >= g->decl_n) {
        return NULL;
    }
    return &s->decls[(size_t) g->decl_ini + (size_t) k];
}

int32_t lexborf_css_rule_count(const lexborf_sheet *s)
{
    return s != NULL ? (int32_t) s->regras_n : 0;
}

int32_t lexborf_css_atrule_count(const lexborf_sheet *s)
{
    return s != NULL ? s->atrules : 0;
}

const uint8_t *lexborf_css_rule_selector(const lexborf_sheet *s, int32_t r, size_t *len)
{
    const lexborf_regra *g = lexborf_regra_em(s, r);

    lexborf_set_len(len, 0);
    if (g == NULL) {
        return NULL;
    }
    lexborf_set_len(len, g->sel_n);
    return g->sel_n == 0 ? LEXBORF_VAZIO : s->buf + g->sel_ini;
}

int32_t lexborf_css_rule_specificity(const lexborf_sheet *s, int32_t r)
{
    const lexborf_regra *g = lexborf_regra_em(s, r);

    return g != NULL ? (int32_t) g->espec : 0;
}

int32_t lexborf_css_decl_count(const lexborf_sheet *s, int32_t r)
{
    const lexborf_regra *g = lexborf_regra_em(s, r);

    return g != NULL ? g->decl_n : 0;
}

const uint8_t *lexborf_css_decl_name(const lexborf_sheet *s, int32_t r, int32_t k, size_t *len)
{
    const lexborf_decl *dd = lexborf_decl_em(s, r, k);

    lexborf_set_len(len, 0);
    if (dd == NULL) {
        return NULL;
    }
    lexborf_set_len(len, dd->nome_n);
    return dd->nome_n == 0 ? LEXBORF_VAZIO : s->buf + dd->nome_ini;
}

const uint8_t *lexborf_css_decl_value(const lexborf_sheet *s, int32_t r, int32_t k, size_t *len)
{
    const lexborf_decl *dd = lexborf_decl_em(s, r, k);

    lexborf_set_len(len, 0);
    if (dd == NULL) {
        return NULL;
    }
    lexborf_set_len(len, dd->val_n);
    return dd->val_n == 0 ? LEXBORF_VAZIO : s->buf + dd->val_ini;
}

int32_t lexborf_css_decl_important(const lexborf_sheet *s, int32_t r, int32_t k)
{
    const lexborf_decl *dd = lexborf_decl_em(s, r, k);

    return dd != NULL ? dd->importante : 0;
}

const char *lexborf_version(void)
{
    return LEXBOR_VERSION_STRING;
}
