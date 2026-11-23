#!/usr/bin/env python3
# verificar_reload.py 1.0.0 - (1.0.0: M8 H3, prova do hot reload do std:reload com o host de tests\reload\host.fj: cenarios 1 a 7 e o exemplo exemplos_libs\hot_reload; roda em dev e em release; o cenario 6 (jogo que vaza) so em dev; o cenario 7 mede e registra, sem portao de tempo)
# Uso: python -I tests\reload\verificar_reload.py [--forja <forja.exe>] [--modo dev|release|ambos]
# Saida 0 = tudo certo; 1 = algum cenario falhou. Testes de DLL so em pasta TEMP propria (mkdtemp, T2); sem internet.
# Mecanica: o host roda QUADROS_HOST quadros de 10 ms; o verificador recompila a DLL no caminho do host, cria a marca e espera o host consumi-la (o host a remove no quadro em que a viu).
import argparse
import re
import shutil
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

VERSAO = "1.0.0"
PastaReload = Path(__file__).resolve().parent
PastaLingua = PastaReload.parent.parent
ForjaPadrao = PastaLingua / "compiler" / "forja.exe"
PastaExemplo = PastaLingua / "exemplos_libs" / "hot_reload"
QUADROS_HOST = 700  # cada build leva ~1 s; o host tem de viver bem alem da ultima troca
PRAZO_MARCA_S = 60
PRAZO_HOST_S = 240
TETO_CARGA_S = 15
AQUECIMENTO_S = 0.5
TROCAS_MEDIDAS = 10
MIB = 1024 * 1024
RE_QUADRO = re.compile(r"^q=(\d+) gen=(\d+) passos=(-?\d+)")
RE_TROCA = re.compile(r"^troca gen=(\d+) q=(\d+) ms=(\d+)")
RE_MARCA = re.compile(r"^marca q=(\d+)")
RE_RECUSA = re.compile(r"^recusada gen=(\d+) q=(\d+)")
RE_ASYNC = re.compile(r"^async troca_ok=(\w+) gen=(\d+) espera_ms=(\d+) quadros=([\d,]+) sem_queda=(\w+)")
RE_RODIZIO = re.compile(r"^rodizio trocas=(\d+) falhas=(\d+) pico100=(\d+) picofim=(\d+) quente=(-?\d+) gen=(\d+)")
RE_VAZAMENTO = re.compile(r"vazamento: (\d+) blocos")


class Resultado:
    def __init__(self, Ok, Detalhe):
        self.Ok = Ok
        self.Detalhe = Detalhe


class Contexto:
    def __init__(self, Forja, Raiz, Release):
        self.Forja = Forja
        self.Release = Release
        self.Raiz = Raiz
        self.Raiz.mkdir(parents=True, exist_ok=True)
        self.Host = Raiz / "host.exe"

    def Fonte(self, Nome):
        return PastaReload / (Nome + ".fj")

    def Dll(self, Nome):
        return self.Raiz / (Nome + ".dll")

    def Pasta(self, Nome):
        Pasta = self.Raiz / Nome
        Pasta.mkdir(parents=True, exist_ok=True)
        return Pasta


def Executar(Comando):
    return subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace")


def Compilar(Forja, Fonte, Saida, Dll, Release):
    Comando = [str(Forja), "build"]
    if Dll:
        Comando += ["--modo", "dll"]
    if Release:
        Comando.append("--release")
    Comando += [str(Fonte), "-o", str(Saida)]
    Res = Executar(Comando)
    return Res.returncode == 0, (Res.stdout + Res.stderr).strip()


def IniciarHost(Exe, Args, Pasta):
    Saida = open(Pasta / "host.out", "w", encoding="utf-8")
    Erro = open(Pasta / "host.err", "w", encoding="utf-8")
    Proc = subprocess.Popen([str(Exe)] + Args, stdout=Saida, stderr=Erro)
    return Proc, Saida, Erro


