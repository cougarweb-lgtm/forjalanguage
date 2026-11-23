# -*- coding: utf-8 -*-
"""Oraculo congelado da bateria nativa: stdout/exit esperados do backend C (gcc) gerados 1x por um forja de referencia fixo e guardados em JSON.

A bateria deixa de rodar gcc a cada rodada: compara o x64 com este arquivo. Chave = sha256(fonte + modulos importados por ele + checagem + args).
Cabecalho do JSON = {sha do forja de referencia, versao do gcc, flags, sha de runtime\\forja_rt.h}; cabecalho mudou = recongelar (--congelar).
Limite herdado do cache_linux: import de pacote do proprio forja (std:, vendor:) nao entra na chave; vale o sha do forja de referencia no cabecalho.

RUNBOOK (qual verificador roda como; tempo manda, rodizio pela fatia N = 0..3):
  100% por fatia : neutralidade byte a byte (verificar_otim --contra ... --sem-passe), determinismo (--determinismo), conferir_regras (+ _cognitiva),
                   verificar_alocador inteiro (o manifesto pula o alvo cujo exe release ja foi aprovado), trap_*, todo alvo cujo exe release mudou.
  amostrados     : fuzz_otim --fatia N (600 de 2400 sementes, so x64 ligado contra este congelado; sem congelado, amostra de 150 com gcc);
                   verificar_otim --amostra 40 --semente N (a semente e o numero da fatia: gira o conjunto); verificar_linux 15% rotativo + alvos da fatia;
                   bateria: so as suites que compilam release x64.
  so no F9       : tudo a 100%, fuzz_otim --completo --tempo 7200, ASan/UBSan, verificar_linux completo.
Uso:
    python oraculo_congelado.py --congelar [--jobs N]     (alocador\\*.fj + sementes 1..2400 do fuzz; so gera o que falta ou o que o cabecalho invalidou)
    python oraculo_congelado.py --verificar               (cabecalho bate? exit 0 sim, 1 exige --congelar)
    python -I oraculo_congelado.py --selftest
API: ObterSaidaEsperada(fonte, checagem, args) -> (stdout, exit); ObterRegistro(...) -> dict {exit, stdout, stderr, invalido}.

SELO (0.21.0 F1 P1, D0720): `--selar` grava no cabecalho {sha256 dos itens, origem, data}. Banco SELADO nao olha gcc, forja_rt.h nem exe de referencia
(o gcc some na 0.21.0), confere o sha dos itens na 1a leitura (adulterado = ErroOraculo) e chave AUSENTE e FALHA (ErroOraculo), nunca regera nem pula:
fixture nova ou editada depois do selo ganha o .esperado a mao (D0707). Lote novo: oraculo_congelado_g1.json (BancoG1; gerado por gaveta\\g1\\forja_g1.exe 0.20.4).
Chave com classe e modo (so entram na chave quando dados, entao as 2400 chaves antigas continuam valendo); modo = release | dev | teste (teste roda `forja test --json`).

BINARIO DE REFERENCIA (0.21.0 F3, D0730): onde o valor e TEMPO (bench, kernels, ELF musl), congela-se o binario gcc, nao o numero. oraculo_congelado_ref.json
(BancoRef, classe binario_gcc, selado) guarda {arquivo, sha256, flags, gcc} de cada binario em ref_gcc\\; BinarioDeReferencia(fonte, plataforma, nivel) devolve o caminho
e falha se a chave, o arquivo ou o sha faltarem. Registro so antes do selo (RegistrarBinarioRef).

Versao: 1.2.0
Changelog: 1.2.0 - 0.21.0 F3: BancoRef, RegistrarBinarioRef, BinarioDeReferencia e selftest do binario de referencia.
           1.1.0 -0.21.0 F1 P1: selo, BancoG1, Modo/Classe na chave, origem nominal em cada registro novo, Congelar recusa banco selado.
           1.0.0 - primeira versao (D0696: oraculo congelado; fixtures do alocador + sementes do fuzz).
Verificacao: python -I oraculo_congelado.py --selftest
"""

