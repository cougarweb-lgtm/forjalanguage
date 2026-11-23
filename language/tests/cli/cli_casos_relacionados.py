"""Casos de cli do campo "relacionados" do pacote do erro (IA1-b14, contrato 1.1 item 4).

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la. A prova ampla (100% dos 10 codigos nos err_*, 13 pontos de prova com a linha certa e o tamanho)
mora em benchmark\\ia_first\\verificar_relacionados_b14.py; aqui ficam os casos curtos do motor.

Regras:
  - check de campo_parecido.fj (g.vda): relacionados com o parecido 'vida' (linha 3) e a struct Nave (tipo, linha 2), cada um com papel, arquivo, linha, coluna, texto e nota;
  - check e build --backend x64 dao os MESMOS relacionados (o diagnostico e um so, antes do backend);
  - --resumo nao leva relacionados (so codigo, arquivo, linha, coluna, causa e sugestao).

Versao: 1.1.0
Changelog: 1.1.0 - 0.21.0 F3: o build x64 e comparado com o check (a perna --backend c saiu).
           1.0.1 - E4: docstring cita benchmark\\ia_first (pasta movida).
           1.0.0 - IA1-b14: primeira versao (2 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

from verificar_cli import ConferirExecucao, Rodar
from cli_casos_limites import LinhasJsonDe

CHAVES = ("papel", "arquivo", "linha", "coluna", "texto", "nota")


def RelacionadosDe(Saida, Codigo="E0201"):
    """Lista 'relacionados' do 1o diagnostico do codigo na saida; None se nao ha o diagnostico."""
    for Linha in LinhasJsonDe(Saida):
        if Linha.get("codigo") == Codigo:
            return Linha.get("relacionados")
    return None


def CasoRelacionadosCampoParecido(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["check", "campo_parecido.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Lista = RelacionadosDe(Rodada.stdout)
    if not Lista:
        return Problemas + [f"sem relacionados no E0201: {Rodada.stdout.strip()[:200]!r}"]
    Problemas += [f"relacionado sem as chaves {CHAVES}: {R}" for R in Lista if any(C not in R for C in CHAVES)]
    Achados = {(R.get("papel"), R.get("linha"), R.get("texto")) for R in Lista}
    for Esperado in (("parecido", 3, "vida: i32 = 3"), ("tipo", 2, "struct Nave")):
        if Esperado not in Achados:
            Problemas.append(f"falta o relacionado {Esperado}; veio {sorted(Achados)}")
    if len(Lista) > 5:
        Problemas.append(f"{len(Lista)} relacionados, o maximo e 5")
    return Problemas


def CasoRelacionadosIguaisNosBackends(Forja, PastaSaidas):
    Problemas = []
    Check = RelacionadosDe(Rodar(Forja, ["check", "campo_parecido.fj"]).stdout)
    for Backend in ("x64",):
        Saida = PastaSaidas / f"rel_{Backend}.exe"
        Rodada = Rodar(Forja, ["build", "--backend", Backend, "campo_parecido.fj", "-o", str(Saida)])
        Lista = RelacionadosDe(Rodada.stdout + Rodada.stderr)
        if not Check or Lista != Check:
            Problemas.append(f"build --backend {Backend}: relacionados {Lista!r}, o check deu {Check!r}")
    Resumo = Rodar(Forja, ["check", "--resumo", "campo_parecido.fj"])
    if "relacionados" in Resumo.stdout:
        Problemas.append("o --resumo trouxe relacionados")
    return Problemas
