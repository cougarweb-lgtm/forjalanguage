"""Gera os dois projetos de medicao do orquestrador de projeto (P22-O3): "largo" e "proj250".

largo: o corpus_200000 (mesma semente e mesmos moldes do benchmark\\corpus\\gerador\\gerar_corpus.py, importado SEM alteracao) dividido em 12 DLLs de ~16,7 mil
linhas (parte_1..parte_12, cada uma com um "pub export proc parte_k()" que roda as chamadas dos seus blocos) + 1 exe (jogo) que chama parte_1..parte_12 por foreign.
A saida do jogo concatenada = corpus_200000.esperado.txt byte a byte (gravada tambem em <pasta>\\esperado.txt).
proj250: 250 modulos de ~800 linhas (13 blocos de molde cada) em 3 camadas: dll "base" (40 modulos), 8 dlls d1..d8 (25 modulos cada, dependem da base), exe "app" (10
modulos, depende das 8). Todas as camadas importam comum\\tipos.fj (struct Entidade): mudar o campo do struct muda o .h da base, e por ela o .h das 8 e o exe.

Versao: 0.3.0
Changelog: 0.3.0 - M3 P22-O3: sai o contorno do stdout (foreign kernel32 GetCurrentProcessId em cada dll raiz): toda dll Forja pura usa o FILE stdout do UCRT.
           0.2.0 - recusa pasta de saida nao vazia (codigo 2, "pasta de saida nao vazia: <caminho>"); ModuloFj (dataclass) no lugar dos 6 parametros de
           FonteDeModulo; --selftest cobre a recusa e as mutacoes.
           0.1.0 - M3 P22-O3: primeira versao (largo, proj250, mutacoes de corpo/struct/assets para o medir_projeto.py).
Uso: python -I gerar_projeto.py --projeto largo|proj250 --saida <pasta vazia ou inexistente> [--semente N]
Verificacao: python -I gerar_projeto.py --selftest
"""

import argparse
import hashlib
import json
import re
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

sys.dont_write_bytecode = True  # nao deixar __pycache__ na pasta do gerador do benchmark (que nao pode ser alterada)
RAIZ_LINGUAGEM = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(RAIZ_LINGUAGEM / "benchmark" / "corpus" / "gerador"))
import gerar_corpus  # noqa: E402  (lint-ok: import depois do sys.path, e o gerador do benchmark importado por caminho)

SEMENTE_PADRAO = 20261107            # a do corpus_200000 do benchmark
LINHAS_LARGO = 200000
DLLS_LARGO = 12
BLOCOS_POR_MODULO = 13               # 13 x 61 linhas = ~800 linhas por modulo
BASE_MODULOS = 40
DLLS_DEPENDENTES = 8
MODULOS_POR_DLL = 25
EXE_MODULOS = 10
PADRAO_UNIDADES = re.compile(r"\b(Metros|Segundos|MetrosPorSegundo)\b")
VIDA_INICIAL = 3
X_INICIAL = 4
FONTE_TIPOS = """pub struct Entidade {
    vida: i32
    x: i32
}

pub proc nova_entidade(vida: i32, x: i32) -> Entidade {
    return Entidade{vida = vida, x = x}
}

pub proc somar_entidade(e: Entidade) -> i32 {
    return e.vida + e.x
}
"""
TEXTO_PROC_DEFAULT_ANTES ="\n    return r\n"
TEXTO_PROC_DEFAULT_DEPOIS = "\n    return r + 0\n"


def Escrever(Pasta, Relativo, Texto):
    """Grava Texto (UTF-8, LF) em Pasta/Relativo criando as pastas."""
    Alvo = Path(Pasta) / Relativo
    Alvo.parent.mkdir(parents=True, exist_ok=True)
    Alvo.write_bytes(Texto.encode("utf-8"))


def GerarBlocos(Sorteio, Primeiro, Quantidade):
    """Devolve (declaracoes, chamadas, saidas) de Quantidade blocos numerados a partir de Primeiro, sorteando na mesma ordem do gerar_corpus."""
    Declaracoes, Chamadas, Saidas = [], [], []
    for Bloco in range(Primeiro, Primeiro + Quantidade):
        for Molde in gerar_corpus.MOLDES:
            Declaracao, Chamada, Saida = Molde(Sorteio, Bloco)
            Declaracoes.append(Declaracao)
            Chamadas.extend(Chamada)
            Saidas.extend(Saida)
    return Declaracoes, Chamadas, Saidas


