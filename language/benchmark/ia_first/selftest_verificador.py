# -*- coding: utf-8 -*-
"""selftest_verificador.py - autoteste do verificar_tarefa.py (sem modelo, sem Forja).

Versao 0.1.0 (11/11/2026). Uso: python verificar_tarefa.py --selftest
Cria tarefas e solucoes minimas em Python (e uma DLL em C, se houver gcc) num diretorio temporario e confere
que o verificador aprova o certo e reprova cada erro plantado: stdout, exit, tempo, traceback, saida grande,
arquivo, env removido, fonte inexistente, tarefa so de outra linguagem.
"""
import argparse
import json
import shutil
import sys
import tempfile
from pathlib import Path

SOLUCAO_OK = "import os, sys\nprint('ola', sys.argv[1], os.environ.get('TAREFA_X', 'ausente'))\n"

# (nome, esperado.json, codigo da solucao, veredito esperado: True, False ou None)
CASOS = [
    ("ok", {"casos": [{"args": ["a"], "env": {"TAREFA_X": "1"}, "stdout": "ola a 1\n", "exit": 0},
                      {"args": ["b"], "env": {"TAREFA_X": None}, "stdout": "ola b ausente\n"}]}, SOLUCAO_OK, True),
    ("stdout_errado", {"args": ["a"], "stdout": "outra coisa\n"}, SOLUCAO_OK, False),
    ("exit_errado", {"args": ["a"], "stdout": "ola a ausente\n", "exit": 3}, SOLUCAO_OK, False),
    ("exit_certo", {"args": [], "stdout": "", "exit": 3}, "import sys\nsys.exit(3)\n", True),
    ("tempo", {"args": [], "stdout": ""}, "while True:\n    pass\n", False),
    ("traceback", {"args": [], "stdout": ""}, "raise ValueError('x')\n", False),
    ("saida_grande", {"args": [], "stdout": "x" * 100000}, "print('x' * 100000, end='')\n", False),
    ("arquivo_ok", {"args": [], "stdout": "", "arquivos": {"s.csv": "a,b\n"}},
     "open('s.csv', 'w', newline='').write('a,b\\n')\n", True),
    ("arquivo_faltando", {"args": [], "stdout": "", "arquivos": {"s.csv": "a,b\n"}}, "pass\n", False),
    ("so_outra", {"so_linguagens": ["forja"], "args": [], "stdout": ""}, "pass\n", None),
]


def MontarTarefa(Raiz, Nome, Esperado, Codigo):
    Pasta = Raiz / "tarefas" / Nome
    (Pasta / "entrada").mkdir(parents=True)
    (Pasta / "esperado.json").write_text(json.dumps(Esperado), encoding="utf-8")
    Fonte = Raiz / "solucoes" / Nome / "prog.py"
    Fonte.parent.mkdir(parents=True)
    Fonte.write_text(Codigo, encoding="utf-8")
    return Fonte


def SolucaoDll(Raiz, Nome, Valor):
    """Tarefa de DLL com um proc somar(a, b) em C; Valor e o corpo (certo: a + b)."""
    Pasta = Raiz / "tarefas" / Nome
    Pasta.mkdir(parents=True)
    Esp = {"tipo": "dll", "simbolo": "somar", "args_tipos": ["i32", "i32"], "retorno": "i32",
           "casos": [{"args": [2, 3], "retorno": 5}, {"args": [-4, 1], "retorno": -3}]}
    (Pasta / "esperado.json").write_text(json.dumps(Esp), encoding="utf-8")
    Fonte = Raiz / "solucoes" / Nome / "prog.c"
    Fonte.parent.mkdir(parents=True)
    Fonte.write_text("__declspec(dllexport) int somar(int a, int b) { return %s; }\n" % Valor, encoding="utf-8")
    return Fonte


def Conferir(Verif, Cfg, Raiz, Falhas):
    Args = argparse.Namespace(Tarefas=str(Raiz / "tarefas"), Forja=None)
    for Nome, Esp, Codigo, Esperado in CASOS:
        Fonte = MontarTarefa(Raiz, Nome, Esp, Codigo)
        R = Verif.Verificar(Args, Cfg, (Nome, "python", Fonte))
        Ok = R["verde"] is Esperado and "erro interno" not in R["motivo"]
        print("  %-18s verde=%-5s esperado=%-5s %s" % (Nome, R["verde"], Esperado, "ok" if Ok else "ERRO: " + R["motivo"]))
        if not Ok:
            Falhas.append(Nome)
    return Args


def ConferirDll(Verif, Cfg, Raiz, Falhas):
    if not shutil.which("gcc"):
        print("  dll: gcc ausente, pulado")
        return
    Args = argparse.Namespace(Tarefas=str(Raiz / "tarefas"), Forja=None)
    for Nome, Corpo, Esperado in (("dll_certa", "a + b", True), ("dll_errada", "a - b", False)):
        Fonte = SolucaoDll(Raiz, Nome, Corpo)
        R = Verif.Verificar(Args, Cfg, (Nome, "c", Fonte))
        Ok = R["verde"] is Esperado
        print("  %-18s verde=%-5s esperado=%-5s %s" % (Nome, R["verde"], Esperado, "ok" if Ok else "ERRO: " + R["motivo"]))
        if not Ok:
            Falhas.append(Nome)


def ConferirFonteInexistente(Verif, Cfg, Raiz, Falhas):
    Args = argparse.Namespace(Tarefas=str(Raiz / "tarefas"), Forja=None)
    R = Verif.Verificar(Args, Cfg, ("ok", "python", Raiz / "nao_existe.py"))
    Ok = R["verde"] is False and "Traceback" not in R["motivo"]
    print("  %-18s verde=%-5s %s" % ("fonte_inexistente", R["verde"], "ok" if Ok else "ERRO: " + R["motivo"]))
    if not Ok:
        Falhas.append("fonte_inexistente")


def Executar(Verif):
    Cfg = Verif.LerJson(Verif.RAIZ / "linguagens.json")
    Cfg["limite_segundos"] = 1  # o caso 'tempo' nao pode esperar 10 s
    Falhas = []
    with tempfile.TemporaryDirectory(prefix="ia_first_selftest_") as T:
        Raiz = Path(T)
        Conferir(Verif, Cfg, Raiz, Falhas)
        ConferirDll(Verif, Cfg, Raiz, Falhas)
        ConferirFonteInexistente(Verif, Cfg, Raiz, Falhas)
    print("selftest: %s" % ("VERDE" if not Falhas else "VERMELHO " + ",".join(Falhas)))
    return 0 if not Falhas else 1


if __name__ == "__main__":
    import verificar_tarefa
    sys.exit(Executar(verificar_tarefa))
