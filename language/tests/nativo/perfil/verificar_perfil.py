#!/usr/bin/env python3
# verificar_perfil.py 1.0.0 - (1.0.0: M8 P3, prova do --perfil com 9 casos sobre tests\nativo\perfil\perfil_*.fj: contagens vindas dos .esperado (nunca tempos); aninhamento, 2 fios, panic e os.exit com os 3 quadros fechados; release com e sem inline; teto de 1.048.576 eventos; formato do JSON; ELF no WSL (PULADA sem a distro); custo de fib_rec em release so imprime)
# Uso: python -I tests\nativo\perfil\verificar_perfil.py [--forja <forja.exe>] [--selftest]
# Saida 0 = tudo certo; 1 = algum caso falhou; 2 = forja ausente. Escrita so em pasta TEMP propria (mkdtemp, apagada no fim); sem internet.
# Verificacao: python -I tests\nativo\perfil\verificar_perfil.py --selftest (conferidores com dados sinteticos) e depois a suite inteira.
import argparse
import json
import os
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
import types
from pathlib import Path

VERSAO = "1.0.0"
PastaPerfil = Path(__file__).resolve().parent
PastaLingua = PastaPerfil.parents[2]
ForjaPadrao = PastaLingua / "compiler" / "forja.exe"
PastaLinux = PastaLingua / "tests" / "nativo" / "linux"
sys.path.insert(0, str(PastaLinux))
import wsl_lote as Wsl  # noqa: E402

TETO_EVENTOS = 1048576
TOLERANCIA_US = 0.002  # ts e dur saem com 3 casas em microssegundos
TOLERANCIA_MS = 0.001  # total_ms e proprio_ms do .txt tambem saem com 3 casas
MEDICOES_CUSTO = 3
ELF_NO_LINUX = "/tmp/perfil_m8_elf"
ASSINATURA_INLINE = "inline: procs inlinadas nao aparecem"
RE_LINHA_TXT = re.compile(r"^\s*(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+(\S+)\s*$")
RE_EVENTOS_TXT = re.compile(r"^eventos no json: (\d+)", re.M)
RE_TS_DUR = re.compile(r'"(ts|dur)":([\d.]+)')


def Executar(Comando, Ambiente=None, Limite=300):
    return subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", env=Ambiente, timeout=Limite)


def Compilar(Forja, Fonte, Exe, Opcoes=()):
    return Executar([str(Forja), "build", *Opcoes, str(Fonte), "-o", str(Exe)])


def Rodar(Exe, Saida=None):
    Ambiente = dict(os.environ)
    Ambiente.pop("FORJA_PERFIL_SAIDA", None)
    if Saida is not None:
        Ambiente["FORJA_PERFIL_SAIDA"] = str(Saida)
    return Executar([str(Exe)], Ambiente, Limite=120)


def ArquivosDoPerfil(Base):
    """Base e o exe (gera <exe>.perfil.*) ou a base sem extensao de FORJA_PERFIL_SAIDA (gera <base>.perfil.*)."""
    return Base.with_name(Base.name + ".perfil.txt"), Base.with_name(Base.name + ".perfil.json")


def LerEsperado(Nome):
    Esperado = {}
    for Linha in (PastaPerfil / (Nome + ".esperado")).read_text(encoding="utf-8").splitlines():
        if Linha.strip():
            Proc, Qtd = Linha.split()
            Esperado[Proc] = int(Qtd)
    return Esperado


def ParsearTxt(Texto):
    Linhas = {}
    for Linha in Texto.splitlines():
        Casa = RE_LINHA_TXT.match(Linha)
        if Casa:
            Linhas[Casa.group(5)] = {"chamadas": int(Casa.group(1)), "total": float(Casa.group(2)), "proprio": float(Casa.group(3))}
    Eventos = RE_EVENTOS_TXT.search(Texto)
    return {"linhas": Linhas, "eventos": int(Eventos.group(1)) if Eventos else None, "texto": Texto}