@dataclass
class ModuloFj:
    """Um .fj de medicao: Nome do proc que roda as chamadas, Declarador ('pub proc' ou 'pub export proc'), Declaracoes e Chamadas dos blocos.
    Importacoes e Cabecalho sao opcionais (Cabecalho None = CABECALHO_UNIDADES do gerador do benchmark)."""
    Nome: str
    Declarador: str
    Declaracoes: list
    Chamadas: list
    Importacoes: list = field(default_factory=list)
    Cabecalho: Optional[list] = None


def LinhasDeImportacao(Importacoes):
    """'import "x"' por linha, com uma linha em branco depois quando ha alguma importacao."""
    Texto = "".join(f'import "{Importacao}"\n' for Importacao in Importacoes)
    return Texto + ("\n" if Importacoes else "")


def FonteDeModulo(Modulo):
    """Cabecalho de unidades + importacoes + declaracoes dos blocos + o proc que roda as chamadas (Declarador 'pub export proc' acrescenta o 'proc main' vazio)."""
    Exporta = Modulo.Declarador == "pub export proc"
    Texto = "\n".join(Modulo.Cabecalho or gerar_corpus.CABECALHO_UNIDADES) + "\n\n"
    Texto += LinhasDeImportacao(Modulo.Importacoes)
    Texto += "\n\n".join(Modulo.Declaracoes) + "\n\n"
    Texto += f"{Modulo.Declarador} {Modulo.Nome}() {{\n" + "\n".join(Modulo.Chamadas) + "\n}\n"
    # o compilador exige 'proc main()' mesmo na dll (E0201); o proc de rodar fica antes
    return Texto + ("\nproc main() {\n}\n" if Exporta else "")


def DividirEmFatias(Total, Fatias):
    """Total dividido em Fatias partes inteiras, as primeiras com 1 a mais quando sobra."""
    Base, Resto = divmod(Total, Fatias)
    return [Base + (1 if Indice < Resto else 0) for Indice in range(Fatias)]


def Sha256Texto(Texto):
    return hashlib.sha256(Texto.encode("utf-8")).hexdigest()


def GravarEsperado(Pasta, Tipo, Produtos, Esperado, Extra):
    """Grava esperado.txt e gerado.json (metadados lidos pelo medir_projeto.py)."""
    Escrever(Pasta, "esperado.txt", Esperado)
    Meta = {"tipo": Tipo, "produtos": Produtos, "linhas_esperado": Esperado.count("\n"), "sha256_esperado": Sha256Texto(Esperado)}
    Meta.update(Extra)
    Escrever(Pasta, "gerado.json", json.dumps(Meta, indent=2) + "\n")
    return Meta


def FonteJogoLargo():
    Foreigns = "".join(f'foreign {{\n    link "parte_{K}"\n    proc parte_{K}()\n}}\n\n' for K in range(1, DLLS_LARGO + 1))
    Chamadas = "".join(f"    parte_{K}()\n" for K in range(1, DLLS_LARGO + 1))
    return Foreigns + "proc main() {\n" + Chamadas + "}\n"


def GerarLargo(Pasta, Semente=SEMENTE_PADRAO, Linhas=LINHAS_LARGO):
    """Projeto largo: 12 dlls + 1 exe. Devolve os metadados (gerado.json)."""
    Total = max(1, round(Linhas / gerar_corpus.LINHAS_POR_BLOCO))
    Sorteio = gerar_corpus.Sorteador(Semente)
    Primeiro, Saidas, LinhasDasDlls = 0, [], []
    Manifesto = "saida = bin\n"
    for Parte, Quantidade in enumerate(DividirEmFatias(Total, DLLS_LARGO), 1):
        Declaracoes, Chamadas, Saida = GerarBlocos(Sorteio, Primeiro, Quantidade)
        Fonte = FonteDeModulo(ModuloFj(f"parte_{Parte}", "pub export proc", Declaracoes, Chamadas))
        Escrever(Pasta, f"parte_{Parte}.fj", Fonte)
        Manifesto += f"produto parte_{Parte} dll parte_{Parte}.fj\n"
        Primeiro += Quantidade
        Saidas.extend(Saida)
        LinhasDasDlls.append(Fonte.count("\n"))
    Escrever(Pasta, "jogo.fj", FonteJogoLargo())
    Manifesto += "produto jogo exe jogo.fj depende " + " ".join(f"parte_{K}" for K in range(1, DLLS_LARGO + 1)) + "\n"
    Escrever(Pasta, "forja.proj", Manifesto)
    Esperado = "\n".join(Saidas) + "\n"
    Produtos = DLLS_LARGO + 1
    return GravarEsperado(Pasta, "largo", Produtos, Esperado, {"semente": Semente, "linhas_pedidas": Linhas, "linhas_por_dll": LinhasDasDlls})


