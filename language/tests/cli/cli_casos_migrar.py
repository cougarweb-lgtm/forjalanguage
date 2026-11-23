"""Casos de cli do forja migrar (IA1-L3d1): dry-run sem gravar, --aplicar com idempotencia, arquivo com erro (nao_analisado) e usos errados.

Importado pelo motor verificar_cli.py; a prova ampla (uma fixture por classe da tabela, CRLF, BOM, pasta, colisao) mora em benchmark\\ia_first\\verificar_migrar.py; aqui ficam os
casos curtos do motor, numa copia temporaria das fixtures de benchmark\\ia_first\\fixtures_migrar.

Regras:
  - sem --aplicar (--dry-run e o padrao): 1 documento JSON ({arquivos, total_trocas, arquivos_alterados, linhas_iguais}) e 0 arquivo gravado; 2 execucoes = bytes iguais;
  - com --aplicar: o arquivo fica igual ao .novo.fj, o `forja check` dele sai 0 e a 2a passada acha 0 trocas;
  - arquivo com erro: estado nao_analisado, diagnostico com o codigo, exit 1 e o arquivo intacto;
  - uso errado (sem entrada, --prova, que nao existe, --dry-run com --aplicar, entrada inexistente) = exit 2;
  - --comentarios (L3d2): codigo Forja em `//` migra junto do codigo, prosa vai para revisao_comentario sem edicao.

Versao: 1.1.3
Changelog: 1.1.3 - D-G3-L2a: CasoMigrarAplica espera 19 trocas (a fixture modulo_file ganhou o case FileError.locked do switch exaustivo). 1.1.2 - B01e2: CasoMigrarAplica espera 12 trocas (a fixture modulo_file ganhou o alloc = heap do read_all).
  1.1.1 - E4: FIXTURES aponta para benchmark\\ia_first (pasta movida).
  1.1.0 - IA1-L3d2: CasoMigrarComentarios; --comentarios sai da lista de usos errados.
  1.0.0 - IA1-L3d1: primeira versao (3 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import shutil
import tempfile
from pathlib import Path

from verificar_cli import ConferirExecucao, PastaAqui, Rodar

FIXTURES = PastaAqui.parent.parent / "benchmark" / "ia_first" / "fixtures_migrar"


def Copia(*Nomes):
    """Pasta temporaria com as fixtures de entrada pedidas."""
    Pasta = Path(tempfile.mkdtemp(prefix="cli_migrar_"))
    for Nome in Nomes:
        shutil.copyfile(FIXTURES / f"{Nome}.fj", Pasta / f"{Nome}.fj")
    return Pasta


def DocumentoDe(Rodada, Problemas):
    try:
        return json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Problemas.append(f"stdout nao e um documento JSON: {Rodada.stdout[:100]!r}")
        return None


def CasoMigrarDryRun(Forja, PastaSaidas):
    Problemas = []
    Pasta = Copia("rotulo_alloc")
    try:
        Antes = (Pasta / "rotulo_alloc.fj").read_bytes()
        Primeira = Rodar(Forja, ["migrar", str(Pasta / "rotulo_alloc.fj")])
        Segunda = Rodar(Forja, ["migrar", "--dry-run", "--json", str(Pasta / "rotulo_alloc.fj")])
        Problemas += ConferirExecucao(Primeira, 0)
        Doc = DocumentoDe(Primeira, Problemas)
        if Doc is not None and (Doc["total_trocas"] != 5 or Doc["arquivos_alterados"] != 0 or not Doc["linhas_iguais"] or Doc["arquivos"][0]["estado"] != "ok"):
            Problemas.append(f"dry-run fora do esperado: {Doc['total_trocas']} trocas, {Doc['arquivos_alterados']} alterados, estado {Doc['arquivos'][0]['estado']}")
        if Primeira.stdout != Segunda.stdout:
            Problemas.append("--dry-run explicito deveria dar os mesmos bytes do padrao")
        if (Pasta / "rotulo_alloc.fj").read_bytes() != Antes:
            Problemas.append("o dry-run gravou")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoMigrarAplica(Forja, PastaSaidas):
    Problemas = []
    Pasta = Copia("modulo_file", "misto")
    try:
        Rodada = Rodar(Forja, ["migrar", "--aplicar", str(Pasta)])
        Problemas += ConferirExecucao(Rodada, 0)
        Doc = DocumentoDe(Rodada, Problemas)
        if Doc is not None and (Doc["arquivos_alterados"] != 2 or Doc["total_trocas"] != 19):
            Problemas.append(f"--aplicar: {Doc['arquivos_alterados']} alterados e {Doc['total_trocas']} trocas, esperado 2 e 19")
        for Nome in ("modulo_file", "misto"):
            if (Pasta / f"{Nome}.fj").read_bytes() != (FIXTURES / f"{Nome}.novo.fj").read_bytes():
                Problemas.append(f"{Nome}.fj migrado difere do .novo.fj")
            Problemas += [f"check de {Nome}: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / f"{Nome}.fj")]), 0)]
        Segunda = DocumentoDe(Rodar(Forja, ["migrar", str(Pasta)]), Problemas)
        if Segunda is not None and Segunda["total_trocas"] != 0:
            Problemas.append("a 2a passada achou trocas (nao e idempotente)")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoMigrarNaoAnalisadoEUsoErrado(Forja, PastaSaidas):
    Problemas = []
    Pasta = Copia("erro_com_forma_antiga")
    try:
        Arquivo = Pasta / "erro_com_forma_antiga.fj"
        Antes = Arquivo.read_bytes()
        Rodada = Rodar(Forja, ["migrar", "--aplicar", str(Arquivo)])
        Problemas += ConferirExecucao(Rodada, 1)
        Doc = DocumentoDe(Rodada, Problemas)
        if Doc is not None and (Doc["arquivos"][0]["estado"] != "nao_analisado" or Doc["arquivos"][0]["diagnostico"] != "E0301" or Doc["arquivos_alterados"] != 0):
            Problemas.append(f"arquivo com erro fora do esperado: {Doc['arquivos'][0]}")
        if Arquivo.read_bytes() != Antes:
            Problemas.append("o arquivo com erro foi editado")
        for Argumentos in ([], ["--prova", "forja.exe", str(Arquivo)], ["--dry-run", "--aplicar", str(Arquivo)], [str(Pasta / "nao_existe.fj")]):
            Problemas += [f"migrar {' '.join(Argumentos)}: {P}" for P in ConferirExecucao(Rodar(Forja, ["migrar"] + Argumentos), 2)]
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas


def CasoMigrarComentarios(Forja, PastaSaidas):
    """IA1-L3d2: --comentarios reescreve o codigo Forja dentro de `//` (igual ao .novo.fj, 17 trocas, check 0, 2a passada 0) e a prosa vai para revisao_comentario sem edicao."""
    Problemas = []
    Pasta = Copia("comentario_codigo", "comentario_prosa")
    try:
        Prosa = (Pasta / "comentario_prosa.fj").read_bytes()
        Sem = DocumentoDe(Rodar(Forja, ["migrar", str(Pasta / "comentario_codigo.fj")]), Problemas)
        if Sem is not None and (Sem["total_trocas"] != 3 or "comentarios_revisao" in Sem):
            Problemas.append(f"sem --comentarios: {Sem['total_trocas']} trocas (esperado 3) ou campos de comentario presentes")
        Rodada = Rodar(Forja, ["migrar", "--comentarios", "--aplicar", str(Pasta)])
        Problemas += ConferirExecucao(Rodada, 0)
        Doc = DocumentoDe(Rodada, Problemas)
        if Doc is not None and (Doc["total_trocas"] != 17 or Doc["comentarios_reescritos"] != 14 or Doc["comentarios_revisao"] != 7):
            Problemas.append(f"--comentarios: {Doc['total_trocas']} trocas, {Doc['comentarios_reescritos']} reescritos, {Doc['comentarios_revisao']} revisoes (esperado 17, 14 e 7)")
        if (Pasta / "comentario_codigo.fj").read_bytes() != (FIXTURES / "comentario_codigo.novo.fj").read_bytes():
            Problemas.append("comentario_codigo.fj migrado difere do .novo.fj")
        if (Pasta / "comentario_prosa.fj").read_bytes() != Prosa:
            Problemas.append("a prosa foi editada")
        Problemas += [f"check: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "comentario_codigo.fj")]), 0)]
        Segunda = DocumentoDe(Rodar(Forja, ["migrar", "--comentarios", str(Pasta / "comentario_codigo.fj")]), Problemas)
        if Segunda is not None and Segunda["total_trocas"] != 0:
            Problemas.append("a 2a passada achou trocas (nao e idempotente)")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    return Problemas
