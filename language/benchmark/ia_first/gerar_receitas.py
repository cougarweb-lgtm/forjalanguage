# gerar_receitas.py -- gera language\doc\receitas.txt e o texto do MEM-FORJA-GUIA-01 a partir de language\tests\receitas\*.fj
"""A "colinha de bolso" da Forja (parecer do Opus, 14/11/2026): o glossario por tarefa deixa de ser escrito a mao e passa a sair das receitas que a bateria
executa (tests\\receitas, verificar_receitas.py). Receita e programa .fj completo com cabecalho `// receita/tags/titulo/nota`; aqui so se junta o texto.
Uso:
    python -I gerar_receitas.py [--receitas <pasta>] [--saida-doc <receitas.txt>] [--conferir]
    python -I gerar_receitas.py --guia-antigo <GUIA-01.txt> [--saida-mem <pasta>]    (grava <pasta>\\MEM-FORJA-GUIA-01.txt)
    python -I gerar_receitas.py --selftest
Saidas:
  doc\\receitas.txt (CRLF como a pasta doc; UTF-8 sem BOM): por receita, "@@ <tag> | <tags>", "# <titulo>", "- <nota>" por linha e o programa sem o cabecalho.
     Deterministico: mesmas receitas, mesmos bytes. --conferir: exit 1 se o arquivo gravado difere do que sai agora.
  MEM-FORJA-GUIA-01.txt (LF): a secao "## MEM-FORJA-GUIA-01" inteira. Cada receita vira um bloco `**#tag #sinonimo...** - FEITO (receita testada)`; os blocos do
     texto antigo cujas tags nao aparecem em nenhuma receita seguem como estavam, marcados `NAO TESTADO` (nao somem: sao conteudo que ainda nao virou receita).
     Quem aplica no claude_memory.db e o main (extrair_mem.py / reimportar_mem.py); este script nao toca o banco.
Versao: 0.2.0
Changelog: 0.2.0 - bullets `- **` do GUIA-01 antigo (texto manual, sem `**#tag**`, ex.: "Rede e URL" com #http/#email) entram intactos no topo da secao; antes eram descartados.
           0.1.0 - POS-Z (colinha de bolso): primeira versao.
Verificacao: python -I gerar_receitas.py --selftest
"""
import argparse
import re
import sys
import tempfile
from pathlib import Path

