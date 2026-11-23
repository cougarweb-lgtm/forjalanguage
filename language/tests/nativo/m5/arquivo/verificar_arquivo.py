# -*- coding: utf-8 -*-
"""std:arquivo (M5 P26c): read_all e write_all no backend x64, dev e release, no Windows.

O que prova (cada caso nos 2 modos, dev e release; saida esperada calculada AQUI, o x64 tem de bater com ela):
  1. copiar.fj: le a entrada com read_all, grava com write_all, le a saida de volta e confere byte a byte. Tamanhos 0 B, 1 B, 1 MiB, 64 MiB + 1 B (lido em blocos; o tamanho
     nunca passa por int32), os 256 valores de byte repetidos com 0D 0A, 0A, 0D, 1A e 00 (sem traducao de fim de linha) e caminho UTF-8 com acento e espaco. Alem do que o
     programa imprime ("n=<bytes> iguais=true"), o sha256 do arquivo gravado tem de ser o da entrada.
  2. erros.fj: arquivo inexistente, pasta inexistente, caminho vazio, arquivo usado como pasta, componente longo demais (not_found); nome invalido (other); pasta no lugar
     do arquivo, destino somente leitura (no_permission, e o conteudo do destino fica intacto); gravar 3 bytes em arquivo novo e sobre um maior (trunca para 3).
  3. vazamento.fj: read_all sem delete = R0009 (exit 101) em dev; em release sai 0.

Uso: python -I verificar_arquivo.py [--forja <forja.exe>] [--jobs N] [--sem-grande]
Saida: linhas de falha (se houver) e, no fim, "verificar_arquivo: <ok>/<total> casos de arquivo" (exit 0 so com ok == total).
  4. linhas.fj (IA1-L6): read_lines com LF, CRLF, ultima linha sem fim de linha, linhas vazias, arquivo vazio, UTF-8 e arquivo inexistente.

Versao: 1.1.1
Changelog: 1.1.1 - 0.21.0 F4 residuo: sai o backend C (aposentado, X0003); a referencia e a saida esperada fixa do proprio script.
           1.1.0 - IA1-L6: linhas.fj e ConferirLinhas (read_lines).
           1.0.0 - M5 P26c: primeira versao.
Verificacao: python -I verificar_arquivo.py --forja <abs do forja.exe>
"""

import argparse
import hashlib
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[3]
BACKENDS = ("x64",)
MODOS = ("dev", "release")
TEMPO_LIMITE = 300
MIB = 1024 * 1024
TAMANHO_GRANDE = 64 * MIB + 1
FONTES = ("copiar", "erros", "vazamento", "linhas")


def Padrao(Tamanho):
    """Bytes do tipo 0..255 repetidos, com as sequencias que um modo texto traduziria no meio."""
    Base = bytes(range(256)) + b"\r\n\n\r\x1a\x00\r\r\n"
    Reps = Tamanho // len(Base) + 1
    return (Base * Reps)[:Tamanho]


def Construir(Forja, Fonte, Backend, Modo, Saida):
    Comando = [str(Forja), "build", "--backend", Backend]
    if Modo == "release":
        Comando.append("--release")
    Comando += [str(Fonte), "-o", str(Saida)]
    Proc = subprocess.run(Comando, capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE)
    if Proc.returncode != 0 or not Saida.exists():
        return Proc.stdout.decode("utf-8", "replace")[:600] + Proc.stderr.decode("utf-8", "replace")[:600]
    return None


def Rodar(Exe, Argumentos):
    """Linha de comando CRUA (str inteira ao CreateProcess); cada argumento entre aspas, vazio vira "". """
    Linha = f'"{Exe}"' + "".join(f' "{A}"' for A in Argumentos)
    Proc = subprocess.run(Linha, capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE)
    return Proc.returncode, Proc.stdout, Proc.stderr


def Sha(Caminho):
    Resumo = hashlib.sha256()
    with open(Caminho, "rb") as F:
        for Bloco in iter(lambda: F.read(8 * MIB), b""):
            Resumo.update(Bloco)
    return Resumo.hexdigest()


