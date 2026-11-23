"""rodar_servidor.py - orquestrador do servidor de prova + bots (M5 P55; alvo linux no M5c-b P62).

Compila servidor.fj e bot.fj uma vez (forja build --release --backend x64|c), sobe o servidor (porta efemera em 127.0.0.1, gravada num arquivo), sobe o bot (40 sockets), espera o fim e confere: rc 0 dos dois, stderr vazio, 0 pacote perdido, hash final igual entre as contagens de workers, peak_memory por minuto e tremor do tick.

Uso: python -I rodar_servidor.py <forja.exe absoluto> [--workers 1,6] [--segundos 60] [--bots 40] [--vezes 1] [--backend x64|c] [--semente 12345] [--ram-plana] [--alvo windows|linux]
  --ram-plana: amostra o PrivateUsage (commit privado) do servidor a cada 60 s e confere (1) ultimo minuto <= 1,02 x minuto 2 (so com 3+ amostras) e (2) os 4 ultimos picos peak_memory iguais (so com 7+ min de servidor).
Versao: 0.3.0
Changelog: 0.3.0 - P62: --alvo linux deixa de ser stub. Compila com o forja REAL (--target x86_64-linux), copia os ELF para /tmp da distro alpine-build, roda servidor e bot no WSL (um sh grava o PID e faz exec do ELF; 127.0.0.1 porta 0 gravada em arquivo). RAM plana no Linux = RssAnon de /proc (amostra por minuto) com os mesmos criterios do Windows.
  0.2.0 - P56 (parecer do arquiteto, Opus): a medicao de 10 min deu degraus de ~110 KB no peak_memory (PeakWorkingSetSize, que nunca desce e conta paginas tocadas pela 1a vez) e 4 min planos; nao ha vazamento (servidor.fj, std/async.fj, rt_net_win.c), a metrica era impropria. Nova regra: PrivateUsage do processo servidor como metrica principal e trava de plato no pico; picos e privados saem por contagem de workers.
  0.1.0 - M5 P55: contagens de workers em sequencia, hash final comparado entre elas, perdidos e stderr conferidos, tremor e pico por minuto; --ram-plana exige o ultimo pico <= 1,02 x o do minuto 1; alvo linux = stub (Linux no P62).
"""
import ctypes
import pathlib
import re
import subprocess
import sys
import tempfile
import time
from ctypes import wintypes

PastaFixtures = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(PastaFixtures.parents[1] / "linux"))  # wsl_lote (tests\nativo\linux): distro e lote do WSL
import wsl_lote  # noqa: E402
ALVO_LINUX = "linux"
ALVO_FORJA_LINUX = "x86_64-linux"
EXTENSAO_DO_ALVO = {"windows": ".exe", "linux": ".elf"}
LIMITE_WSL_SEGUNDOS = 60
TICKS_POR_SEGUNDO = 60
FOLGA_SEGUNDOS = 120
LIMITE_PORTA_SEGUNDOS = 30
FATOR_RAM = 1.02
INTERVALO_RAM_SEGUNDOS = 60
PASSO_ESPERA_SEGUNDOS = 0.5
MINIMO_AMOSTRAS_RAM = 3
MINUTOS_MINIMOS_PLATO = 7
JANELA_PLATO_MINUTOS = 4
PROCESSO_CONSULTA = 0x0400
PROCESSO_LER_MEMORIA = 0x0010
PADRAO = {"workers": "1,6", "segundos": "60", "bots": "40", "vezes": "1", "backend": "x64", "semente": "12345", "alvo": "windows", "ram_plana": False}
REGEX_PORTA = re.compile(r"^127\.0\.0\.1:(\d{2,5})$")
REGEX_FIM = re.compile(r"^FIM hash=([0-9a-f]{16}) ticks=(\d+) perdidos=(\d+) jitter_max_ms=([\d.]+) jitter_p99_ms=([\d.]+)$", re.M)
REGEX_HASH = re.compile(r"^hash tick=(\d+) ([0-9a-f]{16})$", re.M)
REGEX_PICO = re.compile(r"^peak_memory=(\d+) minuto=(\d+)$", re.M)


