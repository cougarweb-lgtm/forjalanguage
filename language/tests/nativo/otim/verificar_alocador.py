# -*- coding: utf-8 -*-
"""Stress do alocador de registradores (M7a F3): cada fixture de alocador\\*.fj compila em release x64 com o alocador (padrao) e com '--sem-otim alocador',
com --checagem nao e sim, e exige o mesmo stdout, o mesmo stderr e o mesmo codigo de saida (inclusive R0005/R0006 exatos depois de print dentro de laco).

Uso:
    python verificar_alocador.py --forja C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe [--sem-manifesto] [--contra C:/.../forja_base.exe]
    python -I verificar_alocador.py --selftest
Saida final: "OK: N/N casos iguais" (ou "FALHOU: ..."); exit 0 verde, 1 divergencia, 2 erro de uso.

Versao: 1.15.0
Changelog: 1.15.0 - 0.21.0 F3: ConferirPilhaDev compara o (exit, pilha) do x64 com o registro congelado do BancoRef (classe pilha, modo dev); o backend C saiu dessa conferencia.
           1.14.2 - P3: o hash do exe (manifesto e neutralidade --contra) sai por mascara_versao.HashSemVersao, com a versao lida de `forja --versao` (VersoesDoForja); o import de verificar_otim saiu.
           1.14.1 - S4a: a regex de recusa do slp aceita o motivo `slot_escalarizavel`.
           1.14.0- S3 (SIMD): fixtures slp_*.fj e trap_slp_*.fj comparam com --sem-otim slp e com o backend C; as positivas exigem o remark `slp fj_<proc>: N grupos vetorizados`, as negativas (slp_neg_) `slp recusou: <motivo>` e nenhum grupo vetorizado.
           1.13.0 - R6a-3: fixtures endereco_*.fj comparam com --sem-otim endereco e com o backend C; as positivas (sem endereco_neg_) exigem o remark `endereco fj_<proc>: N cadeias recolocadas`; endereco_trap_indice.fj tambem confere a pilha dev.
           1.12.0 - R6a: fixtures licm_*.fj e trap_licm_*.fj comparam com --sem-otim licm e com o backend C; as positivas exigem o remark `licm fj_<proc>: N instrucoes subiram` no --debug-code otim.
           1.11.0 - R5 (SIMD): fixtures selecao_*.fj e trap_selecao_*.fj comparam com --sem-otim selecao e com o backend C.
           1.10.0 - M7b R4b: fixtures cfg_*.fj e trap_cfg_*.fj comparam com --sem-otim cfg,layout e com o backend C (use `--passe-neutro cfg,layout` no --contra de uma base sem os passes).
           1.9.0 - M7b R4a: `--passe-neutro <passe>` (padrao numeracao) escolhe o passe desligado na neutralidade do --contra;fixtures inline_folha_laco*, inline_recursao_* e trap_inline_recursao exigem o remark do inline (`inline <chamador> <- <folha>` ou `inline recusa <funcao>: <motivo>`) no --debug-code otim (RemarkInlineR4).
           1.8.0- M7b R3: fixtures numeracao_*.fj e trap_numeracao_*.fj comparam com --sem-otim numeracao e com o backend C; a linha `// espera: copia>0 expr=0 carga>0 mortos>0` da fixture e conferida contra
                   os remarks `numeracao <funcao>: copia ebb=N dom=N, expr ebb=N dom=N, carga ebb=N dom=N, mortos=N` (soma das funcoes, ebb+dom); trap_numeracao_indice.fj tambem confere a pilha dev; `--contra <base.exe>`:
                   neutralidade byte a byte (o forja novo com --sem-otim numeracao tem de gerar o mesmo exe release que o forja base, sem o passe) em todas as fixtures.
           1.7.0- D0696: o oraculo do backend C vem do JSON congelado (rapido\\oraculo_congelado.py; gcc so se faltar, e entao grava); manifesto por hash de exe (rapido\\manifesto_aprovado.json:
                   sha do exe release sem versao + sha da fonte/.esperado por fixture e checagem; igual ao aprovado = so o remark e conferido, nada reexecuta; --sem-manifesto ignora; limite
                   conhecido: o exe do --sem-otim nao entra no hash); PrecisaBackendC separado de ConferirExtras.
           1.6.0 - M7b F3: fixtures sroa_*.fj e trap_sroa_*.fj comparam com --sem-otim sroa e com o backend C; positivas exigem o remark `sroa fj_<proc> k<N>: <tam> B, <campos> campos` no --debug-code otim, negativas exigem `sroa recusa fj_<proc> k<N>: <motivo>`; .esperado ao lado da fixture (stdout) quando existe; trap_sroa_rastreio_inline confere a pilha em build dev (x64 x C); passe ausente vira falha "passe sroa ausente" sem quebrar.
           1.5.0 - M7b F2: fixtures inline_*.fj e trap_inline_*.fj comparam release x --sem-otim inline (stdout, stderr, exit) e x backend C.
           1.4.0 - M7b F1: fixtures divconst_*.fj comparam release x --sem-otim divconst (stdout, stderr, exit) e x backend C.
           1.3.0 - M7a F6: fixtures *checagem_*.fj comparam release x --sem-otim checagem (stdout, stderr, exit) e x backend C.
           1.2.0 - M7a F5: fixtures isel_*.fj comparam release x --sem-otim isel (stdout, stderr, exit) e x backend C; Compilar recebe o nome do passe desligado.
           1.1.0 - M7a F4: fixtures xmm_*.fj (real em xmm) tambem comparam stdout e exit com o backend C; minimo de fixtures 13.
           1.0.0 - M7a F3: primeira versao.
Verificacao: python -I verificar_alocador.py --selftest
"""