def Falhar(Falhas, Rotulo, Detalhe):
    Falhas.append(f"FALHA {Rotulo}: {Detalhe}")


def LimparPasta(Pasta):
    def Forcar(Funcao, Caminho, _Info):
        os.chmod(Caminho, stat.S_IWRITE)
        Funcao(Caminho)

    shutil.rmtree(Pasta, onerror=Forcar)


def CasoCopiar(Exe, Rotulo, Conteudo, Pasta, Nome="entrada.bin", Subpasta=None):
    """Roda copiar.fj sobre Conteudo; devolve a mensagem de falha ou None."""
    Dir = Pasta if Subpasta is None else Pasta / Subpasta
    Dir.mkdir(parents=True, exist_ok=True)
    Entrada = Dir / Nome
    Saida = Dir / ("saida_" + Nome)
    Entrada.write_bytes(Conteudo)
    try:
        Rc, Out, Err = Rodar(Exe, [Entrada, Saida])
        Esperado = f"n={len(Conteudo)} iguais=true\r\n".encode("utf-8")
        if Rc != 0 or Out != Esperado or Err != b"":
            return f"rc={Rc} saida={Out[:100]!r} esperado={Esperado!r} stderr={Err[:200]!r}"
        if not Saida.exists() or Saida.stat().st_size != len(Conteudo) or Sha(Saida) != Sha(Entrada):
            return "o arquivo gravado difere da entrada (tamanho ou sha256)"
        return None
    finally:
        for Caminho in (Entrada, Saida):
            Caminho.unlink(missing_ok=True)


def CasosCopiarPequenos():
    return [
        ("0 B", b"", "c0", None),
        ("1 B", b"\x00", "c1", None),
        ("1 MiB", Padrao(MIB), "c2", None),
        ("256 valores + CRLF", Padrao(4096), "c3", None),
        ("so CRLF e 1A", b"\r\n" * 5000 + b"\x1a" + b"\n\r" * 3000, "c4", None),
        ("caminho com acento e espaco", Padrao(1000), "c5 ação ç/sub pasta", "entrada ç ã.bin"),
    ]


def ConferirCopiarPequenos(Exes, Pasta, Jobs, Falhas):
    Pedidos = []
    for Rotulo, Conteudo, Sub, NomeEspecial in CasosCopiarPequenos():
        for Backend in BACKENDS:
            for Modo in MODOS:
                Pedidos.append((Rotulo, Conteudo, Sub, NomeEspecial, Backend, Modo))

    def Um(Pedido):
        Rotulo, Conteudo, Sub, NomeEspecial, Backend, Modo = Pedido
        Exe = Exes[("copiar", Backend, Modo)]
        Falha = CasoCopiar(Exe, Rotulo, Conteudo, Pasta, NomeEspecial or "entrada.bin", f"{Sub}_{Backend}_{Modo}")
        return f"copiar {Rotulo} {Backend}/{Modo}", Falha

    Ok = 0
    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        for Rotulo, Falha in Piscina.map(Um, Pedidos):
            if Falha is None:
                Ok += 1
            else:
                Falhar(Falhas, Rotulo, Falha)
    return Ok, len(Pedidos)


def ConferirCopiarGrande(Exes, Pasta, Falhas):
    """64 MiB + 1 B: um de cada vez (disco e memoria)."""
    Conteudo = Padrao(TAMANHO_GRANDE)
    Ok = 0
    Total = 0
    for Backend in BACKENDS:
        for Modo in MODOS:
            Total += 1
            Falha = CasoCopiar(Exes[("copiar", Backend, Modo)], "64 MiB + 1", Conteudo, Pasta, "grande.bin", f"grande_{Backend}_{Modo}")
            if Falha is None:
                Ok += 1
            else:
                Falhar(Falhas, f"copiar 64 MiB + 1 {Backend}/{Modo}", Falha)
    return Ok, Total


