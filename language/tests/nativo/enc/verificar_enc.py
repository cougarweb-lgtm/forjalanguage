"""Oraculo NASM do codificador x64 (M1 P4b): compara cada caso de "teste_nativo.exe enc" com "nasm -f bin".

Versao: 0.2.0
Changelog: 0.2.0 - portao M1: confere o sha256 e o tamanho de encode.c, fadec-enc.h, os dois .inc e LICENSE de third\\fadec contra o LEIAME.txt (item 4); diverge = falha.
           0.1.0 - M1 P4b: IGUAL (bytes iguais), EQUIVALENTE (so nas classes da TABELA_CLASSES, com motivo),
                   FALHA (qualquer outra diferenca) e RECUSADO (o codificador recusou um caso valido).
Verificacao: python tests/nativo/enc/verificar_enc.py   (sai 0 so com falhas=0 e recusados=0)
"""

import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[3]
TESTE_NATIVO = RAIZ / "compiler" / "teste_nativo.exe"
PASTA_FADEC = RAIZ / "third" / "fadec"
ARQUIVOS_FADEC = ("encode.c", "fadec-enc.h", "fadec-encode-public.inc", "fadec-encode-private.inc", "LICENSE")
NASM = "nasm"
OBJDUMP = "objdump"
DISP_RIP_BYTES = 4

# Unica porta de entrada para diferenca aceita. Fora destas classes, diferenca de bytes e FALHA.
TABELA_CLASSES = {
    "DIRECAO": {
        "grupos": {"E01"},
        "criterio": "objdump",
        "motivo": "mov r,r: o NASM emite 8B /r e o codificador 89 /r (mesmo efeito, mesmo tamanho); o texto do objdump -M intel tem de ser igual",
    },
    "RIP32": {
        "grupos": {"E08"},
        "criterio": "rip",
        "motivo": "[rip+d32]: o codificador zera os 4 bytes finais (a relocacao e do P7); o NASM tem esses 4 bytes mascarados",
    },
}

RE_LINHA_LST = re.compile(r"^\s*(\d+)\s+([0-9A-F]{8})\s")
RE_OBJDUMP = re.compile(r"^\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )*[0-9a-f]{2}\s*\t(.+)$")


def LerIdx(Caminho):
    Casos = []
    for Linha in Path(Caminho).read_text(encoding="utf-8").splitlines():
        Campos = Linha.split()
        if len(Campos) != 6:
            raise ValueError(f"casos.idx com linha fora do formato: {Linha!r}")
        Grupo, Forma, Off, Tam, Num, Estado = Campos
        Casos.append({"grupo": Grupo, "forma": Forma, "off": int(Off), "tam": int(Tam), "linha": int(Num), "estado": Estado})
    return Casos


def LerListagem(Caminho):
    Posicoes = {}
    for Texto in Path(Caminho).read_text(encoding="utf-8", errors="replace").splitlines():
        Achado = RE_LINHA_LST.match(Texto)
        if Achado:
            # Instrucao com mais de 8 bytes repete o numero da linha nas linhas de continuacao: vale a primeira.
            Posicoes.setdefault(int(Achado.group(1)), int(Achado.group(2), 16))
    return Posicoes


def Desmontar(Caminho):
    Saida = subprocess.run(
        [OBJDUMP, "-D", "-b", "binary", "-m", "i386:x86-64", "-M", "intel", str(Caminho)],
        capture_output=True, text=True, encoding="utf-8", errors="replace", check=True,
    ).stdout
    Textos = {}
    for Linha in Saida.splitlines():
        Achado = RE_OBJDUMP.match(Linha)
        if Achado:
            Textos[int(Achado.group(1), 16)] = Achado.group(2).strip()
    return Textos


def ClassificarRip(BytesNasm, BytesNossos):
    Mascara = BytesNasm[:-DISP_RIP_BYTES] + bytes(DISP_RIP_BYTES)
    return Mascara == BytesNossos


def ClassificarObjdump(BytesNasm, BytesNossos, TextoNasm, TextoNossos):
    return TextoNasm is not None and TextoNasm == TextoNossos and len(BytesNasm) == len(BytesNossos)


def ClassificarCaso(Caso, LinhaAsm, BytesNasm, BytesNossos, TextoNasm, TextoNossos):
    if Caso["estado"] != "ok":
        return "recusado"
    if BytesNasm == BytesNossos:
        return "igual"
    for Classe, Dados in TABELA_CLASSES.items():
        if Caso["grupo"] not in Dados["grupos"]:
            continue
        if Dados["criterio"] == "rip" and "[rel $" in LinhaAsm and ClassificarRip(BytesNasm, BytesNossos):
            return "equivalente:" + Classe
        if Dados["criterio"] == "objdump" and ClassificarObjdump(BytesNasm, BytesNossos, TextoNasm, TextoNossos):
            return "equivalente:" + Classe
    return "falha"


