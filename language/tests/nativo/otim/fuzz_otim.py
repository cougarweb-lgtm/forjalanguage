# -*- coding: utf-8 -*-
"""Fuzzer diferencial do otimizador x64 da Forja (M7a F9): programas .fj gerados por semente, 2 builds cada, comparacao de comportamento.

Para cada semente gera um programa valido (fuzz_otim_gerador.py) e o compila em release com
  x64_on  = forja build --release --backend x64 --checagem sim      (otimizador LIGADO)
  x64_off = o mesmo com --sem-otim tudo
e em amostra (programas sem acesso fora do limite) repete os 2 com --checagem nao. Compara stdout, stderr normalizado
(codigo R, linha, causa) e exit code. Achado = x64_on != x64_off (kind "otim": roda a bissecao de passes). Na fatia, x64_on
diferente do oraculo congelado (kind "congelado") so vira achado quando os 2 builds x64 concordam entre si (1.5.0: sem backend C).
Antes de acusar, reroda os exes: resultado instavel e "flaky", nao achado.
Achado vai para <saida>\\achados\\<semente>_<grupo>_<kind>\\ (programa.fj, minimo.fj reduzido por remocao de linhas, info.txt).

Uso:
    python fuzz_otim.py --forja <forja.exe> --fatia N [--jobs 6]        (padrao do dia a dia: sementes 1..2400 com semente % 4 == N % 4 = 600; so o x64 ligado, comparado com
                                                                         o oraculo congelado rapido\\oraculo_congelado.json; so x64; chave ausente do congelado = falha)
    python fuzz_otim.py --congelar                                      (APOSENTADO na 0.21.0 F4: o congelado veio do backend C; sai com exit 2)
    python fuzz_otim.py --forja <forja.exe> --completo --semente 1 --n 200 [--jobs 6] [--saida <pasta>]
    python fuzz_otim.py --forja <forja.exe> --completo --semente 1 --tempo 7200 --jobs 6          (2 h; F9)
    python fuzz_otim.py --novas 1500 --dirigido todos --cobertura [--semente-base 100001] [--jobs 6]   (1.2.0: sementes FORA do congelado; 1.5.0: so x64 ligado x --sem-otim tudo, sem oraculo C; --dirigido i tambem checa slp)
    python fuzz_otim.py --selftest
Saida final: "fuzz_otim: N programas, V validos (P%), A achados, F flaky, T s"; exit 0 sem achado, 1 com achado, 2 erro de uso.

Versao: 1.5.0
Changelog: 1.5.0 - 0.21.0 F4 passo 3 (sem backend C): a perna C saiu de tudo; cada semente roda 2 builds x64 (ligado x --sem-otim tudo); a fatia confere o congelado e reconfirma com os 2 builds; --novas e --completo/--tempo ficam so com o x64 (sem oraculo vivo); --congelar sai com exit 2 (o congelado vem do backend C); kind "oraculo" e "oraculo0200" aposentados.
           1.4.0 - 0.21.0 F3: --fatia so com o congelado (sem amostra viva de 150 nem fallback com os 3 builds); sementes fora do congelado falham com ErroOtim, nunca usam C vivo; a divergencia segue reconfirmada com os 3 builds.
           1.3.0 - S4 (SIMD): --dirigido aceita a..i (recurso i do gerador 1.5.0: slp; "todos" continua a..h); com i, cada programa e comparado tambem com o x64 --sem-otim slp (achado kind=slp);
                   --cobertura passa a medir o passe slp (remark `slp <fn>: N grupos vetorizados`) e imprime programas com grupo vetorizado, por motivo de recusa e com trap do recurso i depois de print.
           1.2.2- R6a: --dirigido aceita a..h (recurso h do gerador 1.4.0: licm).
           1.2.1 - R5 (SIMD): --dirigido aceita a..g (recurso g do gerador 1.3.0: losango); o selftest compara "todos" com Gerador.RECURSOS_DIRIGIDOS.
           1.2.0 - D0701: --novas N (sementes semente-base..+N-1, fora do congelado; base registrada na saida), --dirigido a..f|todos (so com --novas: forca os recursos do gerador 1.2.0),
                   --cobertura (passe x programas, disparos, disparos com trap; remarks para inline/sroa/numeracao, hash com --sem-otim para os outros; amostra de ate 1000).
                   O oraculo das novas roda 2x (flaky se diferir) e nao usa o congelado.
           1.1.1 - teste GeradorDeterministico: teto de linhas 220 -> 260 (o gerador ganhou structs e programas vao a 255 linhas).
                   1.1.0 - D0696: --fatia N (rodizio de 600 sementes, so x64 ligado contra o congelado, sem gcc), --congelar, --completo (o modo antigo; --tempo tambem o liga). Divergencia na fatia
                   e reconfirmada com os 3 builds vivos antes de virar achado.
           1.0.0 - M7a F9: primeira versao (gerador, 3 builds, bissecao de passes, minimizacao, selftest).
Verificacao: python -I fuzz_otim.py --selftest
"""