import argparse
import base64
import concurrent.futures
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaNativo = PastaAqui.parent
PastaLanguage = PastaAqui.parents[2]
CAMINHO_JSON = PastaAqui / "oraculo_congelado.json"
CAMINHO_JSON_G1 = PastaAqui / "oraculo_congelado_g1.json"
CAMINHO_JSON_REF = PastaAqui / "oraculo_congelado_ref.json"
PASTA_REF = PastaAqui / "ref_gcc"
CLASSE_REF = "binario_gcc"
FORJA_G1 = PastaLanguage / "gaveta" / "g1" / "forja_g1.exe"  # 0.20.4, arquivado antes da aposentadoria do backend C (D0720)
MODOS = ("release", "dev", "teste")
FORJA_REFERENCIA = Path(tempfile.gettempdir()) / "forja_f1_base.exe"  # 0.20.0, o ultimo forja antes do M7b
RUNTIME_RT = PastaLanguage / "runtime" / "forja_rt.h"
FLAGS = "build --backend c --release --checagem <sim|nao>"
FORMATO = 1
TIMEOUT_BUILD = 120
TIMEOUT_EXEC = 60
SEMENTES_FUZZ = range(1, 2401)
CADA_NAO = 4  # igual ao fuzz_otim: 1 de cada 4 sementes sem acesso fora do limite tambem roda com --checagem nao
RE_IMPORT = re.compile(r'^\s*import\s+"([^"]+)"', re.MULTILINE)
MSG_RECONGELAR = "recongele: python tests\\nativo\\rapido\\oraculo_congelado.py --congelar"


class ErroOraculo(RuntimeError):
    pass


def ShaBytes(Dado: bytes) -> str:
    return hashlib.sha256(Dado).hexdigest()


def ModulosImportados(Fonte: Path, Visitados: set | None = None) -> bytes:
    """Nome e conteudo dos .fj importados por caminho relativo (recursivo); pacote do forja (std:, vendor:) fica de fora."""
    Visitados = set() if Visitados is None else Visitados
    Resumo = b""
    for Alvo in RE_IMPORT.findall(Fonte.read_text(encoding="utf-8", errors="replace")):
        Arquivo = Fonte.parent / (Alvo if Alvo.endswith(".fj") else Alvo + ".fj")
        if ":" in Alvo or not Arquivo.is_file() or Arquivo in Visitados:
            continue
        Visitados.add(Arquivo)
        Resumo += Arquivo.name.encode("utf-8") + ShaBytes(Arquivo.read_bytes()).encode("ascii") + ModulosImportados(Arquivo, Visitados)
    return Resumo


def ChaveDe(Fonte, Checagem: str, Args=(), Modo=None, Classe=None) -> str:
    """Fonte = Path (le o arquivo e os modulos que importa) ou str (texto do programa, sem imports). Modo e Classe so entram na chave quando dados (chaves antigas intactas)."""
    Texto, Modulos = (Fonte.read_bytes(), ModulosImportados(Fonte)) if isinstance(Fonte, Path) else (Fonte.encode("utf-8"), b"")
    Campos = {"fonte": ShaBytes(Texto), "modulos": ShaBytes(Modulos), "checagem": Checagem, "args": list(Args)}
    if Modo is not None:
        Campos["modo"] = Modo
    if Classe is not None:
        Campos["classe"] = Classe
    return ShaBytes(json.dumps(Campos, sort_keys=True).encode("utf-8"))


def ShaItens(Itens: dict) -> str:
    """sha256 dos itens em forma canonica: o que o selo protege."""
    return ShaBytes(json.dumps(Itens, sort_keys=True).encode("utf-8"))


def VersaoGcc() -> str:
    try:
        return subprocess.run(["gcc", "--version"], capture_output=True, timeout=30).stdout.decode("utf-8", errors="replace").splitlines()[0]
    except (OSError, IndexError, subprocess.TimeoutExpired):
        return "gcc ausente"


