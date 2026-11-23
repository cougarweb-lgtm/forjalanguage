"""Gerador do corpus de benchmark da Forja (.fj) com a saída esperada.

Versão 1.0.0. Changelog: primeira versão com os moldes T1 a T6, PRNG splitmix64 próprio e --selftest.
Verificação: python gerar_corpus.py --selftest
"""

import argparse
import hashlib
import json
import os
import re
import sys
import tempfile

MASCARA_64 = (1 << 64) - 1
PASSO_SPLITMIX = 0x9E3779B97F4A7C15
MULT_SPLITMIX_1 = 0xBF58476D1CE4E5B9
MULT_SPLITMIX_2 = 0x94D049BB133111EB
# Divisor aprovado pelo principal: 61 linhas reais por bloco, contando separadores e o main.
LINHAS_POR_BLOCO = 61
SEMENTE_SELFTEST = 20261107
LINHAS_SELFTEST = 2000
CABECALHO_UNIDADES = [
    "unit Metros: f32",
    "unit Segundos: f32",
    "unit MetrosPorSegundo = Metros/Segundos",
]
# Fração de um real de quartos: 0 -> ".0", 1 -> ".25", 2 -> ".5", 3 -> ".75".
FRACAO_QUARTOS = {0: "0", 1: "25", 2: "5", 3: "75"}


class ErroSelftest(Exception):
    """Falha de verificação do próprio gerador."""


class Sorteador:
    """PRNG splitmix64 próprio, para a saída não mudar entre versões do Python."""

    def __init__(self, Semente):
        self.Estado = Semente & MASCARA_64

    def Proximo(self):
        self.Estado = (self.Estado + PASSO_SPLITMIX) & MASCARA_64
        Mistura = self.Estado
        Mistura = ((Mistura ^ (Mistura >> 30)) * MULT_SPLITMIX_1) & MASCARA_64
        Mistura = ((Mistura ^ (Mistura >> 27)) * MULT_SPLITMIX_2) & MASCARA_64
        return Mistura ^ (Mistura >> 31)

    def Inteiro(self, Minimo, Maximo):
        return Minimo + self.Proximo() % (Maximo - Minimo + 1)


def FormatarReal(Quartos):
    """Converte quartos (valor = Quartos/4) em literal real da Forja, sempre com ponto."""
    return f"{Quartos // 4}.{FRACAO_QUARTOS[Quartos % 4]}"


def MoldeT1(Sorteio, Bloco):
    BPadrao = Sorteio.Inteiro(1, 9)
    CPadrao = Sorteio.Inteiro(0, 99)
    Limiar = Sorteio.Inteiro(100, 4000)
    Ajuste = Sorteio.Inteiro(1, 50)
    ValorA = Sorteio.Inteiro(1, 50)
    BChamada = Sorteio.Inteiro(1, 9)
    CChamada = Sorteio.Inteiro(0, 99)
    Declaracao = "\n".join([
        f"proc f{Bloco}(a: int, b: int = {BPadrao}, c: int = {CPadrao}) -> int {{",
        "    var r = a * b + c",
        f"    if r > {Limiar} {{",
        f"        r = r - {Limiar}",
        "    } else {",
        f"        r = r + {Ajuste}",
        "    }",
        "    return r",
        "}",
    ])
    Chamada = [f"    print(f{Bloco}({ValorA}, b = {BChamada}, c = {CChamada}))"]
    Resultado = ValorA * BChamada + CChamada
    Resultado = Resultado - Limiar if Resultado > Limiar else Resultado + Ajuste
    return Declaracao, Chamada, [str(Resultado)]


def MoldeT2(Sorteio, Bloco):
    PassoPadrao = Sorteio.Inteiro(1, 9)
    Corte = Sorteio.Inteiro(50, 900)
    Quantidade = Sorteio.Inteiro(1, 40)
    PassoChamada = Sorteio.Inteiro(1, 9)
    Declaracao = "\n".join([
        f"proc s{Bloco}(n: int, passo: int = {PassoPadrao}) -> int {{",
        "    var acc = 0",
        "    for i in 0..<n {",
        "        acc += i * passo",
        f"        if acc > {Corte} {{",
        f"            acc -= {Corte}",
        "        }",
        "    }",
        "    return acc",
        "}",
    ])
    Chamada = [f"    print(s{Bloco}({Quantidade}, passo = {PassoChamada}))"]
    Acumulado = 0
    for Indice in range(Quantidade):
        Acumulado += Indice * PassoChamada
        if Acumulado > Corte:
            Acumulado -= Corte
    return Declaracao, Chamada, [str(Acumulado)]


