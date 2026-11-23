/**
 * grammar.js - gramatica tree-sitter da linguagem Forja (.fj), escrita a mao (nao ha treino).
 * Objetivo: (1) CodeGraph caseiro (cg_search/cg_definition/cg_callers em .fj); (2) cores em editor/GitHub.
 * Versao: 0.9.0   Cobre a sintaxe do compilador ate a Linguagem v1.9 (frontend/parser_*.c e lexer*.c e a verdade) mais o L17 da v1.11 (abaixo).
 * Changelog: 0.9.0 - M6d P92h: 'ends' sem parametro como 4a clausula de posse do foreign_proc (ownership_clause ganha 'ends'; mesma posicao das outras, sem reservar).
 *             0.8.0 - M6c P86: clausula de posse no foreign_proc (ownership_clause: 'creates [p]', 'destroys p', 'takes p'; palavras so valem ali, sem reservar).
 *             0.7.0 - v1.11: chamada postfix (_chamada): 'f(x)(y)', 'obj.campo(a, b = 1)', 'tabela[i].metodo(x)'; 'nome(args)' segue em call_expression.
 *             0.6.0 - v1.10/v1.11: foreign com alias 'proc c_x = "x"(...)' (campo c_name); inteiro hex/bin (0xFF, 0b1010, com '_');
 *                    virgula final em literal de struct e de array; tipo composto (^T ?T [N]T []T) como argumento (size_of/align_of).
 *             0.5.0 - v1.11 L17: static_assert de topo (static_assert_declaration; 'static_assert' NAO e palavra reservada: dentro de proc
 *                    continua call_expression) e lista de capacidade fixa '[dynamic, N]T' (campo capacity de dynamic_array_type).
 *                    'quat' (nome de tipo e literal 'quat{...}') e '_ = f()' (descarte) ja caem em type_identifier/struct_literal e
 *                    assignment_statement sem mudar a gramatica: so o highlights.scm ganhou 'quat' e o '_' de descarte.
 *             0.4.0 - v1.9: tipos map[K]V, bit_set[E], soa[N]S, soa[dynamic]S, matrix[R, C]T e indice duplo m[i, j] (campo index2).
 *             0.3.0 - v1.8b: opaque_declaration ('opaque Nome' dentro de foreign), valor explicito de membro de enum
 *                    ('nome = 5', campo value); proc_type ja cobria 'proc(a: ^i32, b: ^i32) -> i32' como tipo de parametro.
 *             0.2.0 - v1.8: import_declaration ('import "caminho" [as apelido]', so no topo), qualified_name (apelido.nome em
 *                    tipo, chamada, literal de struct e alvo de case; alias = no 'alias'); conflitos GLR declarados: primario x
 *                    qualified_name e tipo x qualified_name; 'case a.b:' resolvido por prec em enum_path.
 *             0.1.0 - primeira versao: decls (unit const struct enum union proc test foreign, pub/export), comandos
 *                    (let/var multiplo, atribuicao, if/else if, while, for, switch, return, break, continue, defer),
 *                    expressoes com precedencia do parser, tipos (^T ?T [N]T []T [dynamic]T proc(...)), f"..." com {expr:formato}.
 * Verificacao: python verificar.py   (gera o parser, compila a DLL e confere ZERO erros nos .fj validos do projeto)
 * Reversao: apagar esta pasta e as linhas "forja" do indexador.py do CodeGraph (EXT_TREESITTER, _LANGS_SUPORTADAS, TS_TIPOS_*).
 *
 * DECISAO - quebra de linha como terminador (a Forja nao tem ';'):
 *   - src/scanner.c e um scanner externo que emite o token _nl SOMENTE quando a gramatica o admite naquele ponto
 *     (valid_symbols[NL]); nos demais pontos a quebra de linha e espaco comum (extras). O _nl engole de uma vez
 *     espacos, comentarios '//' e linhas em branco seguintes (o lexer da Forja tambem os colapsa num NL so) e,
 *     no fim do arquivo sem '\n', vale como um NL de largura zero (o parser do compilador aceita EOF no lugar do NL).
 *   - Cada comando/declaracao termina em _nl; o bloco '{' exige _nl logo depois (EsperarAbreBloco).
 *   - Vantagem: sem ambiguidade nem regra de "newline opcional" espalhada; uma 2a opcao (NL como token comum
 *     da gramatica) foi descartada porque exigiria opcional(_nl) em todo lugar onde o compilador pula quebras.
 *
 * DECISAO - '{' do cabecalho (if/while/for/switch): no cabecalho '{' abre o bloco e nunca um literal de struct/array
 *   (p->sem_literal_struct do compilador). Resolvido SEM conflito GLR com duas familias de expressao geradas por
 *   familia(): '_expr' (completa) e '_expr_cab' (sem literal); parenteses, colchetes e argumentos voltam para a completa.
 *   Os nos sao renomeados por alias, entao a arvore tem so binary_expression, call_expression etc.
 *
 * COMO ESTENDER (v1.8 import/escopo/Arena, v1.9 map/bit_set/soa/matrix): acrescentar a palavra em _item (decl nova),
 *   em _statement (comando novo) ou em _type (tipo novo); expressao nova entra em familia() para valer nas duas familias.
 *   Depois: tree-sitter generate, python verificar.py, e (se o no define simbolo) uma linha em TS_TIPOS_DEFINICAO["forja"]
 *   do indexador.py. Nomes de campo usados pelo indexador: name (definicoes) e function (chamadas): mantenha.
 */

