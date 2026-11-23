/*
 * lexborf.h - cola C99 entre o lexbor 3.0.0 e a ponte Forja (vendor html).
 *
 * Por que existe: a API do lexbor usa inline e struct com ponteiro de funcao;
 * a Forja so enxerga esta cola. O DOM vira um vetor PLANO por indice:
 * o no 0 e o documento; os demais na ordem de pre-ordem. Indice -1 = nenhum.
 *
 * Dono: cada lexborf_doc e cada lexborf_sheet e dono da propria memoria
 * (liberada por lexborf_free / lexborf_css_free). Nao ha estado global mutavel:
 * dois documentos ao mesmo tempo funcionam. Ponteiros devolvidos (texto, nome,
 * valor) valem ate a liberacao do objeto dono.
 *
 * Namespace (lexborf_namespace): 1 = html, 2 = mathml, 3 = svg, 0 = outro/nenhum.
 * Tipo de no (lexborf_node_type): numero do lxb_dom_node_type_t do lexbor
 * (element=1, text=3, comment=8, document=9, doctype=10, ...).
 *
 * Especificidade (lexborf_css_rule_specificity): empacotamento do lexbor
 * (lxb_css_selector_specificity_t) com o bit de !important (bit 31) e os
 * bits de S (bit 27) zerados: A = (v >> 18) & 0x1FF, B = (v >> 9) & 0x1FF,
 * C = v & 0x1FF. Ex.: "#a .b p" -> A=1, B=1, C=1.
 */
#ifndef LEXBORF_H
#define LEXBORF_H

#include <stddef.h>
#include <stdint.h>

typedef struct lexborf_doc lexborf_doc;
typedef struct lexborf_sheet lexborf_sheet;

/* ---- HTML ---- */

lexborf_doc *lexborf_parse(const uint8_t *html, size_t len);
void lexborf_free(lexborf_doc *d);
int32_t lexborf_node_count(const lexborf_doc *d);
int32_t lexborf_node_type(const lexborf_doc *d, int32_t i);
int32_t lexborf_parent(const lexborf_doc *d, int32_t i);
int32_t lexborf_first_child(const lexborf_doc *d, int32_t i);
int32_t lexborf_next_sibling(const lexborf_doc *d, int32_t i);
const uint8_t *lexborf_tag_name(const lexborf_doc *d, int32_t i, size_t *len);
int32_t lexborf_namespace(const lexborf_doc *d, int32_t i);
int32_t lexborf_attr_count(const lexborf_doc *d, int32_t i);
const uint8_t *lexborf_attr_name(const lexborf_doc *d, int32_t i, int32_t k, size_t *len);
const uint8_t *lexborf_attr_value(const lexborf_doc *d, int32_t i, int32_t k, size_t *len);
const uint8_t *lexborf_text(const lexborf_doc *d, int32_t i, size_t *len);
int32_t lexborf_find_all(lexborf_doc *d, int32_t root, const uint8_t *selector,
                         size_t sel_len, int32_t *out, int32_t cap);

/* ---- CSS ---- */

lexborf_sheet *lexborf_css_parse(const uint8_t *css, size_t len);
void lexborf_css_free(lexborf_sheet *s);
int32_t lexborf_css_rule_count(const lexborf_sheet *s);
int32_t lexborf_css_atrule_count(const lexborf_sheet *s);
const uint8_t *lexborf_css_rule_selector(const lexborf_sheet *s, int32_t r, size_t *len);
int32_t lexborf_css_rule_specificity(const lexborf_sheet *s, int32_t r);
int32_t lexborf_css_decl_count(const lexborf_sheet *s, int32_t r);
const uint8_t *lexborf_css_decl_name(const lexborf_sheet *s, int32_t r, int32_t k, size_t *len);
const uint8_t *lexborf_css_decl_value(const lexborf_sheet *s, int32_t r, int32_t k, size_t *len);
int32_t lexborf_css_decl_important(const lexborf_sheet *s, int32_t r, int32_t k);

/* ---- versao ---- */

const char *lexborf_version(void);

#endif /* LEXBORF_H */
