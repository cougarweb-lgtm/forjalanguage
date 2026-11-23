# verificar_rastreio_b12.py -- prova do P-IA1-b12: forja run/test --rastreio (x64, dev): o R-code ganha coluna, valor do indice e pilha .fj
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  r0006    R0006 num proc chamado por main: pilha de 2 quadros (pegar 2:13, main 7:11), coluna 13 no topo, valor {indice 7, tamanho 3}, exit 101, campos do driver mantidos
  recursao R0006 dentro de recursao: pilha com 3+ quadros (desce, desce, desce, desce, main), o mais interno primeiro
  assert   R0003 (assert) em proc chamado por main: pilha de 2 quadros com coluna
  main_erro R0004 (main -> ! E): pilha com o proprio main (linha > 0); linha do topo continua 0
  teste    forja test --rastreio com R0006 num proc chamado por um test: pilha de 2 quadros, o 2o na linha do expect; o resumo do test continua igual
  sem_erro programa sem erro: mesma saida, mesmo exit e mesmo stderr com e sem --rastreio (run e test)
  sem_flag (P69) em dev sem a flag = com a flag, com pilha em toda linha R; em --release a linha R nao tem coluna, valor nem pilha
  c_igual_x64 --backend c --rastreio: a linha R e a do x64 (desde o b13; antes era c_ignora)
  uso      --rastreio fora de build/run/test e com --suporte = exit 2
Uso:
    python -I verificar_rastreio_b12.py --forja <forja.exe> [--referencia <forja_antigo.exe>]
    python -I verificar_rastreio_b12.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python -I verificar_rastreio_b12.py --selftest
Versao: 0.4.0
Changelog: 0.4.0 - M6 P69: a pilha e padrao em dev: sem_flag espera dev sem flag == dev com flag (com pilha em toda linha R) e --release sem os campos (SemCamposNovos fica sem uso).
           0.3.0- M6 P65b: sem_flag nao depende mais do binario antigo (%TEMP%\\forja_i2_final apagado; politica de nao comparar com versao anterior): oraculo proprio = a flag so acrescenta campos (CasoSemFlagProprio); a referencia, se existir, continua valendo.
           0.2.0 - P-IA1-b13: c_ignora vira c_igual_x64 (o backend c passa a ter a pilha).
           0.1.0 - P-IA1-b12: primeira versao.
Verificacao: python -I verificar_rastreio_b12.py --selftest
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

LANG = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
CLI = LANG / "tests" / "cli"
B12 = "b12"
REFERENCIA_PADRAO = Path(os.environ.get("TEMP", ".")) / "forja_i2_final" / "forja.exe"
CAMPOS_NOVOS = ("coluna", "valor", "pilha")
CAMPOS_DO_DRIVER = ("explicacao", "forma_certa", "spec")


def Rodar(Forja, Argumentos):
    """CompletedProcess com bytes; sempre em tests\\cli (o caminho do fixture sai relativo e igual para os dois forja)."""
    return subprocess.run([str(Forja), *Argumentos], capture_output=True, timeout=180, cwd=CLI, stdin=subprocess.DEVNULL)


def LinhasJson(Bytes):
    """Cada linha de Bytes que e um objeto JSON, na ordem."""
    Achadas = []
    for Linha in Bytes.decode("utf-8", errors="replace").splitlines():
        try:
            Dado = json.loads(Linha)
        except json.JSONDecodeError:
            continue
        if isinstance(Dado, dict):
            Achadas.append(Dado)
    return Achadas


def LinhaR(Rodada, Codigo):
    """A unica linha de R-code do codigo no stderr, ou None."""
    Achadas = [L for L in LinhasJson(Rodada.stderr) if L.get("codigo") == Codigo]
    return Achadas[0] if len(Achadas) == 1 else None


# ---------- julgamentos puros (o selftest planta falhas neles) ----------

