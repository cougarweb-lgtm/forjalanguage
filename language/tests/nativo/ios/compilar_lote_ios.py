#!/usr/bin/env python3
"""compilar_lote_ios.py - I4: compila o conjunto linux inteiro (dev e release) para aarch64-apple-ios-sim, confere o Mach-O de cada binario e monta o
pacote de prova (tar.xz solido) que o runner do simulador executa.

Objetivo: (1) enumerar as tarefas do conjunto linux (tests\\nativo\\linux\\conjunto_linux.TodoOConjunto) e compilar todas em paralelo com o candidato;
registrar por tarefa `compilou` ou a recusa nomeada (codigo + causa); (2) verificar_macho em 100% dos binarios gerados (parser em-processo, sem desmontar tudo;
`svc` contado por varredura de palavras de 32 bits do __text e por capstone numa amostra de 50); (3) resolver o esperado (stdout/exit/R-code) pelo
proprio caso ou pelo oraculo x86_64-linux (cache do verificar_a64 ou rodada no WSL); (4) gravar dois pacotes: dirigido (D1..D4) e inteiro.
Formato do pacote (contrato com o runner): pacote_i4/<base>_dev|_release + sidecars <binario>.X ou <base>.X: .esperado.stdout, .esperado.exit,
.esperado.stderr_contem, .args (1 por linha), .env (K=V por linha), .stdin, .timeout; SHA256SUMS.txt, manifesto.json, canarios_esperados.txt.
Uso: python -I compilar_lote_ios.py --forja <forja_ios_i4.exe> [--limite N] [--jobs N] [--conferir <pacote.tar.xz>] [--selftest]
Verificacao: python -I compilar_lote_ios.py --selftest   (provas puras: saneamento, sidecars, conferidor com tar plantado e adulterado)
Depende de: capstone (opcional, so a amostra de `svc`).
Versao: 0.1.1
Changelog: 0.1.1 (23/11/2026) I4f - PULADOS_PLATAFORMA (fonte com caminho Linux, ex.: /mnt).
           0.1.0 (23/11/2026) I4 P1 - criacao.
"""
import argparse
import hashlib
import io
import json
import os
import re
import struct
import subprocess
import sys
import tarfile
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
sys.path.insert(0, str(AQUI))
sys.path.insert(0, str(LANGUAGE / "tests" / "nativo" / "a64"))
sys.path.insert(0, str(LANGUAGE / "tests" / "nativo" / "linux"))
import verificar_macho  # noqa: E402
import verificar_a64  # noqa: E402
import conjunto_linux  # noqa: E402

ALVO = "aarch64-apple-ios-sim"
FORJA_ORACULO = LANGUAGE / "compiler" / "forja.exe"
PASTA_I3 = LANGUAGE / "gaveta" / "ios_prova" / "fixos_i3"  # os 10 programas da I3 recompilados com o candidato (mkfix); o `pacote\` e sobrescrito por ci_rodar_pacote.ps1
PASTA_LOTE = LANGUAGE / "gaveta" / "ios_prova" / "lote"
CARGA_ABYSSAL = LANGUAGE / "gaveta" / "carga_wasm_abyssal"
SHA_CARGA_ABYSSAL = "a095cccf1bf5ef2e79ab3a442ce43de6edc6e2c2ee10fb8c4bc9f94f65779179"
TOPO = "pacote_i4"
TEMPO_BUILD = 120
TEMPO_ORACULO = 20
TAMANHO_PAGINA = 16384
AMOSTRA_SVC = 50
SVC_MASCARA = 0xFFE0001F
SVC_VALOR = 0xD4000001
MODOS = ("dev", "release")
# sinal do NaN do hardware (x86 negativo, ARM positivo): aceita nomeada do a64 (nan_bits); o esperado do oraculo x86 nao serve no ARM
PREFIXOS_NAN_BITS = ("lowering/366_", "lowering/367_", "lowering/368_", "lowering/370_", "lowering/372_")
# I4f: casos cujo FONTE depende de caminho do Linux (inerente ao sistema; nomeado, nunca esconde falha do alvo)
PULADOS_PLATAFORMA = {
    "pasta/read_dir_mnt": "fonte com caminho do Linux (/mnt/c/Users/...): o simulador iOS nao tem /mnt",
}
REGEX_IMPORT_PONTE =re.compile(r'import\s+"std:(file|os|time|rand|process|mem|path|thread|net)\b')
REGEX_PONTE = re.compile(r'\bforeign\b|vendor:|stdin')
REGEX_RCODE = re.compile(r'"codigo": "(R\d{4})"')
GRUPOS_D1 = ("arquivo", "pasta", "proc", "prova", "pontes", "traco")