def CabecalhoAtual(Referencia: Path = FORJA_REFERENCIA) -> dict:
    ShaReferencia = ShaBytes(Referencia.read_bytes()) if Referencia.is_file() else None
    ShaRuntime = ShaBytes(RUNTIME_RT.read_bytes()) if RUNTIME_RT.is_file() else None
    return {"formato": FORMATO, "forja_referencia_sha256": ShaReferencia, "gcc": VersaoGcc(), "flags": FLAGS, "forja_rt_sha256": ShaRuntime}


def NormalizarStderr(Bruto: bytes) -> list:
    """Mesma regra do verificar_otim.NormalizarStderr (codigo, linha, causa); guardada como lista para o JSON."""
    Partes = []
    for Linha in Bruto.decode("utf-8", errors="replace").splitlines():
        try:
            Dados = json.loads(Linha)
            Partes.append([Dados.get("codigo"), Dados.get("linha"), Dados.get("causa")])
        except (ValueError, AttributeError):
            Partes.append(Linha.strip())
    return Partes


def ComandoBuild(Referencia: Path, Fonte: Path, Exe: Path, Checagem: str, Modo: str) -> list:
    """Linha de build do backend C: release usa --release e --checagem; dev nao usa nenhum dos dois."""
    Base = [str(Referencia), "build", "--backend", "c"]
    if Modo == "release":
        Base += ["--release", "--checagem", Checagem]
    return Base + [str(Fonte), "-o", str(Exe)]


def GerarRegistro(Fonte: Path, Checagem: str, Args=(), Referencia: Path = FORJA_REFERENCIA, Modo: str = "release") -> dict:
    """Compila com o forja de referencia (backend C + gcc) e roda: {exit, stdout (b64), stderr, invalido, origem}. Modo teste roda `forja test --json` e guarda a saida."""
    if Modo not in MODOS:
        raise ErroOraculo(f"modo desconhecido: {Modo} (valem {', '.join(MODOS)})")
    if not Referencia.is_file():
        raise ErroOraculo(f"forja de referencia ausente: {Referencia}")
    Origem = Referencia.name
    if Modo == "teste":
        Rodou = subprocess.run([str(Referencia), "test", str(Fonte), "--json", "--backend", "c"], capture_output=True, timeout=TIMEOUT_BUILD)
        return {"invalido": False, "origem": Origem, "exit": Rodou.returncode, "stdout": base64.b64encode(Rodou.stdout).decode("ascii"), "stderr": NormalizarStderr(Rodou.stderr)}
    with tempfile.TemporaryDirectory(prefix="oraculo_cong_") as Temp:
        Exe = Path(Temp) / "c.exe"
        Build = subprocess.run(ComandoBuild(Referencia, Fonte, Exe, Checagem, Modo), capture_output=True, timeout=TIMEOUT_BUILD)
        if Build.returncode != 0:
            return {"invalido": True, "origem": Origem, "causa": (Build.stderr + Build.stdout).decode("utf-8", errors="replace").strip()[:200], "exit": Build.returncode, "stdout": "", "stderr": []}
        try:
            Rodou = subprocess.run([str(Exe), *Args], capture_output=True, timeout=TIMEOUT_EXEC)
        except subprocess.TimeoutExpired:
            return {"invalido": False, "origem": Origem, "exit": "timeout", "stdout": "", "stderr": []}
    return {"invalido": False, "origem": Origem, "exit": Rodou.returncode, "stdout": base64.b64encode(Rodou.stdout).decode("ascii"), "stderr": NormalizarStderr(Rodou.stderr)}


