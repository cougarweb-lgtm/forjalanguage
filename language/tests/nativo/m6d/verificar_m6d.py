"""verificar_m6d.py - suite da contabilidade de memoria por lib, handle de std:file e ends (M6d, P92a).

Roda cada fixture de tests\\nativo\\m6d (dev, 2 alvos) com uma COPIA do forja em %TEMP% e confere exit, stdout e a
contagem de R0009 por tipo (lib / handle). Os .a de teste sao copiados para a pasta de trabalho em %TEMP%.
--selftest confere o proprio harness (tabela x arquivos, sha256 das .a, numeros do oraculo, leitura do JSON no stderr)
sem chamar o compilador: e o aceite do P92a.

Versao: 0.6.0
Changelog:
  0.6.0 - 0.21.0 F4 passo 3: sai o backend C daqui: --congelar (gravava o C de forja_g1) e a perna C do --release-igual saem; o --release-igual compara so x64 windows e x64 linux (COM x SEM ends); o --contra-congelado segue igual (x64 contra o oraculo congelado, sem gcc).
  0.5.0 - contrato 0210 P3 (classe m6d): --congelar e --contra-congelado; a perna C do --release-igual sai do oraculo_congelado_g1_m6d.json (forja_g1 0.20.4 + gcc: grava so onde C == x64, exit + stdout); --contra-congelado compara o x64 windows com o congelado, sem gcc; as pernas x64 windows e x64 linux nao mudam.
  0.4.1 - M6d P92i: --release-oraculo mascara a versao do exe (ELF 'Forja Native X.Y.Z'; PE tambem VERSIONINFO) porque o oraculo e de outra versao; ExecutarLinux e ExecutarWindows rodam em pasta propria (linux_<stem>, win_<stem>) apagada a cada execucao (--repetir: open(create) sobre arquivo existente falha nos 2 SOs, nao so no Linux).
  0.4.0 - M6d P92e: --release-igual (fixtures do grupo fim COM x SEM ` ends`, release byte a byte em x64 windows, x64 linux e backend c; ruido = 2 builds do mesmo fonte); fim_global confere "global": true no report_since.
  0.3.0 - M6d P92d: lib_report na tabela (aguarda vazio), bytes (fixo/oraculo), pilha e "libs" do report_since conferidos; fim_sem_tabela so no alvo windows (kernel32 no linux e X0003).
  0.2.0 - M6d P92c: alvo linux (ExecutarLinux: forja build --target x86_64-linux com o forja original + wsl alpine-build; vendor\\linux copiado para a pasta de trabalho).
  0.1.0 - M6d P92a: primeira versao. Grupos lib|fim|file|tudo; --alvo windows|linux|ambos; --repetir N; --forja <exe>;
          --selftest. --release-igual e --grupo fim com e sem `ends` ficam para o P92e (sem o ligador de ends nao ha o que comparar).
Uso: python tests\\nativo\\m6d\\verificar_m6d.py --selftest
     python tests\\nativo\\m6d\\verificar_m6d.py --grupo lib --alvo ambos [--repetir N] [--forja <forja.exe>]
     python tests\\nativo\\m6d\\verificar_m6d.py --contra-congelado   (x64 contra o congelado; sem gcc, FORJA_CC vazio e PATH sem gcc)
Depende de: stdlib do Python; forja (copia em %TEMP%); wsl alpine-build (alvo linux); oraculo_congelado.py (tests\\nativo\\rapido, so a API e o banco congelado).
"""

import argparse
import base64
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "rapido"))  # banco congelado (API so de leitura e registro)
import oraculo_congelado as Oraculo

PastaM6d = Path(__file__).resolve().parent
PastaLibteste = PastaM6d / "libteste"
PastaOraculo = PastaM6d / "oraculo"
PastaForjaVendor = PastaM6d.parents[2] / "vendor"
ForjaPadrao = Path(os.environ.get("TEMP", tempfile.gettempdir())) / "forja_m6c.exe"
TetoSegundos = 120
PrefixoR0009 = "R0009"
PastaRapido = PastaM6d.parent / "rapido"
CaminhoCongelado = PastaRapido / "oraculo_congelado_g1_m6d.json"
ClasseCongelado = "m6d"
ForjaX64Atual = PastaM6d.parents[2] / "compiler" / "forja.exe"  # x64 atual, usado no congelamento e no contra

