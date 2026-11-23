# -*- coding: utf-8 -*-
"""variante.py - preprocessador das variantes de sintaxe P1 e P2 -> Forja v1.11 (P-IA1-a08, contrato 3.3, D-IA1-10 e 4.2).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao. Traduz SO por tokens e texto escrito (nenhuma analise semantica): V-01 corpo de 1 comando na
        mesma linha, V-02 ';', V-03 && || !, V-04 i++/i--, V-05 ternario (formas de comando; ver variante_tern.py), V-06 chamada
        posicional pela assinatura lida do proprio arquivo, dos .fj da pasta, da std e dos embutidos, V-08 main -> int, V-09 fn/func/def,
        e no P2 os rotulos alloc/to/scope (V-07). O fonte e as colunas sao em BYTES (latin-1), como as do compilador. As linhas de
        saida apontam, por mapa de segmentos, para a linha/coluna do fonte do agente; so V-01, V-05 e V-08 (import novo) mudam o
        NUMERO de linhas (o mapa e muitos-para-um). Alvo do binario: "base" (forja_ia1_base, nao conhece std:os: sai std:proc as os) ou
        "main" (forja_b01_main ou mais novo: std:os como esta).

Uso:
  python variante.py traduzir <origem> -o <destino> --pacote P1|P2 [--alvo base|main] [--std <pasta>]   (arquivo ou pasta; JSON de resumo)
  python variante.py remapear <origem> --destino <pasta> --pacote P1|P2 [--alvo ...]                       (stdin -> stdout, linhas JSON)
  python variante.py --selftest
Pareamento cartao x binario: cartao_P0 (std:os) -> forja_b01_main (md5 B3D3E306...) ou mais novo; cartao_P1 e cartao_P2 (std:os) rodam
nos DOIS binarios, porque este preprocessador troca std:os por std:proc as os quando o par e o forja_ia1_base (md5 39903581...).
Saida do traduzir: {"arquivos", "erros" (E0110 no formato do compilador), "pendencias" (pontos que exigiriam analise semantica)}.
Reversao: apagar os variante*.py. Depende de: Python 3.9+ (biblioteca padrao). Verificacao: python variante.py --selftest
"""
import argparse
import json
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from variante_chamada import Contexto, CarregarModulos, LerAssinaturas, TraduzirChamadas
from variante_regras import (AcharAlias, AjustarImport, DividirBlocos, TirarPontoVirgula, TraduzirMainRetorno, TrocarIncDec,
                             TrocarLogicos, TrocarPalavraProc)
from variante_seg import MapaDaLinha, Juntar, Lexar, Novo, PosicaoOrigem, Seg
from variante_tern import RegistrarTipos, TraduzirTernario

MD5_BASE = "39903581280b80ad0ebbded52bf547a5"
ALIAS_DO_MAIN = "vr_os"
PASTA_STD = Path(__file__).resolve().parents[2] / "std"
IGNORAR = (".traduzido", ".bin", "fontes", "entrada", "material", "__pycache__")


class Resultado:
    def __init__(self, Linhas, Ctx):
        self.Texto = "\n".join(Juntar(S) for S in Linhas)
        self.Mapa = [MapaDaLinha(S) for S in Linhas]
        self.Erros, self.Pendencias = Ctx.Erros, Ctx.Pendencias


def Estagio1(Linhas, Ctx):
    """Regras que olham uma linha so: fn, ';', import do par e a divisao dos corpos de 1 comando."""
    Saida = []
    for Segs in Linhas:
        Saida += DividirBlocos(AjustarImport(TirarPontoVirgula(TrocarPalavraProc(Segs)), Ctx.Alvo))
    return Saida


def LinhaDeImport(Ctx):
    Ancora = Seg("nome", "", 1, 0, False)
    Modulo = '"std:proc"' if Ctx.Alvo == "base" else '"std:os"'
    return [Novo("nome", "import " + Modulo + " as " + ALIAS_DO_MAIN, Ancora)]


def RetornoDoMain(Linhas, Ctx):
    """V-08: main -> int; cria o import de std:os com apelido proprio quando o arquivo nao tem."""
    Alias = AcharAlias(Linhas)
    Linhas, Usou = TraduzirMainRetorno(Linhas, Alias or ALIAS_DO_MAIN)
    if Usou and Alias is None:
        Linhas.insert(0, LinhaDeImport(Ctx))
    return Linhas