class ContadoresMemoriaEx(ctypes.Structure):
    # PROCESS_MEMORY_COUNTERS_EX (psapi.h): cb = 80 bytes em 64 bits; PrivateUsage e o commit privado
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("PageFaultCount", wintypes.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


class ErroAmostragemRam(Exception):
    pass


if sys.platform == "win32":
    KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
    PSAPI = ctypes.WinDLL("psapi", use_last_error=True)
    KERNEL32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    KERNEL32.OpenProcess.restype = wintypes.HANDLE
    KERNEL32.CloseHandle.argtypes = [wintypes.HANDLE]
    KERNEL32.CloseHandle.restype = wintypes.BOOL
    PSAPI.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(ContadoresMemoriaEx), wintypes.DWORD]
    PSAPI.GetProcessMemoryInfo.restype = wintypes.BOOL


def LerOpcoes(Argv):
    Opcoes = dict(PADRAO)
    Posicionais = []
    Indice = 0
    while Indice < len(Argv):
        Argumento = Argv[Indice]
        if Argumento == "--ram-plana":
            Opcoes["ram_plana"] = True
            Indice += 1
        elif Argumento.startswith("--"):
            Opcoes[Argumento[2:]] = Argv[Indice + 1]
            Indice += 2
        else:
            Posicionais.append(Argumento)
            Indice += 1
    return Posicionais, Opcoes


def Ler(Caminho):
    if not Caminho.exists():
        return ""
    return Caminho.read_text(encoding="utf-8", errors="replace")


def FlagsDoAlvo(Opcoes):
    """Windows: --backend x64|c (gera o .exe). Linux: --target x86_64-linux (gera o ELF; o backend nao vale)."""
    if Opcoes["alvo"] == ALVO_LINUX:
        return ["--target", ALVO_FORJA_LINUX]
    return ["--backend", Opcoes["backend"]]


def Compilar(Forja, Fonte, Saida, FlagsAlvo):
    Resposta = subprocess.run([Forja, "build", "--release", *FlagsAlvo, str(Fonte), "-o", str(Saida)],
                              capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=900)
    if Resposta.returncode != 0 or not Saida.exists():
        return f"build {Fonte.name} rc={Resposta.returncode}: {(Resposta.stdout + Resposta.stderr).strip()[:300]!r}"
    return None


def EsperarPorta(Servidor, Arquivo):
    Fim = time.monotonic() + LIMITE_PORTA_SEGUNDOS
    while time.monotonic() < Fim:
        Casou = REGEX_PORTA.match(Ler(Arquivo).strip())
        if Casou:
            return Casou.group(1)
        if Servidor.poll() is not None:
            return None
        time.sleep(0.05)
    return None


def AbrirLogs(Pasta, Sufixo):
    Nomes = ("serv_out", "serv_err", "bot_out", "bot_err")
    return {Nome: open(Pasta / f"{Nome}_{Sufixo}.txt", "wb") for Nome in Nomes}


def Subir(Comando, Arquivos, Nome):
    return subprocess.Popen(Comando, stdout=Arquivos[f"{Nome}_out"], stderr=Arquivos[f"{Nome}_err"])


def Encerrar(Processo):
    if Processo is not None and Processo.poll() is None:
        Processo.kill()


def NomeRodada(Config, Workers):
    return f"w{Workers}_t{Config['ticks']}"


def AbrirProcesso(Pid):
    if sys.platform != "win32":
        raise ErroAmostragemRam("amostragem de RAM exige Windows (psapi)")
    Handle = KERNEL32.OpenProcess(PROCESSO_CONSULTA | PROCESSO_LER_MEMORIA, False, Pid)
    if not Handle:
        raise ErroAmostragemRam(f"OpenProcess falhou no pid {Pid}: winerror={ctypes.get_last_error()}")
    return Handle


def LerPrivado(Handle):
    Contadores = ContadoresMemoriaEx()
    Contadores.cb = ctypes.sizeof(ContadoresMemoriaEx)
    if not PSAPI.GetProcessMemoryInfo(Handle, ctypes.byref(Contadores), Contadores.cb):
        raise ErroAmostragemRam(f"GetProcessMemoryInfo falhou: winerror={ctypes.get_last_error()}")
    return Contadores.PrivateUsage


def AmostrarRam(Serv, LerRam, Amostras):
    try:
        Amostras.append((LerRam(), len(Amostras) + 1))
    except ErroAmostragemRam:
        # Processo que terminou entre o poll e a leitura nao tem mais commit para medir: so encerra a amostragem
        if Serv.poll() is None:
            raise