def GerarModulos(Pasta, Sorteio, Subpasta, Prefixo, Quantidade):
    """Escreve Quantidade modulos <Subpasta>/<Prefixo><nn>.fj, cada um com 'pub proc rodar_<nome>()'. Devolve ([nomes], {nome: saidas})."""
    Nomes, Saidas = [], {}
    for Numero in range(1, Quantidade + 1):
        Nome = f"{Prefixo}{Numero:02d}"
        # nomes de topo (procs, structs, units) sao unicos no programa: blocos numerados pela camada e units com o nome do modulo (PascalCase) como sufixo
        Declaracoes, Chamadas, Saida = GerarBlocos(Sorteio, (Numero - 1) * BLOCOS_POR_MODULO, BLOCOS_POR_MODULO)
        Renomear = lambda Texto: PADRAO_UNIDADES.sub(lambda Achado: f"{Achado.group(1)}{Nome.upper()}", Texto)
        Modulo = ModuloFj(f"rodar_{Nome}", "pub proc", [Renomear(D) for D in Declaracoes], [Renomear(C) for C in Chamadas],
                          Cabecalho=[Renomear(U) for U in gerar_corpus.CABECALHO_UNIDADES])
        Fonte = FonteDeModulo(Modulo)
        Escrever(Pasta, f"{Subpasta}/{Nome}.fj", Fonte)
        Nomes.append(Nome)
        Saidas[Nome] = Saida
    return Nomes, Saidas


def ImportsDe(Subpasta, Nomes):
    return ["comum/tipos"] + [f"{Subpasta}/{Nome}" for Nome in Nomes]


def ChamarModulos(Nomes):
    return "".join(f"    {Nome}.rodar_{Nome}()\n" for Nome in Nomes)


def FonteDaBase(Nomes):
    Texto = "".join(f'import "{Imp}"\n' for Imp in ImportsDe("base", Nomes)) + "\n"
    Texto += "pub export proc base_rodar() {\n" + ChamarModulos(Nomes) + "}\n\n"
    Texto += "pub export proc base_valor(e: tipos.Entidade) -> i32 {\n    return tipos.somar_entidade(e)\n}\n\nproc main() {\n}\n"
    return Texto


def FonteDaDll(Indice, Nomes):
    Texto = "".join(f'import "{Imp}"\n' for Imp in ImportsDe(f"d{Indice}", Nomes)) + "\n"
    Texto += 'foreign {\n    link "base"\n    proc base_valor(e: tipos.Entidade) -> i32\n'
    Texto += "    proc base_rodar()\n}\n\n" if Indice == 1 else "}\n\n"
    Texto += f"pub export proc d{Indice}_rodar(e: tipos.Entidade) -> i32 {{\n"
    Texto += "    base_rodar()\n" if Indice == 1 else ""
    return Texto + ChamarModulos(Nomes) + "    return base_valor(e)\n}\n\nproc main() {\n}\n"


def FonteDoApp(Nomes):
    Texto = "".join(f'import "{Imp}"\n' for Imp in ImportsDe("app", Nomes)) + "\n"
    for Indice in range(1, DLLS_DEPENDENTES + 1):
        Texto += f'foreign {{\n    link "d{Indice}"\n    proc d{Indice}_rodar(e: tipos.Entidade) -> i32\n}}\n\n'
    Texto += f"proc main() {{\n    let e = tipos.nova_entidade({VIDA_INICIAL}, x = {X_INICIAL})\n" + ChamarModulos(Nomes)
    Texto += "".join(f"    print(d{Indice}_rodar(e))\n" for Indice in range(1, DLLS_DEPENDENTES + 1))
    return Texto + "}\n"


