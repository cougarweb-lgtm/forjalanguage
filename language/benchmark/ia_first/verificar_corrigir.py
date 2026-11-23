# verificar_corrigir.py -- confere o comando `forja corrigir` (contrato IA1, P-IA1-b05; metricas M-D2, M-D13 e M-D6)
"""Roda `forja corrigir` em COPIAS dos err_* do lote 1 (E0001 do ';', E0707, E0708, E0709), sem modelo, e confere:
  dry_run   sem --aplicar o arquivo fica byte a byte igual, o JSON traz as chaves na ordem fixa e o diff unificado quando ha correcao
  M-D2      com --aplicar: >= 85% dos casos com correcao certa chegam a exit 0 no check; 100% nao pioram (medido por um check INDEPENDENTE
            do arquivo, nao pelo que o comando diz): o numero de erros nao sobe e o diagnostico original some
  M-D13     e arquivo intacto (hash igual) em todo caso em que o comando diz que desfez
  M-D6      2 execucoes do mesmo comando dao a mesma saida byte a byte
  plantado  arquivo cuja correcao certa faria o numero de erros subir (o ';' que esconde dois erros de semantica): o arquivo fica identico, exit 1,
            "desfeito": true e motivo explicito
  bom_crlf  arquivo com BOM e CRLF: so o ';' sai, BOM e CRLF ficam
  filtro    --codigo X escolhe so as correcoes de X
  uso       arquivo inexistente, opcao desconhecida, codigo desconhecido e falta de arquivo dao exit 2
Uso:
    python -I verificar_corrigir.py --forja <forja.exe> [--lang <pasta language>]
    python -I verificar_corrigir.py --selftest
Saida: uma linha por metrica {id, valor, verde}; exit 0 so se todas verdes (1 = alguma vermelha, 2 = uso).
Versao: 0.2.0
Changelog: 0.2.0 - P-IA1-b06: --lote 2 roda o M-D2/M-D13/M-D6 nos err_* do lote 2 (verificar_sugestoes_l2.ListarLote2); a copia do caso leva as pastas irmas (mods/).
           0.1.0 - P-IA1-b05: primeira versao.
Verificacao: python -I verificar_corrigir.py --selftest
"""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verificar_sugestoes as VS  # noqa: E402  lote 1 e leitor de diagnosticos (mesma pasta)
import verificar_sugestoes_l2 as VS2  # noqa: E402  lote 2 (--lote 2) e a copia do caso com as pastas irmas

CHAVES = ["comando", "arquivo", "aplicar", "codigo", "confianca", "erros_antes", "correcoes", "puladas", "edicoes", "diff", "aplicado", "desfeito",
          "erros_depois", "motivo"]
TEMPO_MAX = 90
MINIMO_EXIT0 = 0.85
FONTE_PLANTADA = b"proc a() {\n    let y = zz\n}\n\nproc b() {\n    let w = qq\n}\n\nproc main() {\n    let x = 1;\n}\n"
FONTE_DOIS_CODIGOS = b"proc a() {\n    var n = 3\n    print(n)\n}\n\nproc b() {\n    let sobra = 2\n}\n\nproc main() {\n    a()\n    b()\n}\n"
FONTE_BOM_CRLF = b"\xef\xbb\xbfproc main() {\r\n    let x = 1;\r\n    print(x)\r\n}\r\n"


def Roda(Forja, Argumentos, Pasta):
    """(exit, stdout, stderr) de uma execucao do forja em Pasta, sem stdin."""
    Proc = subprocess.run([str(Forja), *Argumentos], capture_output=True, cwd=Pasta, stdin=subprocess.DEVNULL, timeout=TEMPO_MAX)
    return Proc.returncode, Proc.stdout.decode("utf-8", errors="replace"), Proc.stderr.decode("utf-8", errors="replace")


def HashDe(Caminho):
    return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()


def LerJson(Saida):
    """O objeto JSON da saida (uma linha); None se nao for."""
    try:
        Objeto = json.loads(Saida)
    except ValueError:
        return None
    return Objeto if isinstance(Objeto, dict) else None


def ProblemasDoEsquema(Obj):
    """Chaves na ordem fixa e tipos basicos do resultado do corrigir."""
    if Obj is None:
        return ["saida nao e um objeto JSON"]
    if list(Obj.keys()) != CHAVES:
        return ["chaves fora da ordem do esquema: %r" % list(Obj.keys())]
    Problemas = []
    if not isinstance(Obj["correcoes"], list) or not isinstance(Obj["diff"], str):
        Problemas.append("correcoes ou diff com tipo errado")
    elif Obj["correcoes"] and not Obj["diff"].startswith("--- a/"):
        Problemas.append("ha correcao mas o diff nao e unificado")
    if Obj["edicoes"] == 0 and Obj["diff"] != "":
        Problemas.append("diff sem edicao")
    return Problemas