def CasosErros(Pasta):
    """(rotulo, modo, caminho como o programa recebe, texto esperado, verificacao pos-execucao ou None)."""
    Base = Pasta / "erros ação"
    Base.mkdir(parents=True, exist_ok=True)
    (Base / "existe.bin").write_bytes(b"12345")
    (Base / "maior.bin").write_bytes(b"x" * 100)
    (Base / "so leitura.bin").write_bytes(b"intacto")
    os.chmod(Base / "so leitura.bin", stat.S_IREAD)
    (Base / "pasta").mkdir(exist_ok=True)

    def ConteudoDe(Nome, Esperado):
        return lambda: (Base / Nome).read_bytes() == Esperado

    return [
        ("r inexistente", "r", Base / "nao existe.bin", "not_found 0", None),
        ("r pasta inexistente", "r", Base / "nao_existe_pasta" / "x.bin", "not_found 0", None),
        ("r caminho vazio", "r", "", "not_found 0", None),
        ("r nome invalido", "r", Base / "a<b.bin", "other 0", None),
        ("r arquivo usado como pasta", "r", Base / "existe.bin" / "x.bin", "not_found 0", None),
        ("r componente longo demais", "r", Base / ("a" * 300), "not_found 0", None),
        ("r pasta no lugar do arquivo", "r", Base / "pasta", "no_permission 0", None),
        ("r arquivo existente", "r", Base / "existe.bin", "ok 5", None),
        ("w pasta no lugar do arquivo", "w", Base / "pasta", "no_permission 0", None),
        ("w destino somente leitura", "w", Base / "so leitura.bin", "no_permission 0", ConteudoDe("so leitura.bin", b"intacto")),
        ("w pasta inexistente", "w", Base / "nao_existe_pasta" / "x.bin", "not_found 0", None),
        ("w arquivo novo", "w", Base / "novo.bin", "ok 0", ConteudoDe("novo.bin", b"\x01\x02\x03")),
        ("w sobre arquivo maior (trunca)", "w", Base / "maior.bin", "ok 0", ConteudoDe("maior.bin", b"\x01\x02\x03")),
    ]


def ConferirErros(Exes, Pasta, Falhas):
    Ok = 0
    Total = 0
    for Rotulo, Modo, Caminho, Esperado, Verifica in CasosErros(Pasta):
        for Backend in BACKENDS:
            for Mod in MODOS:
                Total += 1
                Rc, Out, Err = Rodar(Exes[("erros", Backend, Mod)], [Modo, Caminho])
                Texto = (Esperado + "\r\n").encode("utf-8")
                if Rc == 0 and Out == Texto and Err == b"" and (Verifica is None or Verifica()):
                    Ok += 1
                else:
                    Falhar(Falhas, f"erros {Rotulo} {Backend}/{Mod}", f"rc={Rc} saida={Out[:80]!r} esperado={Texto!r} stderr={Err[:200]!r}")
                if Rotulo == "w arquivo novo" or Rotulo.startswith("w sobre"):
                    (Pasta / "erros ação" / Path(str(Caminho)).name).unlink(missing_ok=True)
                    if Rotulo.startswith("w sobre"):
                        (Pasta / "erros ação" / "maior.bin").write_bytes(b"x" * 100)
    return Ok, Total


def ConferirVazamento(Exes, Pasta, Falhas):
    Entrada = Pasta / "vazamento ação.bin"
    Entrada.write_bytes(Padrao(5000))
    Ok = 0
    Total = 0
    for Backend in BACKENDS:
        for Modo in MODOS:
            Total += 1
            Rc, Out, Err = Rodar(Exes[("vazamento", Backend, Modo)], [Entrada])
            if Modo == "dev":
                Bom = Rc == 101 and b"R0009" in Err and Out == b"n=5000\r\n"
            else:
                Bom = Rc == 0 and Out == b"n=5000\r\n" and Err == b""
            if Bom:
                Ok += 1
            else:
                Falhar(Falhas, f"vazamento {Backend}/{Modo}", f"rc={Rc} saida={Out[:80]!r} stderr={Err[:300]!r}")
    return Ok, Total