def EncerrarHost(Proc, Saida, Erro):
    try:
        Proc.wait(timeout=PRAZO_HOST_S)
    except subprocess.TimeoutExpired:
        Proc.kill()
        Proc.wait()
    Saida.close()
    Erro.close()
    return Proc.returncode


def EsperarHostCarregado(PastaQuente):
    Limite = time.monotonic() + TETO_CARGA_S
    while time.monotonic() < Limite:
        if PastaQuente.exists() and any(PastaQuente.iterdir()):
            return True
        time.sleep(0.05)
    return False


def TrocarComMarca(Forja, Fonte, Alvo, Marca, Release):
    # Compila a fonte sobre o caminho do host, levanta a marca e espera o host consumi-la. Devolve (ok, segundos do build, detalhe).
    Inicio = time.perf_counter()
    Ok, Saida = Compilar(Forja, Fonte, Alvo, True, Release)
    BuildS = time.perf_counter() - Inicio
    if not Ok:
        return False, BuildS, Saida[:300]
    Marca.write_text("1", encoding="ascii")
    Limite = time.monotonic() + PRAZO_MARCA_S
    while Marca.exists():
        if time.monotonic() > Limite:
            return False, BuildS, "marca nao consumida pelo host"
        time.sleep(0.02)
    return True, BuildS, ""


def AcumularLinha(Dados, Linha):
    M = RE_QUADRO.match(Linha)
    if M:
        Dados["quadros"].append((int(M[1]), int(M[2]), int(M[3])))
        return
    M = RE_TROCA.match(Linha)
    if M:
        Dados["trocas"].append((int(M[1]), int(M[2]), int(M[3])))
        return
    M = RE_MARCA.match(Linha)
    if M:
        Dados["marcas"].append(int(M[1]))
        return
    M = RE_RECUSA.match(Linha)
    if M:
        Dados["recusas"].append((int(M[1]), int(M[2])))
        return
    if Linha.startswith("  motivo="):
        Dados["motivos"].append(Linha.split("=", 1)[1])
    elif Linha.startswith("  + "):
        Dados["diferencas"].append(Linha[4:])
    elif Linha.startswith("symbol nao_existe = missing_symbol"):
        Dados["symbol_ausente"] = True


def ParsearQuadros(Texto):
    Dados = {"quadros": [], "trocas": [], "marcas": [], "recusas": [], "motivos": [], "diferencas": [], "symbol_ausente": False}
    for Linha in Texto.splitlines():
        AcumularLinha(Dados, Linha)
    return Dados


def RodarQuadros(Ctx, Nome, Inicial, Fontes):
    # Sobe o host em modo quadros sobre uma copia da DLL inicial e aplica as fontes, uma troca por vez.
    Pasta = Ctx.Pasta(Nome)
    Alvo = Pasta / "jogo.dll"
    Marca = Pasta / "marca.txt"
    Ok, Saida = Compilar(Ctx.Forja, Inicial, Alvo, True, Ctx.Release)
    if not Ok:
        return {"falha": "build inicial falhou: " + Saida[:200]}
    Proc, Fo, Fe = IniciarHost(Ctx.Host, ["quadros", str(Alvo), str(QUADROS_HOST), str(Marca)], Pasta)
    Builds, Falha = [], ""
    try:
        if not EsperarHostCarregado(Pasta / ".forja_quente"):
            Falha = "host nao carregou a DLL"
        time.sleep(AQUECIMENTO_S)  # quadros da DLL inicial antes da primeira troca: o contador tem de chegar a um valor que a troca continua
        for Fonte in Fontes:
            if Falha:
                break
            Ok, BuildS, Detalhe = TrocarComMarca(Ctx.Forja, Fonte, Alvo, Marca, Ctx.Release)
            Builds.append(BuildS)
            if not Ok:
                Falha = "troca falhou: " + Detalhe
    finally:
        Rc = EncerrarHost(Proc, Fo, Fe)
    Texto = (Pasta / "host.out").read_text(encoding="utf-8", errors="replace")
    Erro = (Pasta / "host.err").read_text(encoding="utf-8", errors="replace")
    return {"falha": Falha, "rc": Rc, "erro": Erro, "dados": ParsearQuadros(Texto), "builds": Builds}


