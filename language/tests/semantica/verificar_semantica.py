"""Verifica a semantica do compilador Forja (Fase 5c): casos dourados, fixtures desta pasta, 'forja ast --tipos' e desempenho.

Regras:
  - ok_*.fj: 'forja check' sai com 0 e nao imprime nada (stdout e stderr vazios); se existe <nome>.esperado.txt, 'forja run' (stdin fechado)
    sai com 0 e o stdout e igual ao arquivo.
  - err_*.fj: 'forja check' sai com 1 e imprime a LISTA de diagnosticos (uma linha JSON cada, na ordem); o numero de linhas e
    codigo/arquivo(basename)/linha/coluna de cada uma batem com as linhas de <nome>.esperado.jsonl, e cada diagnostico traz
    causa, explicacao e forma_certa nao vazias.
  - 'forja ast --tipos examples/teste.fj' mostra soma=int, i=int, g=Grifo, d=Metros, t=Segundos, velocidade=MetrosPorSegundo,
    e 'forja ast' sem --tipos continua sem a chave "tipo" resolvida e sem exigir semantica.
  - --desempenho: mediana de 5 rodadas; corpus_20000 total < 300 ms e corpus_200000 check_ns <= 240 ms (trava do corte 0.24.0 = medido do E2 em Forja, 219 ms, +10%; o 25 ms era do C em gcc -O2: o C em -O0 mede 54 ms; limite <= 1,5x do C -O0 = 82 ms vale a partir da 0.25.0, errata da secao 7 do contrato_g3).

Versao: 1.7.0
Changelog: 1.7.0 - corte 0.24.0 do G3 (FORJA-D0805): LimiteCheck200kMs de 25 para 240 ms (medido do forja em Forja +10%; o 25 so o gcc -O2 do C alcancava).
           1.6.0 - Portao Z, divida (a): ok_* com <nome>.esperado.txt rodam com 'forja run' e a saida (CRLF normalizado) e comparada; o forja roda com stdin fechado (DEVNULL), nao herdado.
           1.5.0 - IA1-b15: CompararLista aceita linhas a mais no check (o compilador mostra todos os erros): as esperadas saem na ordem e a 1a obtida e a 1a esperada.
           1.4.0 - Linguagem v1.12 P21v-b: o prefixo okp_ some (os 5 okp_proc_* viraram ok_proc_*: o backend ja os desce).
           1.3.0 -Linguagem v1.12 P21v-a: aceita o prefixo okp_ (ok que o backend ainda nao desce; fora dos scanners de IR/nativo ate o P21v-b).
           1.2.0 - Linguagem v1.11 L7: err_* compara a LISTA de linhas com a do .esperado.jsonl (CompararLista), nao mais exatamente 1 linha.
           1.1.0 - Linguagem v1.11 L1: --pastas (padrao: semantica e regressao); cada pasta e contada e relatada ("N/N casos de <pasta>").
           1.0.0 - Fase 5c: primeira versao (dourado + fixtures, --tipos, desempenho, --selftest).
Verificacao: python verificar_semantica.py --selftest   |   python verificar_semantica.py   |   python verificar_semantica.py --desempenho
"""

import argparse
import json
import statistics
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
RaizLinguagem = PastaAqui.parent.parent
ForjaPadrao = RaizLinguagem / "compiler" / "forja.exe"
DouradoPadrao = PastaAqui.parent / "dourado"
PastasPadrao = [PastaAqui, PastaAqui.parent / "regressao"]
ExemploTeste = RaizLinguagem.parent / "examples" / "teste.fj"
CorpusGerado = RaizLinguagem / "benchmark" / "corpus" / "gerado"
CamposComparados = ("codigo", "arquivo", "linha", "coluna")
CamposDoDiagnostico = ("codigo", "tipo", "arquivo", "linha", "coluna", "causa", "explicacao", "forma_certa")
TiposEsperadosNoExemplo = {
    "soma": "int", "i": "int", "g": "Grifo", "d": "Metros", "t": "Segundos", "velocidade": "MetrosPorSegundo",
}
RodadasDeDesempenho = 5
LimiteTotal20kMs = 300.0
LimiteCheck200kMs = 240.0
TimeoutSegundos = 60


def LerEsperado(CaminhoFj):
    """Todas as linhas de <nome>.esperado.jsonl como lista de dicts (a lista de diagnosticos, na ordem), ou None se o arquivo nao existe."""
    Esperado = CaminhoFj.with_suffix(".esperado.jsonl")
    if not Esperado.exists():
        return None
    Linhas = [L for L in Esperado.read_text(encoding="utf-8").splitlines() if L.strip()]
    return [json.loads(L) for L in Linhas] or None