def TraduzirInterior(Achado, Ctx):
    """'{expr:fmt}' de um f"...": a expressao passa por && || ! e pela chamada posicional; o formato fica."""
    Conteudo = Achado.group(1)
    if Conteudo is None:
        return Achado.group(0)
    Corte = Conteudo.find(":")
    Expr, Formato = (Conteudo, "") if Corte < 0 else (Conteudo[:Corte], Conteudo[Corte:])
    return "{" + Juntar(TraduzirChamadas(TrocarLogicos(Lexar(Expr, 1)), Ctx)) + Formato + "}"


def TraduzirFTextos(Segs, Ctx):
    """Chamadas dentro de {...} de f"..." (o interior perde o mapa de coluna fina: aponta para o inicio do f"...")."""
    for I, S in enumerate(Segs):
        if S.Tipo == "ftexto" and "{" in S.Txt:
            Novo_ = re.sub(r"\{\{|\}\}|\{([^{}\"]*)\}", lambda M: TraduzirInterior(M, Ctx), S.Txt)
            if Novo_ != S.Txt:
                Segs[I] = Seg("ftexto", Novo_, S.Lin, S.Col, False)
    return Segs


def TraduzirTexto(Texto, Ctx):
    """Texto (str latin-1) do fonte do agente -> Resultado (texto v1.11, mapa por linha, erros E0110, pendencias)."""
    Linhas = Estagio1([Lexar(L, N + 1) for N, L in enumerate(Texto.split("\n"))], Ctx)
    Ctx.Locais = LerAssinaturas(Linhas)
    CarregarModulos(Linhas, Ctx)
    RegistrarTipos(Linhas, Ctx)
    Linhas = [TraduzirChamadas(TrocarIncDec(TrocarLogicos(TraduzirFTextos(S, Ctx))), Ctx) for S in Linhas]
    Saida = []
    for Segs in RetornoDoMain(Linhas, Ctx):
        Saida += TraduzirTernario(Segs, Ctx)
    return Resultado(Saida, Ctx)


def TraduzirBytes(Conteudo, Pacote, Alvo, PastaOrigem=None, PastaStd=None):
    """Um fonte (bytes) -> (Resultado, bytes traduzidos)."""
    Ctx = Contexto(Pacote, Alvo, PastaStd or PASTA_STD, PastaOrigem)
    Res = TraduzirTexto(Conteudo.decode("latin-1"), Ctx)
    return Res, Res.Texto.encode("latin-1")


class Traducao:
    """Uma pasta traduzida: origem, destino e, por arquivo (caminho relativo), o Resultado."""

    def __init__(self, Origem, Destino):
        self.Origem, self.Destino, self.Arquivos = Path(Origem), Path(Destino), {}

    def Erros(self):
        Lista = []
        for Rel, Res in sorted(self.Arquivos.items()):
            Lista += [dict(codigo=E["codigo"], tipo=E["tipo"], arquivo=str(self.Origem / Rel), **{K: V for K, V in E.items() if K not in ("codigo", "tipo")}) for E in Res.Erros]
        return Lista

    def Pendencias(self):
        return [(Rel, Lin, Motivo) for Rel, Res in sorted(self.Arquivos.items()) for Lin, Motivo in Res.Pendencias]


def ArquivosFj(Origem):
    Origem = Path(Origem)
    if Origem.is_file():
        return [Origem]
    return sorted(P for P in Origem.rglob("*.fj") if not any(Parte in IGNORAR for Parte in P.relative_to(Origem).parts))


def TraduzirPasta(Origem, Destino, Pacote, Alvo, PastaStd=None):
    """Traduz todos os .fj de Origem (arquivo ou pasta) para Destino, mantendo os caminhos relativos."""
    Base = Path(Origem).parent if Path(Origem).is_file() else Path(Origem)
    Trad = Traducao(Base, Destino)
    for Arq in ArquivosFj(Origem):
        Rel = Arq.relative_to(Base)
        Res, Bytes = TraduzirBytes(Arq.read_bytes(), Pacote, Alvo, Base, PastaStd)
        Saida = Path(Destino) / Rel
        Saida.parent.mkdir(parents=True, exist_ok=True)
        Saida.write_bytes(Bytes)
        Trad.Arquivos[Rel.as_posix()] = Res
    return Trad


def Normalizar(Caminho):
    return os.path.normcase(os.path.abspath(str(Caminho))).replace("\\", "/")


