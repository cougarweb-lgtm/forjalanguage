# -*- coding: utf-8 -*-
"""casos_m_d4.py -- os casos do verificar_m_d4.py (metrica M-D4 do contrato IA1): UMA lista com as 44 entradas da tabela 3.4 (idiomas de outras linguagens) e, para
cada entrada, ao menos um caso: a forma de outra linguagem (errado), o programa certo (certo), a linha do 1o diagnostico, a palavra da Forja que a forma_certa tem de
citar (chave) e a confianca da sugestao (certa = `forja corrigir --aplicar` deixa o texto igual ao certo; so_dica = o certo e conferido a mao).
Os idiomas lexicos e sintaticos (L1) vem de casos_e0110_l1.py e so ganham aqui a entrada da tabela; os semanticos (L2) estao aqui. Dados puros.
Versao: 0.1.1
Changelog: 0.1.1 - O0 P13: entrada "a ^ b" (44 entradas; xor_circunflexo).
           0.1.0 - IA1-L2: primeira versao (43 entradas, 81 casos).
Verificacao: python -I verificar_m_d4.py --selftest
"""

from casos_e0110_l1 import CASOS as CASOS_L1, ERRO_E_LER, Caso, Programa

# As entradas da tabela 3.4 do contrato (a ordem da tabela). "main -> int" cobre main -> i32 e main -> int; "for (;;)" e o for de C.
ENTRADAS = [
    ";", "&&", "||", "!", "i++", "i--", "?:", "if c { x }", "fn", "func", "def", "function", "main -> int", ":=",
    "nil", "null", "NULL", "nullptr", "None", "range(n)", "for (;;)", "elif", "string", "String", "a + b", "[1, 2]", "/* */",
    "printf", "console.log", "fmt.Println", "x?", "x.len()", "x.length", "x.size()", "x.append(v)", "x.push(v)", "import os",
    "throw", "raise", "try/catch", "True", "TRUE", "std:proc", "a ^ b",
]

# A entrada de cada caso do L1, pelo nome do caso.
ENTRADA_DO_L1 = {
    "pv_fim_da_linha": ";", "pv_no_meio": ";", "for_de_c": "for (;;)", "for_com_parenteses_e_in": "for (;;)", "e_logico": "&&", "e_logico_colado": "&&",
    "ou_logico": "||", "negacao": "!", "negacao_com_parenteses": "!", "incremento": "i++", "decremento": "i--", "incremento_prefixo": "i++", "ternario": "?:",
    "try_de_rust": "x?", "bloco_de_um_comando_na_linha": "if c { x }", "bloco_com_else_na_linha": "if c { x }", "proc_inteiro_na_linha": "if c { x }",
    "bloco_na_linha_crlf": "if c { x }", "fn": "fn", "func": "func", "def": "def", "function": "function", "fn_depois_de_pub": "fn", "atribuicao_curta": ":=",
    "elif": "elif", "comentario_de_bloco_numa_linha": "/* */", "comentario_de_bloco_no_fim_da_linha": "/* */", "comentario_de_bloco_em_varias_linhas": "/* */",
    "try_catch": "try/catch", "main_devolve_int": "main -> int", "main_devolve_i32": "main -> int",
    "xor_circunflexo": "a ^ b",
}


def Semantico(Entrada, Nome, Errado, Certo, Linha, Chave, Confianca="certa", Codigo="E0110"):
    """Um caso do L2: o caso do L1 mais a entrada da tabela e o codigo esperado do 1o diagnostico."""
    Dados = Caso(Nome, Errado, Certo, Linha, Chave, Confianca)
    Dados.update(entrada=Entrada, codigo=Codigo)
    return Dados


def Nulo(Entrada, Palavra):
    """let p: ?int = <Palavra>: o nulo de outra linguagem no lugar de none."""
    Corpo = "    let p: ?int = {}\n    print(p or_else 7)\n"
    return Semantico(Entrada, f"nulo_{Palavra}", Programa(Corpo.format(Palavra)), Programa(Corpo.format("none")), 2, "none")


def Booleano(Entrada, Palavra, Certa):
    Corpo = "    let ligado = {}\n    if ligado {{\n        print(1)\n    }}\n"
    return Semantico(Entrada, f"bool_{Palavra}", Programa(Corpo.format(Palavra)), Programa(Corpo.format(Certa)), 2, Certa)


def Tamanho(Entrada, Nome, Forma, Linha=3):
    """print(<Forma>) sobre o array xs: x.len(), x.length, x.size() e x.len viram len(xs)."""
    Base = "    let xs: [3]int = {{1, 2, 3}}\n    print({})\n"
    return Semantico(Entrada, Nome, Programa(Base.format(Forma)), Programa(Base.format("len(xs)")), Linha, "len(x)")


LISTA = "    var l = make([dynamic]int, alloc = heap, cap = 4)\n    defer delete(l)\n"
LANCA_MODELO = ERRO_E_LER.replace("return Erro.falhou", "@@") + "proc main() {\n    print(ler(1) or_else 0)\n}\n"


