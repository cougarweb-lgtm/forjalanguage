# -*- coding: utf-8 -*-
"""Constroi os casos dourados do benchmark do ADR-01 (Forja).

Versao: 1.0.0 | 2026-11-07
Entrada: nenhuma. Saida: <caso>.fj + <caso>.esperado.txt (ok) ou <caso>.esperado.jsonl (erro)
e manifesto.json (SHA-256 de cada arquivo), todos nesta pasta.

Nos casos de erro, o marcador `@@` fica ANTES do caractere onde o contrato manda
reportar; o construtor tira o marcador e calcula linha e coluna (1-based, bytes).
Arquivos em LF, UTF-8 sem BOM. Reversao: apagar os arquivos gerados e rodar de novo.
"""
import hashlib
import json
import os
import sys

PASTA = os.path.dirname(os.path.abspath(__file__))
MARCADOR = "@@"

LISTA_OK = []    # (Nome, Fonte, Saida esperada)
LISTA_ERRO = []  # (Nome, Codigo, Fonte com marcador)


def CasoOk(Nome, Fonte, Saida):
    LISTA_OK.append((Nome, Fonte, Saida))


def CasoErro(Nome, Codigo, Fonte):
    LISTA_ERRO.append((Nome, Codigo, Fonte))


# ---------------------------------------------------------------- casos que compilam e rodam
CasoOk("ok_01_ola", """proc main() {
    print(42)
}
""", ["42"])

CasoOk("ok_02_aritmetica", """proc main() {
    print(2 + 3 * 4)
    print((2 + 3) * 4)
    print(17 / 5)
    print(17 % 5)
    print(-17 / 5)
    print(-17 % 5)
    let a = 10
    let b = 3
    print(a - b - 1)
    print(a > b)
    print(a == b)
    print(a != b)
    print(a <= 10)
}
""", ["14", "20", "3", "2", "-3", "-2", "6", "true", "false", "true", "true"])

CasoOk("ok_03_lacos", """proc main() {
    var soma = 0
    for i in 1..<11 {
        soma += i
    }
    print(soma)
    var n = 0
    while n < 5 {
        n += 1
    }
    print(n)
    if soma > 50 {
        print(1)
    } else if soma > 20 {
        print(2)
    } else {
        print(3)
    }
}
""", ["55", "5", "1"])

CasoOk("ok_04_const", """const LIMITE: int = 100
const PASSO: int = 7

proc main() {
    var x = 0
    while x < LIMITE {
        x += PASSO
    }
    print(x)
}
""", ["105"])

CasoOk("ok_05_struct_padrao", """struct Heroi {
    vida: i32 in 0..=100 = 100
    ouro: int
    forte: bool = true
    nivel: int = 1
}

proc main() {
    let h = Heroi{}
    print(h.vida)
    print(h.ouro)
    print(h.forte)
    print(h.nivel)
    var k = Heroi{ouro = 7, forte = false}
    k.nivel += 2
    print(k.ouro)
    print(k.forte)
    print(k.nivel)
}
""", ["100", "0", "true", "1", "7", "false", "3"])

CasoOk("ok_06_struct_ponteiro", """struct Grifo {
    vida: i32 in 0..=100 = 100
    velocidade: int = 8
    x: int = 0
}

proc mover(g: ^Grifo, passos: int) {
    g.x += g.velocidade * passos
    g.vida -= 10
}

proc main() {
    var g = Grifo{velocidade = 3}
    mover(&g, passos = 4)
    mover(&g, passos = 1)
    print(g.x)
    print(g.vida)
}
""", ["15", "80"])

CasoOk("ok_07_unidades", """unit Metros: f32
unit Segundos: f32
unit MetrosPorSegundo = Metros/Segundos

proc distancia(v: MetrosPorSegundo, dt: Segundos) -> Metros {
    return v * dt
}

proc main() {
    let v: MetrosPorSegundo = 8.0
    let t: Segundos = 0.5
    let d: Metros = distancia(v, dt = t)
    print(d as int)
    let r: MetrosPorSegundo = d / t
    print(r as int)
}
""", ["4", "8"])

CasoOk("ok_08_argumento_nomeado", """proc area(largura: int, altura: int = 2, escala: int = 1) -> int {
    return largura * altura * escala
}

proc main() {
    print(area(3))
    print(area(3, altura = 5))
    print(area(3, escala = 4))
    print(area(3, altura = 5, escala = 2))
}
""", ["6", "15", "24", "30"])