LANG = Path(r"C:\Users\USUARIO\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
RECEITAS_PADRAO = LANG / "tests" / "receitas"
DOC_PADRAO = LANG / "doc" / "receitas.txt"
SAIDA_MEM_PADRAO = Path(tempfile.gettempdir()) / "ia1_doc_mem"
TITULO_GUIA = "Glossário por tarefa: \"quero fazer X\" → como se escreve"
CABECALHO_DOC = ("RECEITAS DA FORJA - o jeito certo de cada tarefa, programas que a bateria executa (tests\\receitas), juntados por benchmark\\ia_first\\gerar_receitas.py; "
                 "nao edite a mao.\nFormato: @@ tag | tags ; # titulo ; - nota ; depois o programa.\n")
PADRAO_CAMPO = re.compile(r"^// (receita|tags|titulo|nota): ?(.*)$")
PADRAO_BLOCO_ANTIGO = re.compile(r"^\*\*((?:#[\w-]+ ?)+)\*\*", re.M)
PADRAO_BULLET_ANTIGO = re.compile(r"^- \*\*", re.M)


def LerReceita(Arquivo):
    """Devolve {tag, tags, titulo, notas, programa} de uma receita; ValueError se o cabecalho nao tem os 4 campos."""
    Campos = {"receita": [], "tags": [], "titulo": [], "nota": []}
    Linhas = Arquivo.read_text(encoding="utf-8").replace("\r\n", "\n").split("\n")
    Fim = 0
    for Numero, Linha in enumerate(Linhas):
        Achou = PADRAO_CAMPO.match(Linha)
        if not Achou:
            break
        Campos[Achou.group(1)].append(Achou.group(2).strip())
        Fim = Numero + 1
    if not all(Campos.values()):
        raise ValueError(f"{Arquivo.name}: cabecalho incompleto")
    Programa = "\n".join(Linhas[Fim:]).strip("\n")
    return {"tag": Campos["receita"][0], "tags": Campos["tags"][0].split(), "titulo": Campos["titulo"][0], "notas": Campos["nota"], "programa": Programa}


def LerTodas(Pasta):
    return [LerReceita(A) for A in sorted(Pasta.glob("*.fj"))]


def TextoDoc(Receitas):
    Partes = [CABECALHO_DOC]
    for R in Receitas:
        Corpo = [f"@@ {R['tag']} | {' '.join(R['tags'])}", f"# {R['titulo']}"] + [f"- {N}" for N in R["notas"]] + [R["programa"], ""]
        Partes.append("\n".join(Corpo))
    return "\n".join(Partes).replace("\n", "\r\n")


def BlocoGuia(R):
    Etiquetas = " ".join("#" + T for T in R["tags"])
    Notas = "\n".join(R["notas"])
    return f"**{Etiquetas}** — FEITO (receita testada: tests\\receitas\\{R['tag']}.fj)\n```\n{R['programa']}\n```\n{Notas}\n"


def BlocosAntigos(Texto):
    """Parte o texto antigo do GUIA-01 em (tags, texto). Bloco `**#tag**` traz as tags; bullet `- **` (texto manual) traz None e so o seu paragrafo.
    O que sobra antes do 1o corte (cabecalho velho) e descartado."""
    Cortes = sorted([(M.start(), "bloco") for M in PADRAO_BLOCO_ANTIGO.finditer(Texto)] + [(M.start(), "bullet") for M in PADRAO_BULLET_ANTIGO.finditer(Texto)])
    Blocos = []
    for (Inicio, Tipo), (Fim, _) in zip(Cortes, Cortes[1:] + [(len(Texto), None)]):
        if Tipo == "bullet":
            Paragrafo = Texto[Inicio:Fim].split("\n\n", 1)[0]
            Blocos.append((None, Paragrafo.rstrip("\n") + "\n"))
            continue
        Bloco = Texto[Inicio:Fim].rstrip("\n") + "\n"
        Tags = {T.lstrip("#").replace("-", "_") for T in PADRAO_BLOCO_ANTIGO.match(Bloco).group(1).split()}
        Blocos.append((Tags, Bloco))
    return Blocos


def MarcarNaoTestado(Bloco):
    Linhas = Bloco.split("\n", 1)
    Cabecalho = re.sub(r" — FEITO.*$", "", Linhas[0])
    Cabecalho = Cabecalho if Cabecalho.endswith("**") else re.sub(r"(\*\*(?:#[\w-]+ ?)+\*\*).*$", r"\1", Linhas[0])
    return f"{Cabecalho} — NAO TESTADO (sem receita; conferir com forja check antes de copiar)\n{Linhas[1] if len(Linhas) > 1 else ''}"


def TextoGuia(Receitas, TextoAntigo):
    Cobertas = {T for R in Receitas for T in R["tags"]}
    Blocos = [BlocoGuia(R) for R in Receitas]
    Antigos = BlocosAntigos(TextoAntigo)
    Primeiro = next((I for I, (Tags, _) in enumerate(Antigos) if Tags is not None), len(Antigos))
    Topo = [Bloco for _, Bloco in Antigos[:Primeiro]]
    Sobras = []
    for Tags, Bloco in Antigos[Primeiro:]:
        if Tags is None:
            Sobras.append(Bloco)
        elif not (Tags & Cobertas):
            Sobras.append(MarcarNaoTestado(Bloco))
    Cabeca = (f"## MEM-FORJA-GUIA-01 — {TITULO_GUIA}\n\nGERADO por benchmark\\ia_first\\gerar_receitas.py a partir de language\\tests\\receitas (cada bloco FEITO e um programa que a bateria "
              "compila e roda nos 2 backends); nao edite a mao: edite a receita. Blocos NAO TESTADO ainda nao viraram receita.\n\n")
    return Cabeca + "\n".join(Topo + Blocos + Sobras)


def Selftest():
    with tempfile.TemporaryDirectory() as Temp:
        Pasta = Path(Temp)
        (Pasta / "um.fj").write_text("// receita: um\n// tags: um uno\n// titulo: T\n// nota: n1\n// nota: n2\nproc main() {\n}\n", encoding="utf-8")
        Lista = LerTodas(Pasta)
        Doc = TextoDoc(Lista)
        Antigo = "x\n- **Rede teste — tags `#url` `#email`.** texto\n\nGERADO antigo\n\n**#um #outro** — FEITO\n```\na\n```\n**#dois #tres** — FEITO (v1)\n```\nb\n```\n"
        Guia = TextoGuia(Lista, Antigo)
        Certo = (Lista[0]["tag"] == "um" and "@@ um | um uno\r\n# T\r\n- n1\r\n- n2\r\nproc main() {\r\n}" in Doc and "**#um #uno** — FEITO (receita testada" in Guia
                 and "**#dois #tres** — NAO TESTADO" in Guia and "**#um #outro**" not in Guia and Doc == TextoDoc(Lista)
                 and "- **Rede teste — tags `#url` `#email`.** texto\n" in Guia and Guia.index("- **Rede teste") < Guia.index("**#um #uno**") and "GERADO antigo" not in Guia)
    print("OK: selftest" if Certo else "FALHOU: selftest")
    return 0 if Certo else 1


def Principal():
    Analisador = argparse.ArgumentParser()
    Analisador.add_argument("--receitas", default=str(RECEITAS_PADRAO))
    Analisador.add_argument("--saida-doc", default=str(DOC_PADRAO))
    Analisador.add_argument("--conferir", action="store_true")
    Analisador.add_argument("--guia-antigo")
    Analisador.add_argument("--saida-mem", default=str(SAIDA_MEM_PADRAO))
    Analisador.add_argument("--selftest", action="store_true")
    Opcoes = Analisador.parse_args()
    if Opcoes.selftest:
        return Selftest()
    Receitas = LerTodas(Path(Opcoes.receitas))
    Doc = TextoDoc(Receitas).encode("utf-8")
    Alvo = Path(Opcoes.saida_doc)
    if Opcoes.conferir:
        Igual = Alvo.is_file() and Alvo.read_bytes() == Doc
        print("OK: receitas.txt em dia" if Igual else "FALHOU: receitas.txt difere do que sai das receitas")
        return 0 if Igual else 1
    Alvo.write_bytes(Doc)
    print(f"{len(Receitas)} receitas -> {Alvo}")
    if Opcoes.guia_antigo:
        Antigo = Path(Opcoes.guia_antigo).read_text(encoding="utf-8").replace("\r\n", "\n")
        Saida = Path(Opcoes.saida_mem)
        Saida.mkdir(parents=True, exist_ok=True)
        (Saida / "MEM-FORJA-GUIA-01.txt").write_bytes(TextoGuia(Receitas, Antigo).encode("utf-8"))
        print(f"GUIA-01 -> {Saida / 'MEM-FORJA-GUIA-01.txt'}")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