# arquivo (relativo a tests\nativo\m6d) -> (grupo, exit dev, R0009 de lib, R0009 de handle, stdout esperado)
TABELA_FIXTURES = {
    "lib/lib_vaza.fj": ("lib", 101, 1, 0, "ok"),
    "lib/lib_ok.fj": ("lib", 0, 0, 0, "ok"),
    "lib/lib_cruzada.fj": ("lib", 0, 0, 0, "ok"),
    "lib/lib_duas.fj": ("lib", 101, 2, 0, "ok"),
    "lib/lib_grupos.fj": ("lib", 101, 17, 0, "ok"),
    "lib/lib_livre_crt.fj": ("lib", 0, 0, 0, "ok"),
    "lib/lib_thread.fj": ("lib", 0, 0, 0, "ok"),
    "lib/lib_thread_sobra.fj": ("lib", 101, 1, 0, "ok"),
    "lib/lib_proc.fj": ("lib", 101, 1, 0, "ok"),
    "lib/cjson_vaza.fj": ("lib", 101, 1, 1, "ok"),
    "lib/cjson_ok.fj": ("lib", 0, 0, 0, "ok"),
    "lib/zlib_vaza.fj": ("lib", 101, 1, 0, "ok"),
    "lib/zlib_ok.fj": ("lib", 0, 0, 0, "ok"),
    "fim/fim_global.fj": ("fim", 0, 0, 0, "ok"),
    "fim/fim_ok.fj": ("fim", 0, 0, 0, "ok"),
    "fim/fim_vaza.fj": ("fim", 101, 1, 0, "ok"),
    "fim/fim_reabre.fj": ("fim", 0, 0, 0, "ok"),
    "fim/fim_sem_tabela.fj": ("fim", 0, 0, 0, "ok"),
    "fim/sqlite_fim_ok.fj": ("fim", 0, 0, 0, "ok"),
    "fim/sqlite_global.fj": ("fim", 0, 0, 0, "ok"),
    "file/file_aberto.fj": ("file", 101, 0, 1, "aberto"),
    "file/file_ok.fj": ("file", 0, 0, 0, "aberto"),
    "file/file_falha.fj": ("file", 0, 0, 0, "true"),
    "file/file_read_all.fj": ("file", 0, 0, 0, "falhou"),
}
TABELA_FIXTURES["lib/lib_report.fj"] = ("lib", 0, 0, 0, "ok")
# fixtures que dependem de codigo de passo posterior: o aguarda fica no CENSO e aqui
AGUARDA = {}
# fixtures so do alvo windows (link de DLL "kernel32": no linux e X0003)
SO_WINDOWS = {"fim/fim_sem_tabela.fj"}
# bytes esperados da linha R0009 lib (None = vem do oraculo do alvo); pilha esperada (procs, do mais interno)
BYTES_LIB = {"lib/lib_vaza.fj": 100, "lib/cjson_vaza.fj": "cjson", "lib/zlib_vaza.fj": "zlib"}
PILHA_LIB = {"lib/lib_vaza.fj": ["main"], "lib/lib_proc.fj": ["abre", "main"]}


def LerAchadosJson(TextoStderr):
    """Cada achado e uma linha JSON DENTRO do stderr (pode vir misturada com texto). Devolve so os dicts validos."""
    Achados = []
    for Linha in TextoStderr.splitlines():
        Inicio = Linha.find("{")
        if Inicio < 0:
            continue
        try:
            Achados.append(json.loads(Linha[Inicio:]))
        except ValueError:
            continue
    return Achados


def ContarR0009(Achados):
    """(lib, handle): R0009 com tipo lib conta como lib; R0009 sem tipo lib conta como handle."""
    Lib = sum(1 for A in Achados if A.get("codigo") == PrefixoR0009 and A.get("tipo") == "lib")
    Handle = sum(1 for A in Achados if A.get("codigo") == PrefixoR0009 and A.get("tipo") != "lib")
    return Lib, Handle


def Sha256Arquivo(Caminho):
    return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()


def ConfereSha(Pasta):
    """Os .a versionados batem com SHA256.txt? Devolve a lista de divergencias."""
    Divergencias = []
    for Linha in (PastaLibteste / "SHA256.txt").read_text(encoding="utf-8").splitlines():
        Esperado, Rel = Linha.split("  ", 1)
        if Sha256Arquivo(Pasta / Rel) != Esperado:
            Divergencias.append(Rel)
    return Divergencias


