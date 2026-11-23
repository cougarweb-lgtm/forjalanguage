#!/usr/bin/env python3
"""cache_linux.py - cache por hash do ELF e oraculo x64-windows congelado do verificar_linux (R8 do contrato M7a, passos 5 e 6).

Objetivo (cache): o manifesto da ultima rodada APROVADA guarda, por (caso, modo), o sha256 do ELF, o sha256 das entradas do caso (fonte, esperado/esperado_so, args, env, dados,
entrada, ELF musl, demais campos do Caso) e o registro verde. ELF e entradas iguais = o caso pula oraculo, lote e determinismo. O build SEMPRE roda (e ele que da o hash); o hash do
forja.exe NAO entra na chave. Sempre rodam: processo (pasta, proc), arquivo, vazamento, std/net e std/process (dependem do ambiente). O "ambiente" (conjunto_linux, casos_linux, esperado_so,
aceitas, runtime/nativo/*.[ch] e os proprios arquivos do verificador) tem um sha: mudou = manifesto ignorado e rodada completa obrigatoria. Hash de ELF dev/teste que mudou entre rodadas = alarme "R2 violada".
Objetivo (congelado): --congelar-oraculo <arq.json> grava, por (sha256 da fonte, modo, args/env), o stdout/stderr/exit do x64-windows de uma base; --oraculo-congelado <arq.json> usa-o nos casos
RELEASE de dourado, exemplos, examples, semantica, m1, m2, m4, v111 e traco (oraculo independente do alocador que o x64-windows da mesma rodada compartilha com o alvo linux). Caminho padrao
documentado: oraculo_congelado_0198.json nesta pasta (fora do selftest; o main o gera de um forja validado). Limite conhecido: o sha da fonte nao cobre os modulos que ela importa.
Uso:   python -I cache_linux.py --selftest   (o resto e usado por verificar_linux.py)
Escrita: manifesto em %TEMP%\\forja_linux\\manifesto_verificar.json (ou --manifesto); o congelado so onde --congelar-oraculo mandar.  Reversao: --sem-cache, ou apagar o manifesto / este arquivo.

Versao: 0.1.2
Changelog: 0.1.2 (18/11/2026) 0.21.0 F4 passo 3: o forja_rt.h deixa de ser entrada do ambiente (nenhum .c do forja.exe o consome desde o emissor C; o build x64 usa runtime/nativo/*.[ch]); o sha do ambiente passa a cobrir esses arquivos.
           0.1.1 (18/11/2026) P3 - o sha do ELF sai sem os bytes de versao do forja (ShaElf -> mascara_versao.HashSemVersao); VERSAO_FORMATO 2 (manifesto); o congelado segue no formato 1 (VERSAO_CONGELADO).
           0.1.0 (17/11/2026) R8 passos 5 e 6 - primeira versao.
"""
import base64
import dataclasses
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI.parent / "otim"))  # mascara_versao (P3): tests\nativo\otim
import mascara_versao as Mascara  # noqa: E402
VERSAO_FORMATO = 2  # manifesto: subiu com o P3 (o sha do ELF mudou de criterio); rodada anterior vira rodada completa
VERSAO_CONGELADO = 1  # formato de oraculo_congelado_0198.json; so o congelado le/grava por ele
MANIFESTO_PADRAO = Path(tempfile.gettempdir()) / "forja_linux" / "manifesto_verificar.json"
CONGELADO_PADRAO = AQUI / "oraculo_congelado_0198.json"
GRUPOS_CONGELAVEIS = ("dourado", "exemplos", "examples", "semantica", "m1", "m2", "m4", "v111", "traco")
GRUPOS_SEMPRE_RODAM = ("pasta", "proc", "arquivo")
TRECHOS_SEMPRE_RODAM = ("vazamento", "std/net", "process")
CAMPOS_FORA_DO_HASH = ("Nome", "Modo", "Fonte", "OraculoWin", "OrigemOraculo")  # Nome e Modo vao na chave; a fonte entra pelo sha do conteudo
ORIGEM_CONGELADO = "congelado"
ORIGEM_MESMA_RODADA = "windows-mesma-rodada"


def ShaBytes(Dado: bytes) -> str:
    return hashlib.sha256(Dado).hexdigest()


def ShaArquivo(Arquivo: Path) -> str | None:
    return ShaBytes(Arquivo.read_bytes()) if Arquivo.is_file() else None