def ContaErros(Forja, Pasta, Nome):
    """(exit, diagnosticos) de um check independente."""
    Codigo, Saida, _ = Roda(Forja, ["check", "--max-erros", "100", Nome], Pasta)
    return Codigo, [D for D in VS.LerDiagnosticos(Saida) if "codigo" in D]


def ProblemasDeDesfeito(Obj, Codigo, HashAntes, HashDepois):
    """Quando o comando diz que desfez: exit 1, motivo explicito e arquivo identico ao original."""
    Problemas = []
    if Codigo != 1:
        Problemas.append("desfez mas o exit foi %d" % Codigo)
    if not Obj.get("motivo"):
        Problemas.append("desfez sem motivo")
    if Obj.get("aplicado"):
        Problemas.append("aplicado e desfeito ao mesmo tempo")
    if HashAntes != HashDepois:
        Problemas.append("desfez mas o arquivo nao e identico ao original")
    return Problemas


def ProblemasDePiora(AntesDiags, DepoisDiags, Original):
    """Medida independente: o numero de erros nao sobe e o diagnostico original some."""
    Problemas = []
    if len(DepoisDiags) > len(AntesDiags):
        Problemas.append("erros subiram de %d para %d" % (len(AntesDiags), len(DepoisDiags)))
    if AntesDiags and any((D["codigo"], D["linha"]) == (Original["codigo"], Original["linha"]) for D in DepoisDiags):
        Problemas.append("o diagnostico original continua")
    return Problemas


def ConferirDryRun(Forja, Copia, Pasta):
    """Sem --aplicar: arquivo intacto, esquema, exit 0 e saida igual em 2 execucoes (retorna problemas de dry_run e de determinismo)."""
    Antes = HashDe(Copia)
    Codigo, Saida, _ = Roda(Forja, ["corrigir", Copia.name], Pasta)
    Problemas = ProblemasDoEsquema(LerJson(Saida))
    if Codigo != 0:
        Problemas.append("exit %d, esperado 0" % Codigo)
    if HashDe(Copia) != Antes:
        Problemas.append("o arquivo mudou sem --aplicar")
    Determinismo = [] if Roda(Forja, ["corrigir", Copia.name], Pasta)[1] == Saida else ["2 execucoes diferem"]
    return Problemas, Determinismo


def ConferirAplicar(Forja, Copia, Pasta):
    """Com --aplicar numa copia: dict com piora, desfez_ok e exit0 (None = sem correcao certa)."""
    Original = Copia.read_bytes()
    HashAntes = HashDe(Copia)
    _, Diags = ContaErros(Forja, Pasta, Copia.name)
    Codigo, Saida, _ = Roda(Forja, ["corrigir", Copia.name, "--aplicar"], Pasta)
    Obj = LerJson(Saida)
    Problemas = ProblemasDoEsquema(Obj)
    if Obj is None or Problemas:
        return {"piora": Problemas, "desfez": [], "exit0": None}
    if Obj["desfeito"]:
        return {"piora": [], "desfez": ProblemasDeDesfeito(Obj, Codigo, HashAntes, HashDe(Copia)), "exit0": None}
    if not Obj["aplicado"]:
        return {"piora": [], "desfez": [], "exit0": None if Copia.read_bytes() == Original else False}
    CodigoDepois, DiagsDepois = ContaErros(Forja, Pasta, Copia.name)
    Piora = ProblemasDePiora(Diags, DiagsDepois, Diags[0]) if Diags else []
    if Obj["erros_depois"] != len(DiagsDepois):
        Piora.append("erros_depois=%r mas o check conta %d" % (Obj["erros_depois"], len(DiagsDepois)))
    return {"piora": Piora, "desfez": [], "exit0": CodigoDepois == 0}


def ConferirCaso(Forja, Fj):
    """Achados de um err_* do lote em copia propria."""
    with tempfile.TemporaryDirectory(prefix="verif_corrigir_") as Tmp:
        Copia = Path(Tmp) / Fj.name
        VS2.CopiarCaso(Fj, Copia)
        DryRun, Determinismo = ConferirDryRun(Forja, Copia, Tmp)
        Achados = ConferirAplicar(Forja, Copia, Tmp)
    Achados.update({"arquivo": Fj.name, "dry_run": DryRun, "determinismo": Determinismo})
    return Achados