def LerEsperadoOraculo(Alvo):
    """Numeros do oraculo por alvo: {'cjson': bytes, 'zlib': bytes}."""
    Numeros = {}
    for Linha in (PastaOraculo / f"esperado_{Alvo}.txt").read_text(encoding="utf-8").splitlines():
        Partes = Linha.replace("|", " ").split()
        if Partes and Partes[0] in ("cjson", "zlib"):
            Numeros[Partes[0]] = int(Partes[1].split("=")[1])
    return Numeros


def SelfTest():
    """Confere o harness sem compilador: tabela x disco, sha das .a, oraculo, leitura do JSON."""
    Falhas = []
    for Rel in TABELA_FIXTURES:
        if not (PastaM6d / Rel).is_file():
            Falhas.append("fixture ausente: " + Rel)
    for Rel in AGUARDA:
        if (PastaM6d / Rel).is_file():
            Falhas.append("aguarda deveria estar ausente ate o passo: " + Rel)
    if ConfereSha(PastaLibteste):
        Falhas.append("sha256 das libs diverge do SHA256.txt")
    for Alvo in ("windows", "linux"):
        Numeros = LerEsperadoOraculo(Alvo)
        if set(Numeros) != {"cjson", "zlib"} or Numeros["cjson"] != 631:
            Falhas.append(f"oraculo {Alvo} fora do esperado: {Numeros}")
    Amostra = 'x R0009 {"codigo": "R0009", "tipo": "lib", "lib": "teste_a", "bytes": 100}\nruido sem json\n'
    Achados = LerAchadosJson(Amostra)
    if ContarR0009(Achados) != (1, 0):
        Falhas.append(f"leitura do JSON no stderr: {ContarR0009(Achados)}")
    if ContarR0009(LerAchadosJson('{"codigo": "R0009", "tipo": "handle"}')) != (0, 1):
        Falhas.append("handle nao contado como handle")
    if not all(Chave.startswith(("lib/", "fim/", "file/")) for Chave in TABELA_FIXTURES):
        Falhas.append("grupo fora de lib|fim|file")
    if Falhas:
        for Falha in Falhas:
            print("FALHA " + Falha)
        print("selftest: FALHOU (" + str(len(Falhas)) + ")")
        return 1
    print(f"selftest: PASSOU ({len(TABELA_FIXTURES)} fixtures, {len(AGUARDA)} aguardando, sha256 das libs ok, oraculo ok)")
    return 0


def PrepararVendor(Tmp):
    """O forja acha os modulos vendor:x e as .a de vendor\\ ao lado do arquivo raiz: copia so o que as fixtures usam."""
    Destino = Path(Tmp) / "vendor"
    for Pasta in ("json", "compression", "db"):
        shutil.copytree(PastaForjaVendor / Pasta, Destino / Pasta, dirs_exist_ok=True)
    for Nome in ("libcjson.a", "libz.a", "libsqlite3.a"):
        shutil.copyfile(PastaForjaVendor / Nome, Destino / Nome)
        shutil.copyfile(PastaForjaVendor / Nome, Path(Tmp) / Nome)  # link direto na fixture: a busca e a pasta do .fj


def ExecutarWindows(Forja, Fixture, Tmp):
    """Dev no Windows: forja run no exe copiado para Tmp. Devolve (exit, stdout, stderr)."""
    Pasta = Path(Tmp) / ("win_" + Path(Fixture).stem)  # cwd proprio e limpo a cada execucao: o arquivo da rodada anterior (--repetir) faria o open(create) falhar tambem no Windows
    shutil.rmtree(Pasta, ignore_errors=True)
    Pasta.mkdir(exist_ok=True)
    Resultado = subprocess.run([str(Forja), "run", str(Fixture)], capture_output=True, text=True, timeout=TetoSegundos, cwd=Pasta)
    return Resultado.returncode, Resultado.stdout.strip(), Resultado.stderr


def ParaWsl(Caminho):
    """C:/x/y -> /mnt/c/x/y."""
    Texto = Path(Caminho).as_posix()
    return "/mnt/" + Texto[0].lower() + Texto[2:] if len(Texto) > 2 and Texto[1] == ":" else Texto