def JulgarQuadro(Quadro, Esperado, Nome):
    """Esperado = (proc, linha, coluna); coluna None = so exige coluna > 0."""
    if not isinstance(Quadro, dict):
        return [f"{Nome}: quadro ausente"]
    Falhas = []
    Proc, Linha, Coluna = Esperado
    if Quadro.get("proc") != Proc:
        Falhas.append(f"{Nome}: proc {Quadro.get('proc')!r}, esperado {Proc!r}")
    if Quadro.get("linha") != Linha:
        Falhas.append(f"{Nome}: linha {Quadro.get('linha')!r}, esperado {Linha}")
    Obtida = Quadro.get("coluna")
    if (Coluna is None and not (isinstance(Obtida, int) and Obtida > 0)) or (Coluna is not None and Obtida != Coluna):
        Falhas.append(f"{Nome}: coluna {Obtida!r}, esperado {Coluna if Coluna is not None else '> 0'}")
    if not isinstance(Quadro.get("arquivo"), str) or not Quadro["arquivo"].endswith(".fj"):
        Falhas.append(f"{Nome}: arquivo {Quadro.get('arquivo')!r}")
    return Falhas


def JulgarPilha(Linha, Esperados, Minimo=None):
    """Pilha com exatamente len(Esperados) quadros (ou, com Minimo, pelo menos Minimo e o inicio/fim iguais ao esperado)."""
    Pilha = Linha.get("pilha")
    if not isinstance(Pilha, list):
        return ["sem o campo pilha"]
    if Minimo is None and len(Pilha) != len(Esperados):
        return [f"pilha com {len(Pilha)} quadros, esperado {len(Esperados)}"]
    if Minimo is not None and len(Pilha) < Minimo:
        return [f"pilha com {len(Pilha)} quadros, esperado pelo menos {Minimo}"]
    Falhas = []
    for K, Esperado in enumerate(Esperados):
        Falhas += JulgarQuadro(Pilha[K], Esperado, f"quadro {K}")
    return Falhas


def JulgarDriver(Linha):
    """O enriquecimento do driver (b10) e os campos do runtime continuam: a pilha nao tira nada."""
    Falhas = []
    for Campo in ("codigo", "tipo", "arquivo", "linha", "causa"):
        if Campo not in Linha:
            Falhas.append(f"falta o campo original {Campo}")
    for Campo in CAMPOS_DO_DRIVER:
        if not isinstance(Linha.get(Campo), str) or not Linha[Campo].strip():
            Falhas.append(f"falta {Campo}")
    return Falhas


def JulgarTopo(Linha, ColunaEsperada):
    """coluna no topo da linha R = a do quadro mais interno, na mesma linha."""
    Falhas = []
    if Linha.get("coluna") != ColunaEsperada:
        Falhas.append(f"coluna {Linha.get('coluna')!r}, esperado {ColunaEsperada}")
    Pilha = Linha.get("pilha") or [{}]
    if Pilha[0].get("linha") != Linha.get("linha") or Pilha[0].get("arquivo") != Linha.get("arquivo"):
        Falhas.append("quadro 0 nao cai na linha e no arquivo do topo")
    return Falhas


def JulgarValor(Linha, Esperado):
    return [] if Linha.get("valor") == Esperado else [f"valor {Linha.get('valor')!r}, esperado {Esperado!r}"]


def JulgarSemCamposNovos(Linha):
    return [f"campo {Campo} sem --rastreio" for Campo in CAMPOS_NOVOS if Campo in Linha]


def JulgarIguais(Obtido, Esperado, Nome):
    return [] if Obtido == Esperado else [f"{Nome}: difere ({Obtido!r:.200} x {Esperado!r:.200})"]


# ---------- casos ----------

def CasoR0006(Forja):
    Rodada = Rodar(Forja, ["run", f"{B12}/indice_2_quadros.fj", "--rastreio"])
    Linha = LinhaR(Rodada, "R0006")
    if Linha is None:
        return "r0006", [f"sem a linha R0006 (exit {Rodada.returncode})"]
    Falhas = [] if Rodada.returncode == 101 else [f"exit {Rodada.returncode}, esperado 101"]
    Falhas += JulgarPilha(Linha, [("pegar", 2, 13), ("main", 7, 11)]) + JulgarTopo(Linha, 13)
    Falhas += JulgarValor(Linha, {"indice": 7, "tamanho": 3}) + JulgarDriver(Linha)
    return "r0006", Falhas


def CasoRecursao(Forja):
    Rodada = Rodar(Forja, ["run", f"{B12}/recursao_indice.fj", "--rastreio"])
    Linha = LinhaR(Rodada, "R0006")
    if Linha is None:
        return "recursao", [f"sem a linha R0006 (exit {Rodada.returncode})"]
    Esperados = [("desce", 3, 17), ("desce", 5, 12), ("desce", 5, 12), ("desce", 5, 12), ("main", 10, 11)]
    return "recursao", JulgarPilha(Linha, Esperados, Minimo=3) + JulgarValor(Linha, {"indice": 9, "tamanho": 3}) + JulgarDriver(Linha)


