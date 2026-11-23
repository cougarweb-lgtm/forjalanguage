# verificar_limites_b10.py -- prova do P-IA1-b10: forja run/test --limite-tempo/--limite-mem/--limite-saida (R0013..R0015), R-code enriquecido pelo driver e --args-file
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  tempo   `while true` com --limite-tempo 5 para em ~5 s (parede medida, descontada a compilacao), exit 102 e R0013 com forma_certa e spec; x64 e c
  saida   print em laco com --limite-saida 64KiB: stdout tem EXATAMENTE 65536 bytes (prefixo do que o programa escreve), exit 104, R0015 diz os bytes descartados; x64 e c
  mem     lista que so cresce com --limite-mem 64: exit 103 e R0014 com o pico medido; x64 e c
  folga   programa curto com os 3 limites folgados (run e test): exit 0 e a saida inteira, igual a sem limite
  rcode   R0002..R0009 do run/test (stderr do programa): a linha ganha explicacao, forma_certa e spec e mantem codigo, tipo, arquivo, linha e causa; x64 e c
  catalogo  100% dos R-codes do catalogo (`forja explicar --lista --json`) com explicacao, forma_certa e spec nao vazios (M-D8 parte 1); R0013..R0015 sem nome de exemplo inventado
  args    --args-file: o mesmo que `--` (espaco, vazio, CRLF, BOM, sem fim de linha, \\n e \\\\), vazio = 0 argumentos, antes do `--`; arquivo ausente = exit 2
  uso     valores invalidos, limites fora de run/test e --args-file no test = exit 2
