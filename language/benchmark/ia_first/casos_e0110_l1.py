# -*- coding: utf-8 -*-
"""casos_e0110_l1.py -- os casos do verificar_e0110_l1.py: uma forma de outra linguagem (errado), o programa certo (certo), a linha do E0110, a palavra da Forja que a
forma_certa tem de citar (chave) e a confianca da sugestao (certa = `forja corrigir --aplicar` deixa o texto igual ao certo; so_dica = o certo e conferido a mao).
Dados puros: o P-IA1-L2 acrescenta os idiomas semanticos (nil/null, range, string, x.len(), import "os", throw, True...) no fim de CASOS.
Versao: 0.1.2
Changelog: 0.1.2 - O0 P13: caso xor_circunflexo ('a ^ b' -> 'a ~ b', IDI_XOR_CIRCUNFLEXO).
           0.1.1 - P72: ';' no meio da linha, ++i, for (x in xs), for de C com contador simples e main -> int/i32 passam de so_dica a provavel (a tabela common/diag_corrige.c ganhou a edicao de uma linha; nao chega ao programa certo sozinha, entao nao e certa).
           0.1.0 - IA1-L1: 33 casos dos idiomas lexicos e sintaticos da tabela 3.4.
Verificacao: python -I verificar_e0110_l1.py --selftest
"""


def Programa(Corpo):
    """proc main() com o corpo dado (cada linha ja com 4 espacos)."""
    return "proc main() {\n" + Corpo + "}\n"


def Caso(Nome, Errado, Certo, Linha, Chave, Confianca="certa"):
    return {"nome": Nome, "errado": Errado, "certo": Certo, "linha": Linha, "chave": Chave, "confianca": Confianca}


ERRO_E_LER = """enum Erro { falhou }

proc ler(n: int) -> int ! Erro {
    if n < 0 {
        return Erro.falhou
    }
    return n
}

"""