def CasoAssert(Forja):
    Rodada = Rodar(Forja, ["run", f"{B12}/assert_aninhado.fj", "--rastreio"])
    Linha = LinhaR(Rodada, "R0003")
    if Linha is None:
        return "assert", [f"sem a linha R0003 (exit {Rodada.returncode})"]
    return "assert", JulgarPilha(Linha, [("checa", 2, 5), ("main", 6, 5)]) + JulgarTopo(Linha, 5) + JulgarDriver(Linha)


def CasoMainErro(Forja):
    Rodada = Rodar(Forja, ["run", "main_erro.fj", "--rastreio"])
    Linha = LinhaR(Rodada, "R0004")
    if Linha is None:
        return "main_erro", [f"sem a linha R0004 (exit {Rodada.returncode})"]
    Falhas = [] if Rodada.returncode == 1 else [f"exit {Rodada.returncode}, esperado 1"]
    Falhas += JulgarPilha(Linha, [("main", 6, 5)]) + JulgarDriver(Linha)
    return "main_erro", Falhas + ([] if Linha.get("linha") == 0 else ["linha do topo deixou de ser 0"])


def CasoTeste(Forja):
    Sem = Rodar(Forja, ["test", f"{B12}/teste_indice.fj"])
    Com = Rodar(Forja, ["test", f"{B12}/teste_indice.fj", "--rastreio"])
    Linha = LinhaR(Com, "R0006")
    if Linha is None:
        return "teste", [f"sem a linha R0006 no test --rastreio (exit {Com.returncode})"]
    Falhas = JulgarIguais(Com.stdout, Sem.stdout, "stdout do test") + JulgarIguais(Com.returncode, Sem.returncode, "exit do test")
    Pilha = Linha.get("pilha") or []
    Falhas += JulgarPilha(Linha, [("pega", 2, 13)], Minimo=2) + (JulgarQuadro(Pilha[1], (Pilha[1].get("proc"), 7, 12), "quadro 1") if len(Pilha) > 1 else [])
    return "teste", Falhas + JulgarDriver(Linha)


def CasoSemErro(Forja):
    Falhas = []
    for Comando, Fixture in (("run", f"{B12}/sem_erro.fj"), ("test", "tres_testes.fj"), ("run", "b10/saida_curta.fj")):
        Sem = Rodar(Forja, [Comando, Fixture])
        Com = Rodar(Forja, [Comando, Fixture, "--rastreio"])
        Falhas += JulgarIguais((Com.returncode, Com.stdout, Com.stderr), (Sem.returncode, Sem.stdout, Sem.stderr), f"{Comando} {Fixture}")
        if Sem.returncode != 0:
            Falhas.append(f"{Comando} {Fixture}: exit {Sem.returncode} sem a flag, esperado 0")
    return "sem_erro", Falhas


PEDIDOS_SEM_FLAG = [("run", f"{B12}/indice_2_quadros.fj"), ("run", f"{B12}/recursao_indice.fj"), ("run", f"{B12}/assert_aninhado.fj"), ("run", "main_erro.fj"),
                    ("run", "panico.fj"), ("test", f"{B12}/teste_indice.fj"), ("test", "tres_testes.fj"), ("run", f"{B12}/sem_erro.fj")]


def SemCamposNovos(Bytes):
    """stderr com os CAMPOS_NOVOS tirados de cada linha JSON (as outras linhas ficam como estao)."""
    Saida = []
    for Linha in Bytes.decode("utf-8", errors="replace").splitlines():
        try:
            Dado = json.loads(Linha)
        except json.JSONDecodeError:
            Saida.append(Linha)
            continue
        if isinstance(Dado, dict):
            Dado = {Chave: Valor for Chave, Valor in Dado.items() if Chave not in CAMPOS_NOVOS}
        Saida.append(json.dumps(Dado, sort_keys=True, ensure_ascii=False))
    return "\n".join(Saida)