def Sanear(Nome: str) -> str:
    """Id da tarefa em [a-z0-9_]: o que nao for letra/digito vira `_`, repeticoes e pontas saem; limite de 70 caracteres."""
    return re.sub(r"[^a-z0-9]+", "_", Nome.lower()).strip("_")[:70].strip("_")


def BasesUnicas(Nomes: list) -> dict:
    """Nome do caso -> base saneada; colisao (dois nomes no mesmo saneamento) ganha sufixo _2, _3."""
    Usadas, Mapa = {}, {}
    for Nome in Nomes:
        if Nome in Mapa:
            continue
        Base = Sanear(Nome) or "caso"
        Contagem = Usadas.get(Base, 0) + 1
        Usadas[Base] = Contagem
        Mapa[Nome] = Base if Contagem == 1 else f"{Base}_{Contagem}"
    return Mapa


def Sha256(Dados: bytes) -> str:
    return hashlib.sha256(Dados).hexdigest()


def MotivoNaoEmpacotavel(Caso) -> str:
    """Vazio = o runner consegue montar o caso so com sidecars; senao o motivo (pulado nomeado, nao falha)."""
    if Caso.Modo not in MODOS:
        return "modo teste: forja test nao tem alvo iOS (X0003)"
    if Caso.Dados or Caso.Pre or Caso.Pos or Caso.Usuario:
        return "Dados/Pre/Pos/Usuario fora do contrato de sidecars do runner"
    if any(A == "" or "\n" in A or "@D@" in A for A in Caso.Args):
        return "argumento vazio, com quebra de linha ou @D@ (pasta de trabalho) nao cabe em .args"
    if any("\n" in V or "=" in K for K, V in Caso.Env.items()):
        return "variavel de ambiente com quebra de linha nao cabe em .env"
    if Caso.Nome.startswith(PREFIXOS_NAN_BITS):
        return "nan_bits: sinal do NaN difere entre x86 (esperado) e ARM (aceita do a64)"
    return PULADOS_PLATAFORMA.get(Caso.Nome, "")


def CompilarUma(Forja: Path, Caso, Base: str, Pasta: Path) -> dict:
    """Compila um caso no modo dele; devolve o registro {compilou|recusa|falha, codigo, causa, obj}."""
    Arquivo = Pasta / f"{Base}_{Caso.Modo}"
    Release = ["--release"] if Caso.Modo == "release" else []
    Comando = [str(Forja), "build", "--target", ALVO, *Release, str(Caso.Fonte), "-o", str(Arquivo)]
    try:
        Processo = subprocess.run(Comando, capture_output=True, timeout=TEMPO_BUILD, stdin=subprocess.DEVNULL, cwd=LANGUAGE)
    except subprocess.TimeoutExpired:
        return {"estado": "falha", "codigo": "", "causa": f"build passou de {TEMPO_BUILD} s", "obj": ""}
    Texto = (Processo.stdout + Processo.stderr).decode("utf-8", "replace")
    if Processo.returncode == 0 and Arquivo.is_file():
        return {"estado": "compilou", "codigo": "", "causa": "", "obj": str(Arquivo)}
    Achado = verificar_a64.REGEX_CODIGO.search(Texto) or verificar_a64.REGEX_FRENTE.search(Texto)
    Codigo = Achado.group(1) if Achado else ""
    Causa = verificar_a64.CausaDoDiagnostico(Texto, Codigo or None)
    Estado = "recusa" if Codigo else "falha"
    return {"estado": Estado, "codigo": Codigo, "causa": Causa or Texto.replace("\n", " | ")[:200], "obj": ""}


def ContarSvc(Dados: bytes, Info: dict) -> int:
    """`svc` por varredura das palavras alinhadas do __text (instrucao 0xD4000001 com imm16 livre)."""
    Texto = [S for S in Info["secs"] if S["nome"] == "__text"][0]
    Corpo = memoryview(Dados[Texto["off"]:Texto["off"] + (Texto["size"] & ~3)]).cast("I")
    return sum(1 for P in Corpo if (P & SVC_MASCARA) == SVC_VALOR)


def FormaDoMacho(Dados: bytes, Info: dict, Binds: list, Rebases: list) -> dict:
    """Classe de forma do binario (D3): paginas do __TEXT e do __DATA, rebases, bss."""
    Seg = {S["nome"]: S for S in Info["segs"]}
    Dado = Seg.get("__DATA", {"filesize": 0, "vmsize": 0})
    return {"tam": len(Dados), "texto_pag": Seg["__TEXT"]["filesize"] // TAMANHO_PAGINA, "dado_pag": Dado["vmsize"] // TAMANHO_PAGINA,
            "rebases": len(Rebases), "bss": int(Dado["vmsize"] > Dado["filesize"]), "binds": len(Binds), "svc": ContarSvc(Dados, Info)}


