"""Casos de cli dos limites do run/test, do R-code enriquecido pelo driver e do --args-file (IA1-b10).

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la. A prova ampla (os 2 backends, 5 s de tempo medido, 64 KiB exatos, 100% do catalogo) mora em
benchmark\\ia_first\\verificar_limites_b10.py; aqui ficam os casos curtos do motor.

Regras:
  - --limite-tempo estoura em exit 102 com R0013; --limite-mem em 103 com R0014; --limite-saida em 104 com R0015 e a saida cortada no limite;
  - R-code do runtime (stderr do programa) ganha explicacao, forma_certa e spec; os campos de antes ficam;
  - explicar R0013..R0015 leva tipo, forma_certa e spec; --args-file entrega os argumentos do arquivo antes dos do '--'; uso errado = exit 2.

Versao: 1.2.0
Changelog: 1.2.0 - 0.21.0 F3: o --args-file tambem roda no x64 (run_args.fj passou a ler o argv por std:os.args).
           1.1.0 - 0.21.0 F3: limite de saida e R-code do runtime rodam so no x64 (a perna C saiu).
           1.0.1 - E4: docstring cita benchmark\\ia_first (pasta movida).
           1.0.0 - IA1-b10: primeira versao (5 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json

from verificar_cli import ConferirExecucao, LinhaJson, Rodar

CAMPOS_ENRIQUECIDOS = ("explicacao", "forma_certa", "spec")


def LinhasJsonDe(Texto):
    Achadas = []
    for Linha in Texto.splitlines():
        try:
            Dado = json.loads(Linha)
        except json.JSONDecodeError:
            continue
        if isinstance(Dado, dict):
            Achadas.append(Dado)
    return Achadas


def ConferirLimite(Rodada, Codigo, Saida):
    """Exit e a linha JSON do codigo no stderr, com forma_certa e spec."""
    Problemas = ConferirExecucao(Rodada, Saida)
    Linhas = [L for L in LinhasJsonDe(Rodada.stderr) if L.get("codigo") == Codigo]
    if len(Linhas) != 1:
        return Problemas + [f"{len(Linhas)} linhas {Codigo} no stderr: {Rodada.stderr.strip()[:160]!r}"]
    Problemas += [f"{Codigo} sem {Campo}" for Campo in ("causa",) + CAMPOS_ENRIQUECIDOS if not Linhas[0].get(Campo)]
    return Problemas


def CasoExplicarLimites(Forja, PastaSaidas):
    Problemas = []
    for Codigo, Tipo, Exit in (("R0013", "runtime", "102"), ("R0014", "memoria", "103"), ("R0015", "runtime", "104")):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Problemas += ConferirExecucao(Rodada, 0)
        Dados = LinhaJson(Rodada.stdout)
        if Dados is None or Dados.get("codigo") != Codigo or Dados.get("tipo") != Tipo or Exit not in Dados.get("explicacao", ""):
            Problemas.append(f"explicar {Codigo}: sem codigo, tipo {Tipo} e exit {Exit}: {Rodada.stdout.strip()[:140]!r}")
        elif not Dados.get("forma_certa") or not Dados.get("spec"):
            Problemas.append(f"explicar {Codigo}: sem forma_certa ou spec")
    return Problemas


def CasoLimiteTempoEMemoria(Forja, PastaSaidas):
    Problemas = []
    Tempo = Rodar(Forja, ["run", "--backend", "x64", "--limite-tempo", "1", "b10/tempo_infinito.fj"])
    Problemas += ConferirLimite(Tempo, "R0013", 102)
    Memoria = Rodar(Forja, ["run", "--backend", "x64", "--limite-mem", "32", "b10/memoria_crescente.fj"])
    Problemas += ConferirLimite(Memoria, "R0014", 103)
    return Problemas


def CasoLimiteSaida(Forja, PastaSaidas):
    Problemas = []
    for Backend in ("x64",):
        Rodada = Rodar(Forja, ["run", "--backend", Backend, "--limite-saida", "4KiB", "b10/saida_infinita.fj"])
        Problemas += [f"{Backend}: {P}" for P in ConferirLimite(Rodada, "R0015", 104)]
        Tamanho = len(Rodada.stdout.encode("utf-8"))    # o Rodar le em modo texto (CRLF vira LF): os 4096 bytes exatos sao provados em verificar_limites_b10.py
        if not 3900 <= Tamanho <= 4096:
            Problemas.append(f"{Backend}: stdout com {Tamanho} bytes, esperado perto de 4096 (o corte)")
    Folga = Rodar(Forja, ["run", "--limite-saida", "1MiB", "--limite-tempo", "60", "--limite-mem", "512", "b10/saida_curta.fj"])
    Problemas += ConferirExecucao(Folga, 0)
    if Folga.stdout.split() != ["um", "dois"]:
        Problemas.append(f"programa curto sob limites folgados: stdout {Folga.stdout!r}")
    return Problemas


def CasoRcodeEnriquecido(Forja, PastaSaidas):
    Problemas = []
    for Backend in ("x64",):
        Rodada = Rodar(Forja, ["run", "--backend", Backend, "panico.fj"])
        Problemas += ConferirExecucao(Rodada, 101)
        Linhas = [L for L in LinhasJsonDe(Rodada.stderr) if L.get("codigo") == "R0003"]
        if len(Linhas) != 1:
            Problemas.append(f"{Backend}: {len(Linhas)} linhas R0003 no stderr")
            continue
        Problemas += [f"{Backend}: R0003 sem {Campo}" for Campo in CAMPOS_ENRIQUECIDOS if not Linhas[0].get(Campo)]
        Problemas += [f"{Backend}: R0003 sem o campo original {Campo}" for Campo in ("tipo", "arquivo", "linha", "causa") if Campo not in Linhas[0]]
    return Problemas


def CasoArgsFileEUso(Forja, PastaSaidas):
    Problemas = []
    Arquivo = PastaSaidas / "b10_args.txt"
    Arquivo.write_bytes(b"a\nb c\n\nfim")
    Rodada = Rodar(Forja, ["run", "--args-file", str(Arquivo), "run_args.fj", "--", "depois"])
    Problemas += ConferirExecucao(Rodada, 0)
    if Rodada.stdout.splitlines() != ["5", "[a]", "[b c]", "[]", "[fim]", "[depois]"]:
        Problemas.append(f"--args-file: stdout {Rodada.stdout.splitlines()!r}")
    for Pedido in (["run", "--args-file", str(PastaSaidas / "nao_existe.txt"), "run_args.fj"], ["test", "--args-file", str(Arquivo), "dois_testes.fj"],
                   ["run", "--limite-tempo", "0", "run_args.fj"], ["run", "--limite-mem", "x", "run_args.fj"], ["run", "--limite-saida", "1GB", "run_args.fj"],
                   ["check", "--limite-tempo", "5", "run_args.fj"]):
        Problemas += ConferirExecucao(Rodar(Forja, Pedido), 2)
    return Problemas