CasoOk("ok_09_recursao", """proc fatorial(n: int) -> int {
    if n <= 1 {
        return 1
    }
    return n * fatorial(n - 1)
}

proc fib(n: int) -> int {
    var a = 0
    var b = 1
    for i in 0..<n {
        let t = a + b
        a = b
        b = t
    }
    return a
}

proc main() {
    print(fatorial(10))
    print(fib(20))
}
""", ["3628800", "6765"])

CasoOk("ok_10_faixa_valida", """struct Alvo {
    vida: i32 in 0..=100 = 100
    nivel: int in 1..<10 = 1
}

proc main() {
    var a = Alvo{vida = 55, nivel = 9}
    print(a.vida)
    print(a.nivel)
    let b = Alvo{}
    print(b.vida)
    print(b.nivel)
}
""", ["55", "9", "100", "1"])

CasoOk("ok_11_conversao", """proc main() {
    let a: int = 7
    let f = a as f32
    let g = f / 2.0
    print(g as int)
    let h = 0.0 - g
    print(h as int)
    let c: i32 = a as i32
    print(c)
    let big = c as int
    print(big + 1)
}
""", ["3", "-3", "7", "8"])

CasoOk("ok_12_struct_com_unidade", """unit Metros: f32
unit Segundos: f32
unit MetrosPorSegundo = Metros/Segundos

struct Corpo {
    velocidade: MetrosPorSegundo = 8.0
    posicao: Metros = 0.0
}

proc mover(c: ^Corpo, dt: Segundos) {
    c.posicao += c.velocidade * dt
}

proc main() {
    var c = Corpo{velocidade = 10.0}
    mover(&c, dt = 0.5)
    mover(&c, dt = 0.25)
    print(c.posicao as int)
    var d = Corpo{}
    mover(&d, dt = 1.0)
    print(d.posicao as int)
}
""", ["7", "8"])

CasoOk("ok_13_logica", """proc main() {
    let a = true
    let b = false
    print(a and b)
    print(a or b)
    print(not b)
    print(a and not b)
    var n = 5
    if n > 3 and n < 10 {
        print(1)
    }
    if n < 3 or n == 5 {
        print(2)
    }
}
""", ["false", "true", "true", "true", "1", "2"])

CasoOk("ok_14_literal_de_struct", """struct Ponto {
    x: int
    y: int = 2
}

struct Caixa {
    largura: int = 4
    altura: int = 5
    origem: int = 0
}

proc area(c: ^Caixa) -> int {
    return c.largura * c.altura
}

proc main() {
    var p = Ponto{x = 3}
    print(p.x + p.y)
    var c = Caixa{altura = 10}
    print(area(&c))
    c.largura = 6
    print(area(&c))
}
""", ["5", "40", "60"])

CasoOk("ok_15_dimensao_derivada", """unit Metros: f32
unit Segundos: f32
unit Area = Metros*Metros
unit Velocidade = Metros/Segundos
unit Aceleracao = Velocidade/Segundos

proc main() {
    let lado: Metros = 3.0
    let a: Area = lado * lado
    print(a as int)
    let v: Velocidade = 6.0
    let t: Segundos = 2.0
    let acel: Aceleracao = v / t
    print(acel as int)
    let distancia: Metros = v * t
    print(distancia as int)
    let razao = distancia / lado
    print(razao as int)
}
""", ["9", "3", "12", "4"])

CasoOk("ok_16_proc_bool", """proc par(n: int) -> bool {
    return n % 2 == 0
}

proc contar(limite: int) -> int {
    var c = 0
    for i in 0..<limite {
        if par(i) {
            c += 1
        }
    }
    return c
}

proc main() {
    print(par(4))
    print(par(7))
    print(contar(10))
}
""", ["true", "false", "5"])

CasoOk("ok_17_declaracao_fora_de_ordem", """proc somar(p: ^Par) -> int {
    return p.a + p.b
}

struct Par {
    a: int = 1
    b: int = 2
}

proc main() {
    var p = Par{b = 5}
    print(somar(&p))
}
""", ["6"])

# ---------------------------------------------------------------- casos de erro (1 erro cada)
CasoErro("err_E0001_caractere", "E0001", """proc main() {
    let x = 5 @@$
    print(x)
}
""")