CASOS = [
    Caso("pv_fim_da_linha", Programa("    let x = 5;\n    print(x)\n"), Programa("    let x = 5\n    print(x)\n"), 2, "';'"),
    Caso("pv_no_meio", Programa("    let x = 5; print(x)\n"), Programa("    let x = 5\n    print(x)\n"), 2, "';'", "provavel"),
    Caso("for_de_c", Programa("    for (var i = 0; i < 3; i++) {\n        print(i)\n    }\n"),
         Programa("    for i in 0..<3 {\n        print(i)\n    }\n"), 2, "for i in 0..<n", "provavel"),
    Caso("for_com_parenteses_e_in", Programa("    let xs: [3]int = {1, 2, 3}\n    for (x in xs) {\n        print(x)\n    }\n"),
         Programa("    let xs: [3]int = {1, 2, 3}\n    for x in xs {\n        print(x)\n    }\n"), 3, "for i in 0..<n", "provavel"),
    Caso("e_logico", Programa("    let a = true\n    let b = false\n    if a && b {\n        print(1)\n    }\n"),
         Programa("    let a = true\n    let b = false\n    if a and b {\n        print(1)\n    }\n"), 4, "'and'"),
    Caso("e_logico_colado", Programa("    let a = true\n    let b = false\n    if a&&b {\n        print(1)\n    }\n"),
         Programa("    let a = true\n    let b = false\n    if a and b {\n        print(1)\n    }\n"), 4, "'and'"),
    Caso("ou_logico", Programa("    let a = true\n    let b = false\n    if a || b {\n        print(1)\n    }\n"),
         Programa("    let a = true\n    let b = false\n    if a or b {\n        print(1)\n    }\n"), 4, "'or'"),
    Caso("negacao", Programa("    let a = true\n    if !a {\n        print(1)\n    }\n"), Programa("    let a = true\n    if not a {\n        print(1)\n    }\n"), 3, "'not'"),
    Caso("negacao_com_parenteses", Programa("    let a = true\n    let b = false\n    if !(a and b) {\n        print(1)\n    }\n"),
         Programa("    let a = true\n    let b = false\n    if not (a and b) {\n        print(1)\n    }\n"), 4, "'not'"),
    Caso("incremento", Programa("    var i = 0\n    i++\n    print(i)\n"), Programa("    var i = 0\n    i += 1\n    print(i)\n"), 3, "+= 1"),
    Caso("decremento", Programa("    var i = 5\n    i--\n    print(i)\n"), Programa("    var i = 5\n    i -= 1\n    print(i)\n"), 3, "-= 1"),
    Caso("incremento_prefixo", Programa("    var i = 0\n    ++i\n    print(i)\n"), Programa("    var i = 0\n    i += 1\n    print(i)\n"), 3, "+= 1", "provavel"),
    Caso("ternario", Programa("    let a = 3\n    let b = a > 2 ? 1 : 0\n    print(b)\n"),
         Programa("    let a = 3\n    var b = 0\n    if a > 2 {\n        b = 1\n    }\n    print(b)\n"), 3, "if com else", "so_dica"),
    Caso("try_de_rust", ERRO_E_LER + "proc obter() -> int ! Erro {\n    let v = ler(3)?\n    return v\n}\n\nproc main() {\n    print(obter() or_else 0)\n}\n",
         ERRO_E_LER + "proc obter() -> int ! Erro {\n    let v = try ler(3)\n    return v\n}\n\nproc main() {\n    print(obter() or_else 0)\n}\n", 11, "try"),
    Caso("bloco_de_um_comando_na_linha", Programa("    let a = 3\n    if a > 2 { print(1) }\n    print(2)\n"),
         Programa("    let a = 3\n    if a > 2 {\n        print(1)\n    }\n    print(2)\n"), 3, "linhas proprias"),
    Caso("bloco_com_else_na_linha", Programa("    let a = 3\n    if a > 2 { print(1) } else { print(2) }\n"),
         Programa("    let a = 3\n    if a > 2 {\n        print(1)\n    } else {\n        print(2)\n    }\n"), 3, "linhas proprias", "so_dica"),
    Caso("proc_inteiro_na_linha", "proc main() { print(1) }\n", "proc main() {\n    print(1)\n}\n", 1, "linhas proprias"),
    Caso("bloco_na_linha_crlf", Programa("    let a = 3\r\n    if a > 2 { print(1) }\r\n").replace("{\n", "{\r\n").replace("}\n", "}\r\n"),
         Programa("    let a = 3\r\n    if a > 2 {\r\n        print(1)\r\n    }\r\n").replace("{\n", "{\r\n").replace("}\n", "}\r\n"), 3, "linhas proprias"),
    Caso("fn", "fn dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"),
         "proc dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"), 1, "proc"),
    Caso("func", "func dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"),
         "proc dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"), 1, "proc"),
    Caso("def", "def dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"),
         "proc dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"), 1, "proc"),
    Caso("function", "function dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"),
         "proc dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"), 1, "proc"),
    Caso("fn_depois_de_pub", "pub fn dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"),
         "pub proc dobro(n: int) -> int {\n    return n * 2\n}\n\n" + Programa("    print(dobro(2))\n"), 1, "proc"),
    Caso("atribuicao_curta", Programa("    x := 5\n    print(x)\n"), Programa("    let x = 5\n    print(x)\n"), 2, "let antes do nome"),
    Caso("elif", Programa("    let a = 3\n    if a > 5 {\n        print(1)\n    } elif a > 2 {\n        print(2)\n    } else {\n        print(3)\n    }\n"),
         Programa("    let a = 3\n    if a > 5 {\n        print(1)\n    } else if a > 2 {\n        print(2)\n    } else {\n        print(3)\n    }\n"), 5, "else if"),
    Caso("comentario_de_bloco_numa_linha", Programa("    /* um comentario */\n    print(1)\n"), Programa("    // um comentario\n    print(1)\n"), 2, "//"),
    Caso("comentario_de_bloco_no_fim_da_linha", Programa("    print(1) /* depois */\n"), Programa("    print(1) // depois\n"), 2, "//"),
    Caso("comentario_de_bloco_em_varias_linhas", Programa("    /* linha um\n       linha dois */\n    print(1)\n"),
         Programa("    // linha um\n    // linha dois\n    print(1)\n"), 2, "//", "so_dica"),
    Caso("try_catch", ERRO_E_LER + Programa("    try {\n        print(ler(1) or_else 0)\n    } catch e {\n        print(2)\n    }\n"),
         ERRO_E_LER + Programa("    print(ler(1) or_else 0)\n"), 11, "switch", "so_dica"),
    Caso("main_devolve_int", "proc main() -> int {\n    print(1)\n    return 0\n}\n", 'import "std:os"\n\nproc main() {\n    print(1)\n    os.exit(0)\n}\n', 1, "std:os", "provavel"),
    Caso("xor_circunflexo", Programa("    let a: u64 = 5\n    let b: u64 = 3\n    let x: u64 = a ^ b\n    print(x)\n"),
         Programa("    let a: u64 = 5\n    let b: u64 = 3\n    let x: u64 = a ~ b\n    print(x)\n"), 4, "a ~ b"),
    Caso("main_devolve_i32","proc main() -> i32 {\n    print(1)\n    return 0\n}\n", 'import "std:os"\n\nproc main() {\n    print(1)\n    os.exit(0)\n}\n', 1, "std:os", "provavel"),
]