class BancoOraculo:
    """O JSON congelado em memoria; Obter e seguro entre threads (o gcc roda fora do cadeado, a gravacao dentro)."""

    def __init__(self, Caminho: Path = CAMINHO_JSON, Referencia: Path = FORJA_REFERENCIA) -> None:
        self.Caminho = Caminho
        self.Referencia = Referencia
        self.Trava = threading.Lock()
        self.Cabecalho, self.Itens = self.Ler()
        self.Conferido = False

    def Ler(self):
        try:
            Dados = json.loads(self.Caminho.read_text(encoding="utf-8"))
            return Dados.get("cabecalho", {}), Dados.get("itens", {})
        except (OSError, ValueError):
            return {}, {}

    def Salvar(self) -> None:
        Texto = json.dumps({"cabecalho": self.Cabecalho, "itens": self.Itens}, sort_keys=True, indent=0)
        self.Caminho.write_text(Texto + "\n", encoding="utf-8", newline="\n")

    @property
    def Selado(self) -> bool:
        return bool(self.Cabecalho.get("selo"))

    def Selar(self, Origem: str) -> None:
        """Grava o selo: sha dos itens + origem nominal + data. Depois disso o banco so le (chave ausente = falha, gcc e forja_rt.h nao entram mais)."""
        if not self.Itens:
            raise ErroOraculo("nao ha o que selar: banco vazio")
        self.Cabecalho["selo"] = {"sha256_itens": ShaItens(self.Itens), "origem": Origem, "itens": len(self.Itens), "data": time.strftime("%Y-%m-%d")}
        self.Salvar()

    def ConferirSelo(self) -> None:
        if ShaItens(self.Itens) != self.Cabecalho["selo"]["sha256_itens"]:
            raise ErroOraculo(f"oraculo selado ADULTERADO: o sha dos itens nao bate com o selo ({self.Caminho.name})")

    def CabecalhoDivergente(self) -> list:
        """Campos do cabecalho gravado que diferem do atual (vazio = bate). Sem arquivo: so quem ainda nao congelou, e nao diverge. Selado: nunca diverge (gcc/forja_rt.h/exe nao contam)."""
        if not self.Itens or self.Selado:
            return []
        Atual = CabecalhoAtual(self.Referencia)
        Ignorar = {"forja_referencia_sha256"} if Atual["forja_referencia_sha256"] is None else set()  # forja de referencia ausente: so serve para ler
        return [Campo for Campo in Atual if Campo not in Ignorar and self.Cabecalho.get(Campo) != Atual[Campo]]

    def ExigirCabecalho(self) -> None:
        if self.Conferido:
            return
        if self.Selado:
            self.ConferirSelo()
            self.Conferido = True
            return
        Diferentes = self.CabecalhoDivergente()
        if Diferentes:
            raise ErroOraculo(f"cabecalho do oraculo congelado mudou ({', '.join(Diferentes)}); {MSG_RECONGELAR}")
        self.Conferido = True

    def Registrar(self, Chave: str, Registro: dict) -> None:
        with self.Trava:
            if self.Selado:
                raise ErroOraculo(f"banco selado nao aceita registro novo ({self.Caminho.name})")
            if not self.Itens:
                self.Cabecalho = CabecalhoAtual(self.Referencia)
            self.Itens[Chave] = Registro
            self.Salvar()

    def Tem(self, Fonte, Checagem: str, Args=(), Modo=None, Classe=None) -> bool:
        return ChaveDe(Fonte, Checagem, Args, Modo, Classe) in self.Itens

    def Obter(self, Fonte, Checagem: str, Args=(), Gerar: bool = True, Modo=None, Classe=None):
        """Registro congelado; None se faltar e Gerar for falso; ausente com Gerar gera (forja de referencia + gcc) e grava. Fonte str so le (gerar exige Path).
        Banco SELADO: chave ausente e FALHA (ErroOraculo), nunca regera nem devolve None."""
        self.ExigirCabecalho()
        Chave = ChaveDe(Fonte, Checagem, Args, Modo, Classe)
        if Chave not in self.Itens and self.Selado:
            raise ErroOraculo(f"chave ausente no oraculo selado ({self.Caminho.name}; classe {Classe}, modo {Modo}): fixture nova ou editada depois do selo, escreva o .esperado a mao (D0707), nao regere")
        if Chave not in self.Itens and Gerar:
            if not isinstance(Fonte, Path):
                raise ErroOraculo("gerar exige a fonte em arquivo (Path)")
            self.Registrar(Chave, GerarRegistro(Fonte, Checagem, Args, self.Referencia, Modo or "release"))
        return self.Itens.get(Chave)