def GerarProj250(Pasta, Semente=SEMENTE_PADRAO):
    """Projeto proj250: base (40 modulos) -> 8 dlls (25 cada) -> app (10). Devolve os metadados (gerado.json)."""
    Sorteio = gerar_corpus.Sorteador(Semente)
    Escrever(Pasta, "comum/tipos.fj", FONTE_TIPOS)
    NomesBase, SaidasBase = GerarModulos(Pasta, Sorteio, "base", "b", BASE_MODULOS)
    Escrever(Pasta, "base.fj", FonteDaBase(NomesBase))
    Manifesto = "saida = bin\nproduto base dll base.fj\n"
    SaidasDlls = []
    for Indice in range(1, DLLS_DEPENDENTES + 1):
        Nomes, Saidas = GerarModulos(Pasta, Sorteio, f"d{Indice}", f"d{Indice}m", MODULOS_POR_DLL)
        Escrever(Pasta, f"d{Indice}.fj", FonteDaDll(Indice, Nomes))
        Manifesto += f"produto d{Indice} dll d{Indice}.fj depende base\n"
        SaidasDlls.append([Linha for Nome in Nomes for Linha in Saidas[Nome]])
    NomesApp, SaidasApp = GerarModulos(Pasta, Sorteio, "app", "e", EXE_MODULOS)
    Escrever(Pasta, "app.fj", FonteDoApp(NomesApp))
    Manifesto += "produto app exe app.fj depende " + " ".join(f"d{K}" for K in range(1, DLLS_DEPENDENTES + 1)) + "\n"
    Escrever(Pasta, "forja.proj", Manifesto)
    Valor = str(VIDA_INICIAL + X_INICIAL)
    Saidas = [Linha for Nome in NomesApp for Linha in SaidasApp[Nome]]
    # base_rodar roda dentro do d1, antes dos modulos do d1: a saida da base vem logo depois dos modulos do app
    Saidas += [Linha for Nome in NomesBase for Linha in SaidasBase[Nome]]
    for SaidaDll in SaidasDlls:
        Saidas += SaidaDll + [Valor]
    return GravarEsperado(Pasta, "proj250", 1 + DLLS_DEPENDENTES + 1, "\n".join(Saidas) + "\n", {"semente": Semente, "modulos": BASE_MODULOS + DLLS_DEPENDENTES * MODULOS_POR_DLL + EXE_MODULOS})


class PastaOcupada(RuntimeError):
    """A pasta de saida ja tem conteudo: o gerador recusa para nao misturar arquivos novos com um projeto antigo (o Main sai com codigo 2)."""


def PastaNaoVazia(Pasta):
    """True se Pasta existe e tem conteudo (ou nao e pasta, o que tambem impede a geracao)."""
    Caminho = Path(Pasta)
    return Caminho.exists() and (not Caminho.is_dir() or any(Caminho.iterdir()))


def Gerar(Tipo, Pasta, Semente=SEMENTE_PADRAO):
    """Gera o projeto Tipo ('largo' ou 'proj250') em Pasta (vazia ou inexistente) e devolve os metadados; pasta com conteudo levanta PastaOcupada."""
    if PastaNaoVazia(Pasta):
        raise PastaOcupada(f"pasta de saida nao vazia: {Pasta}")
    if Tipo == "largo":
        return GerarLargo(Pasta, Semente)
    if Tipo == "proj250":
        return GerarProj250(Pasta, Semente)
    raise ValueError(f"projeto desconhecido: {Tipo!r} (use largo ou proj250)")


def ArquivoDeCorpoMutavel(Pasta, Tipo):
    """O arquivo cujo corpo a mutacao 'so corpo de 1 dll' altera: parte_7 no largo, um modulo da d4 no proj250."""
    return Path(Pasta) / ("parte_7.fj" if Tipo == "largo" else "d4/d4m10.fj")


def MutarCorpo(Pasta, Tipo):
    """Troca 'return r' por 'return r + 0' no corpo de um proc de 1 dll: o .h (interface) e a saida do programa nao mudam."""
    Arquivo = ArquivoDeCorpoMutavel(Pasta, Tipo)
    Texto = Arquivo.read_text(encoding="utf-8")
    if TEXTO_PROC_DEFAULT_ANTES not in Texto:
        raise RuntimeError(f"{Arquivo}: nao achei o trecho do corpo a mutar")
    Arquivo.write_bytes(Texto.replace(TEXTO_PROC_DEFAULT_ANTES, TEXTO_PROC_DEFAULT_DEPOIS, 1).encode("utf-8"))


def DesfazerCorpo(Pasta, Tipo):
    Arquivo = ArquivoDeCorpoMutavel(Pasta, Tipo)
    Texto = Arquivo.read_text(encoding="utf-8")
    Arquivo.write_bytes(Texto.replace(TEXTO_PROC_DEFAULT_DEPOIS, TEXTO_PROC_DEFAULT_ANTES, 1).encode("utf-8"))


def MutarStruct(Pasta, Nome="energia"):
    """Renomeia o campo 'vida' do struct exportado (comum\\tipos.fj): o .h da base muda (so o proj250 tem o struct)."""
    Arquivo = Path(Pasta) / "comum" / "tipos.fj"
    Arquivo.write_bytes(Arquivo.read_text(encoding="utf-8").replace("vida", Nome).encode("utf-8"))


