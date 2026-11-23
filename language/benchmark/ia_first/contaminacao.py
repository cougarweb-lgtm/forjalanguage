# -*- coding: utf-8 -*-
"""contaminacao.py - le o transcript .jsonl de um subagente e diz se a celula do benchmark vale (P-IA1-a07, contrato 4.5).

Versao 0.1.3
Changelog:
  0.1.3 (14/11/2026) Portao Z: o caminho so conta quando a barra INICIA um token (lookbehind que recusa letra, digito, ponto, hifen e barra antes dela): `/var p/` de um
        `sed -i 's/let p/var p/'` do agente casava com `/var` (falso positivo medido na Z_S_R, C_forja CF01, 2 tentativas). So tira falso
        positivo; `cp prog.fj /tmp/x`, `cd /var/log` e `"/tmp/x"` seguem invalidando. Valido para todas as rodadas (reauditoria so do detector).
  0.1.2 (12/11/2026) P-IA1-a11: o prefixo de pasta do sistema (/var, /tmp, /etc...) so conta como caminho quando termina ali (lookahead
        `(?![\w.-])`): `/variavel_ambiente(` do codigo Forja do agente casava com `/var` (falso positivo medido na BASE_ESC, E14).
  0.1.1 (11/11/2026) P-IA1-a10: `..` em comando Bash so invalida se, resolvido a partir do ultimo `cd` para dentro da celula, sai dela
        (falso positivo medido no piloto: `cd <celula>/entrada && ia.py run ../prog.fj` fica dentro). Sem `cd` para dentro da celula,
        qualquer `..` segue invalidando, como na 0.1.0.
  0.1.0 (11/11/2026) primeira versao: celula INVALIDA se o transcript mostra MEM-FORJA, CLAUDE_FORJA, Nucleo Forja,
        ferramenta mem_search/mem_get/cg_*/mcp__* (pelo NOME da ferramenta, nao pelo texto do codigo do agente),
        leitura/escrita/busca fora da celula (e dos --permitir), caminho
        absoluto ou `..` fora da celula em comando Bash, ou injecao de regra nativa (forja.md); tambem tira tokens
        (entrada + saida, por mensagem unica) e a parede do transcript; acha o transcript pelo caminho unico da celula na
        1a mensagem.

Uso:
  python contaminacao.py --celula <pasta da celula> [--transcript <arq.jsonl>] [--permitir <pasta>]... [--json]
  Sem --transcript, procura em ~/.claude/projects/*/*/subagents/agent-*.jsonl o que cita a celula na 1a mensagem.
Saida: JSON {valida, motivos[], tokens{entrada,saida,total}, parede_s, ferramentas{}, transcript}. Exit 0 valida, 1 invalida,
  3 transcript nao achado.
Verificacao: python selftest_ia_first.py (planta contaminacao e exige celula invalida).
Reversao: apagar este arquivo. Depende de: Python 3.9+ (biblioteca padrao).
"""
import argparse
import json
import posixpath
import re
import sys
from datetime import datetime
from pathlib import Path

RAIZ_PROJETOS = Path.home() / ".claude" / "projects"
PADRAO_MARCAS = re.compile(r"MEM-FORJA|CLAUDE_FORJA|N[uú]cleo\s+Forja|rules[\\/]+forja\.md", re.I)
PADRAO_CAMINHO = re.compile(r"(?<![\w.\-/])(?:[A-Za-z]:[\\/]|~[\\/]|/[a-z]/|/(?:tmp|etc|usr|home|mnt|var|opt|dev)(?![\w.-])/?)[^\s\"'|;&<>)]*")
PADRAO_SOBE = re.compile(r"(^|[\s\"'=/\\])\.\.[\\/]")
PADRAO_CD = re.compile(r"\bcd\s+[\"']?([^\"';&|\s]+)")
FERRAMENTAS_DE_ARQUIVO = {"Read": "file_path", "Write": "file_path", "Edit": "file_path", "NotebookEdit": "notebook_path"}
FERRAMENTAS_DE_BUSCA = ("Glob", "Grep")
FERRAMENTAS_DO_CONTRATO = ("Read", "Write", "Edit", "Bash", "SubagentHandback")
PERMITIDOS_FIXOS = ("/dev/null",)


