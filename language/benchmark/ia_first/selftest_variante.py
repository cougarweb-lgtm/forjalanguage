# -*- coding: utf-8 -*-
"""selftest_variante.py - verificacao do preprocessador variante.py (P-IA1-a08). Chamado por: python variante.py --selftest

Versao 0.1.1
Changelog:
  0.1.1 (13/11/2026) E4: FixturesOk le as fixtures de tests\\ (a pasta ia_first foi movida para benchmark).
  0.1.0 (11/11/2026) primeira versao. Quatro familias: (u) tabela de traducoes por regra; (a) as referencias escritas a mao em P1 e P2
        (referencias\\forja_p1 e forja_p2) traduzidas e rodadas pelo verificar_tarefa.py -> todas verdes; (b) erros plantados no fonte P1
        voltam na linha/coluna do fonte do agente; (c) idempotencia (fonte v1.11 passa sem mudar: referencias\\forja e fixtures ok_*) e
        linhas preservadas (mesmo numero de linhas nessas entradas). Compilador: IA_FIRST_FORJA ou %TEMP%\\forja_ia1_base\\forja.exe.

Saida: uma linha por verificacao ("OK ..." / "FALHA ..."); exit 0 so se tudo OK. Reversao: apagar este arquivo.
Depende de: Python 3.9+ e dos modulos irmaos (variante, verificar_tarefa).
"""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
FORJA_PADRAO = Path(os.environ.get("TEMP", ".")) / "forja_ia1_base" / "forja.exe"

# (nome, pacote, entrada, saida esperada) - so a parte que a regra mexe.
TRADUCOES = [
    ("V-02 ponto e virgula", "P1", 'fn f() {\n    let a = 1;\n}\n', 'proc f() {\n    let a = 1\n}\n'),
    ("V-03 && || !", "P1", 'proc f() {\n    if a && !b || c {\n    }\n}\n', 'proc f() {\n    if a and not b or c {\n    }\n}\n'),
    ("V-03 ! com comparacao ganha parenteses", "P1", 'proc f() {\n    if !a == b {\n    }\n}\n', 'proc f() {\n    if (not a) == b {\n    }\n}\n'),
    ("V-04 i++ i--", "P1", 'proc f() {\n    i++\n    j--;\n}\n', 'proc f() {\n    i += 1\n    j -= 1\n}\n'),
    ("V-09 fn func def function", "P1", 'fn a() {\n}\npub func b() {\n}\ndef c() {\n}\nfunction d() {\n}\n',
     'proc a() {\n}\npub proc b() {\n}\nproc c() {\n}\nproc d() {\n}\n'),
    ("V-01 corpo de 1 comando", "P1", 'proc f() {\n    if c { return 0 }\n}\n', 'proc f() {\n    if c {\n        return 0\n    }\n}\n'),
    ("V-01 cadeia if/else if/else", "P1", 'proc f() {\n    if a { x() } else if b { y() } else { z() }\n}\n',
     'proc f() {\n    if a {\n        x()\n    } else if b {\n        y()\n    } else {\n        z()\n    }\n}\n'),
    ("V-01 literal de struct nao e corpo", "P1", 'proc f() {\n    let p = Item{a = 1}\n}\n', 'proc f() {\n    let p = Item{a = 1}\n}\n'),
    ("V-05 atribuicao", "P1", 'proc f() {\n    x = c ? 1 : 2\n}\n', 'proc f() {\n    if c {\n        x = 1\n    } else {\n        x = 2\n    }\n}\n'),
    ("V-05 return", "P1", 'proc f() -> int {\n    return c ? 1 : 2\n}\n', 'proc f() -> int {\n    if c {\n        return 1\n    } else {\n        return 2\n    }\n}\n'),
    ("V-05 let anotado", "P1", 'proc f() {\n    let x: int = c ? 1 : 2\n}\n',
     'proc f() {\n    var x: int = 0\n    if c {\n        x =  1\n    } else {\n        x =  2\n    }\n}\n'),
    ("V-06 chamada posicional", "P1", 'proc g(a: int, b: int, c: int = 0) -> int {\n    return a\n}\nproc f() {\n    g(1, 2, 3)\n}\n',
     'proc g(a: int, b: int, c: int = 0) -> int {\n    return a\n}\nproc f() {\n    g(1, b = 2, c = 3)\n}\n'),
    ("V-06 1o argumento nomeado vai para a frente", "P1", 'proc g(a: int, b: int) -> int {\n    return a\n}\nproc f() {\n    g(b = 2, a = 1)\n}\n',
     'proc g(a: int, b: int) -> int {\n    return a\n}\nproc f() {\n    g(1, b = 2)\n}\n'),
    ("V-06 embutidos", "P1", 'proc f() {\n    append(&l, 5)\n    let m = min(a, b)\n}\n', 'proc f() {\n    append(&l, item = 5)\n    let m = min(a, b = b)\n}\n'),
    ("V-08 main -> int", "P1", 'import "std:os"\nproc main() -> int {\n    return 3\n}\n', 'import "std:os"\nproc main()  {\n    os.exit(3)\n}\n'),
    ("V-07 P2 alloc/scope", "P2", 'proc f() {\n    let a = new(T, alloc = heap)\n    let b = make([dynamic]int, scope)\n    let c = new(T, alloc = scope)\n}\n',
     'proc f() {\n    let a = new(T, em = heap)\n    let b = make([dynamic]int, em = scope)\n    let c = new(T, em = escopo)\n}\n'),
    ("import std:os para o binario base", "P1", 'import "std:os"\n', 'import "std:proc" as os\n'),
]


