"""Casos de cli do forja editar (IA1-I4): renomear em dry-run e com --aplicar, a operacao que piora e e desfeita (M-E2) e os usos errados.

Importado pelo motor verificar_cli.py; a prova ampla (30 operacoes, 0 referencia esquecida, gabarito, M-E2 byte a byte) mora em tests\\editar\\verificar_editar.py;
aqui ficam os casos curtos do motor, numa copia temporaria da fixture tests\\editar\\proj.

Regras:
  - sem --aplicar: 1 linha de JSON (editar_versao, operacoes, edicoes, diff, erros_antes/depois, valido, aplicado=false) e 0 arquivo gravado; 2 execucoes = bytes iguais;
  - com --aplicar: grava, aplicado=true, e `forja check` do projeto sai 0;
  - operacao que deixaria erro novo (colisao de nome, E0202) = exit 1, desfeito=true, o diagnostico do check em "diagnosticos" e os arquivos intactos;
  - uso errado (sem --ops, ops.json ausente ou invalido, operacao desconhecida, opcao desconhecida) = exit 2.

  - IA1-I5: inserir_decl em dry-run (diff com a declaracao nova, 0 arquivo gravado) e com --aplicar; mover_decl entre modulos ajusta os imports e o check sai 0; remover_decl de
    declaracao ainda usada e recusado (exit 1, desfeito=false, motivo com as referencias, 0 arquivo alterado). A prova ampla (60 operacoes com gabarito a mao) e tests\\editar\\verificar_editar_i5.py.

Versao: 1.1.0
Changelog: 1.1.0 - IA1-I5: +3 casos (inserir_decl, mover_decl e remover_decl recusado; fixture tests\\editar\\proj2).
           1.0.0 - IA1-I4: primeira versao (3 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import hashlib
import json
import shutil
import tempfile
from pathlib import Path

from verificar_cli import ConferirExecucao, PastaAqui, Rodar

FIXTURE = PastaAqui.parent / "editar" / "proj"
FIXTURE2 = PastaAqui.parent / "editar" / "proj2"
INSERIR = [{"op": "inserir_decl", "depois_de": "somar", "texto": "pub proc subtrair(a: i32, b: i32) -> i32 {\n    return a - b\n}"}]
MOVER = [{"op": "mover_decl", "id": "dobrar", "para": "util/texto"}]
REMOVER_EM_USO = [{"op": "remover_decl", "id": "somar"}]
RENOMEAR =[{"op": "renomear", "id": "geo/vetor:somar_vetor", "novo": "juntar_vetores"}]
COLISAO = [{"op": "renomear", "id": "atualizar", "novo": "faixa_de"}]


def CopiaComOps(Lista, Fixture=FIXTURE):
    """(raiz temporaria, pasta do projeto, ops.json): o projeto e uma copia da fixture; o ops.json fica ao lado, fora dele."""
    Raiz = Path(tempfile.mkdtemp(prefix="cli_editar_"))
    Pasta = Raiz / "proj"
    shutil.copytree(Fixture, Pasta)
    Ops = Raiz / "ops.json"
    Ops.write_text(json.dumps(Lista), encoding="utf-8")
    return Raiz, Pasta, Ops


def HashPasta(Pasta):
    return {A.relative_to(Pasta).as_posix(): hashlib.sha256(A.read_bytes()).hexdigest() for A in sorted(Pasta.rglob("*")) if A.is_file()}


def JsonDe(Rodada, Problemas):
    try:
        return json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Problemas.append(f"stdout nao e 1 linha de JSON: {Rodada.stdout[:100]!r}")
        return None


def CasoEditarDryRun(Forja, PastaSaidas):
    Problemas = []
    Raiz, Pasta, Ops = CopiaComOps(RENOMEAR)
    try:
        Antes = HashPasta(Pasta)
        Primeira = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops)])
        Segunda = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops)])
        Problemas += ConferirExecucao(Primeira, 0)
        Resposta = JsonDe(Primeira, Problemas)
        if Resposta is not None:
            Faltam = [C for C in ("editar_versao", "operacoes", "edicoes", "diff", "erros_antes", "erros_depois", "valido", "aplicado", "desfeito") if C not in Resposta]
            if Faltam or Resposta["aplicado"] or not Resposta["valido"] or len(Resposta["edicoes"]) != 2:
                Problemas.append(f"dry-run fora do esperado: faltam {Faltam}, aplicado={Resposta.get('aplicado')}, edicoes={len(Resposta.get('edicoes', []))} (esperado 2)")
        if Primeira.stdout != Segunda.stdout:
            Problemas.append("2 execucoes deveriam dar os mesmos bytes")
        if HashPasta(Pasta) != Antes:
            Problemas.append("o dry-run gravou arquivo")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoEditarAplicar(Forja, PastaSaidas):
    Problemas = []
    Raiz, Pasta, Ops = CopiaComOps(RENOMEAR)
    try:
        Rodada = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops), "--aplicar"])
        Problemas += ConferirExecucao(Rodada, 0)
        Resposta = JsonDe(Rodada, Problemas)
        if Resposta is not None and not (Resposta["aplicado"] and Resposta["valido"] and not Resposta["desfeito"]):
            Problemas.append("--aplicar sem aplicado/valido")
        if "juntar_vetores" not in (Pasta / "geo" / "vetor.fj").read_text(encoding="utf-8") or "somar_vetor(" in (Pasta / "main.fj").read_text(encoding="utf-8"):
            Problemas.append("o proc nao foi renomeado na declaracao e na chamada de outro modulo")
        Problemas += [f"check do resultado: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "main.fj")]), 0)]
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoEditarDesfazEUsoErrado(Forja, PastaSaidas):
    Problemas = []
    Raiz, Pasta, Ops = CopiaComOps(COLISAO)
    try:
        Antes = HashPasta(Pasta)
        Rodada = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops), "--aplicar"])
        Problemas += ConferirExecucao(Rodada, 1)
        Resposta = JsonDe(Rodada, Problemas)
        if Resposta is not None and not (Resposta["desfeito"] and not Resposta["aplicado"] and any(D.get("codigo") == "E0202" for D in Resposta["diagnosticos"])):
            Problemas.append("colisao de nome deveria vir desfeita com o E0202 em diagnosticos")
        if HashPasta(Pasta) != Antes:
            Problemas.append("a operacao desfeita alterou arquivo")
        Ruim = Raiz / "ruim.json"
        Ruim.write_text("{nao e json", encoding="utf-8")
        Desconhecida = Raiz / "desconhecida.json"
        Desconhecida.write_text(json.dumps([{"op": "inventar", "id": "x"}]), encoding="utf-8")
        Erros = [["editar", str(Pasta)], ["editar", str(Pasta), "--ops", str(Raiz / "nada.json")], ["editar", str(Pasta), "--ops", str(Ruim)],
                 ["editar", str(Pasta), "--ops", str(Desconhecida)], ["editar", str(Pasta), "--ops", str(Ops), "--zz"]]
        for Argumentos in Erros:
            Problemas += [f"{' '.join(Argumentos[:2])}: {P}" for P in ConferirExecucao(Rodar(Forja, Argumentos), 2)]
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoEditarInserirDecl(Forja, PastaSaidas):
    Problemas = []
    Raiz, Pasta, Ops = CopiaComOps(INSERIR, FIXTURE2)
    try:
        Antes = HashPasta(Pasta)
        Seca = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops)])
        Problemas += ConferirExecucao(Seca, 0)
        Resposta = JsonDe(Seca, Problemas)
        if Resposta is not None and (Resposta["aplicado"] or not Resposta["valido"] or "subtrair" not in json.dumps(Resposta["diff"])):
            Problemas.append("dry-run do inserir_decl sem a declaracao nova no diff, ou aplicado")
        if HashPasta(Pasta) != Antes:
            Problemas.append("o dry-run gravou arquivo")
        Problemas += ConferirExecucao(Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops), "--aplicar"]), 0)
        if "pub proc subtrair(" not in (Pasta / "calc" / "aritmetica.fj").read_text(encoding="utf-8"):
            Problemas.append("a declaracao nova nao foi gravada em calc/aritmetica.fj")
        Problemas += [f"check do resultado: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "main.fj")]), 0)]
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoEditarMoverDecl(Forja, PastaSaidas):
    Problemas = []
    Raiz, Pasta, Ops = CopiaComOps(MOVER, FIXTURE2)
    try:
        Rodada = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops), "--aplicar"])
        Problemas += ConferirExecucao(Rodada, 0)
        Aritmetica = (Pasta / "calc" / "aritmetica.fj").read_text(encoding="utf-8")
        Texto = (Pasta / "util" / "texto.fj").read_text(encoding="utf-8")
        if "proc dobrar(" in Aritmetica or "proc dobrar(" not in Texto or 'import "calc/aritmetica"' not in Texto:
            Problemas.append("a declaracao nao mudou de modulo ou o destino nao ganhou o import da origem")
        if "ar.dobrar(" in (Pasta / "main.fj").read_text(encoding="utf-8") or "tx.dobrar(60)" not in (Pasta / "main.fj").read_text(encoding="utf-8"):
            Problemas.append("a chamada no main nao passou a usar o apelido do destino")
        Problemas += [f"check do resultado: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "main.fj")]), 0)]
        Original = Rodar(Forja, ["run", str(FIXTURE2 / "main.fj")])
        if not Original.stdout or Rodar(Forja, ["run", str(Pasta / "main.fj")]).stdout != Original.stdout:
            Problemas.append("o run do projeto movido deu saida diferente da fixture original")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoEditarRemoverRecusado(Forja, PastaSaidas):
    Problemas = []
    Raiz, Pasta, Ops = CopiaComOps(REMOVER_EM_USO, FIXTURE2)
    try:
        Antes = HashPasta(Pasta)
        for Aplicar in ([], ["--aplicar"]):
            Rodada = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops)] + Aplicar)
            Problemas += ConferirExecucao(Rodada, 1)
            Resposta = JsonDe(Rodada, Problemas)
            if Resposta is not None and (Resposta["aplicado"] or Resposta["desfeito"] or "referencia" not in str(Resposta.get("motivo"))):
                Problemas.append("remover_decl de declaracao em uso deveria ser recusado com o motivo das referencias (desfeito=false)")
            if HashPasta(Pasta) != Antes:
                Problemas.append("a recusa alterou arquivo")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas
