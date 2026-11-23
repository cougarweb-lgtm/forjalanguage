"""Verifica o BIR (P6 do backend x64) sobre o corpus inteiro: validador verde, dump deterministico e dourados.

Regras:
  - cada .fj do corpus roda por 'teste_nativo.exe bir <arq>' (dev) e 'bir <arq> --release', 2 rodadas cada;
  - exit 0 = dump (BIR aceito); exit 1 = diagnostico JSON (X0001 estavel ou erro do front-end do corpus err_*);
    exit 3 = X0904 (BIR invalido) e qualquer outro exit = quebra: ambos reprovam;
  - as 2 rodadas tem de dar exit e saida identicos (o dump e deterministico);
  - os dourados em dourado/*.bir sao comparados byte a byte com o dump dev do .fj de mesmo nome da tabela DOURADOS.

Versao: 1.13.0
Changelog: 1.13.0 - M6 P71 (main, portao P73): dourados memoria e formato regravados (std:mem 0.4.0 ganhou MemSnapshot, snapshot, report_since, track_handle e untrack_handle: procs novas no dump e linhas das arena_*/pool_* deslocadas em 2; so memoria.bir e formato.bir mudaram, os outros dourados ficaram byte a byte iguais).
           1.12.0 - M5 P52/P56: dourados memoria e formato regravados (std:mem ganhou VirtualArena: 7 funcoes va_* no dump, linhas das arena_*/pool_* deslocadas e 4 strings novas no pool; os outros 13 dourados byte a byte iguais). Conferido por diff com a copia anterior.
           1.11.0 - v1.11 L13: dourado raiz (sqrt de f32 e f64 vira RAIZ). Gravado so o raiz.bir novo.
           1.10.0 -M2 P15b: dourado formato (f"..." que imprime, format e format_c com heap, frame e Arena; inteiro, hexa, bool, f32, f64, cstr). Gravado so o formato.bir novo (os outros dourados nao foram regravados).
           1.9.0 - M2 P15a: dourado opcional (?i32, ?^T, ?cstr, tupla, union e switch de ?T). Gravado so o opcional.bir novo (os outros dourados nao foram regravados).
           1.8.0 - M2 P14b: dourado memoria (new no heap e no frame, lista no frame com make/append/indice/fatia/len/delete, frame_reset, Pool; foreign interno da std como proc). Gravado so o memoria.bir novo.
           1.7.0 - M2 P13b: dourado array (literal de [N]T, elemento conferido, fatia, len de fatia, slice_from, array de struct).
           1.6.0 - M2 P13a: dourado texto (literal de str e de cstr, print, == e !=, cstr as str, len, str por valor em parametro e retorno).
           1.5.0 - M2 P12a: dourados resultado (T ! E, try, or_else, switch com ligacao, assert, main -> ! E) e chaveia (switch de enum com valores e padrao, break de laco, panic).
           1.4.0 - M2 P11b: dourado abi (struct por valor de 8 e 12 B, copia privada do argumento, retorno de 12 B pelo endereco de destino).
           1.3.0 - M2 P11a: dourado struct (struct local com padding, copia, &x e p^, faixa de campo em dev).
           1.2.0 - M2 P10b: dourado real (f32/f64 em local, parametro, retorno, conta, comparacao e conversoes).
           1.1.0 - M2 P9: dourado divisao (i32 / em dev com as 3 guardas).
           1.0.0 - backend x64 P6: primeira versao.
Verificacao: python verificar_bir.py --selftest   |   python verificar_bir.py --forja <forja.exe> [--gravar-dourados]
"""

import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
RaizLinguagem = PastaAqui.parent.parent.parent
ForjaPadrao = RaizLinguagem / "compiler" / "forja.exe"
PastaDourados = PastaAqui / "dourado"
TimeoutSegundos = 60
ExitDump, ExitDiagnostico, ExitInvalido = 0, 1, 3

# nome do dourado -> .fj de origem (relativo a RaizLinguagem)
DOURADOS = {
    "01_ola": "..\\examples\\01_ola.fj",
    "para": "tests\\nativo\\bir\\dourado\\para.fj",
    "conversao_checada": "tests\\nativo\\bir\\dourado\\conversao_checada.fj",
    "divisao": "tests\\nativo\\bir\\dourado\\divisao.fj",
    "real": "tests\\nativo\\bir\\dourado\\real.fj",
    "struct": "tests\\nativo\\bir\\dourado\\struct.fj",
    "abi": "tests\\nativo\\bir\\dourado\\abi.fj",
    "resultado": "tests\\nativo\\bir\\dourado\\resultado.fj",
    "chaveia": "tests\\nativo\\bir\\dourado\\chaveia.fj",
    "texto": "tests\\nativo\\bir\\dourado\\texto.fj",
    "array": "tests\\nativo\\bir\\dourado\\array.fj",
    "memoria": "tests\\nativo\\bir\\dourado\\memoria.fj",
    "opcional": "tests\\nativo\\bir\\dourado\\opcional.fj",
    "formato": "tests\\nativo\\bir\\dourado\\formato.fj",
    "raiz": "tests\\nativo\\bir\\dourado\\raiz.fj",
}