def ShaElf(Arquivo: Path, Forja: Path) -> str | None:
    """sha256 do ELF sem os bytes de versao do forja que o gerou (mascara_versao); None se o arquivo nao existe."""
    return Mascara.HashSemVersao(Arquivo, Mascara.VersoesDoForja(Forja)) if Arquivo.is_file() else None


def SerializarCampo(Valor):
    """default do json.dumps: bytes (esperado, dados, entrada) viram hex; Path vira texto."""
    return Valor.hex() if isinstance(Valor, bytes) else str(Valor)


def ShaDasEntradas(Caso, ShaMusl: str | None) -> str:
    """sha das entradas do caso: todos os campos do Caso (fora Nome/Modo/Fonte e o que a rodada preenche) + o conteudo da fonte + o ELF musl."""
    Itens = {Campo.name: getattr(Caso, Campo.name) for Campo in dataclasses.fields(Caso) if Campo.name not in CAMPOS_FORA_DO_HASH}
    Itens["fonte"], Itens["musl"] = ShaArquivo(Caso.Fonte), ShaMusl
    return ShaBytes(json.dumps(Itens, sort_keys=True, default=SerializarCampo).encode("utf-8"))


def ArquivosDoAmbiente(Language: Path) -> list:
    Fixos = [AQUI / Nome for Nome in ("conjunto_linux.py", "casos_linux.py", "wsl_lote.py", "oraculo_c.py", "oraculo_c.txt", "verificar_linux.py", "cache_linux.py",
                                      "divergencias_aceitas.json")]
    return Fixos + sorted((AQUI / "esperado_so").glob("*")) + sorted((Language / "runtime" / "nativo").glob("*.[ch]"))


def ShaDoAmbiente(Language: Path) -> str:
    Resumo = hashlib.sha256()
    for Arquivo in ArquivosDoAmbiente(Language):
        Resumo.update(Arquivo.name.encode("utf-8") + (ShaArquivo(Arquivo) or "ausente").encode("ascii"))
    return Resumo.hexdigest()


def SempreRoda(Caso) -> bool:
    """Caso que depende do ambiente (processo, arquivo, vazamento, rede) nunca e reaproveitado."""
    return Caso.Grupo in GRUPOS_SEMPRE_RODAM or any(Trecho in Caso.Nome for Trecho in TRECHOS_SEMPRE_RODAM)


def ChaveDoCaso(Nome: str, Modo: str) -> str:
    return f"{Nome}|{Modo}"


def Impressoes(Casos: list, ShaElfs: dict, ShasMusl: dict, Chave) -> dict:
    """{(nome, modo): (sha do ELF, sha das entradas)} dos casos com ELF; calcular ANTES de o oraculo preencher o esperado. Chave(Caso) = a chave do ELF."""
    Resultado = {}
    for Caso in Casos:
        if Chave(Caso) in ShaElfs:
            Resultado[(Caso.Nome, Caso.Modo)] = (ShaElfs[Chave(Caso)], ShaDasEntradas(Caso, ShasMusl.get(Chave(Caso))))
    return Resultado


class Manifesto:
    """O manifesto da ultima rodada aprovada (leitura); AmbienteIgual False = nada e reaproveitado."""

    def __init__(self, Caminho: Path, Ambiente: str) -> None:
        Dados = self.Ler(Caminho)
        self.Existe = bool(Dados)
        self.AmbienteIgual = Dados.get("ambiente") == Ambiente
        self.Casos = Dados.get("casos", {})
        self.Determinismo = Dados.get("determinismo", {})

    @staticmethod
    def Ler(Caminho: Path) -> dict:
        try:
            Dados = json.loads(Caminho.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}
        return Dados if isinstance(Dados, dict) and Dados.get("versao") == VERSAO_FORMATO else {}

    def Reaproveitar(self, Nome: str, Modo: str, Impressao: tuple) -> dict | None:
        """O registro verde do caso se o ELF e as entradas batem; senao None."""
        Entrada = self.Casos.get(ChaveDoCaso(Nome, Modo))
        if self.AmbienteIgual and Entrada and (Entrada["elf"], Entrada["entradas"]) == tuple(Impressao):
            return dict(Entrada["registro"], cache=True)
        return None

    def ElfMudou(self, Nome: str, Modo: str, ShaElf: str) -> bool:
        Entrada = self.Casos.get(ChaveDoCaso(Nome, Modo))
        return bool(Entrada) and Entrada["elf"] != ShaElf

    def DeterminismoVerde(self, Nome: str, Modo: str, ShaElf: str, Fatia: bool) -> list | None:
        """Os registros de determinismo da mesma chave com o mesmo ELF; a fatia aceita os dois perfis, o completo so o completo."""
        Entrada = self.Determinismo.get(ChaveDoCaso(Nome, Modo))
        if not (self.AmbienteIgual and Entrada and Entrada["elf"] == ShaElf and (Fatia or Entrada["perfil"] == "completo")):
            return None
        return [dict(Registro, cache=Entrada["perfil"]) for Registro in Entrada["registros"]]