def FalhaBase(R, VazamentoEsperado=False):
    # Falha comum a todo cenario de quadros: build, carga, rc do host e R0009 fora do cenario 6.
    if R.get("falha"):
        return R["falha"]
    if R["rc"] != 0:
        return "host rc=" + str(R["rc"]) + ": " + R["erro"][:200]
    if not VazamentoEsperado and "R0009" in R["erro"]:
        return "R0009 inesperado no stderr: " + R["erro"][:200]
    return ""


def PorQuadro(Dados):
    return {Q: (Gen, Passos) for (Q, Gen, Passos) in Dados["quadros"]}


def ChecarTrocaParaGen1(Dados):
    # v1 -> v2: a troca, a continuidade do contador e +10 por quadro nos 3 quadros seguintes.
    PorQ = PorQuadro(Dados)
    Trocas = [T for T in Dados["trocas"] if T[0] == 1]
    if not Trocas or not Dados["marcas"]:
        return Resultado(False, "troca para gen 1 ou marca ausente")
    Q1, Marca = Trocas[0][1], Dados["marcas"][0]
    if Q1 > Marca + 3:
        return Resultado(False, f"troca em q={Q1} e marca em q={Marca}: mais de 3 quadros")
    if PorQ[Q1][0] != 1 or PorQ[Q1 - 1][0] != 0:
        return Resultado(False, f"geracao nao muda em q={Q1}")
    Antes, Depois = PorQ[Q1 - 1][1], PorQ[Q1][1]
    if Depois != Antes + 10:
        return Resultado(False, f"contador nao continuou: {Antes} -> {Depois} (esperado +10)")
    Seguintes = [PorQ[Q1 + K][1] - PorQ[Q1 + K - 1][1] for K in (1, 2, 3)]
    if Seguintes != [10, 10, 10]:
        return Resultado(False, f"passos seguintes {Seguintes} (esperado 10,10,10)")
    Gen0 = [PorQ[Q][1] - PorQ[Q - 1][1] for Q in range(2, Q1)]
    if any(D != 1 for D in Gen0):
        return Resultado(False, "gen 0 nao subia 1 por quadro")
    return Resultado(True, f"troca q={Q1} (marca q={Marca}), contador {Antes}->{Depois}, depois +10 x3")


def CenarioUm(Ctx):
    R = RodarQuadros(Ctx, "c1", Ctx.Fonte("jogo_v1"), [Ctx.Fonte("jogo_v2")])
    Falha = FalhaBase(R)
    if Falha:
        return Resultado(False, Falha)
    return ChecarTrocaParaGen1(R["dados"])


def CenarioDois(Ctx):
    R = RodarQuadros(Ctx, "c2", Ctx.Fonte("jogo_v1"), [Ctx.Fonte("jogo_v2"), Ctx.Fonte("jogo_v3")])
    Falha = FalhaBase(R)
    if Falha:
        return Resultado(False, Falha)
    D = R["dados"]
    PorQ = PorQuadro(D)
    Base = ChecarTrocaParaGen1(D)
    if not Base.Ok:
        return Base
    if len(D["recusas"]) != 1 or D["motivos"] != ["interface_changed"]:
        return Resultado(False, f"recusas={D['recusas']} motivos={D['motivos']} (esperado 1 interface_changed)")
    QRecusa = D["recusas"][0][1]
    if not any("extra" in Linha for Linha in D["diferencas"]):
        return Resultado(False, f"diff sem a linha do campo novo: {D['diferencas']}")
    if any(Gen == 2 for Gen, _ in PorQ.values()):
        return Resultado(False, "geracao 2 apareceu: a v3 nao devia trocar")
    Passos = [PorQ[Q][1] - PorQ[Q - 1][1] for Q in range(QRecusa + 1, max(PorQ) + 1)]
    if Passos and any(P != 10 for P in Passos):
        return Resultado(False, "depois da recusa o contador deixou de subir 10 por quadro")
    return Resultado(True, f"v3 recusada (interface_changed) em q={QRecusa}; diff: {len(D['diferencas'])} linha(s), host seguiu com v2")