def CarregarJson(Caminho):
    with open(Caminho, encoding="utf-8") as Arq:
        return json.load(Arq)


def LerPerfil(Base):
    """Devolve (txt parseado, json, problemas). Sem os 2 arquivos o caso falha sem olhar o resto."""
    Txt, Json = ArquivosDoPerfil(Base)
    if not Txt.exists() or not Json.exists():
        return None, None, [f"arquivos do perfil ausentes ({Txt.name}, {Json.name})"]
    return ParsearTxt(Txt.read_text(encoding="utf-8")), CarregarJson(Json), []


def ContarJson(Dados):
    Contagem = {}
    for Evento in Dados["traceEvents"]:
        Contagem[Evento["name"]] = Contagem.get(Evento["name"], 0) + 1
    return Contagem


def EventosDe(Dados, Nome):
    return [Evento for Evento in Dados["traceEvents"] if Evento["name"] == Nome]


def ConfereContagens(Linhas, Esperado):
    Problemas = []
    for Proc, Qtd in Esperado.items():
        Obtido = Linhas.get(Proc, {}).get("chamadas", 0)
        if Obtido != Qtd:
            Problemas.append(f"{Proc}: {Obtido} chamadas, esperado {Qtd}")
    return Problemas


def ConfereJsonContaTxt(Dados, Linhas, Nomes):
    Contagem = ContarJson(Dados)
    Problemas = []
    for Nome in Nomes:
        Json = Contagem.get(Nome, 0)
        Txt = Linhas.get(Nome, {}).get("chamadas", 0)
        if Json != Txt:
            Problemas.append(f"{Nome}: json tem {Json} eventos, txt diz {Txt}")
    return Problemas


def ConfereSaida(Proc, Codigo, Stdout=None):
    Problemas = []
    if Proc.returncode != Codigo:
        Problemas.append(f"exit {Proc.returncode}, esperado {Codigo}")
    if Stdout is not None and Proc.stdout.strip() != Stdout:
        Problemas.append(f"stdout {Proc.stdout.strip()[:80]!r}, esperado {Stdout!r}")
    return Problemas


def ConfereRelacoes(Linhas, Cadeia):
    """Em toda linha: proprio <= total. Na cadeia externa -> interna: total externo >= total interno."""
    Problemas = []
    for Proc, Linha in Linhas.items():
        if Linha["proprio"] > Linha["total"] + TOLERANCIA_MS:
            Problemas.append(f"{Proc}: proprio {Linha['proprio']} > total {Linha['total']}")
    for Externo, Interno in zip(Cadeia, Cadeia[1:]):
        if Externo not in Linhas or Interno not in Linhas:
            Problemas.append(f"{Externo} ou {Interno} ausente no resumo")
        elif Linhas[Externo]["total"] + TOLERANCIA_MS < Linhas[Interno]["total"]:
            Problemas.append(f"total de {Externo} menor que o de {Interno}")
    return Problemas


def Dentro(Filho, Pai):
    """Filho cabe no intervalo do pai e no mesmo fio."""
    return (Filho["tid"] == Pai["tid"]
            and Filho["ts"] >= Pai["ts"] - TOLERANCIA_US
            and Filho["ts"] + Filho["dur"] <= Pai["ts"] + Pai["dur"] + TOLERANCIA_US)


def CadeiaAninhada(Dados, Cadeia):
    """Cada proc da cadeia (externa -> interna) tem um evento dentro de um evento da anterior, no mesmo fio."""
    Problemas = []
    for Pai, Filho in zip(Cadeia, Cadeia[1:]):
        if not any(Dentro(F, P) for F in EventosDe(Dados, Filho) for P in EventosDe(Dados, Pai)):
            Problemas.append(f"{Filho} nao esta dentro de {Pai}")
    return Problemas