_BANCO_PADRAO: BancoOraculo | None = None


def BancoPadrao() -> BancoOraculo:
    global _BANCO_PADRAO
    if _BANCO_PADRAO is None:
        _BANCO_PADRAO = BancoOraculo()
    return _BANCO_PADRAO


_BANCO_G1: BancoOraculo | None = None


def BancoG1() -> BancoOraculo:
    """Lote da 0.21.0 (classes novas), gerado pelo forja_g1 arquivado em gaveta\\g1."""
    global _BANCO_G1
    if _BANCO_G1 is None:
        _BANCO_G1 = BancoOraculo(CAMINHO_JSON_G1, FORJA_G1)
    return _BANCO_G1


_BANCO_REF: BancoOraculo | None = None


def BancoRef() -> BancoOraculo:
    """Binarios gcc de referencia (0.21.0 F3, D0730): bench, kernels e ELF musl compilados 1x pelo forja_g1 (backend C + gcc).
    O C nao muda, entao o binario e a referencia exata em qualquer maquina: ele roda ao vivo e a razao x64/gcc continua honesta (um JSON de tempos nao seria)."""
    global _BANCO_REF
    if _BANCO_REF is None:
        _BANCO_REF = BancoOraculo(CAMINHO_JSON_REF, FORJA_G1)
    return _BANCO_REF


def ChaveRef(Fonte: Path, Plataforma: str, Nivel: str) -> str:
    return ChaveDe(Fonte, "ref", (Plataforma, Nivel), None, CLASSE_REF)


def RegistrarBinarioRef(Banco: BancoOraculo, Fonte: Path, Plataforma: str, Nivel: str, Binario: Path, Flags: str, Pasta: Path = PASTA_REF) -> Path:
    """Copia o binario gcc para ref_gcc\\ e registra {arquivo, sha256, flags, gcc}; so antes do selo (o banco selado recusa registro novo)."""
    Pasta.mkdir(parents=True, exist_ok=True)
    Destino = Pasta / f"{Fonte.parent.name}.{Fonte.stem}.{Plataforma}.{Nivel}{Binario.suffix}"
    shutil.copyfile(Binario, Destino)
    Banco.Registrar(ChaveRef(Fonte, Plataforma, Nivel), {"arquivo": Destino.name, "sha256": ShaBytes(Destino.read_bytes()), "plataforma": Plataforma, "nivel": Nivel,
                                                          "flags": Flags, "gcc": VersaoGcc(), "origem": FORJA_G1.name, "fonte": Fonte.name})
    return Destino


def BinarioDeReferencia(Fonte: Path, Plataforma: str, Nivel: str, Banco: BancoOraculo | None = None, Pasta: Path = PASTA_REF) -> Path:
    """Caminho do binario gcc congelado desta fonte/plataforma/nivel. Chave ausente, arquivo ausente ou sha trocado = ErroOraculo (nunca pula, nunca recompila)."""
    Banco = Banco or BancoRef()
    Banco.ExigirCabecalho()
    Registro = Banco.Itens.get(ChaveRef(Fonte, Plataforma, Nivel))
    if Registro is None:
        raise ErroOraculo(f"sem binario gcc de referencia para {Fonte.name} ({Plataforma}/{Nivel}) em {Banco.Caminho.name}: fonte nova ou editada depois da captura")
    Caminho = Pasta / Registro["arquivo"]
    if not Caminho.is_file():
        raise ErroOraculo(f"binario gcc de referencia ausente: {Caminho}")
    if ShaBytes(Caminho.read_bytes()) != Registro["sha256"]:
        raise ErroOraculo(f"binario gcc de referencia ADULTERADO (sha nao bate com o registro): {Caminho.name}")
    return Caminho


def ObterRegistro(Fonte, Checagem: str, Args=(), Gerar: bool = True):
    return BancoPadrao().Obter(Fonte, Checagem, Args, Gerar)


