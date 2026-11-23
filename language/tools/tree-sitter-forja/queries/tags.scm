; tags.scm - definicoes e referencias da Forja (padrao tree-sitter tags: @definition.* / @reference.*, @name).
; Versao 0.1.0. O CodeGraph caseiro (indexador.py) NAO le este arquivo: usa os mesmos nos por TS_TIPOS_DEFINICAO["forja"]
; (campo name) e TS_TIPOS_CHAMADA["forja"] (campo function). Mantenha os dois alinhados.

(proc_declaration name: (identifier) @name) @definition.function
(foreign_proc name: (identifier) @name) @definition.function
(struct_declaration name: (type_identifier) @name) @definition.class
(enum_declaration name: (type_identifier) @name) @definition.type
(union_declaration name: (type_identifier) @name) @definition.type
(const_declaration name: (identifier) @name) @definition.constant
(unit_declaration name: (identifier) @name) @definition.type
(test_declaration name: (string) @name) @definition.function

(call_expression function: (identifier) @name) @reference.call
(type_identifier) @name @reference.type
