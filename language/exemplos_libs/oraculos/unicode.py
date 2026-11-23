#!/usr/bin/env python3
r"""unicode.py - oraculo do pacote vendor:unicode (utf8proc 2.12.0 sobre o Unicode 18.0.0).

Fonte independente: os arquivos oficiais UnicodeData.txt e NormalizationTest.txt da unicode.org (pasta <ucd>), nao o utf8proc.
Faz:
  (a) calcula, para todo codepoint 0..0x10FFFF menos surrogates, a categoria geral (sem linha = Cn), a minuscula e a maiuscula
      simples, e produz o MESMO resumo que unicode_basico.fj (contagem por categoria, totais e hash FNV-1a 64 dos registros);
  (b) gera o arquivo de casos NFC/NFD (entrada_hex TAB nfc_hex TAB nfd_hex, bytes UTF-8) com as colunas c1, c2, c3 do NormalizationTest;
  (c) com --forja, roda o exemplo sem argumentos (compara com (a) e com os 4 casos embutidos) e depois com o arquivo de casos
      (`forja run exemplo -- <casos>`), conferindo nfc/nfd N/N.
Uso: python -I oraculos\unicode.py <pasta_ucd> [--casos <arq>] [--forja <forja.exe absoluto>]
Versao: 0.1.0
Changelog: 0.1.0 - primeira versao (categoria, minuscula/maiuscula simples, hash FNV-1a 64 e casos NFC/NFD do NormalizationTest).
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

VersaoUtf8proc = "2.12.0"  # tarball v2.12.0 (ver vendor\unicode\dependencia.json)
Exemplo = Path(__file__).resolve().parents[1] / "unicode_basico.fj"
MascaraDe64Bits = (1 << 64) - 1
PrimoFnv = 1099511628211
BaseFnv = 0xcbf29ce484222325
LimiteCodepoint = 0x110000
# mesma ordem do enum unicode.GeneralCategory e da tabela de nomes do exemplo (indice = byte do hash)
Ordem = [
    ("Lu", "letter_uppercase"), ("Ll", "letter_lowercase"), ("Lt", "letter_titlecase"), ("Lm", "letter_modifier"),
    ("Lo", "letter_other"), ("Mn", "mark_nonspacing"), ("Mc", "mark_spacing_combining"), ("Me", "mark_enclosing"),
    ("Nd", "number_decimal_digit"), ("Nl", "number_letter"), ("No", "number_other"), ("Pc", "punctuation_connector"),
    ("Pd", "punctuation_dash"), ("Ps", "punctuation_open"), ("Pe", "punctuation_close"),
    ("Pi", "punctuation_initial_quote"), ("Pf", "punctuation_final_quote"), ("Po", "punctuation_other"),
    ("Sm", "symbol_math"), ("Sc", "symbol_currency"), ("Sk", "symbol_modifier"), ("So", "symbol_other"),
    ("Zs", "separator_space"), ("Zl", "separator_line"), ("Zp", "separator_paragraph"), ("Cc", "other_control"),
    ("Cf", "other_format"), ("Cs", "other_surrogate"), ("Co", "other_private_use"), ("Cn", "unassigned"),
]
IndicePorCategoria = {Sigla: Indice for Indice, (Sigla, _) in enumerate(Ordem)}
# Unica divergencia medida entre o utf8proc 2.12.0 e o UnicodeData 18.0.0 nos mapeamentos simples (20/11/2026, varredura
# dos 1112064 codepoints): o utf8proc devolve U+1E9E para a maiuscula de U+00DF; o UnicodeData nao tem mapeamento simples
# para U+00DF (so o SpecialCasing, "SS"). Aplicada ao oraculo e avisada na saida, para nao esconder a diferenca.
ExcecoesMaiuscula = {0x00DF: 0x1E9E}
# 4 casos embutidos no exemplo (sem argumentos); o oraculo confere que cada um esta no NormalizationTest
CasosEmbutidos = [
    ("c385", "c385", "41cc8a"),
    ("c3a9", "c3a9", "65cc81"),
    ("e1b88a", "e1b88a", "44cc87"),
    ("cd84", "cc88cc81", "cc88cc81"),
]


def LerUnicodeData(Ucd: Path):
    """Categoria, minuscula e maiuscula simples por codepoint (linhas <..., First>/<..., Last> viram faixa)."""
    Categorias, Baixa, Alta = {}, {}, {}
    InicioFaixa = None
    for Linha in (Ucd / "UnicodeData.txt").read_text(encoding="utf-8").splitlines():
        if not Linha:
            continue
        Campos = Linha.split(";")
        Cp = int(Campos[0], 16)
        Nome, Sigla = Campos[1], Campos[2]
        if Nome.endswith(", First>"):
            InicioFaixa = Cp
            continue
        if Nome.endswith(", Last>"):
            for Ponto in range(InicioFaixa, Cp + 1):
                Categorias[Ponto] = Sigla
            InicioFaixa = None
            continue
        Categorias[Cp] = Sigla
        if Campos[12]:
            Alta[Cp] = int(Campos[12], 16)
        if Campos[13]:
            Baixa[Cp] = int(Campos[13], 16)
    return Categorias, Baixa, Alta


def FnvBytes(Hash: int, Registro: bytes) -> int:
    for Byte in Registro:
        Hash = ((Hash ^ Byte) * PrimoFnv) & MascaraDe64Bits
    return Hash


def ResumoLinhas(Ucd: Path, VersaoUnicode: str) -> list:
    """Linhas do resumo exatamente como o unicode_basico.fj imprime (sem as linhas de casos)."""
    Categorias, Baixa, Alta = LerUnicodeData(Ucd)
    Alta.update(ExcecoesMaiuscula)
    Contagem = [0] * len(Ordem)
    Total = ComMinuscula = ComMaiuscula = 0
    Hash = BaseFnv
    for Cp in range(LimiteCodepoint):
        if 0xD800 <= Cp <= 0xDFFF:
            continue
        Indice = IndicePorCategoria[Categorias.get(Cp, "Cn")]
        Minuscula = Baixa.get(Cp, Cp)
        Maiuscula = Alta.get(Cp, Cp)
        Contagem[Indice] += 1
        Total += 1
        if Minuscula != Cp:
            ComMinuscula += 1
        if Maiuscula != Cp:
            ComMaiuscula += 1
        Registro = (Cp.to_bytes(4, "little") + bytes([Indice])
                    + Minuscula.to_bytes(4, "little") + Maiuscula.to_bytes(4, "little"))
        Hash = FnvBytes(Hash, Registro)
    Linhas = [f"utf8proc {VersaoUtf8proc} / Unicode {VersaoUnicode}"]
    Linhas += [f"{Nome} {Contagem[Indice]}" for Indice, (_, Nome) in enumerate(Ordem)]
    Linhas += [f"total {Total}", f"com_minuscula {ComMinuscula}", f"com_maiuscula {ComMaiuscula}",
               f"hash_fnv1a64 {Hash:016x}"]
    return Linhas


def HexDeCodepoints(TextoCodepoints: str) -> str:
    return "".join(chr(int(Parte, 16)).encode("utf-8").hex() for Parte in TextoCodepoints.split())


def LerCasos(Ucd: Path) -> list:
    """(entrada, nfc, nfd) em hex de bytes UTF-8, das colunas c1, c2 e c3 do NormalizationTest."""
    Casos = []
    for Linha in (Ucd / "NormalizationTest.txt").read_text(encoding="utf-8").splitlines():
        if not Linha or Linha.startswith(("#", "@")):
            continue
        Campos = Linha.split("#")[0].split(";")
        if len(Campos) < 5:
            continue
        Casos.append((HexDeCodepoints(Campos[0]), HexDeCodepoints(Campos[1]), HexDeCodepoints(Campos[2])))
    return Casos


def VersaoDoUnicode(Ucd: Path) -> str:
    """Versao do Unicode pelo cabecalho do NormalizationTest (ex.: NormalizationTest-18.0.0.txt)."""
    for Linha in (Ucd / "NormalizationTest.txt").read_text(encoding="utf-8").splitlines()[:3]:
        if Linha.startswith("# NormalizationTest-"):
            return Linha.split("NormalizationTest-")[1].split(".txt")[0]
    raise SystemExit("versao do Unicode nao achada no cabecalho do NormalizationTest.txt")


def RodarForja(Forja: Path, Argumentos: list) -> str:
    Proc = subprocess.run([str(Forja), "run", str(Exemplo), *Argumentos], capture_output=True, timeout=900)
    if Proc.returncode != 0:
        raise SystemExit(f"forja run falhou ({Proc.returncode}): {Proc.stderr.decode('utf-8', 'replace')[-400:]}")
    return Proc.stdout.decode("utf-8")


def CompararComForja(Forja: Path, Argumentos: list, Esperado: list, Rotulo: str) -> bool:
    Saida = RodarForja(Forja, Argumentos).splitlines()
    if Saida == Esperado:
        return True
    print(f"DIVERGE ({Rotulo}):")
    for Meu, Dela in zip(Esperado, Saida):
        if Meu != Dela:
            print(f"  oraculo: {Meu}\n  forja:   {Dela}")
            break
    else:
        print(f"  linhas: oraculo {len(Esperado)}, forja {len(Saida)}")
    return False


def Main() -> int:
    Analisador = argparse.ArgumentParser(description="oraculo do vendor:unicode (UCD oficial)")
    Analisador.add_argument("ucd", type=Path, help="pasta com UnicodeData.txt e NormalizationTest.txt (Unicode 18.0.0)")
    Analisador.add_argument("--casos", type=Path, default=Path(tempfile.gettempdir()) / "unicode_nfc_casos.txt")
    Analisador.add_argument("--forja", type=Path, default=None, help="forja.exe (caminho absoluto) para rodar o exemplo")
    Argumentos = Analisador.parse_args()
    Ucd = Argumentos.ucd.resolve()

    VersaoUnicode = VersaoDoUnicode(Ucd)
    if VersaoUnicode != "18.0.0":
        print(f"versao do Unicode {VersaoUnicode} difere da declarada pela lib (18.0.0)")
        return 1
    Casos = LerCasos(Ucd)
    Faltam = [Caso for Caso in CasosEmbutidos if Caso not in set(Casos)]
    if Faltam:
        print(f"casos embutidos fora do NormalizationTest: {Faltam}")
        return 1
    Argumentos.casos.write_text("".join(f"{A}\t{B}\t{C}\n" for A, B, C in Casos), encoding="utf-8", newline="\n")
    print(f"casos gravados: {Argumentos.casos} ({len(Casos)} linhas)")
    print("excecao aplicada: U+00DF maiuscula = U+1E9E (utf8proc; UnicodeData sem mapeamento simple)", file=sys.stderr)

    Resumo = ResumoLinhas(Ucd, VersaoUnicode)
    Embutidos = len(CasosEmbutidos)
    Esperado = Resumo + [f"nfc ok {Embutidos}/{Embutidos}", f"nfd ok {Embutidos}/{Embutidos}", "erros 0"]
    EsperadoArquivo = Resumo + [f"nfc ok {len(Casos)}/{len(Casos)}", f"nfd ok {len(Casos)}/{len(Casos)}", "erros 0"]
    print("\n".join(EsperadoArquivo))
    if Argumentos.forja is None:
        return 0

    Forja = Argumentos.forja.resolve()
    Ok = CompararComForja(Forja, [], Esperado, "sem argumentos")
    Ok = CompararComForja(Forja, ["--", str(Argumentos.casos)], EsperadoArquivo, "arquivo de casos") and Ok
    print("OK: forja bate com o oraculo" if Ok else "FALHOU")
    return 0 if Ok else 1


if __name__ == "__main__":
    sys.exit(Main())
