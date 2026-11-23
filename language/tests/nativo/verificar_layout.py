# -*- coding: utf-8 -*-
"""Verifica o layout dos tipos da Forja contra o congelado do gcc (classe layout), pelo backend x64.

Regras:
  - Fonte = layout\\tipos_todos.fj (o universo). Ficam os tipos e procs antes do proc main; o main vira uma sonda
    temporaria em %TEMP% que imprime size_of, align_of e offset_of(T.campo) de cada tipo medido. Nada vai ao projeto.
  - A sonda compila com forja build --backend x64 e roda. Cada linha da saida casa com uma medida do roteiro, na ordem.
  - Esperado = congelado (rapido\\oraculo_congelado_g1_layout.json, selado). Lido, nunca regravado. Divergencia = o x64
    difere do gcc: e divergencia real de ABI; NAO regrave o congelado nem ajuste o esperado; relate.
  - Nao medido = o que a Forja nao alcanca: tupla de retorno e resultado `T ! E` nao sao tipo nomeavel (size_of nao
    aceita), e deslocamento de uniao, colecao e opcional (offset_of so aceita struct). Aparece na saida; classe
    obrigatoria sem nenhuma medida reprova.
  - --contra-congelado e o mesmo caminho do padrao (mantido por compatibilidade): nao ha mais gcc nem varredura de programas.
Versao: 2.0.1
Changelog: 2.0.1 - 0.21.0 F4 passo 3 (decisao do main): TC_TUPLA e TC_RESULTADO saem de ClassesObrigatorias. Tupla de retorno e `T ! E` nao sao
                   tipo nomeavel (size_of = E0101) e sem o backend C nao ha contraparte do gcc a conferir; o comportamento delas segue coberto
                   pelos testes de execucao. Os 37 deslocamentos de campo que offset_of nao alcanca (uniao, fatia, opcional, soa, dinamico)
                   tambem eram so a contraparte do C: divida registrada, nao cobertura nova.
           2.0.0 - 0.21.0 F4 passo 3: backend C aposentado (forja build --modo c --prova-layout saiu com X0003). A prova
                   passa a sair do x64: uma sonda temporaria por universo; --congelar retirado (era o lado C); a varredura
                   por programa (prova por programa) sai com o backend C.
           1.1.0 - 0.21.0 P3 (classe layout): --congelar grava, por tipo do universo tipos_todos.fj, as linhas _Static_assert
                   (size, align, offsets) onde o C (forja_g1 0.20.4 + gcc) e o x64 batem; --contra-congelado compara o x64
                   com esse congelado, sem gcc nem forja_g1 (rapido\\oraculo_congelado_g1_layout.json).
           1.0.0 - P1 do M0: primeira versao (6 grupos de corpus, cobertura por TC_*, prova com e sem -DFORJA_DEV).
Verificacao: python -I verificar_layout.py                    (precisa de ..\\compiler\\forja.exe atualizado)
             python -I verificar_layout.py --contra-congelado (mesmo caminho; so o forja.exe atual)
"""

import argparse
import base64
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path
from typing import NamedTuple

PastaAqui = Path(__file__).resolve().parent
PastaCompilador = PastaAqui.parents[1] / "compiler"
Forja = PastaCompilador / "forja.exe"
CaminhoCongelado = PastaAqui / "rapido" / "oraculo_congelado_g1_layout.json"
ClasseLayout = "layout"
CheckagemLayout = "prova"
sys.path.insert(0, str(PastaAqui / "rapido"))
import oraculo_congelado as Oraculo  # noqa: E402 (so o banco e a chave; nenhum gcc entra aqui)
Fonte = PastaAqui / "layout" / "tipos_todos.fj"
TimeoutSegundos = 120
MarcaMain = "proc main() {"
ArquivoSonda = "sonda_layout.fj"
ExeSonda = "sonda_layout.exe"
FuncaoDaSonda = {"size": "size_of", "align": "align_of"}
LimiteDivergenciasMostradas = 30
ClassesObrigatorias = [
    "TC_STRUCT", "TC_UNIAO", "TC_OPCIONAL", "TC_ARRAY", "TC_FATIA", "TC_DINAMICO",
    "TC_SOA_FIXA", "TC_SOA_DINAMICA", "TC_MATRIZ", "TC_MAPA", "TC_TEXTO", "TC_ENUM", "TC_BIT_SET",
]
RegexTamanho = re.compile(r"sizeof\((\S+?)\) == (\d+) && _Alignof\(\1\) == (\d+)")
RegexDeslocamento = re.compile(r"offsetof\(\S+?, (\w+)\) == (\d+)")