def FecharProcesso(Handle):
    if Handle:
        KERNEL32.CloseHandle(Handle)


def TextoDeArgumento(Alvo, Valor):
    """Texto de um argumento do processo. No Linux, caminho Windows vira /mnt/c/... (a distro monta os discos)."""
    if Alvo == ALVO_LINUX and isinstance(Valor, pathlib.Path):
        return wsl_lote.CaminhoWsl(Valor)
    return str(Valor)


def PidDo(Pasta, Nome):
    return Pasta / f"pid_{Nome}.txt"


def ComandoDoProcesso(Alvo, Exe, Argumentos, Pasta, Nome):
    """Linha de comando do processo. No Linux um sh da distro grava o proprio PID e faz exec do ELF: o PID do wsl.exe nao e o do servidor."""
    Textos = [TextoDeArgumento(Alvo, Argumento) for Argumento in Argumentos]
    if Alvo != ALVO_LINUX:
        return [str(Exe)] + Textos
    Roteiro = Pasta / f"run_{Nome}.sh"
    Linhas = [
        "#!/bin/sh",
        f"echo $$ > {wsl_lote.Aspas(wsl_lote.CaminhoWsl(PidDo(Pasta, Nome)))}",
        "exec " + " ".join(wsl_lote.Aspas(Texto) for Texto in [Exe] + Textos),
    ]
    Roteiro.write_bytes(("\n".join(Linhas) + "\n").encode("utf-8"))
    return ["wsl", "-d", wsl_lote.DISTRO, "--", "sh", wsl_lote.CaminhoWsl(Roteiro)]


def LerSaida(Caminho, Alvo):
    """Texto de uma saida do processo. No Linux tira o aviso UTF-16 que o wsl.exe escreve no proprio stderr."""
    if Alvo == ALVO_LINUX and Caminho.exists():
        return wsl_lote.TextoDoWsl(Caminho.read_bytes())
    return Ler(Caminho)


def LerRssAnonLinux(Pid):
    """RssAnon do processo na distro, em bytes (o analogo do PrivateUsage do Windows). ErroAmostragemRam se o processo ja saiu."""
    Resposta = wsl_lote.Wsl(["cat", f"/proc/{int(Pid)}/status"], LIMITE_WSL_SEGUNDOS)
    if Resposta.returncode != 0:
        raise ErroAmostragemRam(f"/proc/{Pid}/status indisponivel: rc={Resposta.returncode}")
    for Linha in wsl_lote.TextoDoWsl(Resposta.stdout).splitlines():
        if Linha.startswith("RssAnon:"):
            return int(Linha.split()[1]) * 1024
    raise ErroAmostragemRam(f"RssAnon ausente em /proc/{Pid}/status")


def AbrirMedidor(Config, Serv, Pasta, Sufixo):
    """(ler, fechar) da RAM do servidor no alvo; None sem --ram-plana. Windows: commit privado por handle. Linux: RssAnon por /proc."""
    if not Config["ram_plana"]:
        return None
    if Config["alvo"] == ALVO_LINUX:
        Arquivo = PidDo(Pasta, f"servidor_{Sufixo}")
        Pid = Ler(Arquivo).strip()
        if not Pid.isdigit():
            raise ErroAmostragemRam(f"PID do servidor nao gravado em {Arquivo}")
        return (lambda: LerRssAnonLinux(Pid), lambda: None)
    Handle = AbrirProcesso(Serv.pid)
    return (lambda: LerPrivado(Handle), lambda: FecharProcesso(Handle))


def MatarNoWsl(Pasta, Sufixo):
    """Mata na distro o servidor e o bot desta rodada: matar o wsl.exe nao mata o ELF. PID que ja saiu e ignorado."""
    for Nome in (f"servidor_{Sufixo}", f"bot_{Sufixo}"):
        Pid = Ler(PidDo(Pasta, Nome)).strip()
        if Pid.isdigit():
            try:
                wsl_lote.Wsl(["kill", "-9", Pid], LIMITE_WSL_SEGUNDOS)
            except subprocess.TimeoutExpired:
                pass  # distro travada: o estouro de tempo da rodada ja foi reportado


