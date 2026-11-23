# -*- coding: utf-8 -*-
"""verificar_migrar.py -- IA1-L3d1: o 'forja migrar' troca as formas antigas da tabela de renomeados pelas novas, sem regex, idempotente e sem mexer fora da linha da troca.

Cada fixture de fixtures_migrar\\ tem a forma antiga; a que muda tem o esperado byte a byte em <nome>.novo.fj (sem .novo.fj = o arquivo nao pode mudar). O verificador roda numa pasta
temporaria com uma juncao `vendor` para language\\vendor (removida no fim, sem tocar no destino) e confere, por fixture:
  - dry-run (padrao): JSON com estado, quantidade e classes das trocas certos; cada troca aponta o texto antigo na linha/coluna dita (conferido contra o arquivo, em BYTES); NADA e gravado;
  - --aplicar: o arquivo fica igual ao .novo.fj; mesmo numero de linhas e toda linha sem troca igual byte a byte (conferencia propria, independente do forja); `forja check` limpo;
  - 2a passada: 0 trocas (idempotente); o estado revisao e o nao_analisado nao mudam o arquivo;
  - CRLF e BOM: o mesmo arquivo com CRLF (e com BOM) sai igual ao esperado com CRLF (e BOM);
  - pasta: recursiva, em ordem de nome, ignora language_*, .git* e backup*; varias entradas; --aplicar so mexe nos listados;
  - --comentarios (L3d2): comentario que parseia como Forja (linha, depois de codigo, bloco de linhas seguidas) tem a forma antiga trocada na mesma linha e coluna (em BYTES); prosa
    e o que nao parseia NAO e editado e vai para comentarios_revisao (estado revisao_comentario); sem a opcao os comentarios ficam intactos; --aplicar igual ao esperado, linhas
    iguais, check limpo, 2a passada com 0 trocas, CRLF e BOM;
  - uso: sem argumento, opcao desconhecida (--prova ainda nao existe), --dry-run com --aplicar e entrada inexistente saem com exit 2.
Classes cobertas: rotulo, valor-alocador, modulo, qualificador, pacote, proc, tipo, membro, param (as 9 da tabela) e argumento-novo (B01d: insercao de ", alloc = heap", coluna_fim == coluna, antigo "").
Uso:
    python -I verificar_migrar.py [--forja <forja.exe>] [--selftest]
Exit 0 se tudo passa, 1 se algum caso falha, 2 se o forja nao existe. Contra o compilador anterior ao L3d1 (%TEMP%\\forja_L3c3_final\\forja.exe, sem `migrar`) fica VERMELHO.
Depende de: so a biblioteca padrao (cmd mklink /J, do Windows).
Versao: 0.5.0
Changelog: 0.5.0 - S3 P4 (L11b-2, D10): casos libm (LIBM_CASOS, CasosLibm): foreign de libm vira math.<fn> (f64, f32 por alias, bloco misto, sqrtf sem import, import ja existente);
           tipo, nome de parametro e literal f32 nao migram (motivo no JSON); saida do programa igual antes e depois; 2a passada 0.
           0.4.0 - B01d: classe argumento-novo (insercao de alloc = heap em chamada sem alloc); fixtures alloc_chamadas (tambem em CRLF e BOM) e alloc_blocos.
           0.3.2 - S2g-json: vendor_json migra tambem indice, valor, texto e caminho (139 trocas; o fixture ganhou o membro invalid_path).
           0.3.1 - IA1-L8: o net.fj perdeu a divida dos 2 E0709 (`_ = `); vendor_rede_com_divida virou vendor_rede e migra (ok, 10 trocas).
           0.3.0 - IA1-L3d2: --comentarios (fixtures comentario_codigo e comentario_prosa; casos dry-run, sem a opcao, aplicar, CRLF, BOM e prosa). --prova segue recusada (exit 2).
           0.2.0 - IA1-L3d1: fixture modulo_sem_main (modulo de biblioteca, sem proc main: migra e o check dele so reclama de E0201).
           0.1.0 - IA1-L3d1: primeira versao.
Verificacao: python -I verificar_migrar.py --selftest   |   python -I verificar_migrar.py --forja <forja.exe>
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
FIXTURES = AQUI / "fixtures_migrar"
VENDOR = AQUI.parent.parent / "vendor"
FORJA_PADRAO = AQUI.parent.parent / "compiler" / "forja.exe"
TIMEOUT = 120
BOM = b"\xef\xbb\xbf"
CLASSES_DA_TABELA = {"rotulo", "valor-alocador", "modulo", "qualificador", "pacote", "proc", "tipo", "membro", "param", "argumento-novo"}
# nome -> (estado, quantidade de trocas, classes). Sem .novo.fj o arquivo nao pode mudar.
CASOS = {
    "rotulo_alloc": ("ok", 5, {"rotulo"}),
    "valor_scope": ("ok", 8, {"rotulo", "valor-alocador"}),
    "rotulo_to": ("ok", 2, {"rotulo"}),
    "misto": ("ok", 3, {"rotulo", "valor-alocador"}),
    "modulo_file": ("ok", 15, {"modulo", "qualificador", "argumento-novo"}),  # B01e2: read_all ganhou alloc
    "modulo_file_as": ("ok", 2, {"modulo", "argumento-novo"}),  # B01e2: read_all ganhou alloc
    "colisao_usuario": ("sem_trocas", 0, set()),
    "colisao_qualificador": ("revisao", 2, {"rotulo", "argumento-novo"}),  # B01e2: read_all ganhou alloc
    "erro_com_forma_antiga": ("nao_analisado", 0, set()),
    "modulo_sem_main": ("ok", 1, {"rotulo"}),
    "vendor_banco":("ok", 23, {"modulo", "qualificador", "tipo", "proc"}),
    "vendor_banco_min": ("ok", 3, {"modulo", "qualificador", "proc"}),
    "vendor_compressao": ("ok", 34, {"modulo", "qualificador", "tipo", "proc", "param", "membro"}),
    "vendor_json": ("ok", 139, {"tipo", "membro", "proc", "param"}),
    "vendor_pacote_pasta": ("ok", 1, {"pacote"}),
    "vendor_pacote_physics2d": ("ok", 1, {"pacote"}),
    "vendor_pacote_servidor": ("ok", 2, {"pacote"}),
    "vendor_rede": ("ok", 10, {"membro", "modulo", "qualificador", "tipo"}),
    "alloc_chamadas": ("ok", 10, {"argumento-novo"}),  # B01d: 0 args, 1 e varios args, aninhada, virgula final, literal multilinha; com alloc e sem Allocator ficam
    "alloc_blocos": ("ok", 9, {"argumento-novo"}),  # B01d: defer, switch, or_else e dentro de proc que ja tem alloc (alloc = alloc fica)
}
SEM_MAIN = {"modulo_sem_main"}  # modulos de biblioteca: o check deles sai E0201
DIAGNOSTICOS = {"erro_com_forma_antiga": "E0301"}
CHAVES_ARQUIVO = ["arquivo", "trocas", "detalhes", "estado", "diagnostico", "motivo"]
CHAVES_DETALHE = ["linha", "coluna", "coluna_fim", "classe", "antigo", "novo"]
CHAVES_DOCUMENTO = ["arquivos", "total_trocas", "arquivos_alterados", "linhas_iguais"]
# --comentarios (L3d2): as formas antigas que os comentarios que NAO parseiam (ou o qualificador duvidoso) deixam para a revisao a mao, na ordem do arquivo
FORMAS_DA_REVISAO = [["em"], ["para", "em"], ["arquivo.tamanho"]]
FORMAS_DA_PROSA = [["para"], ["em", "para"], ["std:arquivo", "vendor:banco/banco"], ["para"]]


def Rodar(Forja, Argumentos, Pasta):
    """Roda o forja e devolve (exit, stdout, stderr)."""
    Rodada = subprocess.run([str(Forja)] + Argumentos, capture_output=True, cwd=str(Pasta), timeout=TIMEOUT)
    return Rodada.returncode, Rodada.stdout.decode("utf-8", "replace"), Rodada.stderr.decode("utf-8", "replace")


def Ligar(Origem, Destino):
    """Juncao Destino -> Origem (cmd mklink /J, sem administrador)."""
    if not Destino.exists():
        subprocess.run(["cmd", "/c", "mklink", "/J", str(Destino), str(Origem)], capture_output=True, check=True)


def Desligar(Destino):
    """Remove a juncao sem tocar no destino (rmdir em juncao so tira o link)."""
    if Destino.exists():
        subprocess.run(["cmd", "/c", "rmdir", str(Destino)], capture_output=True)


def Documento(Saida):
    """O documento JSON do stdout; None se nao e um JSON."""
    try:
        return json.loads(Saida)
    except ValueError:
        return None


def ProblemasDeLinhas(Antes, Depois, Trocas):
    """Mesmo numero de linhas e toda linha fora de uma troca igual byte a byte (a linha da troca so muda por dentro)."""
    LinhasAntes, LinhasDepois = Antes.split(b"\n"), Depois.split(b"\n")
    if len(LinhasAntes) != len(LinhasDepois):
        return [f"numero de linhas mudou: {len(LinhasAntes)} -> {len(LinhasDepois)}"]
    ComTroca = {T["linha"] for T in Trocas}
    return [f"linha {N} mudou sem troca" for N, (A, D) in enumerate(zip(LinhasAntes, LinhasDepois), 1) if N not in ComTroca and A != D]


def ProblemasDePosicao(Original, Trocas):
    """Cada troca aponta, em BYTES, o texto antigo dito (linha/coluna a partir de 1, coluna_fim exclusiva, sem contar o \\r)."""
    Linhas = Original.split(b"\n")
    Problemas = []
    for T in Trocas:
        Texto = Linhas[T["linha"] - 1].rstrip(b"\r")[T["coluna"] - 1:T["coluna_fim"] - 1]
        if Texto != T["antigo"].encode("utf-8"):
            Problemas.append(f"linha {T['linha']} coluna {T['coluna']}: no arquivo {Texto!r}, dito {T['antigo']!r}")
    return Problemas


def ProblemasDeFormato(Doc):
    """O JSON tem as chaves do contrato, na ordem."""
    if Doc is None:
        return ["stdout nao e um documento JSON"]
    Problemas = [] if list(Doc) == CHAVES_DOCUMENTO else [f"chaves do documento {list(Doc)}"]
    for Arq in Doc.get("arquivos", []):
        if list(Arq) != CHAVES_ARQUIVO:
            Problemas.append(f"chaves do arquivo {list(Arq)}")
        Problemas += [f"chaves do detalhe {list(D)}" for D in Arq.get("detalhes", []) if list(D) != CHAVES_DETALHE]
    return Problemas


def ProblemasDoDryRun(Forja, Pasta, Nome):
    """dry-run: estado, quantidade, classes e posicoes certos, exit certo e nada gravado. Devolve (problemas, trocas)."""
    Estado, Quantidade, Classes = CASOS[Nome]
    Arquivo = Pasta / f"{Nome}.fj"
    Antes = Arquivo.read_bytes()
    Codigo, Saida, _ = Rodar(Forja, ["migrar", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    Problemas = ProblemasDeFormato(Doc)
    if Problemas:
        return Problemas, []
    Item = Doc["arquivos"][0]
    if Item["estado"] != Estado or Item["trocas"] != Quantidade or {D["classe"] for D in Item["detalhes"]} != Classes:
        Problemas.append(f"estado {Item['estado']} trocas {Item['trocas']} classes {sorted({D['classe'] for D in Item['detalhes']})}, esperado {Estado} {Quantidade} {sorted(Classes)}")
    if Item["diagnostico"] != DIAGNOSTICOS.get(Nome) and Estado == "nao_analisado":
        Problemas.append(f"diagnostico {Item['diagnostico']}, esperado {DIAGNOSTICOS.get(Nome)}")
    if Codigo != (1 if Estado == "nao_analisado" else 0):
        Problemas.append(f"exit {Codigo}")
    if Estado == "revisao" and not Item["motivo"]:
        Problemas.append("revisao sem motivo")
    if Doc["arquivos_alterados"] != 0 or Arquivo.read_bytes() != Antes:
        Problemas.append("o dry-run gravou")
    return Problemas + ProblemasDePosicao(Antes, Item["detalhes"]), Item["detalhes"]


def Esperado(Nome, Original):
    """O arquivo depois do migrar: o .novo.fj, ou o proprio original se nao pode mudar."""
    Novo = FIXTURES / f"{Nome}.novo.fj"
    return Novo.read_bytes() if Novo.is_file() else Original


def ProblemasDoAplicar(Forja, Pasta, Nome, Trocas):
    """--aplicar: arquivo igual ao esperado, linhas iguais, check limpo (nos ok) e a 2a passada sem trocas."""
    Arquivo = Pasta / f"{Nome}.fj"
    Original = Arquivo.read_bytes()
    Problemas = []
    Codigo, Saida, _ = Rodar(Forja, ["migrar", "--aplicar", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    if Doc is None:
        return ["--aplicar: stdout nao e JSON"]
    Depois = Arquivo.read_bytes()
    if Depois != Esperado(Nome, Original):
        Problemas.append("o arquivo migrado difere do esperado")
    Problemas += ProblemasDeLinhas(Original, Depois, Trocas)
    if Doc["arquivos_alterados"] != (1 if Depois != Original else 0) or not Doc["linhas_iguais"]:
        Problemas.append(f"arquivos_alterados {Doc['arquivos_alterados']} linhas_iguais {Doc['linhas_iguais']}")
    Check = Rodar(Forja, ["check", Arquivo.name], Pasta)
    if Nome in SEM_MAIN:
        if '"codigo": "E0201"' not in Check[1] + Check[2]:
            Problemas.append("o check do modulo migrado deveria reclamar so do main (E0201)")
    elif CASOS[Nome][0] == "revisao":
        # o que ficou para revisao continua na forma antiga, que o compilador recusa (L3f): E0110 e o unico diagnostico
        if Check[0] != 1 or '"codigo": "E0110"' not in Check[1] or Check[1].count('"codigo"') != Check[1].count('"codigo": "E0110"'):
            Problemas.append("o check do arquivo com revisao deveria acusar so E0110 (a forma antiga que ficou)")
    elif CASOS[Nome][0] in ("ok", "sem_trocas") and Check[0] != 0:
        Problemas.append("o check do arquivo migrado nao sai 0")
    _, Saida2, _ = Rodar(Forja, ["migrar", Arquivo.name], Pasta)
    Doc2 = Documento(Saida2)
    if Doc2 is None or Doc2["total_trocas"] != 0:
        Problemas.append("2a passada com trocas (nao e idempotente)")
    return Problemas


def PreparaPasta(Pasta, Nomes):
    """Copia as fixtures (so as de entrada) para a pasta e liga o vendor."""
    for Nome in Nomes:
        shutil.copyfile(FIXTURES / f"{Nome}.fj", Pasta / f"{Nome}.fj")
    Ligar(VENDOR, Pasta / "vendor")


def CasosPorFixture(Forja, Raiz):
    """[(rotulo, problemas)] do dry-run e do aplicar de cada fixture."""
    Resultados = []
    PreparaPasta(Raiz, CASOS)
    for Nome in CASOS:
        Problemas, Trocas = ProblemasDoDryRun(Forja, Raiz, Nome)
        Resultados.append((f"dry_run_{Nome}", Problemas))
        Resultados.append((f"aplicar_{Nome}", ProblemasDoAplicar(Forja, Raiz, Nome, Trocas)))
    return Resultados


def ProblemasDeCodificacao(Forja, Raiz, Nome, Variante):
    """CRLF ou BOM: o arquivo com a mesma troca sai igual ao esperado nessa codificacao."""
    Original = (FIXTURES / f"{Nome}.fj").read_bytes()
    Alvo = Esperado(Nome, Original)
    if Variante == "crlf":
        Original, Alvo = Original.replace(b"\n", b"\r\n"), Alvo.replace(b"\n", b"\r\n")
    else:
        Original, Alvo = BOM + Original, BOM + Alvo
    Pasta = Raiz / f"cod_{Variante}_{Nome}"
    Pasta.mkdir()
    Ligar(VENDOR, Pasta / "vendor")
    (Pasta / f"{Nome}.fj").write_bytes(Original)
    Rodar(Forja, ["migrar", "--aplicar", f"{Nome}.fj"], Pasta)
    Depois = (Pasta / f"{Nome}.fj").read_bytes()
    Desligar(Pasta / "vendor")
    return [] if Depois == Alvo else [f"{Variante}: o arquivo migrado difere do esperado nessa codificacao"]


def CasosDeCodificacao(Forja, Raiz):
    Nomes = ("modulo_file", "valor_scope", "vendor_banco_min", "alloc_chamadas")
    return [(f"{V}_{N}", ProblemasDeCodificacao(Forja, Raiz, N, V)) for V in ("crlf", "bom") for N in Nomes]


def MontarArvore(Raiz):
    """Raiz\\a.fj, Raiz\\sub\\b.fj e, nas pastas ignoradas, c.fj com forma antiga que nao pode mudar."""
    Antigo = (FIXTURES / "rotulo_alloc.fj").read_bytes()
    Outro = (FIXTURES / "rotulo_to.fj").read_bytes()
    Arvore = {"a.fj": Antigo, "sub/b.fj": Outro, "sub/leia.txt": b"nada", "language_m4/c.fj": Antigo, ".git/d.fj": Antigo, "backup_velho/e.fj": Antigo, "Backup/f.fj": Antigo}
    for Caminho, Dados in Arvore.items():
        Destino = Raiz / Caminho
        Destino.parent.mkdir(parents=True, exist_ok=True)
        Destino.write_bytes(Dados)
    return Arvore


def CasoPasta(Forja, Raiz):
    """Pasta recursiva em ordem, ignorando as pastas language_*, .git* e backup*; --aplicar so mexe nos listados; varias entradas."""
    Pasta = Raiz / "arvore"
    Pasta.mkdir()
    Arvore = MontarArvore(Pasta)
    _, Saida, _ = Rodar(Forja, ["migrar", "arvore"], Raiz)
    Doc = Documento(Saida)
    Listados = [A["arquivo"] for A in Doc["arquivos"]] if Doc else None
    Problemas = [] if Listados == ["arvore/a.fj", "arvore/sub/b.fj"] else [f"arquivos listados {Listados}"]
    Rodar(Forja, ["migrar", "--aplicar", "arvore"], Raiz)
    for Caminho, Dados in Arvore.items():
        Mudou = (Pasta / Caminho).read_bytes() != Dados
        if Mudou != (Caminho in ("a.fj", "sub/b.fj")):
            Problemas.append(f"{Caminho}: mudou={Mudou}")
    _, Saida2, _ = Rodar(Forja, ["migrar", "arvore/a.fj", "arvore/sub/b.fj"], Raiz)
    Doc2 = Documento(Saida2)
    if Doc2 is None or Doc2["total_trocas"] != 0 or len(Doc2["arquivos"]) != 2:
        Problemas.append("2 entradas: a 2a passada nao achou 0 trocas em 2 arquivos")
    return Problemas


def CasoUso(Forja, Raiz):
    """Uso invalido = exit 2, mensagem no stderr e stdout vazio."""
    (Raiz / "u.fj").write_bytes((FIXTURES / "rotulo_to.fj").read_bytes())
    Entradas = [[], ["--prova", "x.exe", "u.fj"], ["--dry-run", "--aplicar", "u.fj"], ["--foo", "u.fj"], ["nao_existe.fj"], ["--json"]]
    Problemas = []
    for Argumentos in Entradas:
        Codigo, Saida, Erro = Rodar(Forja, ["migrar"] + Argumentos, Raiz)
        if Codigo != 2 or Saida.strip() or "migrar" not in Erro:
            Problemas.append(f"migrar {' '.join(Argumentos)}: exit {Codigo}, stdout {Saida[:40]!r}")
    return Problemas


def Instalar(Raiz, Nome, Dados=None):
    """Copia a fixture (ou grava 'Dados') em Raiz\\Nome.fj e devolve o caminho."""
    Destino = Raiz / f"{Nome}.fj"
    Destino.write_bytes((FIXTURES / f"{Nome}.fj").read_bytes() if Dados is None else Dados)
    return Destino


def FormasDasRevisoes(Item):
    return [R["formas_antigas_encontradas"] for R in Item.get("comentarios_revisao", [])]


def ProblemasDoDryRunComentarios(Forja, Pasta):
    """--comentarios em dry-run: trocas de codigo e de comentario, posicoes em BYTES, revisoes e nada gravado."""
    Arquivo = Instalar(Pasta, "comentario_codigo")
    Antes = Arquivo.read_bytes()
    _, Saida, _ = Rodar(Forja, ["migrar", "--comentarios", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    if Doc is None or list(Doc) != CHAVES_DOCUMENTO + ["comentarios_reescritos", "comentarios_revisao"]:
        return [f"chaves do documento {list(Doc) if Doc else Saida[:60]!r}"]
    Item = Doc["arquivos"][0]
    Problemas = []
    if list(Item) != CHAVES_ARQUIVO + ["comentarios_revisao"]:
        Problemas.append(f"chaves do arquivo {list(Item)}")
    Comentarios = [D for D in Item["detalhes"] if D.get("origem") == "comentario"]
    if (Item["estado"], Item["trocas"], len(Comentarios), Doc["comentarios_reescritos"], Doc["comentarios_revisao"]) != ("ok", 17, 14, 14, 3):
        Problemas.append(f"estado {Item['estado']} trocas {Item['trocas']} comentarios {len(Comentarios)}/{Doc['comentarios_reescritos']} revisao {Doc['comentarios_revisao']}")
    if FormasDasRevisoes(Item) != FORMAS_DA_REVISAO:
        Problemas.append(f"formas da revisao {FormasDasRevisoes(Item)}")
    Linhas = Antes.split(b"\n")
    for R in Item["comentarios_revisao"]:
        if R["trecho"].encode("utf-8") not in Linhas[R["linha"] - 1] or R["estado"] != "revisao_comentario":
            Problemas.append(f"revisao fora do lugar: {R}")
    if Doc["arquivos_alterados"] != 0 or Arquivo.read_bytes() != Antes:
        Problemas.append("o dry-run gravou")
    return Problemas + ProblemasDePosicao(Antes, Item["detalhes"])


def ProblemasSemComentarios(Forja, Pasta):
    """Sem --comentarios o comportamento nao muda: so as 3 trocas de codigo, comentarios intactos, nenhum campo de comentario no JSON."""
    Arquivo = Instalar(Pasta, "comentario_codigo")
    Antes = Arquivo.read_bytes()
    _, Saida, _ = Rodar(Forja, ["migrar", "--aplicar", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    Problemas = ProblemasDeFormato(Doc)
    if Doc is not None and Doc["total_trocas"] != 3:
        Problemas.append(f"total_trocas {Doc['total_trocas']}, esperado 3")
    Depois = Arquivo.read_bytes()
    Mudou = [N for N, (A, D) in enumerate(zip(Antes.split(b"\n"), Depois.split(b"\n")), 1) if A != D and b"//" in A and A.lstrip().startswith(b"//")]
    if Mudou:
        Problemas.append(f"comentarios de linha inteira mudaram sem --comentarios: {Mudou}")
    return Problemas


def ProblemasDoAplicarComentarios(Forja, Pasta):
    """--comentarios --aplicar: igual ao .novo.fj, linhas iguais, check limpo, 2a passada com 0 trocas e as mesmas revisoes."""
    Arquivo = Instalar(Pasta, "comentario_codigo")
    Original = Arquivo.read_bytes()
    _, Saida, _ = Rodar(Forja, ["migrar", "--comentarios", "--aplicar", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    if Doc is None:
        return ["stdout nao e JSON"]
    Depois = Arquivo.read_bytes()
    Problemas = [] if Depois == Esperado("comentario_codigo", Original) else ["o arquivo migrado difere do esperado"]
    Problemas += ProblemasDeLinhas(Original, Depois, Doc["arquivos"][0]["detalhes"])
    if Doc["arquivos_alterados"] != 1 or not Doc["linhas_iguais"]:
        Problemas.append(f"arquivos_alterados {Doc['arquivos_alterados']} linhas_iguais {Doc['linhas_iguais']}")
    if Rodar(Forja, ["check", Arquivo.name], Pasta)[0] != 0:
        Problemas.append("o check do arquivo migrado nao sai 0")
    _, Saida2, _ = Rodar(Forja, ["migrar", "--comentarios", Arquivo.name], Pasta)
    Doc2 = Documento(Saida2)
    if Doc2 is None or Doc2["total_trocas"] != 0 or Doc2["comentarios_reescritos"] != 0:
        Problemas.append("2a passada com trocas (nao e idempotente)")
    elif FormasDasRevisoes(Doc2["arquivos"][0]) != FORMAS_DA_REVISAO or Doc2["arquivos"][0]["estado"] != "revisao_comentario":
        Problemas.append(f"2a passada: revisoes {FormasDasRevisoes(Doc2['arquivos'][0])}, estado {Doc2['arquivos'][0]['estado']}")
    return Problemas


def ProblemasDeCodificacaoComentarios(Forja, Raiz, Variante):
    """CRLF ou BOM: o arquivo com comentarios sai igual ao esperado nessa codificacao."""
    Original = (FIXTURES / "comentario_codigo.fj").read_bytes()
    Alvo = (FIXTURES / "comentario_codigo.novo.fj").read_bytes()
    if Variante == "crlf":
        Original, Alvo = Original.replace(b"\n", b"\r\n"), Alvo.replace(b"\n", b"\r\n")
    else:
        Original, Alvo = BOM + Original, BOM + Alvo
    Pasta = Raiz / f"cm_{Variante}"
    Pasta.mkdir()
    Arquivo = Instalar(Pasta, "comentario_codigo", Original)
    _, Saida, _ = Rodar(Forja, ["migrar", "--comentarios", "--aplicar", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    Problemas = [] if Arquivo.read_bytes() == Alvo else [f"{Variante}: o arquivo migrado difere do esperado nessa codificacao"]
    if Doc is not None and (Doc["total_trocas"] != 17 or not Doc["linhas_iguais"]):
        Problemas.append(f"{Variante}: total_trocas {Doc['total_trocas']} linhas_iguais {Doc['linhas_iguais']}")
    return Problemas


def ProblemasDaProsa(Forja, Pasta):
    """So prosa: nada e editado (nem com --aplicar), tudo vai para revisao_comentario e o estado e revisao_comentario; sem --comentarios e sem_trocas."""
    Arquivo = Instalar(Pasta, "comentario_prosa")
    Antes = Arquivo.read_bytes()
    Codigo, Saida, _ = Rodar(Forja, ["migrar", "--comentarios", "--aplicar", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    if Doc is None:
        return ["stdout nao e JSON"]
    Item = Doc["arquivos"][0]
    Problemas = []
    if (Item["estado"], Item["trocas"], Doc["arquivos_alterados"], Codigo) != ("revisao_comentario", 0, 0, 0) or Arquivo.read_bytes() != Antes:
        Problemas.append(f"estado {Item['estado']} trocas {Item['trocas']} alterados {Doc['arquivos_alterados']} exit {Codigo}")
    if FormasDasRevisoes(Item) != FORMAS_DA_PROSA:
        Problemas.append(f"formas {FormasDasRevisoes(Item)}")
    Sem = Documento(Rodar(Forja, ["migrar", Arquivo.name], Pasta)[1])
    if Sem is None or Sem["arquivos"][0]["estado"] != "sem_trocas":
        Problemas.append("sem --comentarios o estado deveria ser sem_trocas")
    return Problemas


def CasosDeComentario(Forja, Raiz):
    """[(rotulo, problemas)] dos casos de --comentarios, cada um numa subpasta."""
    Casos = [("dry_run", ProblemasDoDryRunComentarios), ("sem_a_opcao", ProblemasSemComentarios), ("aplicar", ProblemasDoAplicarComentarios), ("prosa", ProblemasDaProsa)]
    Resultados = []
    for Rotulo, Funcao in Casos:
        Pasta = Raiz / f"cm_{Rotulo}"
        Pasta.mkdir()
        Resultados.append((f"comentarios_{Rotulo}", Funcao(Forja, Pasta)))
    Resultados += [(f"comentarios_{V}", ProblemasDeCodificacaoComentarios(Forja, Raiz, V)) for V in ("crlf", "bom")]
    return Resultados


# S3 P4 (D10): foreign de libm -> std:math. Fontes e saidas esperadas ficam aqui (sem .novo.fj): o caso confere o dry-run, o --aplicar, o check,
# a 2a passada e, quando migra, que o programa imprime o mesmo antes e depois. estado/trocas/classes/motivo sao o contrato do JSON.
def LinhasDoCaso(Linhas):
    return "\n".join(Linhas) + "\n"


LIBM_CASOS = {
    "f64": {
        "fonte": LinhasDoCaso(['foreign {', '    link "m"', '    proc c_sqrt = "sqrt"(x: f64) -> f64', '    proc pow(x: f64, y: f64) -> f64', '    proc floor(x: f64) -> f64',
                               '    proc sin(x: f64) -> f64', '}', '', 'proc main() {', '    print(c_sqrt(144.0) as int)', '    print(pow(2.0, y = 10.0) as int)',
                               '    print(floor(-2.5) as int)', '    print((sin(0.0) * 1000.0) as int)', '}']),
        "esperado": LinhasDoCaso(['import "std:math" as math', 'foreign {', '    link "m"', '}', '', 'proc main() {', '    print(sqrt(144.0) as int)',
                                  '    print(math.pow(2.0, y = 10.0) as int)', '    print(math.floor(-2.5) as int)', '    print((math.sin(0.0) * 1000.0) as int)', '}']),
        "estado": "ok", "trocas": 9, "classes": {"libm", "libm-linha"}, "motivo": None, "roda": True,
    },
    "f32_alias": {
        "fonte": LinhasDoCaso(['foreign {', '    link "msvcrt"', '    proc mat_sinf = "sinf"(x: f32) -> f32', '    proc mat_cosf = "cosf"(x: f32) -> f32', '}', '',
                               'proc main() {', '    let a: f32 = 1.0', '    print((mat_sinf(a) * 1000.0) as int)', '    print((mat_cosf(a) * 1000.0) as int)', '}']),
        "esperado": LinhasDoCaso(['import "std:math" as math', 'foreign {', '    link "msvcrt"', '}', '', 'proc main() {', '    let a: f32 = 1.0',
                                  '    print((math.sin(a) * 1000.0) as int)', '    print((math.cos(a) * 1000.0) as int)', '}']),
        "estado": "ok", "trocas": 5, "classes": {"libm", "libm-linha"}, "motivo": None, "roda": True,
    },
    "bloco_misto": {
        "fonte": LinhasDoCaso(['foreign {', '    link "m"', '    proc sin(x: f64) -> f64', '    proc minha_funcao(x: f64) -> f64', '}', '', 'proc main() {',
                               '    print((sin(0.0) * 1000.0) as int)', '}']),
        "esperado": LinhasDoCaso(['import "std:math" as math', 'foreign {', '    link "m"', '    proc minha_funcao(x: f64) -> f64', '}', '', 'proc main() {',
                                  '    print((math.sin(0.0) * 1000.0) as int)', '}']),
        "estado": "ok", "trocas": 3, "classes": {"libm", "libm-linha"}, "motivo": None, "roda": True,
    },
    "sqrtf_sem_import": {
        "fonte": LinhasDoCaso(['foreign {', '    link "msvcrt"', '    proc c_sqrt = "sqrtf"(x: f32) -> f32', '}', '', 'proc main() {', '    let r: f32 = 16.0',
                               '    print(c_sqrt(r) as int)', '}']),
        "esperado": LinhasDoCaso(['foreign {', '    link "msvcrt"', '}', '', 'proc main() {', '    let r: f32 = 16.0', '    print(sqrt(r) as int)', '}']),
        "estado": "ok", "trocas": 2, "classes": {"libm", "libm-linha"}, "motivo": None, "roda": True,
    },
    "import_ja_existe": {
        "fonte": LinhasDoCaso(['import "std:math" as math', '', 'foreign {', '    link "m"', '    proc sin(x: f64) -> f64', '}', '', 'proc main() {',
                               '    print((sin(0.0) * 1000.0) as int)', '    print((math.cos(0.0) * 1000.0) as int)', '}']),
        "esperado": LinhasDoCaso(['import "std:math" as math', '', 'foreign {', '    link "m"', '}', '', 'proc main() {',
                                  '    print((math.sin(0.0) * 1000.0) as int)', '    print((math.cos(0.0) * 1000.0) as int)', '}']),
        "estado": "ok", "trocas": 2, "classes": {"libm", "libm-linha"}, "motivo": None, "roda": True,
    },
    "tipo_nao_bate": {
        "fonte": LinhasDoCaso(['foreign {', '    link "msvcrt"', '    proc sinf(x: f64) -> f64', '}', '', 'proc main() {', '    print(sinf(1.0) as int)', '}']),
        "esperado": None, "estado": "sem_trocas", "trocas": 0, "classes": set(), "motivo": "o tipo nao bate", "roda": False,
    },
    "nome_diferente": {
        "fonte": LinhasDoCaso(['foreign {', '    link "m"', '    proc atan2(a: f64, b: f64) -> f64', '}', '', 'proc main() {',
                               '    print(atan2(1.0, b = 2.0) as int)', '}']),
        "esperado": None, "estado": "sem_trocas", "trocas": 0, "classes": set(), "motivo": "os nomes dos parametros diferem", "roda": False,
    },
    "literal_f32": {
        "fonte": LinhasDoCaso(['foreign {', '    link "msvcrt"', '    proc mat_sinf = "sinf"(x: f32) -> f32', '}', '', 'proc main() {',
                               '    let r: f32 = mat_sinf(1.0)', '    print((r * 1000.0) as int)', '}']),
        "esperado": None, "estado": "revisao", "trocas": 0, "classes": set(), "motivo": "a migracao da libm fica como esta", "roda": False,
    },
}


def ProblemasDoCasoLibm(Forja, Pasta, Nome, Caso):
    """Um caso de libm: dry-run (JSON, nada gravado), --aplicar (igual ao esperado), check, saida do programa igual e 2a passada sem trocas."""
    Arquivo = Pasta / f"{Nome}.fj"
    Arquivo.write_bytes(Caso["fonte"].encode("utf-8"))
    Antes = Arquivo.read_bytes()
    Codigo, Saida, _ = Rodar(Forja, ["migrar", Arquivo.name], Pasta)
    Doc = Documento(Saida)
    Problemas = ProblemasDeFormato(Doc)
    if Problemas:
        return Problemas
    Item = Doc["arquivos"][0]
    if Codigo != 0:
        Problemas.append(f"dry-run exit {Codigo}")
    if Item["estado"] != Caso["estado"] or Item["trocas"] != Caso["trocas"] or {D["classe"] for D in Item["detalhes"]} != Caso["classes"]:
        Problemas.append(f"estado {Item['estado']} trocas {Item['trocas']} classes {sorted({D['classe'] for D in Item['detalhes']})}, esperado {Caso['estado']} {Caso['trocas']} {sorted(Caso['classes'])}")
    if Caso["motivo"] is not None and Caso["motivo"] not in (Item["motivo"] or ""):
        Problemas.append(f"motivo {Item['motivo']!r} nao traz {Caso['motivo']!r}")
    if Arquivo.read_bytes() != Antes or Doc["arquivos_alterados"] != 0:
        Problemas.append("o dry-run gravou")
    Rodar(Forja, ["migrar", "--aplicar", Arquivo.name], Pasta)
    Depois = Arquivo.read_bytes()
    if Depois != (Caso["esperado"] or Caso["fonte"]).encode("utf-8"):
        Problemas.append("o arquivo migrado difere do esperado")
    if Rodar(Forja, ["check", Arquivo.name], Pasta)[0] != 0:
        Problemas.append("o check do arquivo migrado nao sai 0")
    if Caso["roda"]:
        Original = Pasta / f"{Nome}_orig.fj"
        Original.write_bytes(Caso["fonte"].encode("utf-8"))
        Saida1 = Rodar(Forja, ["run", Original.name], Pasta)
        Saida2 = Rodar(Forja, ["run", Arquivo.name], Pasta)
        if Saida1[0] != 0 or Saida2[0] != 0 or Saida1[1] != Saida2[1]:
            Problemas.append(f"saida do programa difere: antes {Saida1[1]!r} depois {Saida2[1]!r}")
    _, Saida3, _ = Rodar(Forja, ["migrar", Arquivo.name], Pasta)
    Doc3 = Documento(Saida3)
    if Doc3 is None or Doc3["total_trocas"] != 0:
        Problemas.append("2a passada com trocas (nao e idempotente)")
    return Problemas


def CasosLibm(Forja, Raiz):
    """[(rotulo, problemas)] dos casos de foreign de libm, numa subpasta (sem vendor)."""
    Pasta = Raiz / "libm"
    Pasta.mkdir()
    return [(f"libm_{Nome}", ProblemasDoCasoLibm(Forja, Pasta, Nome, Caso)) for Nome, Caso in LIBM_CASOS.items()]


def Verificar(Forja):
    """Roda tudo numa pasta temporaria; devolve a quantidade de falhas."""
    with tempfile.TemporaryDirectory(prefix="migrar_") as Nome:
        Raiz = Path(Nome)
        try:
            Resultados = CasosPorFixture(Forja, Raiz) + CasosDeCodificacao(Forja, Raiz) + CasosDeComentario(Forja, Raiz) + [("pasta", CasoPasta(Forja, Raiz)), ("uso", CasoUso(Forja, Raiz))]
            Resultados += CasosLibm(Forja, Raiz)
        finally:
            for Sub in list(Raiz.rglob("vendor")):
                Desligar(Sub)
    for Rotulo, Problemas in Resultados:
        print(f"{'FALHOU' if Problemas else 'ok    '} {Rotulo}")
        for Problema in Problemas:
            print(f"       {Problema}")
    Falhas = sum(1 for _, Problemas in Resultados if Problemas)
    print(f"casos={len(Resultados)} falhas={Falhas}")
    return Falhas


def AutoTeste():
    """Plantio: fixtures e .novo.fj existem, as 9 classes da tabela estao cobertas, e os conferidores reprovam o ruim e aprovam o bom."""
    Faltam = [N for N in CASOS if not (FIXTURES / f"{N}.fj").is_file()]
    Faltam += [N for N, (Estado, _, _) in CASOS.items() if Estado in ("ok", "revisao") and not (FIXTURES / f"{N}.novo.fj").is_file()]
    Cobertas = set().union(*(C for _, _, C in CASOS.values()))
    Falhas = []
    if Faltam:
        Falhas.append(f"fixtures: faltam {Faltam}")
    if Cobertas != CLASSES_DA_TABELA:
        Falhas.append(f"classes sem fixture: {sorted(CLASSES_DA_TABELA - Cobertas)}")
    Antes = b"a\nb em\nc\n"
    if ProblemasDeLinhas(Antes, b"a\nb alloc\nc\n", [{"linha": 2}]):
        Falhas.append("linhas: bom reprovado")
    if not ProblemasDeLinhas(Antes, b"a\nb alloc\nc\nd\n", [{"linha": 2}]) or not ProblemasDeLinhas(Antes, b"A\nb alloc\nc\n", [{"linha": 2}]):
        Falhas.append("linhas: ruim aprovado")
    Troca = {"linha": 2, "coluna": 3, "coluna_fim": 5, "antigo": "em"}
    if ProblemasDePosicao(Antes, [Troca]) or not ProblemasDePosicao(Antes, [dict(Troca, coluna=4)]):
        Falhas.append("posicao: conferidor errado")
    print("selftest:", "ok" if not Falhas else f"FALHOU {Falhas}")
    return 1 if Falhas else 0


def Principal():
    Parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    Parser.add_argument("--forja", default=str(FORJA_PADRAO), help="forja.exe a verificar")
    Parser.add_argument("--selftest", action="store_true")
    Argumentos = Parser.parse_args()
    if Argumentos.selftest:
        return AutoTeste()
    if not Path(Argumentos.forja).is_file():
        print(f"forja nao encontrado: {Argumentos.forja}")
        return 2
    return 1 if Verificar(str(Path(Argumentos.forja).resolve())) else 0


if __name__ == "__main__":
    sys.exit(Principal())
