# -*- coding: utf-8 -*-
"""Bateria de interop da Forja: casos reais de foreign (DLL, import library, C++ por wrapper, vendor) em dev e release.

Cada caso e uma pasta tests\\interop\\<pasta>\\<caso>\\ com: a lib em C/C++ (fonte), construir_lib.py, o artefato binario versionado,
artefatos.sha256, caso.fj + esperado.txt (stdout; comparado sem CRLF) e, opcionais:
  - caso_<x>.fj + esperado_<x>.txt [+ esperado_<x>.exit (padrao 0)] [+ esperado_<x>.err: um JSON por linha, cada um subconjunto
    de alguma linha JSON do stderr]  - programa extra do mesmo caso (ex.: R0010 de excecao C++, exit 101);
  - externos.txt: um caminho absoluto por linha, copiado para a pasta de trabalho (ausente = caso "pulado", nunca "ok");
  - caminho.txt: um diretorio por linha acrescentado ao PATH do run (relativo = a pasta de trabalho).
Modos: dev = 'forja run'; release = 'forja build --release' + executar o .exe. O PATH do run leva a pasta de trabalho (copia do caso).
O sha256 dos artefatos e conferido antes de usar; a bateria nunca chama gcc. Linha 'vendor:<arq>' do artefatos.sha256 = arquivo de
language\\vendor (conferido e copiado); 'vendor:<pacote>/<arq>' (modulo .fj de import "vendor:...") vai para vendor\\<pacote>\\<arq> na pasta de
trabalho. Compilador: --forja (use uma COPIA, nunca o forja.exe que outro agente reconstroi).

Uso: python -I verificar_interop.py --forja <forja.exe> [--backend x64] [--pasta dinamica|estatica] [--caso nome] [--jobs N]
Saida: tabela "caso | programa | modo | resultado" e "interop: N casos, L linhas, K ok, F falharam, P puladas"; exit 1 se alguma falhou.
--backend x64 roda as pastas dinamica (P23b) e estatica (P23g: .a/.lib de C ligadas pelo ligador nativo), sem pulo por backend.

Versao: 0.4.1
Changelog: 0.4.1 - 0.21.0 F3: padrao x64, backend C aposentado (--backend so aceita x64).
           0.4.0 - M3b P23g: a pasta estatica roda no x64 (PastasSoBackendC e MotivoSoBackendC removidos); o "pulado" fica so para externo ausente.
           0.3.0 - M3 P23b: --backend x64 liga a pasta dinamica; estatica pulada no x64 por motivo explicito (PastasSoBackendC).
           0.2.0 - M3 P19i-b: vendor:<pacote>/<arq> copiado para vendor\\<pacote>\\ (modulos .fj do vendor); pasta estatica.
           0.1.0 - M3 P19i-a: primeira versao (dinamica; backend C; --backend x64 = pulado).
Verificacao: python -I verificar_interop.py --forja %TEMP%\\forja_p19i\\forja.exe --backend x64
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaVendor = PastaAqui.parent.parent / "vendor"
BaseTrabalho = Path(tempfile.gettempdir()) / "forja_interop"
PastasDeCasos = ("dinamica", "estatica")
Modos = ("dev", "release")
TimeoutSegundos = 300


class Pulado(Exception):
    """Caso que nao pode rodar nesta maquina (externo ausente) ou ainda nao suportado pelo backend; nunca conta como ok."""


def LerTexto(Caminho):
    return Path(Caminho).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def Rodar(Comando, Ambiente, Pasta):
    return subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace",
                          timeout=TimeoutSegundos, env=Ambiente, cwd=Pasta)


def LinhasUteis(Arquivo):
    """Linhas nao vazias e nao comentario (#) de um .txt/.sha256 do caso; arquivo ausente = lista vazia."""
    if not Arquivo.exists():
        return []
    return [L.strip() for L in LerTexto(Arquivo).split("\n") if L.strip() and not L.strip().startswith("#")]


def ConferirArtefatos(Caso):
    """Confere artefatos.sha256 (nomes do caso ou vendor:); devolve (origem, destino relativo a pasta de trabalho) dos arquivos de vendor.
    vendor:<arq> vai solto na pasta de trabalho; vendor:<pacote>/<arq> (modulo .fj do import "vendor:...") vai para vendor\\<pacote>\\<arq>,
    onde o compilador procura (ao lado do arquivo raiz). ValueError se algum sha256 divergir."""
    Copias = []
    for Linha in LinhasUteis(Caso / "artefatos.sha256"):
        Hash, Nome = Linha.split(None, 1)
        Origem = PastaVendor / Nome[7:] if Nome.startswith("vendor:") else Caso / Nome
        if not Origem.exists() or hashlib.sha256(Origem.read_bytes()).hexdigest() != Hash:
            raise ValueError(f"sha256 de {Nome} nao confere (regenerar com construir_lib.py ou atualizar o vendor)")
        if Nome.startswith("vendor:"):
            Relativo = Path("vendor") / Nome[7:] if "/" in Nome[7:] else Path(Nome[7:])
            Copias.append((Origem, Relativo))  # so o vendor e copiado; os artefatos do caso ja vem com a copia da pasta
    return Copias


def PrepararTrabalho(Caso, Backend):
    """Copia o caso, os artefatos vendor: e os externos para uma pasta de trabalho nova; devolve (trabalho, pasta de saida do release, PATH extra)."""
    Copias = ConferirArtefatos(Caso)
    Raiz = BaseTrabalho / Backend / Caso.name
    shutil.rmtree(Raiz, ignore_errors=True)
    Trabalho, Saida = Raiz / "caso", Raiz / "saida"
    shutil.copytree(Caso, Trabalho)
    Saida.mkdir(parents=True)
    Externos = [(Path(L), Path(Path(L).name)) for L in LinhasUteis(Caso / "externos.txt")]
    for Origem, Relativo in Copias + Externos:
        if not Origem.exists():
            raise Pulado(f"externo ausente: {Origem}")
        (Trabalho / Relativo).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(Origem, Trabalho / Relativo)
    Extras = [str((Trabalho / L).resolve()) for L in LinhasUteis(Caso / "caminho.txt")]
    return Trabalho, Saida, Extras


def ProgramasDoCaso(Caso):
    """Lista de (nome, fonte.fj, esperado.txt) do caso: caso.fj e cada caso_<x>.fj com o esperado_<x>.txt ao lado."""
    Programas = []
    for Fonte in sorted(Caso.glob("caso*.fj")):
        Sufixo = Fonte.stem[len("caso"):]
        Programas.append((Fonte.stem, Fonte.name, Caso / f"esperado{Sufixo}.txt"))
    return Programas


def AmbienteDoRun(Trabalho, Extras):
    Ambiente = dict(os.environ)
    Ambiente["PATH"] = os.pathsep.join([str(Trabalho)] + Extras + [Ambiente.get("PATH", "")])
    return Ambiente


def ExecutarModo(Forja, Backend, Fonte, Modo, Contexto):
    """Roda um programa em um modo; devolve o CompletedProcess do programa (dev: forja run; release: build + exe)."""
    Trabalho, Saida, Ambiente = Contexto
    Base = [str(Forja)]
    if Modo == "dev":
        return Rodar(Base + ["run", "--backend", Backend, Fonte], Ambiente, Trabalho)
    Exe = Saida / (Path(Fonte).stem + ".exe")
    Build = Rodar(Base + ["build", "--release", "--backend", Backend, Fonte, "-o", str(Exe)], Ambiente, Trabalho)
    if Build.returncode != 0:
        return Build
    return Rodar([str(Exe)], Ambiente, Trabalho)


def JsonsDoStderr(Texto):
    Dados = []
    for Linha in Texto.splitlines():
        if Linha.strip().startswith("{"):
            try:
                Dados.append(json.loads(Linha))
            except json.JSONDecodeError:
                continue  # linha com { que nao e JSON do compilador: ignorada de proposito
    return Dados


def ConferirErr(Esperado, Stderr):
    """Cada JSON de esperado_<x>.err tem de ser subconjunto de alguma linha JSON do stderr."""
    Dados = JsonsDoStderr(Stderr)
    Faltam = []
    for Linha in LinhasUteis(Esperado):
        Quer = json.loads(Linha)
        if not any(all(D.get(K) == V for K, V in Quer.items()) for D in Dados):
            Faltam.append(Linha)
    return Faltam


def Avaliar(Caso, Programa, Resultado):
    """Compara stdout, exit e stderr do programa com os esperado_*; devolve a lista de problemas (vazia = ok)."""
    _, _, Esperado = Programa
    Sufixo = Esperado.stem
    QuerExit = int(LinhasUteis(Esperado.with_suffix(".exit"))[0]) if Esperado.with_suffix(".exit").exists() else 0
    Problemas = []
    if Resultado.returncode != QuerExit:
        Problemas.append(f"exit {Resultado.returncode} (esperado {QuerExit}); stderr {Resultado.stderr.strip()[:300]!r}")
    if Resultado.stdout.replace("\r\n", "\n") != LerTexto(Esperado):
        Problemas.append(f"stdout difere de {Esperado.name}: {Resultado.stdout.replace(chr(13), '')[:300]!r}")
    for Faltou in ConferirErr(Esperado.with_suffix(".err"), Resultado.stderr):
        Problemas.append(f"{Sufixo}: stderr sem {Faltou}")
    return Problemas


def VerificarCaso(Forja, Backend, Caso):
    """Roda todos os programas do caso nos 2 modos; devolve lista de (caso, programa, modo, resultado)."""
    Programas = ProgramasDoCaso(Caso)
    try:
        Trabalho, Saida, Extras = PrepararTrabalho(Caso, Backend)
    except Pulado as Motivo:
        return [(Caso.name, P[0], M, f"pulado ({Motivo})") for P in Programas for M in Modos]
    except ValueError as Erro:
        return [(Caso.name, P[0], M, f"FALHOU {Erro}") for P in Programas for M in Modos]
    Contexto = (Trabalho, Saida, AmbienteDoRun(Trabalho, Extras))
    Linhas = []
    for Programa in Programas:
        for Modo in Modos:
            Problemas = Avaliar(Caso, Programa, ExecutarModo(Forja, Backend, Programa[1], Modo, Contexto))
            Linhas.append((Caso.name, Programa[0], Modo, "ok" if not Problemas else "FALHOU " + " | ".join(Problemas)))
    return Linhas


def ListarCasos(Pastas, Filtro):
    Casos = []
    for Pasta in Pastas:
        Casos += [C for C in sorted((PastaAqui / Pasta).glob("*")) if C.is_dir() and (not Filtro or C.name == Filtro)]
    return Casos


def Resumir(Linhas, Casos):
    Ok = sum(1 for L in Linhas if L[3] == "ok")
    Puladas = sum(1 for L in Linhas if L[3].startswith("pulado"))
    Falhas = len(Linhas) - Ok - Puladas
    print(f"interop: {len(Casos)} casos, {len(Linhas)} linhas, {Ok} ok, {Falhas} falharam, {Puladas} puladas")
    return Falhas


def Principal():
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--forja", required=True)
    Analise.add_argument("--backend", default="x64", choices=("x64",))
    Analise.add_argument("--pasta", choices=PastasDeCasos)
    Analise.add_argument("--caso", default="")
    Analise.add_argument("--jobs", type=int, default=4)
    Opcoes = Analise.parse_args()
    Casos = ListarCasos([Opcoes.pasta] if Opcoes.pasta else PastasDeCasos, Opcoes.caso)
    if not Casos:
        print("interop: nenhum caso encontrado (pasta/caso inexistente)")
        return 2
    with ThreadPoolExecutor(max_workers=max(1, Opcoes.jobs)) as Pool:
        Rodadas = list(Pool.map(lambda C: VerificarCaso(Opcoes.forja, Opcoes.backend, C), Casos))
    Linhas = [L for Rodada in Rodadas for L in Rodada]
    for Caso, Programa, Modo, Resultado in Linhas:
        print(f"{Caso} | {Programa} | {Modo} | {Resultado}")
    return 1 if Resumir(Linhas, Casos) else 0


if __name__ == "__main__":
    sys.exit(Principal())
