#!/usr/bin/env python3
# verificar_json.py 0.1.1 - prova do pacote std:json (cJSON 1.7.19) da Forja. (0.1.1: S2g-json, o teste_json.fj do std:json 0.2.0 tem 14 testes; a prova segue "0 falharam".)
# Uso: python language\vendor\json\verificar_json.py [--forja <forja.exe>]
# Roda: (1) forja test teste_json.fj; (2) json_basico e json_manifesto contra os .esperado;
#       (3) forja build do manifesto e confere que o .exe nao importa nenhuma DLL da lib (so Windows).
# Saida 0 = tudo certo; 1 = alguma prova falhou. Depende de: gcc/objdump no PATH (MinGW).
import argparse
import os
import subprocess
import sys
import tempfile

PastaPacote = os.path.dirname(os.path.abspath(__file__))
PastaExemplos = os.path.normpath(os.path.join(PastaPacote, "..", "..", "exemplos_libs"))
ForjaPadrao = os.path.normpath(os.path.join(PastaPacote, "..", "..", "compiler", "forja.exe"))
DllsPermitidas = ("kernel32.dll", "api-ms-win-crt-")


def Executar(Comando: list, Pasta: str) -> subprocess.CompletedProcess:
    return subprocess.run(Comando, cwd=Pasta, capture_output=True, text=True, encoding="utf-8", errors="replace")


def ProvarTestes(Forja: str) -> bool:
    Saida = Executar([Forja, "test", "teste_json.fj"], PastaExemplos)
    Ok = Saida.returncode == 0 and "0 falharam" in Saida.stdout
    print("[teste_json]", "OK" if Ok else "FALHOU", Saida.stdout.strip().splitlines()[-1:] or Saida.stderr[-200:])
    return Ok


def ProvarExemplo(Forja: str, Nome: str) -> bool:
    Saida = Executar([Forja, "run", Nome + ".fj"], PastaExemplos)
    with open(os.path.join(PastaExemplos, Nome + ".esperado"), encoding="utf-8", errors="replace") as Arquivo:
        Esperado = Arquivo.read()
    Ok = Saida.returncode == 0 and Saida.stdout.replace("\r\n", "\n") == Esperado.replace("\r\n", "\n")
    print(f"[{Nome}]", "OK" if Ok else "FALHOU (saida diferente do .esperado ou exit != 0)")
    return Ok


def DllsImportadas(Exe: str) -> list:
    Saida = subprocess.run(["objdump", "-p", Exe], capture_output=True, text=True, errors="replace")
    return sorted({L.split(":", 1)[1].strip().lower() for L in Saida.stdout.splitlines() if "DLL Name" in L})


def ProvarExeSemDll(Forja: str) -> bool:
    with tempfile.TemporaryDirectory() as Temp:
        Exe = os.path.join(Temp, "json_manifesto.exe")
        Saida = Executar([Forja, "build", "--release", "json_manifesto.fj", "-o", Exe], PastaExemplos)
        if Saida.returncode != 0:
            print("[exe sem dll] FALHOU no build:", Saida.stderr[-300:])
            return False
        Estranhas = [D for D in DllsImportadas(Exe) if not D.startswith(DllsPermitidas)]
        Roda = subprocess.run([Exe], cwd=PastaExemplos, capture_output=True, text=True, errors="replace")
        Ok = not Estranhas and Roda.returncode == 0 and "manifesto ok" in Roda.stdout
        print("[exe sem dll]", "OK" if Ok else f"FALHOU dlls_estranhas={Estranhas} exit={Roda.returncode}")
        return Ok


def Principal() -> int:
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--forja", default=ForjaPadrao)
    Forja = Analise.parse_args().forja
    Provas = [ProvarTestes(Forja), ProvarExemplo(Forja, "json_basico"), ProvarExemplo(Forja, "json_manifesto"), ProvarExeSemDll(Forja)]
    print("RESULTADO:", "tudo certo" if all(Provas) else "FALHOU")
    return 0 if all(Provas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