CasoErro("err_E0001_string_aberta", "E0001", """proc main() {
    let s = @@"abc
    print(1)
}
""")

CasoErro("err_E0101_nome_esperado", "E0101", """proc main() {
    let @@= 5
    print(1)
}
""")

CasoErro("err_E0101_expressao", "E0101", """proc main() {
    print(1 + @@)
}
""")

CasoErro("err_E0101_fim_de_arquivo", "E0101", """proc main() {
    print(1)
}
proc outro() {
    print(2)
@@""")

CasoErro("err_E0101_dois_comandos_na_linha", "E0101", """proc main() {
    let a = 1 @@let b = 2
    print(a)
}
""")

CasoErro("err_E0201_variavel", "E0201", """proc main() {
    print(@@y)
}
""")

CasoErro("err_E0201_campo", "E0201", """struct Grifo {
    vida: int = 1
}

proc main() {
    let g = Grifo{}
    print(g.@@mana)
}
""")

CasoErro("err_E0201_proc", "E0201", """proc main() {
    @@foo(1)
}
""")

CasoErro("err_E0201_tipo_de_parametro", "E0201", """proc f(a: @@Foo) {
    print(1)
}

proc main() {
    print(2)
}
""")

CasoErro("err_E0201_sem_main", "E0201", """@@proc outro() {
    print(1)
}
""")

CasoErro("err_E0301_real_em_i32", "E0301", """proc main() {
    let x: i32 = @@1.5
    print(x)
}
""")

CasoErro("err_E0301_i32_com_int", "E0301", """proc main() {
    let a: i32 = 1
    let b: int = @@a
    print(b)
}
""")

CasoErro("err_E0301_operador_bool", "E0301", """proc main() {
    print(1 @@+ true)
}
""")

CasoErro("err_E0301_print_array", "E0301", """proc main() {
    let v: [3]i32 = {}
    print(@@v)
}
""")

CasoErro("err_E0301_condicao", "E0301", """proc main() {
    if @@1 {
        print(1)
    }
}
""")

CasoErro("err_E0301_retorno", "E0301", """proc f() -> int {
    return @@true
}

proc main() {
    print(f())
}
""")

CasoErro("err_E0302_sem_return", "E0302", """proc f() -> int {
    let x = 1
@@}

proc main() {
    print(f())
}
""")

CasoErro("err_E0412_soma", "E0412", """unit Metros: f32
unit Segundos: f32

proc main() {
    let d: Metros = 1.0
    let t: Segundos = 2.0
    let s = d @@+ t
    print(1)
}
""")

CasoErro("err_E0412_inicializador", "E0412", """unit Metros: f32
unit Segundos: f32

proc main() {
    let d: Metros = 1.0
    let t: Segundos = @@d
    print(1)
}
""")

CasoErro("err_E0412_produto_sem_unidade", "E0412", """unit Metros: f32

proc main() {
    let lado: Metros = 3.0
    let a = lado @@* lado
    print(1)
}
""")

CasoErro("err_E0412_f32_com_unidade", "E0412", """unit Metros: f32

proc main() {
    let x: Metros = 1.0
    let z: f32 = 2.0
    let y = x @@+ z
    print(1)
}
""")

CasoErro("err_E0412_argumento", "E0412", """unit Metros: f32
unit Segundos: f32

proc andar(d: Metros) {
    print(1)
}

proc main() {
    let t: Segundos = 1.0
    andar(@@t)
}
""")

CasoErro("err_E0420_padrao_do_campo", "E0420", """struct Grifo {
    vida: i32 in 0..=100 = @@150
}

proc main() {
    print(1)
}
""")

CasoErro("err_E0420_literal_negativo", "E0420", """struct Grifo {
    vida: i32 in 0..=100 = 100
}

proc main() {
    var g = Grifo{vida = @@-1}
    print(g.vida)
}
""")

CasoErro("err_E0420_limite_exclusivo", "E0420", """struct Nivel {
    n: int in 0..<10 = @@10
}

proc main() {
    print(1)
}
""")

CasoErro("err_E0420_literal_acima", "E0420", """struct Grifo {
    vida: i32 in 0..=100 = 100
}

proc main() {
    var g = Grifo{vida = @@101}
    print(g.vida)
}
""")