import argparse
import base64
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "rapido"))
import oraculo_congelado as Oraculo  # noqa: E402
import mascara_versao as Mascara  # noqa: E402

PastaFixtures = Path(__file__).resolve().parent / "alocador"
TIMEOUT_BUILD = 120
TIMEOUT_EXEC = 60
ARQUIVOS_ESPERADOS_MINIMO = 13
MANIFESTO = Path(__file__).resolve().parents[1] / "rapido" / "manifesto_aprovado.json"
PrefixoReal = "xmm_"  # fixtures de real: alem de ligado x desligado, comparam com o backend C
PrefixoIsel = "isel_"  # fixtures do isel (M7a F5): comparam com --sem-otim isel (nao com alocador) e com o backend C

PalavraChecagem = "checagem_"  # fixtures do passe checagem (M7a F6): comparam com --sem-otim checagem e com o backend C
PalavraCopiaArg = "copia_arg_"  # fixtures do passe copia_arg (M7a): comparam com --sem-otim copia_arg e com o backend C
PrefixoDivConst = "divconst_"  # fixtures do passe divconst (M7b F1): comparam com --sem-otim divconst e com o backend C
PrefixoInline = ("inline_", "trap_inline_")  # fixtures do passe inline (M7b F2): comparam com --sem-otim inline e com o backend C
PrefixoSroa = ("sroa_", "trap_sroa_")  # fixtures do passe sroa (M7b F3): comparam com --sem-otim sroa e com o backend C
PrefixoNumeracao = ("numeracao_", "trap_numeracao_")  # fixtures do passe numeracao (M7b R3): comparam com --sem-otim numeracao e com o backend C
PrefixoCfg = ("cfg_", "trap_cfg_")  # fixtures dos passes cfg e layout (M7b R4b): comparam com --sem-otim cfg,layout e com o backend C
PrefixoSelecao = ("selecao_", "trap_selecao_")  # fixtures do passe selecao (R5): comparam com --sem-otim selecao e com o backend C
PrefixoLicm = ("licm_", "trap_licm_")  # fixtures do passe licm (R6a): comparam com --sem-otim licm e com o backend C; as positivas (licm_ sem _neg_) exigem o remark do passe
RegexRemarkLicm = re.compile(r"licm fj_\S+: \d+ instrucoes subiram")
PrefixoEndereco = "endereco_"  # fixtures do passe endereco (R6a-3): comparam com --sem-otim endereco e com o backend C; as positivas (sem endereco_neg_) exigem o remark do passe
RegexRemarkEndereco = re.compile(r"endereco fj_\S+: \d+ cadeias recolocadas")
PrefixoSlp = ("slp_", "trap_slp_")  # fixtures do passe slp (S3): comparam com --sem-otim slp e com o backend C; as positivas (sem _neg_) exigem `slp fj_<proc>: N grupos vetorizados`, as negativas `slp recusou: <motivo>`
RegexRemarkSlp = re.compile(r"slp fj_\S+: \d+ grupos vetorizados")
RegexRecusaSlp = re.compile(r"slp recusou: (forma|operacao_mista|folhas_mistas|lanes_nao_contiguas|uso_externo|alias|chama_trap_ou_alias|ordem|valor_redefinido|valor_lido_duas_vezes|valores_diferentes|constantes_diferentes|bases_diferentes|operando|grande|copias|sem_carga_vetorial|slot_escalarizavel)")
PasseNeutro = ["numeracao", False]  # [passe desligado na neutralidade (--passe-neutro), desliga tambem no forja base (--passe-na-base)]
RegexEspera = re.compile(r"^//\s*espera:\s*(.+)$", re.MULTILINE)
RegexTermoEspera = re.compile(r"^(copia|expr|carga|mortos)(>|=)(\d+)$")
RegexRemarkNumeracao = re.compile(r"numeracao \S+: copia ebb=(\d+) dom=(\d+), expr ebb=(\d+) dom=(\d+), carga ebb=(\d+) dom=(\d+), mortos=(\d+)")
PrefixoRemark = "sroa_"  # positivas e negativas do sroa: exigem remark no --debug-code otim (exceto as de SemRemark)
SemRemark = ("sroa_simbolos.fj",)  # so confere a saida igual, com --simbolos sim
RemarkInlineR4 = {  # fixture do inline R4a -> remark exigido no --debug-code otim
    "inline_folha_laco.fj": r"inline fj_main <- fj_soma_ate b",
    "inline_folha_laco_trap.fj": r"inline fj_main <- fj_soma_vetor b",
    "inline_recursao_fib.fj": r"inline fj_fib <- fj_fib b",
    "trap_inline_recursao.fj": r"inline fj_desce <- fj_desce b",
    "inline_recursao_neg_laco.fj": r"inline recusa fj_soma_rec: laco",
    "inline_recursao_neg_crescimento.fj": r"inline recusa fj_pesada: tamanho",
}
ExtraPorFixture = {"sroa_simbolos.fj": ("--simbolos", "sim")}
FixturesPilha = ("trap_sroa_rastreio_inline.fj", "trap_numeracao_indice.fj", "endereco_trap_indice.fj")  # alem da saida, comparam a pilha de rastreio em build dev (x64 x C)
MensagemAusente = "passe sroa ausente"
PassoDesconhecido = "passo desconhecido"  # texto do forja quando --sem-otim recusa um passe que nao existe
RegexPositivo = re.compile(r"sroa fj_\S+ k\d+: \d+ B, \d+ campos")
RegexRecusa = re.compile(r"sroa recusa fj_\S+ k\d+: (escapa|endereco_multiplo|indice_dinamico|classe|sobreposicao|tamanho|campos|grupo|orcamento)")