def CompararDiagnostico(Obtido, Esperado):
    """Lista de diferencas entre o diagnostico obtido e o esperado (vazia = bateu). Arquivo: so o nome-base."""
    Diferencas = []
    for Campo in CamposComparados:
        ValorObtido = Obtido.get(Campo)
        ValorEsperado = Esperado.get(Campo)
        if Campo == "arquivo":
            ValorObtido = Path(str(ValorObtido)).name
            ValorEsperado = Path(str(ValorEsperado)).name
        if ValorObtido != ValorEsperado:
            Diferencas.append(f"{Campo}: obtido {ValorObtido!r}, esperado {ValorEsperado!r}")
    return Diferencas


def FaltamCamposDoDiagnostico(Obtido):
    """Campos do contrato ausentes ou vazios no diagnostico."""
    return [C for C in CamposDoDiagnostico if Obtido.get(C) in (None, "")]


def RodarForja(Forja, *Argumentos):
    Processo = subprocess.run([str(Forja), *map(str, Argumentos)], capture_output=True, text=True, encoding="utf-8",
                              timeout=TimeoutSegundos, stdin=subprocess.DEVNULL)
    return Processo.returncode, Processo.stdout, Processo.stderr


def ConferirOk(Forja, CaminhoFj):
    Codigo, Saida, Erro = RodarForja(Forja, "check", CaminhoFj)
    if Codigo != 0:
        return [f"forja check: exit {Codigo}, esperado 0 -> {Saida.strip()[:240]} {Erro.strip()[:120]}"]
    if Saida.strip() or Erro.strip():
        return ["forja check: programa valido nao pode imprimir nada"]
    return []


def NormalizarFimDeLinha(Texto):
    """O forja run escreve CRLF no Windows e o .esperado.txt e LF: compara com LF."""
    return Texto.replace("\r\n", "\n")


def ConferirSaida(Forja, CaminhoFj):
    """Roda o ok_* com 'forja run' (stdin fechado) e compara o stdout com <nome>.esperado.txt; sem esse arquivo, nao confere nada."""
    Esperado = CaminhoFj.with_suffix(".esperado.txt")
    if not Esperado.exists():
        return []
    Codigo, Saida, Erro = RodarForja(Forja, "run", CaminhoFj)
    if Codigo != 0:
        return [f"forja run: exit {Codigo}, esperado 0 -> {Erro.strip()[:160]}"]
    Texto = NormalizarFimDeLinha(Esperado.read_text(encoding="utf-8"))
    if NormalizarFimDeLinha(Saida) != Texto:
        return [f"forja run: saida difere de {Esperado.name}: obtido {NormalizarFimDeLinha(Saida)[:120]!r}, esperado {Texto[:120]!r}"]
    return []


def CompararLista(Linhas, Esperados):
    """Falhas entre as linhas JSON obtidas e a lista esperada: cada esperada aparece nas obtidas, na mesma ordem (campos de CamposComparados), e a 1a obtida e a 1a
    esperada. b15: o check mostra todos os erros; linhas a mais (erros que antes ficavam ocultos) sao aceitas, linhas a menos nao."""
    if len(Linhas) < len(Esperados):
        return [f"forja check: {len(Linhas)} linhas no stdout, esperado {len(Esperados)} ou mais"]
    Obtidos = []
    for Posicao, Linha in enumerate(Linhas, start=1):
        try:
            Obtidos.append(json.loads(Linha))
        except json.JSONDecodeError as Falha:
            return [f"linha {Posicao}: stdout nao e JSON ({Falha})"]
    Falhas = []
    Cursor = 0
    for Posicao, Esperado in enumerate(Esperados, start=1):
        while Cursor < len(Obtidos) and CompararDiagnostico(Obtidos[Cursor], Esperado):
            if Posicao == 1:
                break
            Cursor += 1
        if Cursor >= len(Obtidos):
            return Falhas + [f"esperado {Posicao} ({Esperado.get('codigo')} linha {Esperado.get('linha')}) nao saiu"]
        Falhas += [f"linha {Cursor + 1}: {D}" for D in CompararDiagnostico(Obtidos[Cursor], Esperado)]
        Cursor += 1
    for Posicao, Obtido in enumerate(Obtidos, start=1):
        Falhas += [f"linha {Posicao}: campo '{C}' ausente ou vazio" for C in FaltamCamposDoDiagnostico(Obtido)]
    return Falhas


