# -*- coding: utf-8 -*-
"""coleta_ia_first.py - coletar: verifica as celulas prontas e grava <celula>\\resultado.json (P-IA1-a07, contrato 4.6).

Versao 0.2.1
Changelog:
  0.2.1 (12/11/2026) E24 nos pacotes P1/P2: o variante.py traduz cada arquivo COM os outros do conjunto na pasta de origem (V-06 le a assinatura do
        modulo ali); antes cada arquivo ia sozinho e `modulo.proc(a, b)` virava E0502 falso na re-verificacao (4 celulas P1/P2 da E24 sairam
        vermelhas com o wrapper verde). 1 arquivo: igual a antes.
  0.2.0 (12/11/2026) E24 (2 arquivos): a re-verificacao de cada ida usa o CONJUNTO de fontes guardado pelo ia.py 0.3.0 (fontes\\<sha>.conj\\*; celula de
        1 arquivo segue em fontes\\<sha>.src, igual a antes) e o verde final usa todos os fontes da raiz da celula (ia.ArquivosDoConjunto), nao so
        o prog.<ext>. Pacotes P1/P2: cada arquivo do conjunto passa pelo variante.py. Fecha o limite herdado da 0.1.1. Reversao: copia 0.1.2 em
        %TEMP%\\ia_antes_e24\\ (rodadas antigas continuam lidas: .src e resultado.json inalterados).
  0.1.2 (11/11/2026) P-IA1-a17: celulas dos conjuntos P e R usam MedidasDePR (conjunto_pr.py): P nota o resposta.txt contra o gabarito (verde = acuracia >= 95%);
        R confere a PASTA inteira (check, run e checagens estruturais). Os demais conjuntos seguem o mesmo caminho de antes.
  0.1.1 (11/11/2026) P-IA1-a08: nos pacotes P1/P2 (celula.json "variante") o fonte guardado passa por variante.py antes do verificador
        (forma antiga E0110 = vermelho com o motivo). Limite herdado do a07: so o prog.<ext> e guardado; tarefa multi-arquivo (E24)
        nao tem como ser verificada pelos fontes guardados.
  0.1.0 (11/11/2026) primeira versao. pass@1 = o fonte guardado na ida 1 passa no verificador; idas_ate_verde = a 1a ida cujo
        fonte guardado passa (None = nunca; vale LIMITE 5); verde_final = o prog.<ext> que sobrou na celula; L = resposta.txt
        igual ao gabarito (espacos no fim de linha e linhas finais vazias ignorados). Verificador: verificar_tarefa (E) e
        verificar_conjunto (Cx, Dx, C_forja, D_forja). Celula invalida (contaminacao.py) e refeita UMA vez (pasta _t2); a 2a
        invalidacao grava PARA. Sem transcript achado a celula fica valida com contaminacao_verificada = false.

Reversao: apagar os resultado.json. Depende de: Python 3.9+, gcc no PATH (referencia C), verificar_tarefa e verificar_conjunto.
"""
import tempfile
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path
from types import SimpleNamespace

import conjunto_pr as PR
import contaminacao
import ia as IA
import lint_casa
import verificar_conjunto as VC
import verificar_tarefa as VT
from celula_ia_first import CriarCelula
from ia_comum import RAIZ, GravarJson, LerJson, LerJsonl

CAMPOS_DA_CELULA = ("id", "rodada", "modelo", "conjunto", "tarefa", "lingua", "braco", "repeticao", "tentativa")


def NormalizarResposta(Texto):
    Linhas = [L.rstrip() for L in Texto.replace("\r\n", "\n").split("\n")]
    while Linhas and Linhas[-1] == "":
        Linhas.pop()
    return "\n".join(Linhas)


def VerdeL(Cel, Pasta):
    Gab = LerJson(RAIZ / "referencias" / Cel["lingua"] / "L" / Cel["tarefa"] / "gabarito.json")
    Resposta = Pasta / "resposta.txt"
    if not Resposta.is_file():
        return False, "resposta.txt nao existe"
    Esperado = NormalizarResposta("%sexit=%d" % (Gab["stdout"], Gab["exit"]))
    Obtido = NormalizarResposta(Resposta.read_text(encoding="utf-8", errors="replace"))
    return Obtido == Esperado, "" if Obtido == Esperado else "resposta difere do gabarito"