PASTAS_CORPUS = [
    ("..\\examples", "*.fj"),
    ("tests\\dourado", "*.fj"),
    ("tests\\lowering", "*.fj"),
    ("tests\\semantica", "ok_*.fj"),
    ("tests\\nativo\\m1", "*.fj"),
    ("tests\\nativo\\x", "*.fj"),
    ("tests\\nativo\\bir\\dourado", "*.fj"),
]


def ListarCorpus():
    Arquivos = []
    for Pasta, Padrao in PASTAS_CORPUS:
        Arquivos += sorted((RaizLinguagem / Pasta).rglob(Padrao))
    return Arquivos


def RodarBir(Driver, Arquivo, Release):
    Argumentos = [str(Driver), "bir", str(Arquivo)] + (["--release"] if Release else [])
    try:
        Rodada = subprocess.run(
            Argumentos, capture_output=True, timeout=TimeoutSegundos, cwd=RaizLinguagem / "compiler",
        )
    except subprocess.TimeoutExpired:
        return (-1, b"timeout")
    return (Rodada.returncode, Rodada.stdout)


def CodigoDoDiagnostico(Saida):
    try:
        return json.loads(Saida.decode("utf-8", errors="replace").splitlines()[0]).get("codigo", "?")
    except (ValueError, IndexError):
        return "?"


def ConferirArquivo(Driver, Arquivo):
    """Devolve (classe, problemas): classe = 'bir' | 'X0001' | 'front-end' | 'FALHA'."""
    Problemas = []
    Classe = "FALHA"
    for Release in (False, True):
        Primeira = RodarBir(Driver, Arquivo, Release)
        Segunda = RodarBir(Driver, Arquivo, Release)
        Modo = "release" if Release else "dev"
        if Primeira != Segunda:
            Problemas.append(f"{Modo}: duas rodadas diferentes")
        if Primeira[0] == ExitInvalido:
            Problemas.append(f"{Modo}: X0904 (BIR invalido) {Primeira[1][:160]!r}")
        elif Primeira[0] not in (ExitDump, ExitDiagnostico):
            Problemas.append(f"{Modo}: exit {Primeira[0]} (quebra)")
        if not Release:
            Classe = ClasseDe(Primeira)
    return Classe, Problemas


def ClasseDe(Rodada):
    if Rodada[0] == ExitDump:
        return "bir"
    if Rodada[0] == ExitDiagnostico:
        return "X0001" if CodigoDoDiagnostico(Rodada[1]) == "X0001" else "front-end"
    return "FALHA"


def ConferirDourados(Driver, Gravar):
    Problemas = []
    for Nome, Origem in DOURADOS.items():
        # caminho relativo ao cwd do driver: o dump grava o caminho do .fj, que nao pode depender da maquina
        Relativo = os.path.relpath(RaizLinguagem / Origem, RaizLinguagem / "compiler")
        Saida = RodarBir(Driver, Relativo, False)
        if Saida[0] != ExitDump:
            Problemas.append(f"dourado {Nome}: exit {Saida[0]}")
            continue
        Caminho = PastaDourados / f"{Nome}.bir"
        if Gravar:
            PastaDourados.mkdir(exist_ok=True)
            Caminho.write_bytes(Saida[1])
        elif not Caminho.exists() or Caminho.read_bytes() != Saida[1]:
            Problemas.append(f"dourado {Nome}: dump difere de {Caminho.name}")
    return Problemas


def Verificar(Driver, Gravar):
    Corpus = ListarCorpus()
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Resultados = list(Pool.map(lambda A: ConferirArquivo(Driver, A), Corpus))
    Contagem = {}
    Falhas = 0
    for Arquivo, (Classe, Problemas) in zip(Corpus, Resultados):
        Contagem[Classe] = Contagem.get(Classe, 0) + 1
        for Problema in Problemas:
            Falhas += 1
            print(f"FALHA {Arquivo.relative_to(RaizLinguagem.parent)}: {Problema}")
    for Problema in ConferirDourados(Driver, Gravar):
        Falhas += 1
        print(f"FALHA {Problema}")
    print(f"corpus {len(Corpus)} arquivos: {Contagem}; falhas={Falhas}")
    return Falhas == 0


def Autoteste():
    """O corpus tem de existir e a tabela de dourados apontar para .fj reais."""
    Corpus = ListarCorpus()
    if len(Corpus) < 100:
        print(f"selftest: corpus com {len(Corpus)} arquivos (esperado >= 100)")
        return False
    for Nome, Origem in DOURADOS.items():
        if not (RaizLinguagem / Origem).exists():
            print(f"selftest: dourado {Nome} aponta para {Origem}, que nao existe")
            return False
    print(f"selftest ok: {len(Corpus)} arquivos no corpus, {len(DOURADOS)} dourados")
    return True


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao)
    Analisador.add_argument("--selftest", action="store_true")
    Analisador.add_argument("--gravar-dourados", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return 0 if Autoteste() else 1
    Driver = Path(Argumentos.forja).resolve().parent / "teste_nativo.exe"
    if not Driver.exists():
        print(f"{Driver} nao existe (rode construir.bat)")
        return 2
    return 0 if Verificar(Driver, Argumentos.gravar_dourados) else 1


if __name__ == "__main__":
    sys.exit(Principal())