def CasosLinhas():
    """(rotulo, conteudo do arquivo, saida esperada) de read_lines: LF, CRLF, ultima linha sem fim, vazias, arquivo vazio, so fim de linha."""
    return [
        ("LF", b"a\nb\nc\n", "3\n[a]\n[b]\n[c]\n"),
        ("CRLF", b"a\r\nbb\r\n", "2\n[a]\n[bb]\n"),
        ("ultima sem fim de linha", b"um\ndois", "2\n[um]\n[dois]\n"),
        ("ultima CRLF solta com CR", b"x\r\ny\r", "2\n[x]\n[y]\n"),
        ("linhas vazias", b"\n\na\n\n", "4\n[]\n[]\n[a]\n[]\n"),
        ("arquivo vazio", b"", "0\n"),
        ("so um fim de linha", b"\n", "1\n[]\n"),
        ("UTF-8", "ação\nç\n".encode("utf-8"), "2\n[ação]\n[ç]\n"),
    ]


def ConferirLinhas(Exes, Pasta, Falhas):
    Ok = 0
    Total = 0
    Entrada = Pasta / "linhas ação.txt"
    for Rotulo, Conteudo, Esperado in CasosLinhas():
        Entrada.write_bytes(Conteudo)
        for Backend in BACKENDS:
            for Modo in MODOS:
                Total += 1
                Rc, Out, Err = Rodar(Exes[("linhas", Backend, Modo)], [Entrada])
                Texto = Esperado.replace("\n", "\r\n").encode("utf-8")
                if Rc == 0 and Out == Texto and Err == b"":
                    Ok += 1
                else:
                    Falhar(Falhas, f"linhas {Rotulo} {Backend}/{Modo}", f"rc={Rc} saida={Out[:80]!r} esperado={Texto!r} stderr={Err[:200]!r}")
    for Backend in BACKENDS:
        for Modo in MODOS:
            Total += 1
            Rc, Out, Err = Rodar(Exes[("linhas", Backend, Modo)], [Pasta / "nao existe.txt"])
            if Rc == 0 and Out == b"erro\r\n":
                Ok += 1
            else:
                Falhar(Falhas, f"linhas inexistente {Backend}/{Modo}", f"rc={Rc} saida={Out[:80]!r}")
    return Ok, Total


def ConstruirTudo(Forja, Pasta, Jobs, Falhas):
    Pedidos = [(F, B, M) for F in FONTES for B in BACKENDS for M in MODOS]
    Exes = {}

    def Um(Pedido):
        Fonte, Backend, Modo = Pedido
        Saida = Pasta / f"{Fonte}_{Backend}_{Modo}.exe"
        return Pedido, Saida, Construir(Forja, PastaAqui / f"{Fonte}.fj", Backend, Modo, Saida)

    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        for Pedido, Saida, Erro in Piscina.map(Um, Pedidos):
            if Erro is not None:
                Falhar(Falhas, f"build {Pedido}", Erro)
            else:
                Exes[Pedido] = Saida
    return Exes


def Principal():
    Analisador = argparse.ArgumentParser()
    Analisador.add_argument("--forja", default=str(PastaLanguage / "compiler" / "forja.exe"))
    Analisador.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 4))
    Analisador.add_argument("--sem-grande", action="store_true", help="pula o caso de 64 MiB + 1 B")
    Pedido = Analisador.parse_args()
    Pasta = Path(tempfile.mkdtemp(prefix="forja_m5_arquivo_"))
    Falhas = []
    Ok = Total = 0
    try:
        Exes = ConstruirTudo(Path(Pedido.forja), Pasta, Pedido.jobs, Falhas)
        if not Falhas:
            Etapas = [
                lambda: ConferirCopiarPequenos(Exes, Pasta, Pedido.jobs, Falhas),
                lambda: ConferirErros(Exes, Pasta, Falhas),
                lambda: ConferirVazamento(Exes, Pasta, Falhas),
                lambda: ConferirLinhas(Exes, Pasta, Falhas),
            ]
            if not Pedido.sem_grande:
                Etapas.append(lambda: ConferirCopiarGrande(Exes, Pasta, Falhas))
            for Etapa in Etapas:
                O, T = Etapa()
                Ok += O
                Total += T
    finally:
        LimparPasta(Pasta)
    for Linha in Falhas[:40]:
        print(Linha)
    print(f"verificar_arquivo: {Ok}/{Total} casos de arquivo")
    return 0 if not Falhas and Ok == Total else 1


if __name__ == "__main__":
    sys.exit(Principal())