def ConferirPlantado(Forja):
    """A correcao certa faria os erros subirem de 1 para 2: o arquivo volta identico e o comando diz por que."""
    with tempfile.TemporaryDirectory(prefix="verif_corrigir_") as Tmp:
        Copia = Path(Tmp) / "plantado.fj"
        Copia.write_bytes(FONTE_PLANTADA)
        HashAntes = HashDe(Copia)
        Codigo, Saida, Erro = Roda(Forja, ["corrigir", Copia.name, "--aplicar"], Tmp)
        Obj = LerJson(Saida)
        if Obj is None:
            return ["saida nao e JSON"]
        Problemas = ProblemasDoEsquema(Obj)
        if not Obj["desfeito"]:
            Problemas.append("o plantado nao foi desfeito")
        Problemas += ProblemasDeDesfeito(Obj, Codigo, HashAntes, HashDe(Copia))
        if "desfeita" not in Erro:
            Problemas.append("o stderr nao diz que a correcao foi desfeita")
        return Problemas


def ConferirBomCrlf(Forja):
    """So o ';' sai: BOM e CRLF ficam, byte a byte."""
    with tempfile.TemporaryDirectory(prefix="verif_corrigir_") as Tmp:
        Copia = Path(Tmp) / "bom.fj"
        Copia.write_bytes(FONTE_BOM_CRLF)
        Codigo, Saida, _ = Roda(Forja, ["corrigir", Copia.name, "--aplicar"], Tmp)
        Esperado = FONTE_BOM_CRLF.replace(b"1;\r\n", b"1\r\n")
        Problemas = [] if Codigo == 0 else ["exit %d" % Codigo]
        if Copia.read_bytes() != Esperado:
            Problemas.append("o arquivo nao e o original sem o ';': %r" % Copia.read_bytes())
        Obj = LerJson(Saida)
        if Obj is None or not Obj.get("aplicado"):
            Problemas.append("nao aplicou")
        return Problemas


def ConferirFiltro(Forja):
    """Sem filtro as duas correcoes; --codigo X so as de X; o dry-run nao grava."""
    Problemas = []
    with tempfile.TemporaryDirectory(prefix="verif_corrigir_") as Tmp:
        Copia = Path(Tmp) / "dois.fj"
        Copia.write_bytes(FONTE_DOIS_CODIGOS)
        for Argumentos, Esperados in (([], ["E0707", "E0708"]), (["--codigo", "E0707"], ["E0707"]), (["--codigo", "E0708"], ["E0708"]), (["--codigo", "E0709"], [])):
            Obj = LerJson(Roda(Forja, ["corrigir", Copia.name, *Argumentos], Tmp)[1])
            Codigos = [C["codigo"] for C in Obj["correcoes"]] if Obj else None
            if Codigos != Esperados:
                Problemas.append("%r deu %r, esperado %r" % (Argumentos, Codigos, Esperados))
        Original = Copia.read_bytes()
        Roda(Forja, ["corrigir", Copia.name, "--codigo", "E0708", "--aplicar"], Tmp)
        Depois = Copia.read_bytes()
        if b"var n" not in Depois or b"let sobra" in Depois or Depois == Original:
            Problemas.append("--aplicar com --codigo E0708 mexeu no que nao devia: %r" % Depois)
    return Problemas


def ConferirUso(Forja):
    """Erros de uso: exit 2 e mensagem em PT-BR no stderr."""
    Problemas = []
    with tempfile.TemporaryDirectory(prefix="verif_corrigir_") as Tmp:
        (Path(Tmp) / "a.fj").write_bytes(b"proc main() {\n}\n")
        for Argumentos in (["nao_existe.fj"], ["a.fj", "--foo"], ["a.fj", "--codigo", "E9999"], ["a.fj", "--codigo"], [], ["a.fj", "b.fj"]):
            Codigo, Saida, Erro = Roda(Forja, ["corrigir", *Argumentos], Tmp)
            if Codigo != 2 or Saida or not Erro.startswith("forja:"):
                Problemas.append("%r: exit %d, stdout %r, stderr %r" % (Argumentos, Codigo, Saida, Erro[:60]))
    return Problemas


