r"""Verifica o orquestrador de projeto (forja build --projeto) com projetos minusculos montados em %TEMP%: 30 casos.

Casos 1..14 (--plano): cada um monta um projeto com a DLL "lib" (importa mods\\util.fj) e o exe "app" (foreign "lib"), constroi os produtos com os mesmos filhos
"forja build --backend x64 ... --entradas ... --lib-pasta ..." que o orquestrador dispara, grava o estado (dev.tsv / release.tsv) com a chave calculada AQUI, em Python
(FNV-1a 64 sobre o texto canonico do contrato), e confere o que o plano diz. Assim o teste tambem prova que a chave do C e a do Python sao a mesma.
Casos 15..26 (execucao, P22-O2): rodam o orquestrador de verdade (jobs, falha isolada, saida em bloco, B0006, B0008, pai morto, DLL em uso, hash_interface real).
Caso 27 (F6): FORJA_ORQ_TEMPOS grava o jsonl por etapa; sem a variavel nenhum arquivo e a saida e a mesma.

Caso 28 (P22-O3): 3 dlls Forja puras + exe (fixture ordem_stdout\): a saida com stdout em arquivo e em pipe sai em ordem, igual ao esperado.txt fixo (capturado do backend C e conferido igual na 0.20.4).

Caso 29 (F8): o estado gravado sem descarga em disco, truncado/vazio/com lixo, so faz recompilar.
Caso 30 (M4 P24a2): o jsonl do FORJA_ORQ_TEMPOS traz ini_us em toda linha; etapas e vidas dos filhos com instante >= 0, total em 0 e o app depois das dlls.

Versao: 0.6.3
Changelog: 0.6.3 - 0.21.0 F3: caso 28 deixa de rodar o backend C ao vivo; compara so o x64 com esperado.txt (fixo, conferido igual ao C na 0.20.4).
           0.6.2 - IA1-b09: o JSON exato do B0005/B0007 aceita o campo "spec" ("cli: ...") no fim; o resto continua exato.
           0.6.1 - M4 P24c: PROCEDIMENTOS_GRANDES 30000 -> 100000. Com o compilador ~2x mais rapido (arena) os 3 filhos do caso "pai morto" terminavam antes do kill
                    ("proximo build" achava tudo reusado): a fixture acompanha a velocidade do compilador, mesmo teste e mesmas contagens (30).
           0.6.0 - M4 P24a2: +1 caso (ini_us nos tempos do orquestrador; 29 -> 30).
           0.5.0 - M3 F8: +1 caso (estado torto; 28 -> 29).
           0.4.1- M3 F5: docstring raw (prefixo r) para o SyntaxWarning do escape '\)'; mesmo texto.
           0.4.0 - M3 P22-O3: +1 caso (stdout em ordem com 3 dlls puras; 27 -> 28), fixture tests\projeto\ordem_stdout\.
           0.3.0 - M3 F6: +1 caso (FORJA_ORQ_TEMPOS; 26 -> 27); FORJA_ORQ_TEMPOS entra em VARIAVEIS_DO_TESTE.
           0.2.0- M3 P22-O2: +12 casos de execucao (3 dlls + exe com jobs 1 e 4 e bytes iguais; falha injetada isolada; erro de usuario com o JSON do filho intacto; B0006; B0008 com 2
           orquestradores; pai morto por TerminateProcess; --jobs invalido; FORJA_JOBS; --json com "filhos"; no-op com filhos 0; DLL em uso; hash_interface real na execucao).
           0.1.0 - M3 P22-O1b: primeira versao (primeira vez; nada mudou; fonte mudou; modulo importado mudou; .glb criado; forja.exe diferente; --release separado; interface
           mudou; so corpo mudou; saida apagada; --tudo; B0005 x2 com JSON exato; B0007).
Uso: python -I verificar_projeto.py --forja <forja.exe>   (use uma COPIA do forja.exe; a saida final e "N/30 casos de projeto")
Verificacao: python -I verificar_projeto.py --forja C:/caminho/forja.exe
"""

import argparse
import hashlib
import json
import os
import re
import secrets
import shutil
import subprocess
import sys
import tempfile
import time
from collections import namedtuple
from pathlib import Path

SEMENTE_FNV = 0xCBF29CE484222325
PRIMO_FNV = 0x100000001B3
MASCARA_64 = 0xFFFFFFFFFFFFFFFF
FILETIME_DO_UNIX = 116444736000000000
TIMEOUT_FILHO = 120
TOTAL_DE_CASOS = 30
ESPERA_MAXIMA = 60          # segundos esperando um filho aparecer ou um build acabar
PROCEDIMENTOS_GRANDES = 100000   # 400 mil linhas por dll: o build leva ~1 s, tempo de matar o pai
ALVO = "x86_64-windows"
MANIFESTO = "saida = bin\nproduto app exe app.fj depende lib\nproduto lib dll lib.fj\n"
FONTE_UTIL = "pub proc dobro(x: i32) -> i32 {\n    return x * 2\n}\n"
FONTE_LIB = 'import "mods/util"\n\npub export proc somar(a: i32, b: i32) -> i32 {\n    return util.dobro(a) + b\n}\n\nproc main() {\n}\n'
FONTE_APP = 'foreign {\n    link "lib"\n    proc somar(a: i32, b: i32) -> i32\n}\n\nproc main() {\n    print(somar(2, b = 3))\n}\n'
# Textos do catalogo (diag.c): o JSON exato do B0005 e do B0007 sai com eles.
B0005_EXPLICACAO = (
    "O forja.proj (manifesto do projeto) tem uma declaracao invalida; a causa diz o arquivo, a linha e o motivo: nome de produto repetido ou fora de [a-z0-9_]+, "
    "tipo diferente de exe e dll, arquivo de entrada que nao existe, 'depende' de produto que nao existe, 'saida' declarada duas vezes, nenhum produto, ou uma linha "
    "que nao e 'saida = <pasta>' nem 'produto'."
)
B0005_FORMA = "saida = bin  -  produto motor dll src/motor.fj  -  produto jogo exe src/jogo.fj depende motor"
B0007_EXPLICACAO = (
    "Os produtos do forja.proj dependem uns dos outros em ciclo: a causa lista o ciclo na ordem (a -> b -> a). "
    "Sem uma ordem de build nenhum deles pode ser compilado primeiro."
)
B0007_FORMA = "Tire uma das dependencias do ciclo (a clausula 'depende' de um dos produtos listados) ou junte os produtos em um so."

IdsDeForja = {}


def HashFnv(Dados, Hash=SEMENTE_FNV):
    """FNV-1a de 64 bits, igual ao Hash64 do compilador."""
    for Byte in Dados:
        Hash = ((Hash ^ Byte) * PRIMO_FNV) & MASCARA_64
    return Hash


def IdDoForja(Forja):
    """Hash64 dos bytes do forja.exe (o id do binario), uma vez por arquivo."""
    Chave = str(Forja)
    if Chave not in IdsDeForja:
        IdsDeForja[Chave] = HashFnv(Path(Forja).read_bytes())
    return IdsDeForja[Chave]


def Hex16(Valor):
    return "%016x" % Valor


def FiletimeDe(Caminho):
    """mtime em FILETIME (100 ns), o mesmo valor que o compilador le por GetFileAttributesEx."""
    return os.stat(Caminho).st_mtime_ns // 100 + FILETIME_DO_UNIX