def CenarioTres(Ctx):
    R = RodarQuadros(Ctx, "c3", Ctx.Fonte("jogo_v1"), [Ctx.Fonte("jogo_sem_update")])
    Falha = FalhaBase(R)
    if Falha:
        return Resultado(False, Falha)
    D = R["dados"]
    PorQ = PorQuadro(D)
    if not D["symbol_ausente"]:
        return Resultado(False, "symbol de nome ausente nao deu missing_symbol")
    if len(D["recusas"]) != 1 or D["motivos"] != ["interface_changed"]:
        return Resultado(False, f"recusas={D['recusas']} motivos={D['motivos']}")
    if any(Gen != 0 for Gen, _ in PorQ.values()):
        return Resultado(False, "a DLL sem update trocou de geracao")
    if any(PorQ[Q][1] - PorQ[Q - 1][1] != 1 for Q in range(2, max(PorQ) + 1)):
        return Resultado(False, "host deixou de subir 1 por quadro com a versao anterior")
    return Resultado(True, f"jogo sem update recusado em q={D['recusas'][0][1]} (interface_changed: o hash cobre o nome do export); missing_symbol testado direto; host segue com v1")


def CopiarArquivo(Origem, Destino):
    shutil.copyfile(Origem, Destino)


def CenarioQuatro(Ctx):
    Pasta = Ctx.Pasta("c4")
    Alvo = Pasta / "alvo.dll"
    CopiarArquivo(Ctx.Dll("v1"), Alvo)
    Proc, Fo, Fe = IniciarHost(Ctx.Host, ["rodizio", str(Alvo), str(Ctx.Dll("v1")), str(Ctx.Dll("v2")), "1000"], Pasta)
    Rc = EncerrarHost(Proc, Fo, Fe)
    Texto = (Pasta / "host.out").read_text(encoding="utf-8", errors="replace")
    M = next((RE_RODIZIO.match(Linha) for Linha in Texto.splitlines() if RE_RODIZIO.match(Linha)), None)
    if Rc != 0 or M is None:
        return Resultado(False, f"rodizio rc={Rc}: {Texto[-200:]}")
    Trocas, Falhas, Pico100, PicoFim, Quente, Gen = (int(X) for X in M.groups())
    Crescimento = PicoFim - Pico100
    if Falhas != 0 or Gen != 1000:
        return Resultado(False, f"falhas={Falhas} gen={Gen} (esperado 0 e 1000)")
    if Crescimento > 4 * MIB:
        return Resultado(False, f"pico cresceu {Crescimento} bytes apos 100 trocas (limite 4 MiB)")
    if not 0 <= Quente <= 2:
        return Resultado(False, f"{Quente} arquivo(s) em .forja_quente (limite 2)")
    return Resultado(True, f"{Trocas} trocas, 0 falhas; pico100={Pico100} picofim={PicoFim} crescimento={Crescimento / MIB:.2f} MiB; quente={Quente}")