def Resumir(Achados, Plantado, BomCrlf, Filtro, Uso):
    """Metricas a partir dos achados: lista de (id, valor, verde)."""
    Falhas = lambda Chave: [A["arquivo"] + ": " + P for A in Achados for P in A[Chave]]  # noqa: E731
    Certos = [A for A in Achados if A["exit0"] is not None]
    Verdes = sum(1 for A in Certos if A["exit0"])
    Taxa = Verdes / len(Certos) if Certos else 0.0
    Piora = Falhas("piora")
    return [
        ("dry_run", {"casos": len(Achados), "problemas": Falhas("dry_run")}, not Falhas("dry_run") and bool(Achados)),
        ("M-D2", {"exit0_apos_aplicar": Verdes, "casos_com_correcao": len(Certos), "taxa": round(Taxa, 3), "piora": Piora}, not Piora and Taxa >= MINIMO_EXIT0),
        ("M-D13", {"problemas": Falhas("desfez") + Piora}, not Falhas("desfez") and not Piora),
        ("M-D6", {"problemas": Falhas("determinismo")}, not Falhas("determinismo")),
        ("plantado", {"problemas": Plantado}, not Plantado),
        ("bom_crlf", {"problemas": BomCrlf}, not BomCrlf),
        ("filtro", {"problemas": Filtro}, not Filtro),
        ("uso", {"problemas": Uso}, not Uso),
    ]


def Selftest():
    """O bom passa e cada defeito plantado nas conferencias e acusado (nao precisa de forja.exe)."""
    Falhou = 0
    Bom = dict(zip(CHAVES, ["corrigir", "a.fj", True, None, "certa", 1, [{"id": "x"}], 0, 1, "--- a/a.fj\n+++ b/a.fj\n", True, False, 0, None]))
    Desfeito = dict(Bom, aplicado=False, desfeito=True, erros_depois=2, motivo="o numero de erros subiu de 1 para 2")
    Um = [{"codigo": "E0001", "linha": 2}]
    Dois = [{"codigo": "E0001", "linha": 2}, {"codigo": "E0201", "linha": 5}]
    Provas = [
        ("esquema bom", not ProblemasDoEsquema(Bom)),
        ("chave fora de ordem", bool(ProblemasDoEsquema(dict(reversed(list(Bom.items())))))),
        ("diff vazio com correcao", bool(ProblemasDoEsquema(dict(Bom, diff="")))),
        ("saida que nao e JSON", bool(ProblemasDoEsquema(LerJson("lixo")))),
        ("desfeito bom", not ProblemasDeDesfeito(Desfeito, 1, "h", "h")),
        ("desfeito com arquivo diferente", bool(ProblemasDeDesfeito(Desfeito, 1, "h", "outro"))),
        ("desfeito com exit 0", bool(ProblemasDeDesfeito(Desfeito, 0, "h", "h"))),
        ("desfeito sem motivo", bool(ProblemasDeDesfeito(dict(Desfeito, motivo=None), 1, "h", "h"))),
        ("piora ausente", not ProblemasDePiora(Um, [], Um[0])),
        ("erros subiram (o plantado)", bool(ProblemasDePiora(Um, Dois, Um[0]))),
        ("original continua", bool(ProblemasDePiora(Um, Um, Um[0]))),
    ]
    for Nome, Acusou in Provas:
        if not Acusou:
            print("selftest: falhou: " + Nome)
            Falhou += 1
    print("selftest: %s" % ("ok" if Falhou == 0 else "%d falha(s)" % Falhou))
    return Falhou


def Principal():
    Parser = argparse.ArgumentParser(description="Confere o forja corrigir (P-IA1-b05).")
    Parser.add_argument("--forja")
    Parser.add_argument("--lang", default=str(Path(__file__).resolve().parents[2]))
    Parser.add_argument("--lote", type=int, choices=(1, 2), default=1, help="1 = E0001 do ';', E0707..E0709 (padrao); 2 = lote 2 do b06")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 1 if Selftest() else 0
    if not Args.forja:
        Parser.error("--forja e obrigatorio")
    Forja = Path(Args.forja).resolve()
    Lote = VS2.ListarLote2(Path(Args.lang)) if Args.lote == 2 else VS.ListarLote(Path(Args.lang))
    Achados = [ConferirCaso(Forja, Fj) for Fj, _ in Lote]
    Metricas = Resumir(Achados, ConferirPlantado(Forja), ConferirBomCrlf(Forja), ConferirFiltro(Forja), ConferirUso(Forja))
    for Id, Valor, Verde in Metricas:
        print(json.dumps({"id": Id, "valor": Valor, "verde": Verde}, ensure_ascii=False))
    print("casos=%d falhas=%d" % (len(Lote), sum(1 for _, _, V in Metricas if not V)))
    return 0 if all(V for _, _, V in Metricas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