def PrepararVendorLinux(Tmp):
    """O alvo linux acha as estaticas ELF em <pasta do .fj>\\vendor\\linux."""
    Destino = Path(Tmp) / "vendor" / "linux"
    Destino.mkdir(parents=True, exist_ok=True)
    for Nome in ("libcjson.a", "libz.a", "libsqlite3.a"):
        shutil.copyfile(PastaForjaVendor / "linux" / Nome, Destino / Nome)
    for Nome in ("libteste_a.a", "libteste_b.a"):
        shutil.copyfile(PastaLibteste / "linux" / Nome, Destino / Nome)


def ExecutarLinux(Forja, Fixture, Tmp):
    """Dev no Linux: forja build --target x86_64-linux (o forja ORIGINAL: a copia perde a semente musl) e o ELF roda no wsl alpine-build. Devolve (exit, stdout, stderr)."""
    Elf = Path(Tmp) / (Path(Fixture).stem + ".elf")
    Build = subprocess.run([str(Forja), "build", "--target", "x86_64-linux", str(Fixture), "-o", str(Elf)], capture_output=True, text=True, timeout=TetoSegundos, cwd=Tmp)
    if Build.returncode != 0:
        return Build.returncode, "", "build: " + Build.stdout[:300] + Build.stderr[:300]
    Pasta = Path(Tmp) / ("linux_" + Path(Fixture).stem)  # cwd proprio: o arquivo que a execucao Windows deixou em Tmp nao pode existir (create falha no Linux)
    shutil.rmtree(Pasta, ignore_errors=True)  # e o da rodada anterior (--repetir) tambem nao: open(create) sobre arquivo existente falha no Linux
    Pasta.mkdir(exist_ok=True)
    Resultado = subprocess.run(["wsl.exe", "-d", "alpine-build", "--cd", ParaWsl(Pasta), "--", ParaWsl(Elf)], capture_output=True, timeout=TetoSegundos)
    Erro = Resultado.stderr.decode("utf-8", errors="replace").replace("\x00", "")
    return Resultado.returncode, Resultado.stdout.decode("utf-8", errors="replace").replace("\r", "").strip(), Erro


def Conferir(Rel, Dados, Exit, Saida, Erro):
    """Compara com a TABELA. Devolve a lista de divergencias (vazia = passou)."""
    _, ExitEsperado, LibEsperado, HandleEsperado, SaidaEsperada = Dados
    Lib, Handle = ContarR0009(LerAchadosJson(Erro))
    Problemas = []
    if Exit != ExitEsperado:
        Problemas.append(f"exit {Exit} (esperado {ExitEsperado})")
    if Saida != SaidaEsperada:
        Problemas.append(f"stdout {Saida[:120]!r} (esperado {SaidaEsperada!r})")
    if Lib != LibEsperado:
        Problemas.append(f"R0009 lib {Lib} (esperado {LibEsperado})")
    if Handle != HandleEsperado:
        Problemas.append(f"R0009 handle {Handle} (esperado {HandleEsperado})")
    Problemas.extend(ConferirDetalhes(Rel, "linux" if "[linux]" in Rel else "windows", Saida, Erro))
    return [Rel + ": " + P for P in Problemas]


def ConferirDetalhes(Rel, Alvo, Saida, Erro):
    """Bytes da linha lib (fixo ou oraculo do alvo), pilha .fj do grupo e o "libs" do report_since (P92d)."""
    Chave = Rel.split(" ")[0]
    Achados = LerAchadosJson(Erro)
    Linhas = [A for A in Achados if A.get("codigo") == PrefixoR0009 and A.get("tipo") == "lib"]
    Problemas = []
    if Chave in BYTES_LIB and Linhas:
        Esperado = BYTES_LIB[Chave]
        if isinstance(Esperado, str):
            Esperado = LerEsperadoOraculo(Alvo)[Esperado]
        if Linhas[0].get("bytes") != Esperado:
            Problemas.append(f"bytes {Linhas[0].get('bytes')} (esperado {Esperado})")
    if Chave in PILHA_LIB and Linhas:
        Procs = [Q.get("proc") for Q in Linhas[0].get("pilha", [])]
        if Procs != PILHA_LIB[Chave]:
            Problemas.append(f"pilha {Procs} (esperado {PILHA_LIB[Chave]})")
    if Chave == "lib/lib_report.fj":
        Relatos = [A for A in Achados if A.get("tipo") == "mem_report"]
        Libs = Relatos[0].get("libs") if Relatos else None
        if Libs != [{"lib": "teste_a", "blocos": 1, "bytes": 100, "global": False}]:
            Problemas.append(f"libs do report_since {Libs}")
    if Chave == "fim/fim_global.fj":
        Relatos = [A for A in Achados if A.get("tipo") == "mem_report"]
        Libs = Relatos[0].get("libs") if Relatos else None
        if Libs != [{"lib": "teste_a", "blocos": 1, "bytes": 64, "global": True}]:
            Problemas.append(f"libs do report_since {Libs}")
    return Problemas