def Normalizar(Caminho):
    """Caminho em minusculo com / e sem barra final; /c/x vira c:/x (Git Bash)."""
    Texto = str(Caminho).replace("\\", "/").rstrip("/").lower()
    return re.sub(r"^/([a-z])/", r"\1:/", Texto)


def Dentro(Caminho, Raizes):
    Alvo = Normalizar(Caminho)
    return any(Alvo == R or Alvo.startswith(R + "/") for R in Raizes)


def LerRegistros(Transcript):
    Registros = []
    for Linha in Path(Transcript).read_text(encoding="utf-8", errors="replace").splitlines():
        try:
            Registros.append(json.loads(Linha))
        except ValueError:
            continue
    return Registros


def BlocosDe(Registro):
    Conteudo = Registro.get("message", {}).get("content", [])
    return Conteudo if isinstance(Conteudo, list) else [{"type": "text", "text": Conteudo}]


def UsosDeFerramenta(Registros):
    """Lista de (nome, input) de todo tool_use do subagente."""
    return [(B.get("name", ""), B.get("input", {})) for R in Registros if R.get("type") == "assistant"
            for B in BlocosDe(R) if B.get("type") == "tool_use"]


def MotivosDeFerramenta(Nome, Entrada):
    if Nome.startswith("mcp__") or Nome in ("mem_search", "mem_get") or Nome.startswith("cg_"):
        return ["ferramenta proibida: " + Nome]
    return [] if Nome in FERRAMENTAS_DO_CONTRATO else ["ferramenta fora do contrato do bench_ia: " + Nome]


def MotivosDeCaminho(Nome, Entrada, Raizes):
    """Leitura/escrita/busca fora da celula (Read, Write, Edit, Glob, Grep)."""
    if Nome in FERRAMENTAS_DE_ARQUIVO:
        Alvo = Entrada.get(FERRAMENTAS_DE_ARQUIVO[Nome], "")
        return [] if Dentro(Alvo, Raizes) else ["%s fora da celula: %s" % (Nome, Alvo)]
    if Nome in FERRAMENTAS_DE_BUSCA:
        Alvo = Entrada.get("path")
        return [] if Alvo and Dentro(Alvo, Raizes) else ["%s fora da celula (path=%s)" % (Nome, Alvo)]
    return []


def MotivosDeBash(Comando, Raizes):
    Motivos = []
    for Achado in PADRAO_CAMINHO.findall(Comando):
        if Normalizar(Achado) not in PERMITIDOS_FIXOS and not Dentro(Achado, Raizes):
            Motivos.append("Bash toca fora da celula: " + Achado)
    if SobeDaCelula(Comando, Raizes):
        Motivos.append("Bash usa .. (sobe da celula): " + Comando[:120])
    return Motivos


def SobeDaCelula(Comando, Raizes):
    """True se algum `..` do comando sai da celula. `..` depois de um `cd` para dentro da celula e resolvido a partir dele
    (cd entrada && ... ../prog.fj fica dentro); sem esse `cd`, qualquer `..` conta como antes."""
    for Achado in PADRAO_SOBE.finditer(Comando):
        Base = None
        for Cd in PADRAO_CD.finditer(Comando[:Achado.start()]):
            Destino = Normalizar(Cd.group(1))
            Base = Destino if Dentro(Destino, Raizes) else None
        if Base is None:
            return True
        Caminho = Normalizar(Comando[Achado.start():].split()[0].strip("\"'=").lstrip("/\\ "))
        if not Dentro(posixpath.normpath(Base + "/" + Caminho), Raizes):
            return True
    return False


def MotivosDeUso(Usos, Raizes):
    Motivos = []
    for Nome, Entrada in Usos:
        Motivos += MotivosDeFerramenta(Nome, Entrada) + MotivosDeCaminho(Nome, Entrada, Raizes)
        if Nome == "Bash":
            Motivos += MotivosDeBash(str(Entrada.get("command", "")), Raizes)
        Marca = PADRAO_MARCAS.search(json.dumps(Entrada, ensure_ascii=False))
        if Marca:
            Motivos.append("marca %r em %s" % (Marca.group(0), Nome))
    return Motivos