def DesfazerStruct(Pasta, Nome="energia"):
    Arquivo = Path(Pasta) / "comum" / "tipos.fj"
    Arquivo.write_bytes(Arquivo.read_text(encoding="utf-8").replace(Nome, "vida").encode("utf-8"))


def CriarAssets(Pasta):
    """Cria (ou retoca) um .glb, um .png e um .wav nas pastas do projeto: assets nunca entram no .entradas, entao nao disparam build."""
    Conteudo = bytes(range(256)) * 16
    for Relativo in ("assets/modelo.glb", "assets/textura.png", "assets/som.wav", "bin/extra.glb"):
        Alvo = Path(Pasta) / Relativo
        Alvo.parent.mkdir(parents=True, exist_ok=True)
        Alvo.write_bytes(Conteudo)


def ConferirGeracao(Temp):
    """Falhas do selftest: gera o largo de 2000 linhas e o proj250 completo em Temp; confere o esperado do largo contra o gerar_corpus e a estrutura do proj250."""
    PastaLargo, PastaProj = Path(Temp) / "largo", Path(Temp) / "proj250"
    Meta = GerarLargo(PastaLargo, SEMENTE_PADRAO, 2000)
    Esperado = gerar_corpus.GerarCorpus(SEMENTE_PADRAO, 2000)[1]
    MetaProj = GerarProj250(PastaProj)
    Modulos = len(list(PastaProj.glob("*/*.fj"))) - 1  # menos o comum\tipos.fj
    Falhas = []
    if (PastaLargo / "esperado.txt").read_text(encoding="utf-8") != Esperado:
        Falhas.append("esperado do largo difere do gerar_corpus")
    if Modulos != 250 or MetaProj["modulos"] != 250 or Meta["produtos"] != 13 or MetaProj["produtos"] != 10:
        Falhas.append(f"modulos={Modulos} produtos largo={Meta['produtos']} proj250={MetaProj['produtos']}")
    return Falhas


def ConferirPastaOcupada(Temp):
    """Falhas do selftest: pasta de saida com um arquivo tem de ser recusada com codigo 2, sem arquivo novo dentro."""
    Pasta = Path(Temp) / "ocupada"
    Pasta.mkdir()
    (Pasta / "sobra.txt").write_text("x", encoding="utf-8")
    Codigo = Main(["--projeto", "largo", "--saida", str(Pasta)])
    Sobras = sorted(Arquivo.name for Arquivo in Pasta.iterdir())
    return [] if Codigo == 2 and Sobras == ["sobra.txt"] else ["pasta de saida nao vazia nao foi recusada com codigo 2"]


def ExercitarMutacoes(Temp):
    """Roda as mutacoes e desfazeres de corpo e struct no proj250 de Temp (o medir_projeto.py depende deles)."""
    PastaProj = Path(Temp) / "proj250"
    MutarCorpo(PastaProj, "proj250")
    DesfazerCorpo(PastaProj, "proj250")
    MutarStruct(PastaProj)
    DesfazerStruct(PastaProj)


def Selftest():
    """Confere a geracao, a recusa de pasta nao vazia e as mutacoes; sai com 1 se alguma falhar."""
    with tempfile.TemporaryDirectory(prefix="gerar_projeto_") as Temp:
        Falhas = ConferirGeracao(Temp) + ConferirPastaOcupada(Temp)
        if not Falhas:
            ExercitarMutacoes(Temp)
    for Falha in Falhas:
        print(f"SELFTEST FALHOU: {Falha}")
    if Falhas:
        return 1
    print("SELFTEST OK")
    return 0


def Main(Argumentos=None):
    Parser = argparse.ArgumentParser(description="Gera os projetos largo e proj250 do P22-O3.")
    Parser.add_argument("--projeto", choices=["largo", "proj250"])
    Parser.add_argument("--saida")
    Parser.add_argument("--semente", type=int, default=SEMENTE_PADRAO)
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args(Argumentos)
    if Args.selftest:
        return Selftest()
    if not Args.projeto or not Args.saida:
        Parser.error("informe --projeto e --saida (ou --selftest)")
    try:
        Meta = Gerar(Args.projeto, Args.saida, Args.semente)
    except PastaOcupada as Erro:
        # codigo 2 (uso invalido) e nao 1: a pasta esta errada, nada foi gerado
        print(Erro, file=sys.stderr)
        return 2
    print(json.dumps(Meta, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(Main())