def NovoContexto(Forja):
    return {"forja": Path(Forja), "vc": {}, "cache": {}}


def ContextoConjunto(Ctx, Conj):
    if Conj not in Ctx["vc"]:
        Ctx["vc"][Conj] = VC.MontarCtx(RAIZ, Ctx["forja"], [Conj])
    return Ctx["vc"][Conj]


def VerdeE(Cel, Ctx, Fonte):
    Args = SimpleNamespace(Forja=str(Ctx["forja"]), Tarefas=str(RAIZ / "tarefas" / "E"))
    R = VT.Verificar(Args, VT.LerJson(RAIZ / "linguagens.json"), (Cel["tarefa"], Cel["lingua"], Fonte))
    return R["verde"] is True, R.get("motivo", "")


def VerdeBug(Cel, Ctx, Pasta):
    """Cx, Dx, C_forja, D_forja: o programa corrigido tem de passar limpo e bater com o esperado.json (mesmo criterio da referencia)."""
    VCtx = ContextoConjunto(Ctx, Cel["conjunto"])
    Decl = VCtx["catalogos"][Cel["conjunto"]][Cel["tarefa"]][Cel["lingua"]]
    Modo = Decl.get("modo", "run")
    Esp = VC.LerJson(VC.PastaTarefa(VCtx, Cel["conjunto"], Cel["tarefa"], Cel["lingua"]) / "esperado.json")["casos"][0]
    Motivos = VC.MotivosReferencia(VC.RodarFases(VCtx, Cel["lingua"], Pasta, Modo), Esp, Modo)
    return not Motivos, "; ".join(Motivos)[:300]


def TraduzirSePreciso(Cel, Arquivos):
    """Pacotes P1/P2: cada fonte do agente passa por variante.py antes do verificador. Arquivos = {nome: bytes}.
    Devolve ({nome: bytes}, None) ou (None, motivo de erro)."""
    Dados = Cel.get("variante")
    if not Dados:
        return Arquivos, None
    import variante
    Traduzidos = {}
    # V-06 le a assinatura do modulo nos .fj da pasta de origem: com 2+ arquivos eles precisam estar la (1 arquivo = como antes)
    with tempfile.TemporaryDirectory(prefix="ia_trad_") as Origem:
        if len(Arquivos) > 1:
            for Nome, Conteudo in Arquivos.items():
                (Path(Origem) / Nome).write_bytes(Conteudo)
        PastaOrigem = Origem if len(Arquivos) > 1 else None
        for Nome, Conteudo in Arquivos.items():
            Res, Bytes = variante.TraduzirBytes(Conteudo, Dados["pacote"], Dados["alvo"], PastaOrigem, Dados["std"])
            if Res.Erros:
                return None, "forma antiga: " + "; ".join(E["causa"] for E in Res.Erros[:3])
            Traduzidos[Nome] = Bytes
    return Traduzidos, None


def NomeDoPrincipal(Cel):
    return "prog" + Cel["extensao"]


def VerdeDoConjunto(Cel, Ctx, Arquivos):
    """(verde, motivo) de um conjunto de fontes {nome: bytes} para a tarefa da celula; a pasta e remontada num diretorio temporario."""
    Arquivos, Erro = TraduzirSePreciso(Cel, Arquivos)
    if Arquivos is None:
        return False, Erro
    with tempfile.TemporaryDirectory(prefix="ia_ver_") as Dir:
        Pasta = Path(Dir)
        for Nome, Conteudo in Arquivos.items():
            (Pasta / Nome).write_bytes(Conteudo)
        if Cel["conjunto"] == "E":
            return VerdeE(Cel, Ctx, Pasta / NomeDoPrincipal(Cel))
        return VerdeBug(Cel, Ctx, Pasta)