def Compilar(Forja, Fonte, Saida, Checagem, SemPasse, Backend="x64", *, Extra=()):
    Comando = [Forja, "build", "--backend", Backend, "--release", "--checagem", Checagem, str(Fonte), "-o", str(Saida)]
    if SemPasse:
        Comando += ["--sem-otim", SemPasse]
    Comando += list(Extra)
    Resultado = subprocess.run(Comando, capture_output=True, timeout=TIMEOUT_BUILD)
    if Resultado.returncode != 0:
        Causa = (Resultado.stderr + Resultado.stdout).decode("utf-8", errors="replace").strip()[:300]
        raise RuntimeError(f"{Fonte.name}: build falhou (exit {Resultado.returncode}): {Causa}")


def Rodar(Exe):
    try:
        Resultado = subprocess.run([str(Exe)], capture_output=True, timeout=TIMEOUT_EXEC)
    except subprocess.TimeoutExpired:
        return ("timeout", b"", b"")
    return (Resultado.returncode, Resultado.stdout, Resultado.stderr)


def ConferirComBackendC(Forja, Fonte, Checagem, Temp, RCom):
    """Oraculo de real (M7a F4): o x64 com alocador tem de dar o mesmo stdout e exit que o backend C congelado (gcc so se faltar no JSON). None se igual."""
    Stdout, Saida = Oraculo.ObterSaidaEsperada(Fonte, Checagem)
    if (RCom[0], RCom[1]) == (Saida, Stdout):
        return None
    return f"x64 difere do backend C congelado: x64 exit={RCom[0]} stdout={RCom[1][:80]!r}; c exit={Saida} stdout={Stdout[:80]!r}"


