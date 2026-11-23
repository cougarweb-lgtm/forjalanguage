"""Casos de cli do repasse de argumentos: forja run app.fj [--release] -- a "b c" entrega tudo depois do '--' ao programa.

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la.

Regras:
  - o programa (run_args.fj) imprime quantos argumentos recebeu e cada um entre colchetes;
  - espaco, aspa, barra no fim, argumento vazio e o que parece opcao do forja (--release, -o) chegam intactos;
  - o '--' vale em dev e em --release; sem '--' o programa recebe 0 argumentos;
  - fora do run (test, build...) argumentos depois de '--' sao uso errado (exit 2), nunca ignorados em silencio.

Versao: 1.2.0
Changelog: 1.2.0 - 0.21.0 F4 passo 3: CasoArgumentosForaDoRun troca o build --modo c (agora X0003 antes do erro de '--') por build --modo exe; o espectro 'so valem em run' exit 2 e o mesmo.
           1.1.0 - 0.21.0 F3: run_args.fj le o argv por std:os.args (nao mais pelo msvcrt), entao o run roda no x64 padrao, sem --backend c.
           1.0.1 - M4 P25b: CasoRunArgumentos leva --backend c explicito (run_args.fj le o argv pelo msvcrt do exe do gcc; o padrao do dev passou a ser x64).
           1.0.0 - forja run -- args: CasoRunArgumentos e CasoArgumentosForaDoRun.
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

from verificar_cli import ConferirExecucao, Rodar

ARGUMENTOS_DO_PROGRAMA = ["a", "b c", 'd"e', "x\\", "", "y\\\\z q\\", "--release", "-o"]
SAIDA_ESPERADA = ["8"] + [f"[{Argumento}]" for Argumento in ARGUMENTOS_DO_PROGRAMA]


def CasoRunArgumentos(Forja, PastaSaidas):
    Problemas = []
    for Opcoes in ([], ["--release"]):
        Rodada = Rodar(Forja, ["run"] + Opcoes + ["run_args.fj", "--"] + ARGUMENTOS_DO_PROGRAMA)
        Problemas += ConferirExecucao(Rodada, 0)
        if Rodada.stdout.splitlines() != SAIDA_ESPERADA:
            Problemas.append(f"run {Opcoes}: stdout {Rodada.stdout.splitlines()!r}, esperado {SAIDA_ESPERADA!r}")
    Sem = Rodar(Forja, ["run", "run_args.fj"])
    Problemas += ConferirExecucao(Sem, 0)
    if Sem.stdout.splitlines() != ["0"]:
        Problemas.append(f"run sem '--': stdout {Sem.stdout.splitlines()!r}, esperado ['0']")
    return Problemas


def CasoArgumentosForaDoRun(Forja, PastaSaidas):
    Problemas = []
    Pedidos = [
        ["test", "dois_testes.fj", "--", "a"],
        ["build", "--modo", "exe", "sem_teste.fj", "-o", str(PastaSaidas / "fora.exe"), "--", "a"],
        ["check", "sem_teste.fj", "--", "a"],
    ]
    for Pedido in Pedidos:
        Rodada = Rodar(Forja, Pedido)
        Problemas += ConferirExecucao(Rodada, 2)
        if "so valem em run" not in Rodada.stderr:
            Problemas.append(f"{Pedido[0]}: stderr sem 'so valem em run': {Rodada.stderr.strip()[:100]!r}")
    return Problemas