def MoldeT3(Sorteio, Bloco):
    IncPadrao = Sorteio.Inteiro(1, 7)
    Limite = Sorteio.Inteiro(1, 500)
    IncChamada = Sorteio.Inteiro(1, 7)
    Declaracao = "\n".join([
        f"proc w{Bloco}(limite: int, inc: int = {IncPadrao}) -> int {{",
        "    var x = 0",
        "    var passos = 0",
        "    while x < limite {",
        "        x += inc",
        "        passos += 1",
        "    }",
        "    return passos",
        "}",
    ])
    Chamada = [f"    print(w{Bloco}({Limite}, inc = {IncChamada}))"]
    Acumulado = 0
    Passos = 0
    while Acumulado < Limite:
        Acumulado += IncChamada
        Passos += 1
    return Declaracao, Chamada, [str(Passos)]


def MoldeT4(Sorteio, Bloco):
    VidaInicial = Sorteio.Inteiro(20, 100)
    YPadrao = Sorteio.Inteiro(1, 9)
    Dano = Sorteio.Inteiro(1, 5)
    YChamada = Sorteio.Inteiro(1, 9)
    Passos1 = Sorteio.Inteiro(1, 30)
    Passos2 = Sorteio.Inteiro(1, 30)
    Declaracao = "\n".join([
        f"struct E{Bloco} {{",
        f"    vida: i32 in 0..=100 = {VidaInicial}",
        "    x: int = 0",
        f"    y: int = {YPadrao}",
        "}",
        "",
        f"proc m{Bloco}(e: ^E{Bloco}, passos: int) {{",
        "    e.x += passos * e.y",
        f"    e.vida -= {Dano}",
        "}",
    ])
    Chamada = [
        f"    var e{Bloco} = E{Bloco}{{y = {YChamada}}}",
        f"    m{Bloco}(&e{Bloco}, passos = {Passos1})",
        f"    m{Bloco}(&e{Bloco}, passos = {Passos2})",
        f"    print(e{Bloco}.x)",
        f"    print(e{Bloco}.vida)",
    ]
    X = (Passos1 + Passos2) * YChamada
    Vida = VidaInicial - 2 * Dano
    return Declaracao, Chamada, [str(X), str(Vida)]