class Bancada:
    """Um projeto em %TEMP% (lib dll + app exe) e o estado que um orquestrador teria gravado."""

    def __init__(Self, Forja, Base):
        Self.Forja = Path(Forja)
        Self.Pasta = Path(os.path.abspath(tempfile.mkdtemp(prefix="p_", dir=Base)))
        Self.Linhas = {"dev": {}, "release": {}}
        Self.Escrever("forja.proj", MANIFESTO)
        Self.Escrever("mods/util.fj", FONTE_UTIL)
        Self.Escrever("lib.fj", FONTE_LIB)
        Self.Escrever("app.fj", FONTE_APP)

    def Caminho(Self, Relativo):
        return Self.Pasta / Relativo

    def Escrever(Self, Relativo, Texto):
        Alvo = Self.Caminho(Relativo)
        Alvo.parent.mkdir(parents=True, exist_ok=True)
        Alvo.write_bytes(Texto.encode("utf-8"))

    def Anexar(Self, Relativo, Texto):
        with open(Self.Caminho(Relativo), "ab") as Arquivo:
            Arquivo.write(Texto.encode("utf-8"))

    def PastaEstado(Self, Modo):
        return Self.Caminho(".forja_cache/estado/" + Modo)

    def Filho(Self, Nome, Tipo, Modo):
        """O mesmo filho que o orquestrador dispara; devolve o CompletedProcess."""
        Bin = Self.Caminho("bin")
        Bin.mkdir(parents=True, exist_ok=True)
        Self.PastaEstado(Modo).mkdir(parents=True, exist_ok=True)
        Comando = [str(Self.Forja), "build", "--backend", "x64"]
        Comando += ["--modo", "dll"] if Tipo == "dll" else []
        Comando += ["--release"] if Modo == "release" else []
        Comando += [str(Self.Caminho(Nome + ".fj")), "-o", str(Bin / (Nome + "." + Tipo))]
        Comando += ["--entradas", str(Self.PastaEstado(Modo) / (Nome + ".entradas")), "--lib-pasta", str(Bin)]
        return subprocess.run(Comando, cwd=Self.Pasta, capture_output=True, timeout=TIMEOUT_FILHO)

    def LerEntradas(Self, Nome, Modo):
        Linhas = []
        Texto = (Self.PastaEstado(Modo) / (Nome + ".entradas")).read_text(encoding="utf-8")
        for Linha in Texto.splitlines()[1:]:
            Tipo, Caminho, Tamanho, Mtime, Hash = Linha.split("\t")
            Linhas.append((Tipo, Caminho, Hash))
        return Linhas

    def TextoCanonico(Self, Nome, Tipo, Modo, IdForja, Deps):
        """O texto canonico do contrato; Deps = {nome_da_dep: (hash_interface ou None, caminho da saida)}."""
        Texto = f"forja={Hex16(IdForja)}\nalvo={ALVO}\nmodo={Modo}\nnivel=0\ntipo={Tipo}\nentrada={Self.Caminho(Nome + '.fj')}\nlib-pasta={Self.Caminho('bin')}\n"
        for NomeDep in sorted(Deps):
            Interface = Deps[NomeDep][0]
            Texto += f"dep:{NomeDep}=" + ("-" if Interface is None else Hex16(Interface)) + "\n"
        Saidas = {Caminho.lower() for _, Caminho in Deps.values()}
        for _, Caminho, Hash in Self.LerEntradas(Nome, Modo):
            if Caminho.lower() not in Saidas:
                Texto += f"{Caminho}={Hash}\n"
        return Texto

    def Construir(Self, Nome, Tipo, Modo="dev", Forja=None):
        """Roda o filho e grava a linha do produto no estado do modo."""
        Resultado = Self.Filho(Nome, Tipo, Modo)
        if Resultado.returncode != 0:
            raise RuntimeError(f"build de {Nome} falhou: {Resultado.stderr.decode('utf-8', 'replace')}")
        IdForja = IdDoForja(Forja or Self.Forja)
        Deps = {}
        if Nome == "app":
            Linha = Self.Linhas[Modo]["lib"]
            Deps["lib"] = (Linha["interface"], str(Self.Caminho("bin/lib.dll")))
        Texto = Self.TextoCanonico(Nome, Tipo, Modo, IdForja, Deps)
        Cabecalho = "".join(Texto.splitlines(keepends=True)[:7])
        Saida = Self.Caminho("bin/" + Nome + "." + Tipo)
        Interface = HashFnv(Self.Caminho("bin/" + Nome + ".h").read_bytes()) if Tipo == "dll" else None
        TextoDeps = ",".join(f"{N}=" + ("-" if D[0] is None else Hex16(D[0])) for N, D in sorted(Deps.items())) or "-"
        Self.Linhas[Modo][Nome] = {
            "chave": HashFnv(Texto.encode("utf-8")),
            "interface": Interface,
            "saida": str(Saida),
            "tamanho": Saida.stat().st_size,
            "mtime": FiletimeDe(Saida),
            "ambiente": HashFnv(Cabecalho.encode("utf-8")),
            "deps": TextoDeps,
        }
        Self.GravarEstado(Modo)

    def GravarEstado(Self, Modo):
        Texto = "# forja-estado 1\n"
        for Nome in sorted(Self.Linhas[Modo]):
            L = Self.Linhas[Modo][Nome]
            Interface = "-" if L["interface"] is None else Hex16(L["interface"])
            Texto += "\t".join([Nome, Hex16(L["chave"]), Interface, L["saida"], str(L["tamanho"]), str(L["mtime"]), Hex16(L["ambiente"]), L["deps"]]) + "\n"
        Arquivo = Self.Caminho(f".forja_cache/estado/{Modo}.tsv")
        Arquivo.parent.mkdir(parents=True, exist_ok=True)
        Arquivo.write_bytes(Texto.encode("utf-8"))

    def ConstruirTudo(Self, Modo="dev"):
        Self.Construir("lib", "dll", Modo)
        Self.Construir("app", "exe", Modo)

    def Plano(Self, *Opcoes, Forja=None, Manifesto=None):
        Alvo = str(Manifesto or Self.Caminho("forja.proj"))
        Comando = [str(Forja or Self.Forja), "build", "--projeto", Alvo, "--plano", *Opcoes]
        Resultado = subprocess.run(Comando, cwd=Self.Pasta, capture_output=True, timeout=TIMEOUT_FILHO)
        # O stdio do Windows grava CRLF; o JSON e comparado com LF.
        return Resultado.returncode, Resultado.stdout.decode("utf-8").replace("\r\n", "\n"), Resultado.stderr.decode("utf-8").replace("\r\n", "\n")


def LerPlano(Saida):
    """[(produto, motivo ou None)] na ordem impressa, e (produtos, recompilariam, reusados) do resumo."""
    Itens, Resumo = [], None
    for Linha in Saida.splitlines():
        Recompila = re.fullmatch(r"forja: \[(\w+)\] recompilaria \((.*)\)", Linha)
        Reusa = re.fullmatch(r"forja: \[(\w+)\] reusado", Linha)
        Final = re.fullmatch(r"forja: plano: (\d+) produtos, (\d+) recompilariam, (\d+) reusados", Linha)
        if Recompila:
            Itens.append((Recompila.group(1), Recompila.group(2)))
        elif Reusa:
            Itens.append((Reusa.group(1), None))
        elif Final:
            Resumo = tuple(int(G) for G in Final.groups())
    return Itens, Resumo