import argparse
import base64
import concurrent.futures
import hashlib
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fuzz_otim_gerador as Gerador  # noqa: E402
import verificar_otim as Otim  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "rapido"))
import oraculo_congelado as Oraculo  # noqa: E402

FATIAS = 4

FORJA_PADRAO = Otim.FORJA_PADRAO
SAIDA_PADRAO = Path(tempfile.gettempdir()) / "fuzz_otim"
ORCAMENTO_MINIMIZAR = 90  # segundos por achado
CADA_NAO = 4  # 1 de cada 4 programas sem acesso fora do limite tambem roda com --checagem nao
VARIANTES = (("x64_on", ["--backend", "x64"]), ("x64_off", ["--backend", "x64", "--sem-otim", "tudo"]))


def CompilarVariante(Forja, Fonte, Saida, Extra, Checagem):
    """(returncode, causa) do build release; o comando e o mesmo do verificar_otim com a variante no fim."""
    Comando = [Forja, "build", "--release", Fonte, "-o", str(Saida), "--checagem", Checagem] + Extra
    Resultado = subprocess.run(Comando, capture_output=True, timeout=Otim.TIMEOUT_BUILD)
    Causa = (Resultado.stdout + Resultado.stderr).decode("utf-8", errors="replace").strip()[:300]
    return Resultado.returncode, Causa


def RodarGrupo(Forja, Fonte, Pasta, Checagem):
    """{variante: resultado} ou ('build', {variante: (rc, causa)}) quando algum build falha."""
    Resultados, Falhas = {}, {}
    for Nome, Extra in VARIANTES:
        Exe = Pasta / f"{Nome}_{Checagem}.exe"
        Rc, Causa = CompilarVariante(Forja, Fonte, Exe, Extra, Checagem)
        if Rc != 0:
            Falhas[Nome] = (Rc, Causa)
        else:
            Resultados[Nome] = Otim.RodarCompleto(Exe)
    return Resultados, Falhas


def ClassificarGrupo(Resultados, Falhas):
    """'invalido' (programa recusado pelo forja com diagnostico), 'build' (falha de build assimetrica/interna), 'otim' ou None."""
    if Falhas:
        if len(Falhas) == len(VARIANTES) and all(Rc == 1 for Rc, _ in Falhas.values()):
            return "invalido"
        return "build"
    if Resultados["x64_on"] != Resultados["x64_off"]:
        return "otim"
    return None


def EstavelNaReexecucao(Pasta, Checagem, Resultados):
    """False quando algum exe, rodado de novo, nao repete o resultado da 1a vez (nao determinismo do ambiente, nao achado)."""
    for Nome, _ in VARIANTES:
        for _ in range(2):
            if Otim.RodarCompleto(Pasta / f"{Nome}_{Checagem}.exe") != Resultados[Nome]:
                return False
    return True


def Diferenca(Resultados):
    Linhas = []
    for Nome, Valor in Resultados.items():
        Linhas.append(f"{Nome}: exit={Valor[0]} stderr={Valor[2]} stdout_md5={hash(Valor[1]) & 0xFFFFFFFF:08x} stdout_ini={Valor[1][:80]!r} stdout_fim={Valor[1][-80:]!r}")
    return "\n".join(Linhas)


def AvaliarGrupo(Forja, Texto, Pasta, Checagem):
    """(kind, resultados, falhas) para um grupo de checagem; kind None = igual; 'flaky' quando instavel."""
    Fonte = Pasta / "programa.fj"
    Fonte.write_text(Texto, encoding="utf-8", newline="\n")
    Resultados, Falhas = RodarGrupo(Forja, str(Fonte), Pasta, Checagem)
    Kind = ClassificarGrupo(Resultados, Falhas)
    if Kind in ("otim", "oraculo") and not EstavelNaReexecucao(Pasta, Checagem, Resultados):
        Kind = "flaky"
    return Kind, Resultados, Falhas


def MesmoAchado(Forja, Linhas, Kind, Checagem):
    """Predicado da minimizacao: o texto ainda e valido e mostra o mesmo kind de divergencia."""
    with tempfile.TemporaryDirectory(prefix="fuzz_min_") as Temp:
        Obtido, _, _ = AvaliarGrupo(Forja, "\n".join(Linhas) + "\n", Path(Temp), Checagem)
    return Obtido == Kind