const PREC = { OR_ELSE: 1, OR: 2, AND: 3, CMP: 4, ADD: 5, MUL: 6, CAST: 7, UNARY: 8, POSTFIX: 9 };

const BINARIOS = [
  [PREC.OR_ELSE, 'or_else'],
  [PREC.OR, 'or'],
  [PREC.AND, 'and'],
  [PREC.CMP, ['==', '!=', '<', '<=', '>', '>=']],
  [PREC.ADD, ['+', '-', '|', '~']],
  [PREC.MUL, ['*', '/', '%', '&', '<<', '>>']],
];

function commaSep(rule) {
  return optional(commaSep1(rule));
}

function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

/**
 * Familia de expressoes. sfx = '' (completa) ou '_cab' (cabecalho: sem literal de struct/array).
 * Operandos recursivos apontam para a propria familia; o que fica entre delimitadores usa a completa ($._expr).
 */
function familia(sfx, cheia) {
  const e = ($) => $['_expr' + sfx];
  const regras = {};

  regras['_expr' + sfx] = ($) => choice(
    alias($['_bin' + sfx], $.binary_expression),
    alias($['_cast' + sfx], $.cast_expression),
    alias($['_unary' + sfx], $.unary_expression),
    alias($['_campo' + sfx], $.field_expression),
    alias($['_deref' + sfx], $.dereference_expression),
    alias($['_indice' + sfx], $.index_expression),
    alias($['_fatia' + sfx], $.slice_expression),
    alias($['_chamada' + sfx], $.call_expression),
    $['_primario' + sfx],
  );

  regras['_bin' + sfx] = ($) => choice(...BINARIOS.map(([p, op]) =>
    prec.left(p, seq(field('left', e($)), field('operator', Array.isArray(op) ? choice(...op) : op), field('right', e($)))))
  );

  regras['_cast' + sfx] = ($) => prec.left(PREC.CAST, seq(field('value', e($)), 'as', field('type', $._type)));

  regras['_unary' + sfx] = ($) => prec(PREC.UNARY, seq(field('operator', choice('-', 'not', '&', '~', 'try')), field('operand', e($))));

  regras['_campo' + sfx] = ($) => prec(PREC.POSTFIX, seq(field('object', e($)), '.', field('field', $.identifier)));

  regras['_deref' + sfx] = ($) => prec(PREC.POSTFIX, seq(field('pointer', e($)), '^'));

  regras['_indice' + sfx] = ($) => prec(PREC.POSTFIX, seq(
    field('object', e($)), '[', field('index', $._expr), optional(seq(',', field('index2', $._expr))), ']'));

  regras['_fatia' + sfx] = ($) => prec(PREC.POSTFIX, seq(
    field('object', e($)), '[',
    choice('..', seq(field('start', $._expr), choice('..<', '..='), field('end', $._expr))),
    ']'));

  // v1.11: chamada sobre expressao que nao e nome puro (obj.campo(a, b=1), tabela[i].m(x), f(x)(y), (f)(x)).
  // 'nome(args)' e 'apelido.nome(args)' continuam em call_expression (_primario), para o indexador ver o nome.
  regras['_chamada' + sfx] = ($) => prec(PREC.POSTFIX, seq(
    field('function', choice(
      alias($['_campo' + sfx], $.field_expression),
      alias($['_indice' + sfx], $.index_expression),
      alias($['_chamada' + sfx], $.call_expression),
      $.call_expression,
      $.parenthesized_expression,
    )),
    field('arguments', $.argument_list),
  ));

  regras['_primario' + sfx] = ($) => choice(
    $.integer, $.real, $.string, $.fstring, $.boolean, $.none,
    $.call_expression,
    $.identifier,
    $.parenthesized_expression,
    ...(cheia ? [$.struct_literal, $.array_literal] : []),
  );

  return regras;
}