def DestinoWsl(Pasta):
    return f"/tmp/forja_p62/{Pasta.name}"


def CopiarParaWsl(Exes, Pasta):
    """Copia os ELF para a distro (/tmp: o /mnt/c nao garante o bit de execucao) e devolve os caminhos de la. Falha = RuntimeError."""
    Destino = DestinoWsl(Pasta)
    Dests = {Nome: f"{Destino}/{Caminho.name}" for Nome, Caminho in Exes.items()}
    Origens = [wsl_lote.CaminhoWsl(Caminho) for Caminho in Exes.values()]
    Comandos = [["mkdir", "-p", Destino], ["cp", *Origens, Destino + "/"], ["chmod", "755", *Dests.values()]]
    for Comando in Comandos:
        Resposta = wsl_lote.Wsl(Comando, LIMITE_WSL_SEGUNDOS)
        if Resposta.returncode != 0:
            raise RuntimeError(f"{Comando[0]} na distro falhou rc={Resposta.returncode}: {wsl_lote.TextoDoWsl(Resposta.stderr)[:200]!r}")
    return Dests


def EsperarEAmostrar(Serv, Bot, Config, Amostras, Medidor):
    Limite = Config["ticks"] / TICKS_POR_SEGUNDO + FOLGA_SEGUNDOS
    Inicio = time.monotonic()
    try:
        while Serv.poll() is None or Bot.poll() is None:
            Decorrido = time.monotonic() - Inicio
            if Decorrido >= Limite:
                raise subprocess.TimeoutExpired("servidor e bot", Limite)
            if Medidor and Serv.poll() is None and Decorrido >= (len(Amostras) + 1) * INTERVALO_RAM_SEGUNDOS:
                AmostrarRam(Serv, Medidor[0], Amostras)
            time.sleep(PASSO_ESPERA_SEGUNDOS)
    finally:
        if Medidor:
            Medidor[1]()


def ExecutarPar(Exes, Config, Workers, Pasta):
    Sufixo = NomeRodada(Config, Workers)
    Alvo = Config["alvo"]
    Porta = Pasta / f"porta_{Sufixo}.txt"
    Porta.unlink(missing_ok=True)
    Arquivos = AbrirLogs(Pasta, Sufixo)
    Serv = None
    Bot = None
    Amostras = []
    try:
        Comando = ComandoDoProcesso(Alvo, Exes["servidor"], [Workers, Config["bots"], Config["ticks"], Porta], Pasta, f"servidor_{Sufixo}")
        Serv = Subir(Comando, Arquivos, "serv")
        Numero = EsperarPorta(Serv, Porta)
        if Numero is None:
            return {"erro": "servidor nao anunciou a porta"}
        Inicio = time.monotonic()
        Comando = ComandoDoProcesso(Alvo, Exes["bot"], [Numero, Config["bots"], Config["ticks"], Config["semente"]], Pasta, f"bot_{Sufixo}")
        Bot = Subir(Comando, Arquivos, "bot")
        Medidor = AbrirMedidor(Config, Serv, Pasta, Sufixo)
        EsperarEAmostrar(Serv, Bot, Config, Amostras, Medidor)
        return {"rc_serv": Serv.wait(), "rc_bot": Bot.wait(), "duracao": time.monotonic() - Inicio, "privados": Amostras}
    except subprocess.TimeoutExpired:
        return {"erro": "estouro de tempo: processo nao terminou"}
    except ErroAmostragemRam as Exc:
        return {"erro": f"amostragem de RAM falhou: {Exc}"}
    finally:
        Encerrar(Serv)
        Encerrar(Bot)
        if Alvo == ALVO_LINUX:
            MatarNoWsl(Pasta, Sufixo)
        for Arquivo in Arquivos.values():
            Arquivo.close()


def ResumoVazio(Falhas):
    return {"falhas": Falhas, "hash": None, "perdidos": None, "jitter_max_ms": None, "jitter_p99_ms": None, "picos": [], "privados": [], "notas": [], "duracao": None, "ticks_por_s": None}