def MoldeT5(Sorteio, Bloco):
    DtPadrao = Sorteio.Inteiro(1, 16)
    VelQuartos = Sorteio.Inteiro(1, 32)
    DtQuartos = Sorteio.Inteiro(1, 32)
    DtTexto = FormatarReal(DtPadrao)
    VelTexto = FormatarReal(VelQuartos)
    Dt2Texto = FormatarReal(DtQuartos)
    Declaracao = "\n".join([
        f"proc d{Bloco}(v: MetrosPorSegundo, dt: Segundos = {DtTexto}) -> Metros {{",
        "    return v * dt",
        "}",
    ])
    Chamada = [
        f"    let dist{Bloco}: Metros = d{Bloco}({VelTexto}, dt = {Dt2Texto})",
        f"    print(dist{Bloco} as int)",
    ]
    return Declaracao, Chamada, [str((VelQuartos * DtQuartos) // 16)]


def MoldeT6(Sorteio, Bloco):
    MPadrao = Sorteio.Inteiro(2, 9)
    Corte = Sorteio.Inteiro(0, 40)
    Numero = Sorteio.Inteiro(1, 400)
    MChamada = Sorteio.Inteiro(2, 9)
    Declaracao = "\n".join([
        f"proc p{Bloco}(n: int, m: int = {MPadrao}) -> bool {{",
        f"    return n % m == 0 and n > {Corte}",
        "}",
    ])
    Chamada = [f"    print(p{Bloco}({Numero}, m = {MChamada}))"]
    Verdadeiro = Numero % MChamada == 0 and Numero > Corte
    return Declaracao, Chamada, ["true" if Verdadeiro else "false"]


MOLDES = [MoldeT1, MoldeT2, MoldeT3, MoldeT4, MoldeT5, MoldeT6]


def GerarCorpus(Semente, Linhas):
    """Devolve (texto_fj, texto_esperado) para a semente e as linhas pedidas."""
    QuantidadeBlocos = max(1, round(Linhas / LINHAS_POR_BLOCO))
    Sorteio = Sorteador(Semente)
    Declaracoes = []
    Chamadas = []
    Saidas = []
    for Bloco in range(QuantidadeBlocos):
        for Molde in MOLDES:
            Declaracao, Chamada, Saida = Molde(Sorteio, Bloco)
            Declaracoes.append(Declaracao)
            Chamadas.extend(Chamada)
            Saidas.extend(Saida)
    Cabecalho = "\n".join(CABECALHO_UNIDADES) + "\n\n"
    Corpo = "\n\n".join(Declaracoes) + "\n\n"
    Main = "proc main() {\n" + "\n".join(Chamadas) + "\n}\n"
    return Cabecalho + Corpo + Main, "\n".join(Saidas) + "\n"


def Sha256Texto(Texto):
    return hashlib.sha256(Texto.encode("utf-8")).hexdigest()


def GravarArquivo(Caminho, Texto):
    with open(Caminho, "w", encoding="utf-8", newline="\n") as Arquivo:
        Arquivo.write(Texto)


def GerarArquivos(Semente, Linhas, Saida):
    """Grava corpus_<linhas>.fj, corpus_<linhas>.esperado.txt e manifesto.json em Saida."""
    Texto, Esperado = GerarCorpus(Semente, Linhas)
    os.makedirs(Saida, exist_ok=True)
    GravarArquivo(os.path.join(Saida, f"corpus_{Linhas}.fj"), Texto)
    GravarArquivo(os.path.join(Saida, f"corpus_{Linhas}.esperado.txt"), Esperado)
    Manifesto = {
        "seed": Semente,
        "linhas_pedidas": Linhas,
        "linhas_reais": Texto.count("\n"),
        "sha256_fj": Sha256Texto(Texto),
        "sha256_esperado": Sha256Texto(Esperado),
    }
    GravarArquivo(os.path.join(Saida, "manifesto.json"), json.dumps(Manifesto, indent=2) + "\n")
    return Manifesto


def ConferirManifestos(ManifestoA, ManifestoB, Linhas):
    if ManifestoA["sha256_fj"] != ManifestoB["sha256_fj"]:
        raise ErroSelftest(f"hash do .fj difere entre as duas geracoes com seed {SEMENTE_SELFTEST}")
    if ManifestoA["sha256_esperado"] != ManifestoB["sha256_esperado"]:
        raise ErroSelftest(f"hash do esperado difere entre as duas geracoes com seed {SEMENTE_SELFTEST}")
    Reais = ManifestoA["linhas_reais"]
    if abs(Reais - Linhas) > 0.05 * Linhas:
        raise ErroSelftest(f"linhas_reais={Reais} fora de +-5% de {Linhas}")


def ConferirArquivos(Pasta, Linhas):
    with open(os.path.join(Pasta, f"corpus_{Linhas}.fj"), encoding="utf-8") as Arquivo:
        Texto = Arquivo.read()
    with open(os.path.join(Pasta, f"corpus_{Linhas}.esperado.txt"), encoding="utf-8") as Arquivo:
        Esperado = Arquivo.read()
    if not Texto.isascii():
        raise ErroSelftest("o .fj gerado contem caractere fora de ASCII")
    Prints = sum(1 for Linha in Texto.split("\n") if "print(" in Linha)
    Saidas = Esperado.split("\n")[:-1]
    if Prints != len(Saidas):
        raise ErroSelftest(f"{Prints} chamadas print no .fj, mas {len(Saidas)} linhas no esperado")
    for Saida in Saidas:
        if not (re.fullmatch(r"-?[0-9]+", Saida) or Saida in ("true", "false")):
            raise ErroSelftest(f"linha do esperado nao e inteiro nem bool: {Saida!r}")


def Selftest():
    try:
        with tempfile.TemporaryDirectory() as Temp:
            PastaA = os.path.join(Temp, "a")
            PastaB = os.path.join(Temp, "b")
            ManifestoA = GerarArquivos(SEMENTE_SELFTEST, LINHAS_SELFTEST, PastaA)
            ManifestoB = GerarArquivos(SEMENTE_SELFTEST, LINHAS_SELFTEST, PastaB)
            ConferirManifestos(ManifestoA, ManifestoB, LINHAS_SELFTEST)
            ConferirArquivos(PastaA, LINHAS_SELFTEST)
    except ErroSelftest as Erro:
        print(f"SELFTEST FALHOU: {Erro}")
        return 1
    print("SELFTEST OK")
    return 0


def Main(Argumentos=None):
    Parser = argparse.ArgumentParser(description="Gera o corpus .fj do benchmark da Forja.")
    Parser.add_argument("--seed", type=int)
    Parser.add_argument("--linhas", type=int)
    Parser.add_argument("--saida")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args(Argumentos)
    if Args.selftest:
        return Selftest()
    if Args.seed is None or Args.linhas is None or Args.saida is None:
        Parser.error("informe --seed, --linhas e --saida (ou --selftest)")
    if Args.linhas < 1:
        Parser.error(f"--linhas precisa ser >= 1; recebido {Args.linhas}")
    Manifesto = GerarArquivos(Args.seed, Args.linhas, Args.saida)
    print(json.dumps(Manifesto, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(Main())