def CenarioCinco(Ctx):
    Pasta = Ctx.Pasta("c5")
    Alvo = Pasta / "alvo.dll"
    CopiarArquivo(Ctx.Dll("v1"), Alvo)
    Proc, Fo, Fe = IniciarHost(Ctx.Host, ["async", str(Alvo), str(Ctx.Dll("v2"))], Pasta)
    Rc = EncerrarHost(Proc, Fo, Fe)
    Texto = (Pasta / "host.out").read_text(encoding="utf-8", errors="replace")
    M = next((RE_ASYNC.match(Linha) for Linha in Texto.splitlines() if RE_ASYNC.match(Linha)), None)
    if Rc != 0 or M is None:
        return Resultado(False, f"async rc={Rc}: {Texto[-200:]}")
    TrocaOk, Gen, EsperaMs, Quadros, SemQueda = M.groups()
    if TrocaOk != "true" or Gen != "1" or SemQueda != "true":
        return Resultado(False, f"troca_ok={TrocaOk} gen={Gen} sem_queda={SemQueda}")
    if int(EsperaMs) < 500:
        return Resultado(False, f"troca {EsperaMs} ms depois do pedido (esperado >= 500, R-C9)")
    if any(int(Q) == 0 for Q in Quadros.split(",")):
        return Resultado(False, "alguma tarefa nao rodou")
    return Resultado(True, f"4 tarefas de 500 ms, troca {EsperaMs} ms depois do pedido, sem queda; quadros={Quadros}")


def CenarioSeis(Ctx):
    R = RodarQuadros(Ctx, "c6", Ctx.Fonte("jogo_v1"), [Ctx.Fonte("jogo_vaza"), Ctx.Fonte("jogo_v2")])
    Falha = FalhaBase(R, VazamentoEsperado=True)
    if Falha:
        return Resultado(False, Falha)
    D = R["dados"]
    PorQ = PorQuadro(D)
    G1 = [T for T in D["trocas"] if T[0] == 1]
    G2 = [T for T in D["trocas"] if T[0] == 2]
    if not G1 or not G2:
        return Resultado(False, "troca para gen 1 ou 2 ausente")
    QuadrosVaza = sum(1 for Gen, _ in PorQ.values() if Gen == 1)
    M = RE_VAZAMENTO.search(R["erro"])
    if M is None or int(M[1]) != QuadrosVaza:
        Achado = M[1] if M else None
        return Resultado(False, f"R0009 no stderr: {Achado} blocos, esperado {QuadrosVaza} (um por quadro com a vaza)")
    if any(Gen > 2 for Gen, _ in PorQ.values()):
        return Resultado(False, "geracao acima de 2")
    Q2 = G2[0][1]
    if any(PorQ[Q][1] - PorQ[Q - 1][1] != 10 for Q in range(Q2 + 1, max(PorQ) + 1)):
        return Resultado(False, "host nao seguiu com a v2 depois da troca da vaza")
    return Resultado(True, f"R0009 no stderr ({QuadrosVaza} blocos = quadros da vaza, saiu no fim do host); host segue com v2 depois de q={Q2}")


def MedirTrocas(Ctx):
    Fontes = [Ctx.Fonte("jogo_v2") if N % 2 == 0 else Ctx.Fonte("jogo_v1") for N in range(TROCAS_MEDIDAS)]
    R = RodarQuadros(Ctx, "c7", Ctx.Fonte("jogo_v1"), Fontes)
    Falha = FalhaBase(R)
    if Falha:
        return Resultado(False, Falha)
    Trocas = R["dados"]["trocas"]
    if len(Trocas) != TROCAS_MEDIDAS or R["dados"]["recusas"]:
        return Resultado(False, f"{len(Trocas)} trocas e {len(R['dados']['recusas'])} recusas (esperado {TROCAS_MEDIDAS} e 0)")
    Builds = R["builds"]
    Reloads = [T[2] / 1000.0 for T in Trocas]
    Totais = [B + RL for B, RL in zip(Builds, Reloads)]
    Detalhe = (f"build+deteccao+reload, n={TROCAS_MEDIDAS}: total mediana={statistics.median(Totais):.3f} s "
               f"(build {statistics.median(Builds):.3f} s, reload {statistics.median(Reloads) * 1000:.0f} ms; "
               f"deteccao = 1 quadro de 10 ms, nao cronometrada); meta 0,3 s com o Defender; sem portao")
    return Resultado(True, Detalhe)