def FatiaCasos(Casos, Posicoes, TamNasm):
    Fatias = []
    for Indice, Caso in enumerate(Casos):
        Inicio = Posicoes[Caso["linha"]]
        Proximo = Casos[Indice + 1]["linha"] if Indice + 1 < len(Casos) else None
        Fim = Posicoes[Proximo] if Proximo is not None else TamNasm
        Fatias.append((Inicio, Fim))
    return Fatias


def ConferirTudo(Pasta):
    Casos = LerIdx(Pasta / "casos.idx")
    Asm = (Pasta / "casos.asm").read_text(encoding="utf-8").splitlines()
    NasmBin = (Pasta / "nasm.bin").read_bytes()
    NossosBin = (Pasta / "nossos.bin").read_bytes()
    Posicoes = LerListagem(Pasta / "casos.lst")
    Fatias = FatiaCasos(Casos, Posicoes, len(NasmBin))
    TextosNasm = Desmontar(Pasta / "nasm.bin")
    TextosNossos = Desmontar(Pasta / "nossos.bin")
    Contagem = {"igual": 0, "falha": 0, "recusado": 0}
    Detalhe = []
    for Caso, (Inicio, Fim) in zip(Casos, Fatias):
        BytesNasm = NasmBin[Inicio:Fim]
        BytesNossos = NossosBin[Caso["off"]:Caso["off"] + Caso["tam"]]
        Resultado = ClassificarCaso(Caso, Asm[Caso["linha"] - 1], BytesNasm, BytesNossos,
                                    TextosNasm.get(Inicio), TextosNossos.get(Caso["off"]))
        Chave = Resultado.split(":")[0]
        Contagem[Chave] = Contagem.get(Chave, 0) + 1
        if Resultado.startswith("equivalente:"):
            Contagem[Resultado] = Contagem.get(Resultado, 0) + 1
        if Chave in ("falha", "recusado") and len(Detalhe) < 20:
            Detalhe.append(f"{Chave} linha {Caso['linha']} {Caso['forma']}: {Asm[Caso['linha'] - 1]!r} nasm={BytesNasm.hex()} nossos={BytesNossos.hex()}")
    return len(Casos), Contagem, Detalhe


def ConferirHashesFadec():
    """Os 5 arquivos de terceiros da Forja (blobs do commit fixado) tem de bater com o sha256 e o tamanho do LEIAME. Devolve a lista de problemas."""
    Leiame = (PASTA_FADEC / "LEIAME.txt").read_text(encoding="utf-8")
    Problemas = []
    for Nome in ARQUIVOS_FADEC:
        Achado = re.search(rf"^{re.escape(Nome)}\s+(\d+)\s+([0-9a-f]{{64}})\s*$", Leiame, re.MULTILINE)
        Arquivo = PASTA_FADEC / Nome
        if Achado is None:
            Problemas.append(f"{Nome}: sem linha 'nome tamanho sha256' no LEIAME.txt")
        elif not Arquivo.is_file():
            Problemas.append(f"{Nome}: arquivo ausente de third\\fadec")
        else:
            Bytes = Arquivo.read_bytes()
            if len(Bytes) != int(Achado.group(1)) or hashlib.sha256(Bytes).hexdigest() != Achado.group(2):
                Problemas.append(f"{Nome}: tamanho {len(Bytes)} ou sha256 diverge do LEIAME.txt (esperado {Achado.group(1)} {Achado.group(2)})")
    return Problemas


def Principal():
    Divergencias = ConferirHashesFadec()
    for Linha in Divergencias:
        print(f"fadec: {Linha}")
    print(f"fadec: {len(ARQUIVOS_FADEC) - len(Divergencias)}/{len(ARQUIVOS_FADEC)} hashes conferem com o LEIAME.txt")
    if Divergencias:
        return 1
    with tempfile.TemporaryDirectory(prefix="forja_enc_") as Temp:
        Pasta = Path(Temp)
        Rc = subprocess.run([str(TESTE_NATIVO), "enc", str(Pasta)]).returncode
        if Rc != 0:
            print(f"teste_nativo.exe enc falhou (rc={Rc})")
            return 1
        Rc = subprocess.run([NASM, "-f", "bin", "-l", str(Pasta / "casos.lst"), "-o", str(Pasta / "nasm.bin"),
                             str(Pasta / "casos.asm")]).returncode
        if Rc != 0:
            print(f"nasm falhou (rc={Rc})")
            return 1
        Total, Contagem, Detalhe = ConferirTudo(Pasta)
    for Linha in Detalhe:
        print(Linha)
    for Classe, Dados in TABELA_CLASSES.items():
        print(f"classe {Classe}: {Contagem.get('equivalente:' + Classe, 0)} casos ({Dados['motivo']})")
    Iguais = Contagem["igual"]
    Equivalentes = sum(Valor for Chave, Valor in Contagem.items() if Chave.startswith("equivalente:"))
    Falhas = Contagem["falha"]
    Recusados = Contagem["recusado"]
    print(f"casos={Total} iguais={Iguais} equivalentes={Equivalentes} falhas={Falhas} recusados={Recusados}")
    return 0 if Falhas == 0 and Recusados == 0 else 1


if __name__ == "__main__":
    sys.exit(Principal())