# (tipo do congelado, expressoes Forja na ordem das linhas sizeof, campos medidos com offset_of: so struct)
TABELA_MEDIDA = [
    ("TC_STRUCT fj_Padding", ("Padding",), ("a", "b", "c")),
    ("TC_STRUCT fj_Aninhada", ("Aninhada",), ("p", "f")),
    ("TC_ENUM fj_Erro", ("Erro",), ()),
    ("TC_ENUM fj_Cor5", ("Cor5",), ()),
    ("TC_UNIAO fj_Valor", ("Valor",), ()),
    ("TC_UNIAO fj_Mistura", ("Mistura",), ()),
    ("TC_ARRAY fj_Arr_35", ("[3]Padding",), ()),
    ("TC_FATIA fj_Fatia_32", ("[]Padding",), ()),
    ("TC_DINAMICO fj_Din_36", ("[dynamic]Padding",), ()),
    ("TC_SOA_FIXA fj_Soa_38", ("soa[3]Padding",), ()),
    ("TC_SOA_DINAMICA fj_Soa_39", ("soa[dynamic]Padding",), ()),
    ("TC_MATRIZ fj_Mat_40", ("matrix[3, 3]f32",), ()),
    ("TC_MATRIZ fj_Mat_41", ("matrix[4, 4]f64",), ()),
    ("TC_MAPA forja_mapa", ("map[str]int",), ()),
    ("TC_TEXTO forja_str", ("str",), ()),
    ("TC_OPCIONAL fj_Opc_33", ("?u8",), ()),
    ("TC_OPCIONAL fj_Opc_34", ("?f64",), ()),
    ("TC_BIT_SET uint8_t", ("bit_set[Cor5]",), ()),
    ("TC_BIT_SET uint32_t", ("bit_set[Cor20]",), ()),
    ("TC_BIT_SET uint64_t", ("bit_set[Cor40]", "bit_set[Cor60]"), ()),
]
# Sem expressao Forja: tupla de retorno e resultado `T ! E` nao sao tipo nomeavel; size_of nao os aceita.
SEM_EXPRESSAO = [
    "TC_TUPLA fj_Tup_29", "TC_TUPLA fj_Tup_31", "TC_RESULTADO fj_Resultado_26", "TC_RESULTADO fj_Resultado_27",
]


class ErroLayout(Exception):
    """Falha do verificador: congelado ausente ou adulterado, roteiro incoerente, build ou execucao da sonda."""


class Medida(NamedTuple):
    TipoC: str      # linha do congelado, ex.: "TC_STRUCT fj_Padding"
    Expr: str       # tipo Forja, ex.: "Padding"
    Tipo: str       # "size", "align" ou "offset"
    Campo: str      # so no offset: nome Forja do campo (sem o f_ do C)
    Esperado: int   # valor que o gcc congelou


def ChaveDoTipo(TipoC: str) -> str:
    return Oraculo.ChaveDe(Fonte, CheckagemLayout, (TipoC,), None, ClasseLayout)


def LerRegistro(Banco, TipoC: str) -> dict:
    Registro = Banco.Itens.get(ChaveDoTipo(TipoC))
    if Registro is None:
        raise ErroLayout(f"chave ausente no congelado: {TipoC} (FALHA; nao regera)")
    return Registro