def ConferirPlano(Resultado, Esperado):
    """Esperado = [(produto, motivo ou None)] na ordem (camada, nome). Devolve None se bate, senao o texto do desvio."""
    Rc, Saida, Erro = Resultado
    Itens, Resumo = LerPlano(Saida)
    Recompilam = sum(1 for _, Motivo in Esperado if Motivo is not None)
    if Rc != 0 or Erro != "":
        return f"rc={Rc} stderr={Erro!r}"
    if Itens != Esperado:
        return f"plano {Itens} != esperado {Esperado}"
    if Resumo != (len(Esperado), Recompilam, len(Esperado) - Recompilam):
        return f"resumo {Resumo} != {(len(Esperado), Recompilam, len(Esperado) - Recompilam)}"
    return None


def DiagnosticoExato(Resultado, Codigo, Causa, Explicacao, Forma):
    """Rc 2, stdout vazio e a linha JSON exata no stderr."""
    Rc, Saida, Erro = Resultado
    Esperado = json.dumps({"codigo": Codigo, "causa": Causa, "tipo": "build", "explicacao": Explicacao, "forma_certa": Forma}, ensure_ascii=False) + "\n"
    if Rc != 2 or Saida != "":
        return f"rc={Rc} stdout={Saida!r}"
    # IA1-b09: todo diagnostico leva "spec" (secao: trecho) no fim; o resto do JSON tem de ser exatamente o esperado
    Sufixo = Erro.rsplit(', "spec": ', 1)
    Sem = Sufixo[0] + "}\n" if len(Sufixo) == 2 and Sufixo[1].startswith('"cli: ') and Sufixo[1].endswith('"}\n') else Erro
    return None if Sem == Esperado else f"stderr {Erro!r} != {Esperado!r}"


# ---------- orquestrador de verdade (casos 15..26) ----------

TIMEOUT_ORQUESTRADOR = 240
Resultado = namedtuple("Resultado", "Rc Saida Erro ErroCru")
MANIFESTO_LIB_APP = "saida = bin\nproduto app exe app.fj depende lib\nproduto lib dll lib.fj\n"
MANIFESTO_TRES = "saida = bin\nproduto app exe app.fj depende d1 d2 d3\nproduto d1 dll d1.fj\nproduto d2 dll d2.fj\nproduto d3 dll d3.fj\n"
VARIAVEIS_DO_TESTE = ("FORJA_JOBS", "FORJA_TESTE_INJETAR", "FORJA_TESTE_INJETAR_PRODUTO", "FORJA_ORQ_TEMPOS")
REGEX_RECOMPILADO = re.compile(r"forja: \[(\w+)\] recompilado \((.*)\) (\d+) ms(?: \| (.*))?")
REGEX_REUSADO = re.compile(r"forja: \[(\w+)\] reusado")
REGEX_FALHOU = re.compile(r"forja: \[(\w+)\] falhou \(exit (\d+)\)")
REGEX_PULADO = re.compile(r"forja: \[(\w+)\] pulado \(depende de (\w+)\)")
REGEX_PROJETO = re.compile(r"forja: projeto: (\d+) produtos, (\d+) recompilados, (\d+) reusados, (\d+) falharam, (\d+) pulados, jobs=(\d+), (\d+) ms")
CAMPOS_PROJETO = ("produtos", "recompilados", "reusados", "falharam", "pulados", "jobs", "ms")
CODIGO_DO_AJUDANTE = "import ctypes, sys, time\nctypes.WinDLL(sys.argv[1])\nprint('ok', flush=True)\ntime.sleep(120)\n"
REGEX_SOBRA = re.compile(r".*\.(tmp|velho)\d+$")


def AmbienteLimpo(Extra=None):
    """O ambiente do teste sem as variaveis do orquestrador que o usuario possa ter, mais o Extra."""
    Ambiente = {Chave: Valor for Chave, Valor in os.environ.items() if Chave not in VARIAVEIS_DO_TESTE}
    Ambiente.update(Extra or {})
    return Ambiente


def RodarOrquestrador(Forja, Pasta, Opcoes, Ambiente=None):
    """forja build --projeto <Opcoes> na Pasta (o forja.proj da pasta atual)."""
    Comando = [str(Forja), "build", "--projeto", *Opcoes]
    R = subprocess.run(Comando, cwd=Pasta, capture_output=True, timeout=TIMEOUT_ORQUESTRADOR, env=AmbienteLimpo(Ambiente))
    return Resultado(R.returncode, R.stdout.decode("utf-8").replace("\r\n", "\n"), R.stderr.decode("utf-8", "replace").replace("\r\n", "\n"), R.stderr)


def LerResumo(Saida):
    """([(produto, acao, detalhe)] na ordem impressa, {campo: n} do projeto). detalhe: motivo (recompilado), exit (falhou), a dependencia (pulado) ou None (reusado)."""
    Itens, Projeto = [], None
    for Linha in Saida.splitlines():
        if Achou := REGEX_RECOMPILADO.fullmatch(Linha):
            Itens.append((Achou.group(1), "recompilado", Achou.group(2)))
        elif Achou := REGEX_REUSADO.fullmatch(Linha):
            Itens.append((Achou.group(1), "reusado", None))
        elif Achou := REGEX_FALHOU.fullmatch(Linha):
            Itens.append((Achou.group(1), "falhou", int(Achou.group(2))))
        elif Achou := REGEX_PULADO.fullmatch(Linha):
            Itens.append((Achou.group(1), "pulado", Achou.group(2)))
        elif Achou := REGEX_PROJETO.fullmatch(Linha):
            Projeto = dict(zip(CAMPOS_PROJETO, (int(G) for G in Achou.groups())))
    return Itens, Projeto


def FonteDll(Numero):
    return f"pub export proc f{Numero}(a: i32) -> i32 {{\n    return a + {Numero}\n}}\n\nproc main() {{\n}}\n"


def FonteAppTres():
    Blocos = "".join(f'foreign {{\n    link "d{N}"\n    proc f{N}(a: i32) -> i32\n}}\n\n' for N in (1, 2, 3))
    return Blocos + "proc main() {\n    print(f1(10) + f2(10) + f3(10))\n}\n"


def FonteGrande(Prefixo):
    """Uma dll de PROCEDIMENTOS_GRANDES exports: o build dela leva alguns segundos."""
    Partes = ["proc main() {\n}\n"]
    for N in range(PROCEDIMENTOS_GRANDES):
        Partes.append(f"pub export proc g{Prefixo}_{N}(x: i32) -> i32 {{\n    return x + {N}\n}}\n")
    return "".join(Partes)


def ContarProcessos(Imagem):
    """Quantos processos do Windows tem esse nome de imagem."""
    R = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {Imagem}", "/FO", "CSV", "/NH"], capture_output=True, text=True)
    return sum(1 for Linha in R.stdout.splitlines() if Linha.lower().startswith(f'"{Imagem.lower()}"'))


def EsperarAte(Condicao, Processo=None):
    """True quando a Condicao() vale; False se o Processo morreu antes ou passou ESPERA_MAXIMA."""
    Limite = time.monotonic() + ESPERA_MAXIMA
    while time.monotonic() < Limite:
        if Condicao():
            return True
        if Processo is not None and Processo.poll() is not None:
            return Condicao()
        time.sleep(0.01)
    return False