Uso:
    python -I verificar_limites_b10.py --forja <forja.exe>
    python -I verificar_limites_b10.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python -I verificar_limites_b10.py --selftest
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-b10: primeira versao.
Verificacao: python -I verificar_limites_b10.py --selftest
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LANG = Path(r"C:\Users\USUARIO\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
CLI = LANG / "tests" / "cli"
B10 = CLI / "b10"
BACKENDS = ["x64", "c"]
TEMPO_LIMITE_S = 5
TOLERANCIA_ANTES_S = 0.3        # o forja nunca encerra antes do limite (menos a folga do relogio)
TOLERANCIA_DEPOIS_S = 3.0       # e encerra logo depois (alem do tempo de compilar, medido a parte)
NOMES_INVENTADOS = ["Grifo", "Bala", "Vertice", "Circulo", "Particula", "Janela", "Bando", "ErroCarga", "ErroIo", "Cor"]
RCODES_REAIS = [  # (fixture, comando, codigo) com a linha JSON no stderr do programa
    (CLI / "panico.fj", "run", "R0003"), (CLI / "lista_indice_fora.fj", "run", "R0006"), (CLI / "vazamento.fj", "run", "R0009"),
    (CLI / "main_erro.fj", "run", "R0004"), (CLI / "delete_duplo.fj", "run", "R0008"), (CLI / "json_estouro.fj", "test", "R0002"),
]
CAMPOS_ORIGINAIS = ["codigo", "tipo", "arquivo", "linha", "causa"]
CAMPOS_ENRIQUECIDOS = ["explicacao", "forma_certa", "spec"]


def Rodar(Forja, Argumentos, Pasta=None):
    """(CompletedProcess com bytes, segundos de parede)."""
    Inicio = time.perf_counter()
    Rodada = subprocess.run([str(Forja), *Argumentos], capture_output=True, timeout=180, cwd=Pasta, stdin=subprocess.DEVNULL)
    return Rodada, time.perf_counter() - Inicio


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


# ---------- julgamentos puros (o selftest planta falhas nelas) ----------

def JulgarEnriquecido(Linha, Codigo):
    """A linha de R-code tem os 5 campos do runtime e os 3 do driver, todos de texto nao vazio (linha pode ser 0)."""
    Falhas = []
    if Linha.get("codigo") != Codigo:
        Falhas.append(f"codigo {Linha.get('codigo')!r}, esperado {Codigo}")
    for Campo in CAMPOS_ORIGINAIS:
        if Campo not in Linha:
            Falhas.append(f"{Codigo}: falta o campo original {Campo}")
    for Campo in CAMPOS_ENRIQUECIDOS:
        if not isinstance(Linha.get(Campo), str) or not Linha[Campo].strip():
            Falhas.append(f"{Codigo}: falta {Campo}")
    return Falhas


def JulgarLimite(Rodada, Codigo, Exit):
    """exit certo e, no stderr, a linha JSON do codigo com forma_certa e spec."""
    Falhas = []
    if Rodada.returncode != Exit:
        Falhas.append(f"exit {Rodada.returncode}, esperado {Exit}")
    Linhas = [L for L in LinhasJson(Rodada.stderr) if L.get("codigo") == Codigo]
    if len(Linhas) != 1:
        return Falhas + [f"{len(Linhas)} linhas {Codigo} no stderr, esperado 1"]
    for Campo in ("causa", "explicacao", "forma_certa", "spec"):
        if not isinstance(Linhas[0].get(Campo), str) or not Linhas[0][Campo].strip():
            Falhas.append(f"{Codigo}: falta {Campo}")
    return Falhas


def JulgarTempo(Segundos, Compilacao):
    """Parede do run com limite 5: nem antes de 5 s, nem muito depois (compilacao descontada)."""
    Gasto = Segundos - Compilacao
    if Gasto < TEMPO_LIMITE_S - TOLERANCIA_ANTES_S:
        return [f"parou em {Gasto:.2f} s, antes do limite de {TEMPO_LIMITE_S} s"]
    if Gasto > TEMPO_LIMITE_S + TOLERANCIA_DEPOIS_S:
        return [f"parou em {Gasto:.2f} s, muito depois do limite de {TEMPO_LIMITE_S} s"]
    return []


def JulgarCorte(Stdout, Limite, Linha):
    """Stdout cortado em Limite bytes exatos, e e o prefixo do que o programa escreve (linhas inteiras repetidas)."""
    Falhas = []
    if len(Stdout) != Limite:
        Falhas.append(f"stdout com {len(Stdout)} bytes, esperado {Limite}")
    Stdout = Stdout.replace(b"\r\n", b"\n")     # o print em modo texto escreve CRLF; o corte e em bytes crus
    if not Linha.startswith(Stdout[:len(Linha)]) or Stdout.replace(Linha, b"").count(b"\n") > 1:
        Falhas.append("stdout nao e o prefixo da saida do programa")
    return Falhas


def JulgarDescartados(Causa):
    Achado = re.search(r"(\d+) bytes foram descartados", Causa)
    return [] if Achado and int(Achado.group(1)) > 0 else ["R0015 sem a contagem de bytes descartados"]


def JulgarLinhasIguais(Obtido, Esperado, Nome):
    return [] if Obtido == Esperado else [f"{Nome}: {Obtido!r}, esperado {Esperado!r}"]


def JulgarCatalogo(Itens):
    """Cada item {codigo, explicacao, forma_certa, spec}: tudo nao vazio; R0013..R0015 existem e sem nome de exemplo inventado."""
    Falhas = []
    Codigos = {Item.get("codigo") for Item in Itens}
    for Codigo in ("R0013", "R0014", "R0015"):
        if Codigo not in Codigos:
            Falhas.append(f"{Codigo} fora do catalogo")
    for Item in Itens:
        for Campo in CAMPOS_ENRIQUECIDOS:
            if not isinstance(Item.get(Campo), str) or not Item[Campo].strip():
                Falhas.append(f"{Item.get('codigo')}: falta {Campo}")
        if Item.get("codigo") in ("R0013", "R0014", "R0015"):
            Texto = Item.get("explicacao", "") + " " + Item.get("forma_certa", "")
            Falhas += [f"{Item['codigo']}: cita o nome de exemplo {Nome}" for Nome in NOMES_INVENTADOS if re.search(rf"\b{Nome}\b", Texto)]
    return Falhas


# ---------- casos ----------

def CasoTempo(Forja):
    Falhas = []
    for Backend in BACKENDS:
        _, Compilacao = Rodar(Forja, ["run", "--backend", Backend, str(B10 / "saida_curta.fj")])
        Rodada, Segundos = Rodar(Forja, ["run", "--backend", Backend, "--limite-tempo", str(TEMPO_LIMITE_S), str(B10 / "tempo_infinito.fj")])
        Falhas += [f"{Backend}: {F}" for F in JulgarLimite(Rodada, "R0013", 102) + JulgarTempo(Segundos, Compilacao)]
    return "tempo", Falhas


def CasoSaida(Forja):
    Falhas = []
    Linha = b"linha repetida do programa que nunca para de escrever\n"
    for Backend in BACKENDS:
        for Valor in ("64KiB", "65536"):
            Rodada, _ = Rodar(Forja, ["run", "--backend", Backend, "--limite-saida", Valor, str(B10 / "saida_infinita.fj")])
            Resto = JulgarLimite(Rodada, "R0015", 104) + JulgarCorte(Rodada.stdout, 65536, Linha)
            Linhas = [L for L in LinhasJson(Rodada.stderr) if L.get("codigo") == "R0015"]
            Resto += JulgarDescartados(Linhas[0].get("causa", "")) if Linhas else []
            Falhas += [f"{Backend} {Valor}: {F}" for F in Resto]
    return "saida", Falhas


def CasoMemoria(Forja):
    Falhas = []
    for Backend in BACKENDS:
        Rodada, Segundos = Rodar(Forja, ["run", "--backend", Backend, "--limite-mem", "64", str(B10 / "memoria_crescente.fj")])
        Resto = JulgarLimite(Rodada, "R0014", 103)
        Linhas = [L for L in LinhasJson(Rodada.stderr) if L.get("codigo") == "R0014"]
        if Linhas and not re.search(r"pico medido: \d+ MiB", Linhas[0].get("causa", "")):
            Resto.append("R0014 sem o pico medido")
        if Segundos > 60:
            Resto.append(f"levou {Segundos:.1f} s")
        Falhas += [f"{Backend}: {F}" for F in Resto]
    return "mem", Falhas


def CasoFolga(Forja):
    Falhas = []
    Folgas = ["--limite-tempo", "60", "--limite-mem", "512", "--limite-saida", "1MiB"]
    for Backend in BACKENDS:
        Base, _ = Rodar(Forja, ["run", "--backend", Backend, str(B10 / "saida_curta.fj")])
        Com, _ = Rodar(Forja, ["run", "--backend", Backend, *Folgas, str(B10 / "saida_curta.fj")])
        Falhas += JulgarLinhasIguais((Com.returncode, Com.stdout, Com.stderr), (0, Base.stdout, Base.stderr), f"run {Backend}")
        BaseT, _ = Rodar(Forja, ["test", "--backend", Backend, str(CLI / "tres_testes.fj")])
        ComT, _ = Rodar(Forja, ["test", "--backend", Backend, *Folgas, str(CLI / "tres_testes.fj")])
        Falhas += JulgarLinhasIguais((ComT.returncode, ComT.stdout, ComT.stderr), (BaseT.returncode, BaseT.stdout, BaseT.stderr), f"test {Backend}")
    return "folga", Falhas


def CasoRcode(Forja):
    Falhas = []

    def Um(Item):
        (Fixture, Comando, Codigo), Backend = Item
        Rodada, _ = Rodar(Forja, [Comando, "--backend", Backend, str(Fixture)])
        Achadas = [L for L in LinhasJson(Rodada.stderr) if L.get("codigo") == Codigo]
        if not Achadas:
            return [f"{Comando} {Fixture.name} {Backend}: sem a linha {Codigo} no stderr"]
        return [f"{Fixture.name} {Backend}: {F}" for F in JulgarEnriquecido(Achadas[0], Codigo)]

    with ThreadPoolExecutor(max_workers=8) as Pool:
        for Resultado in Pool.map(Um, [(R, B) for R in RCODES_REAIS for B in BACKENDS]):
            Falhas += Resultado
    return "rcode", Falhas


def CasoCatalogo(Forja):
    Lista, _ = Rodar(Forja, ["explicar", "--lista", "--json"])
    try:
        Codigos = [Item["codigo"] for Item in json.loads(Lista.stdout.decode("utf-8")) if Item["codigo"].startswith("R")]
    except (ValueError, KeyError, TypeError):
        return "catalogo", ["explicar --lista --json nao deu a lista de codigos"]
    Itens = []
    for Codigo in Codigos:
        Rodada, _ = Rodar(Forja, ["explicar", Codigo])
        Itens += LinhasJson(Rodada.stdout)
    return "catalogo", JulgarCatalogo(Itens) + ([] if len(Itens) == len(Codigos) else ["algum R-code sem explicar"])


def Programa(Forja, Argumentos):
    """Linhas do stdout do run_args.fj (backend c) com os argumentos dados antes do arquivo."""
    Rodada, _ = Rodar(Forja, ["run", "--backend", "c", *Argumentos], CLI)
    return Rodada


def CasoArgs(Forja):
    Falhas = []
    with tempfile.TemporaryDirectory(prefix="b10_args_") as Nome:
        Pasta = Path(Nome)
        Casos = {  # nome: (bytes do arquivo, argumentos esperados)
            "simples": (b"a\nb c\n\nfim\n", ["a", "b c", "", "fim"]),
            "crlf_bom_sem_fim": (b"\xef\xbb\xbfum\r\ndois tres\r\nultimo", ["um", "dois tres", "ultimo"]),
            "escapes": (b"x\\\\y\nlinha\\ndois\nC:\\pasta", ["x\\y", "linha\ndois", "C:\\pasta"]),
            "vazio": (b"", []),
            "so_uma_linha_vazia": (b"\n", [""]),
        }
        for Nome, (Conteudo, Esperados) in Casos.items():
            Arquivo = Pasta / f"{Nome}.txt"
            Arquivo.write_bytes(Conteudo)
            Rodada = Programa(Forja, ["--args-file", str(Arquivo), "run_args.fj"])
            Saida = Rodada.stdout.decode("utf-8", errors="replace").replace("\r\n", "\n")
            Esperada = "\n".join([str(len(Esperados))] + [f"[{A}]" for A in Esperados]) + "\n"
            Falhas += JulgarLinhasIguais((Rodada.returncode, Saida), (0, Esperada), f"args-file {Nome}")
        Arquivo = Pasta / "ordem.txt"
        Arquivo.write_bytes(b"do arquivo\n")
        Rodada = Programa(Forja, ["--args-file", str(Arquivo), "run_args.fj", "--", "depois", "x y"])
        Falhas += JulgarLinhasIguais(Rodada.stdout.decode("utf-8").replace("\r\n", "\n"), "3\n[do arquivo]\n[depois]\n[x y]\n", "args-file antes do --")
        Ausente = Programa(Forja, ["--args-file", str(Pasta / "nao_existe.txt"), "run_args.fj"])
        Falhas += JulgarLinhasIguais(Ausente.returncode, 2, "args-file ausente")
    return "args", Falhas


def CasoUso(Forja):
    Falhas = []
    Ruins = [["run", "--limite-tempo", "abc"], ["run", "--limite-tempo", "0"], ["run", "--limite-tempo", "-1"], ["run", "--limite-tempo", "5."],
             ["run", "--limite-mem", "0"], ["run", "--limite-mem", "1.5"], ["run", "--limite-saida", "0"], ["run", "--limite-saida", "10GB"],
             ["test", "--limite-saida", "x"], ["check", "--limite-tempo", "5"], ["build", "--limite-mem", "5"], ["test", "--args-file", "x.txt"],
             ["run", "--limite-tempo"]]
    for Pedido in Ruins:
        Rodada, _ = Rodar(Forja, [*Pedido, str(B10 / "saida_curta.fj")] if Pedido[-1] != "--limite-tempo" else Pedido)
        if Rodada.returncode != 2:
            Falhas.append(f"{' '.join(Pedido)}: exit {Rodada.returncode}, esperado 2")
    return "uso", Falhas


CASOS = [CasoCatalogo, CasoUso, CasoArgs, CasoFolga, CasoRcode, CasoSaida, CasoMemoria, CasoTempo]


def RodarCasos(Forja):
    Vermelhos = 0
    for Caso in CASOS:
        try:
            Id, Falhas = Caso(Forja)
        except Exception as Erro:  # um caso que quebra e vermelho, nao derruba os outros
            Id, Falhas = Caso.__name__, [f"excecao: {type(Erro).__name__}: {Erro}"]
        Vermelhos += bool(Falhas)
        print(json.dumps({"id": Id, "valor": "ok" if not Falhas else f"{len(Falhas)} falhas", "meta": "ok", "verde": not Falhas, "falhas": Falhas}, ensure_ascii=False), flush=True)
    return Vermelhos


class Simulada:
    """Rodada falsa para o selftest."""

    def __init__(self, Codigo, Saida=0, Stdout=b"", Extra=None):
        Linha = {"codigo": Codigo, "causa": "c", "explicacao": "e", "forma_certa": "f", "spec": "s"}
        Linha.update(Extra or {})
        self.returncode, self.stdout, self.stderr = Saida, Stdout, (json.dumps(Linha) + "\n").encode()


def Selftest():
    """Cada julgamento aceita o certo e recusa o errado plantado."""
    Linha = b"abc\n"
    Bom = {"codigo": "R0003", "tipo": "runtime", "arquivo": "a", "linha": 1, "causa": "c", "explicacao": "e", "forma_certa": "f", "spec": "s"}
    Item = {"codigo": "R0013", "explicacao": "e", "forma_certa": "f", "spec": "s"}
    Todos = [dict(Item, codigo=C) for C in ("R0013", "R0014", "R0015")]
    Casos = {
        "enriquecido ok": not JulgarEnriquecido(Bom, "R0003"),
        "enriquecido sem forma_certa": bool(JulgarEnriquecido({Chave: Valor for Chave, Valor in Bom.items() if Chave != "forma_certa"}, "R0003")),
        "enriquecido sem spec": bool(JulgarEnriquecido(dict(Bom, spec=" "), "R0003")),
        "enriquecido sem causa": bool(JulgarEnriquecido({Chave: Valor for Chave, Valor in Bom.items() if Chave != "causa"}, "R0003")),
        "limite ok": not JulgarLimite(Simulada("R0013", 102), "R0013", 102),
        "limite exit errado": bool(JulgarLimite(Simulada("R0013", 1), "R0013", 102)),
        "limite codigo errado": bool(JulgarLimite(Simulada("R0014", 102), "R0013", 102)),
        "limite sem spec": bool(JulgarLimite(Simulada("R0013", 102, Extra={"spec": ""}), "R0013", 102)),
        "tempo ok": not JulgarTempo(5.4, 0.3),
        "tempo cedo": bool(JulgarTempo(2.0, 0.3)),
        "tempo tarde": bool(JulgarTempo(30.0, 0.3)),
        "corte ok": not JulgarCorte(Linha * 3, 12, Linha),
        "corte tamanho errado": bool(JulgarCorte(Linha * 3, 13, Linha)),
        "corte lixo": bool(JulgarCorte(b"xyz\n" * 3, 12, Linha)),
        "descartados ok": not JulgarDescartados("a saida 123 bytes foram descartados"),
        "descartados zero": bool(JulgarDescartados("0 bytes foram descartados")),
        "descartados ausente": bool(JulgarDescartados("sem numero")),
        "catalogo ok": not JulgarCatalogo(Todos),
        "catalogo sem R0015": bool(JulgarCatalogo(Todos[:2])),
        "catalogo sem spec": bool(JulgarCatalogo(Todos[:2] + [dict(Item, codigo="R0015", spec="")])),
        "catalogo nome inventado": bool(JulgarCatalogo(Todos[:2] + [dict(Item, codigo="R0015", explicacao="o Grifo")])),
        "iguais ok": not JulgarLinhasIguais(1, 1, "x"),
        "iguais difere": bool(JulgarLinhasIguais(1, 2, "x")),
        "json ignora texto": LinhasJson(b"texto\n{\"a\": 1}\n[1]\n") == [{"a": 1}],
    }
    Falhas = [Nome for Nome, Ok in Casos.items() if not Ok]
    print(json.dumps({"id": "selftest", "valor": f"{len(Casos) - len(Falhas)}/{len(Casos)}", "meta": f"{len(Casos)}/{len(Casos)}", "verde": not Falhas, "falhas": Falhas}))
    return 1 if Falhas else 0


def Principal():
    Args = argparse.ArgumentParser(description="Verifica o P-IA1-b10 (limites do run/test, R-code enriquecido, --args-file)")
    Args.add_argument("--forja", type=Path)
    Args.add_argument("--esperar-vermelho", action="store_true")
    Args.add_argument("--selftest", action="store_true")
    Opcoes = Args.parse_args()
    if Opcoes.selftest:
        return Selftest()
    if Opcoes.forja is None:
        raise SystemExit("falta --forja <forja.exe>")
    Vermelhos = RodarCasos(Opcoes.forja)
    if Opcoes.esperar_vermelho:
        return 0 if Vermelhos else 1
    return 1 if Vermelhos else 0


if __name__ == "__main__":
    sys.exit(Principal())