def RelativoDoArquivo(Trad, Texto):
    """Chave (caminho relativo) do arquivo citado num diagnostico; None se nao e da traducao."""
    Alvo = Normalizar(Texto)
    for Rel in Trad.Arquivos:
        if Normalizar(Trad.Destino / Rel) == Alvo or Normalizar(Trad.Origem / Rel) == Alvo:
            return Rel
    Nomes = [Rel for Rel in Trad.Arquivos if Path(Rel).name == Path(Texto).name]
    return Nomes[0] if len(Nomes) == 1 else None


def RemapearObjeto(Obj, Trad):
    """Percorre o JSON de um diagnostico e devolve linha/coluna/coluna_fim para o fonte do agente (recursivo)."""
    if isinstance(Obj, list):
        return [RemapearObjeto(I, Trad) for I in Obj]
    if not isinstance(Obj, dict):
        return Obj
    Novo_ = {Chave: RemapearObjeto(Valor, Trad) for Chave, Valor in Obj.items()}
    Rel = RelativoDoArquivo(Trad, Obj["arquivo"]) if isinstance(Obj.get("arquivo"), str) else None
    if Rel is None or not isinstance(Obj.get("linha"), int):
        return Novo_
    return RemapearPosicao(Novo_, Obj, Trad, Rel)


def RemapearPosicao(Novo_, Obj, Trad, Rel):
    Mapa = Trad.Arquivos[Rel].Mapa
    Linha = Obj["linha"]
    if not 1 <= Linha <= len(Mapa) or not isinstance(Obj.get("coluna"), int):
        return Novo_
    Pos = PosicaoOrigem(Mapa[Linha - 1], Obj["coluna"])
    if Pos is None:
        return Novo_
    Novo_["arquivo"], Novo_["linha"], Novo_["coluna"] = str(Trad.Origem / Rel), Pos[0], Pos[1]
    if isinstance(Obj.get("coluna_fim"), int):
        Fim = PosicaoOrigem(Mapa[Linha - 1], Obj["coluna_fim"])
        Novo_["coluna_fim"] = Fim[1] if Fim and Fim[0] == Pos[0] and Fim[1] > Pos[1] else Pos[1] + 1
    return Novo_


def RemapearTexto(Texto, Trad):
    """Remapeia cada linha JSON do texto (stdout/stderr do compilador ou do programa); o resto passa sem mudar."""
    Saida = []
    for Linha in Texto.split("\n"):
        Saida.append(RemapearLinha(Linha, Trad))
    return "\n".join(Saida)


def RemapearLinha(Linha, Trad):
    if not Linha.lstrip().startswith("{"):
        return Linha
    try:
        return json.dumps(RemapearObjeto(json.loads(Linha), Trad), ensure_ascii=False)
    except ValueError:
        return Linha


def ErrosComoJson(Trad):
    return "\n".join(json.dumps(E, ensure_ascii=False) for E in Trad.Erros())


def AlvoDoForja(Caminho):
    """'base' se o binario e o forja_ia1_base congelado (md5 39903581...); 'main' para qualquer outro (mais novo)."""
    import hashlib
    try:
        return "base" if hashlib.md5(Path(Caminho).read_bytes()).hexdigest() == MD5_BASE else "main"
    except OSError:
        return "main"


def Argumentos(Argv):
    P = argparse.ArgumentParser(description="Preprocessador P1/P2 -> Forja v1.11")
    P.add_argument("comando", nargs="?", choices=["traduzir", "remapear"])
    P.add_argument("origem", nargs="?")
    P.add_argument("-o", "--destino")
    P.add_argument("--pacote", default="P1", choices=["P1", "P2"])
    P.add_argument("--alvo", default="base", choices=["base", "main"])
    P.add_argument("--std", default=None)
    P.add_argument("--selftest", action="store_true")
    return P.parse_args(Argv)


def Principal(Argv):
    Args = Argumentos(Argv)
    if Args.selftest:
        import selftest_variante
        return selftest_variante.Executar()
    if not Args.comando or not Args.origem or not Args.destino:
        print("uso: variante.py traduzir|remapear <origem> -o <destino> --pacote P1|P2 [--alvo base|main]  |  --selftest", file=sys.stderr)
        return 2
    Trad = TraduzirPasta(Args.origem, Args.destino, Args.pacote, Args.alvo, Args.std)
    if Args.comando == "traduzir":
        print(json.dumps({"arquivos": len(Trad.Arquivos), "erros": Trad.Erros(), "pendencias": Trad.Pendencias()}, ensure_ascii=False))
        return 1 if Trad.Erros() else 0
    sys.stdout.write(RemapearTexto(sys.stdin.read(), Trad))
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
