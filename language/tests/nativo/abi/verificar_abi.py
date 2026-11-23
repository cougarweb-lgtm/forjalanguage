# -*- coding: utf-8 -*-
"""verificar_abi.py - bateria 10.2 a (Forja -> C) da ABI Win64 na fronteira: x64 contra a saida congelada do C, 200 assinaturas por semente, dev e release (M3 P20).

Para cada semente: confere o sha256 de abi_lib_<s>.dll e do texto do abi_lib_<s>.c que o gerar_abi.py gera hoje (artefatos.sha256), regenera o abi_chamar_<s>.fj,
compila com o x64 (--backend x64), roda o exe na mesma pasta (com a DLL) e compara stdout, stderr e codigo de saida byte a byte com a saida congelada do backend C
(esperados/abi_s<s>_<modo>[_ind].txt, capturada uma vez com --backend c explicito).
Exige codigo 0 e uma linha de saida por 'print(' do .fj (o programa nao pode ter parado antes). Uma execucao = semente x modo; 3 sementes x 2 modos = 6.
Divergencia: semente, modo, indice da assinatura (a funcao abi_fN mais proxima do 1o byte diferente) e a linha do stdout.
Uma execucao = semente x modo x {direta, indireta}; 3 sementes x 2 modos x 2 = 12 (a direta chama por foreign, a indireta por proc obtido de GetProcAddress).

Lado export (M3 P22b; --lado export): a Forja exporta (DLL --modo dll --backend x64) e programas C chamam: b = abi_exp_<s>.exe com as 200 assinaturas por semente x dev/release (6);
e = fx_preserva.exe confere rbx rbp rdi rsi r12-r15 e xmm6-xmm15 depois de um export pesado, x64 contra a saida congelada do C (2); g = fx_realoca.exe carrega a DLL fora da base 0x180000000, com ADDR64 reais
(teste_nativo.exe dll64) e a realoca.fj (2); exe_host = DLL que importa a API do fx_host.exe, x64 contra a saida congelada do C (2). --lado ambos (padrao) roda os dois lados.

Uso: python -I verificar_abi.py --forja <exe> [--sementes 1,2,3] [--quantidade 200] [--jobs N] [--lado import|export|ambos]
Versao: 0.3.1
Changelog: 0.3.1 - 0.21.0 F3: o lado C sai da execucao; x64 contra a saida congelada do C em esperados/ (abi import: 12 arquivos; fx_preserva e exe_host: 4), capturada uma vez com --backend c explicito. Some a comparacao das DLLs que era vazia (CompilarDll sem --backend cai em x64).
           0.3.0 - M3 P22b: --lado export (b, e, g, exe_host: 12 execucoes) e ambos (padrao); o lado import e o mesmo.
           0.2.0 - M3 P21v-c: cada semente x modo roda tambem na variante INDIRETA (gerar_abi.Gerar(..., Indireto=True): as 200 funcoes chegam por GetProcAddress e viram proc por 'as', IR_CHAMADA_IND; DLL propria abi_ind_<s>, structs sem array); 3 x 2 x 2 = 12 execucoes.
           0.1.0 - M3 P20: primeira versao.
Verificacao: python -I verificar_abi.py --forja <copia do forja.exe> --sementes 1,2,3
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui))
sys.path.insert(0, str(PastaAqui.parent))
import gerar_abi  # noqa: E402
import verificar_nativo as Nativo  # noqa: E402  (Compilar, Executar e PrimeiroDiferente: o mesmo diferencial do corpus)

ArquivoHashes = "artefatos.sha256"
RegexChamadaAbi = re.compile(r"abi_f(\d+)\(")
PastaEsperados = PastaAqui / "esperados"


def LerHashes():
    Hashes = {}
    for Linha in (PastaAqui / ArquivoHashes).read_text(encoding="utf-8").splitlines():
        Hash, _, Nome = Linha.partition("  ")
        Hashes[Nome.split("(")[0]] = Hash
    return Hashes


def ConferirArtefatos(Semente, Indireto, TextoC):
    """Texto do problema se a DLL ou o .c gerado nao bate com o artefatos.sha256; None se confere."""
    Hashes = LerHashes()
    Nome = gerar_abi.NomeDaLib(Semente, Indireto)
    Dll = PastaAqui / f"{Nome}.dll"
    if not Dll.is_file() or hashlib.sha256(Dll.read_bytes()).hexdigest() != Hashes.get(Dll.name):
        return f"{Dll.name} nao bate com o sha256 de {ArquivoHashes} (rode construir_abi.py)"
    if hashlib.sha256(TextoC.encode("utf-8")).hexdigest() != Hashes.get(f"{Nome}.c"):
        return f"o {Nome}.c que o gerar_abi.py gera hoje nao bate com o da DLL versionada (rode construir_abi.py)"
    return None


def IndiceDaAssinatura(TextoFj, Saida, Indice):
    """A funcao abi_fN da chamada que gerou a linha 'Indice' do stdout (conta os print( de cada chamada; 0 se nao achar)."""
    Linhas = TextoFj.splitlines()
    Impressos = 0
    Atual = 0
    for Linha in Linhas:
        Achado = RegexChamadaAbi.search(Linha)
        if Achado and "proc " not in Linha:
            Atual = int(Achado.group(1))
        if Linha.strip().startswith("print("):
            if Impressos == Indice:
                return Atual
            Impressos += 1
    return 0


def Comparar(Rotulo, TextoFj, Esperadas, SaidaC, SaidaX):
    """Texto da divergencia ou None. Rotulo = 'semente S modo' (com ' indireta' na variante indireta)."""
    for Campo in ("stdout", "stderr"):
        Indice = Nativo.PrimeiroDiferente(SaidaC[Campo], SaidaX[Campo])
        if Indice >= 0:
            Linha = SaidaC[Campo][:Indice].count(b"\n")
            Funcao = IndiceDaAssinatura(TextoFj, SaidaC[Campo], Linha)
            return f"{Rotulo}: {Campo} difere no byte {Indice} (linha {Linha + 1}, assinatura abi_f{Funcao})"
    if SaidaC["codigo"] != SaidaX["codigo"]:
        return f"{Rotulo}: codigo de saida C={SaidaC['codigo']} x64={SaidaX['codigo']}"
    if SaidaC["codigo"] != 0 or len(SaidaC["stdout"].splitlines()) != Esperadas:
        return f"{Rotulo}: programa parou antes (codigo {SaidaC['codigo']}, {len(SaidaC['stdout'].splitlines())} de {Esperadas} linhas)"
    return None


def LerEsperado(Nome):
    """Bytes da saida congelada do backend C em esperados/<Nome> (CRLF preservado, como o Nativo.Executar e o RodarPrograma devolvem)."""
    return (PastaEsperados / Nome).read_bytes()


def Executar(Forja, Pedido, Quantidade, Trabalho):
    """Uma execucao (semente x modo x direta/indireta) numa pasta propria: o x64 roda e confere com a saida congelada do C; devolve o texto da divergencia ou None."""
    Semente, Modo, Indireto = Pedido
    Sufixo = "_ind" if Indireto else ""
    Rotulo = f"semente {Semente} {Modo}{' indireta' if Indireto else ''}"
    TextoC, TextoFj = gerar_abi.Gerar(Semente, Quantidade, Indireto)
    Pasta = Path(Trabalho) / f"s{Semente}_{Modo}{Sufixo}"
    Pasta.mkdir(parents=True)
    Dll = f"{gerar_abi.NomeDaLib(Semente, Indireto)}.dll"
    shutil.copy2(PastaAqui / Dll, Pasta / Dll)
    Fonte = Pasta / f"{gerar_abi.NomeDoFj(Semente, Indireto)}.fj"
    Fonte.write_text(TextoFj, encoding="utf-8", newline="\n")
    Codigo, Erro = Nativo.Compilar(Forja, str(Fonte), Modo == "release", Pasta / "x.exe", True, str(Pasta))
    if Codigo != 0:
        return f"{Rotulo}: build x64 falhou: {Erro[:300]!r}"
    Esperado = {"stdout": LerEsperado(f"abi_s{Semente}_{Modo}{Sufixo}.txt"), "stderr": b"", "codigo": 0}
    Esperadas = sum(1 for L in TextoFj.splitlines() if L.strip().startswith("print("))
    return Comparar(Rotulo, TextoFj, Esperadas, Esperado, Nativo.Executar(Pasta / "x.exe", str(Pasta)))


# ---- lado export (M3 P22b): a Forja EXPORTA; programas C (gcc) chamam e conferem ----

PastaLibs = Nativo.PastaLibsM3
PastaDll = PastaAqui.parent / "m3" / "dll"
TesteNativo = Nativo.ArquivoForjaOrigem.parent / "teste_nativo.exe"
TimeoutBuild = Nativo.TimeoutBuildSegundos
TimeoutRun = Nativo.TimeoutExecucaoSegundos
LinhasDoRelato = 6  # quantas linhas da saida do programa C entram no texto de uma divergencia
EsperadoExeHost = "exe_host_roda(10) = 104"


def CompilarDll(Forja, Fonte, Release, Saida, Backend64):
    """forja build --modo dll [--release] [--backend x64] na pasta da fonte; (codigo, erro+stdout)."""
    Comando = [Forja, "build", "--modo", "dll"] + (["--release"] if Release else []) + [str(Fonte), "-o", str(Saida)] + (["--backend", "x64"] if Backend64 else [])
    Resultado = subprocess.run(Comando, capture_output=True, timeout=TimeoutBuild, cwd=Path(Fonte).parent)
    return Resultado.returncode, Resultado.stderr + Resultado.stdout


def RodarPrograma(Comando, Pasta):
    """(codigo, stdout em texto) de um programa de teste (fx_*.exe ou abi_exp_*.exe) rodado na Pasta."""
    try:
        Resultado = subprocess.run([str(C) for C in Comando], capture_output=True, stdin=subprocess.DEVNULL, timeout=TimeoutRun, cwd=Pasta)
    except subprocess.TimeoutExpired:
        return "timeout", ""
    return Resultado.returncode, (Resultado.stdout + Resultado.stderr).decode("utf-8", errors="replace")


def Relato(Saida):
    return " | ".join(Saida.strip().splitlines()[:LinhasDoRelato])


def ConferirArtefatosExport(Semente, TextoC):
    """Texto do problema se o abi_exp_<s>.exe ou o .c gerado hoje nao bate com o artefatos.sha256; None se confere."""
    Hashes = LerHashes()
    Nome = gerar_abi.NomeDoExport(Semente)
    Exe = PastaAqui / f"{Nome}.exe"
    if not Exe.is_file() or hashlib.sha256(Exe.read_bytes()).hexdigest() != Hashes.get(Exe.name):
        return f"{Exe.name} nao bate com o sha256 de {ArquivoHashes} (rode construir_abi.py --so-export)"
    if hashlib.sha256(TextoC.encode("utf-8")).hexdigest() != Hashes.get(f"{Nome}.c"):
        return f"o {Nome}.c que o gerar_abi.py gera hoje nao bate com o do programa versionado (rode construir_abi.py --so-export)"
    return None


def ExecutarAbiExport(Forja, Pedido, Quantidade, Trabalho):
    """b: a DLL Forja (x64) com as 200 assinaturas de uma semente num modo; o abi_exp_<s>.exe chama e confere contra a referencia em C. Texto da divergencia ou None."""
    Semente, Modo = Pedido
    Rotulo = f"abi export semente {Semente} {Modo}"
    Nome = gerar_abi.NomeDoExport(Semente)
    _, TextoFj = gerar_abi.GerarExport(Semente, Quantidade)
    Pasta = Path(Trabalho) / f"export_s{Semente}_{Modo}"
    Pasta.mkdir(parents=True)
    Fonte = Pasta / f"{Nome}.fj"
    Fonte.write_text(TextoFj, encoding="utf-8", newline="\n")
    Codigo, Erro = CompilarDll(Forja, Fonte, Modo == "release", Pasta / f"{Nome}.dll", True)
    if Codigo != 0:
        return f"{Rotulo}: build x64 da DLL falhou: {Erro[:300]!r}"
    shutil.copy2(PastaAqui / f"{Nome}.exe", Pasta / f"{Nome}.exe")
    Codigo, Saida = RodarPrograma([Pasta / f"{Nome}.exe", Pasta / f"{Nome}.dll"], Pasta)
    if Codigo != 0 or f"{Nome}: {Quantidade} chamadas, 0 diferencas" not in Saida:
        return f"{Rotulo}: codigo {Codigo}: {Relato(Saida)}"
    return None


def ExecutarPreserva(Forja, Modo, Trabalho):
    """e: preserva.fj (export pesado) pelo x64; o fx_preserva.exe confere os 18 nao-volateis e a saida tem de ser a congelada do C (esperados/). Texto ou None."""
    Rotulo = f"fx_preserva {Modo}"
    Pasta = Path(Trabalho) / f"preserva_{Modo}"
    Pasta.mkdir(parents=True)
    Fonte = shutil.copy2(PastaDll / "preserva.fj", Pasta / "preserva.fj")
    Dll = Pasta / "x.dll"
    Codigo, Erro = CompilarDll(Forja, Fonte, Modo == "release", Dll, True)
    if Codigo != 0:
        return f"{Rotulo}: build x64 falhou: {Erro[:300]!r}"
    Codigo, Saida = RodarPrograma([PastaLibs / "fx_preserva.exe", Dll, "pesado"], Pasta)
    if Codigo != 0 or Saida.count("mascara de registradores alterados = 0x00000") != 3:
        return f"{Rotulo}: x64 codigo {Codigo}: {Relato(Saida)}"
    Esperado = LerEsperado(f"fx_preserva_{Modo}.txt").decode("utf-8", errors="replace")
    return None if Saida == Esperado else f"{Rotulo}: resultado do x64 ({Relato(Saida)}) difere do congelado do C ({Relato(Esperado)})"


def ExecutarRealoca(Forja, Modo, Trabalho):
    """g: a DLL de teste com ADDR64 reais (teste_nativo.exe dll64) e a realoca.fj x64 carregam FORA de 0x180000000 (fx_realoca.exe reserva a base) e passam nas conferencias. Texto ou None."""
    Rotulo = f"fx_realoca {Modo}"
    Pasta = Path(Trabalho) / f"realoca_{Modo}"
    Pasta.mkdir(parents=True)
    Fonte = shutil.copy2(PastaDll / "realoca.fj", Pasta / "realoca.fj")
    Codigo, Erro = CompilarDll(Forja, Fonte, Modo == "release", Pasta / "realoca.dll", True)
    if Codigo != 0:
        return f"{Rotulo}: build x64 da DLL falhou: {Erro[:300]!r}"
    Gerada = subprocess.run([str(TesteNativo), "dll64", str(Pasta / "dll64.dll")], capture_output=True, timeout=TimeoutBuild)
    if Gerada.returncode != 0:
        return f"{Rotulo}: teste_nativo.exe dll64 falhou: {Gerada.stderr[:200]!r}"
    Codigo, Saida = RodarPrograma([PastaLibs / "fx_realoca.exe", Pasta / "dll64.dll", Pasta / "realoca.dll"], Pasta)
    if Codigo != 0 or "dll64.dll fora da base preferida; campos ADDR64=ok" not in Saida or "realoca.dll fora da base preferida; campos ADDR64=-; realoca_confere=ok" not in Saida:
        return f"{Rotulo}: codigo {Codigo}: {Relato(Saida)}"
    return None


def ExecutarExeHost(Forja, Modo, Trabalho):
    """exe_host: exe_host.fj (importa a API que o fx_host.exe exporta) pelo x64; o fx_host.exe carrega a DLL e a saida tem de ser a congelada do C (esperados/). Texto ou None."""
    Rotulo = f"exe_host {Modo}"
    Pasta = Path(Trabalho) / f"exe_host_{Modo}"
    Pasta.mkdir(parents=True)
    Fonte = shutil.copy2(PastaDll / "exe_host.fj", Pasta / "exe_host.fj")
    shutil.copy2(PastaLibs / "libfx_host.dll.a", Pasta / "libfx_host.dll.a")
    Dll = Pasta / "x.dll"
    Codigo, Erro = CompilarDll(Forja, Fonte, Modo == "release", Dll, True)
    if Codigo != 0:
        return f"{Rotulo}: build x64 falhou: {Erro[:300]!r}"
    Codigo, Saida = RodarPrograma([PastaLibs / "fx_host.exe", Dll], Pasta)
    if Codigo != 0 or EsperadoExeHost not in Saida:
        return f"{Rotulo}: x64 codigo {Codigo}: {Relato(Saida)}"
    Esperado = LerEsperado(f"exe_host_{Modo}.txt").decode("utf-8", errors="replace")
    return None if Saida == Esperado else f"{Rotulo}: saida do x64 ({Relato(Saida)}) difere da congelada do C ({Relato(Esperado)})"


def VerificarLadoExport(Forja, Opcoes, Trabalho):
    """Roda os 4 grupos do lado export; devolve o numero de execucoes que divergiram."""
    Modos = ("dev", "release")
    Grupos = [("abi b", lambda P: ExecutarAbiExport(Forja, P, Opcoes.quantidade, Trabalho), [(S, M) for S in Opcoes.Sementes for M in Modos]),
              ("fx_preserva e", lambda M: ExecutarPreserva(Forja, M, Trabalho), list(Modos)),
              ("fx_realoca g", lambda M: ExecutarRealoca(Forja, M, Trabalho), list(Modos)),
              ("exe_host", lambda M: ExecutarExeHost(Forja, M, Trabalho), list(Modos))]
    Total = 0
    with ThreadPoolExecutor(max_workers=max(1, Opcoes.jobs)) as Pool:
        Futuros = [(Nome, Pedidos, [Pool.submit(Funcao, P) for P in Pedidos]) for Nome, Funcao, Pedidos in Grupos]
        for Nome, Pedidos, Fila in Futuros:
            Falhas = [F.result() for F in Fila]
            for Pedido, Falha in zip(Pedidos, Falhas):
                if Falha is not None:
                    print(f"DIFERENTE: {Falha}")
            Diferentes = sum(1 for F in Falhas if F is not None)
            print(f"verificar_abi: {len(Pedidos)} execucoes, {len(Pedidos) - Diferentes} iguais, {Diferentes} diferentes (lado export, {Nome})")
            Total += Diferentes
    return Total


def ConferirArtefatosOuSair(Opcoes):
    """Hashes das DLLs/exes/.c versionados; devolve o texto do problema ou None."""
    if Opcoes.lado in ("import", "ambos"):
        for Semente, Indireto in [(S, I) for S in Opcoes.Sementes for I in (False, True)]:
            Problema = ConferirArtefatos(Semente, Indireto, gerar_abi.Gerar(Semente, Opcoes.quantidade, Indireto)[0])
            if Problema:
                return Problema
    if Opcoes.lado in ("export", "ambos"):
        for Semente in Opcoes.Sementes:
            Problema = ConferirArtefatosExport(Semente, gerar_abi.GerarExport(Semente, Opcoes.quantidade)[0])
            if Problema:
                return Problema
        return Nativo.ConferirHashesDasLibs()
    return None


def VerificarLadoImport(Forja, Opcoes, Trabalho):
    Pedidos = [(S, M, I) for S in Opcoes.Sementes for M in ("dev", "release") for I in (False, True)]
    with ThreadPoolExecutor(max_workers=max(1, Opcoes.jobs)) as Pool:
        Falhas = list(Pool.map(lambda P: Executar(Forja, P, Opcoes.quantidade, Trabalho), Pedidos))
    for (Semente, Modo, Indireto), Falha in zip(Pedidos, Falhas):
        print(f"semente {Semente} {Modo}{' indireta' if Indireto else ''}: {'IGUAL' if Falha is None else 'DIFERENTE: ' + Falha}")
    Diferentes = sum(1 for F in Falhas if F is not None)
    print(f"verificar_abi: {len(Pedidos)} execucoes, {len(Pedidos) - Diferentes} iguais, {Diferentes} diferentes "
          f"({Opcoes.quantidade} assinaturas por semente)")
    return Diferentes


def Principal():
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--forja", required=True)
    Analise.add_argument("--sementes", default="1,2,3")
    Analise.add_argument("--quantidade", type=int, default=200)
    Analise.add_argument("--jobs", type=int, default=min(6, os.cpu_count() or 1))
    Analise.add_argument("--lado", choices=("import", "export", "ambos"), default="ambos")
    Opcoes = Analise.parse_args()
    Opcoes.Sementes = [int(S) for S in Opcoes.sementes.split(",")]
    Problema = ConferirArtefatosOuSair(Opcoes)
    if Problema:
        print(f"X0801 {Problema}")
        return 2
    Forja = str(Path(Opcoes.forja).resolve())
    Trabalho = tempfile.mkdtemp(prefix="verificar_abi_")
    try:
        Diferentes = 0
        if Opcoes.lado in ("import", "ambos"):
            Diferentes += VerificarLadoImport(Forja, Opcoes, Trabalho)
        if Opcoes.lado in ("export", "ambos"):
            Diferentes += VerificarLadoExport(Forja, Opcoes, Trabalho)
        return 0 if Diferentes == 0 else 1
    finally:
        shutil.rmtree(Trabalho, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Principal())
