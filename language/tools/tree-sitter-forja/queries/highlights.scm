; highlights.scm - cores da Forja (.fj). Versao 0.5.0 (0.5.0: palavra ends do foreign (grammar 0.9.0); 0.4.0: palavras creates/destroys/takes do foreign (grammar 0.8.0); 0.3.0: sem mudanca de consulta - integer hex/bin e c_name do foreign_proc ja caem em @number/@string; 0.2.0: v1.11 L17: quat, static_assert de topo, '_' de descarte). Verificacao: python verificar.py --consultas
; Capturas no padrao tree-sitter (@keyword, @type, @function, @string, @number, @comment, @operator ...).

; comentarios
(comment) @comment

; literais
(string) @string
(fstring) @string
(fstring_content) @string
(format_spec) @string.special
(escape_sequence) @string.escape
(interpolation "{" @punctuation.special)
(interpolation "}" @punctuation.special)
(integer) @number
(real) @number
(boolean) @constant.builtin
(none) @constant.builtin

; palavras-chave
[
  "proc" "const" "let" "var" "struct" "enum" "union" "unit" "foreign" "link" "opaque"
  "test" "defer" "import"
] @keyword

; clausula de posse do foreign (v0.8.0): contextual, so depois da assinatura do proc foreign
[ "creates" "destroys" "takes" "ends" ] @keyword

(alias) @namespace

[ "if" "else" "for" "in" "while" "switch" "case" "default" "break" "continue" "return" ] @keyword.control

[ "try" "or_else" "as" ] @keyword.operator
[ "and" "or" "not" ] @keyword.operator
(modifier) @keyword

; tipos
(type_identifier) @type
((type_identifier) @type.builtin
  (#match? @type.builtin "^(i8|i16|i32|i64|u8|u16|u32|u64|int|uint|f32|f64|bool|str|cstr|rawptr|quat)$"))
"dynamic" @type.builtin
(pointer_type "^" @type)
(optional_type "?" @type)

; definicoes
(proc_declaration name: (identifier) @function)
(foreign_proc name: (identifier) @function)
(test_declaration name: (string) @string.special)
(const_declaration name: (identifier) @constant)
(unit_declaration name: (identifier) @type)
(enum_member name: (identifier) @constant)
(parameter name: (identifier) @variable.parameter)
(tuple_element name: (identifier) @variable.parameter)
(field_declaration name: (identifier) @property)
(field_initializer name: (identifier) @property)
(named_argument name: (identifier) @variable.parameter)
(field_expression field: (identifier) @property)
(enum_path enum: (identifier) @type member: (identifier) @constant)

; chamadas (new e make entram como call_expression com function identifier)
(call_expression function: (identifier) @function.call)
((call_expression function: (identifier) @function.builtin)
  (#match? @function.builtin "^(new|make|append|print|expect|transmute)$"))
(static_assert_declaration function: (identifier) @function.builtin)

; '_ = f()' descarta o resultado: o '_' nao e variavel
((assignment_statement target: (identifier) @variable.builtin)
  (#eq? @variable.builtin "_"))

; operadores e pontuacao
[
  "+" "-" "*" "/" "%" "==" "!=" "<" "<=" ">" ">=" "=" "+=" "-="
  "&" "|" "~" "<<" ">>" "->" "!" "?" "^" ".." "..<" "..="
] @operator

[ "(" ")" "[" "]" "{" "}" ] @punctuation.bracket
[ "," ":" "." ] @punctuation.delimiter