def VerificarMacho(Caminho: str) -> dict:
    """verificar_macho em-processo, sem imprimir: devolve {falhas, forma}."""
    Dados = Path(Caminho).read_bytes()
    Fal = verificar_macho.Falhas()
    try:
        Info = verificar_macho.LerMacho(Dados)
        verificar_macho.VerificarEstrutura(Dados, Info, Fal)
        verificar_macho.VerificarAssinatura(Dados, Info, os.path.basename(Caminho), Fal)
        Binds, Rebases = verificar_macho.DecodificarFixups(Dados, Info, Fal)
        Forma = FormaDoMacho(Dados, Info, Binds, Rebases)
    except (struct.error, IndexError, KeyError, ValueError) as Erro:
        return {"falhas": [f"parser: {Erro!r}"], "forma": {}}
    return {"falhas": Fal.Lista, "forma": Forma}


def AmostraSvcCapstone(Caminhos: list) -> dict:
    """Desmonta com capstone ate AMOSTRA_SVC binarios (espacados) e conta mnemonico `svc`; sem capstone devolve {}."""
    if verificar_macho.capstone is None or not Caminhos:
        return {}
    Passo = max(1, len(Caminhos) // AMOSTRA_SVC)
    Resultado = {}
    for Caminho in Caminhos[::Passo][:AMOSTRA_SVC]:
        Dados = Path(Caminho).read_bytes()
        Info = verificar_macho.LerMacho(Dados)
        Texto = [S for S in Info["secs"] if S["nome"] == "__text"][0]
        Md = verificar_macho.capstone.Cs(verificar_macho.capstone.CS_ARCH_ARM64, verificar_macho.capstone.CS_MODE_ARM)
        Corpo = Dados[Texto["off"]:Texto["off"] + Texto["size"]]
        Resultado[Caminho] = sum(1 for L in Md.disasm(Corpo, Texto["addr"]) if L.mnemonic == "svc")
    return Resultado


def CompilarOraculo(Obra_, Pasta: Path) -> None:
    """Compila o caso para x86_64-linux (oraculo) quando o cache nao tem a saida; erro de front-end fica em Obra_.CodigoOraculo."""
    Obra_.Oraculo = Pasta / f"o{Obra_.Indice}.elf"
    Caso = Obra_.Tarefa_.Caso
    Falha = verificar_a64.Compilar(verificar_a64.ComandoForja(FORJA_ORACULO, "x86_64-linux", Caso.Modo, Caso.Fonte, Obra_.Oraculo), Obra_.Oraculo, TEMPO_BUILD)
    if Falha:
        Obra_.Registro = {"falha_oraculo": Falha.Causa or Falha.Erro}
        Obra_.CodigoOraculo = Falha.Codigo


def ResolverOraculos(Casos: list, Pasta: Path, Jobs: int) -> dict:
    """{(Nome, Modo): Saida} do oraculo x86_64-linux dos casos sem esperado proprio: cache do verificar_a64, senao compila + roda no WSL (e grava no cache)."""
    Pasta.mkdir(parents=True, exist_ok=True)
    Sha = verificar_a64.ShaDoArquivo(FORJA_ORACULO)
    Opc = verificar_a64.Opcoes(FORJA_ORACULO, FORJA_ORACULO, Pasta, TEMPO_ORACULO, False, False, Linux=True, Cache=True, ShaForja=Sha)
    Obras = []
    for Indice, Caso in enumerate(C for C in Casos if C.Esperado is None):
        Tarefa = verificar_a64.Tarefa(Caso, Caso.Modo)
        Obra_ = verificar_a64.Obra(Tarefa, Indice, ChaveOraculo=verificar_a64.ChaveDaEntrada(Tarefa, Sha))
        Obra_.SaidaOraculo = verificar_a64.LerCache(Obra_.ChaveOraculo)
        Obras.append(Obra_)
    Faltam = [O for O in Obras if O.SaidaOraculo is None]
    print(f"oraculo: {len(Obras) - len(Faltam)} no cache, {len(Faltam)} a compilar e rodar no WSL", flush=True)
    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        list(Piscina.map(lambda O: CompilarOraculo(O, Pasta), Faltam))
    verificar_a64.RodarOraculos(Faltam, Opc)
    return {(O.Tarefa_.Caso.Nome, O.Tarefa_.Modo): O for O in Obras}


def EsperadoDoCaso(Caso, Obra_) -> dict | None:
    """{stdout, exit, rcode}: do proprio caso (Esperado bytes) ou da saida do oraculo; None = sem esperado utilizavel (nome do motivo em 'motivo')."""
    if Caso.Esperado is not None:
        Codigo = re.search(r"R\d{4}", Caso.ErroTem or "")
        return {"stdout": Caso.Esperado, "exit": Caso.Exit, "rcode": Codigo.group(0) if Codigo else None}
    Saida = Obra_.SaidaOraculo if Obra_ else None
    if Saida is None or Saida.Motivo or Saida.Exit is None:
        return {"motivo": f"oraculo sem saida ({'recusa ' + str(Obra_.CodigoOraculo) if Obra_ and Obra_.CodigoOraculo else (Saida.Motivo if Saida else 'nao rodou')})"}
    Codigo = REGEX_RCODE.search(Saida.Stderr.decode("utf-8", "replace"))
    return {"stdout": Saida.Stdout, "exit": Saida.Exit, "rcode": Codigo.group(1) if Codigo else None}


def Sidecars(Caso, Esperado: dict) -> dict:
    """Sidecars por binario (sufixo -> bytes) a partir do esperado e dos campos do caso."""
    Saida = {"esperado.stdout": Esperado["stdout"], "esperado.exit": str(Esperado["exit"]).encode()}
    if Esperado["rcode"]:
        Saida["esperado.stderr_contem"] = Esperado["rcode"].encode()
    if Caso.Args:
        Saida["args"] = ("\n".join(Caso.Args) + "\n").encode("utf-8")
    if Caso.Env:
        Saida["env"] = ("\n".join(f"{K}={V}" for K, V in Caso.Env.items()) + "\n").encode("utf-8")
    if Caso.Entrada is not None:
        Saida["stdin"] = Caso.Entrada
    return Saida


def EhPonte(Caso) -> bool:
    """D1: tarefa de ponte (grupo de arquivo/pasta/proc..., Args/Env/Entrada, ou fonte com std:file/os/time/rand/process/mem/path/thread/net, foreign, vendor, stdin)."""
    if Caso.Nome.split("/")[0] in GRUPOS_D1 or Caso.Args or Caso.Env or Caso.Entrada is not None:
        return True
    Texto = Caso.Fonte.read_text(encoding="utf-8", errors="replace")
    return bool(REGEX_IMPORT_PONTE.search(Texto) or REGEX_PONTE.search(Texto))


def EscolherForma(Formas: dict) -> set:
    """D3: {Nome} do resto puro: 1 por classe (paginas do __TEXT, do __DATA, faixa de rebases, bss) mais os extremos (maiores, mais rebases, menores)."""
    Classes, Escolhidos = {}, set()
    for Nome in sorted(Formas):
        F = Formas[Nome]
        Classes.setdefault((F["texto_pag"], F["dado_pag"], F["rebases"].bit_length(), F["bss"]), Nome)
    Escolhidos.update(Classes.values())
    PorTam = sorted(Formas, key=lambda N: Formas[N]["tam"])
    PorRebase = sorted(Formas, key=lambda N: Formas[N]["rebases"])
    Escolhidos.update(PorTam[-15:] + PorTam[:5] + PorRebase[-10:])
    return Escolhidos


def Conjuntos(Itens: list) -> dict:
    """Itens = [{caso, esperado, forma, ...}] por (Nome, Modo) rodaveis -> {Nome: {D1,D2,D3}} por nome (os dois modos entram juntos)."""
    PorNome = {}
    for It in Itens:
        Marcas = PorNome.setdefault(It["caso"].Nome, set())
        if EhPonte(It["caso"]):
            Marcas.add("D1")
        if It["esperado"]["exit"] != 0 or It["esperado"]["rcode"]:
            Marcas.add("D2")
    Puros = {It["caso"].Nome: It["forma"] for It in Itens if not PorNome[It["caso"].Nome] and It["caso"].Modo == "dev" and It["forma"]}
    for Nome in EscolherForma(Puros):
        PorNome[Nome].add("D3")
    return PorNome


def ArquivosDoItem(Item: dict) -> dict:
    """relpath -> bytes do binario e dos sidecars do item (por binario)."""
    Nome = f"{Item['base']}_{Item['caso'].Modo}"
    Arquivos = {Nome: Path(Item["obj"]).read_bytes()}
    for Sufixo, Dados in Sidecars(Item["caso"], Item["esperado"]).items():
        Arquivos[f"{Nome}.{Sufixo}"] = Dados
    return Arquivos


def CompilarFixo(Forja: Path, Fonte: Path, Base: str, Modo: str, Pasta: Path) -> bytes:
    """Compila uma fonte fixa (D4) para o iOS; falha do build derruba a rodada."""
    Saida = Pasta / f"{Base}_{Modo}"
    Release = ["--release"] if Modo == "release" else []
    Processo = subprocess.run([str(Forja), "build", "--target", ALVO, *Release, str(Fonte), "-o", str(Saida)], capture_output=True, cwd=LANGUAGE, timeout=TEMPO_BUILD)
    if Processo.returncode != 0 or not Saida.is_file():
        raise SystemExit(f"fixo {Base} {Modo}: build falhou: {(Processo.stdout + Processo.stderr).decode('utf-8', 'replace')[:300]}")
    return Saida.read_bytes()


def ArquivosFixos(Forja: Path, Pasta: Path) -> tuple:
    """D4: os 20 binarios da I3 (como estao), a carga do Abyssal (dev e release, sha conferido) e os 2 canarios. Devolve (arquivos, casos do manifesto, canarios)."""
    Arquivos, Casos = {}, []
    for Arquivo in sorted(PASTA_I3.iterdir()):
        if Arquivo.name != "SHA256SUMS.txt":
            Arquivos[Arquivo.name] = Arquivo.read_bytes()
    for Base in sorted({N.rsplit("_", 1)[0] for N in Arquivos if N.endswith(("_dev", "_release"))}):
        Casos.append({"id": Base, "origem": "i3", "grupo": "D4", "conjuntos": ["D4"], "modos": list(MODOS)})
    Esperado = (CARGA_ABYSSAL / "esperado.txt").read_bytes()
    if Sha256(Esperado) != SHA_CARGA_ABYSSAL or len(Esperado) != 493:
        raise SystemExit("esperado.txt da carga do Abyssal nao bate com o sha/tamanho do plano")
    for Modo in MODOS:
        Arquivos[f"carga_abyssal_{Modo}"] = CompilarFixo(Forja, CARGA_ABYSSAL / "carga_abyssal.fj", "carga_abyssal", Modo, Pasta)
    Arquivos["carga_abyssal.esperado.stdout"] = Esperado
    Arquivos["carga_abyssal.esperado.exit"] = b"0"
    Casos.append({"id": "carga_abyssal", "origem": "carga_wasm_abyssal", "grupo": "D4", "conjuntos": ["D4"], "modos": list(MODOS)})
    Canarios = AdicionarCanarios(Arquivos, Casos)
    return Arquivos, Casos, Canarios


def AdicionarCanarios(Arquivos: dict, Casos: list) -> list:
    """2 canarios: o binario `ola` da I3 com stdout errado e com exit errado (o runner tem de reprova-los)."""
    Certo = Arquivos["ola.esperado.stdout"]
    Variantes = {"canario_stdout_errado": (b"ESPERADO PROPOSITALMENTE ERRADO\n", Arquivos["ola.esperado.exit"]), "canario_exit_errado": (Certo, b"99")}
    for Base, (Stdout, Saida) in Variantes.items():
        for Modo in MODOS:
            Arquivos[f"{Base}_{Modo}"] = Arquivos[f"ola_{Modo}"]
        Arquivos[f"{Base}.esperado.stdout"] = Stdout
        Arquivos[f"{Base}.esperado.exit"] = Saida
        Casos.append({"id": Base, "origem": "canario", "grupo": "D4", "conjuntos": ["D4"], "modos": list(MODOS)})
    return list(Variantes)


def GravarTar(Caminho: Path, Arquivos: dict) -> str:
    """tar.xz SOLIDO (um fluxo xz so), ordem alfabetica, mtime 0, dono 0; binarios 0755, o resto 0644. Devolve o sha256 do .tar.xz."""
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(Caminho, "w:xz", preset=9) as Tar:
        for Rel in sorted(Arquivos):
            Dados = Arquivos[Rel]
            Info = tarfile.TarInfo(f"{TOPO}/{Rel}")
            Info.size, Info.mtime, Info.uid, Info.gid = len(Dados), 0, 0, 0
            Info.mode = 0o755 if Rel.endswith(("_dev", "_release")) else 0o644
            Tar.addfile(Info, io.BytesIO(Dados))
    return Sha256(Caminho.read_bytes())


def MontarPacote(Selecionados: list, Fixos: tuple, ShaForja: str) -> dict:
    """Arquivos do pacote (sem o tar): casos selecionados + D4 + SHA256SUMS.txt + manifesto.json + canarios_esperados.txt."""
    Arquivos, Manifesto = dict(Fixos[0]), list(Fixos[1])
    for Item in Selecionados:
        Novos = ArquivosDoItem(Item)
        if set(Novos) & set(Arquivos):
            raise SystemExit(f"colisao de nome no pacote: {sorted(set(Novos) & set(Arquivos))[:3]}")
        Arquivos.update(Novos)
        Manifesto.append({"id": Item["base"], "nome": Item["caso"].Nome, "origem": Item["caso"].Nome.split("/")[0], "grupo": Item["grupo"],
                          "conjuntos": Item["conjuntos"], "modo": Item["caso"].Modo, "forja_sha": ShaForja})
    Arquivos["manifesto.json"] = json.dumps({"alvo": ALVO, "forja_sha": ShaForja, "casos": Manifesto}, indent=1, ensure_ascii=False).encode("utf-8")
    Arquivos["canarios_esperados.txt"] = ("\n".join(Fixos[2]) + "\n").encode("utf-8")
    Arquivos["SHA256SUMS.txt"] = "".join(f"{Sha256(D)} *{R}\n" for R, D in sorted(Arquivos.items())).encode("utf-8")
    return Arquivos


def ConferirPacote(Caminho: Path) -> list:
    """Conferidor local: reabre o tar.xz, confere SHA256SUMS.txt (todos os arquivos, sem sobra) e que cada binario tem esperado.exit (<binario>.X ou <base>.X)."""
    Problemas, Arquivos = [], {}
    with tarfile.open(Caminho, "r:xz") as Tar:
        for Membro in Tar.getmembers():
            if Membro.isfile():
                Arquivos[Membro.name.removeprefix(f"{TOPO}/")] = Tar.extractfile(Membro).read()
    Soma = {}
    for Linha in Arquivos.get("SHA256SUMS.txt", b"").decode("utf-8").splitlines():
        Sha, _, Rel = Linha.partition(" *")
        Soma[Rel] = Sha
    for Rel, Dados in Arquivos.items():
        if Rel != "SHA256SUMS.txt" and Soma.get(Rel) != Sha256(Dados):
            Problemas.append(f"sha256 ausente ou diferente: {Rel}")
    Problemas += [f"SHA256SUMS lista arquivo que nao existe: {R}" for R in Soma if R not in Arquivos]
    Binarios = [R for R in Arquivos if R.endswith(("_dev", "_release"))]
    for Rel in Binarios:
        Base = Rel.rsplit("_", 1)[0]
        if f"{Rel}.esperado.exit" not in Arquivos and f"{Base}.esperado.exit" not in Arquivos:
            Problemas.append(f"binario sem esperado.exit: {Rel}")
    for Base in Arquivos.get("canarios_esperados.txt", b"").decode("utf-8").split():
        if f"{Base}_dev" not in Arquivos or f"{Base}_release" not in Arquivos:
            Problemas.append(f"canario sem binario: {Base}")
    Problemas += [] if "manifesto.json" in Arquivos else ["sem manifesto.json"]
    return Problemas


def Processar(Forja: Path, Caso, Base: str, Pasta: Path) -> dict:
    """Compila o caso e, se compilou, confere o Mach-O; um registro por (caso, modo)."""
    Registro = {"caso": Caso, "base": Base, "nome": Caso.Nome, "modo": Caso.Modo, "forma": {}, "falhas_macho": []}
    if Caso.Modo not in MODOS:
        return {**Registro, "estado": "pulado", "codigo": "", "causa": MotivoNaoEmpacotavel(Caso), "obj": ""}
    Registro.update(CompilarUma(Forja, Caso, Base, Pasta))
    if Registro["estado"] == "compilou":
        Macho = VerificarMacho(Registro["obj"])
        Registro["falhas_macho"], Registro["forma"] = Macho["falhas"], Macho["forma"]
    return Registro


def CompilarTudo(Forja: Path, Casos: list, Pasta: Path, Jobs: int) -> list:
    """Compila e verifica todos os casos em paralelo (uma thread por nucleo: o forja roda em processo filho)."""
    Pasta.mkdir(parents=True, exist_ok=True)
    Bases = BasesUnicas([C.Nome for C in Casos])
    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        return list(Piscina.map(lambda C: Processar(Forja, C, Bases[C.Nome], Pasta), Casos))


def ResumirCompilacao(Registros: list) -> dict:
    """Contagens: por estado, recusas por codigo + causa, falhas, Mach-O e `svc`."""
    Compilados = [R for R in Registros if R["estado"] == "compilou"]
    Recusas = {}
    for R in Registros:
        if R["estado"] in ("recusa", "falha"):
            Chave = f"{R['codigo'] or 'sem codigo'}: {R['causa'][:100]}"
            Recusas[Chave] = Recusas.get(Chave, 0) + 1
    Amostra = AmostraSvcCapstone([R["obj"] for R in Compilados])
    return {"tarefas": len(Registros), "compilou": len(Compilados), "recusa": sum(1 for R in Registros if R["estado"] == "recusa"),
            "falha": sum(1 for R in Registros if R["estado"] == "falha"), "pulado_modo": sum(1 for R in Registros if R["estado"] == "pulado"),
            "recusas_por_causa": dict(sorted(Recusas.items(), key=lambda P: -P[1])),
            "macho_ok": sum(1 for R in Compilados if not R["falhas_macho"]), "macho_falhas": [f"{R['nome']} {R['modo']}: {R['falhas_macho'][:2]}" for R in Compilados if R["falhas_macho"]],
            "svc_varredura_total": sum(R["forma"].get("svc", 0) for R in Compilados), "svc_amostra_capstone": {"binarios": len(Amostra), "svc": sum(Amostra.values())}}


def ItensRodaveis(Registros: list, Obras: dict) -> tuple:
    """(itens, pulados): item = registro + esperado resolvido; pulado = {nome, modo, motivo} (nomeado) dos compilados que o runner nao roda."""
    Itens, Pulados = [], []
    for R in Registros:
        if R["estado"] != "compilou" or R["falhas_macho"]:
            continue
        Caso = R["caso"]
        Motivo = MotivoNaoEmpacotavel(Caso)
        Esperado = None if Motivo else EsperadoDoCaso(Caso, Obras.get((Caso.Nome, Caso.Modo)))
        if Esperado and "motivo" in Esperado:
            Motivo, Esperado = Esperado["motivo"], None
        if Motivo:
            Pulados.append({"nome": Caso.Nome, "modo": Caso.Modo, "motivo": Motivo})
            continue
        Itens.append({**R, "esperado": Esperado})
    return Itens, Pulados


def Selecionar(Itens: list, PorNome: dict, Inteiro: bool) -> list:
    """Itens do pacote: todos (inteiro) ou os que tem marca D1/D2/D3 (dirigido); acrescenta `grupo` e `conjuntos`."""
    Saida = []
    for It in Itens:
        Marcas = sorted(PorNome[It["caso"].Nome])
        if Inteiro or Marcas:
            Saida.append({**It, "grupo": It["caso"].Nome.split("/")[0], "conjuntos": Marcas or ["resto"]})
    return Saida


def ContarPorMotivo(Pulados: list) -> dict:
    Contagem = {}
    for P in Pulados:
        Contagem[P["motivo"][:110]] = Contagem.get(P["motivo"][:110], 0) + 1
    return dict(sorted(Contagem.items(), key=lambda P: -P[1]))


def GravarPacotes(Itens: list, ShaForja: str, Forja: Path, Pasta: Path) -> dict:
    """Grava pacote_i4_dirigido.tar.xz e pacote_i4_inteiro.tar.xz em PASTA_LOTE e confere cada um com o conferidor local."""
    PorNome = Conjuntos(Itens)
    Fixos = ArquivosFixos(Forja, Pasta)
    Resultado = {}
    for Nome, Inteiro in (("dirigido", False), ("inteiro", True)):
        Selecionados = Selecionar(Itens, PorNome, Inteiro)
        Arquivos = MontarPacote(Selecionados, Fixos, ShaForja)
        Caminho = PASTA_LOTE / f"pacote_i4_{Nome}.tar.xz"
        Sha = GravarTar(Caminho, Arquivos)
        Resultado[Nome] = {"arquivo": Caminho.name, "sha256": Sha, "tamanho_mb": round(Caminho.stat().st_size / 1048576, 2), "casos_rodaveis": len(Selecionados),
                           "binarios": sum(1 for R in Arquivos if R.endswith(("_dev", "_release"))), "conferidor": ConferirPacote(Caminho) or "OK",
                           "por_conjunto": {M: sum(1 for S in Selecionados if M in S["conjuntos"]) for M in ("D1", "D2", "D3", "resto")}}
    return Resultado


def Executar(Argumentos) -> int:
    Forja = Path(Argumentos.forja).resolve()
    Casos = conjunto_linux.TodoOConjunto()
    Casos = Casos[:Argumentos.limite] if Argumentos.limite else Casos
    Pasta = Path(tempfile.gettempdir()) / "ios_lote_i4"
    Jobs = Argumentos.jobs or os.cpu_count() or 4
    Registros = CompilarTudo(Forja, Casos, Pasta / "obj", Jobs)
    Resumo = ResumirCompilacao(Registros)
    print(json.dumps({K: V for K, V in Resumo.items() if K != "recusas_por_causa"}, indent=1, ensure_ascii=False), flush=True)
    Rodaveis = [R["caso"] for R in Registros if R["estado"] == "compilou" and not MotivoNaoEmpacotavel(R["caso"])]
    Obras = ResolverOraculos(Rodaveis, Pasta / "oraculo", Jobs)
    Itens, Pulados = ItensRodaveis(Registros, Obras)
    Resumo.update({"forja": Forja.name, "forja_sha256": Sha256(Forja.read_bytes()), "rodaveis": len(Itens), "pulados_nomeados": ContarPorMotivo(Pulados)})
    Resumo["pacotes"] = GravarPacotes(Itens, Resumo["forja_sha256"], Forja, Pasta / "obj")
    PASTA_LOTE.mkdir(parents=True, exist_ok=True)
    (PASTA_LOTE / "manifesto_resumo_i4.json").write_text(json.dumps(Resumo, indent=1, ensure_ascii=False), encoding="utf-8")
    (Pasta / "registros.json").write_text(json.dumps([{K: V for K, V in R.items() if K != "caso"} for R in Registros], indent=0, default=str), encoding="utf-8")
    print(json.dumps({"rodaveis": Resumo["rodaveis"], "pacotes": Resumo["pacotes"], "pulados_nomeados": Resumo["pulados_nomeados"]}, indent=1, ensure_ascii=False))
    Falhou = Resumo["falha"] or Resumo["macho_falhas"] or any(P["conferidor"] != "OK" for P in Resumo["pacotes"].values())
    return 1 if Falhou else 0


def ProvaPura() -> list:
    """Selftest sem compilador: saneamento, bases unicas, sidecars e o conferidor com tar plantado (verde) e adulterado (reprova)."""
    Falhas = []
    if Sanear("arquivo/erros r nome a<b (x)") != "arquivo_erros_r_nome_a_b_x":
        Falhas.append("Sanear")
    if BasesUnicas(["a/b c", "a/b-c", "a/b c"]) != {"a/b c": "a_b_c", "a/b-c": "a_b_c_2"}:
        Falhas.append("BasesUnicas")
    Caso = type("C", (), {"Args": ["x", "y z"], "Env": {"K": "v"}, "Entrada": b"in"})()
    Lado = Sidecars(Caso, {"stdout": b"o", "exit": 3, "rcode": "R0006"})
    if set(Lado) != {"esperado.stdout", "esperado.exit", "esperado.stderr_contem", "args", "env", "stdin"} or Lado["args"] != b"x\ny z\n":
        Falhas.append("Sidecars")
    Base = {"a_dev": b"\x01", "a_release": b"\x02", "a.esperado.exit": b"0", "a.esperado.stdout": b"", "canarios_esperados.txt": b"a\n", "manifesto.json": b"{}"}
    Base["SHA256SUMS.txt"] = "".join(f"{Sha256(D)} *{R}\n" for R, D in sorted(Base.items())).encode()
    with tempfile.TemporaryDirectory() as Tmp:
        Verde = Path(Tmp) / "v.tar.xz"
        GravarTar(Verde, Base)
        if ConferirPacote(Verde):
            Falhas.append(f"conferidor reprovou o pacote bom: {ConferirPacote(Verde)}")
        Ruim = Path(Tmp) / "r.tar.xz"
        GravarTar(Ruim, {**Base, "a_dev": b"\x09"})
        if not any("sha256" in P for P in ConferirPacote(Ruim)):
            Falhas.append("conferidor nao acusou binario adulterado")
        Sem = {K: V for K, V in Base.items() if K != "a.esperado.exit"}
        Sem["SHA256SUMS.txt"] = "".join(f"{Sha256(D)} *{R}\n" for R, D in sorted(Sem.items()) if R != "SHA256SUMS.txt").encode()
        GravarTar(Path(Tmp) / "s.tar.xz", Sem)
        if not any("esperado.exit" in P for P in ConferirPacote(Path(Tmp) / "s.tar.xz")):
            Falhas.append("conferidor nao acusou binario sem esperado.exit")
    return Falhas


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="I4: compila o conjunto linux para aarch64-apple-ios-sim e monta o pacote de prova.")
    Analisador.add_argument("--forja", help="forja com o alvo iOS (ex.: language\\compiler\\forja_ios_i4.exe)")
    Analisador.add_argument("--limite", type=int, default=0, help="so os N primeiros casos (teste rapido)")
    Analisador.add_argument("--jobs", type=int, default=0, help="builds em paralelo (0 = nucleos)")
    Analisador.add_argument("--conferir", help="so confere um .tar.xz (SHA256SUMS e esperado.exit) e sai")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        Falhas = ProvaPura()
        print("compilar_lote_ios selftest:", "VERDE" if not Falhas else Falhas)
        return 1 if Falhas else 0
    if Argumentos.conferir:
        Problemas = ConferirPacote(Path(Argumentos.conferir))
        print("conferir:", "OK" if not Problemas else Problemas[:20])
        return 1 if Problemas else 0
    if not Argumentos.forja:
        Analisador.error("--forja e obrigatorio")
    return Executar(Argumentos)


if __name__ == "__main__":
    sys.exit(Principal())