def CasoSemFlagProprio(Forja):
    """Contrato do M6 P69 (D-M6-6): em dev a pilha e padrao, entao SEM a flag a execucao e igual a COM ela (stdout, exit e stderr byte a byte) e toda linha R de falha leva
    a pilha; em --release continua sem coluna, valor nem pilha (a flag e ignorada)."""
    Falhas = []
    for Comando, Fixture in PEDIDOS_SEM_FLAG:
        Sem = Rodar(Forja, [Comando, Fixture])
        Com = Rodar(Forja, [Comando, Fixture, "--rastreio"])
        Falhas += JulgarIguais((Sem.returncode, Sem.stdout, Sem.stderr), (Com.returncode, Com.stdout, Com.stderr), f"{Comando} {Fixture} dev sem flag igual a com flag")
        for Linha in LinhasJson(Sem.stderr):
            if str(Linha.get("codigo", "")).startswith("R") and "pilha" not in Linha:
                Falhas.append(f"{Fixture}: linha R de dev sem a pilha (padrao desde o P69): {str(Linha)[:160]}")
        Release = Rodar(Forja, [Comando, Fixture, "--release"])
        for Linha in LinhasJson(Release.stderr):
            if str(Linha.get("codigo", "")).startswith("R"):
                Falhas += [f"{Fixture} --release: {F}" for F in JulgarSemCamposNovos(Linha)]
    return "sem_flag", Falhas


def CasoSemFlag(Forja, Referencia):
    return CasoSemFlagProprio(Forja)  # P69: o contrato mudou (pilha padrao em dev); um forja de referencia antigo nao serve mais de oraculo


def CasoCIgnora(Forja):
    """Desde o b13 o backend c nao ignora mais a flag: a linha R dele e a do x64 (a prova ampla e verificar_rastreio_b13.py)."""
    Falhas = []
    for Fixture, Codigo in (("main_erro.fj", "R0004"), ("panico.fj", "R0003")):
        X64 = LinhaR(Rodar(Forja, ["run", "--backend", "x64", Fixture, "--rastreio"]), Codigo)
        C = LinhaR(Rodar(Forja, ["run", "--backend", "c", Fixture, "--rastreio"]), Codigo)
        if C is None or C != X64:
            Falhas.append(f"backend c {Fixture}: linha {Codigo} {C!r:.200} difere da do x64 {X64!r:.200}")
    return "c_igual_x64", Falhas


def CasoUso(Forja):
    Falhas = []
    for Pedido in (["check", f"{B12}/sem_erro.fj", "--rastreio"], ["ast", f"{B12}/sem_erro.fj", "--rastreio"], ["build", f"{B12}/sem_erro.fj", "--rastreio", "--modo", "dll", "-o", "x.dll"]):
        Rodada = Rodar(Forja, Pedido)
        if Rodada.returncode != 2:
            Falhas.append(f"{' '.join(Pedido)}: exit {Rodada.returncode}, esperado 2")
    return "uso", Falhas


def RodarCasos(Forja, Referencia):
    Casos = [CasoR0006, CasoRecursao, CasoAssert, CasoMainErro, CasoTeste, CasoSemErro, lambda F: CasoSemFlag(F, Referencia), CasoCIgnora, CasoUso]
    Vermelhos = 0
    for Caso in Casos:
        try:
            Id, Falhas = Caso(Forja)
        except Exception as Erro:  # um caso que quebra e vermelho, nao derruba os outros
            Id, Falhas = getattr(Caso, "__name__", "caso"), [f"excecao: {type(Erro).__name__}: {Erro}"]
        Vermelhos += bool(Falhas)
        print(json.dumps({"id": Id, "valor": "ok" if not Falhas else f"{len(Falhas)} falhas", "meta": "ok", "verde": not Falhas, "falhas": Falhas}, ensure_ascii=False), flush=True)
    return Vermelhos