def CenarioExemplo(Ctx):
    Pasta = Ctx.Pasta("exemplo")
    Dll = Pasta / "jogo.dll"
    Ok, Saida = Compilar(Ctx.Forja, PastaExemplo / "jogo.fj", Dll, True, Ctx.Release)
    if not Ok:
        return Resultado(False, "build do jogo do exemplo falhou: " + Saida[:200])
    Exe = Pasta / "host.exe"
    Ok, Saida = Compilar(Ctx.Forja, PastaExemplo / "host.fj", Exe, False, Ctx.Release)
    if not Ok:
        return Resultado(False, "build do host do exemplo falhou: " + Saida[:200])
    Res = Executar([str(Exe), str(Dll), "20"])
    Quadros = [RE_QUADRO.match(Linha) for Linha in Res.stdout.splitlines()]
    Passos = [int(M[3]) for M in Quadros if M]
    if Res.returncode != 0 or len(Passos) != 20:
        return Resultado(False, f"exemplo rc={Res.returncode}, {len(Passos)} quadros: {Res.stderr[:200]}")
    if Passos != list(range(1, 21)):
        return Resultado(False, f"contador do exemplo: {Passos}")
    return Resultado(True, "host.exe jogo.dll 20: 20 quadros, contador 1..20, sem troca")


def ImprimirCheque(Rotulo, Nome, Resultado, Contagem):
    Status = "ok" if Resultado.Ok else "FALHA"
    print(f"cenario {Nome} [{Rotulo}]: {Status} {Resultado.Detalhe}")
    Contagem["total"] += 1
    Contagem["ok"] += 1 if Resultado.Ok else 0


def RodarModo(Forja, Raiz, Rotulo, Release, Contagem):
    Ctx = Contexto(Forja, Raiz / Rotulo, Release)
    Ok, Saida = Compilar(Forja, PastaReload / "host.fj", Ctx.Host, False, Release)
    if not Ok:
        print(f"host nao compilou [{Rotulo}]: {Saida[:300]}")
        Contagem["total"] += 1
        return
    for Nome, Fonte in (("v1", "jogo_v1"), ("v2", "jogo_v2")):
        Compilar(Forja, Ctx.Fonte(Fonte), Ctx.Dll(Nome), True, Release)
    Cenarios = [("1", CenarioUm), ("2", CenarioDois), ("3", CenarioTres), ("4", CenarioQuatro), ("5", CenarioCinco), ("7", MedirTrocas), ("exemplo", CenarioExemplo)]
    if not Release:
        Cenarios.insert(5, ("6", CenarioSeis))
    for Nome, Funcao in Cenarios:
        ImprimirCheque(Rotulo, Nome, Funcao(Ctx), Contagem)


def Main():
    Parser = argparse.ArgumentParser(description="Prova do hot reload (M8 H3)")
    Parser.add_argument("--forja", default=str(ForjaPadrao))
    Parser.add_argument("--modo", choices=["dev", "release", "ambos"], default="ambos")
    Args = Parser.parse_args()
    Forja = Path(Args.forja)
    Modos = {"dev": [("dev", False)], "release": [("release", True)], "ambos": [("dev", False), ("release", True)]}[Args.modo]
    Contagem = {"total": 0, "ok": 0}
    Raiz = Path(tempfile.mkdtemp(prefix="forja_reload_"))
    try:
        for Rotulo, Release in Modos:
            RodarModo(Forja, Raiz, Rotulo, Release, Contagem)
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    if Contagem["ok"] == Contagem["total"]:
        print(f"RESULTADO: tudo certo ({Contagem['ok']}/{Contagem['total']} cenarios)")
        return 0
    print(f"RESULTADO: {Contagem['total'] - Contagem['ok']} falha(s) ({Contagem['ok']}/{Contagem['total']} cenarios)")
    return 1


if __name__ == "__main__":
    raise SystemExit(Main())