def ObterSaidaEsperada(Fonte, Checagem: str, Args=()):
    """(stdout bytes, exit) do backend C congelado para esta fonte/checagem/args; gera e grava se faltar."""
    Registro = ObterRegistro(Fonte, Checagem, Args)
    if Registro["invalido"]:
        raise ErroOraculo(f"o forja de referencia recusou a fonte: {Registro.get('causa')}")
    return base64.b64decode(Registro["stdout"]), Registro["exit"]


def FixturesDoAlocador() -> list:
    """(fonte, checagem) das fixtures do alocador que o verificar_alocador confere com o backend C."""
    sys.path.insert(0, str(PastaNativo / "otim"))
    import verificar_alocador as Alocador
    Casos = []
    for Fonte in sorted(Alocador.PastaFixtures.glob("*.fj")):
        if Alocador.PrecisaBackendC(Fonte.name):
            Casos += [(Fonte, C) for C in (("sim",) if Fonte.name.startswith("trap_") else ("nao", "sim"))]
    return Casos


def SementesDoFuzz() -> list:
    """(texto, checagem) das sementes 1..2400, na mesma regra do fuzz_otim.TestarSemente."""
    sys.path.insert(0, str(PastaNativo / "otim"))
    import fuzz_otim_gerador as Gerador
    Casos = []
    for Semente in SEMENTES_FUZZ:
        Texto, Meta = Gerador.GerarPrograma(Semente)
        Casos.append((Texto, "sim"))
        if Semente % CADA_NAO == 0 and not Meta["oob"]:
            Casos.append((Texto, "nao"))
    return Casos


def CongelarUm(Banco: BancoOraculo, Fonte, Checagem: str, Temp: Path) -> bool:
    """True se gerou agora; texto de semente vai para um .fj temporario so para o forja ler."""
    if Banco.Tem(Fonte, Checagem):
        return False
    Arquivo = Fonte
    if isinstance(Fonte, str):
        Arquivo = Temp / f"{ShaBytes(Fonte.encode('utf-8'))[:16]}_{Checagem}.fj"
        Arquivo.write_text(Fonte, encoding="utf-8", newline="\n")
    Registro = GerarRegistro(Arquivo, Checagem, Referencia=Banco.Referencia)
    with Banco.Trava:
        Banco.Itens[ChaveDe(Fonte, Checagem)] = Registro
    return True


def Congelar(Jobs: int, SoFuzz: bool = False) -> int:
    Banco = BancoPadrao()
    if Banco.Selado:
        raise ErroOraculo("banco selado: nao se recongela (o gcc sai na 0.21.0); fixture nova = .esperado a mao")
    if Banco.CabecalhoDivergente():
        Banco.Itens.clear()  # cabecalho mudou: tudo e regerado
    Banco.Cabecalho = CabecalhoAtual()
    if Banco.Cabecalho["forja_referencia_sha256"] is None:
        raise ErroOraculo(f"forja de referencia ausente: {FORJA_REFERENCIA}")
    Casos = ([] if SoFuzz else FixturesDoAlocador()) + SementesDoFuzz()
    with tempfile.TemporaryDirectory(prefix="oraculo_cong_fontes_") as Temp, concurrent.futures.ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Novos = sum(Pool.map(lambda Caso: CongelarUm(Banco, Caso[0], Caso[1], Path(Temp)), Casos))
    Banco.Salvar()
    Invalidos = sum(1 for R in Banco.Itens.values() if R["invalido"])
    print(f"oraculo_congelado: {len(Casos)} casos, {Novos} gerados agora, {len(Banco.Itens)} no arquivo ({Invalidos} recusados pelo forja de referencia)")
    return 0


def Selar(Origem: str, Caminho: Path) -> int:
    Banco = BancoOraculo(Caminho)
    if Banco.Selado:
        print(f"oraculo_congelado: {Caminho.name} ja esta selado ({Banco.Cabecalho['selo']['data']}, {Banco.Cabecalho['selo']['itens']} itens)")
        return 1
    Banco.Selar(Origem)
    print(f"oraculo_congelado: {Caminho.name} SELADO, {len(Banco.Itens)} itens, sha {Banco.Cabecalho['selo']['sha256_itens'][:16]}, origem {Origem}")
    return 0