def AlarmesR2(Mani: Manifesto, Impressao: dict) -> list:
    """Casos dev/teste cujo ELF mudou desde a rodada aprovada (R2: so o release pode mudar)."""
    return sorted(f"{Nome} [{Modo}]" for (Nome, Modo), (ShaElf, _) in Impressao.items() if Modo != "release" and Mani.ElfMudou(Nome, Modo, ShaElf))


def EntradasDoManifesto(Registros: list, Impressao: dict) -> dict:
    Casos = {}
    for Registro in Registros:
        Par = (Registro["caso"], Registro["modo"])
        if Registro["ok"] and not Registro.get("pulado") and not Registro["caso"].startswith(("determinismo", "estrutura")) and Par in Impressao:
            Casos[ChaveDoCaso(*Par)] = {"elf": Impressao[Par][0], "entradas": Impressao[Par][1], "registro": {K: V for K, V in Registro.items() if K != "cache"}}
    return Casos


def DeterminismoDoManifesto(Registros: list, ShasDeterminismo: dict, Perfil: str) -> dict:
    """{nome|modo: {elf, perfil, registros}} agrupando os registros determinismo/ e determinismo-debug/ da mesma chave."""
    Grupos = {}
    for Registro in Registros:
        if not Registro["caso"].startswith("determinismo"):
            continue
        Par = (Registro["caso"].split("/", 1)[1], Registro["modo"])
        if Par in ShasDeterminismo and Registro["ok"]:
            Grupo = Grupos.setdefault(ChaveDoCaso(*Par), {"elf": ShasDeterminismo[Par], "perfil": Perfil, "registros": []})
            if isinstance(Registro.get("cache"), str):
                Grupo["perfil"] = Registro["cache"]
            Grupo["registros"].append({K: V for K, V in Registro.items() if K != "cache"})
    return Grupos


def GravarManifesto(Caminho: Path, Ambiente: str, Casos: dict, Determinismo: dict) -> None:
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Temporario = Caminho.with_suffix(".tmp")
    Temporario.write_text(json.dumps({"versao": VERSAO_FORMATO, "ambiente": Ambiente, "casos": Casos, "determinismo": Determinismo}, ensure_ascii=False), encoding="utf-8")
    Temporario.replace(Caminho)


def UsaCongelado(Caso) -> bool:
    """So os casos RELEASE dos grupos de oraculo independente (o dev mantem o x64-windows da mesma rodada)."""
    return Caso.Modo == "release" and Caso.Grupo in GRUPOS_CONGELAVEIS


def ChaveCongelada(Caso) -> str:
    """sha256 de (sha da fonte, modo, args, env)."""
    return ShaBytes(json.dumps([ShaArquivo(Caso.Fonte), Caso.Modo, Caso.Args, Caso.Env], sort_keys=True, default=SerializarCampo).encode("utf-8"))


def CarregarCongelado(Arquivo: Path) -> dict:
    Dados = json.loads(Arquivo.read_text(encoding="utf-8"))
    if Dados.get("versao") != VERSAO_CONGELADO:
        raise SystemExit(f"{Arquivo}: formato de oraculo congelado desconhecido ({Dados.get('versao')!r})")
    return Dados["itens"]


def ItemCongelado(Rodada) -> dict:
    return {"stdout": base64.b64encode(Rodada.stdout).decode("ascii"), "stderr": base64.b64encode(Rodada.stderr).decode("ascii"), "exit": Rodada.returncode}


def RodadaCongelada(Item: dict) -> subprocess.CompletedProcess:
    return subprocess.CompletedProcess([], Item["exit"], base64.b64decode(Item["stdout"]), base64.b64decode(Item["stderr"]))


def GravarCongelado(Arquivo: Path, Itens: dict, Forja: Path) -> None:
    Texto = {"versao": VERSAO_CONGELADO, "forja_sha256": ShaArquivo(Forja), "itens": Itens}
    Arquivo.write_text(json.dumps(Texto, ensure_ascii=False, indent=0, sort_keys=True), encoding="utf-8")