def ConfereFios(Dados):
    Pais = EventosDe(Dados, "pai")
    Tids = {P["tid"] for P in Pais}
    Problemas = [] if len(Tids) == 2 else [f"pai em {len(Tids)} fios, esperado 2"]
    Fora = [F for F in EventosDe(Dados, "filho") if not any(Dentro(F, P) for P in Pais)]
    if Fora:
        Problemas.append(f"{len(Fora)} eventos de filho fora do pai no mesmo fio")
    return Problemas


def ConfereAninhamento(Eventos):
    """Por fio, dois intervalos ou se separam ou um cabe no outro (pilha por inicio)."""
    Porfio = {}
    for Evento in Eventos:
        Porfio.setdefault(Evento["tid"], []).append(Evento)
    for Tid, Lista in Porfio.items():
        Lista.sort(key=lambda Evento: (Evento["ts"], -Evento["dur"]))
        Pilha = []
        for Evento in Lista:
            while Pilha and Pilha[-1]["ts"] + Pilha[-1]["dur"] <= Evento["ts"] + TOLERANCIA_US:
                Pilha.pop()
            if Pilha and Evento["ts"] + Evento["dur"] > Pilha[-1]["ts"] + Pilha[-1]["dur"] + TOLERANCIA_US:
                return [f"fio {Tid}: {Evento['name']} cruza {Pilha[-1]['name']}"]
            Pilha.append(Evento)
    return []


def ConfereFormatoJson(Dados, Texto):
    Problemas = []
    if set(Dados) != {"traceEvents", "displayTimeUnit", "otherData"}:
        Problemas.append(f"chaves de topo {sorted(Dados)}")
    if Dados.get("displayTimeUnit") != "ns":
        Problemas.append("displayTimeUnit diferente de ns")
    Meta = Dados.get("otherData", {})
    for Chave in ("forja", "truncado", "eventos"):
        if Chave not in Meta:
            Problemas.append(f"otherData sem {Chave}")
    Eventos = Dados["traceEvents"]
    for Evento in Eventos:
        if Evento.get("ph") != "X" or Evento.get("pid") != 1 or not isinstance(Evento.get("tid"), int) or not isinstance(Evento.get("name"), str) or Evento.get("dur", -1) < 0:
            Problemas.append(f"evento fora do formato: {Evento}")
            break
    if Meta.get("eventos") != len(Eventos):
        Problemas.append(f"otherData.eventos {Meta.get('eventos')} != {len(Eventos)} eventos")
    if any(not re.fullmatch(r"\d+\.\d{3}", Valor) for _, Valor in RE_TS_DUR.findall(Texto)):
        Problemas.append("ts ou dur sem exatamente 3 casas decimais")
    Problemas += ConfereAninhamento(Eventos)
    return Problemas


def OrdemDoArquivo(Dados):
    """Informativo: ts crescente por fio na ordem do arquivo? O runtime grava na ordem de saida (quem fecha antes sai antes)."""
    Ultimo = {}
    for Evento in Dados["traceEvents"]:
        if Evento["ts"] < Ultimo.get(Evento["tid"], -1.0):
            return False
        Ultimo[Evento["tid"]] = Evento["ts"]
    return True


def ConstruirERodar(Forja, Pasta, Nome, Opcoes=(), Sufixo=""):
    Exe = Pasta / (Nome + Sufixo + ".exe")
    Build = Compilar(Forja, PastaPerfil / (Nome + ".fj"), Exe, Opcoes)
    if Build.returncode != 0:
        return None, None, "build falhou: " + (Build.stdout + Build.stderr).strip()[:300]
    return Exe, Rodar(Exe), None


def Veredito(Problemas, Sucesso):
    if Problemas:
        return "falha", "; ".join(Problemas)
    return "ok", Sucesso