def Verificar() -> int:
    Banco = BancoPadrao()
    if Banco.Selado:
        Banco.ConferirSelo()
        print(f"OK: selado ({Banco.Cabecalho['selo']['origem']}), sha dos {len(Banco.Itens)} itens bate")
        return 0
    Diferentes = Banco.CabecalhoDivergente()
    if not Banco.Itens:
        print(f"oraculo_congelado: vazio; {MSG_RECONGELAR}")
        return 1
    if Diferentes:
        print(f"FALHOU: cabecalho mudou ({', '.join(Diferentes)}); {MSG_RECONGELAR}")
        return 1
    print(f"OK: cabecalho bate, {len(Banco.Itens)} itens")
    return 0


def SelftestSelo(Temp: Path) -> list:
    """Selo: cabecalho selado nao diverge, chave ausente falha, item adulterado falha, banco selado nao aceita registro; chave com modo/classe difere."""
    Falhas = []
    Fonte = Temp / "s.fj"
    Fonte.write_text("proc main() {}\n", encoding="utf-8")
    Caminho = Temp / "selado.json"
    Banco = BancoOraculo(Caminho)
    Banco.Registrar(ChaveDe(Fonte, "sim", (), "release", "nativo"), {"invalido": False, "origem": "x", "exit": 0, "stdout": "", "stderr": []})
    Banco.Selar("selftest")
    Lido = BancoOraculo(Caminho)
    Lido.Cabecalho["gcc"] = "outro gcc que nao pode importar"
    if not Lido.Selado or Lido.CabecalhoDivergente():
        Falhas.append("banco selado nao deveria divergir por gcc")
    if Lido.Obter(Fonte, "sim", (), False, "release", "nativo") is None:
        Falhas.append("registro selado deveria voltar")
    for Modo, Classe in (("dev", "nativo"), ("release", "rt"), (None, None)):
        try:
            Lido.Obter(Fonte, "sim", (), True, Modo, Classe)
            Falhas.append(f"chave ausente ({Modo}/{Classe}) no selado deveria falhar")
        except ErroOraculo:  # lint-ok: iaf-erro-engolido a falha e o resultado esperado: o teste prova que ela acontece
            pass
    try:
        Lido.Registrar("novo", {})
        Falhas.append("selado nao deveria aceitar registro")
    except ErroOraculo:  # lint-ok: iaf-erro-engolido a falha e o resultado esperado: o teste prova que ela acontece
        pass
    Dados = json.loads(Caminho.read_text(encoding="utf-8"))
    Dados["itens"][next(iter(Dados["itens"]))]["exit"] = 7
    Caminho.write_text(json.dumps(Dados), encoding="utf-8")
    try:
        BancoOraculo(Caminho).Obter(Fonte, "sim", (), False, "release", "nativo")
        Falhas.append("item adulterado deveria falhar")
    except ErroOraculo:  # lint-ok: iaf-erro-engolido a falha e o resultado esperado: o teste prova que ela acontece
        pass
    if len({ChaveDe(Fonte, "sim"), ChaveDe(Fonte, "sim", (), "dev"), ChaveDe(Fonte, "sim", (), "dev", "rt")}) != 3:
        Falhas.append("modo e classe deveriam mudar a chave")
    return Falhas