def ConjuntoGuardado(Cel, Pasta, Sha):
    """Fontes da ida de hash Sha como {nome: bytes}; None se nao foi guardado (.src = 1 arquivo, o principal; .conj = pasta com todos)."""
    if not Sha:
        return None
    Unico = Pasta / "fontes" / (Sha + ".src")
    if Unico.is_file():
        return {NomeDoPrincipal(Cel): Unico.read_bytes()}
    Varios = Pasta / "fontes" / (Sha + ".conj")
    if Varios.is_dir():
        return {A.name: A.read_bytes() for A in Varios.iterdir() if A.is_file()}
    return None


def VerdeDoSha(Cel, Ctx, Pasta, Sha):
    Chave = (Cel["id"], Sha)
    if Chave not in Ctx["cache"]:
        Arquivos = ConjuntoGuardado(Cel, Pasta, Sha)
        Ctx["cache"][Chave] = VerdeDoConjunto(Cel, Ctx, Arquivos) if Arquivos else (False, "sem fonte guardado")
    return Ctx["cache"][Chave][0]


def IdasAteVerde(Cel, Ctx, Pasta, Linhas):
    for Linha in (L for L in Linhas if L.get("tipo") == "ida"):
        if VerdeDoSha(Cel, Ctx, Pasta, Linha.get("sha_fonte")):
            return Linha["n"]
    return None


def VerdeComUmaMaquina(Cel, Ctx, Pasta, Linhas):
    """pass@1 + 1 ida de maquina: a 1a correcao aplicada logo depois da ida 1 deixa o fonte verde."""
    Idas = 0
    for Linha in Linhas:
        Idas += Linha.get("tipo") == "ida"
        if Linha.get("tipo") == "maquina":
            return Idas == 1 and VerdeDoSha(Cel, Ctx, Pasta, Linha.get("sha_depois"))
    return False


def HistogramaDeCodigos(Linhas):
    Contagem = {}
    for Linha in (L for L in Linhas if L.get("tipo") == "ida"):
        for Codigo in Linha.get("codigos", []):
            Contagem[Codigo] = Contagem.get(Codigo, 0) + 1
    return Contagem


def MedidasDeIdas(Cel, Ctx, Pasta, Linhas):
    """Campos que vem do idas.jsonl e dos fontes guardados (conjuntos que compilam)."""
    Idas = [L for L in Linhas if L.get("tipo") == "ida"]
    ATe = IdasAteVerde(Cel, Ctx, Pasta, Linhas)
    return {"idas": len(Idas), "idas_ate_verde": ATe, "pass1": ATe == 1, "pass5": ATe is not None,
            "pass1_mais_maquina": ATe == 1 or VerdeComUmaMaquina(Cel, Ctx, Pasta, Linhas),
            "idas_maquina": sum(1 for L in Linhas if L.get("tipo") == "maquina"),
            "consultas": sum(1 for L in Linhas if L.get("tipo") == "consulta"),
            "chars_diagnostico": sum(L.get("chars_stdout", 0) + L.get("chars_stderr", 0) for L in Idas),
            "chars_consulta": sum(L.get("chars_stdout", 0) for L in Linhas if L.get("tipo") == "consulta"),
            "violou_l": any(L.get("tipo") == "proibido" for L in Linhas), "codigos": HistogramaDeCodigos(Linhas)}


def MedidasDeL(Cel, Pasta, Linhas):
    Verde, Motivo = VerdeL(Cel, Pasta)
    return {"idas": 0, "idas_ate_verde": 0 if Verde else None, "pass1": Verde, "pass5": Verde, "pass1_mais_maquina": Verde,
            "idas_maquina": 0, "consultas": 0, "chars_diagnostico": 0, "chars_consulta": 0, "codigos": {},
            "violou_l": any(L.get("tipo") in ("ida", "proibido") for L in Linhas), "verde_final": Verde, "motivo_final": Motivo}


def MedidasDePR(Cel, Ctx, Pasta, Linhas):
    """Conjuntos P (resposta.txt das 20 perguntas) e R (pasta refatorada, varios arquivos): a conta fica em conjunto_pr.py."""
    if Cel["conjunto"] == "P":
        return PR.MedidasDeP(Cel, Pasta, Linhas)
    return PR.MedidasDeR(Cel, Ctx["forja"], Pasta, Linhas)