module.exports = grammar({
  name: 'forja',

  word: ($) => $.identifier,

  externals: ($) => [$._nl, $._erro_sentinela],

  extras: ($) => [/[ \t\r\f\n]/, $.comment],

  // 'apelido.nome' (qualified_name) x 'objeto.campo' (field_expression) so se distinguem olhando o que vem depois
  // ('(' ou '{' = qualificado); o GLR resolve, sem decidir pelo compilador (que usa a tabela de apelidos).
  conflicts: ($) => [
    [$._primario, $.qualified_name],
    [$._primario_cab, $.qualified_name],
    [$._type, $.qualified_name],
  ],

  rules: {
    source_file: ($) => seq(repeat($.import_declaration), repeat($._item)),

    // import so vale no topo do arquivo (o compilador rejeita depois de qualquer declaracao)
    import_declaration: ($) => seq(
      'import', field('path', $.string),
      optional(seq('as', field('alias', alias($.identifier, $.alias)))),
      $._nl,
    ),

    // apelido.nome (modulo importado): onde cabe nome de tipo, de chamada e alvo de case
    qualified_name: ($) => seq(field('module', alias($.identifier, $.alias)), '.', field('name', $.identifier)),

    // ---------------------------------------------------------------- declaracoes
    _item: ($) => seq(
      repeat(field('modifier', alias(choice('pub', 'export'), $.modifier))),
      choice(
        $.unit_declaration,
        $.const_declaration,
        $.struct_declaration,
        $.enum_declaration,
        $.union_declaration,
        $.proc_declaration,
        $.test_declaration,
        $.foreign_block,
        $.static_assert_declaration,
      ),
    ),

    // v1.11 L9: static_assert(cond, msg = "...") no topo. Palavra por regex (nao string) para nao reservar o nome:
    // so e valida neste ponto da gramatica, e o identifier segue valendo para a chamada dentro de proc.
    static_assert_declaration: ($) => seq(
      field('function', alias(/static_assert/, $.identifier)), field('arguments', $.argument_list), $._nl,
    ),

    unit_declaration: ($) => seq(
      'unit', field('name', $.identifier),
      choice(
        seq(':', field('base', $._type)),
        seq('=', field('left', $.identifier), field('operator', choice('*', '/')), field('right', $.identifier)),
      ),
      $._nl,
    ),

    const_declaration: ($) => seq(
      'const', field('name', $.identifier), ':', field('type', $._type), '=', field('value', $._expr), $._nl,
    ),

    struct_declaration: ($) => seq(
      'struct', field('name', alias($.identifier, $.type_identifier)),
      '{', $._nl, repeat($.field_declaration), '}', $._nl,
    ),

    field_declaration: ($) => seq(
      field('name', $.identifier), ':', field('type', $._type),
      optional($.field_range),
      optional(seq('=', field('default', $._expr))),
      $._nl,
    ),

    field_range: ($) => seq('in', $._limit, choice('..=', '..<'), $._limit),

    _limit: ($) => seq(optional('-'), $.integer),

    enum_declaration: ($) => seq(
      'enum', field('name', alias($.identifier, $.type_identifier)),
      '{', optional($._nl), repeat($.enum_member), '}', $._nl,
    ),

    // v1.8b: valor explicito 'nome = 5' (inteiro com '-' opcional, como o AnalisarLimite do compilador)
    enum_member: ($) => seq(
      field('name', $.identifier), optional(seq('=', field('value', $._limit))), optional(','), optional($._nl),
    ),

    union_declaration: ($) => seq(
      'union', field('name', alias($.identifier, $.type_identifier)),
      '{', optional($._nl), repeat($.union_variant), '}', $._nl,
    ),

    union_variant: ($) => seq(field('type', $._type), optional(','), optional($._nl)),

    proc_declaration: ($) => seq(
      'proc', field('name', $.identifier), $.parameter_list,
      optional(seq('->', field('return_type', $._return_type))),
      field('body', $.block), $._nl,
    ),

    parameter_list: ($) => seq('(', commaSep($.parameter), ')'),

    parameter: ($) => seq(
      field('name', $.identifier), ':', field('type', $._type),
      optional(seq('=', field('default', $._expr))),
    ),

    _return_type: ($) => choice(
      seq($._type, optional(seq('!', field('error', alias($.identifier, $.type_identifier))))),
      seq('!', field('error', alias($.identifier, $.type_identifier))),
      $.tuple_return,
    ),

    tuple_return: ($) => seq(
      '(', commaSep1($.tuple_element), ')',
      optional(seq('!', field('error', alias($.identifier, $.type_identifier)))),
    ),

    tuple_element: ($) => seq(field('name', $.identifier), ':', field('type', $._type)),

    test_declaration: ($) => seq('test', field('name', $.string), field('body', $.block), $._nl),

    foreign_block: ($) => seq(
      'foreign', '{', $._nl, repeat1(choice($.link_declaration, $.opaque_declaration, $.foreign_proc)), '}', $._nl,
    ),

    // v1.8b: 'opaque Nome' (tipo cujo layout fica na biblioteca C; sem campos)
    opaque_declaration: ($) => seq('opaque', field('name', alias($.identifier, $.type_identifier)), $._nl),

    link_declaration: ($) => seq('link', field('library', $.string), $._nl),

    foreign_proc: ($) => seq(
      'proc', field('name', $.identifier), optional(seq('=', field('c_name', $.string))), $.parameter_list,
      optional(seq('->', field('return_type', $._return_type))),
      optional($.ownership_clause),
      $._nl,
    ),

    // v0.8.0 (D-M6c-3): clausula de posse SUFIXO do proc foreign, no maximo uma. 'creates', 'destroys' e 'takes'
    // NAO sao palavras reservadas: o word (identifier) so as devolve como palavra onde esta regra as admite.
    // v0.9.0 (D-M6d-7, P92h): 'ends' sem parametro (o proc encerra a lib inteira); mesma regra contextual.
    ownership_clause: ($) => choice(
      seq('creates', optional(field('param', $.identifier))),
      seq('destroys', field('param', $.identifier)),
      seq('takes', field('param', $.identifier)),
      'ends',
    ),

    // ---------------------------------------------------------------- tipos
    _type: ($) => choice(
      alias($.identifier, $.type_identifier),
      $.qualified_name,
      $.pointer_type,
      $.optional_type,
      $.array_type,
      $.slice_type,
      $.dynamic_array_type,
      $.map_type,
      $.bit_set_type,
      $.soa_type,
      $.matrix_type,
      $.proc_type,
    ),

    pointer_type: ($) => seq('^', field('element', $._type)),
    optional_type: ($) => seq('?', field('element', $._type)),
    array_type: ($) => seq('[', field('size', choice($.integer, $.identifier)), ']', field('element', $._type)),
    slice_type: ($) => seq('[', ']', field('element', $._type)),
    // v1.11 L15a: '[dynamic, N]T' (lista de capacidade fixa; N literal inteiro ou nome de const)
    dynamic_array_type: ($) => seq(
      '[', 'dynamic', optional(seq(',', field('capacity', choice($.integer, $.identifier)))), ']', field('element', $._type)),
    map_type: ($) => seq('map', '[', field('key', $._type), ']', field('value', $._type)),
    bit_set_type: ($) => seq('bit_set', '[', field('element', $._type), ']'),
    soa_type: ($) => seq('soa', '[', field('size', choice($.integer, $.identifier, 'dynamic')), ']', field('element', $._type)),
    matrix_type: ($) => seq('matrix', '[', field('rows', choice($.integer, $.identifier)), ',', field('columns', choice($.integer, $.identifier)), ']', field('element', $._type)),

    proc_type: ($) => seq(
      'proc', '(', commaSep(choice(seq($.identifier, ':', $._type), $._type)), ')',
      optional(seq('->', field('return_type', $._type))),
    ),

    // ---------------------------------------------------------------- comandos
    block: ($) => seq('{', $._nl, repeat($._statement), '}'),

    _statement: ($) => choice(
      $.let_statement,
      $.assignment_statement,
      $.expression_statement,
      $.if_statement,
      $.while_statement,
      $.for_statement,
      $.switch_statement,
      $.return_statement,
      $.break_statement,
      $.continue_statement,
      $.defer_statement,
    ),

    let_statement: ($) => seq(
      choice('let', 'var'),
      field('name', $.identifier), repeat(seq(',', field('name', $.identifier))),
      optional(seq(':', field('type', $._type))),
      '=', field('value', $._expr), $._nl,
    ),

    assignment_statement: ($) => seq(
      field('target', $._expr), field('operator', choice('=', '+=', '-=')), field('value', $._expr), $._nl,
    ),

    expression_statement: ($) => seq($._expr, $._nl),

    if_statement: ($) => seq($._if_corpo, $._nl),

    // 'else if' reaproveita o corpo sem o _nl (so o if de fora consome a quebra de linha, como no compilador)
    _if_corpo: ($) => seq(
      'if', field('condition', $._expr_cab), field('consequence', $.block),
      optional(field('alternative', $.else_clause)),
    ),

    else_clause: ($) => seq('else', choice($.block, alias($._if_corpo, $.if_statement))),

    while_statement: ($) => seq('while', field('condition', $._expr_cab), field('body', $.block), $._nl),

    for_statement: ($) => seq(
      'for', $.for_binding, 'in', field('iterable', $._expr_cab),
      optional(seq(choice('..<', '..='), field('end', $._expr_cab))),
      field('body', $.block), $._nl,
    ),

    for_binding: ($) => seq(
      optional('&'), field('name', $.identifier),
      optional(seq(',', optional('&'), field('name', $.identifier))),
    ),

    switch_statement: ($) => seq(
      'switch', optional(seq(field('binding', $.identifier), 'in')), field('value', $._expr_cab),
      '{', $._nl, repeat($.case_clause), '}', $._nl,
    ),

    case_clause: ($) => seq(
      choice(seq('case', field('target', $._case_target)), 'default'), ':',
      optional($._nl), repeat($._statement),
    ),

    _case_target: ($) => choice($.none, $.enum_path, $._type),

    // 'case a.b:' e igual na forma para enum_path e tipo qualificado de uniao: prec escolhe enum_path (so o compilador
    // sabe se 'a' e apelido); 'case a.b.c:' nao tem ambiguidade (o 1o par e o apelido).
    enum_path: ($) => prec(1, seq(field('enum', choice($.identifier, $.qualified_name)), '.', field('member', $.identifier))),

    return_statement: ($) => seq('return', optional(commaSep1($._expr)), $._nl),

    break_statement: ($) => seq('break', $._nl),

    continue_statement: ($) => seq('continue', $._nl),

    defer_statement: ($) => seq('defer', choice($.expression_statement, $.assignment_statement)),

    // ---------------------------------------------------------------- expressoes
    ...familia('', true),
    ...familia('_cab', false),

    parenthesized_expression: ($) => seq('(', $._expr, ')'),

    // new(T, ...) e make(T, ...) levam um TIPO no 1o argumento
    call_expression: ($) => choice(
      seq(field('function', choice($.identifier, $.qualified_name)), field('arguments', $.argument_list)),
      seq(field('function', alias(choice('new', 'make'), $.identifier)), field('arguments', alias($._argumentos_de_tipo, $.argument_list))),
    ),

    argument_list: ($) => seq('(', commaSep($._argument), ')'),

    _argumentos_de_tipo: ($) => seq('(', $._type, repeat(seq(',', $._argument)), ')'),

    // size_of(^T), align_of([3]f32): tipo composto como argumento (comeca por ^ ? [ , que nenhuma expressao usa)
    _argument: ($) => choice($.named_argument, $._expr, $.pointer_type, $.optional_type, $.array_type, $.slice_type),

    named_argument: ($) => seq(field('name', $.identifier), '=', field('value', $._expr)),

    struct_literal: ($) => seq(
      field('type', choice(alias($.identifier, $.type_identifier), $.qualified_name)),
      '{', commaSep($.field_initializer), optional(','), '}',
    ),

    field_initializer: ($) => seq(field('name', $.identifier), '=', field('value', $._expr)),

    array_literal: ($) => seq('{', commaSep($._expr), optional(','), '}'),

    // ---------------------------------------------------------------- literais
    integer: () => token(choice(/0[xX][0-9A-Fa-f_]+/, /0[bB][01_]+/, /[0-9]+/)),

    real: () => /[0-9]+\.[0-9]+/,

    string: () => token(/"([^"\\\n]|\\[^\n])*"/),

    fstring: ($) => seq(
      'f"',
      repeat(choice($.fstring_content, $.escape_sequence, $.interpolation)),
      token.immediate('"'),
    ),

    fstring_content: () => token.immediate(prec(1, /[^"{}\\\n]+/)),

    escape_sequence: () => token.immediate(prec(1, choice(/\\[^\n]/, '{{', '}}'))),

    interpolation: ($) => seq(
      token.immediate('{'), $._expr_cab, optional(seq(':', $.format_spec)), '}',
    ),

    format_spec: () => token.immediate(/[^}"\n]+/),

    boolean: () => choice('true', 'false'),

    none: () => 'none',

    identifier: () => /[A-Za-z_][A-Za-z0-9_]*/,

    comment: () => token(seq('//', /[^\n]*/)),
  },
});