def ParseCongelado(Registro: dict) -> dict:
    """Tamanhos [(size, align)] na ordem das linhas sizeof, e deslocamentos {f_campo: offset} do gcc congelado."""
    Linhas = base64.b64decode(Registro["stdout"]).decode("utf-8").splitlines()
    Tamanhos, Deslocamentos = [], {}
    for Linha in Linhas:
        Casou = RegexTamanho.search(Linha)
        if Casou:
            Tamanhos.append((int(Casou.group(2)), int(Casou.group(3))))
            continue
        Casou = RegexDeslocamento.search(Linha)
        if Casou:
            Deslocamentos[Casou.group(1)] = int(Casou.group(2))
    return {"tamanhos": Tamanhos, "deslocamentos": Deslocamentos}


def MedidasDoTipo(TipoC: str, Exprs: tuple, Campos: tuple, Banco) -> list:
    """Medidas esperadas de um tipo do congelado. Campo de struct tem de estar todo na tabela; uniao e colecao ficam nao medidos."""
    Dados = ParseCongelado(LerRegistro(Banco, TipoC))
    if len(Dados["tamanhos"]) != len(Exprs):
        raise ErroLayout(f"{TipoC}: congelado tem {len(Dados['tamanhos'])} sizeof, a tabela pede {len(Exprs)}")
    Pedidos = {"f_" + Campo for Campo in Campos}
    Ausentes = Pedidos - set(Dados["deslocamentos"])
    Sobrando = set(Dados["deslocamentos"]) - Pedidos
    if Ausentes or (TipoC.startswith("TC_STRUCT") and Sobrando):
        raise ErroLayout(f"{TipoC}: campos da tabela {sorted(Pedidos)} != congelado {sorted(Dados['deslocamentos'])}")
    Medidas = []
    for Expr, (Tamanho, Alinhamento) in zip(Exprs, Dados["tamanhos"]):
        Medidas.append(Medida(TipoC, Expr, "size", "", Tamanho))
        Medidas.append(Medida(TipoC, Expr, "align", "", Alinhamento))
        for Campo in Campos:
            Medidas.append(Medida(TipoC, Expr, "offset", Campo, Dados["deslocamentos"]["f_" + Campo]))
    return Medidas


def LinhaDaSonda(Item: Medida) -> str:
    if Item.Tipo == "offset":
        return f"    print(offset_of({Item.Expr}.{Item.Campo}))"
    return f"    print({FuncaoDaSonda[Item.Tipo]}({Item.Expr}))"


def TextoDaSonda(Medidas: list) -> str:
    """O universo (tipos e procs antes do main) mais um proc main que imprime uma linha por medida."""
    Texto = Fonte.read_text(encoding="utf-8")
    if MarcaMain not in Texto:
        raise ErroLayout(f"{Fonte.name} sem '{MarcaMain}': nao da para separar o universo da sonda")
    Cabeca = Texto.split(MarcaMain)[0]
    Linhas = "\n".join(LinhaDaSonda(Item) for Item in Medidas)
    return Cabeca + MarcaMain + "\n" + Linhas + "\n}\n"


def RodarSonda(TextoSonda: str, Pasta: Path) -> list:
    """Compila a sonda com forja build --backend x64 em Pasta (temporaria), roda e devolve os inteiros impressos."""
    Arquivo = Pasta / ArquivoSonda
    Arquivo.write_text(TextoSonda, encoding="utf-8", newline="\n")
    Exe = Pasta / ExeSonda
    Comando = [str(Forja), "build", "--backend", "x64", str(Arquivo), "-o", str(Exe)]
    Build = subprocess.run(Comando, capture_output=True, timeout=TimeoutSegundos)
    if Build.returncode != 0:
        Saida = Build.stdout.decode("utf-8", errors="replace")[:300]
        raise ErroLayout(f"forja build --backend x64 saiu com {Build.returncode} na sonda: {Saida}")
    Rodou = subprocess.run([str(Exe)], capture_output=True, timeout=TimeoutSegundos)
    if Rodou.returncode != 0:
        raise ErroLayout(f"sonda saiu com {Rodou.returncode} (esperado 0)")
    return [int(Linha) for Linha in Rodou.stdout.decode("utf-8").split()]


def RotuloDaMedida(Item: Medida) -> str:
    Alvo = f"{Item.Expr}.{Item.Campo}" if Item.Campo else Item.Expr
    return f"{Item.TipoC} [{Alvo}] {Item.Tipo}"