def ConferirSaidas(Execucao, TextoServ, TextoBot, Erros, Ticks):
    Falhas = []
    if Execucao["rc_serv"] != 0:
        Falhas.append(f"servidor rc={Execucao['rc_serv']}")
    if Execucao["rc_bot"] != 0:
        Falhas.append(f"bot rc={Execucao['rc_bot']}")
    for Nome, Texto in Erros.items():
        if Texto.strip():
            Falhas.append(f"stderr do {Nome} nao esta vazio: {Texto.strip()[:200]!r}")
    for Linha in (TextoServ + "\n" + TextoBot).splitlines():
        if "FALHA" in Linha:
            Falhas.append(Linha)
    if "bot OK" not in TextoBot:
        Falhas.append("bot sem a linha 'bot OK'")
    return Falhas


def AnalisarSaidas(Pasta, Sufixo, Execucao, Config):
    Ticks = Config["ticks"]
    Alvo = Config["alvo"]
    TextoServ = LerSaida(Pasta / f"serv_out_{Sufixo}.txt", Alvo)
    TextoBot = LerSaida(Pasta / f"bot_out_{Sufixo}.txt", Alvo)
    Erros = {"servidor": LerSaida(Pasta / f"serv_err_{Sufixo}.txt", Alvo), "bot": LerSaida(Pasta / f"bot_err_{Sufixo}.txt", Alvo)}
    Falhas = ConferirSaidas(Execucao, TextoServ, TextoBot, Erros, Ticks)
    Fins = REGEX_FIM.findall(TextoServ)
    if not Fins:
        Falhas.append("servidor sem linha FIM")
        return ResumoVazio(Falhas)
    Hash, FimTicks, Perdidos, JitterMax, JitterP99 = Fins[-1]
    Hashes = REGEX_HASH.findall(TextoServ)
    if int(FimTicks) != Ticks:
        Falhas.append(f"FIM com ticks={FimTicks} (esperado {Ticks})")
    if int(Perdidos) != 0:
        Falhas.append(f"pacotes perdidos={Perdidos}")
    if not Hashes or Hashes[-1] != (str(Ticks), Hash):
        Falhas.append(f"ultimo hash nao bate com o FIM ({Hash})")
    Picos = [(int(Bytes), int(Minuto)) for Bytes, Minuto in REGEX_PICO.findall(TextoServ)]
    return {"falhas": Falhas, "hash": Hash, "perdidos": int(Perdidos), "jitter_max_ms": JitterMax, "jitter_p99_ms": JitterP99, "picos": Picos, "privados": Execucao["privados"], "notas": [], "duracao": Execucao["duracao"], "ticks_por_s": Ticks / Execucao["duracao"]}


def RodarUma(Exes, Config, Workers, Pasta):
    Execucao = ExecutarPar(Exes, Config, Workers, Pasta)
    if "erro" in Execucao:
        return ResumoVazio([Execucao["erro"]])
    Resumo = AnalisarSaidas(Pasta, NomeRodada(Config, Workers), Execucao, Config)
    if Config["ram_plana"]:
        FalhasRam, NotasRam = AvaliarRam(Resumo)
        Resumo["falhas"] += FalhasRam
        Resumo["notas"] = NotasRam
    return Resumo


def AvaliarPrivado(Resumo):
    Privados = [Bytes for Bytes, Minuto in Resumo["privados"]]
    if len(Privados) < MINIMO_AMOSTRAS_RAM:
        return [], [f"regra (1) nao aplicavel: {len(Privados)} amostra(s) de PrivateUsage (precisa de {MINIMO_AMOSTRAS_RAM}+)"]
    # Privados[1] e o minuto 2: a amostra do minuto n fica na posicao n-1
    if Privados[-1] > FATOR_RAM * Privados[1]:
        return [f"RAM nao plana: PrivateUsage do ultimo minuto {Privados[-1]} > {FATOR_RAM} x {Privados[1]} (minuto 2)"], []
    return [], []


def AvaliarPlato(Resumo):
    Duracao = Resumo["duracao"]
    if Duracao is None or Duracao < MINUTOS_MINIMOS_PLATO * 60:
        return [], [f"trava de plato nao aplicavel: servidor rodou menos de {MINUTOS_MINIMOS_PLATO} min"]
    Ultimos = [Bytes for Bytes, Minuto in Resumo["picos"][-JANELA_PLATO_MINUTOS:]]
    if len(Ultimos) < JANELA_PLATO_MINUTOS:
        return [f"trava de plato: so {len(Ultimos)} pico(s) por minuto impresso(s), precisa de {JANELA_PLATO_MINUTOS}"], []
    if len(set(Ultimos)) > 1:
        return [f"trava de plato: picos dos {JANELA_PLATO_MINUTOS} ultimos minutos nao sao iguais: {Ultimos}"], []
    return [], []


