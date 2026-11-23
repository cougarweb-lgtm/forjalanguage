# -*- coding: utf-8 -*-
"""celula_ia_first.py - monta as celulas do benchmark IA FIRST (P-IA1-a07): tarefas, pastas, celula.json, prompt, toolchains.

Versao 0.1.2
Changelog:
  0.1.2 (11/11/2026) P-IA1-a17: conjuntos P (projeto + perguntas) e R (refatoracoes) entram em CONJUNTOS_DE_PASTA_UNICA e CONJUNTOS_SO_FORJA
        (a celula recebe a pasta inteira de tarefas\\P\\P01 ou tarefas\\R\\Rnn, subpastas incluidas; gabaritos ficam em referencias\\). Nada mais mudou.
  0.1.1 (11/11/2026) P-IA1-a08: celula.json ganha "variante" (pacote, alvo do binario, pasta do variante.py, std) nos pacotes P1 e P2
        (None nos demais); o material cartao_P1/P2 passa a existir.
  0.1.0 (11/11/2026) primeira versao: enumera as tarefas por conjunto e linguagem, cria a pasta da celula (pedido, programa
        inicial, diagnostico_inicial, entrada, material, ia.py, celula.json; NUNCA esperado.json nem gabarito), copia o forja.exe
        da rodada (md5 conferido), gera o texto do prompt fixo e confere as toolchains (gcc, python, node).

Usado por rodar_ia_first.py (preparar, prompt, retomar) e coleta_ia_first.py (celula refeita).
Reversao: apagar a pasta da rodada em %TEMP%\\ia_first\\rodadas\\<id>. Depende de: Python 3.9+ (biblioteca padrao).
"""
import hashlib
import shutil
import subprocess
from pathlib import Path

from ia_comum import RAIZ, GravarJson, LerJson, LIMITE_IDAS

CONJUNTOS_DE_PASTA_UNICA = ("E", "C_forja", "D_forja", "P", "R")
CONJUNTOS_SO_FORJA = ("C_forja", "D_forja", "P", "R")
NAO_COPIAR = ("esperado.json", "gabarito.json", "catalogo.json")
NOMES_DAS_LINGUAGENS = {"forja": "Forja", "c": "C", "python": "Python", "js": "JavaScript"}


def CarregarLinguagens():
    return LerJson(RAIZ / "linguagens.json")


def PastaOrigem(Conj, Id, Lingua):
    Base = RAIZ / "tarefas" / Conj / Id
    return Base if Conj in CONJUNTOS_DE_PASTA_UNICA else Base / Lingua


def LinguaAplica(Conj, Id, Lingua):
    if Conj in CONJUNTOS_SO_FORJA:
        return Lingua == "forja"
    if Conj == "E":
        Esp = LerJson(RAIZ / "tarefas" / "E" / Id / "esperado.json")
        return Lingua in Esp.get("so_linguagens", [Lingua])
    return True


def IdsDoConjunto(Conj, Lingua):
    Raiz = RAIZ / "tarefas" / Conj
    Ids = sorted(P.name for P in Raiz.iterdir() if P.is_dir() and not P.name.startswith("__"))
    return [I for I in Ids if PastaOrigem(Conj, I, Lingua).is_dir() and LinguaAplica(Conj, I, Lingua)]


def Braco(Lingua, Pacote):
    """Braco da celula: o pacote de documentacao na Forja; a propria linguagem nas outras."""
    return Pacote if Lingua == "forja" else Lingua


def NomeDaCelula(Item):
    Nome = "%s_%s_%s_%s_r%d" % (Item["conjunto"], Item["tarefa"], Item["lingua"], Item["braco"], Item["repeticao"])
    return Nome if Item["tentativa"] == 1 else "%s_t%d" % (Nome, Item["tentativa"])


def Md5(Caminho):
    return hashlib.md5(Path(Caminho).read_bytes()).hexdigest()


def CopiarForja(Origem, Destino):
    """Copia fiel do compilador para dentro da rodada; recusa se o md5 da copia difere."""
    Destino = Path(Destino)
    Destino.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(Origem, Destino)
    if Md5(Origem) != Md5(Destino):
        raise OSError("copia do forja.exe difere da origem: " + str(Origem))
    Versao = subprocess.run([str(Destino), "--versao"], capture_output=True, text=True, timeout=20).stdout.strip()
    return {"caminho": str(Destino), "md5": Md5(Destino), "versao": Versao}


def ArquivosDaOrigem(Origem):
    return [P for P in Origem.iterdir() if P.name not in NAO_COPIAR]