def CasoFib(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_fib", ["--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, Json, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Problemas = ConfereSaida(Proc, 0, "6765") + ConfereContagens(Txt["linhas"], LerEsperado("perfil_fib"))
    Problemas += ConfereJsonContaTxt(Json, Txt["linhas"], ["fib", "main"])
    return Veredito(Problemas, "fib(20): 21891 chamadas de fib; json e txt batem")


def CasoCadeia(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_cadeia", ["--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, _, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Problemas = ConfereSaida(Proc, 0, "40") + ConfereContagens(Txt["linhas"], LerEsperado("perfil_cadeia"))
    Problemas += ConfereRelacoes(Txt["linhas"], ["a", "b", "c"])
    return Veredito(Problemas, "a > b > c com laco: totais decrescentes, proprio <= total")


def CasoFios(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_fios", ["--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, Json, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Problemas = ConfereSaida(Proc, 0, "fios ok") + ConfereContagens(Txt["linhas"], LerEsperado("perfil_fios"))
    Problemas += ConfereFios(Json)
    return Veredito(Problemas, "2 std:thread: 2 fios distintos; cada filho dentro do pai no mesmo fio")


def CasoPanico(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_panico", ["--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, Json, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Problemas = ConfereSaida(Proc, 101)
    if "R0003" not in Proc.stderr:
        Problemas.append("stderr sem o relatorio R0003")
    Problemas += ConfereContagens(Txt["linhas"], LerEsperado("perfil_panico"))
    Problemas += CadeiaAninhada(Json, ["main", "a", "b", "c"])
    if Json["otherData"].get("truncado") is not False:
        Problemas.append("truncado diferente de false")
    return Veredito(Problemas, "panic em c: exit 101, 3 quadros fechados, 2 arquivos gravados")


def CasoSaida(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_saida", ["--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, Json, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Problemas = ConfereSaida(Proc, 3) + ConfereContagens(Txt["linhas"], LerEsperado("perfil_saida"))
    Problemas += CadeiaAninhada(Json, ["main", "a", "b", "c"])
    return Veredito(Problemas, "os.exit(3) em c: exit 3, 3 quadros fechados, 2 arquivos gravados")


def CasoRelease(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_release", ["--release", "--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, _, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Problemas = ConfereSaida(Proc, 0, "1000\n332833500")
    if "pequena" in Txt["linhas"]:
        Problemas.append("pequena apareceu em release sem --sem-otim inline")
    if ASSINATURA_INLINE not in Txt["texto"]:
        Problemas.append("cabecalho sem a frase de inline")
    Problemas += ConfereContagens(Txt["linhas"], {"main": 1})
    Exe2, Proc2, Erro2 = ConstruirERodar(Forja, Pasta, "perfil_release", ["--release", "--perfil", "--sem-otim", "inline"], "_sem")
    if Erro2:
        return Veredito(Problemas + [Erro2], "")
    Txt2, _, Problemas2 = LerPerfil(Exe2)
    if Problemas2:
        return Veredito(Problemas + Problemas2, "")
    Problemas += ConfereSaida(Proc2, 0, "1000\n332833500") + ConfereContagens(Txt2["linhas"], LerEsperado("perfil_release"))
    return Veredito(Problemas, "release: pequena some com inline e volta com --sem-otim inline (1000)")


def CasoTeto(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_teto", ["--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, Json, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Meta = Json["otherData"]
    Problemas = ConfereSaida(Proc, 0, "2000000") + ConfereContagens(Txt["linhas"], LerEsperado("perfil_teto"))
    if Meta.get("truncado") is not True:
        Problemas.append("json sem truncado true")
    if Meta.get("eventos") != TETO_EVENTOS or len(Json["traceEvents"]) != TETO_EVENTOS:
        Problemas.append(f"json com {len(Json['traceEvents'])} eventos, esperado {TETO_EVENTOS}")
    if Txt["eventos"] != TETO_EVENTOS:
        Problemas.append(f"txt diz {Txt['eventos']} eventos no json, esperado {TETO_EVENTOS}")
    return Veredito(Problemas, "2 M de chamadas: json truncado em 1.048.576; agregado completo")


def CasoFormatoJson(Forja, Pasta):
    Exe, Proc, Erro = ConstruirERodar(Forja, Pasta, "perfil_fib", ["--perfil"])
    if Erro:
        return Veredito([Erro], "")
    Txt, Json, Problemas = LerPerfil(Exe)
    if Problemas:
        return Veredito(Problemas, "")
    Texto = ArquivosDoPerfil(Exe)[1].read_text(encoding="utf-8")
    Problemas = ConfereFormatoJson(Json, Texto)
    Sucesso = "formato Chrome Trace ok; ts por fio " + ("crescente no arquivo" if OrdemDoArquivo(Json) else "NAO crescente no arquivo (ordem de saida)")
    return Veredito(Problemas, Sucesso)


def ScriptElfLinux(Elf, Base):
    """Copia o ELF para /tmp da distro (o drvfs nao da +x), roda com a base do perfil e apaga a copia. Caminhos entram por shlex.quote: o wsl.exe nao repassa argumentos posicionais."""
    Elf_Wsl = shlex.quote(Wsl.CaminhoWsl(Elf))
    Base_Wsl = shlex.quote(Wsl.CaminhoWsl(Base))
    return f"cp {Elf_Wsl} {ELF_NO_LINUX} && chmod +x {ELF_NO_LINUX} && FORJA_PERFIL_SAIDA={Base_Wsl} {ELF_NO_LINUX}; RC=$?; rm -f {ELF_NO_LINUX}; exit $RC"


def CasoLinux(Forja, Pasta):
    if not Wsl.DistroDisponivel():
        return "pulada", "PULADA (WSL/distro ausente)"
    Elf = Pasta / "perfil_fib_linux.elf"
    Build = Compilar(Forja, PastaPerfil / "perfil_fib.fj", Elf, ["--target", "x86_64-linux", "--perfil"])
    if Build.returncode != 0:
        return Veredito(["build linux falhou: " + (Build.stdout + Build.stderr).strip()[:300]], "")
    Base = Pasta / "linux_fib"
    Roda = Wsl.Wsl(["sh", "-c", ScriptElfLinux(Elf, Base)], 120)
    Proc = types.SimpleNamespace(returncode=Roda.returncode, stdout=Wsl.TextoDoWsl(Roda.stdout), stderr="")
    Txt, Json, Problemas = LerPerfil(Base)
    if Problemas:
        return Veredito(Problemas, "")
    Problemas = ConfereSaida(Proc, 0, "6765") + ConfereContagens(Txt["linhas"], LerEsperado("perfil_fib"))
    Problemas += ConfereJsonContaTxt(Json, Txt["linhas"], ["fib", "main"])
    return Veredito(Problemas, "ELF no WSL alpine-build grava os mesmos 2 arquivos; contagens iguais")


CASOS = [
    ("fib", CasoFib),
    ("cadeia", CasoCadeia),
    ("fios", CasoFios),
    ("panico", CasoPanico),
    ("saida", CasoSaida),
    ("release", CasoRelease),
    ("teto", CasoTeto),
    ("formato_json", CasoFormatoJson),
    ("linux", CasoLinux),
]


def RodarUmCaso(Forja, Pasta, Funcao):
    try:
        return Funcao(Forja, Pasta)
    except (subprocess.TimeoutExpired, OSError, KeyError, ValueError, json.JSONDecodeError) as Erro:
        return "falha", f"{type(Erro).__name__}: {Erro}"


def MedirCusto(Forja, Pasta):
    """Informativo, sem portao: fib_rec(30) em release com e sem --perfil (mediana)."""
    Exes = {"sem": Pasta / "custo_sem.exe", "com": Pasta / "custo_com.exe"}
    Opcoes = {"sem": ["--release"], "com": ["--release", "--perfil"]}
    for Rotulo, Exe in Exes.items():
        Build = Compilar(Forja, PastaPerfil / "perfil_custo.fj", Exe, Opcoes[Rotulo])
        if Build.returncode != 0:
            print(f"custo: build {Rotulo} falhou (so medicao, nao reprova)")
            return
    Medias = {}
    for Rotulo, Exe in Exes.items():
        Amostras = []
        for _ in range(MEDICOES_CUSTO):
            Inicio = time.perf_counter()
            Rodar(Exe)
            Amostras.append((time.perf_counter() - Inicio) * 1000.0)
        Medias[Rotulo] = statistics.median(Amostras)
    Razao = Medias["com"] / Medias["sem"] if Medias["sem"] > 0 else float("nan")
    print(f"custo fib_rec(30) release: sem --perfil {Medias['sem']:.1f} ms | com --perfil {Medias['com']:.1f} ms | razao {Razao:.2f}x (mediana de {MEDICOES_CUSTO}; so medicao)")


def RodarCasos(Forja, Raiz):
    Estados = []
    for Numero, (Nome, Funcao) in enumerate(CASOS, start=1):
        Pasta = Raiz / f"caso{Numero}"
        Pasta.mkdir()
        Estado, Detalhe = RodarUmCaso(Forja, Pasta, Funcao)
        print(f"caso {Numero} {Nome}: {Estado.upper()} - {Detalhe}")
        Estados.append(Estado)
    MedirCusto(Forja, Raiz)
    Falhas = Estados.count("falha")
    Executados = len(Estados) - Estados.count("pulada")
    if Falhas:
        print(f"RESULTADO: FALHOU ({Falhas} de {Executados} casos)")
        return 1
    print(f"RESULTADO: tudo certo ({Executados}/{Executados} casos)")
    return 0


def Selftest():
    Problemas = []
    Eventos = [
        {"name": "a", "ph": "X", "ts": 1.0, "dur": 10.0, "pid": 1, "tid": 1},
        {"name": "b", "ph": "X", "ts": 2.0, "dur": 3.0, "pid": 1, "tid": 1},
        {"name": "c", "ph": "X", "ts": 4.0, "dur": 20.0, "pid": 1, "tid": 1},
    ]
    Dados = {"traceEvents": Eventos}
    if CadeiaAninhada(Dados, ["a", "b"]):
        Problemas.append("a>b devia passar")
    if not CadeiaAninhada(Dados, ["a", "c"]):
        Problemas.append("a>c devia falhar (c passa do fim de a)")
    if not ConfereAninhamento(Eventos):
        Problemas.append("cruzamento a/c devia ser pego")
    Txt = ParsearTxt("perfil forja X\neventos no json: 4\n    chamadas      total_ms    proprio_ms  %proprio  nome\n       3   1.000   0.500   50.0  fib\n")
    if Txt["linhas"].get("fib", {}).get("chamadas") != 3 or Txt["eventos"] != 4:
        Problemas.append("ParsearTxt nao leu a linha ou os eventos")
    if not ConfereContagens({"fib": {"chamadas": 3}}, {"fib": 2}):
        Problemas.append("contagem errada devia falhar")
    if not ConfereRelacoes({"a": {"total": 1.0, "proprio": 2.0}}, ["a"]):
        Problemas.append("proprio > total devia falhar")
    if Problemas:
        print("selftest: FALHOU - " + "; ".join(Problemas))
        return 1
    print("selftest: ok (6 verificacoes)")
    return 0


def Principal():
    Analisador = argparse.ArgumentParser(description="Prova do --perfil (M8 P3).")
    Analisador.add_argument("--forja", default=str(ForjaPadrao))
    Analisador.add_argument("--selftest", action="store_true")
    Args = Analisador.parse_args()
    if Args.selftest:
        return Selftest()
    Forja = Path(Args.forja)
    if not Forja.exists():
        print(f"forja ausente: {Forja}")
        return 2
    Raiz = Path(tempfile.mkdtemp(prefix="perfil_m8_"))
    try:
        return RodarCasos(Forja, Raiz)
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Principal())