def Selftest():
    """Cada julgamento aceita o certo e recusa o errado plantado."""
    Quadro = {"proc": "pegar", "arquivo": "a.fj", "linha": 2, "coluna": 13}
    Main = {"proc": "main", "arquivo": "a.fj", "linha": 7, "coluna": 11}
    Bom = {"codigo": "R0006", "tipo": "runtime", "arquivo": "a.fj", "linha": 2, "coluna": 13, "causa": "c", "valor": {"indice": 7, "tamanho": 3},
           "pilha": [Quadro, Main], "explicacao": "e", "forma_certa": "f", "spec": "s"}
    Casos = {
        "pilha ok": not JulgarPilha(Bom, [("pegar", 2, 13), ("main", 7, 11)]),
        "pilha com 1 quadro": bool(JulgarPilha(dict(Bom, pilha=[Quadro]), [("pegar", 2, 13), ("main", 7, 11)])),
        "pilha ausente": bool(JulgarPilha({Chave: Valor for Chave, Valor in Bom.items() if Chave != "pilha"}, [("pegar", 2, 13)])),
        "pilha minimo ok": not JulgarPilha(dict(Bom, pilha=[Quadro, Quadro, Main]), [("pegar", 2, 13)], Minimo=3),
        "pilha minimo curto": bool(JulgarPilha(Bom, [("pegar", 2, 13)], Minimo=3)),
        "quadro proc errado": bool(JulgarQuadro(dict(Quadro, proc="x"), ("pegar", 2, 13), "q")),
        "quadro linha errada": bool(JulgarQuadro(dict(Quadro, linha=3), ("pegar", 2, 13), "q")),
        "quadro coluna errada": bool(JulgarQuadro(dict(Quadro, coluna=1), ("pegar", 2, 13), "q")),
        "quadro coluna zero": bool(JulgarQuadro(dict(Quadro, coluna=0), ("pegar", 2, None), "q")),
        "quadro sem arquivo": bool(JulgarQuadro({Chave: Valor for Chave, Valor in Quadro.items() if Chave != "arquivo"}, ("pegar", 2, 13), "q")),
        "topo ok": not JulgarTopo(Bom, 13),
        "topo sem coluna": bool(JulgarTopo({Chave: Valor for Chave, Valor in Bom.items() if Chave != "coluna"}, 13)),
        "topo quadro em outra linha": bool(JulgarTopo(dict(Bom, linha=9), 13)),
        "valor ok": not JulgarValor(Bom, {"indice": 7, "tamanho": 3}),
        "valor ausente": bool(JulgarValor({Chave: Valor for Chave, Valor in Bom.items() if Chave != "valor"}, {"indice": 7, "tamanho": 3})),
        "valor errado": bool(JulgarValor(dict(Bom, valor={"indice": 7, "tamanho": 4}), {"indice": 7, "tamanho": 3})),
        "driver ok": not JulgarDriver(Bom),
        "driver sem forma_certa": bool(JulgarDriver(dict(Bom, forma_certa=""))),
        "sem campos novos ok": not JulgarSemCamposNovos({"codigo": "R0006", "linha": 2}),
        "sem campos novos com pilha": bool(JulgarSemCamposNovos(Bom)),
        "iguais ok": not JulgarIguais(1, 1, "x"),
        "iguais difere": bool(JulgarIguais(b"a", b"b", "x")),
        "json ignora texto": LinhasJson(b"texto\n{\"a\": 1}\n[1]\n") == [{"a": 1}],
        "sem campos novos: tira so os novos": SemCamposNovos(b'{"codigo": "R0006", "coluna": 3, "pilha": [], "linha": 2}\ntexto') == '{"codigo": "R0006", "linha": 2}\ntexto',
        "sem campos novos: campo original difere": SemCamposNovos(b'{"codigo": "R0006", "linha": 2}') != SemCamposNovos(b'{"codigo": "R0006", "linha": 3}'),
    }
    Falhas = [Nome for Nome, Ok in Casos.items() if not Ok]
    print(json.dumps({"id": "selftest", "valor": f"{len(Casos) - len(Falhas)}/{len(Casos)}", "meta": f"{len(Casos)}/{len(Casos)}", "verde": not Falhas, "falhas": Falhas}))
    return 1 if Falhas else 0


def Principal():
    Parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Parser.add_argument("--forja", help="forja.exe a verificar")
    Parser.add_argument("--referencia", default=str(REFERENCIA_PADRAO), help="forja.exe anterior, para o byte a byte sem a flag")
    Parser.add_argument("--esperar-vermelho", action="store_true", help="exit 0 so se algum caso ficar vermelho (prova de que a regua discrimina)")
    Parser.add_argument("--selftest", action="store_true")
    Argumentos = Parser.parse_args()
    if Argumentos.selftest:
        return Selftest()
    if not Argumentos.forja:
        Parser.error("--forja e obrigatorio")
    Vermelhos = RodarCasos(Path(Argumentos.forja), Path(Argumentos.referencia))
    if Argumentos.esperar_vermelho:
        return 0 if Vermelhos else 1
    return 1 if Vermelhos else 0


if __name__ == "__main__":
    sys.exit(Principal())