def PasseDo(Fonte):
    """Passe desligado na comparacao: o prefixo da fixture escolhe o passe (convencao do cabecalho deste arquivo)."""
    Nome = Fonte.name
    if Nome.startswith(PrefixoNumeracao):
        return "numeracao"
    if Nome.startswith(PrefixoCfg):
        return "cfg,layout"
    if Nome.startswith(PrefixoSelecao):
        return "selecao"
    if Nome.startswith(PrefixoLicm):
        return "licm"
    if Nome.startswith(PrefixoEndereco):
        return "endereco"
    if Nome.startswith(PrefixoSlp):
        return "slp"
    if Nome.startswith(PrefixoInline):
        return "inline"
    if Nome.startswith(PrefixoDivConst):
        return "divconst"
    if Nome.startswith(PrefixoIsel):
        return "isel"
    if PalavraChecagem in Nome:
        return "checagem"
    if PalavraCopiaArg in Nome:
        return "copia_arg"
    if Nome.startswith(PrefixoSroa):
        return "sroa"
    return "alocador"


def ConferirRemarkNumeracao(Fonte, Debug):
    """Soma os remarks `numeracao ...` das funcoes e confere cada termo de `// espera: copia>0 expr=0 ...` da fixture (sem a linha, so exige que o passe tenha rodado). None se ok."""
    Texto = Debug.read_text(encoding="utf-8", errors="replace")
    if "passe numeracao" not in Texto:
        return "passe numeracao ausente no --debug-code otim"
    Soma = {"copia": 0, "expr": 0, "carga": 0, "mortos": 0}
    for Achado in RegexRemarkNumeracao.finditer(Texto):
        N = [int(X) for X in Achado.groups()]
        Soma["copia"] += N[0] + N[1]
        Soma["expr"] += N[2] + N[3]
        Soma["carga"] += N[4] + N[5]
        Soma["mortos"] += N[6]
    Linha = RegexEspera.search(Fonte.read_text(encoding="utf-8"))
    for Termo in (Linha.group(1).split() if Linha else []):
        Parte = RegexTermoEspera.match(Termo)
        if Parte is None:
            return f"termo invalido em '// espera:': {Termo}"
        Classe, Op, Valor = Parte.group(1), Parte.group(2), int(Parte.group(3))
        if not (Soma[Classe] > Valor if Op == ">" else Soma[Classe] == Valor):
            return f"remark numeracao: esperado {Termo}, obtido {Classe}={Soma[Classe]}"
    return None


def ConferirRemark(Fonte, Com):
    """Positiva exige `sroa fj_<proc> k<N>: <tam> B, <campos> campos`; negativa exige `sroa recusa fj_<proc> k<N>: <motivo>` no --debug-code otim. Fixture numeracao: ver ConferirRemarkNumeracao. None se ok."""
    Nome = Fonte.name
    if Nome.startswith(PrefixoNumeracao):
        Debug = Path(str(Com) + ".debug.txt")
        return ConferirRemarkNumeracao(Fonte, Debug) if Debug.is_file() else "--debug-code otim nao gravou o .debug.txt"
    if Nome.startswith("licm_") and not Nome.startswith("licm_neg_"):
        Debug = Path(str(Com) + ".debug.txt")
        if not Debug.is_file():
            return "--debug-code otim nao gravou o .debug.txt"
        return None if RegexRemarkLicm.search(Debug.read_text(encoding="utf-8", errors="replace")) else "remark ausente no --debug-code otim: esperado 'licm fj_<proc>: N instrucoes subiram'"
    if Nome.startswith(PrefixoEndereco) and not Nome.startswith("endereco_neg_"):
        Debug = Path(str(Com) + ".debug.txt")
        if not Debug.is_file():
            return "--debug-code otim nao gravou o .debug.txt"
        return None if RegexRemarkEndereco.search(Debug.read_text(encoding="utf-8", errors="replace")) else "remark ausente no --debug-code otim: esperado 'endereco fj_<proc>: N cadeias recolocadas'"
    if Nome.startswith("trap_slp_"):
        return None  # o trap fecha o bloco (a checagem de indice o parte): so a saida e a pilha contam
    if Nome.startswith(PrefixoSlp):
        Debug = Path(str(Com) + ".debug.txt")
        if not Debug.is_file():
            return "--debug-code otim nao gravou o .debug.txt"
        Texto = Debug.read_text(encoding="utf-8", errors="replace")
        if "slp_neg_" in Nome:
            return None if RegexRecusaSlp.search(Texto) and not RegexRemarkSlp.search(Texto) else "remark ausente (ou grupo vetorizado) no --debug-code otim: esperado 'slp recusou: <motivo>' e nenhum 'slp fj_<proc>: N grupos vetorizados'"
        return None if RegexRemarkSlp.search(Texto) else "remark ausente no --debug-code otim: esperado 'slp fj_<proc>: N grupos vetorizados'"
    if not (Nome in RemarkInlineR4 or (Nome.startswith(PrefixoRemark) and Nome not in SemRemark)):
        return None
    Debug = Path(str(Com) + ".debug.txt")
    if not Debug.is_file():
        return "--debug-code otim nao gravou o .debug.txt"
    Texto = Debug.read_text(encoding="utf-8", errors="replace")
    if Nome in RemarkInlineR4:
        return None if re.search(RemarkInlineR4[Nome], Texto) else f"remark ausente no --debug-code otim: esperado '{RemarkInlineR4[Nome]}'"
    if Nome.startswith("sroa_neg_"):
        return None if RegexRecusa.search(Texto) else "remark ausente no --debug-code otim: esperado 'sroa recusa fj_<proc> k<N>: <motivo>'"
    return None if RegexPositivo.search(Texto) else "remark ausente no --debug-code otim: esperado 'sroa fj_<proc> k<N>: <tam> B, <campos> campos'"


def ConferirEsperado(Fonte, RCom):
    """Se a fixture tem .esperado ao lado (gerado do backend C), o stdout do x64 tem de bater. None se ok ou sem arquivo."""
    Arquivo = Fonte.with_suffix(".esperado")
    if not Arquivo.is_file():
        return None
    Esperado = Arquivo.read_bytes()
    return None if RCom[1] == Esperado else f"stdout difere do .esperado: obtido={RCom[1][:80]!r} esperado={Esperado[:80]!r}"