def TirarEnds(Texto):
    """Fonte sem a clausula `ends` (o gemeo do --release-igual): o ` ends` no fim da linha de proc."""
    return re.sub(r"[ \t]+ends[ \t]*(\r?)$", r"\1", Texto, flags=re.MULTILINE)


def ConstruirRelease(Forja, Fonte, Pasta, Alvo, Backend):
    """forja build --release de Fonte (copiada para Pasta com as libs) e devolve os bytes do exe, ou (None, detalhe)."""
    Pasta.mkdir(parents=True, exist_ok=True)
    PrepararVendor(Pasta)
    PrepararVendorLinux(Pasta)
    for Nome in ("libteste_a.a", "libteste_b.a"):
        shutil.copyfile(PastaLibteste / Nome, Pasta / Nome)
    Destino = Pasta / ("saida" if Alvo == "linux" else "saida.exe")
    Comando = [str(Forja), "build", "--release", "--backend", Backend] + (["--target", "x86_64-linux"] if Alvo == "linux" else []) + [str(Fonte), "-o", str(Destino)]
    Resultado = subprocess.run(Comando, capture_output=True, text=True, timeout=TetoSegundos, cwd=Pasta)
    if Resultado.returncode != 0 or not Destino.is_file():
        return None, f"build exit {Resultado.returncode}: {Resultado.stdout[:200]}{Resultado.stderr[:200]}"
    return Destino.read_bytes(), ""


def ReleaseIgual(Forja, Rel, Tmp):
    """Release COM x SEM ` ends` nos 2 alvos x64 (windows e linux); o ruido vem de 3 builds do MESMO fonte (c2/c3 contra c1). Devolve problemas."""
    Original = (PastaM6d / Rel).read_text(encoding="utf-8")
    Sem = TirarEnds(Original)
    if Sem == Original:
        return [Rel + ": nenhuma clausula ends para tirar"]
    Variantes = [("x64_windows", "windows", "x64"), ("x64_linux", "linux", "x64")]
    if Rel in SO_WINDOWS:
        Variantes = [V for V in Variantes if V[1] == "windows"]
    Problemas = []
    for Rotulo, Alvo, Backend in Variantes:
        Bytes = {}
        for Tag, Texto in (("c1", Original), ("c2", Original), ("c3", Original), ("s1", Sem)):
            Pasta = Tmp / Rotulo / Path(Rel).stem[:6] / Tag
            Pasta.mkdir(parents=True, exist_ok=True)
            Fonte = Pasta / Path(Rel).name
            Fonte.write_text(Texto, encoding="utf-8")
            Bytes[Tag], Detalhe = ConstruirRelease(Forja, Fonte, Pasta, Alvo, Backend)
            if Bytes[Tag] is None:
                Problemas.append(f"{Rel} [{Rotulo}] {Tag}: {Detalhe}")
                break
        if len(Bytes) < 4 or any(B is None for B in Bytes.values()):
            continue
        if len(Bytes["c1"]) != len(Bytes["s1"]):
            Problemas.append(f"{Rel} [{Rotulo}]: tamanhos {len(Bytes['c1'])} x {len(Bytes['s1'])}")
            continue
        Ruido = set()
        for Outro in ("c2", "c3"):
            if len(Bytes["c1"]) == len(Bytes[Outro]):
                Ruido.update(K for K, (A, B) in enumerate(zip(Bytes["c1"], Bytes[Outro])) if A != B)
        Diferentes = [K for K, (A, B) in enumerate(zip(Bytes["c1"], Bytes["s1"])) if A != B and K not in Ruido]
        if Diferentes:
            Problemas.append(f"{Rel} [{Rotulo}]: release difere em {len(Diferentes)} byte(s), o 1o no {Diferentes[0]} (ruido {len(Ruido)})")
    return Problemas


