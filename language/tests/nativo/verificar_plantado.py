# -*- coding: utf-8 -*-
"""Prova que o diferencial (verificar_nativo, niveis 1 e 2) localiza divergencias plantadas (P18b).

Para cada tests\\nativo\\plantado\\p<k>.fj builda o x64 em dev duas vezes, sem FORJA_TESTE_PLANTAR (referencia) e com FORJA_TESTE_PLANTAR=<k>
(a variavel so e lida pelo x64), e compara a localizacao EXATA com p<k>.esperado.json:
  p1 e p2: X0802 com arquivo, linha, coluna, funcao e instr do traco; p3: X0803 (codigo de saida) com esperado 0 e obtido 1.
Sem o backend C (0.21.0 F4 residuo) a referencia e o proprio x64 sem o plantio: Comparar acusa a divergencia e DetalheX0802
(--traco nos dois lados) localiza a primeira instrucao divergente.
Campo null no .esperado.json = o diagnostico nao traz esse campo. Imprime "plantadas=3 localizadas=3"; saida 0 so se todas localizaram.

Uso:
    python -I verificar_plantado.py [--forja <exe>]

Versao: 0.2.0
Changelog: 0.2.0 - 0.21.0 F4 residuo: sem o backend C a referencia e o x64 sem FORJA_TESTE_PLANTAR (antes, o lado C do CompararArquivo).
           0.1.0 - P18b: primeira versao (usa CompararArquivo do verificar_nativo 1.7.0; nada do diferencial e duplicado).
Verificacao: python -I verificar_plantado.py
"""

import argparse
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui))
import verificar_nativo as Diferencial  # noqa: E402

PastaPlantado = PastaAqui / "plantado"
VariavelPlantio = "FORJA_TESTE_PLANTAR"
ModoDoPlantio = "dev"
QuantidadePlantios = 3
# No X0803 o "esperado" e o "obtido" saem do diagnostico do diferencial como c_hex e x64_hex.
CamposDoX0803 = {"esperado": "c_hex", "obtido": "x64_hex"}


def AcharDiagnostico(Detalhes, Codigo):
    """O diagnostico de um codigo na lista que o diferencial devolveu (X0801 de stdout vem junto do X0802); None se nao veio."""
    for Item in Detalhes:
        if Item.get("codigo") == Codigo:
            return Item
    return None


def ValorObtido(Diag, Campo):
    if Diag.get("codigo") == "X0803" and Campo in CamposDoX0803:
        return Diag.get(CamposDoX0803[Campo])
    if Campo == "arquivo":
        return Path(Diag["arquivo"]).name
    return Diag.get(Campo)


def Divergencias(Esperado, Diag):
    """Campos do .esperado.json que o diagnostico nao confirma: lista de (campo, esperado, obtido)."""
    Erros = []
    for Campo, Valor in Esperado.items():
        Obtido = ValorObtido(Diag, Campo)
        if Obtido != Valor:
            Erros.append((Campo, Valor, Obtido))
    return Erros


def RodarLado(Forja, Arquivo, PastaTmp, Nome, Plantio):
    """Build x64 com --traco e a execucao de um lado: Plantio=0 sem a variavel; 1..3 com FORJA_TESTE_PLANTAR=<k>. None se o build recusar."""
    Exe = Path(PastaTmp) / f"{Nome}.exe"
    os.environ.pop(VariavelPlantio, None)
    if Plantio:
        os.environ[VariavelPlantio] = str(Plantio)
    try:
        Codigo, Erro = Diferencial.Compilar(Forja, Arquivo, False, Exe, True, PastaTmp, True)
    finally:
        os.environ.pop(VariavelPlantio, None)
    if Codigo != 0:
        print(f"{Nome}: build x64 recusou (rc={Codigo}): {Erro.decode('utf-8', 'replace')[:300]}")
        return None
    return Diferencial.Executar(Exe, PastaTmp)


def RodarPlantio(Forja, Numero, PastaTmp):
    """Roda p<Numero>.fj sem e com o plantio (x64 contra x64) e devolve os diagnosticos ([] se os dois lados ficaram iguais)."""
    Arquivo = str(PastaPlantado / f"p{Numero}.fj")
    Referencia = RodarLado(Forja, Arquivo, PastaTmp, f"p{Numero}_ref", 0)
    Plantada = RodarLado(Forja, Arquivo, PastaTmp, f"p{Numero}_plant", Numero)
    if Referencia is None or Plantada is None:
        return []
    Divergencia = Diferencial.Comparar(Arquivo, ModoDoPlantio, Referencia, Plantada, False)
    if Divergencia is None:
        return []
    X0802 = Diferencial.DetalheX0802(Forja, Arquivo, ModoDoPlantio, (Referencia, Plantada))
    return [Divergencia] + ([X0802] if X0802 else [])


def Localizou(Forja, Numero, PastaTmp):
    Esperado = json.loads((PastaPlantado / f"p{Numero}.esperado.json").read_text(encoding="utf-8"))
    Diag = AcharDiagnostico(RodarPlantio(Forja, Numero, PastaTmp), Esperado["codigo"])
    if Diag is None:
        print(f"p{Numero}: {Esperado['codigo']} nao veio do diferencial")
        return False
    Erros = Divergencias(Esperado, Diag)
    for Campo, Valor, Obtido in Erros:
        print(f"p{Numero}: campo {Campo}: esperado {Valor!r}, obtido {Obtido!r}")
    if not Erros:
        print(f"p{Numero}: {Esperado['codigo']} localizado ({json.dumps(Esperado)})")
    return not Erros


def SemPlantioFicaIgual(Forja, Numero, PastaTmp):
    """Controle negativo: duas builds x64 do mesmo p<Numero>.fj SEM a variavel tem de dar 'igual' (so o plantio cria a divergencia)."""
    Arquivo = str(PastaPlantado / f"p{Numero}.fj")
    Um = RodarLado(Forja, Arquivo, PastaTmp, f"p{Numero}_ctl_a", 0)
    Dois = RodarLado(Forja, Arquivo, PastaTmp, f"p{Numero}_ctl_b", 0)
    Igual = Um is not None and Dois is not None and Diferencial.Comparar(Arquivo, ModoDoPlantio, Um, Dois, False) is None
    print(f"p{Numero}: sem {VariavelPlantio} -> {'igual' if Igual else 'diferente'}")
    return Igual


def Principal():
    Parser = argparse.ArgumentParser(description="Localiza as 3 divergencias plantadas (FORJA_TESTE_PLANTAR).")
    Parser.add_argument("--forja", default=None, help="exe da Forja (padrao: copia em TEMP)")
    Args = Parser.parse_args()

    PastaTmp = tempfile.mkdtemp(prefix="verificar_plantado_")
    try:
        Forja = Args.forja or Diferencial.CopiarForja(PastaTmp)
        Localizadas = sum(Localizou(Forja, K, PastaTmp) for K in range(1, QuantidadePlantios + 1))
        Limpos = sum(SemPlantioFicaIgual(Forja, K, PastaTmp) for K in range(1, QuantidadePlantios + 1))
    finally:
        shutil.rmtree(PastaTmp, ignore_errors=True)
    print(f"plantadas={QuantidadePlantios} localizadas={Localizadas} controle_negativo={Limpos}")
    return 0 if Localizadas == QuantidadePlantios and Limpos == QuantidadePlantios else 1


if __name__ == "__main__":
    sys.exit(Principal())
