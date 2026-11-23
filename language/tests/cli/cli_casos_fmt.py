"""Casos de cli do forja fmt (F1 P1b): dry-run sem gravar, --aplicar com conferencia, --json, pasta com bom + ruim + erro, BOM+CRLF, caso degenerado, --stdin e usos errados.

Importado pelo motor verificar_cli.py. As fixtures sao as de tests\\fmt\\casos (antes/depois). Todo trabalho e numa copia em pasta temporaria (tempfile.mkdtemp, removida no finally);
nenhum arquivo do repositorio e formatado.

Regras:
  - dry-run (padrao): exit 1 quando algum arquivo mudaria, o diff e o rodape no stdout, o arquivo fica intacto e 2 execucoes dao a mesma saida;
  - --aplicar: o arquivo fica igual ao depois e a 2a passada da exit 0;
  - --json: {"arquivos": [...], "totais": {...}} com os campos (estado, desvios, primeiro, edicoes com a regra, diagnostico, motivo);
  - pasta com arquivo bom + arquivo ruim + arquivo com erro: o erro e nao_analisado e intacto, exit 1;
  - BOM+CRLF: --aplicar grava o depois (LF, sem BOM);
  - caso degenerado (so espaco e linha em branco): o dry-run mostra o apagamento e --aplicar deixa o arquivo vazio;
  - --stdin: o texto canonico na saida; --stdin com --aplicar, --json ou caminho = exit 2;
  - caminho ausente, opcao desconhecida, sem argumentos e --regras sem --stdin = exit 2;
  - arquivo so-leitura com --aplicar = exit 2 e o arquivo intacto.

Versao: 1.4.0
Changelog: 1.4.0 - 0.25.1: +1 caso (CasoCheckCanonicoResumo: check --canonico --resumo em forma resumo, igual ao forja_c.exe; SaidaDiag unico no F0001).
           1.3.0 - Portao Z b1: +1 caso (CasoFmtSoLeituraNaoSobrescreve: fmt --aplicar em so-leitura, exit diferente de 0, arquivo intacto, sem sobra).
           1.2.0 - F1 P2: +6 casos do check --canonico (F0001): canonico passa, fora da forma = 1 F0001 com correcao, sem a flag nada muda, erro + --canonico sem F0001, --canonico fora do check = exit 2, explicar F0001.
           1.1.0 - F1 P1b: +1 caso (arquivo sem '\\n' final: diff de verdade no dry-run, fix do edicoes.c 0.2.1).
           1.0.0 - F1 P1b: primeira versao (9 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import os
import shutil
import stat
import subprocess
import tempfile
from pathlib import Path

from verificar_cli import ConferirExecucao, PastaAqui, Rodar

FIXTURES = PastaAqui.parent / "fmt" / "casos"
NOME_BASE = "espaco_basico"
ERRO_DE_SINTAXE = b"proc main( {\n"
RODAPE_DRY_RUN_UM = "1 mudariam, 0 nao analisados, 0 canonicos"
RODAPE_GRAVADO_UM = "1 gravados, 0 nao analisados, 0 canonicos"
RODAPE_CANONICO_UM = "0 mudariam, 0 nao analisados, 1 canonicos"


def NovaPasta():
    return Path(tempfile.mkdtemp(prefix="cli_fmt_"))


def Antes(Nome):
    return (FIXTURES / f"{Nome}.antes.fj").read_bytes()


def Depois(Nome):
    return (FIXTURES / f"{Nome}.depois.fj").read_bytes()


def RodarBytes(Forja, Argumentos, Entrada=b""):
    """Como Rodar, mas a entrada e a saida em bytes (sem a traducao de fim de linha do modo texto)."""
    return subprocess.run([str(Forja)] + Argumentos, input=Entrada, capture_output=True, timeout=60, cwd=PastaAqui)


def JsonDe(Rodada, Problemas):
    try:
        return json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Problemas.append(f"stdout nao e JSON: {Rodada.stdout[:100]!r}")
        return None


def CasoFmtDryRun(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = Pasta / f"{NOME_BASE}.fj"
        shutil.copyfile(FIXTURES / f"{NOME_BASE}.antes.fj", Arquivo)
        Bytes = Arquivo.read_bytes()
        Primeira = Rodar(Forja, ["fmt", str(Arquivo)])
        Segunda = Rodar(Forja, ["fmt", str(Arquivo)])
        Problemas += ConferirExecucao(Primeira, 1, UltimaEsperada=RODAPE_DRY_RUN_UM, TrechoStdout="--- a/")
        if Primeira.stdout != Segunda.stdout:
            Problemas.append("duas execucoes do dry-run deram saidas diferentes")
        if Arquivo.read_bytes() != Bytes:
            Problemas.append("o dry-run gravou")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtAplica(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = Pasta / f"{NOME_BASE}.fj"
        shutil.copyfile(FIXTURES / f"{NOME_BASE}.antes.fj", Arquivo)
        Rodada = Rodar(Forja, ["fmt", "--aplicar", str(Arquivo)])
        Problemas += ConferirExecucao(Rodada, 0, UltimaEsperada=RODAPE_GRAVADO_UM)
        if Arquivo.read_bytes() != Depois(NOME_BASE):
            Problemas.append("--aplicar nao deixou o arquivo igual ao depois")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", str(Arquivo)]), 0, UltimaEsperada=RODAPE_CANONICO_UM)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtJson(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = Pasta / f"{NOME_BASE}.fj"
        shutil.copyfile(FIXTURES / f"{NOME_BASE}.antes.fj", Arquivo)
        Dry = Rodar(Forja, ["fmt", "--json", str(Arquivo)])
        Problemas += ConferirExecucao(Dry, 1)
        Doc = JsonDe(Dry, Problemas)
        if Doc is not None:
            Item = Doc["arquivos"][0]
            Campos = {"linha", "coluna", "coluna_fim", "antigo", "novo", "regra"}
            if Item["estado"] != "mudaria" or not Item["edicoes"] or Item["desvios"] != len(Item["edicoes"]):
                Problemas.append(f"dry-run --json fora do esperado: estado {Item['estado']}, {len(Item['edicoes'])} edicoes, {Item['desvios']} desvios")
            if not Campos <= set(Item["edicoes"][0]):
                Problemas.append(f"edicao sem os campos {sorted(Campos)}: {Item['edicoes'][0]}")
            if Doc["totais"] != {"arquivos": 1, "canonicos": 0, "mudariam": 1, "gravados": 0, "nao_analisados": 0}:
                Problemas.append(f"totais do dry-run: {Doc['totais']}")
        Gravado = Rodar(Forja, ["fmt", "--aplicar", "--json", str(Arquivo)])
        Problemas += ConferirExecucao(Gravado, 0)
        Doc2 = JsonDe(Gravado, Problemas)
        if Doc2 is not None and (Doc2["arquivos"][0]["estado"] != "gravado" or Doc2["totais"]["gravados"] != 1):
            Problemas.append(f"--aplicar --json fora do esperado: {Doc2['arquivos'][0]['estado']}, {Doc2['totais']}")
        if Arquivo.read_bytes() != Depois(NOME_BASE):
            Problemas.append("--aplicar --json nao deixou o arquivo igual ao depois")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtPasta(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Projeto = Pasta / "projeto"
        Projeto.mkdir()
        (Projeto / "bom.fj").write_bytes(Depois(NOME_BASE))
        shutil.copyfile(FIXTURES / f"{NOME_BASE}.antes.fj", Projeto / "ruim.fj")
        (Projeto / "erro.fj").write_bytes(ERRO_DE_SINTAXE)
        Antes_ = {Nome: (Projeto / Nome).read_bytes() for Nome in ("bom.fj", "ruim.fj", "erro.fj")}
        Doc = JsonDe(Rodar(Forja, ["fmt", "--json", str(Projeto)]), Problemas)
        if Doc is not None:
            Estados = {Path(Item["arquivo"]).name: Item["estado"] for Item in Doc["arquivos"]}
            if Estados != {"bom.fj": "canonico", "erro.fj": "nao_analisado", "ruim.fj": "mudaria"}:
                Problemas.append(f"estados da pasta: {Estados}")
            if Doc["totais"] != {"arquivos": 3, "canonicos": 1, "mudariam": 1, "gravados": 0, "nao_analisados": 1}:
                Problemas.append(f"totais da pasta: {Doc['totais']}")
            if Doc["arquivos"][1]["diagnostico"] is None:
                Problemas.append("o arquivo com erro nao trouxe diagnostico")
        Rodada = Rodar(Forja, ["fmt", "--aplicar", str(Projeto)])
        Problemas += ConferirExecucao(Rodada, 1, UltimaEsperada="1 gravados, 1 nao analisados, 1 canonicos")
        if (Projeto / "ruim.fj").read_bytes() != Depois(NOME_BASE):
            Problemas.append("ruim.fj nao ficou igual ao depois")
        for Nome in ("bom.fj", "erro.fj"):
            if (Projeto / Nome).read_bytes() != Antes_[Nome]:
                Problemas.append(f"{Nome} foi tocado e nao devia")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtBomCrlf(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = Pasta / "bom_crlf.fj"
        Arquivo.write_bytes(b"\xef\xbb\xbf" + Depois(NOME_BASE).replace(b"\n", b"\r\n"))
        Dry = Rodar(Forja, ["fmt", "--json", str(Arquivo)])
        Problemas += ConferirExecucao(Dry, 1)
        Doc = JsonDe(Dry, Problemas)
        if Doc is not None:
            Item = Doc["arquivos"][0]
            if Item["desvios"] != 2 or Item["primeiro"]["regra"] != "bom" or Item["edicoes"]:
                Problemas.append(f"BOM+CRLF: {Item['desvios']} desvios, primeiro {Item['primeiro']}, {len(Item['edicoes'])} edicoes (esperado 2, bom e nenhuma edicao)")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", str(Arquivo)]), 1, TrechoStdout="so BOM e/ou fim de linha CRLF")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", "--aplicar", str(Arquivo)]), 0)
        if Arquivo.read_bytes() != Depois(NOME_BASE):
            Problemas.append("BOM+CRLF: --aplicar nao gravou o depois (LF, sem BOM)")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtDegenerado(Forja, PastaSaidas):
    """So espaco e linha em branco: o motor nao expressa a faixa toda, por isso o diff e de apagamento (3 linhas) e o arquivo fica vazio."""
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = Pasta / "so_espacos.fj"
        Arquivo.write_bytes(b"   \n\n  \n")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", str(Arquivo)]), 1, TrechoStdout="@@ -1,3 +0,0 @@")
        if Arquivo.read_bytes() != b"   \n\n  \n":
            Problemas.append("o dry-run gravou o caso degenerado")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", "--aplicar", str(Arquivo)]), 0)
        if Arquivo.read_bytes() != b"":
            Problemas.append("--aplicar nao deixou o arquivo degenerado vazio")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", str(Arquivo)]), 0)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtSemFimDeLinha(Forja, PastaSaidas):
    """Arquivo sem '\\n' final com desvio na ultima linha: o dry-run mostra o diff de verdade (sem crash, edicoes.c 0.2.1) e --aplicar deixa o arquivo igual ao depois."""
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = Pasta / "sem_nl_final.fj"
        shutil.copyfile(FIXTURES / "sem_nl_final.antes.fj", Arquivo)
        Bytes = Arquivo.read_bytes()
        if Bytes.endswith(b"\n"):
            Problemas.append("a fixture sem_nl_final.antes.fj devia acabar sem '\\n'")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", str(Arquivo)]), 1, UltimaEsperada=RODAPE_DRY_RUN_UM, TrechoStdout="--- a/")
        Rodada = Rodar(Forja, ["fmt", str(Arquivo)])
        if "diff indisponivel" in Rodada.stdout or "@@" not in Rodada.stdout or "No newline at end of file" not in Rodada.stdout:
            Problemas.append("o dry-run devia mostrar um diff com hunk e a marca 'No newline at end of file'")
        if Arquivo.read_bytes() != Bytes:
            Problemas.append("o dry-run gravou")
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", "--aplicar", str(Arquivo)]), 0, UltimaEsperada=RODAPE_GRAVADO_UM)
        if Arquivo.read_bytes() != Depois("sem_nl_final"):
            Problemas.append("--aplicar nao deixou o arquivo igual ao depois")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtStdin(Forja, PastaSaidas):
    Problemas = []
    Rodada = RodarBytes(Forja, ["fmt", "--stdin"], Antes(NOME_BASE))
    Problemas += ConferirExecucao(Rodada, 0)
    if Rodada.stdout != Depois(NOME_BASE):
        Problemas.append("--stdin nao devolveu o depois")
    Regras = RodarBytes(Forja, ["fmt", "--stdin", "--regras"], Antes(NOME_BASE))
    if Regras.returncode != 0 or not Regras.stderr.startswith(b"desvios="):
        Problemas.append("--stdin --regras sem a linha desvios= no stderr")
    Erro = RodarBytes(Forja, ["fmt", "--stdin"], ERRO_DE_SINTAXE)
    Problemas += ConferirExecucao(Erro, 1)
    if Erro.stdout != b"" or b"nao_analisado" not in Erro.stderr:
        Problemas.append("--stdin com erro de sintaxe: stdout devia estar vazio e o JSON no stderr")
    for Argumentos in (["--stdin", "--aplicar"], ["--stdin", "--json"], ["--stdin", "x.fj"]):
        Rodada = RodarBytes(Forja, ["fmt"] + Argumentos, Antes(NOME_BASE))
        Problemas += [f"fmt {' '.join(Argumentos)}: {P}" for P in ConferirExecucao(Rodada, 2)]
        if Rodada.stdout != b"":
            Problemas.append(f"fmt {' '.join(Argumentos)}: stdout devia estar vazio")
    return Problemas


def CasoFmtUsoErrado(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = Pasta / f"{NOME_BASE}.fj"
        shutil.copyfile(FIXTURES / f"{NOME_BASE}.antes.fj", Arquivo)
        for Argumentos in ([], [str(Pasta / "nao_existe.fj")], ["--foo", str(Arquivo)], ["--regras", str(Arquivo)]):
            Problemas += [f"fmt {' '.join(Argumentos)}: {P}" for P in ConferirExecucao(Rodar(Forja, ["fmt"] + Argumentos), 2)]
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtSoLeitura(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    Arquivo = Pasta / f"{NOME_BASE}.fj"
    try:
        shutil.copyfile(FIXTURES / f"{NOME_BASE}.antes.fj", Arquivo)
        Antes_ = Arquivo.read_bytes()
        os.chmod(Arquivo, stat.S_IREAD)
        Problemas += ConferirExecucao(Rodar(Forja, ["fmt", "--aplicar", str(Arquivo)]), 2)
        if Arquivo.read_bytes() != Antes_:
            Problemas.append("o arquivo so-leitura foi alterado")
        if sorted(os.listdir(Pasta)) != [Arquivo.name]:
            Problemas.append(f"sobrou arquivo na pasta depois da falha: {sorted(os.listdir(Pasta))}")
    finally:
        os.chmod(Arquivo, stat.S_IREAD | stat.S_IWRITE)
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoFmtSoLeituraNaoSobrescreve(Forja, PastaSaidas):
    """fmt --aplicar em .fj nao canonico so-leitura: exit diferente de 0, arquivo igual ao antes e nenhum .tmp/.velho na pasta."""
    Problemas = []
    Pasta = NovaPasta()
    Arquivo = Pasta / f"{NOME_BASE}.fj"
    try:
        shutil.copyfile(FIXTURES / f"{NOME_BASE}.antes.fj", Arquivo)
        Antes_ = Arquivo.read_bytes()
        os.chmod(Arquivo, stat.S_IREAD)
        Rodada = Rodar(Forja, ["fmt", "--aplicar", str(Arquivo)])
        if Rodada.returncode == 0:
            Problemas.append("fmt --aplicar em so-leitura saiu com exit 0")
        if Arquivo.read_bytes() != Antes_:
            Problemas.append("o arquivo so-leitura foi alterado")
        if sorted(os.listdir(Pasta)) != [Arquivo.name]:
            Problemas.append(f"sobrou arquivo na pasta: {sorted(os.listdir(Pasta))}")
    finally:
        os.chmod(Arquivo, stat.S_IREAD | stat.S_IWRITE)
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


# ---------- F1 P2: check --canonico (F0001) ----------

FONTE_CANONICO_P2 = "proc main() {\n    print(1)\n}\n"
FONTE_FORA_DA_FORMA_P2 = "proc main() {\n      print( 1 )\n}\n"  # indentacao de 6 espacos e 2 espacos dentro de ( )
FONTE_ERRO_SEMANTICO_P2 = "proc main() {\n      print(x)\n}\n"  # x nao declarado (E0201) e fora da forma
FONTE_ERRO_SINTAXE_P2 = "proc main( {\n"


def EscreverFonteP2(Pasta, Nome, Texto):
    Arquivo = Pasta / Nome
    Arquivo.write_bytes(Texto.encode("utf-8"))
    return Arquivo


def LinhasNaoVaziasP2(Rodada):
    return [Linha for Linha in Rodada.stdout.splitlines() if Linha.strip()]


def JsonDeLinhaP2(Linha):
    try:
        return json.loads(Linha)
    except json.JSONDecodeError:
        return None


def CasoCheckCanonicoPassa(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = EscreverFonteP2(Pasta, "canonico.fj", FONTE_CANONICO_P2)
        Rodada = Rodar(Forja, ["check", "--canonico", str(Arquivo)])
        Problemas += ConferirExecucao(Rodada, 0)
        if LinhasNaoVaziasP2(Rodada):
            Problemas.append(f"check --canonico em arquivo canonico imprimiu: {Rodada.stdout[:120]!r}")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def ConferirF0001Fora(Problemas, Dados, Arquivo, Forja):
    """Confere os campos do F0001 do arquivo FONTE_FORA_DA_FORMA_P2 (linha 2, coluna 1, indentacao, correcao certa)."""
    Esperados = {"codigo": "F0001", "tipo": "formato", "linha": 2, "coluna": 1, "coluna_fim": 7, "confianca": "certa"}
    for Chave, Valor in Esperados.items():
        if Dados.get(Chave) != Valor:
            Problemas.append(f"F0001 campo {Chave} = {Dados.get(Chave)!r}, esperado {Valor!r}")
    if not str(Dados.get("causa", "")).startswith("fora da forma canonica: indentacao ("):
        Problemas.append(f"causa do F0001 fora do esperado: {Dados.get('causa')!r}")
    if Dados.get("forma_certa") != f"rode forja fmt --aplicar {Arquivo}":
        Problemas.append(f"forma_certa do F0001: {Dados.get('forma_certa')!r}")
    Correcao = Dados.get("correcao") or [{}]
    if Correcao[0].get("novo") != "    " or Correcao[0].get("antigo") != "      ":
        Problemas.append(f"correcao do F0001 fora do esperado: {Correcao[0]!r}")
    Fmt = json.loads(Rodar(Forja, ["fmt", "--json", str(Arquivo)]).stdout)
    Desvios = Fmt["arquivos"][0]["desvios"]
    if f"fora da forma canonica: indentacao ({Desvios} desvios)" != Dados.get("causa"):
        Problemas.append(f"o total de desvios do F0001 nao bate com o fmt --json ({Desvios}): {Dados.get('causa')!r}")


def CasoCheckCanonicoAcusaF0001(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = EscreverFonteP2(Pasta, "fora.fj", FONTE_FORA_DA_FORMA_P2)
        Rodada = Rodar(Forja, ["check", "--canonico", str(Arquivo)])
        Problemas += ConferirExecucao(Rodada, 1)
        Linhas = LinhasNaoVaziasP2(Rodada)
        Dados = JsonDeLinhaP2(Linhas[0]) if len(Linhas) == 1 else None
        if Dados is None:
            Problemas.append(f"esperava 1 linha JSON (F0001), veio {Rodada.stdout[:160]!r}")
        else:
            ConferirF0001Fora(Problemas, Dados, Arquivo, Forja)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoCheckSemCanonicoIgual(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = EscreverFonteP2(Pasta, "fora.fj", FONTE_FORA_DA_FORMA_P2)
        Rodada = Rodar(Forja, ["check", str(Arquivo)])
        Problemas += ConferirExecucao(Rodada, 0)
        if LinhasNaoVaziasP2(Rodada):
            Problemas.append(f"check sem --canonico acusou algo: {Rodada.stdout[:120]!r}")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoCheckCanonicoComErro(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        for Nome, Texto, CodigoEsperado in (("semantico.fj", FONTE_ERRO_SEMANTICO_P2, "E0201"), ("sintaxe.fj", FONTE_ERRO_SINTAXE_P2, None)):
            Arquivo = EscreverFonteP2(Pasta, Nome, Texto)
            Rodada = Rodar(Forja, ["check", "--canonico", str(Arquivo)])
            Problemas += [f"{Nome}: {P}" for P in ConferirExecucao(Rodada, 1)]
            Linhas = LinhasNaoVaziasP2(Rodada)
            if "F0001" in Rodada.stdout or not Linhas:
                Problemas.append(f"{Nome}: o erro devia sair sozinho (sem F0001): {Rodada.stdout[:160]!r}")
            Dados = JsonDeLinhaP2(Linhas[0]) if Linhas else None
            if Dados is None or (CodigoEsperado is not None and Dados.get("codigo") != CodigoEsperado):
                Problemas.append(f"{Nome}: codigo do erro {Dados and Dados.get('codigo')!r}, esperado {CodigoEsperado!r}")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoCanonicoForaDoCheck(Forja, PastaSaidas):
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = EscreverFonteP2(Pasta, "fora.fj", FONTE_FORA_DA_FORMA_P2)
        for Comando in (["run"], ["build"], ["ast"]):
            Problemas += [f"{Comando[0]} --canonico: {P}" for P in ConferirExecucao(Rodar(Forja, Comando + ["--canonico", str(Arquivo)]), 2)]
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoCheckCanonicoResumo(Forja, PastaSaidas):
    """0.25.1: check --canonico --resumo imprime o F0001 na forma resumo (SaidaDiag unico do principal), igual byte a byte ao forja_c.exe."""
    Problemas = []
    Pasta = NovaPasta()
    try:
        Arquivo = EscreverFonteP2(Pasta, "fora.fj", FONTE_FORA_DA_FORMA_P2)
        Rodada = Rodar(Forja, ["check", "--canonico", "--resumo", str(Arquivo)])
        Problemas += ConferirExecucao(Rodada, 1)
        Linhas = LinhasNaoVaziasP2(Rodada)
        Dados = JsonDeLinhaP2(Linhas[0]) if len(Linhas) == 1 else None
        if Dados is None:
            Problemas.append(f"esperava 1 linha JSON (F0001 resumido), veio {Rodada.stdout[:160]!r}")
            return Problemas
        if set(Dados) != {"codigo", "arquivo", "linha", "coluna", "causa", "sugestao"}:
            Problemas.append(f"F0001 em --resumo deveria ter so codigo, arquivo, linha, coluna, causa e sugestao: {sorted(Dados)}")
        Oraculo = Path(Forja).with_name("forja_c.exe")
        if Oraculo.exists():
            Antigo = Rodar(str(Oraculo), ["check", "--canonico", "--resumo", str(Arquivo)])
            if Antigo.stdout != Rodada.stdout:
                Problemas.append(f"o F0001 resumido difere do forja_c.exe ({len(Rodada.stdout)} contra {len(Antigo.stdout)} bytes)")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoExplicarF0001(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "F0001"])
    Problemas = ConferirExecucao(Rodada, 0)
    Linhas = LinhasNaoVaziasP2(Rodada)
    Dados = JsonDeLinhaP2(Linhas[0]) if Linhas else None
    if Dados is None or Dados.get("codigo") != "F0001" or Dados.get("tipo") != "formato" or not Dados.get("forma_certa"):
        Problemas.append(f"explicar F0001 sem o catalogo: {Dados!r}")
    return Problemas