def MotivosDeInjecao(Registros):
    """Marca da Forja em qualquer registro que nao seja o pedido do main nem fala do assistente (resultado, lembrete, regra)."""
    Motivos = []
    for Indice, Registro in enumerate(Registros):
        if Indice == 0 or Registro.get("type") == "assistant":
            continue
        Marca = PADRAO_MARCAS.search(json.dumps(Registro, ensure_ascii=False))
        if Marca:
            Motivos.append("marca %r injetada/lida (registro %d, tipo %s)" % (Marca.group(0), Indice, Registro.get("type")))
    return Motivos


def Tokens(Registros):
    """Entrada (input + cache criado + cache lido) e saida somadas por mensagem unica (o ultimo registro de cada id)."""
    PorId = {}
    for Registro in Registros:
        Mensagem = Registro.get("message", {})
        if Registro.get("type") == "assistant" and Mensagem.get("usage"):
            PorId[Mensagem.get("id") or Registro.get("uuid")] = Mensagem["usage"]
    Entrada = sum(U.get("input_tokens", 0) + U.get("cache_creation_input_tokens", 0) + U.get("cache_read_input_tokens", 0) for U in PorId.values())
    Saida = sum(U.get("output_tokens", 0) for U in PorId.values())
    return {"entrada": Entrada, "saida": Saida, "total": Entrada + Saida}


def Parede(Registros):
    Horas = [R["timestamp"] for R in Registros if R.get("timestamp")]
    if len(Horas) < 2:
        return 0.0
    return round((LerHora(max(Horas)) - LerHora(min(Horas))).total_seconds(), 1)


def LerHora(Texto):
    return datetime.fromisoformat(Texto.replace("Z", "+00:00"))


def Analisar(Transcript, Celula, Permitidos=()):
    """Resultado da conferencia de um transcript contra uma celula."""
    Registros = LerRegistros(Transcript)
    Raizes = [Normalizar(Celula)] + [Normalizar(P) for P in Permitidos]
    Usos = UsosDeFerramenta(Registros)
    Motivos = MotivosDeUso(Usos, Raizes) + MotivosDeInjecao(Registros)
    Contagem = {}
    for Nome, _ in Usos:
        Contagem[Nome] = Contagem.get(Nome, 0) + 1
    return {"valida": not Motivos, "motivos": Motivos[:20], "tokens": Tokens(Registros), "parede_s": Parede(Registros),
            "ferramentas": Contagem, "transcript": str(Transcript)}


def PrimeiroPedido(Arquivo):
    """Texto da 1a mensagem do subagente (o pedido do main), normalizado."""
    with open(Arquivo, encoding="utf-8", errors="replace") as Arq:
        Registro = json.loads(Arq.readline() or "{}")
    return Normalizar(json.dumps(BlocosDe(Registro), ensure_ascii=False).replace("\\\\", "/"))


def AcharTranscript(Celula, Desde=0.0, Raiz=RAIZ_PROJETOS):
    """Transcript de subagente cuja 1a mensagem cita o caminho da celula (o mais novo primeiro)."""
    Alvo = re.compile(re.escape(Normalizar(Celula)) + r"(?![\w])")
    Arquivos = sorted(Raiz.glob("*/*/subagents/agent-*.jsonl"), key=lambda A: A.stat().st_mtime, reverse=True)
    for Arquivo in Arquivos:
        if Arquivo.stat().st_mtime < Desde:
            break
        try:
            if Alvo.search(PrimeiroPedido(Arquivo)):
                return Arquivo
        except (OSError, ValueError):
            continue
    return None


def Principal():
    Analisador = argparse.ArgumentParser(description="Contaminacao de uma celula do benchmark IA FIRST")
    Analisador.add_argument("--celula", required=True)
    Analisador.add_argument("--transcript")
    Analisador.add_argument("--permitir", action="append", default=[])
    Analisador.add_argument("--json", action="store_true")
    Args = Analisador.parse_args()
    Arquivo = Path(Args.transcript) if Args.transcript else AcharTranscript(Args.celula)
    if Arquivo is None or not Arquivo.is_file():
        print(json.dumps({"valida": None, "motivos": ["transcript nao achado"]}))
        return 3
    Resultado = Analisar(Arquivo, Args.celula, Args.permitir)
    print(json.dumps(Resultado, ensure_ascii=False))
    return 0 if Resultado["valida"] else 1


if __name__ == "__main__":
    sys.exit(Principal())