def Minimizar(Linhas, Predicado, Orcamento=ORCAMENTO_MINIMIZAR):
    """Remocao de blocos de linhas (metade, quarto, ... 1) enquanto o predicado seguir verdadeiro; para no orcamento (segundos)."""
    Fim = time.time() + Orcamento
    Tam = max(1, len(Linhas) // 2)
    while Tam >= 1 and time.time() < Fim:
        Mudou, I = False, 0
        while I < len(Linhas) and time.time() < Fim:
            Candidato = Linhas[:I] + Linhas[I + Tam:]
            if Candidato and Predicado(Candidato):
                Linhas, Mudou = Candidato, True
            else:
                I += Tam
        if not Mudou:
            Tam //= 2
    return Linhas


def SalvarAchado(Forja, Passes, Saida, Semente, Checagem, Kind, Texto, Resultados, Falhas):
    Pasta = Saida / "achados" / f"{Semente}_{Checagem}_{Kind}"
    Pasta.mkdir(parents=True, exist_ok=True)
    (Pasta / "programa.fj").write_text(Texto, encoding="utf-8", newline="\n")
    Minimo = Minimizar(Texto.splitlines(), lambda L: MesmoAchado(Forja, L, Kind, Checagem))
    (Pasta / "minimo.fj").write_text("\n".join(Minimo) + "\n", encoding="utf-8", newline="\n")
    Culpado = "n/a (oraculo: nao e passe)"
    if Kind == "otim":
        with tempfile.TemporaryDirectory(prefix="fuzz_bis_") as Temp:
            Conjunto = Otim.BisseccaoPasses(Forja, str(Pasta / "minimo.fj"), Path(Temp), Passes, Checagem)
        Culpado = ", ".join(Conjunto) if Conjunto else "minimo nao diverge mais" if Conjunto is None else "nenhum subconjunto resolve"
    Info = [f"semente={Semente} checagem={Checagem} kind={Kind}", f"passe culpado: {Culpado}", f"linhas: {len(Texto.splitlines())} -> {len(Minimo)}"]
    (Pasta / "info.txt").write_text("\n".join(Info) + "\n" + Diferenca(Resultados) + "\n" + repr(Falhas) + "\n", encoding="utf-8")
    return f"ACHADO semente={Semente} checagem={Checagem} kind={Kind} passe culpado: {Culpado} ({Pasta})"


def TestarSemente(Forja, Passes, Saida, Semente, UsarNao, Dirigido=()):
    """(validos 0/1, flaky 0/1, [textos dos achados]). Dirigido: recursos do gerador forcados (so nas sementes novas)."""
    Texto, Meta = Gerador.GerarPrograma(Semente, Dirigido)
    Grupos = ["sim"] + (["nao"] if UsarNao and not Meta["oob"] else [])
    Achados, Valido, Flaky = [], 0, 0
    with tempfile.TemporaryDirectory(prefix=f"fuzz_{Semente}_") as Temp:
        for Checagem in Grupos:
            Kind, Resultados, Falhas = AvaliarGrupo(Forja, Texto, Path(Temp), Checagem)
            Valido = 0 if Kind == "invalido" else 1
            Flaky += Kind == "flaky"
            if Kind in ("otim", "build"):
                Achados.append(SalvarAchado(Forja, Passes, Saida, Semente, Checagem, Kind, Texto, Resultados, Falhas))
            if Kind == "invalido":
                break
    return Valido, Flaky, Achados


def Executar(Args):
    Forja = Args.forja
    if not os.path.isabs(Forja) or not os.path.isfile(Forja):
        raise Otim.ErroOtim(f"--forja precisa ser caminho absoluto de um arquivo existente: {Forja}")
    Saida = Path(Args.saida)
    Passes = Otim.DescobrirPasses(Forja, str(Otim.PastaBench / "fib_rec.fj"))
    Inicio = time.time()
    Fim = Inicio + Args.tempo if Args.tempo else None
    Total = Validos = Flaky = 0
    Achados = []
    print(f"fuzz_otim {VERSAO} | passes: {', '.join(Passes)} | semente inicial {Args.semente} | jobs {Args.jobs} | saida {Saida}", flush=True)
    Proxima = Args.semente
    with concurrent.futures.ThreadPoolExecutor(max_workers=Args.jobs) as Pool:
        while (Fim is None and Total < Args.n) or (Fim is not None and time.time() < Fim):
            Lote = range(Proxima, Proxima + (Args.jobs * 4 if Fim else min(Args.jobs * 4, Args.n - Total)))
            Proxima = Lote.stop
            for V, F, A in Pool.map(lambda S: TestarSemente(Forja, Passes, Saida, S, S % CADA_NAO == 0), Lote):
                Total, Validos, Flaky = Total + 1, Validos + V, Flaky + F
                Achados += A
                for Linha in A:
                    print(Linha, flush=True)
            print(f"  ... {Total} programas, {len(Achados)} achados, {time.time() - Inicio:.0f} s", flush=True)
    Taxa = 100.0 * Validos / max(Total, 1)
    print(f"fuzz_otim: {Total} programas, {Validos} validos ({Taxa:.1f}%), {len(Achados)} achados, {Flaky} flaky, {time.time() - Inicio:.0f} s | ultima semente {Proxima - 1}")
    return 0 if not Achados else 1


def RegistroComoResultado(Registro):
    """(exit, stdout, stderr normalizado) do congelado, no formato do Otim.RodarCompleto."""
    Stderr = tuple(tuple(P) if isinstance(P, list) else P for P in Registro["stderr"])
    return Registro["exit"], base64.b64decode(Registro["stdout"]), Stderr


def ConfereComCongelado(Forja, Fonte, Checagem, Registro):
    """'ok', 'invalido' (o x64 recusa como o forja de referencia recusou) ou 'divergiu'; Fonte ja gravada em arquivo."""
    Exe = Fonte.parent / f"x64_on_{Checagem}.exe"
    Rc, _ = CompilarVariante(Forja, str(Fonte), Exe, VARIANTES[0][1], Checagem)
    if Registro["invalido"]:
        return "invalido" if Rc == 1 else "divergiu"
    if Rc != 0:
        return "divergiu"
    return "ok" if Otim.RodarCompleto(Exe) == RegistroComoResultado(Registro) else "divergiu"


def TestarSementeFatia(Forja, Passes, Saida, Semente, Banco):
    """(validos 0/1, flaky 0/1, [achados]): so o x64 ligado contra o congelado; divergencia e reconfirmada com os 2 builds x64 (TestarSemente) antes de virar achado."""
    Texto, Meta = Gerador.GerarPrograma(Semente)
    Grupos = ["sim"] + (["nao"] if Semente % CADA_NAO == 0 and not Meta["oob"] else [])
    Valido = 1
    with tempfile.TemporaryDirectory(prefix=f"fuzz_f{Semente}_") as Temp:
        Fonte = Path(Temp) / "programa.fj"
        Fonte.write_text(Texto, encoding="utf-8", newline="\n")
        for Checagem in Grupos:
            Registro = Banco.Obter(Texto, Checagem, Gerar=False)
            if Registro is None:
                raise Otim.ErroOtim(f"semente {Semente} ({Checagem}) fora do congelado; o congelado nao e regenerado (backend C aposentado)")
            Veredito = ConfereComCongelado(Forja, Fonte, Checagem, Registro)
            if Veredito == "invalido":
                return 0, 0, []
            if Veredito == "divergiu":
                Vivo = TestarSemente(Forja, Passes, Saida, Semente, Semente % CADA_NAO == 0)
                return Vivo if Vivo[2] else (Vivo[0], Vivo[1], [f"ACHADO semente={Semente} checagem={Checagem} kind=congelado (x64 ligado e desligado concordam entre si e difieren do congelado: congelado antigo ou bug x64; sem C nao se decide)"])
    return Valido, 0, []


def SementesDaFatia(Fatia, Banco):
    """As 600 sementes da fatia, todas no congelado (sem amostra viva). Chave ausente: o banco selado levanta ErroOraculo antes; senao ErroOtim."""
    Todas = [S for S in Oraculo.SEMENTES_FUZZ if S % FATIAS == Fatia % FATIAS]
    Faltam = [S for S in Todas if Banco.Obter(Gerador.GerarPrograma(S)[0], "sim", Gerar=False) is None]
    if Faltam:
        raise Otim.ErroOtim(f"fatia {Fatia}: {len(Faltam)} sementes fora do congelado (ex.: {Faltam[:5]}); --fatia nao usa C vivo")
    return Todas


def ExecutarFatia(Args):
    Forja = Args.forja
    if not os.path.isabs(Forja) or not os.path.isfile(Forja):
        raise Otim.ErroOtim(f"--forja precisa ser caminho absoluto de um arquivo existente: {Forja}")
    Banco = Oraculo.BancoPadrao()
    Sementes = SementesDaFatia(Args.fatia, Banco)
    Passes = Otim.DescobrirPasses(Forja, str(Otim.PastaBench / "fib_rec.fj"))
    Saida, Inicio = Path(Args.saida), time.time()
    print(f"fuzz_otim {VERSAO} --fatia {Args.fatia} | {len(Sementes)} sementes | congelado, so x64 ligado | jobs {Args.jobs}", flush=True)
    Tarefa = lambda S: TestarSementeFatia(Forja, Passes, Saida, S, Banco)
    with concurrent.futures.ThreadPoolExecutor(max_workers=Args.jobs) as Pool:
        Resultados = list(Pool.map(Tarefa, Sementes))
    Achados = [A for _, _, Lista in Resultados for A in Lista]
    for Linha in Achados:
        print(Linha, flush=True)
    Validos = sum(V for V, _, _ in Resultados)
    print(f"fuzz_otim: {len(Sementes)} programas, {Validos} validos ({100.0 * Validos / max(len(Sementes), 1):.1f}%), {len(Achados)} achados, {sum(F for _, F, _ in Resultados)} flaky, {time.time() - Inicio:.0f} s")
    return 0 if not Achados else 1


VERSAO = "1.5.0"
SEMENTE_BASE_NOVAS = 100001  # fora do congelado (1..2400); pode ser trocada com --semente-base e vai para o cabecalho da saida
LIMITE_AMOSTRA_COBERTURA = 1000
PASSES_COM_REMARK = ("inline", "sroa", "numeracao", "slp")  # descoberto: so estes emitem remark de disparo (slp desde 0.1.0, S3); os outros so tem a linha-resumo
REGEX_REMARK = {"inline": r"^inline \S+ <- ", "sroa": r"^sroa (?!recusa)\S+ k\d+: ", "slp": r"^slp \S+: [1-9]\d* grupos vetorizados"}


def ValidarDirigido(Texto):
    """'' (sem dirigido) ou letras de a..i; 'todos' = a..h (o i, do slp, so quando pedido). Qualquer outra coisa recusa."""
    Valor = (Texto or "").strip().lower()
    if Valor == "":
        return ""
    if Valor == "todos":
        return Gerador.RECURSOS_ANTIGOS
    if not set(Valor) <= set(Gerador.RECURSOS_DIRIGIDOS):
        raise Otim.ErroOtim(f"--dirigido aceita a..i ou todos, recebi: {Texto!r}")
    return "".join(sorted(set(Valor)))


def ConfereSemSlp(Forja, Fonte, Checagem):
    """'ok' ou 'divergiu': o x64_on que acabou de ser gerado contra o mesmo build com --sem-otim slp (S4: bits das lanes iguais ao escalar)."""
    Ligado, Sem = Fonte.parent / f"x64_on_{Checagem}.exe", Fonte.parent / f"x64_sem_slp_{Checagem}.exe"
    Rc, _ = CompilarVariante(Forja, str(Fonte), Sem, VARIANTES[0][1] + ["--sem-otim", "slp"], Checagem)
    if Rc != 0:
        return "divergiu"
    return "ok" if Otim.RodarCompleto(Ligado) == Otim.RodarCompleto(Sem) else "divergiu"


def AchadoSlp(Saida, Semente, Checagem, Texto):
    """Grava o programa do achado kind=slp (x64 ligado difere do x64 com --sem-otim slp) e devolve a linha de relatorio."""
    Pasta = Saida / "achados" / f"{Semente}_{Checagem}_slp"
    Pasta.mkdir(parents=True, exist_ok=True)
    (Pasta / "programa.fj").write_text(Texto, encoding="utf-8", newline="\n")
    return f"ACHADO semente={Semente} checagem={Checagem} kind=slp (x64 ligado difere do x64 com --sem-otim slp) ({Pasta})"


def TestarSementeNova(Forja, Passes, Saida, Semente, Dirigido):
    """(validos 0/1, flaky 0/1, [achados]) de uma semente fora do congelado: x64 ligado x --sem-otim tudo; com 'i', tambem x64 ligado x --sem-otim slp."""
    Texto, Meta = Gerador.GerarPrograma(Semente, Dirigido)
    Grupos = ["sim"] + (["nao"] if Semente % CADA_NAO == 0 and not Meta["oob"] else [])
    with tempfile.TemporaryDirectory(prefix=f"fuzz_n{Semente}_") as Temp:
        Pasta = Path(Temp)
        for Checagem in Grupos:
            Kind, Resultados, Falhas = AvaliarGrupo(Forja, Texto, Pasta, Checagem)
            if Kind == "invalido":
                return 0, 0, []
            if Kind == "flaky":
                return 0, 1, []
            if Kind is not None:
                return 1, 0, [SalvarAchado(Forja, Passes, Saida, Semente, Checagem, Kind, Texto, Resultados, Falhas)]
            if "i" in Dirigido and ConfereSemSlp(Forja, Pasta / "programa.fj", Checagem) == "divergiu":
                return 1, 0, [AchadoSlp(Saida, Semente, Checagem, Texto)]
    return 1, 0, []


def RemarkDisparou(Passe, Linhas):
    """Disparo pelo remark do --debug-code otim: inline (chamada trocada), sroa (agregado trocado), numeracao (algum contador > 0)."""
    if Passe == "numeracao":
        return any(int(Numero) > 0 for Linha in Linhas if Linha.startswith("numeracao ") for Numero in re.findall(r"=(\d+)", Linha))
    return any(re.search(REGEX_REMARK[Passe], Linha) for Linha in Linhas)


def ShaDoArquivo(Caminho):
    return hashlib.sha256(Caminho.read_bytes()).hexdigest()


def MedirUmPrograma(Forja, Passes, Texto, Pasta):
    """(disparos {passe: bool}, trap ocorrido bool) ou None se o programa nao compila. Remark para os passes que emitem; hash com --sem-otim para os outros."""
    Fonte = Pasta / "programa.fj"
    Fonte.write_text(Texto, encoding="utf-8", newline="\n")
    Exe = Pasta / "on.exe"
    Rc, _ = CompilarVariante(Forja, str(Fonte), Exe, VARIANTES[0][1] + ["--debug-code", "otim"], "sim")
    if Rc != 0:
        return None
    Linhas = Path(str(Exe) + ".debug.txt").read_text(encoding="utf-8", errors="replace").splitlines()
    Hash = ShaDoArquivo(Exe)
    Slp = ResumoSlp(Linhas)
    Disparos = {}
    for Passe in Passes:
        if Passe in PASSES_COM_REMARK:
            Disparos[Passe] = RemarkDisparou(Passe, Linhas)
            continue
        Rc2, _ = CompilarVariante(Forja, str(Fonte), Pasta / "sem.exe", ["--backend", "x64", "--sem-otim", Passe], "sim")
        Disparos[Passe] = Rc2 == 0 and ShaDoArquivo(Pasta / "sem.exe") != Hash
    return Disparos, Otim.RodarCompleto(Exe)[0] != 0, Slp


def ResumoSlp(Linhas):
    """{grupos: total de grupos vetorizados, motivos: {motivo das linhas `slp recusou: <motivo>`}} do remark do passe slp."""
    Grupos = sum(int(M.group(1)) for M in (re.match(r"^slp \S+: (\d+) grupos vetorizados", L) for L in Linhas) if M)
    return {"grupos": Grupos, "motivos": {M.group(1) for M in (re.match(r"^slp recusou: (\w+)", L) for L in Linhas) if M}}


def ContarSlp(Totais, Meta, Ocorrido, Slp):
    """Cobertura do slp (S4): programas com grupo vetorizado, por motivo de recusa, e com trap do recurso i depois de print (grupo vetorizado no programa)."""
    Cont = Totais["slp"]
    Cont["vetorizados"] += Slp["grupos"] > 0
    Cont["trap_apos_print"] += bool(Meta.get("slp_trap")) and Ocorrido and Slp["grupos"] > 0
    for Motivo in Slp["motivos"]:
        Cont["motivos"][Motivo] = Cont["motivos"].get(Motivo, 0) + 1


def MedirCobertura(Forja, Passes, Sementes, Dirigido, Jobs):
    """Tabela {passe: [programas, disparos, disparos com trap]} sobre a amostra de ate 1000 sementes, e os totais de trap."""
    Amostra = Otim.AmostrarCasos(list(Sementes), LIMITE_AMOSTRA_COBERTURA)
    Tabela = {Passe: [0, 0, 0] for Passe in Passes}
    Totais = {"medidos": 0, "invalidos": 0, "planejado": 0, "ocorrido": 0, "slp": {"vetorizados": 0, "trap_apos_print": 0, "motivos": {}}}

    def Medir(Semente):
        Texto, Meta = Gerador.GerarPrograma(Semente, Dirigido)
        with tempfile.TemporaryDirectory(prefix=f"fuzz_cob{Semente}_") as Temp:
            return Meta, MedirUmPrograma(Forja, Passes, Texto, Path(Temp))

    with concurrent.futures.ThreadPoolExecutor(max_workers=Jobs) as Pool:
        for Meta, Medido in Pool.map(Medir, Amostra):
            if Medido is None:
                Totais["invalidos"] += 1
                continue
            Disparos, Ocorrido, Slp = Medido
            Planejado = Meta["trap"] is not None
            ContarSlp(Totais, Meta, Ocorrido, Slp)
            Totais["medidos"] += 1
            Totais["planejado"] += Planejado
            Totais["ocorrido"] += Ocorrido
            for Passe, Disparou in Disparos.items():
                Tabela[Passe][0] += 1
                Tabela[Passe][1] += Disparou
                Tabela[Passe][2] += Disparou and (Planejado or Ocorrido)
    return Tabela, Totais


def ContarRecursos(Textos):
    """{recurso: programas que contem algum marcador} sobre os textos gerados (sem compilar)."""
    return {Letra: sum(any(Marca in Texto for Marca in Gerador.MARCADORES_DIRIGIDO[Letra]) for Texto in Textos) for Letra in Gerador.RECURSOS_DIRIGIDOS}


def ImprimirCobertura(Tabela, Totais, Recursos, Total, Dirigido):
    print(f"cobertura: {Totais['medidos']} programas medidos ({Totais['invalidos']} nao compilaram), trap planejado {Totais['planejado']}, trap ocorrido {Totais['ocorrido']}")
    print("passe           programas  disparos  com trap")
    for Passe, (Progs, Disp, Trap) in Tabela.items():
        print(f"{Passe:<15} {Progs:>9}  {Disp:>8}  {Trap:>8}")
    Slp = Totais["slp"]
    if Slp["vetorizados"] or Slp["motivos"]:
        print(f"slp (programas medidos {Totais['medidos']}): com grupo vetorizado {Slp['vetorizados']}, com trap do recurso i depois de print {Slp['trap_apos_print']}; programas por motivo de recusa:")
        for Motivo, Quantos in sorted(Slp["motivos"].items(), key=lambda Par: -Par[1]):
            print(f"  {Motivo:<24} {Quantos:>6}")
    print(f"recursos (programas com o marcador, de {Total}; forcado = {Dirigido or 'nenhum'}):")
    for Letra, Quantos in Recursos.items():
        print(f"  {Letra}: {Quantos} ({100.0 * Quantos / max(Total, 1):.1f}%)")


def ExecutarNovas(Args):
    Forja = Args.forja
    if not os.path.isabs(Forja) or not os.path.isfile(Forja):
        raise Otim.ErroOtim(f"--forja precisa ser caminho absoluto de um arquivo existente: {Forja}")
    Dirigido = ValidarDirigido(Args.dirigido)
    Sementes = range(Args.semente_base, Args.semente_base + Args.novas)
    Passes = Otim.DescobrirPasses(Forja, str(Otim.PastaBench / "fib_rec.fj"))
    Saida, Inicio = Path(Args.saida), time.time()
    print(f"fuzz_otim {VERSAO} --novas {Args.novas} | semente-base {Args.semente_base} | dirigido {Dirigido or 'nao'} | x64 ligado x --sem-otim tudo (sem oraculo C) | jobs {Args.jobs}", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=Args.jobs) as Pool:
        Resultados = list(Pool.map(lambda S: TestarSementeNova(Forja, Passes, Saida, S, Dirigido), Sementes))
    Achados = [A for _, _, Lista in Resultados for A in Lista]
    for Linha in Achados:
        print(Linha, flush=True)
    Validos = sum(V for V, _, _ in Resultados)
    Flaky = sum(F for _, F, _ in Resultados)
    print(f"fuzz_otim: {len(Sementes)} programas, {Validos} validos ({100.0 * Validos / max(len(Sementes), 1):.1f}%), {len(Achados)} achados, {Flaky} flaky, {time.time() - Inicio:.0f} s")
    if Args.cobertura:
        Tabela, Totais = MedirCobertura(Forja, Passes, Sementes, Dirigido, Args.jobs)
        Textos = [Gerador.GerarPrograma(S, Dirigido)[0] for S in Sementes]
        ImprimirCobertura(Tabela, Totais, ContarRecursos(Textos), len(Sementes), Dirigido)
    return 0 if not Achados else 1


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def TesteGeradorDeterministico():
    Afirmar(Gerador.GerarPrograma(7) == Gerador.GerarPrograma(7), "mesma semente, mesmo programa")
    Afirmar(Gerador.GerarPrograma(7)[0] != Gerador.GerarPrograma(8)[0], "sementes diferentes, programas diferentes")
    Tamanhos = [len(Gerador.GerarPrograma(S)[0].splitlines()) for S in range(40)]
    # Teto 260: o gerador ganhou structs e programas vao a 255 linhas (1.1.1); 220 era o limite antigo.
    Afirmar(min(Tamanhos) >= 30 and max(Tamanhos) <= 260, f"tamanhos fora da faixa: {min(Tamanhos)}..{max(Tamanhos)}")


def TesteClassificacao():
    Igual = {N: (0, b"1\n", ()) for N, _ in VARIANTES}
    Afirmar(ClassificarGrupo(Igual, {}) is None, "tudo igual = sem achado")
    Ruim = dict(Igual, x64_on=(0, b"2\n", ()))
    Afirmar(ClassificarGrupo(Ruim, {}) == "otim", "ligado diferente do desligado = otim")
    Afirmar(ClassificarGrupo({}, {N: (1, "E0301") for N, _ in VARIANTES}) == "invalido", "recusa de todos com exit 1 = invalido")
    Afirmar(ClassificarGrupo({"x64_off": Igual["x64_off"]}, {"x64_on": (3, "interno")}) == "build", "falha interna = build")


def TesteMinimizar():
    Linhas = [f"linha{K}" for K in range(40)]
    Minimo = Minimizar(Linhas, lambda L: "linha7" in L and "linha31" in L, 20)
    Afirmar(Minimo == ["linha7", "linha31"], f"a minimizacao deveria deixar so as 2 linhas, deixou {Minimo}")


def TesteProgramasReais():
    Afirmar(os.path.isfile(FORJA_PADRAO), f"forja nao encontrado: {FORJA_PADRAO}")
    Passes = Otim.DescobrirPasses(FORJA_PADRAO, str(Otim.PastaBench / "fib_rec.fj"))
    with tempfile.TemporaryDirectory(prefix="fuzz_st_") as Temp:
        for Semente in (1, 2, 3, 4, 5, 6):
            Valido, Flaky, Achados = TestarSemente(FORJA_PADRAO, Passes, Path(Temp), Semente, Semente % 2 == 0)
            Afirmar(Valido == 1 and not Achados, f"semente {Semente}: valido={Valido} achados={Achados}")


def TesteFatiaOffline():
    Vazio = Oraculo.BancoOraculo(Path(tempfile.gettempdir()) / "fuzz_otim_selftest_inexistente.json")
    Recusou = False
    try:
        SementesDaFatia(5, Vazio)
    except Otim.ErroOtim:
        Recusou = True
    Afirmar(Recusou, "fatia sem congelado deveria recusar, nunca cair em C vivo")
    Registro = {"invalido": False, "exit": 3, "stdout": base64.b64encode(b"x\n").decode("ascii"), "stderr": [["R0001", 4, "c"], "texto"]}
    Afirmar(RegistroComoResultado(Registro) == (3, b"x\n", (("R0001", 4, "c"), "texto")), "RegistroComoResultado")


def TesteDirigido():
    Afirmar(Gerador.GerarPrograma(7, "abcdef") == Gerador.GerarPrograma(7, "abcdef"), "dirigido: mesma semente, mesmo programa")
    Afirmar(Gerador.GerarPrograma(7, ()) == Gerador.GerarPrograma(7), "sem dirigido = gerador padrao (congelado intacto)")
    for Letra in Gerador.RECURSOS_DIRIGIDOS:
        Marcado = [any(Marca in Gerador.GerarPrograma(S, Letra)[0] for Marca in Gerador.MARCADORES_DIRIGIDO[Letra]) for S in range(100001, 100009)]
        Afirmar(all(Marcado), f"recurso {Letra} sem marcador em 8 sementes")
    Afirmar(ValidarDirigido("todos") == Gerador.RECURSOS_ANTIGOS and ValidarDirigido("ia") == "ai" and ValidarDirigido("") == "", "ValidarDirigido")
    try:
        ValidarDirigido("z")
    except Otim.ErroOtim:
        return
    raise AssertionError("--dirigido z deveria recusar")


def TesteSlp(Forja):
    """Recurso i: determinismo, ausencia sem o pedido (a..h nao muda) e 12 sementes que compilam em release."""
    Afirmar(Gerador.GerarPrograma(9, "i") == Gerador.GerarPrograma(9, "i"), "recurso i: mesma semente, mesmo programa")
    Afirmar(all("sl_" not in Gerador.GerarPrograma(S, Gerador.RECURSOS_ANTIGOS)[0] for S in range(100001, 100011)), "a..h nao pode gerar nada do recurso i")
    with tempfile.TemporaryDirectory(prefix="fuzz_slp_st_") as Temp:
        for Semente in range(100001, 100013):
            Fonte = Path(Temp) / f"s{Semente}.fj"
            Fonte.write_text(Gerador.GerarPrograma(Semente, "i")[0], encoding="utf-8", newline="\n")
            Rc, Causa = CompilarVariante(Forja, str(Fonte), Path(Temp) / f"s{Semente}.exe", VARIANTES[0][1], "sim")
            Afirmar(Rc == 0, f"recurso i, semente {Semente} nao compila: {Causa}")


def Selftest(Forja=FORJA_PADRAO):
    Testes = [("GeradorDeterministico", TesteGeradorDeterministico), ("Classificacao", TesteClassificacao), ("Minimizar", TesteMinimizar), ("FatiaOffline", TesteFatiaOffline),
              ("ProgramasReais", TesteProgramasReais), ("Dirigido", TesteDirigido), ("Slp", lambda: TesteSlp(Forja))]
    Falhas = 0
    for Nome, Teste in Testes:
        try:
            Teste()
        except Exception as Erro:
            print(f"FALHOU {Nome}: {Erro}")
            Falhas += 1
            continue
        print(f"OK {Nome}")
    print(f"selftest: {len(Testes) - Falhas}/{len(Testes)}")
    return Falhas == 0


def Main():
    Parser = argparse.ArgumentParser(description="Fuzzer diferencial do otimizador x64 (ligado x --sem-otim tudo; congelado na fatia).")
    Parser.add_argument("--forja", default=FORJA_PADRAO, help="forja.exe (caminho absoluto; use uma COPIA em language\\compiler)")
    Parser.add_argument("--semente", type=int, default=1, help="primeira semente (as seguintes sao consecutivas: reproduzivel)")
    Parser.add_argument("--n", type=int, default=100, help="quantos programas (ignorado com --tempo)")
    Parser.add_argument("--tempo", type=int, default=0, help="roda por SEGUNDOS em vez de --n programas")
    Parser.add_argument("--jobs", type=int, default=4)
    Parser.add_argument("--saida", default=str(SAIDA_PADRAO), help="pasta dos achados (padrao %%TEMP%%\\fuzz_otim)")
    Parser.add_argument("--fatia", type=int, default=0, help="rodizio: sementes 1..2400 com semente %% 4 == FATIA %% 4, so x64 ligado contra o congelado (padrao)")
    Parser.add_argument("--completo", action="store_true", help="modo antigo (2 builds x64 vivos, --semente/--n/--tempo), para o F9")
    Parser.add_argument("--congelar", action="store_true", help="APOSENTADO (0.21.0 F4, sem backend C): sai com exit 2")
    Parser.add_argument("--novas", type=int, default=0, help="N sementes fora do congelado (semente-base..+N-1), x64 ligado x --sem-otim tudo")
    Parser.add_argument("--semente-base", type=int, default=SEMENTE_BASE_NOVAS, help="primeira semente de --novas (registrada na saida)")
    Parser.add_argument("--dirigido", default="", help="recursos a..i (ou todos = a..h) forcados no gerador; so com --novas")
    Parser.add_argument("--cobertura", action="store_true", help="tabela passe x disparos x trap (amostra de ate 1000); so com --novas")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 0 if Selftest(Args.forja) else 1
    if Args.novas:
        return ExecutarNovas(Args)
    if Args.dirigido or Args.cobertura:
        raise Otim.ErroOtim("--dirigido e --cobertura valem so com --novas N")
    if Args.congelar:
        raise Otim.ErroOtim("--congelar aposentado (0.21.0 F4): o congelado veio do backend C, que saiu; o oraculo congelado atual fica como esta")
    return Executar(Args) if Args.completo or Args.tempo else ExecutarFatia(Args)


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (Otim.ErroOtim, Oraculo.ErroOraculo) as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        sys.exit(2)