def ConferirErro(Forja, CaminhoFj, Esperados):
    Codigo, Saida, Erro = RodarForja(Forja, "check", CaminhoFj)
    if Codigo != 1:
        return [f"forja check: exit {Codigo}, esperado 1 (stderr: {Erro.strip()[:120]})"]
    return CompararLista([L for L in Saida.splitlines() if L.strip()], Esperados)


def VerificarCaso(Forja, CaminhoFj):
    """Devolve (nome, [falhas]) de um .fj."""
    Esperado = LerEsperado(CaminhoFj)
    if CaminhoFj.name.startswith("err_"):
        if Esperado is None:
            return CaminhoFj.name, ["falta o .esperado.jsonl"]
        return CaminhoFj.name, ConferirErro(Forja, CaminhoFj, Esperado)
    return CaminhoFj.name, ConferirOk(Forja, CaminhoFj) + ConferirSaida(Forja, CaminhoFj)


def ListarCasos(*Pastas):
    return sorted(F for Pasta in Pastas for F in Pasta.glob("*.fj") if F.name.startswith(("ok_", "err_")))


def VerificarCasos(Forja, Pasta, Rotulo):
    Casos = ListarCasos(Pasta)
    with ThreadPoolExecutor() as Pool:
        Resultados = list(Pool.map(lambda F: VerificarCaso(Forja, F), Casos))
    Falhou = [(N, Fs) for N, Fs in Resultados if Fs]
    for Nome, Falhas in Falhou:
        print(f"FALHOU {Nome}")
        for Falha in Falhas:
            print(f"    {Falha}")
    Passou = len(Resultados) - len(Falhou)
    print(f"{Passou}/{len(Resultados)} casos {Rotulo}")
    return Passou == len(Resultados) and len(Resultados) > 0


def ColetarTiposDeNos(Arvore):
    """Mapa nome -> tipo dos nos let/var/for/param/campo que trazem 'tipo' (texto) e um nome."""
    Mapa = {}
    Pilha = [Arvore]
    while Pilha:
        No = Pilha.pop()
        if isinstance(No, dict):
            Nome = No.get("nome") if "nome" in No else No.get("var")
            if isinstance(Nome, str) and isinstance(No.get("tipo"), str) and No.get("no") != "nome":
                Mapa[Nome] = No["tipo"]
            Pilha.extend(No.values())
        elif isinstance(No, list):
            Pilha.extend(No)
    return Mapa


def VerificarTipos(Forja):
    Falhas = []
    Codigo, Saida, Erro = RodarForja(Forja, "ast", "--tipos", ExemploTeste)
    if Codigo != 0:
        return [f"forja ast --tipos: exit {Codigo} -> {Saida.strip()[:200]} {Erro.strip()[:120]}"]
    Mapa = ColetarTiposDeNos(json.loads(Saida))
    for Nome, Tipo in TiposEsperadosNoExemplo.items():
        if Mapa.get(Nome) != Tipo:
            Falhas.append(f"tipo de '{Nome}': obtido {Mapa.get(Nome)!r}, esperado {Tipo!r}")
    CodigoSem, SaidaSem, _ = RodarForja(Forja, "ast", ExemploTeste)
    if CodigoSem != 0 or ColetarTiposDeNos(json.loads(SaidaSem)):
        Falhas.append("forja ast sem --tipos mudou (saiu com erro ou ganhou o tipo resolvido)")
    CodigoUso, _, _ = RodarForja(Forja, "check", "--tipos", ExemploTeste)
    if CodigoUso != 2:
        Falhas.append(f"check --tipos deveria ser erro de uso (2), saiu {CodigoUso}")
    print(f"{'FALHOU' if Falhas else 'OK'}: forja ast --tipos em examples/teste.fj ({len(TiposEsperadosNoExemplo)} tipos)")
    for Falha in Falhas:
        print(f"    {Falha}")
    return not Falhas


def MedirTempos(Forja, Arquivo):
    """Uma rodada: dict do --tempos (lex_ns, parse_ns, check_ns, total_ns)."""
    with tempfile.TemporaryDirectory() as Pasta:
        Saida = Path(Pasta) / "t.json"
        Codigo, _, Erro = RodarForja(Forja, "check", Arquivo, "--tempos", Saida)
        if Codigo != 0:
            raise RuntimeError(f"check falhou em {Arquivo.name}: exit {Codigo} {Erro.strip()[:120]}")
        return json.loads(Saida.read_text(encoding="utf-8"))


def MedianaMs(Rodadas, Campo):
    return statistics.median(R[Campo] for R in Rodadas) / 1e6