def PilhaDev(Forja, Fonte, Backend, Temp):
    """(exit, pilha) de um build dev com --rastreio; a pilha e o campo 'pilha' do JSON que o R-code grava no stderr."""
    Exe = Path(Temp) / f"dev_{Backend}.exe"
    Resultado = subprocess.run([Forja, "build", "--backend", Backend, "--rastreio", str(Fonte), "-o", str(Exe)], capture_output=True, timeout=TIMEOUT_BUILD)
    if Resultado.returncode != 0:
        raise RuntimeError(f"{Fonte.name}: build dev {Backend} falhou (exit {Resultado.returncode})")
    Rc, _, Erro = Rodar(Exe)
    try:
        Pilha = json.loads(Erro.decode("utf-8", errors="replace").strip()).get("pilha")
    except ValueError:
        Pilha = None
    return Rc, Pilha


def TextoPilhaRef(Pilha):
    # Mesmo formato do TextoPilha de capturar_ref_gcc (que grava o BancoRef): texto como veio, o resto em JSON canonico.
    return Pilha if isinstance(Pilha, str) else json.dumps(Pilha, ensure_ascii=False, sort_keys=True)


def ConferirPilhaDev(Forja, Fonte, Temp):
    """x64 dev com --rastreio tem de dar o mesmo (exit, pilha) do registro congelado do BancoRef (classe pilha, modo dev; gcc so na captura). None se ok."""
    Rc, Pilha = PilhaDev(Forja, Fonte, "x64", Temp)
    Registro = Oraculo.BancoRef().Obter(Fonte, "dev", (), False, "dev", "pilha")
    Congelado = (Registro["exit"], base64.b64decode(Registro["stdout"]).decode("utf-8"))
    if (Rc, TextoPilhaRef(Pilha)) == Congelado:
        return None
    return f"pilha de rastreio difere do registro congelado: x64 exit={Rc} pilha={TextoPilhaRef(Pilha)[:120]!r}; congelado exit={Congelado[0]} pilha={Congelado[1][:120]!r}"


def PrecisaBackendC(Nome):
    """Fixtures que alem de ligado x desligado comparam com o backend C."""
    return Nome.startswith((PrefixoReal, PrefixoIsel, PrefixoDivConst) + PrefixoInline + PrefixoSroa + PrefixoNumeracao + PrefixoCfg + PrefixoLicm + PrefixoSlp) or PalavraChecagem in Nome or PalavraCopiaArg in Nome


def ConferirExtras(Forja, Fonte, Checagem, Temp, Com, RCom):
    """Checagens depois da igualdade com o passe desligado: remark, .esperado, backend C e pilha dev. None se ok."""
    Falha = ConferirRemark(Fonte, Com) or ConferirEsperado(Fonte, RCom)
    if not Falha and PrecisaBackendC(Fonte.name):
        Falha = ConferirComBackendC(Forja, Fonte, Checagem, Temp, RCom)
    if not Falha and Fonte.name in FixturesPilha:
        Falha = ConferirPilhaDev(Forja, Fonte, Temp)
    return Falha


def ChaveManifesto(Fonte, Checagem):
    return f"{Fonte.name}|{Checagem}"


def ShaEntradas(Fonte):
    """sha da fonte e do .esperado ao lado: o exe igual so vale se as entradas da comparacao tambem forem as mesmas."""
    Esperado = Fonte.with_suffix(".esperado")
    return Oraculo.ShaBytes(Fonte.read_bytes() + b"\0" + (Esperado.read_bytes() if Esperado.is_file() else b""))