class Obra:
    """Um projeto em %TEMP% para o orquestrador rodar de verdade (sem estado forjado em Python)."""

    def __init__(Self, Forja, Base, Manifesto, Fontes):
        Self.Forja = Path(Forja)
        Self.Pasta = Path(os.path.abspath(tempfile.mkdtemp(prefix="o_", dir=Base)))
        Self.Escrever("forja.proj", Manifesto)
        for Nome, Texto in Fontes.items():
            Self.Escrever(Nome, Texto)

    def Caminho(Self, Relativo):
        return Self.Pasta / Relativo

    def Escrever(Self, Relativo, Texto):
        Alvo = Self.Caminho(Relativo)
        Alvo.parent.mkdir(parents=True, exist_ok=True)
        Alvo.write_bytes(Texto.encode("utf-8"))

    def Anexar(Self, Relativo, Texto):
        with open(Self.Caminho(Relativo), "ab") as Arquivo:
            Arquivo.write(Texto.encode("utf-8"))

    def Rodar(Self, *Opcoes, Ambiente=None, Forja=None):
        return RodarOrquestrador(Forja or Self.Forja, Self.Pasta, Opcoes, Ambiente)

    def HashesDeBin(Self):
        return {A.name: hashlib.sha256(A.read_bytes()).hexdigest() for A in sorted(Self.Caminho("bin").iterdir())}

    def Limpar(Self):
        shutil.rmtree(Self.Caminho("bin"), ignore_errors=True)
        shutil.rmtree(Self.Caminho(".forja_cache"), ignore_errors=True)

    def Sobras(Self):
        """Os *.tmp<pid> e *.velho<pid> que ficaram em qualquer pasta do projeto."""
        return sorted(str(A.relative_to(Self.Pasta)) for A in Self.Pasta.rglob("*") if A.is_file() and REGEX_SOBRA.fullmatch(A.name))

    def SaidaDoApp(Self):
        R = subprocess.run([str(Self.Caminho("bin/app.exe"))], capture_output=True, timeout=30)
        return R.stdout.decode("utf-8").strip()


def ObraTres(Ctx):
    Fontes = {f"d{N}.fj": FonteDll(N) for N in (1, 2, 3)}
    Fontes["app.fj"] = FonteAppTres()
    return Ctx.NovaObra(MANIFESTO_TRES, Fontes)


def ObraLibApp(Ctx):
    return Ctx.NovaObra(MANIFESTO_LIB_APP, {"lib.fj": FONTE_LIB, "app.fj": FONTE_APP, "mods/util.fj": FONTE_UTIL})


def ItensTodosNovos(Nomes):
    return [(Nome, "recompilado", "primeira vez") for Nome in Nomes]


# ---------- os 14 casos ----------


def CasoPrimeiraVez(Ctx):
    B = Ctx.Nova()
    Tudo = [("lib", "primeira vez"), ("app", "primeira vez")]
    Desvio = ConferirPlano(B.Plano(), Tudo)
    if Desvio:
        return Desvio
    Rc, Saida, Erro = B.Plano("--json")
    Linhas = [json.loads(L) for L in Saida.splitlines()]
    Esperado = [{"produto": "lib", "acao": "recompilaria", "motivo": "primeira vez"}, {"produto": "app", "acao": "recompilaria", "motivo": "primeira vez"},
                {"plano": {"produtos": 2, "recompilariam": 2, "reusados": 0}}]
    return None if (Rc == 0 and Erro == "" and Linhas == Esperado) else f"json {Linhas}"