def Forja():
    return Path(os.environ.get("IA_FIRST_FORJA") or FORJA_PADRAO)


def Registrar(Linhas, Nome, Ok, Detalhe=""):
    Linhas.append(("OK    " if Ok else "FALHA ") + Nome + (" - " + Detalhe if Detalhe and not Ok else ""))
    return Ok


def VerificarTraducoes(V, Saida):
    Tudo = True
    for Nome, Pacote, Entrada, Esperado in TRADUCOES:
        Alvo = "base" if "binario base" in Nome else "main"
        _, Bytes = V.TraduzirBytes(Entrada.encode("latin-1"), Pacote, Alvo)
        Tudo &= Registrar(Saida, "traducao " + Nome, Bytes.decode("latin-1") == Esperado, repr(Bytes.decode("latin-1")))
    _, Bytes = V.TraduzirBytes(b'proc f() {\n    let a = new(T, em = heap)\n}\n', "P2", "main")
    Res, _ = V.TraduzirBytes(b'proc f() {\n    let a = new(T, em = heap)\n}\n', "P2", "main")
    return Tudo & Registrar(Saida, "traducao P2 rotulo antigo vira E0110", len(Res.Erros) == 1 and Res.Erros[0]["codigo"] == "E0110")


def VerificarReferencias(V, Saida):
    """(a) referencias a mao em P1 e P2 -> traduzidas -> verificar_tarefa -> todas verdes."""
    import verificar_tarefa as VT
    Tudo = True
    for Pacote, Pasta in (("P1", "forja_p1"), ("P2", "forja_p2")):
        with tempfile.TemporaryDirectory(prefix="var_ref_") as Tmp:
            Tarefas = sorted(P.name for P in (RAIZ / "referencias" / Pasta).iterdir() if P.is_dir())
            for Tarefa in Tarefas:
                Trad = V.TraduzirPasta(RAIZ / "referencias" / Pasta / Tarefa, Path(Tmp) / "forja" / Tarefa, Pacote, V.AlvoDoForja(Forja()))
                Tudo &= Registrar(Saida, "%s %s traducao sem erro nem pendencia" % (Pacote, Tarefa), not Trad.Erros() and not Trad.Pendencias(), str(Trad.Erros()[:1] + Trad.Pendencias()[:1]))
            Args = ["--referencias", "--referencias-dir", Tmp, "--lingua", "forja", "--tarefa", ",".join(Tarefas), "--forja", str(Forja()), "--json"]
            Proc = subprocess.run([sys.executable, str(RAIZ / "verificar_tarefa.py")] + Args, capture_output=True, text=True, timeout=600)
            Res = json.loads(Proc.stdout.strip().splitlines()[-1])
            Falhas = [R for R in Res if R["verde"] is not True]
            Tudo &= Registrar(Saida, "%s referencias verdes: %d/%d" % (Pacote, len(Res) - len(Falhas), len(Res)), not Falhas, str([(R["tarefa"], R["motivo"][:200]) for R in Falhas]))
    return Tudo


def PosicaoNoTexto(Texto, Trecho, Ocorrencia=0):
    """(linha, coluna) 1-based do trecho no texto."""
    Pos = -1
    for _ in range(Ocorrencia + 1):
        Pos = Texto.index(Trecho, Pos + 1)
    Linha = Texto.count("\n", 0, Pos) + 1
    return Linha, Pos - (Texto.rfind("\n", 0, Pos) + 1) + 1


PLANTADOS = [
    ("erro depois de ';' e V-01", 'import "std:os"\nfn main() {\n    let a = os.args();\n    if len(a) > 1 && a[1] == "x" { let n: i32 = "txt"; }\n}\n', '"txt"'),
    ("erro no ramo do ternario", 'fn main() {\n    var x: i32 = 0;\n    x = 1 > 0 ? 5 : "z";\n}\n', '"z"'),
    ("erro em argumento posicional", 'fn f(a: int, b: int) -> int { return a + b; }\nfn main() {\n    print(f(1, "s"));\n}\n', '"s"'),
    ("erro depois de varias linhas V-01", 'fn main() {\n    if true { print(1); }\n    if true { print(2); }\n    let q: i32 = "bad";\n}\n', '"bad"'),
    ("erro com acento antes na linha (colunas em bytes)", 'fn main() {\n    if true { let s = "çã" + "a" }\n}\n', '+'),
]