def AvaliarRam(Resumo):
    FalhasPrivado, NotasPrivado = AvaliarPrivado(Resumo)
    FalhasPlato, NotasPlato = AvaliarPlato(Resumo)
    return FalhasPrivado + FalhasPlato, NotasPrivado + NotasPlato


def ImprimirResumo(Workers, Config, Resumo):
    print(f"w={Workers} ticks={Config['ticks']} dur={Resumo['duracao']} ticks/s={Resumo['ticks_por_s']} hash={Resumo['hash']} perdidos={Resumo['perdidos']} jitter_max_ms={Resumo['jitter_max_ms']} jitter_p99_ms={Resumo['jitter_p99_ms']} picos={Resumo['picos']} privados={Resumo['privados']}")


def Main(Argv):
    Posicionais, Opcoes = LerOpcoes(Argv)
    if Opcoes["alvo"] not in EXTENSAO_DO_ALVO:
        print("FALHA --alvo deve ser windows ou linux")
        return 2
    if not Posicionais:
        print("uso: python -I rodar_servidor.py <forja.exe absoluto> [--workers 1,6] [--segundos 60] [--bots 40] [--vezes 1] [--backend x64|c] [--semente 12345] [--ram-plana] [--alvo windows|linux]")
        return 2
    Forja = Posicionais[0]
    ListaWorkers = [int(Parte) for Parte in Opcoes["workers"].split(",")]
    Config = {"alvo": Opcoes["alvo"], "ticks": int(Opcoes["segundos"]) * TICKS_POR_SEGUNDO, "bots": int(Opcoes["bots"]), "semente": int(Opcoes["semente"]), "ram_plana": Opcoes["ram_plana"]}
    Pasta = pathlib.Path(tempfile.mkdtemp(prefix="p55_servidor_"))
    Extensao = EXTENSAO_DO_ALVO[Config["alvo"]]
    Exes = {"servidor": Pasta / f"servidor{Extensao}", "bot": Pasta / f"bot{Extensao}"}
    for Fonte, Saida in ((PastaFixtures / "servidor.fj", Exes["servidor"]), (PastaFixtures / "bot.fj", Exes["bot"])):
        Erro = Compilar(Forja, Fonte, Saida, FlagsDoAlvo(Opcoes))
        if Erro:
            print(f"FALHA {Erro}")
            return 1
    if Config["alvo"] == ALVO_LINUX:
        try:
            Exes = CopiarParaWsl(Exes, Pasta)
        except RuntimeError as Exc:
            print(f"FALHA {Exc}")
            return 1
    Falhas = []
    Hashes = []
    for Volta in range(1, int(Opcoes["vezes"]) + 1):
        for Workers in ListaWorkers:
            Resumo = RodarUma(Exes, Config, Workers, Pasta)
            Falhas += [f"w={Workers} volta={Volta}: {Falha}" for Falha in Resumo["falhas"]]
            if Resumo["hash"]:
                Hashes.append(Resumo["hash"])
                ImprimirResumo(Workers, Config, Resumo)
            for Nota in Resumo["notas"]:
                print(f"w={Workers} volta={Volta} NOTA {Nota}")
    if not Hashes:
        Falhas.append("nenhuma rodada deu hash final")
    elif len(set(Hashes)) > 1:
        Falhas.append(f"hash final diverge entre as rodadas: {sorted(set(Hashes))}")
    if Config["alvo"] == ALVO_LINUX:
        wsl_lote.Wsl(["rm", "-rf", DestinoWsl(Pasta)], LIMITE_WSL_SEGUNDOS)
        wsl_lote.Wsl(["rmdir", DestinoWsl(Pasta).rsplit("/", 1)[0]], LIMITE_WSL_SEGUNDOS)  # so apaga se vazia
    for Falha in Falhas:
        print("FALHA", Falha)
    print(f"alvo={Config['alvo']} ticks={Config['ticks']} rodadas_com_hash={len(Hashes)} falhas={len(Falhas)} pasta={Pasta}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