def VerdeFinal(Cel, Ctx, Pasta):
    if not (Pasta / NomeDoPrincipal(Cel)).is_file():
        return False, "prog nao existe"
    return VerdeDoConjunto(Cel, Ctx, {A.name: A.read_bytes() for A in IA.ArquivosDoConjunto(Pasta, Cel["extensao"])})


def Contaminacao(Pasta):
    Celula = Pasta / "celula.json"
    Transcript = contaminacao.AcharTranscript(Pasta, Desde=Celula.stat().st_mtime - 60)
    if Transcript is None:
        return {"valida": True, "verificada": False, "motivos": [], "tokens": None, "parede_s": None, "ferramentas": {}}
    return dict(contaminacao.Analisar(Transcript, Pasta), verificada=True)


def ResultadoDaCelula(Cel, Ctx, Pasta):
    Linhas = LerJsonl(Pasta / "idas.jsonl")
    if Cel["conjunto"] == "L":
        Medidas = MedidasDeL(Cel, Pasta, Linhas)
    elif Cel["conjunto"] in ("P", "R"):
        Medidas = MedidasDePR(Cel, Ctx, Pasta, Linhas)
    else:
        Medidas = MedidasDeIdas(Cel, Ctx, Pasta, Linhas)
        Medidas["verde_final"], Medidas["motivo_final"] = VerdeFinal(Cel, Ctx, Pasta)
    Contam = Contaminacao(Pasta)
    Lint = lint_casa.AchadosDoArquivo(Pasta / "prog.fj") if Cel["lingua"] == "forja" and (Pasta / "prog.fj").is_file() else []
    Base = {Campo: Cel[Campo] for Campo in CAMPOS_DA_CELULA}
    return dict(Base, **Medidas, valida=Contam["valida"], contaminacao_verificada=Contam["verificada"], motivos_invalidez=Contam["motivos"],
                tokens=Contam["tokens"], parede_s=Contam["parede_s"], ferramentas=Contam["ferramentas"],
                lint_bw5=Lint if Cel["conjunto"] == "E" else [])


def TemAtividade(Pasta):
    return (Pasta / "idas.jsonl").is_file() or (Pasta / "resposta.txt").is_file()


def Pendentes(Rodada):
    """Ultima tentativa de cada celula que ainda nao tem resultado.json."""
    Ultima = {}
    for Entrada in Rodada["celulas"]:
        Chave = (Entrada["conjunto"], Entrada["tarefa"], Entrada["lingua"], Entrada["braco"], Entrada["repeticao"])
        if Chave not in Ultima or Entrada["tentativa"] > Ultima[Chave]["tentativa"]:
            Ultima[Chave] = Entrada
    return [E for E in Ultima.values() if not (Path(E["pasta"]) / "resultado.json").is_file()]


def Refazer(Rodada, Entrada):
    Novo = dict(Entrada, tentativa=Entrada["tentativa"] + 1)
    Rodada["celulas"].append(CriarCelula(Rodada, Novo))
    return Novo["tentativa"]


def ColetarUma(Rodada, Ctx, Entrada):
    Pasta = Path(Entrada["pasta"])
    Resultado = ResultadoDaCelula(LerJson(Pasta / "celula.json"), Ctx, Pasta)
    if not Resultado["valida"]:
        if Entrada["tentativa"] == 1:
            Resultado["refeita_na_tentativa"] = Refazer(Rodada, Entrada)
        else:
            Resultado["PARA"] = True
    GravarJson(Pasta / "resultado.json", Resultado)
    return Resultado


def Coletar(PastaRodada, Filtro=(), Forcar=False):
    """Coleta as celulas pendentes com atividade (ou as do Filtro, mesmo sem, com Forcar). Devolve a lista de resultados."""
    Rodada = LerJson(Path(PastaRodada) / "rodada.json")
    Ctx = NovoContexto(Rodada["forja"]["caminho"])
    Alvos = [E for E in Pendentes(Rodada) if (not Filtro or E["id"] in Filtro) and (Forcar or TemAtividade(Path(E["pasta"])))]
    with ThreadPoolExecutor(max_workers=4) as Pool:
        Resultados = list(Pool.map(partial(ColetarUma, Rodada, Ctx), Alvos))
    GravarJson(Path(PastaRodada) / "rodada.json", Rodada)
    return Resultados