def LerManifesto():
    try:
        return json.loads(MANIFESTO.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def VerificarCaso(Forja, Fonte, Checagem, Manifesto):
    """(falha, impressao, pulou): falha None se igual; impressao = {exe, entradas} do release (None se nao compilou); pulou True quando o exe e igual ao aprovado.
    Passe sroa ausente vira falha com a mensagem certa, sem quebrar."""
    with tempfile.TemporaryDirectory(prefix="verificar_alocador_") as Temp:
        Com, Sem = Path(Temp) / "com.exe", Path(Temp) / "sem.exe"
        Passe = PasseDo(Fonte)
        Extra = ExtraPorFixture.get(Fonte.name, ())
        PedeRemark = (Fonte.name.startswith(PrefixoRemark) and Fonte.name not in SemRemark) or Fonte.name.startswith(PrefixoNumeracao) or Fonte.name.startswith(PrefixoSlp) or Fonte.name in RemarkInlineR4 or (Fonte.name.startswith("licm_") and not Fonte.name.startswith("licm_neg_")) or (Fonte.name.startswith(PrefixoEndereco) and not Fonte.name.startswith("endereco_neg_"))
        Debug = ("--debug-code", "otim") if PedeRemark else ()
        Compilar(Forja, Fonte, Com, Checagem, None, Extra=Extra + Debug)
        Impressao = {"exe": Mascara.HashSemVersao(Com, Mascara.VersoesDoForja(Forja)), "entradas": ShaEntradas(Fonte)}
        if Manifesto.get(ChaveManifesto(Fonte, Checagem)) == Impressao:
            return ConferirRemark(Fonte, Com), Impressao, True
        try:
            Compilar(Forja, Fonte, Sem, Checagem, Passe, Extra=Extra)
        except RuntimeError as Erro:
            if Passe == "sroa" and PassoDesconhecido in str(Erro):
                return MensagemAusente, None, False
            raise
        RCom, RSem = Rodar(Com), Rodar(Sem)
        if RCom != RSem:
            return f"saida difere: com {Passe} exit={RCom[0]} stdout={RCom[1][:80]!r}; sem exit={RSem[0]} stdout={RSem[1][:80]!r}", Impressao, False
        return ConferirExtras(Forja, Fonte, Checagem, Temp, Com, RCom), Impressao, False


def ConferirNeutralidade(Forja, Base, Fonte, Checagem):
    """O forja novo com --sem-otim numeracao tem de gerar, byte a byte (mascara_versao.HashSemVersao tira a versao de cada forja), o mesmo exe release que o forja de antes da fatia. None se igual."""
    with tempfile.TemporaryDirectory(prefix="verificar_alocador_n_") as Temp:
        ExeBase, ExeNovo = Path(Temp) / "base.exe", Path(Temp) / "novo.exe"
        Compilar(Base, Fonte, ExeBase, Checagem, PasseNeutro[0] if PasseNeutro[1] else None)
        Compilar(Forja, Fonte, ExeNovo, Checagem, PasseNeutro[0])
        Versoes = Mascara.VersoesDoForja(Forja, Base)
        if Mascara.HashSemVersao(ExeBase, Versoes) == Mascara.HashSemVersao(ExeNovo, Versoes):
            return None
        return "exe com --sem-otim numeracao difere do exe do forja base"


def Neutralidade(Forja, Base, Casos):
    """--contra: confere ConferirNeutralidade em todos os casos; imprime as falhas e devolve quantas."""
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as Pool:
        Futuros = [(F, C, Pool.submit(ConferirNeutralidade, Forja, Base, F, C)) for F, C in Casos]
        Falhas = [(F, C, Fu.result()) for F, C, Fu in Futuros]
    Ruins = [(F, C, Causa) for F, C, Causa in Falhas if Causa is not None]
    for F, C, Causa in Ruins:
        print(f"FALHOU neutralidade {F.name} --checagem {C}: {Causa}")
    print(f"{'OK' if not Ruins else 'FALHOU'}: neutralidade {len(Casos) - len(Ruins)}/{len(Casos)} exes iguais ao base com --sem-otim {PasseNeutro[0]}")
    return len(Ruins)


def Principal(Forja, UsarManifesto=True, Base=None):
    Fontes = sorted(PastaFixtures.glob("*.fj"))
    if len(Fontes) < ARQUIVOS_ESPERADOS_MINIMO:
        print(f"FALHOU: so {len(Fontes)} fixtures em {PastaFixtures}")
        return 1
    # trap_*: sem --checagem sim o acesso fora do limite e comportamento indefinido (o layout do quadro muda a saida), entao so valem com ela
    Casos = [(F, C) for F in Fontes for C in (("sim",) if F.name.startswith("trap_") or F.name.endswith("_trap.fj") else ("nao", "sim"))]
    Anterior = LerManifesto() if UsarManifesto else {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as Pool:
        Futuros = [(F, C, Pool.submit(VerificarCaso, Forja, F, C, Anterior)) for F, C in Casos]
        Resultados = [(F, C) + Fu.result() for F, C, Fu in Futuros]
    Falhas = [(f"{F.name} --checagem {C}", Falha) for F, C, Falha, _, _ in Resultados if Falha is not None]
    for Nome, Causa in Falhas:
        print(f"FALHOU {Nome}: {Causa}")
    Pulados = sum(1 for *_, Pulou in Resultados if Pulou)
    Verdes = {ChaveManifesto(F, C): Imp for F, C, Falha, Imp, _ in Resultados if Falha is None and Imp}
    if UsarManifesto and Verdes:
        MANIFESTO.write_text(json.dumps(Verdes, sort_keys=True, indent=0) + "\n", encoding="utf-8", newline="\n")
    print(f"{'OK' if not Falhas else 'FALHOU'}: {len(Casos) - len(Falhas)}/{len(Casos)} casos iguais ({Pulados} com exe igual ao aprovado, sem reexecutar)")
    NaoNeutros = Neutralidade(Forja, Base, Casos) if Base else 0
    return 0 if not Falhas and NaoNeutros == 0 else 1


def Selftest():
    Falhas = []
    if len(list(PastaFixtures.glob("*.fj"))) < ARQUIVOS_ESPERADOS_MINIMO:
        Falhas.append("faltam fixtures em alocador")
    print("selftest: " + ("OK" if not Falhas else "; ".join(Falhas)))
    return 0 if not Falhas else 1


def Main():
    Parser = argparse.ArgumentParser(description="Stress do alocador de registradores (liga x desliga).")
    Parser.add_argument("--forja", default=None, help="forja.exe (caminho absoluto)")
    Parser.add_argument("--sem-manifesto", action="store_true", help="ignora o manifesto de exes aprovados (reexecuta tudo) e nao o regrava")
    Parser.add_argument("--contra", default=None, help="forja.exe de ANTES da fatia (caminho absoluto): confere a neutralidade byte a byte de --sem-otim numeracao")
    Parser.add_argument("--passe-neutro", default="numeracao", help="passe desligado no forja novo na neutralidade do --contra (padrao numeracao; use inline quando a base tem o numeracao)")
    Parser.add_argument("--passe-na-base", action="store_true", help="desliga o --passe-neutro tambem no forja base (base que ja tem o passe: compara base e novo, ambos sem ele)")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    PasseNeutro[0], PasseNeutro[1] = Args.passe_neutro, Args.passe_na_base
    if Args.selftest:
        return Selftest()
    if not Args.forja or not os.path.isabs(Args.forja) or not os.path.isfile(Args.forja):
        print("--forja precisa ser caminho absoluto de um arquivo existente", file=sys.stderr)
        return 2
    if Args.contra and (not os.path.isabs(Args.contra) or not os.path.isfile(Args.contra)):
        print("--contra precisa ser caminho absoluto de um arquivo existente", file=sys.stderr)
        return 2
    return Principal(Args.forja, not Args.sem_manifesto, Args.contra)


if __name__ == "__main__":
    sys.exit(Main())