CasoErro("err_E0501_fora_de_ordem", "E0501", """proc area(w: int, h: int = 2, e: int = 1) -> int {
    return w * h * e
}

proc main() {
    print(area(3, e = 1, @@h = 2))
}
""")

CasoErro("err_E0501_repetido", "E0501", """proc area(w: int, h: int = 2, e: int = 1) -> int {
    return w * h * e
}

proc main() {
    print(area(3, h = 1, @@h = 2))
}
""")

CasoErro("err_E0502_primeiro_nomeado", "E0502", """proc area(w: int, h: int = 2) -> int {
    return w * h
}

proc main() {
    print(area(w = 3, @@2))
}
""")  # IA1-L3b (V-06 = B): o 1o nomeado e valido; o erro agora e o posicional DEPOIS do nomeado

CasoErro("err_E0502_posicional_extra", "E0502", """proc area(w: int, h: int = 2) -> int {
    return w * h
}

proc main() {
    print(area(3, 2, @@1))
}
""")  # IA1-L3b (V-06 = B): o 2o posicional e valido; o erro agora e o argumento que sobra (3 para 2 parametros)

CasoErro("err_E0503_faltando", "E0503", """proc dois(w: int, h: int) -> int {
    return w * h
}

proc main() {
    print(@@dois(3))
}
""")

CasoErro("err_E0504_argumento_inexistente", "E0504", """proc area(w: int, h: int = 2) -> int {
    return w * h
}

proc main() {
    print(area(3, @@z = 1))
}
""")

CasoErro("err_E0504_campo_inexistente", "E0504", """struct Grifo {
    vida: int = 1
}

proc main() {
    var g = Grifo{@@mana = 1}
    print(g.vida)
}
""")


# ---------------------------------------------------------------- gravacao
def Gravar(Caminho, Texto):
    with open(Caminho, "wb") as Arquivo:
        Arquivo.write(Texto.encode("utf-8"))


def PosicaoDoMarcador(Fonte, Nome):
    if Fonte.count(MARCADOR) != 1:
        raise SystemExit(f"{Nome}: precisa de exatamente 1 marcador {MARCADOR}")
    Indice = Fonte.index(MARCADOR)
    Antes = Fonte[:Indice]
    Linha = Antes.count("\n") + 1
    UltimoLf = Antes.rfind("\n")
    Coluna = len(Antes[UltimoLf + 1:].encode("utf-8")) + 1
    return Fonte.replace(MARCADOR, "", 1), Linha, Coluna


def Construir():
    Manifesto = {"versao": "1.0.0", "casos": []}
    for Nome, Fonte, Saida in LISTA_OK:
        Gravar(os.path.join(PASTA, Nome + ".fj"), Fonte)
        Gravar(os.path.join(PASTA, Nome + ".esperado.txt"), "".join(Linha + "\n" for Linha in Saida))
        Manifesto["casos"].append({"nome": Nome, "tipo": "ok"})
    for Nome, Codigo, Fonte in LISTA_ERRO:
        Limpo, Linha, Coluna = PosicaoDoMarcador(Fonte, Nome)
        Gravar(os.path.join(PASTA, Nome + ".fj"), Limpo)
        Esperado = {"codigo": Codigo, "arquivo": Nome + ".fj", "linha": Linha, "coluna": Coluna}
        Gravar(os.path.join(PASTA, Nome + ".esperado.jsonl"),
               json.dumps(Esperado, ensure_ascii=False) + "\n")
        Manifesto["casos"].append({"nome": Nome, "tipo": "erro", "codigo": Codigo,
                                   "linha": Linha, "coluna": Coluna})
    for Caso in Manifesto["casos"]:
        for Sufixo in (".fj", ".esperado.txt", ".esperado.jsonl"):
            Caminho = os.path.join(PASTA, Caso["nome"] + Sufixo)
            if os.path.exists(Caminho):
                with open(Caminho, "rb") as Arquivo:
                    Chave = "sha256" + Sufixo.replace(".", "_")
                    Caso[Chave] = hashlib.sha256(Arquivo.read()).hexdigest()
    Gravar(os.path.join(PASTA, "manifesto.json"),
           json.dumps(Manifesto, indent=2, ensure_ascii=False) + "\n")
    print(f"ok={len(LISTA_OK)} erro={len(LISTA_ERRO)} total={len(LISTA_OK) + len(LISTA_ERRO)}")


if __name__ == "__main__":
    sys.exit(Construir())