def CompararMedidas(Medidas: list, Valores: list) -> list:
    """Uma divergencia por medida em que o x64 difere do gcc congelado."""
    if len(Valores) != len(Medidas):
        raise ErroLayout(f"sonda imprimiu {len(Valores)} valores, o roteiro tem {len(Medidas)} medidas")
    return [f"{RotuloDaMedida(Item)}: x64={Valor} gcc={Item.Esperado}"
            for Item, Valor in zip(Medidas, Valores) if Valor != Item.Esperado]


def ConferirContagem(Banco) -> list:
    Esperado = len(TABELA_MEDIDA) + len(SEM_EXPRESSAO)
    if len(Banco.Itens) == Esperado:
        return []
    return [f"contagem: congelado {len(Banco.Itens)} itens, universo {Esperado} tipos"]


def ImprimirResumo(Banco, Medidas: list, Cobertura: Counter) -> None:
    for Classe in ClassesObrigatorias:
        print(f"cobertura {Classe}={Cobertura.get(Classe, 0)}")
    for TipoC in SEM_EXPRESSAO:
        print(f"NAO MEDIDO {TipoC}: sem tipo nomeavel na Forja (size_of e offset_of nao aceitam)")
    Deslocamentos = [len(ParseCongelado(Registro)["deslocamentos"]) for Registro in Banco.Itens.values()]
    Tamanhos = [len(ParseCongelado(Registro)["tamanhos"]) for Registro in Banco.Itens.values()]
    TamanhosMedidos = sum(1 for Item in Medidas if Item.Tipo == "size")
    OffsetsMedidos = sum(1 for Item in Medidas if Item.Tipo == "offset")
    print(f"tamanhos medidos={TamanhosMedidos}/{sum(Tamanhos)} (size e align); "
          f"deslocamentos medidos={OffsetsMedidos}/{sum(Deslocamentos)} (offset_of so aceita struct)")


def ConferirLayout(Pasta: Path) -> int:
    Inicio = time.perf_counter()
    Banco = Oraculo.BancoOraculo(CaminhoCongelado)
    if Banco.Selado:
        Banco.ConferirSelo()
    Medidas = []
    for TipoC, Exprs, Campos in TABELA_MEDIDA:
        Medidas += MedidasDoTipo(TipoC, Exprs, Campos, Banco)
    Valores = RodarSonda(TextoDaSonda(Medidas), Pasta)
    Divergencias = ConferirContagem(Banco) + CompararMedidas(Medidas, Valores)
    Cobertura = Counter(Item.TipoC.split(" ")[0] for Item in Medidas if Item.Tipo == "size")
    ImprimirResumo(Banco, Medidas, Cobertura)
    for Divergencia in Divergencias[:LimiteDivergenciasMostradas]:
        print(f"DIVERGENCIA {Divergencia}")
    Faltam = [Classe for Classe in ClassesObrigatorias if Cobertura.get(Classe, 0) == 0]
    if Faltam:
        print("SEM COBERTURA: " + ", ".join(Faltam))
    Tempo = time.perf_counter() - Inicio
    print(f"layout x64 contra congelado: medidas={len(Medidas)} divergencias={len(Divergencias)} tempo={Tempo:.2f}s")
    return 1 if Divergencias or Faltam else 0


def Main():
    Parser = argparse.ArgumentParser(description="Prova de layout dos tipos da Forja contra o congelado do gcc (classe layout, backend x64).")
    Parser.add_argument("--contra-congelado", action="store_true", help="mantido por compatibilidade: mesmo caminho do padrao")
    Parser.parse_args()
    if not Forja.exists():
        print(f"forja.exe nao achado em {Forja}; rode construir.bat antes")
        return 1
    Pasta = Path(tempfile.mkdtemp(prefix="forja_layout_"))
    try:
        return ConferirLayout(Pasta)
    except (ErroLayout, ValueError, OSError, subprocess.TimeoutExpired) as Erro:
        print(f"FALHA layout: {Erro}")
        return 1
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Main())