def Compilar(Forja_, Arq):
    Proc = subprocess.run([str(Forja_), "check", str(Arq)], capture_output=True, timeout=60)
    return Proc.stdout.decode("utf-8", errors="replace")


def VerificarPlantados(V, Saida):
    """(b) erro plantado na linha N coluna C do fonte P1 volta na linha/coluna certas."""
    Tudo = True
    for Nome, Fonte, Trecho in PLANTADOS:
        with tempfile.TemporaryDirectory(prefix="var_plan_") as Tmp:
            Origem, Destino = Path(Tmp) / "ag", Path(Tmp) / "tr"
            Origem.mkdir()
            (Origem / "prog.fj").write_bytes(Fonte.encode("utf-8"))
            Trad = V.TraduzirPasta(Origem, Destino, "P1", V.AlvoDoForja(Forja()))
            Texto = V.RemapearTexto(Compilar(Forja(), Destino / "prog.fj"), Trad)
            Esperado = PosicaoNoTexto(Fonte.encode("utf-8").decode("latin-1"), Trecho.encode("utf-8").decode("latin-1"))
            Linhas = [json.loads(L) for L in Texto.splitlines() if L.startswith("{")]
            Obtido = (Linhas[0]["linha"], Linhas[0]["coluna"]) if Linhas else None
            Tudo &= Registrar(Saida, "plantado %s: esperado %s obtido %s" % (Nome, Esperado, Obtido), Obtido == Esperado, Texto[:200])
    return Tudo


def FixturesOk():
    Raiz = RAIZ.parent.parent / "tests"
    return sorted(P for P in Raiz.rglob("ok_*.fj") if "dourado" not in P.parts and "ia_first" not in P.parts)[:400]


def ProgramaValido(Arq):
    """True se o fonte passa no check do compilador (so esses provam idempotencia: fixture 'ok_' de sintaxe pode nao ser programa valido)."""
    if not Forja().is_file():
        return True
    return subprocess.run([str(Forja()), "check", str(Arq)], capture_output=True, timeout=60).returncode == 0


def VerificarIdempotencia(V, Saida):
    """(c) fonte v1.11 passa sem mudar e com o mesmo numero de linhas (P1, alvo main); a saida traduzida e ponto fixo (alvo base)."""
    Todos = sorted((RAIZ / "referencias" / "forja").glob("E*/*.fj")) + FixturesOk()
    Arquivos = [A for A in Todos if ProgramaValido(A)]
    Saida.append("      (%d fontes v1.11 validas de %d; o resto nao passa no proprio check e nao serve de prova)" % (len(Arquivos), len(Todos)))
    Mudou, Linhas, PontoFixo = [], [], []
    for Arq in Arquivos:
        Bruto = Arq.read_bytes()
        _, Saida1 = V.TraduzirBytes(Bruto, "P1", "main", Arq.parent)
        Mudou += [] if Saida1 == Bruto else [Arq.name]
        Linhas += [] if Saida1.count(b"\n") == Bruto.count(b"\n") else [Arq.name]
        _, Base1 = V.TraduzirBytes(Bruto, "P1", "base", Arq.parent)
        _, Base2 = V.TraduzirBytes(Base1, "P1", "base", Arq.parent)
        PontoFixo += [] if Base1 == Base2 else [Arq.name]
    Ok = Registrar(Saida, "idempotencia v1.11 em %d fontes (mudaram: %d)" % (len(Arquivos), len(Mudou)), not Mudou, str(Mudou[:5]))
    Ok &= Registrar(Saida, "linhas preservadas em %d fontes (diferentes: %d)" % (len(Arquivos), len(Linhas)), not Linhas, str(Linhas[:5]))
    return Ok & Registrar(Saida, "traducao e ponto fixo no alvo base (nao: %d)" % len(PontoFixo), not PontoFixo, str(PontoFixo[:5]))


def Executar():
    import variante as V
    Saida = []
    Passos = [VerificarTraducoes, VerificarIdempotencia]
    if Forja().is_file():
        Passos += [VerificarPlantados, VerificarReferencias]
    else:
        Registrar(Saida, "compilador", False, "nao existe: %s (defina IA_FIRST_FORJA)" % Forja())
    Tudo = all([Passo(V, Saida) for Passo in Passos]) and Forja().is_file()
    print("\n".join(Saida))
    print("selftest_variante: %s (%d verificacoes)" % ("VERDE" if Tudo else "VERMELHO", len(Saida)))
    return 0 if Tudo else 1