def CasoNadaMudou(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    return ConferirPlano(B.Plano(), [("lib", None), ("app", None)])


def CasoFonteMudou(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    B.Anexar("app.fj", "// mudou\n")
    return ConferirPlano(B.Plano(), [("lib", None), ("app", "fonte mudou: app.fj")])


def CasoModuloImportadoMudou(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    B.Anexar("mods/util.fj", "// mudou\n")
    return ConferirPlano(B.Plano(), [("lib", "fonte mudou: mods\\util.fj"), ("app", None)])


def CasoGlbCriado(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    B.Escrever("assets/modelo.glb", "glTF falso")
    B.Escrever("bin/textura.png", "png falso")
    return ConferirPlano(B.Plano(), [("lib", None), ("app", None)])


def CasoForjaDiferente(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    Outra = Ctx.Base / "forja_outro.exe"
    shutil.copyfile(B.Forja, Outra)
    with open(Outra, "ab") as Arquivo:
        Arquivo.write(b"\x00")
    Motivo = "forja.exe, modo ou opcoes mudaram"
    return ConferirPlano(B.Plano(Forja=Outra), [("lib", Motivo), ("app", Motivo)])


def CasoReleaseSeparado(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    Primeira = [("lib", "primeira vez"), ("app", "primeira vez")]
    Desvio = ConferirPlano(B.Plano("--release"), Primeira) or ConferirPlano(B.Plano(), [("lib", None), ("app", None)])
    if Desvio:
        return Desvio
    B.ConstruirTudo("release")
    return ConferirPlano(B.Plano("--release"), [("lib", None), ("app", None)])


def CasoInterfaceMudou(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    Antes = B.Linhas["dev"]["lib"]["interface"]
    B.Anexar("lib.fj", "\npub export proc triplo(x: i32) -> i32 {\n    return x * 3\n}\n")
    B.Construir("lib", "dll")
    if B.Linhas["dev"]["lib"]["interface"] == Antes:
        return "o .h da lib nao mudou com o novo export"
    return ConferirPlano(B.Plano(), [("lib", None), ("app", "interface de lib mudou")])


def CasoSoCorpoMudou(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    Antes = B.Linhas["dev"]["lib"]["interface"]
    B.Escrever("lib.fj", FONTE_LIB.replace("util.dobro(a) + b", "util.dobro(a) + b + 1"))
    Desvio = ConferirPlano(B.Plano(), [("lib", "fonte mudou: lib.fj"), ("app", None)])
    if Desvio:
        return Desvio
    B.Construir("lib", "dll")
    if B.Linhas["dev"]["lib"]["interface"] != Antes:
        return "o .h da lib mudou so por causa do corpo"
    return ConferirPlano(B.Plano(), [("lib", None), ("app", None)])


def CasoSaidaApagada(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    os.remove(B.Caminho("bin/app.exe"))
    B.Anexar("bin/lib.h", "\n")
    Motivo = "saida ausente ou alterada"
    return ConferirPlano(B.Plano(), [("lib", Motivo), ("app", Motivo)])


def CasoTudo(Ctx):
    B = Ctx.Nova()
    B.ConstruirTudo()
    Motivo = "forcado (--tudo)"
    return ConferirPlano(B.Plano("--tudo"), [("lib", Motivo), ("app", Motivo)])


def CasoB0005NomeRepetido(Ctx):
    B = Ctx.Nova()
    B.Escrever("forja.proj", "produto app exe app.fj\nproduto app dll lib.fj\n")
    Causa = f"{B.Caminho('forja.proj')} linha 2: nome repetido 'app'"
    return DiagnosticoExato(B.Plano(), "B0005", Causa, B0005_EXPLICACAO, B0005_FORMA)


def CasoB0005EntradaAusente(Ctx):
    B = Ctx.Nova()
    B.Escrever("forja.proj", "# comentario\nproduto app exe nao_existe.fj\n")
    Causa = f"{B.Caminho('forja.proj')} linha 2: entrada ausente 'nao_existe.fj'"
    return DiagnosticoExato(B.Plano(), "B0005", Causa, B0005_EXPLICACAO, B0005_FORMA)


def CasoB0007Ciclo(Ctx):
    B = Ctx.Nova()
    B.Escrever("forja.proj", "produto a exe app.fj depende b\nproduto b dll lib.fj depende a\n")
    return DiagnosticoExato(B.Plano(), "B0007", "ciclo de dependencia: a -> b -> a", B0007_EXPLICACAO, B0007_FORMA)


# ---------- os 12 casos de execucao ----------


def CasoJobsUmEQuatro(Ctx):
    O = ObraTres(Ctx)
    Esperado = ItensTodosNovos(["d1", "d2", "d3", "app"])
    R1 = O.Rodar("--jobs", "1")
    Itens1, P1 = LerResumo(R1.Saida)
    if R1.Rc != 0 or Itens1 != Esperado or P1 is None or P1["jobs"] != 1:
        return f"jobs 1: rc={R1.Rc} {Itens1} {P1} stderr={R1.Erro!r}"
    Hashes1 = O.HashesDeBin()
    O.Limpar()
    R4 = O.Rodar("--jobs", "4")
    Itens4, P4 = LerResumo(R4.Saida)
    if R4.Rc != 0 or Itens4 != Esperado or P4 is None or P4["jobs"] != 4:
        return f"jobs 4: rc={R4.Rc} {Itens4} {P4} stderr={R4.Erro!r}"
    if O.HashesDeBin() != Hashes1 or len(Hashes1) < 4:
        return f"bytes de bin diferentes entre jobs 1 e 4: {Hashes1} x {O.HashesDeBin()}"
    return None if O.SaidaDoApp() == "36" else f"app imprimiu {O.SaidaDoApp()!r}"


def CasoFalhaInjetada(Ctx):
    O = ObraTres(Ctx)
    Injecao = {"FORJA_TESTE_INJETAR": "X0904", "FORJA_TESTE_INJETAR_PRODUTO": "d2"}
    R = O.Rodar("--jobs", "4", Ambiente=Injecao)
    Itens, P = LerResumo(R.Saida)
    Esperado = [("d1", "recompilado", "primeira vez"), ("d2", "falhou", 3), ("d3", "recompilado", "primeira vez"), ("app", "pulado", "d2")]
    if R.Rc != 3 or Itens != Esperado or P is None or (P["recompilados"], P["falharam"], P["pulados"]) != (2, 1, 1):
        return f"rc={R.Rc} {Itens} {P} stderr={R.Erro!r}"
    if not (O.Caminho("bin/d1.dll").is_file() and O.Caminho("bin/d3.dll").is_file()) or O.Caminho("bin/d2.dll").exists() or O.Caminho("bin/app.exe").exists():
        return f"saidas depois da falha: {sorted(O.HashesDeBin())}"
    R2 = O.Rodar()
    Itens2, _ = LerResumo(R2.Saida)
    Esperado2 = [("d1", "reusado", None), ("d2", "recompilado", "primeira vez"), ("d3", "reusado", None), ("app", "recompilado", "primeira vez")]
    return None if (R2.Rc == 0 and Itens2 == Esperado2) else f"sem injecao: rc={R2.Rc} {Itens2}"


def CasoErroDeUsuario(Ctx):
    O = ObraTres(Ctx)
    O.Escrever("d2.fj", "proc ( {\n")
    R = O.Rodar("--jobs", "2")
    Itens, _ = LerResumo(R.Saida)
    Esperado = [("d1", "recompilado", "primeira vez"), ("d2", "falhou", 1), ("d3", "recompilado", "primeira vez"), ("app", "pulado", "d2")]
    if R.Rc != 1 or Itens != Esperado:
        return f"rc={R.Rc} {Itens} stderr={R.Erro!r}"
    Avulso = subprocess.run([str(O.Forja), "build", "--backend", "x64", "--modo", "dll", str(O.Caminho("d2.fj")), "-o", str(O.Caminho("avulso.dll"))], capture_output=True, timeout=60)
    Cabecalho = b"forja: [d2] saida do build:"
    Posicao = R.ErroCru.find(Cabecalho)
    Esperado = Avulso.stdout + Avulso.stderr    # o bloco e o .out e depois o .err do filho, bytes crus (com o CRLF do stdio)
    if Avulso.returncode != 1 or not Esperado or Posicao < 0:
        return f"filho avulso rc={Avulso.returncode} {Esperado!r}; cabecalho em {Posicao}; stderr={R.ErroCru!r}"
    Bloco = R.ErroCru[Posicao + len(Cabecalho):].removeprefix(b"\r").removeprefix(b"\n")
    if Bloco != Esperado:
        return f"bloco {Bloco!r} != saida do filho avulso {Esperado!r}"
    Diagnostico = json.loads(Esperado.decode("utf-8").splitlines()[0])
    return None if Diagnostico.get("codigo") else f"JSON do filho sem codigo: {Diagnostico}"


def CasoB0006(Ctx):
    O = ObraLibApp(Ctx)
    R0 = O.Rodar()
    if R0.Rc != 0:
        return f"build inicial rc={R0.Rc} {R0.Erro!r}"
    O.Escrever("forja.proj", "saida = bin\nproduto app exe app.fj\nproduto lib dll lib.fj\n")
    R = O.Rodar()
    Itens, _ = LerResumo(R.Saida)
    Esperado = [("app", "falhou", 1), ("lib", "reusado", None)]
    if R.Rc != 1 or Itens != Esperado:
        return f"rc={R.Rc} {Itens} stderr={R.Erro!r}"
    Diagnosticos = [json.loads(L) for L in R.Erro.splitlines() if L.startswith("{")]
    Achou = [D for D in Diagnosticos if D.get("codigo") == "B0006"]
    if len(Achou) != 1 or "'app'" not in Achou[0]["causa"] or "'lib'" not in Achou[0]["causa"]:
        return f"B0006 ausente ou torto: {Diagnosticos}"
    return None


def CasoB0008(Ctx):
    O = Ctx.NovaObra("saida = bin\nproduto g dll g.fj\n", {"g.fj": FonteGrande("a")})
    Primeiro = subprocess.Popen([str(O.Forja), "build", "--projeto"], cwd=O.Pasta, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=AmbienteLimpo())
    try:
        if not EsperarAte(O.Caminho(".forja_cache/log/g.out").exists, Primeiro):
            return "o 1o orquestrador nao disparou o filho"
        R2 = O.Rodar()
        Rc1 = Primeiro.wait(timeout=TIMEOUT_ORQUESTRADOR)
    finally:
        if Primeiro.poll() is None:
            Primeiro.kill()
    Linhas = [L for L in R2.Erro.splitlines() if L.startswith("{")]
    Diagnostico = json.loads(Linhas[0]) if len(Linhas) == 1 else {}
    if R2.Rc != 2 or Diagnostico.get("codigo") != "B0008" or not Diagnostico.get("causa", "").endswith("projeto em build por outro processo"):
        return f"2o orquestrador: rc={R2.Rc} stdout={R2.Saida!r} stderr={R2.Erro!r}"
    return None if Rc1 == 0 and O.Caminho("bin/g.dll").is_file() else f"1o orquestrador: rc={Rc1}"


def CasoPaiMorto(Ctx):
    Imagem = "fp" + secrets.token_hex(4) + ".exe"
    Copia = Ctx.Base / Imagem
    shutil.copyfile(Ctx.Forja, Copia)
    Fontes = {f"d{N}.fj": FonteGrande(f"d{N}") for N in (1, 2, 3)}
    Fontes["app.fj"] = "proc main() {\n    print(36)\n}\n"
    O = Ctx.NovaObra(MANIFESTO_TRES, Fontes)
    Pai = subprocess.Popen([str(Copia), "build", "--projeto", "--jobs", "3"], cwd=O.Pasta, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=AmbienteLimpo())
    Logs = O.Caminho(".forja_cache/log")
    if not EsperarAte(lambda: Logs.is_dir() and len(list(Logs.glob("*.out"))) >= 3, Pai):
        Pai.kill()
        return "os 3 filhos nao foram disparados"
    Antes = ContarProcessos(Imagem)
    Pai.kill()  # TerminateProcess: nenhum tratador, so o job object salva os filhos
    Pai.wait()
    time.sleep(1.0)
    Depois = ContarProcessos(Imagem)
    if Depois != 0 or Antes < 2:
        subprocess.run(["taskkill", "/F", "/IM", Imagem], capture_output=True)
        return f"processos de {Imagem}: {Antes} com o pai vivo (esperado >= 2: pai e filho), {Depois} 1 s depois da morte do pai (esperado 0)"
    O.Escrever("bin/d1.dll.tmp" + str(Pai.pid), "lixo")
    O.Escrever("bin/d1.dll.velho" + str(Pai.pid), "lixo")
    O.Escrever(".forja_cache/estado/dev/d1.entradas.tmp" + str(Pai.pid), "lixo")
    R = O.Rodar("--jobs", "3", Forja=Copia)
    Itens, _ = LerResumo(R.Saida)
    if R.Rc != 0 or [I[1] for I in Itens] != ["recompilado"] * 4 or O.Sobras():
        return f"proximo build: rc={R.Rc} {Itens} sobras={O.Sobras()} stderr={R.Erro!r}"
    return None if O.SaidaDoApp() == "36" else f"app imprimiu {O.SaidaDoApp()!r}"


def CasoJobsInvalido(Ctx):
    O = ObraTres(Ctx)
    for Valor in ("0", "65", "abc"):
        R = O.Rodar("--jobs", Valor)
        if R.Rc != 2 or "--jobs" not in R.Erro or O.Caminho(".forja_cache").exists():
            return f"--jobs {Valor}: rc={R.Rc} stderr={R.Erro!r}"
    return None


def CasoForjaJobs(Ctx):
    O = ObraTres(Ctx)
    R = O.Rodar(Ambiente={"FORJA_JOBS": "2"})
    _, P = LerResumo(R.Saida)
    if R.Rc != 0 or P is None or P["jobs"] != 2:
        return f"FORJA_JOBS=2: rc={R.Rc} {P} {R.Erro!r}"
    _, P = LerResumo(O.Rodar("--jobs", "3", Ambiente={"FORJA_JOBS": "2"}).Saida)
    if P is None or P["jobs"] != 3:
        return f"--jobs 3 deveria vencer FORJA_JOBS=2: {P}"
    _, P = LerResumo(O.Rodar().Saida)
    if P is None or not 1 <= P["jobs"] <= 3:
        return f"jobs padrao fora de 1..3 (3 dlls e a camada mais larga): {P}"
    for Valor in ("0", "65"):
        Ruim = O.Rodar(Ambiente={"FORJA_JOBS": Valor})
        if Ruim.Rc != 2 or "FORJA_JOBS" not in Ruim.Erro:
            return f"FORJA_JOBS={Valor}: rc={Ruim.Rc} stderr={Ruim.Erro!r}"
    return None


def CasoJsonFilhos(Ctx):
    O = ObraTres(Ctx)
    R = O.Rodar("--json", "--jobs", "2")
    if R.Rc != 0 or R.Erro != "":
        return f"rc={R.Rc} stderr={R.Erro!r}"
    Linhas = [json.loads(L) for L in R.Saida.splitlines()]
    Produtos, Final = Linhas[:-1], Linhas[-1].get("projeto", {})
    Nomes = [P.get("produto") for P in Produtos]
    Tudo = all(P.get("acao") == "recompilado" and P.get("motivo") == "primeira vez" and P.get("exit") == 0 and isinstance(P.get("ms"), int) for P in Produtos)
    Contagens = (Final.get("produtos"), Final.get("recompilados"), Final.get("reusados"), Final.get("falharam"), Final.get("pulados"), Final.get("jobs"), Final.get("filhos"))
    if Nomes != ["d1", "d2", "d3", "app"] or not Tudo or Contagens != (4, 4, 0, 0, 0, 2, 4):
        return f"json {Linhas}"
    return None


def CasoNoOp(Ctx):
    O = ObraTres(Ctx)
    Primeiro = O.Rodar()
    if Primeiro.Rc != 0:
        return f"build inicial rc={Primeiro.Rc} {Primeiro.Erro!r}"
    R = O.Rodar()
    Itens, P = LerResumo(R.Saida)
    if R.Rc != 0 or R.Erro != "" or Itens != [(N, "reusado", None) for N in ("d1", "d2", "d3", "app")] or P is None or P["recompilados"] != 0:
        return f"no-op: rc={R.Rc} {Itens} {P} stderr={R.Erro!r}"
    Linhas = [json.loads(L) for L in O.Rodar("--json").Saida.splitlines()]
    Final = Linhas[-1].get("projeto", {})
    return None if (Final.get("filhos"), Final.get("reusados")) == (0, 4) else f"no-op json {Linhas}"


def CasoDllEmUso(Ctx):
    O = ObraLibApp(Ctx)
    if O.Rodar().Rc != 0:
        return "build inicial falhou"
    Antes = O.Caminho("bin/lib.dll").read_bytes()
    Ajudante = subprocess.Popen([sys.executable, "-I", "-c", CODIGO_DO_AJUDANTE, str(O.Caminho("bin/lib.dll"))], stdout=subprocess.PIPE)
    try:
        if Ajudante.stdout.readline().strip() != b"ok":
            return "o processo de teste nao conseguiu carregar a dll"
        O.Escrever("lib.fj", FONTE_LIB.replace("util.dobro(a) + b", "util.dobro(a) + b + 1"))
        R = O.Rodar()
        Velhos = sorted(A.name for A in O.Caminho("bin").glob("lib.dll.velho*"))
    finally:
        Ajudante.kill()
        Ajudante.wait()
    Itens, _ = LerResumo(R.Saida)
    if R.Rc != 0 or Itens != [("lib", "recompilado", "fonte mudou: lib.fj"), ("app", "reusado", None)]:
        return f"rc={R.Rc} {Itens} stderr={R.Erro!r}"
    if len(Velhos) != 1 or "saida em uso" not in R.Saida or Velhos[0] not in R.Saida:
        return f"antigo nao renomeado ou nao dito no resumo: {Velhos} {R.Saida!r}"
    if O.Caminho("bin/lib.dll").read_bytes() == Antes:
        return "a lib.dll nova nao foi gravada"
    if O.Rodar().Rc != 0 or O.Sobras():
        return f"a faxina do build seguinte nao apagou o antigo: {O.Sobras()}"
    return None


def CasoInterfaceReal(Ctx):
    O = ObraLibApp(Ctx)
    if O.Rodar().Rc != 0:
        return "build inicial falhou"
    O.Escrever("lib.fj", FONTE_LIB.replace("util.dobro(a) + b", "util.dobro(a) + b + 1"))
    Itens, _ = LerResumo(O.Rodar().Saida)
    if Itens != [("lib", "recompilado", "fonte mudou: lib.fj"), ("app", "reusado", None)] or O.SaidaDoApp() != "8":
        return f"so o corpo mudou: {Itens} app={O.SaidaDoApp()!r}"
    O.Anexar("lib.fj", "\npub export proc triplo(x: i32) -> i32 {\n    return x * 3\n}\n")
    Itens, _ = LerResumo(O.Rodar().Saida)
    Esperado = [("lib", "recompilado", "fonte mudou: lib.fj"), ("app", "recompilado", "interface de lib mudou")]
    return None if Itens == Esperado else f"a interface mudou: {Itens}"


ETAPAS_POR_EXECUCAO = ("ler_proj", "ordem", "trava", "pastas_faxina", "estado_ler", "id_forja", "resumo", "total")
ETAPAS_POR_PRODUTO = ("avaliar", "canais", "criar", "job", "retomar", "vida_do_filho", "latencia", "emitir_bloco", "declaradas", "linha_estado", "gravar_estado")


def DesvioDasLinhasDeTempo(Linhas):
    """None se cada linha tem etapa e us inteiro >= 0 e todas as etapas por execucao e por produto aparecem; senao o texto do desvio."""
    for Linha in Linhas:
        Us = Linha.get("us")
        if not isinstance(Linha.get("etapa"), str) or not isinstance(Us, int) or isinstance(Us, bool) or Us < 0:
            return f"linha sem etapa/us valido: {Linha}"
    for Etapa in ETAPAS_POR_EXECUCAO:
        Achadas = [L for L in Linhas if L["etapa"] == Etapa and "produto" not in L]
        if len(Achadas) != 1:
            return f"etapa por execucao '{Etapa}' apareceu {len(Achadas)} vez(es)"
    Trava = next(L for L in Linhas if L["etapa"] == "trava")
    if not isinstance(Trava.get("n"), int) or Trava["n"] < 0:
        return f"trava sem 'n': {Trava}"
    for Etapa in ETAPAS_POR_PRODUTO:
        Produtos = sorted(L["produto"] for L in Linhas if L["etapa"] == Etapa and "produto" in L)
        if Produtos != ["app", "d1", "d2", "d3"]:
            return f"etapa por produto '{Etapa}' em {Produtos}"
    if not any(L["etapa"] == "ocioso_do_slot" and "produto" in L for L in Linhas):
        return "nenhum ocioso_do_slot (com --jobs 1 o 2o filho em diante reocupa a vaga)"
    return None


def SemMs(Texto):
    """O resumo sem os milissegundos, que mudam de uma execucao para a outra."""
    return re.sub(r"\d+ ms", "N ms", Texto)


def CasoTemposOrquestrador(Ctx):
    O = ObraTres(Ctx)
    Arquivo = Ctx.Base / (O.Pasta.name + "_tempos.jsonl")
    Sem = O.Rodar("--jobs", "1")
    if Sem.Rc != 0 or Arquivo.exists() or list(O.Pasta.rglob("*.jsonl")):
        return f"sem a variavel: rc={Sem.Rc} arquivo={Arquivo.exists()} stderr={Sem.Erro!r}"
    O.Limpar()
    Com = O.Rodar("--jobs", "1", Ambiente={"FORJA_ORQ_TEMPOS": str(Arquivo)})
    if Com.Rc != 0 or SemMs(Com.Saida) != SemMs(Sem.Saida) or Com.Erro != Sem.Erro:
        return f"com a variavel a saida mudou: rc={Com.Rc} {Com.Saida!r} x {Sem.Saida!r} stderr={Com.Erro!r}"
    if not Arquivo.is_file():
        return "o arquivo de tempos nao foi criado"
    try:
        Linhas = [json.loads(L) for L in Arquivo.read_text(encoding="utf-8").splitlines()]
    except ValueError as Erro:
        return f"linha que nao e JSON: {Erro}"
    return DesvioDasLinhasDeTempo(Linhas)


ETAPAS_COM_INSTANTE = ETAPAS_POR_EXECUCAO + ("avaliar", "vida_do_filho", "emitir_bloco", "declaradas", "linha_estado", "gravar_estado", "ocioso_do_slot")
FOLGA_US = 1000  # relogios diferentes (QPC das etapas, FILETIME da vida dos filhos): 1 ms de folga nas comparacoes de ordem


def DesvioDosInstantes(Linhas):
    """None se toda linha tem ini_us inteiro (>= -1), as etapas de ETAPAS_COM_INSTANTE e as vidas tem ini_us >= 0 e <= total, o total comeca em 0 e o app nasce depois das 3 dlls
    terminarem; senao o texto do desvio."""
    for Linha in Linhas:
        Ini = Linha.get("ini_us")
        if not isinstance(Ini, int) or isinstance(Ini, bool) or Ini < -1:
            return f"linha sem ini_us valido: {Linha}"
        if Linha["etapa"] in ETAPAS_COM_INSTANTE and Ini < 0:
            return f"etapa {Linha['etapa']} sem instante de inicio: {Linha}"
    Total = next(L for L in Linhas if L["etapa"] == "total")
    if Total["ini_us"] != 0:
        return f"total deveria comecar em ini_us 0: {Total}"
    Vidas = {L["produto"]: L for L in Linhas if L["etapa"] == "vida_do_filho"}
    for Produto, Vida in Vidas.items():
        if Vida["ini_us"] > Total["us"]:
            return f"vida de {Produto} comeca depois do fim do orquestrador: {Vida} total {Total['us']}"
    for Dll in ("d1", "d2", "d3"):
        if Vidas["app"]["ini_us"] + FOLGA_US < Vidas[Dll]["ini_us"] + Vidas[Dll]["us"]:
            return f"o app nasceu antes de {Dll} terminar: app {Vidas['app']} {Dll} {Vidas[Dll]}"
    return None


def CasoInstantesDosTempos(Ctx):
    O = ObraTres(Ctx)
    Arquivo = Ctx.Base / (O.Pasta.name + "_instantes.jsonl")
    Rodada = O.Rodar("--jobs", "2", Ambiente={"FORJA_ORQ_TEMPOS": str(Arquivo)})
    if Rodada.Rc != 0 or not Arquivo.is_file():
        return f"rc={Rodada.Rc} arquivo={Arquivo.is_file()} stderr={Rodada.Erro!r}"
    try:
        Linhas = [json.loads(L) for L in Arquivo.read_text(encoding="utf-8").splitlines()]
    except ValueError as Erro:
        return f"linha que nao e JSON: {Erro}"
    return DesvioDosInstantes(Linhas)


PASTA_ORDEM_STDOUT = Path(__file__).resolve().parent / "ordem_stdout"
FONTES_ORDEM_STDOUT = ("d1.fj", "d2.fj", "d3.fj", "app.fj")


def EsperadoDaOrdem(Fontes):
    """esperado.txt com o marcador @LONGA@ trocado pela linha longa do d2.fj, em CRLF (o stdout de arquivo ou pipe do Windows e modo texto)."""
    Longa = re.search(r'print\("([0-9a-f]+)"\)', Fontes["d2.fj"]).group(1)
    if len(Longa) <= 4096:
        raise ValueError(f"a linha longa do d2.fj tem {len(Longa)} bytes (precisa de mais de 4096)")
    Texto = (PASTA_ORDEM_STDOUT / "esperado.txt").read_text(encoding="utf-8").replace("@LONGA@", Longa)
    return Texto.replace("\n", "\r\n").encode("ascii")


def SaidasDoExe(Exe):
    """(stdout para ARQUIVO, stdout para PIPE) do exe, em bytes, rodando na pasta dele."""
    Arquivo = Exe.parent / "saida_arquivo.bin"
    with open(Arquivo, "wb") as Destino:
        subprocess.run([str(Exe)], cwd=Exe.parent, stdout=Destino, timeout=60, check=True)
    Pipe = subprocess.run([str(Exe)], cwd=Exe.parent, stdout=subprocess.PIPE, timeout=60, check=True).stdout
    return Arquivo.read_bytes(), Pipe


def CasoOrdemDoStdout(Ctx):
    Fontes = {Nome: (PASTA_ORDEM_STDOUT / Nome).read_text(encoding="utf-8") for Nome in FONTES_ORDEM_STDOUT}
    Esperado = EsperadoDaOrdem(Fontes)
    O = Ctx.NovaObra((PASTA_ORDEM_STDOUT / "forja.proj").read_text(encoding="utf-8"), Fontes)
    if O.Rodar().Rc != 0:
        return "build do projeto (3 dlls + exe) falhou"
    X64Arquivo, X64Pipe = SaidasDoExe(O.Caminho("bin/app.exe"))
    for Rotulo, Saida in (("x64 em arquivo", X64Arquivo), ("x64 em pipe", X64Pipe)):
        if Saida != Esperado:
            return f"{Rotulo}: {len(Saida)} bytes diferem do esperado ({len(Esperado)} bytes) no byte {next((K for K, (A, B) in enumerate(zip(Saida, Esperado)) if A != B), min(len(Saida), len(Esperado)))}"
    return None


def CasoEstadoTorto(Ctx):
    """O estado e gravado sem descarga em disco: depois de uma queda ele pode estar truncado no meio de uma linha, vazio ou com lixo; o build so recompila, nunca quebra."""
    O = ObraTres(Ctx)
    if O.Rodar().Rc != 0:
        return "build inicial falhou"
    Saida = O.SaidaDoApp()
    Arquivo = O.Caminho(".forja_cache/estado/dev.tsv")
    Original = Arquivo.read_bytes()
    for Rotulo, Torto in (("truncado no meio da ultima linha", Original[:-20]), ("vazio", b""), ("lixo binario", b"\x00" * len(Original))):
        Arquivo.write_bytes(Torto)
        R = O.Rodar()
        Itens, P = LerResumo(R.Saida)
        if R.Rc != 0 or R.Erro != "" or P is None or P["falharam"] != 0 or len(Itens) != 4 or O.SaidaDoApp() != Saida:
            return f"estado {Rotulo}: rc={R.Rc} {Itens} {P} stderr={R.Erro!r}"
        if Rotulo != "truncado no meio da ultima linha" and P["recompilados"] != 4:
            return f"estado {Rotulo}: esperava recompilar os 4 produtos, recompilou {P['recompilados']}"
    R = O.Rodar()
    Itens, P = LerResumo(R.Saida)
    return None if (R.Rc == 0 and P is not None and P["recompilados"] == 0) else f"depois do estado refeito o no-op falhou: {Itens} {P}"


CASOS = (
    ("primeira vez", CasoPrimeiraVez),
    ("nada mudou", CasoNadaMudou),
    ("fonte mudou", CasoFonteMudou),
    ("modulo importado mudou", CasoModuloImportadoMudou),
    (".glb criado (reusado)", CasoGlbCriado),
    ("forja.exe diferente", CasoForjaDiferente),
    ("--release separado de dev", CasoReleaseSeparado),
    ("interface mudou", CasoInterfaceMudou),
    ("so corpo mudou", CasoSoCorpoMudou),
    ("saida apagada", CasoSaidaApagada),
    ("--tudo", CasoTudo),
    ("B0005 nome repetido", CasoB0005NomeRepetido),
    ("B0005 entrada ausente", CasoB0005EntradaAusente),
    ("B0007 ciclo", CasoB0007Ciclo),
    ("3 dlls + exe com jobs 1 e 4 (bytes iguais)", CasoJobsUmEQuatro),
    ("falha injetada isola o produto", CasoFalhaInjetada),
    ("erro de usuario: JSON do filho intacto", CasoErroDeUsuario),
    ("B0006 dependencia nao declarada", CasoB0006),
    ("B0008 dois orquestradores", CasoB0008),
    ("pai morto: nenhum filho vivo", CasoPaiMorto),
    ("--jobs 0, 65 e abc = uso errado", CasoJobsInvalido),
    ("FORJA_JOBS e --jobs", CasoForjaJobs),
    ("--json com filhos", CasoJsonFilhos),
    ("no-op: filhos 0", CasoNoOp),
    ("DLL em uso: renomeia o antigo", CasoDllEmUso),
    ("hash_interface real na execucao", CasoInterfaceReal),
    ("FORJA_ORQ_TEMPOS: uma linha JSON por etapa", CasoTemposOrquestrador),
    ("FORJA_ORQ_TEMPOS: ini_us em toda linha de etapa e de vida", CasoInstantesDosTempos),
    ("stdout em ordem: 3 dlls + exe, arquivo e pipe, x64 = esperado fixo", CasoOrdemDoStdout),
    ("estado torto/vazio/lixo: so recompila", CasoEstadoTorto),
)


class Contexto:
    def __init__(Self, Forja, Base):
        Self.Forja = Forja
        Self.Base = Base

    def Nova(Self):
        return Bancada(Self.Forja, Self.Base)

    def NovaObra(Self, Manifesto, Fontes):
        return Obra(Self.Forja, Self.Base, Manifesto, Fontes)


def Principal():
    Analisador = argparse.ArgumentParser(description="Verifica o --plano e a execucao do orquestrador de projeto (30 casos).")
    Analisador.add_argument("--forja", required=True, help="caminho do forja.exe (use uma copia)")
    Argumentos = Analisador.parse_args()
    Forja = Path(os.path.abspath(Argumentos.forja))
    if not Forja.is_file():
        print(f"forja nao encontrado: {Forja}", file=sys.stderr)
        return 2
    assert len(CASOS) == TOTAL_DE_CASOS
    Base = Path(tempfile.mkdtemp(prefix="forja_projeto_"))
    Ctx = Contexto(Forja, Base)
    Passaram = 0
    for Nome, Caso in CASOS:
        try:
            Desvio = Caso(Ctx)
        except Exception as Erro:  # um caso que explode e um caso que falhou
            Desvio = f"excecao {type(Erro).__name__}: {Erro}"
        print(("OK     " if Desvio is None else "FALHOU ") + Nome + ("" if Desvio is None else " :: " + Desvio))
        Passaram += Desvio is None
    shutil.rmtree(Base, ignore_errors=True)
    print(f"{Passaram}/{TOTAL_DE_CASOS} casos de projeto")
    return 0 if Passaram == TOTAL_DE_CASOS else 1


if __name__ == "__main__":
    sys.exit(Principal())