def VerificarDesempenho(Forja):
    Rodadas20k = [MedirTempos(Forja, CorpusGerado / "corpus_20000.fj") for _ in range(RodadasDeDesempenho)]
    Rodadas200k = [MedirTempos(Forja, CorpusGerado / "corpus_200000.fj") for _ in range(RodadasDeDesempenho)]
    Total20k = MedianaMs(Rodadas20k, "total_ns")
    Check200k = MedianaMs(Rodadas200k, "check_ns")
    print(f"corpus_20000 (mediana de {RodadasDeDesempenho}): lex {MedianaMs(Rodadas20k, 'lex_ns'):.2f} ms, "
          f"parse {MedianaMs(Rodadas20k, 'parse_ns'):.2f} ms, check {MedianaMs(Rodadas20k, 'check_ns'):.2f} ms, total {Total20k:.2f} ms")
    print(f"corpus_200000 (mediana de {RodadasDeDesempenho}): lex {MedianaMs(Rodadas200k, 'lex_ns'):.2f} ms, "
          f"parse {MedianaMs(Rodadas200k, 'parse_ns'):.2f} ms, check {Check200k:.2f} ms, total {MedianaMs(Rodadas200k, 'total_ns'):.2f} ms")
    Passou = Total20k < LimiteTotal20kMs and Check200k <= LimiteCheck200kMs
    print(f"{'OK' if Passou else 'FALHOU'}: portoes total20k < {LimiteTotal20kMs:.0f} ms e check200k <= {LimiteCheck200kMs:.0f} ms")
    return Passou


def Autoteste():
    Bom = {"codigo": "E0301", "arquivo": "C:/x/a.fj", "linha": 2, "coluna": 5}
    Mesmo = {"codigo": "E0301", "arquivo": "a.fj", "linha": 2, "coluna": 5}
    assert CompararDiagnostico(Bom, Mesmo) == []
    assert len(CompararDiagnostico({**Bom, "coluna": 6}, Mesmo)) == 1
    assert len(CompararDiagnostico({**Bom, "codigo": "E0001", "linha": 9}, Mesmo)) == 2
    assert FaltamCamposDoDiagnostico({"codigo": "E0301"}) != []
    Completo = {C: "x" for C in CamposDoDiagnostico}
    assert FaltamCamposDoDiagnostico(Completo) == []
    Arvore = {"declaracoes": [{"no": "let", "nome": "a", "tipo": "int", "valor": {"no": "nome", "nome": "a", "tipo": "int"}}]}
    assert ColetarTiposDeNos(Arvore) == {"a": "int"}
    assert LerEsperado(PastaAqui / "nao_existe.fj") is None
    Linha = '{"codigo": "E0301", "tipo": "t", "arquivo": "a.fj", "linha": 2, "coluna": 5, "causa": "c", "explicacao": "e", "forma_certa": "f"}'
    assert CompararLista([Linha], [Mesmo]) == []
    assert CompararLista([Linha, Linha], [Mesmo]) == []  # b15: linha a mais aceita
    Outra = Linha.replace('"linha": 2', '"linha": 9')
    assert CompararLista([Outra, Linha], [Mesmo]) != []  # a 1a obtida tem de ser a 1a esperada
    assert CompararLista([Linha], [Mesmo, Mesmo]) != []  # linha a menos
    assert NormalizarFimDeLinha("2\r\n1\r\n") == "2\n1\n"
    assert ConferirSaida(PastaAqui / "nao_existe.fj", PastaAqui / "nao_existe.fj") == []  # sem .esperado.txt nao roda nada
    print("autoteste OK: 13 verificacoes")
    return True


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao, help="caminho do forja.exe")
    Analisador.add_argument("--dourado", type=Path, default=DouradoPadrao, help="pasta do corpus dourado")
    Analisador.add_argument("--pastas", type=Path, nargs="+", default=PastasPadrao, help="pastas de casos (padrao: semantica e regressao)")
    Analisador.add_argument("--selftest", action="store_true", help="testa as funcoes de comparacao e sai")
    Analisador.add_argument("--desempenho", action="store_true", help="so mede os tempos nos corpus gerados")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return 0 if Autoteste() else 1
    if not Argumentos.forja.exists():
        print(f"forja.exe nao encontrado em {Argumentos.forja}; rode construir.bat", file=sys.stderr)
        return 2
    if Argumentos.desempenho:
        return 0 if VerificarDesempenho(Argumentos.forja) else 1
    Resultados = [VerificarCasos(Argumentos.forja, Argumentos.dourado, "dourados")]
    Resultados += [VerificarCasos(Argumentos.forja, Pasta, f"de {Pasta.name}") for Pasta in Argumentos.pastas]
    Resultados.append(VerificarTipos(Argumentos.forja))
    return 0 if all(Resultados) else 1


if __name__ == "__main__":
    sys.exit(Principal())