def Lanca(Comando):
    """O programa de ERRO_E_LER com o comando dado no lugar do `return Erro.falhou` (linha 5)."""
    return LANCA_MODELO.replace("@@", Comando)

CASOS_L2 = [
    Nulo("nil", "nil"), Nulo("null", "null"), Nulo("NULL", "NULL"), Nulo("nullptr", "nullptr"), Nulo("None", "None"), Nulo("null", "undefined"),
    Semantico("nil", "nulo_no_retorno", "proc achar(n: int) -> ?int {\n    if n > 0 {\n        return n\n    }\n    return nil\n}\n\nproc main() {\n    print(achar(1) or_else 0)\n}\n",
              "proc achar(n: int) -> ?int {\n    if n > 0 {\n        return n\n    }\n    return none\n}\n\nproc main() {\n    print(achar(1) or_else 0)\n}\n", 5, "none"),
    Semantico("range(n)", "range_um_argumento", Programa("    for i in range(3) {\n        print(i)\n    }\n"), Programa("    for i in 0..<3 {\n        print(i)\n    }\n"), 2, "..<"),
    Semantico("range(n)", "range_dois_argumentos", Programa("    for i in range(1, 4) {\n        print(i)\n    }\n"), Programa("    for i in 1..<4 {\n        print(i)\n    }\n"), 2, "..<"),
    Semantico("range(n)", "range_de_len", Programa("    let xs: [3]int = {1, 2, 3}\n    for i in range(len(xs)) {\n        print(xs[i])\n    }\n"),
              Programa("    let xs: [3]int = {1, 2, 3}\n    for i in 0..<len(xs) {\n        print(xs[i])\n    }\n"), 3, "..<"),
    Semantico("range(n)", "range_com_passo", Programa("    for i in range(0, 10, 2) {\n        print(i)\n    }\n"), Programa("    for i in 0..<5 {\n        print(i * 2)\n    }\n"), 2, "..<", "so_dica"),
    Semantico("string", "tipo_string", Programa("    let s: string = \"oi\"\n    print(s)\n"), Programa("    let s: str = \"oi\"\n    print(s)\n"), 2, "str"),
    Semantico("String", "tipo_String", Programa("    let s: String = \"oi\"\n    print(s)\n"), Programa("    let s: str = \"oi\"\n    print(s)\n"), 2, "str"),
    Semantico("String", "tipo_String_no_parametro", "proc dizer(s: String) {\n    print(s)\n}\n\nproc main() {\n    dizer(\"oi\")\n}\n",
              "proc dizer(s: str) {\n    print(s)\n}\n\nproc main() {\n    dizer(\"oi\")\n}\n", 1, "str"),
    Semantico("a + b", "soma_de_dois_literais", Programa("    print(\"a\" + \"b\")\n"), Programa("    print(f\"ab\")\n"), 2, "f\"", "certa", "E0307"),
    Semantico("a + b", "soma_de_dois_nomes", Programa("    let a = \"x\"\n    let b = \"y\"\n    print(a + b)\n"),
              Programa("    let a = \"x\"\n    let b = \"y\"\n    print(f\"{a}{b}\")\n"), 4, "f\"", "certa", "E0307"),
    Semantico("[1, 2]", "array_de_inteiros", Programa("    let xs = [1, 2, 3]\n    print(xs[0])\n"), Programa("    let xs: [3]int = {1, 2, 3}\n    print(xs[0])\n"), 2, "{"),
    Semantico("[1, 2]", "array_de_textos", Programa("    let nomes = [\"a\", \"b\"]\n    print(nomes[0])\n"),
              Programa("    let nomes: [2]str = {\"a\", \"b\"}\n    print(nomes[0])\n"), 2, "{"),
    Semantico("[1, 2]", "array_com_tipo_escrito", Programa("    let xs: [2]int = [4, 5]\n    print(xs[1])\n"), Programa("    let xs: [2]int = {4, 5}\n    print(xs[1])\n"), 2, "{"),
    Semantico("[1, 2]", "array_misto", Programa("    let xs = [1, 2.5]\n    print(len(xs))\n"), Programa("    let xs: [2]f32 = {1.0, 2.5}\n    print(len(xs))\n"), 2, "{", "so_dica"),
    Semantico("[1, 2]", "array_ao_estilo_go", Programa("    let xs = []int{1, 2}\n    print(xs[0])\n"), Programa("    let xs: [2]int = {1, 2}\n    print(xs[0])\n"), 2, "{", "so_dica"),
    Semantico("printf", "printf_com_formato", Programa("    let n = 3\n    printf(\"valor %d\", n)\n"), Programa("    let n = 3\n    print(f\"valor {n}\")\n"), 3, "print", "so_dica"),
    Semantico("printf", "printf_simples", Programa("    printf(\"oi\")\n"), Programa("    print(\"oi\")\n"), 2, "print", "so_dica"),
    Semantico("printf", "println_solto", Programa("    println(\"oi\")\n"), Programa("    print(\"oi\")\n"), 2, "print"),
    Semantico("printf", "puts", Programa("    puts(\"oi\")\n"), Programa("    print(\"oi\")\n"), 2, "print"),
    Semantico("console.log", "console_log", Programa("    console.log(\"oi\")\n"), Programa("    print(\"oi\")\n"), 2, "print"),
    Semantico("console.log", "console_log_de_variavel", Programa("    let n = 3\n    console.log(n)\n"), Programa("    let n = 3\n    print(n)\n"), 3, "print"),
    Semantico("console.log", "console_log_com_dois_argumentos", Programa("    let n = 3\n    console.log(\"n:\", n)\n"), Programa("    let n = 3\n    print(f\"n: {n}\")\n"), 3, "print", "so_dica"),
    Semantico("fmt.Println", "fmt_println", Programa("    fmt.Println(\"oi\")\n"), Programa("    print(\"oi\")\n"), 2, "print"),
    Semantico("fmt.Println", "fmt_printf", Programa("    let n = 3\n    fmt.Printf(\"%d\", n)\n"), Programa("    let n = 3\n    print(n)\n"), 3, "print", "so_dica"),
    Tamanho("x.len()", "tamanho_len_chamada", "xs.len()"),
    Tamanho("x.length", "tamanho_length", "xs.length"),
    Tamanho("x.size()", "tamanho_size_chamada", "xs.size()"),
    Tamanho("x.len()", "tamanho_len_campo", "xs.len"),
    Semantico("x.len()", "tamanho_em_conta", Programa("    let xs: [3]int = {1, 2, 3}\n    let n = xs.len() + 1\n    print(n)\n"),
              Programa("    let xs: [3]int = {1, 2, 3}\n    let n = len(xs) + 1\n    print(n)\n"), 3, "len(x)"),
    Semantico("x.len()", "tamanho_do_texto", Programa("    let s = \"abc\"\n    print(s.len())\n"), Programa("    let s = \"abc\"\n    print(len(s))\n"), 3, "len(x)"),
    Semantico("x.length", "tamanho_da_lista", Programa(LISTA + "    append(&l, item = 1)\n    print(l.length)\n"), Programa(LISTA + "    append(&l, item = 1)\n    print(len(l))\n"), 5, "len(x)"),
    Semantico("x.append(v)", "append_metodo", Programa(LISTA + "    l.append(7)\n    print(len(l))\n"), Programa(LISTA + "    append(&l, item = 7)\n    print(len(l))\n"), 4, "append(&x"),
    Semantico("x.push(v)", "push_metodo", Programa(LISTA + "    l.push(7)\n    print(len(l))\n"), Programa(LISTA + "    append(&l, item = 7)\n    print(len(l))\n"), 4, "append(&x"),
    Semantico("x.push(v)", "push_de_variavel", Programa(LISTA + "    let v = 7\n    l.push(v)\n    print(len(l))\n"),
              Programa(LISTA + "    let v = 7\n    append(&l, item = v)\n    print(len(l))\n"), 5, "append(&x"),
    Semantico("x.push(v)", "push_com_dois_valores", Programa(LISTA + "    l.push(1, 2)\n    print(len(l))\n"),
              Programa(LISTA + "    append(&l, item = 1)\n    append(&l, item = 2)\n    print(len(l))\n"), 4, "append(&x", "so_dica"),
    Semantico("import os", "import_com_aspas", 'import "os"\n\nproc main() {\n    os.exit(3)\n}\n', 'import "std:os"\n\nproc main() {\n    os.exit(3)\n}\n', 1, "std:"),
    Semantico("import os", "import_sem_aspas", "import os\n\nproc main() {\n    os.exit(3)\n}\n", 'import "std:os"\n\nproc main() {\n    os.exit(3)\n}\n', 1, "std:"),
    Semantico("throw", "throw_de_membro", Lanca("throw Erro.falhou"), Lanca("return Erro.falhou"), 5, "return"),
    Semantico("raise", "raise_de_membro", Lanca("raise Erro.falhou"), Lanca("return Erro.falhou"), 5, "return"),
    Semantico("throw", "throw_new", Lanca("throw new Erro(\"falhou\")"), Lanca("return Erro.falhou"), 5, "return", "so_dica"),
    Booleano("True", "True", "true"), Booleano("TRUE", "TRUE", "true"), Booleano("True", "False", "false"), Booleano("TRUE", "FALSE", "false"),
    Semantico("std:proc", "std_proc_com_apelido", 'import "std:proc" as os\n\nproc main() {\n    os.exit(0)\n}\n', 'import "std:os" as os\n\nproc main() {\n    os.exit(0)\n}\n', 1, "std:os",
              "certa", "E1001"),
]


def MontarCasos():
    """Os casos do L1 (com a entrada da tabela) e os do L2, nesta ordem."""
    Casos = []
    for Caso_L1 in CASOS_L1:
        Casos.append(dict(Caso_L1, entrada=ENTRADA_DO_L1[Caso_L1["nome"]], codigo="E0110"))
    return Casos + CASOS_L2


CASOS = MontarCasos()