def Selftest() -> int:
    from casos_linux import Caso
    Falhas = []
    with tempfile.TemporaryDirectory(prefix="forja_cache_selftest_") as Pasta_:
        Pasta = Path(Pasta_)
        Fonte = Pasta / "a.fj"
        Fonte.write_bytes(b"proc main() {}\n")
        Base = Caso("grupo/a", Fonte, "release", ["x"], {"K": "v"}, Esperado=b"ok\n", Grupo="dourado")
        Referencia = ShaDasEntradas(Base, "musl1")
        Variantes = {"esperado": dataclasses.replace(Base, Esperado=b"outro\n"), "args": dataclasses.replace(Base, Args=["y"]), "env": dataclasses.replace(Base, Env={"K": "w"})}
        for Nome, Variante in Variantes.items():
            if ShaDasEntradas(Variante, "musl1") == Referencia:
                Falhas.append(f"entradas iguais apesar de mudar {Nome}")
        if ShaDasEntradas(Base, "musl2") == Referencia or ShaDasEntradas(dataclasses.replace(Base, Nome="outro"), "musl1") != Referencia:
            Falhas.append("musl deve entrar no hash e o nome nao")
        Fonte.write_bytes(b"proc main() { }\n")
        if ShaDasEntradas(Base, "musl1") == Referencia or ChaveCongelada(Base) == ChaveCongelada(dataclasses.replace(Base, Modo="dev")):
            Falhas.append("fonte/modo devem mudar os hashes")
        Falhas += SelftestManifesto(Pasta, Base)
    print("cache_linux selftest:", "VERDE" if not Falhas else f"FALHOU {Falhas}")
    return 1 if Falhas else 0


def SelftestManifesto(Pasta: Path, Base) -> list:
    """Invalidacao: so reaproveita com ELF E entradas iguais e o mesmo ambiente; dev/teste com ELF diferente = alarme R2; congelado ida e volta."""
    Falhas = []
    Impressao = {(Base.Nome, "release"): ("elf1", "ent1"), (Base.Nome, "dev"): ("elf2", "ent2")}
    Registros = [{"caso": Base.Nome, "modo": "release", "ok": True, "oraculo": "esperado", "motivo": None}, {"caso": Base.Nome, "modo": "dev", "ok": True, "oraculo": "esperado", "motivo": None},
                 {"caso": "determinismo/" + Base.Nome, "modo": "release", "ok": True, "oraculo": "dois builds", "motivo": None}]
    Arquivo = Pasta / "manifesto.json"
    GravarManifesto(Arquivo, "amb", EntradasDoManifesto(Registros, Impressao), DeterminismoDoManifesto(Registros, {(Base.Nome, "release"): "elf1"}, "fatia"))
    Mani = Manifesto(Arquivo, "amb")
    if Mani.Reaproveitar(Base.Nome, "release", ("elf1", "ent1")) is None:
        Falhas.append("igual nao reaproveitou")
    for Rotulo, Impr in (("mudou o ELF", ("elf9", "ent1")), ("mudou o esperado/args", ("elf1", "ent9"))):
        if Mani.Reaproveitar(Base.Nome, "release", Impr) is not None:
            Falhas.append(f"reaproveitou apesar de {Rotulo}")
    if Manifesto(Arquivo, "outro").Reaproveitar(Base.Nome, "release", ("elf1", "ent1")) is not None:
        Falhas.append("reaproveitou com outro ambiente")
    if AlarmesR2(Mani, {(Base.Nome, "dev"): ("elfX", "ent2")}) != [f"{Base.Nome} [dev]"] or AlarmesR2(Mani, {(Base.Nome, "release"): ("elfX", "e")}):
        Falhas.append("alarme R2 (dev muda = alarme; release nao)")
    if Mani.DeterminismoVerde(Base.Nome, "release", "elf1", False) is not None or not Mani.DeterminismoVerde(Base.Nome, "release", "elf1", True):
        Falhas.append("perfil do determinismo (completo nao aceita fatia; fatia aceita)")
    Congelado = Pasta / "congelado.json"
    GravarCongelado(Congelado, {ChaveCongelada(Base): ItemCongelado(subprocess.CompletedProcess([], 3, b"o\r\n", b"e"))}, Pasta / "inexistente.exe")
    Volta = RodadaCongelada(CarregarCongelado(Congelado)[ChaveCongelada(Base)])
    if (Volta.stdout, Volta.stderr, Volta.returncode) != (b"o\r\n", b"e", 3):
        Falhas.append("congelado ida e volta")
    return Falhas


if __name__ == "__main__":
    sys.path.insert(0, str(AQUI))
    sys.exit(Selftest() if "--selftest" in sys.argv else print(__doc__) or 2)