def SelftestBinario(Temp: Path) -> list:
    """Binario de referencia: registrado e selado volta; nivel nao capturado falha; binario trocado em disco falha."""
    Falhas = []
    Fonte, Binario, Pasta = Temp / "b.fj", Temp / "x.exe", Temp / "ref"
    Fonte.write_text("proc main() {}\n", encoding="utf-8")
    Binario.write_bytes(b"MZ-binario")
    Banco = BancoOraculo(Temp / "ref.json")
    RegistrarBinarioRef(Banco, Fonte, "win", "O2", Binario, "-O2", Pasta)
    Banco.Selar("selftest")
    Lido = BancoOraculo(Temp / "ref.json")
    Caminho = BinarioDeReferencia(Fonte, "win", "O2", Lido, Pasta)
    if Caminho.read_bytes() != b"MZ-binario":
        Falhas.append("binario de referencia nao voltou igual")
    try:
        BinarioDeReferencia(Fonte, "win", "O0", Lido, Pasta)
        Falhas.append("nivel nao capturado deveria falhar")
    except ErroOraculo:  # lint-ok: iaf-erro-engolido a falha e o resultado esperado: o teste prova que ela acontece
        pass
    Caminho.write_bytes(b"trocado")
    try:
        BinarioDeReferencia(Fonte, "win", "O2", Lido, Pasta)
        Falhas.append("binario trocado em disco deveria falhar")
    except ErroOraculo:  # lint-ok: iaf-erro-engolido a falha e o resultado esperado: o teste prova que ela acontece
        pass
    return Falhas


def Selftest() -> int:
    Falhas = []
    with tempfile.TemporaryDirectory(prefix="oraculo_st_") as Temp:
        Modulo, Fonte = Path(Temp) / "m.fj", Path(Temp) / "a.fj"
        Modulo.write_text("proc f() {}\n", encoding="utf-8")
        Fonte.write_text('import "m"\nproc main() {}\n', encoding="utf-8")
        Antes = ChaveDe(Fonte, "sim")
        Modulo.write_text("proc f() { }\n", encoding="utf-8")
        if ChaveDe(Fonte, "sim") == Antes:
            Falhas.append("a chave deveria mudar quando o modulo importado muda")
        if ChaveDe(Fonte, "sim") == ChaveDe(Fonte, "nao") or ChaveDe(Fonte, "sim", ("1",)) == ChaveDe(Fonte, "sim"):
            Falhas.append("checagem e args deveriam entrar na chave")
        Banco = BancoOraculo(Path(Temp) / "c.json")
        Banco.Registrar("k", {"invalido": False, "exit": 0, "stdout": "", "stderr": []})
        Banco.Cabecalho["gcc"] = "outro"
        Banco.Conferido = False
        if "gcc" not in Banco.CabecalhoDivergente():
            Falhas.append("cabecalho alterado deveria divergir")
        if BancoOraculo(Path(Temp) / "c.json").Itens.get("k") is None:
            Falhas.append("registro gravado nao voltou do disco")
        Falhas += SelftestSelo(Path(Temp))
        Falhas += SelftestBinario(Path(Temp))
    if NormalizarStderr(b'{"codigo":"R1","linha":3,"causa":"x"}\nabc \n') != [["R1", 3, "x"], "abc"]:
        Falhas.append("NormalizarStderr")
    print("selftest: " + ("OK" if not Falhas else "; ".join(Falhas)))
    return 0 if not Falhas else 1


def Main() -> int:
    Parser = argparse.ArgumentParser(description="Oraculo congelado (backend C + gcc gerado 1x).")
    Parser.add_argument("--congelar", action="store_true")
    Parser.add_argument("--verificar", action="store_true")
    Parser.add_argument("--selftest", action="store_true")
    Parser.add_argument("--jobs", type=int, default=4)
    Parser.add_argument("--selar", metavar="ARQUIVO_JSON", help="grava o selo no banco (caminho absoluto do .json); exige --origem")
    Parser.add_argument("--origem", default="", help="origem nominal do selo, ex.: 'forja 0.20.0 + gcc 15.2.0'")
    Args = Parser.parse_args()
    if Args.selftest:
        return Selftest()
    if Args.selar:
        if not Args.origem:
            raise ErroOraculo("--selar exige --origem")
        return Selar(Args.origem, Path(Args.selar))
    if Args.congelar:
        return Congelar(Args.jobs)
    if Args.verificar:
        return Verificar()
    Parser.print_help()
    return 2


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except ErroOraculo as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        sys.exit(2)