def CopiarMaterial(Item, Celula):
    """Material permitido dentro da celula: cartao do pacote na Forja; nada nas outras linguagens."""
    if Item["lingua"] != "forja" or Item["braco"] == "semcartao":
        return []
    Fonte = RAIZ / "material" / "forja" / ("cartao_%s.txt" % Item["braco"])
    if not Fonte.is_file():
        raise FileNotFoundError("material do pacote %s nao existe: %s" % (Item["braco"], Fonte))
    (Celula / "material").mkdir()
    shutil.copyfile(Fonte, Celula / "material" / "cartao.txt")
    return ["material/cartao.txt"]


def DadosDeVariante(Rodada, Item):
    """Pacotes P1/P2 da Forja: o ia.py e o coletar passam o fonte por variante.py (fora da celula: o agente nao le o preprocessador)."""
    if Item["lingua"] != "forja" or Item["braco"] not in ("P1", "P2"):
        return None
    from variante import AlvoDoForja
    return {"pacote": Item["braco"], "alvo": AlvoDoForja(Rodada["forja"]["caminho"]), "pasta": str(RAIZ),
            "std": str(RAIZ.parents[1] / "std")}


def DadosDaCelula(Rodada, Item, Material):
    Cfg = CarregarLinguagens()
    Lin = Cfg["linguagens"][Item["lingua"]]
    Comandos = {Chave: Lin.get(Chave) for Chave in ("check", "build", "run", "test")}
    return {"id": NomeDaCelula(Item), "rodada": Rodada["id"], "modelo": Rodada["modelo"], "conjunto": Item["conjunto"],
            "tarefa": Item["tarefa"], "lingua": Item["lingua"], "braco": Item["braco"], "repeticao": Item["repeticao"],
            "tentativa": Item["tentativa"], "extensao": Lin["extensao"], "forja": Rodada["forja"]["caminho"],
            "limite_idas": LIMITE_IDAS, "limite_segundos": Cfg["limite_segundos"], "limite_build_segundos": Cfg["limite_build_segundos"],
            "saida_max_bytes": Cfg["saida_max_bytes"], "comandos": Comandos, "material": Material, "lock": None,
            "variante": DadosDeVariante(Rodada, Item)}


def CriarCelula(Rodada, Item):
    """Cria a pasta da celula (falha se ela ja existe) e devolve o registro para o rodada.json."""
    Celula = Path(Rodada["pasta"]) / "celulas" / NomeDaCelula(Item)
    Celula.mkdir(parents=True)
    Origem = PastaOrigem(Item["conjunto"], Item["tarefa"], Item["lingua"])
    for Arquivo in ArquivosDaOrigem(Origem):
        Alvo = Celula / Arquivo.name
        if Arquivo.is_dir():
            shutil.copytree(Arquivo, Alvo)
        else:
            shutil.copyfile(Arquivo, Alvo)
    shutil.copyfile(RAIZ / "ia.py", Celula / "ia.py")
    GravarJson(Celula / "celula.json", DadosDaCelula(Rodada, Item, CopiarMaterial(Item, Celula)))
    return dict(Item, id=NomeDaCelula(Item), pasta=str(Celula))


def TextoDoPrompt(Rodada, Entrada):
    """Prompt FIXO do spawn (contrato 4.3): so troca a linguagem e o material. As regras vivem em bench_ia.md."""
    Cel = LerJson(Path(Entrada["pasta"]) / "celula.json")
    Pasta = Entrada["pasta"].replace("\\", "/")
    Material = ", ".join(Cel["material"]) or "nenhum"
    return ("Celula: %s\nLinguagem: %s (programa em prog%s)\nMaterial permitido: %s\n"
            "Comando: python \"%s/ia.py\" run|check|build|test prog%s [-- argumentos do programa]\nBatimento: %s/batimento.log\n"
            % (Pasta, NOMES_DAS_LINGUAGENS[Cel["lingua"]], Cel["extensao"], Material, Pasta, Cel["extensao"], Pasta))


def VersaoDe(Comando):
    try:
        Saida = subprocess.run(Comando, capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return (Saida.stdout or Saida.stderr).strip().splitlines()[0] if Saida.returncode == 0 and (Saida.stdout or Saida.stderr).strip() else None


def Toolchains(Forja):
    """Por linguagem: {disponivel, versao, motivo}. Opcional ausente = pulada com motivo (nunca vermelha); nada e instalado."""
    Cfg = CarregarLinguagens()["linguagens"]
    Resultado = {}
    for Nome, Lin in Cfg.items():
        Comando = [str(Forja) if C == "{forja}" else C for C in Lin["versao_comando"]]
        Achou = Comando[0] == str(Forja) or shutil.which(Comando[0]) is not None
        Versao = VersaoDe(Comando) if Achou else None
        Pulo = None if Versao else ("ferramenta ausente (%s)" % Comando[0])
        Resultado[Nome] = {"disponivel": bool(Versao), "obrigatoria": Lin["obrigatoria"], "versao": Versao, "motivo": Pulo}
    return Resultado