def MascararVersao(Bruto):
    """O exe release leva a versao do compilador que o gerou: ELF e PE a string 'Forja Native X.Y.Z'; o PE tambem o VERSIONINFO (VS_FIXEDFILEINFO: remendo nos 2 pares de LS;
    strings 'FileVersion'/'ProductVersion' em UTF-16). O oraculo e de outra versao: esses bytes viram '#' (mesmo tamanho) antes de comparar."""
    Bruto = re.sub(rb"Forja Native [0-9.]+", lambda Achado: b"Forja Native " + b"#" * (len(Achado.group(0)) - 13), Bruto)
    Bruto = re.sub(rb"(\xbd\x04\xef\xfe[\x00-\xff]{10})[\x00-\xff]{2}([\x00-\xff]{6})[\x00-\xff]{2}", rb"\1##\2##", Bruto)
    return re.sub(rb"(e\x00r\x00s\x00i\x00o\x00n\x00(?:\x00\x00)+)((?:[0-9]\x00|\.\x00)+)", lambda Achado: Achado.group(1) + b"#\x00" * (len(Achado.group(2)) // 2), Bruto)


def ReleaseContraOraculo(Forja, Oraculo, Fonte, Tmp, Alvo):
    """Release x64 de Fonte com o forja novo e com o oraculo (forja_m6c.exe); ruido = o oraculo contra ele mesmo (carimbo/GUID do PE). Devolve (problema ou "", bytes, ruido)."""
    Pasta = Tmp / (Path(Fonte).stem + "_" + Alvo)
    Pasta.mkdir(parents=True, exist_ok=True)
    PrepararVendor(Pasta)
    PrepararVendorLinux(Pasta)
    for Nome in ("libteste_a.a", "libteste_b.a"):
        shutil.copyfile(PastaLibteste / Nome, Pasta / Nome)
    if (Path(Fonte).parent / "cjson_entrada.json").is_file():
        shutil.copyfile(Path(Fonte).parent / "cjson_entrada.json", Pasta / "cjson_entrada.json")
    Copia = Pasta / Path(Fonte).name
    shutil.copyfile(Fonte, Copia)
    Bytes = []
    for Qual in (Oraculo, Oraculo, Forja):
        Bruto, Detalhe = ConstruirRelease(Qual, Copia, Pasta, Alvo, "x64")
        if Bruto is None:
            return f"{Path(Fonte).name} [{Alvo}]: {Detalhe}", 0, 0
        Bytes.append(MascararVersao(Bruto))
    Ruido = {K for K, (A, B) in enumerate(zip(Bytes[0], Bytes[1])) if A != B} if len(Bytes[0]) == len(Bytes[1]) else set()
    Diferentes = [K for K, (A, B) in enumerate(zip(Bytes[0], Bytes[2])) if A != B and K not in Ruido]
    if len(Bytes[0]) != len(Bytes[2]) or Diferentes:
        return f"{Path(Fonte).name} [{Alvo}]: tamanhos {len(Bytes[0])} x {len(Bytes[2])}, {len(Diferentes)} byte(s) diferentes (ruido {len(Ruido)})", 0, 0
    return "", len(Bytes[0]), len(Ruido)


def TextosDaVariante(Rel):
    """As duas variantes do fim, o mesmo par do --release-igual: orig (com ends) e sem (ends tirado)."""
    Original = (PastaM6d / Rel).read_text(encoding="utf-8")
    return [("orig", Original), ("sem", TirarEnds(Original))]


def RodarExeRelease(Dados, Pasta):
    """Grava o exe release numa pasta propria e roda: (exit, stdout). So o programa, sem gcc."""
    Pasta.mkdir(parents=True, exist_ok=True)
    Exe = Pasta / "programa.exe"
    Exe.write_bytes(Dados)
    Resultado = subprocess.run([str(Exe)], capture_output=True, text=True, timeout=TetoSegundos, cwd=Pasta)
    return Resultado.returncode, Resultado.stdout.strip()


def ConstruirERodar(Forja, Rel, Texto, *, PastaTmp, Variante, Backend):
    """Release windows do texto da variante (backend x64) e roda o exe: (exit, stdout, motivo). Build recusado = (None, None, motivo)."""
    Pasta = PastaTmp / "congelado" / Path(Rel).stem / Variante / Backend
    Pasta.mkdir(parents=True, exist_ok=True)
    Fonte = Pasta / Path(Rel).name
    Fonte.write_text(Texto, encoding="utf-8")
    Dados, Motivo = ConstruirRelease(Forja, Fonte, Pasta, "windows", Backend)
    if Dados is None:
        return None, None, Motivo
    Exit, Saida = RodarExeRelease(Dados, Pasta / "rodar")
    return Exit, Saida, ""


def AbrirCongelado():
    """Abre o banco m6d so para leitura: o cabecalho tem de bater antes (selado = so o selo, sem gcc e sem regerar)."""
    Banco = Oraculo.BancoOraculo(CaminhoCongelado, Oraculo.FORJA_G1)
    Banco.ExigirCabecalho()
    return Banco


def ComparaComCongelado(Rel, Variante, Registro, ResultadoNativo):
    """Um item: x64 (exit, stdout, motivo) contra o registro congelado. None = chave ausente (falha); pulado = o C recusou ao congelar."""
    Exit, Saida, Motivo = ResultadoNativo
    Nome = f"{Rel} [{Variante}]"
    if Registro is None:
        return ("FALHOU", Nome + ": congelado-ausente (fixture nova ou editada depois do selo: .esperado a mao, nao regere)")
    if Registro.get("pulado"):
        return ("PULADO", f"{Nome}: {Registro['motivo']}")
    if Exit is None:
        return ("FALHOU", f"{Nome}: x64 recusou: {Motivo}")
    Esperado = (Registro["exit"], base64.b64decode(Registro["stdout"]).decode("utf-8"))
    if (Exit, Saida) != Esperado:
        return ("FALHOU", f"{Nome}: x64 exit {Exit} stdout {Saida[:120]!r} != congelado exit {Esperado[0]} stdout {Esperado[1][:120]!r}")
    return ("IGUAL", f"{Nome} exit {Exit}, stdout {len(Saida)} car.")


def ContraCongeladoFim(Rel, Tmp, Banco, ForjaX64):
    """--contra-congelado: as pernas x64 (windows e linux, bytes) do --release-igual sem gcc, e o x64 windows contra o C congelado; chave ausente e falha, nunca regera."""
    Linhas = [("FALHOU", P) for P in ReleaseIgual(ForjaX64, Rel, Tmp)]
    for Variante, Texto in TextosDaVariante(Rel):
        ResultadoNativo = ConstruirERodar(ForjaX64, Rel, Texto, PastaTmp=Tmp, Variante=Variante, Backend="x64")
        try:
            Registro = Banco.Obter(PastaM6d / Rel, "release-" + Variante, (), False, "release", ClasseCongelado)
        except Oraculo.ErroOraculo as Erro:
            Linhas.append(("FALHOU", f"{Rel} [{Variante}]: {Erro}"))
            continue
        Linhas.append(ComparaComCongelado(Rel, Variante, Registro, ResultadoNativo))
    return Linhas


def Principal(Argumentos):
    Parser = argparse.ArgumentParser(description="suite M6d (P92a)")
    Parser.add_argument("--selftest", action="store_true")
    Parser.add_argument("--grupo", choices=["lib", "fim", "file", "tudo"], default="tudo")
    Parser.add_argument("--alvo", choices=["windows", "linux", "ambos"], default="windows")
    Parser.add_argument("--repetir", type=int, default=1)
    Parser.add_argument("--release-igual", action="store_true", help="release COM x SEM ` ends` byte a byte nos 2 alvos x64 (so as fixtures do grupo fim)")
    Parser.add_argument("--release-oraculo", default="", help="forja de referencia (forja_m6c.exe): release x64 windows e linux dos examples 01..04 e das fixtures lib/ byte a byte contra ele")
    Parser.add_argument("--so", default="", help="so as fixtures cujo caminho contem este texto (ex.: thread)")
    Parser.add_argument("--contra-congelado", action="store_true", help="x64 windows e linux do grupo fim sem gcc; o x64 windows contra o congelado (implica o grupo fim)")
    Parser.add_argument("--forja", default=None, help="padrao: forja de TEMP; com --contra-congelado: compiler\\forja.exe (x64 atual)")
    Opcoes = Parser.parse_args(Argumentos)
    if Opcoes.selftest:
        return SelfTest()
    Forja = Path(Opcoes.forja or (ForjaX64Atual if Opcoes.contra_congelado else ForjaPadrao))
    if not Forja.is_file():
        print("forja nao encontrado: " + str(Forja))
        return 2
    Escolhidas = [R for R, D in TABELA_FIXTURES.items() if Opcoes.grupo in ("tudo", D[0]) and Opcoes.so in R]
    Falhas = []
    with tempfile.TemporaryDirectory() as Tmp:
        Copia = Path(Tmp) / "forja.exe"
        shutil.copyfile(Forja, Copia)
        PrepararVendor(Tmp)
        PrepararVendorLinux(Tmp)
        for Nome in ("libteste_a.a", "libteste_b.a"):
            shutil.copyfile(PastaLibteste / Nome, Path(Tmp) / Nome)
        if Opcoes.release_oraculo:
            Itens = [PastaM6d.parents[3] / "examples" / N for N in ("01_ola.fj", "02_resultado.fj", "03_unidades_e_faixa.fj", "04_defer_e_testes.fj")]
            Itens += [PastaM6d / R for R in TABELA_FIXTURES if R.startswith("lib/")]
            for Item in Itens:
                for Alvo in ("windows", "linux"):
                    Problema, Tamanho, Ruido = ReleaseContraOraculo(Forja, Path(Opcoes.release_oraculo), Item, Path(Tmp), Alvo)
                    print(("FALHOU " if Problema else "IGUAL  ") + (Problema or f"{Item.name} [{Alvo}] ({Tamanho} B, ruido {Ruido})"))
                    Falhas.extend([Problema] if Problema else [])
            Escolhidas = []
        if Opcoes.contra_congelado:
            Banco = AbrirCongelado()
            Linhas = []
            for Rel in [R for R in Escolhidas if R.startswith("fim/")]:
                Linhas += ContraCongeladoFim(Rel, Path(Tmp), Banco, Forja)
            for Estado, Texto in Linhas:
                if Estado in ("FALHOU", "DIVERGE"):
                    Falhas.append(f"[{Estado}] {Texto}")
                else:
                    print(f"{Estado} {Texto}")
            Contagem = Counter(Estado for Estado, _ in Linhas)
            print("verificar_m6d contra-congelado: " + ", ".join(f"{Estado} {Qtd}" for Estado, Qtd in sorted(Contagem.items())))
            Escolhidas = []
        elif Opcoes.release_igual:
            for Rel in [R for R in Escolhidas if R.startswith("fim/")]:
                Problemas = ReleaseIgual(Forja, Rel, Path(Tmp))
                print(("FALHOU " if Problemas else "PASSOU ") + "release-igual " + Rel)
                Falhas.extend(Problemas)
            Escolhidas = []
        for Rel in Escolhidas:
            Fixture = Path(Tmp) / Path(Rel).name
            shutil.copyfile(PastaM6d / Rel, Fixture)
            for _ in range(Opcoes.repetir):
                if Opcoes.alvo != "linux":
                    Exit, Saida, Erro = ExecutarWindows(Copia, Fixture, Tmp)
                    Falhas.extend(Conferir(Rel + " [windows]", TABELA_FIXTURES[Rel], Exit, Saida, Erro))
                if Opcoes.alvo != "windows" and Rel not in SO_WINDOWS:
                    Exit, Saida, Erro = ExecutarLinux(Forja, Fixture, Tmp)
                    Falhas.extend(Conferir(Rel + " [linux]", TABELA_FIXTURES[Rel], Exit, Saida, Erro))
    for Falha in Falhas:
        print("FALHA " + Falha)
    print(f"verificar_m6d {Opcoes.grupo}: {len(Escolhidas) * Opcoes.repetir} execucoes, {len(Falhas)} falha(s)")
    return 0 if not Falhas else 1


if __name__ == "__main__":
    try:
        sys.exit(Principal(sys.argv[1:]))
    except Oraculo.ErroOraculo as Erro:
        print(f"ERRO: {Erro}")
        sys.exit(2)
