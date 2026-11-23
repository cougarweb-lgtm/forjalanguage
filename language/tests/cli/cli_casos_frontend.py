"""Casos de cli do front-end: test, run, check, ast e explicar (catalogo de codigos E/R/B/G), modulos e vazamento em run.

Importado pelo motor verificar_cli.py (que monta a lista CASOS na ordem original); as funcoes comuns vem de la.

Versao: 1.11.0
Changelog: 1.11.0 - S4 P1 (D1): CasoModulosTeste espera o teste do modulo (roda e falha: exit 1, "3 testes, 1 falharam", FALHOU [mods/com_teste_cli] teste do modulo).
           1.10.1 - IA1-b14: CasoSugestaoE0301 ignora tambem o campo relacionados (antes do spec no diagnostico sem correcao) ao conferir que o extra vem logo depois de forma_certa.
           1.10.0 - IA1-I1: os casos de forja ast esperam ast_versao 1.0.0 (ids, fim e ref; antes 0.11.0).
           1.9.2 - IA1-b09: CasoSugestaoE0301 e CasoCadeiaE0706 ignoram o campo spec (agora no fim de todo diagnostico) ao conferir que o extra vem logo depois de forma_certa.
           1.9.1 - IA1-b06: CasoParecidoE0201 aceita o pacote do erro (correcao, confianca...) depois do extra "parecidos"; o extra continua logo depois de forma_certa.
           1.9.0- Linguagem v1.12 P21v-b: CasoRunProcValorControlado vira CasoRunProcValor (proc_valor.fj roda e imprime 8, 10, 3, 14); mesma contagem.
           1.8.0 -Linguagem v1.12 P21v-a: CasoExplicarV112ProcValor, CasoCheckProcValor e CasoRunProcValorControlado (fixture proc_valor.fj).
           1.7.0 - Linguagem v1.11 L16b: CasoExplicarR0011.
           1.6.0 -Linguagem v1.11 L16a: CasoTesteFiltroUmDeTres, CasoTesteFiltroZero, CasoTesteJsonExpect e CasoTesteJsonR0002 (fixtures tres_testes.fj, json_expect.fj, json_estouro.fj).
           1.5.0 - Linguagem v1.11 L7: CasoMaxErrosUm e CasoMaxErrosZero (fixture multi_erros.fj; check --max-erros).
           1.4.0 - Linguagem v1.11 L6: CasoColunaFimE0201 (fixture coluna_fim_e0201.fj) e CasoStdinOk/CasoStdinErro/CasoStdinImport (check --stdin).
           1.3.0 - Linguagem v1.11 L5: CasoParecidoE0201 (fixture campo_parecido.fj, extra "parecidos").
           1.2.0 - Linguagem v1.11 L2: CasoCadeiaE0706 (fixture fuga_cadeia.fj).
           1.1.0 - Linguagem v1.11 L1: CasoExplicarE0330 e CasoSugestaoE0301 (fixture sugestao_real_em_inteiro.fj).
           1.0.0 - P9a: casos movidos do verificar_cli.py 1.21.0 sem mudar nenhum.
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import os

from verificar_cli import ConferirExecucao, ConferirJsonCodigo, LinhaJson, PastaAqui, Rodar, UltimoJson


def CasoTestePassa(Forja, PastaSaidas):
    return ConferirExecucao(Rodar(Forja, ["test", "dois_testes.fj"]), 0, "2 testes, 0 falharam")


def CasoTesteFalha(Forja, PastaSaidas):
    return ConferirExecucao(Rodar(Forja, ["test", "teste_falha.fj"]), 1, "1 testes, 1 falharam", "FALHOU expect falso")


def CasoSemTeste(Forja, PastaSaidas):
    return ConferirExecucao(Rodar(Forja, ["test", "sem_teste.fj"]), 0, "0 testes, 0 falharam")


def CasoRunPanico(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["run", "panico.fj"])
    Problemas = ConferirExecucao(Rodada, 101, None, "7")
    if '"R0003"' not in Rodada.stderr:
        Problemas.append("stderr sem o JSON R0003 do panic")
    return Problemas


def CasoRunRelease(Forja, PastaSaidas):
    return ConferirExecucao(Rodar(Forja, ["run", "--release", "sem_teste.fj"]), 0, "1")


def CasoExplicarE0301(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "E0301"])
    Dados = LinhaJson(Rodada.stdout)
    Problemas = ConferirExecucao(Rodada, 0)
    if Dados is None or Dados.get("codigo") != "E0301" or "forma_certa" not in Dados:
        Problemas.append(f"explicar E0301 sem JSON valido: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoExplicarR0001(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "R0001"])
    Problemas = ConferirExecucao(Rodada, 0)
    return Problemas + ConferirJsonCodigo(Rodada.stdout, "R0001")


def CasoExplicarDesconhecido(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["explicar", "E9999"])
    return ConferirExecucao(Rodada, 2) + ([] if "desconhecido" in Rodada.stderr else ["stderr sem 'desconhecido'"])


def CasoExplicarNovos(Forja, PastaSaidas):
    """Os codigos novos da v1.2 existem no catalogo e trazem forma_certa."""
    Problemas = []
    for Codigo in ("E0303", "E0304", "E0305", "E0420", "E0609"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Dados = LinhaJson(Rodada.stdout)
        Problemas += ConferirExecucao(Rodada, 0)
        if Dados is None or Dados.get("codigo") != Codigo or not Dados.get("forma_certa"):
            Problemas.append(f"explicar {Codigo} sem JSON com forma_certa: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoExplicarV13(Forja, PastaSaidas):
    """Os codigos novos da v1.3 existem no catalogo e trazem explicacao e forma_certa."""
    Problemas = []
    for Codigo in ("E0306", "E0307", "E0706", "R0005"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Dados = LinhaJson(Rodada.stdout)
        Problemas += ConferirExecucao(Rodada, 0)
        if Dados is None or Dados.get("codigo") != Codigo or not Dados.get("forma_certa") or not Dados.get("explicacao"):
            Problemas.append(f"explicar {Codigo} sem JSON com explicacao e forma_certa: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoMainDevolveErro(Forja, PastaSaidas):
    """'main -> ! E' que devolve erro: o programa sai com 1 e o stderr traz o JSON R0004."""
    Rodada = Rodar(Forja, ["run", "main_erro.fj"])
    Problemas = ConferirExecucao(Rodada, 1, None, "7")
    return Problemas + ConferirJsonCodigo(Rodada.stderr, "R0004")


def CasoForeignAst(Forja, PastaSaidas):
    """forja ast: ast_versao 0.7.0, no 'link' com a biblioteca e proc com "externo": true e sem corpo."""
    Rodada = Rodar(Forja, ["ast", "foreign_libm.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Ast = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Problemas + ["forja ast nao devolveu JSON"]
    Nos = Ast.get("declaracoes", [])
    Links = [N for N in Nos if N.get("no") == "link"]
    Procs = [N for N in Nos if N.get("no") == "proc"]
    if Ast.get("ast_versao") != "1.0.0":
        Problemas.append(f"ast_versao {Ast.get('ast_versao')!r}, esperado '1.0.0'")
    if [N.get("biblioteca") for N in Links] != ["m"]:
        Problemas.append(f"nos link {Links!r}, esperado um com biblioteca 'm'")
    Externos = [N for N in Procs if N.get("externo") is True]
    if len(Externos) != 2 or any("corpo" in N for N in Externos):
        Problemas.append("esperados 2 procs externos, sem 'corpo'")
    return Problemas


def CasoExplicarV14(Forja, PastaSaidas):
    """Os codigos novos da v1.4 existem no catalogo e trazem explicacao e forma_certa."""
    Problemas = []
    for Codigo in ("E0904", "E0905", "E0906", "B0004"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Dados = LinhaJson(Rodada.stdout)
        Problemas += ConferirExecucao(Rodada, 0)
        if Dados is None or Dados.get("codigo") != Codigo or not Dados.get("forma_certa") or not Dados.get("explicacao"):
            Problemas.append(f"explicar {Codigo} sem JSON com explicacao e forma_certa: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoIndiceConstanteDobrado(Forja, PastaSaidas):
    """a[POS + 1] em [3]int: a semantica nao dobra conta (check passa); o lowering dobra e da E0421 no build."""
    Check = Rodar(Forja, ["check", "indice_const.fj"])
    Build = Rodar(Forja, ["build", "indice_const.fj", "-o", str(PastaSaidas / "indice_const.exe")])
    return ConferirExecucao(Check, 0) + ConferirExecucao(Build, 1) + ConferirJsonCodigo(Build.stdout, "E0421")


def NosDoJson(Dado):
    """Todos os dicts da arvore JSON (a AST), em profundidade."""
    if isinstance(Dado, dict):
        yield Dado
        for Valor in Dado.values():
            yield from NosDoJson(Valor)
    elif isinstance(Dado, list):
        for Item in Dado:
            yield from NosDoJson(Item)


def CasoArrayAst(Forja, PastaSaidas):
    """forja ast de array_ast.fj: ast 0.5.0, nos indice/fatia (um 'inteira')/literal_array, for por_ref com indice, TipoRef array [N]."""
    Rodada = Rodar(Forja, ["ast", "array_ast.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Ast = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Problemas + ["forja ast nao devolveu JSON"]
    Nos = list(NosDoJson(Ast))
    Tipos = {N.get("no") for N in Nos}
    if Ast.get("ast_versao") != "1.0.0":
        Problemas.append(f"ast_versao {Ast.get('ast_versao')!r}, esperado '1.0.0'")
    for Esperado in ("indice", "fatia", "literal_array"):
        if Esperado not in Tipos:
            Problemas.append(f"AST sem nos '{Esperado}'")
    if not any(N.get("no") == "fatia" and N.get("limite") == "inteira" for N in Nos):
        Problemas.append("AST sem fatia com limite 'inteira'")
    if not any(N.get("por_ref") is True and "indice" in N and "colecao" in N for N in Nos):
        Problemas.append("AST sem for com por_ref true e indice")
    if not any(N.get("forma") == "array" and N.get("tamanho") == "N" for N in Nos):
        Problemas.append("AST sem TipoRef forma 'array' com tamanho 'N'")
    return Problemas


def CasoExplicarV15(Forja, PastaSaidas):
    """Os codigos novos da v1.5 existem no catalogo e trazem explicacao e forma_certa."""
    Problemas = []
    for Codigo in ("E0308", "E0309", "E0421", "R0006"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Dados = LinhaJson(Rodada.stdout)
        Problemas += ConferirExecucao(Rodada, 0)
        if Dados is None or Dados.get("codigo") != Codigo or not Dados.get("forma_certa") or not Dados.get("explicacao"):
            Problemas.append(f"explicar {Codigo} sem JSON com explicacao e forma_certa: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoMemoriaAst(Forja, PastaSaidas):
    """forja ast de memoria_ast.fj: ast 0.7.0, um "tipo" com tipo_escrito de forma 'dinamico' e outro com forma 'nome'."""
    Rodada = Rodar(Forja, ["ast", "memoria_ast.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Ast = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Problemas + ["forja ast nao devolveu JSON"]
    if Ast.get("ast_versao") != "1.0.0":
        Problemas.append(f"ast_versao {Ast.get('ast_versao')!r}, esperado '1.0.0'")
    Escritos = [N["tipo_escrito"] for N in NosDoJson(Ast) if N.get("no") == "tipo" and isinstance(N.get("tipo_escrito"), dict)]
    # Tipo simples (Grifo) nao traz "forma": a AST so grava "nome".
    if not any(E.get("forma") == "dinamico" for E in Escritos):
        Problemas.append("AST sem no \"tipo\" cujo tipo_escrito tem forma 'dinamico'")
    if not any("forma" not in E and "nome" in E for E in Escritos):
        Problemas.append("AST sem no \"tipo\" cujo tipo_escrito e um nome simples")
    return Problemas


def CasoDeleteDuploDev(Forja, PastaSaidas):
    """run (dev): delete duas vezes no mesmo ponteiro sai 101 com R0008, depois de imprimir 10 e 1."""
    Rodada = Rodar(Forja, ["run", "delete_duplo.fj"])
    Problemas = ConferirExecucao(Rodada, 101)
    if Rodada.stdout.replace("\r\n", "\n") != "10\n1\n":
        Problemas.append(f"stdout {Rodada.stdout!r}, esperado '10\\n1\\n'")
    if "R0008" not in Rodada.stderr:
        Problemas.append(f"stderr sem R0008: {Rodada.stderr.strip()[:160]!r}")
    return Problemas


def CasoListaIndiceForaDev(Forja, PastaSaidas):
    """run (dev): l[1] numa lista de 1 elemento sai 101 com R0006, depois de imprimir 1 e 2."""
    Rodada = Rodar(Forja, ["run", "lista_indice_fora.fj"])
    Problemas = ConferirExecucao(Rodada, 101)
    if Rodada.stdout.replace("\r\n", "\n") != "1\n2\n":
        Problemas.append(f"stdout {Rodada.stdout!r}, esperado '1\\n2\\n'")
    if "R0006" not in Rodada.stderr:
        Problemas.append(f"stderr sem R0006: {Rodada.stderr.strip()[:160]!r}")
    return Problemas


def CasoExplicarV16(Forja, PastaSaidas):
    """Os codigos novos da v1.6 existem no catalogo e trazem explicacao e forma_certa."""
    Problemas = []
    for Codigo in ("E0310", "E0311", "R0007", "R0008"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Dados = LinhaJson(Rodada.stdout)
        Problemas += ConferirExecucao(Rodada, 0)
        if Dados is None or Dados.get("codigo") != Codigo or not Dados.get("forma_certa") or not Dados.get("explicacao"):
            Problemas.append(f"explicar {Codigo} sem JSON com explicacao e forma_certa: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoV17Ast(Forja, PastaSaidas):
    """forja ast de v17.fj: ast 0.7.0, no com forma 'opcional', union, let_multiplo, ftexto e a chave 'retornos'."""
    Rodada = Rodar(Forja, ["ast", "v17.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Ast = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Problemas + ["forja ast nao devolveu JSON"]
    if Ast.get("ast_versao") != "1.0.0":
        Problemas.append(f"ast_versao {Ast.get('ast_versao')!r}, esperado '1.0.0'")
    Nos = list(NosDoJson(Ast))
    if not any(N.get("forma") == "opcional" for N in Nos):
        Problemas.append("AST sem no com \"forma\": \"opcional\"")
    for Tipo in ("union", "let_multiplo", "ftexto"):
        if not any(N.get("no") == Tipo for N in Nos):
            Problemas.append(f"AST sem no {Tipo!r}")
    if not any("retornos" in N for N in Nos):
        Problemas.append("AST sem a chave \"retornos\"")
    return Problemas


def CasoExplicarV17(Forja, PastaSaidas):
    """Os codigos novos da v1.7 existem no catalogo e trazem explicacao e forma_certa."""
    Problemas = []
    for Codigo in ("E0312", "E0313", "E0314", "E0315"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Dados = LinhaJson(Rodada.stdout)
        Problemas += ConferirExecucao(Rodada, 0)
        if Dados is None or Dados.get("codigo") != Codigo or not Dados.get("forma_certa") or not Dados.get("explicacao"):
            Problemas.append(f"explicar {Codigo} sem JSON com explicacao e forma_certa: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoModulosPanico(Forja, PastaSaidas):
    """run de modulos_panico.fj: imprime 5, o panic do modulo sai 101 com R0003 apontando mods/falha_cli.fj linha 3."""
    Rodada = Rodar(Forja, ["run", "modulos_panico.fj"])
    Problemas = ConferirExecucao(Rodada, 101)
    if Rodada.stdout.replace("\r\n", "\n") != "5\n":
        Problemas.append(f"stdout {Rodada.stdout!r}, esperado '5\\n'")
    Dados = UltimoJson(Rodada.stderr)
    if Dados is None or Dados.get("codigo") != "R0003" or Dados.get("linha") != 3 \
            or not str(Dados.get("arquivo", "")).replace("\\", "/").endswith("mods/falha_cli.fj"):
        Problemas.append(f"stderr sem R0003 em mods/falha_cli.fj linha 3: {Rodada.stderr.strip()[-200:]!r}")
    return Problemas


def CasoModulosTeste(Forja, PastaSaidas):
    """test de modulos_teste.fj: os 2 testes da raiz e o teste do modulo rodam (S4 P1, D1); o do modulo falha e sai 1 com o rotulo qualificado."""
    return ConferirExecucao(Rodar(Forja, ["test", "modulos_teste.fj"]), 1, "3 testes, 1 falharam",
                            "FALHOU [mods/com_teste_cli] teste do modulo")


def CasoModulosErros(Forja, PastaSaidas):
    """check: import de si mesmo = E1002 1:8; erro de tipo dentro do modulo = E0301 em mods/com_erro.fj 2:18."""
    Rodada = Rodar(Forja, ["check", "importa_a_si.fj"])
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "E1002")
    Dados = LinhaJson(Rodada.stdout) or {}
    if (Dados.get("linha"), Dados.get("coluna")) != (1, 8):
        Problemas.append(f"E1002 em {Dados.get('linha')}:{Dados.get('coluna')}, esperado 1:8")
    Rodada = Rodar(Forja, ["check", "modulo_com_erro.fj"])
    Problemas += ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "E0301")
    Dados = LinhaJson(Rodada.stdout) or {}
    if (Dados.get("linha"), Dados.get("coluna")) != (2, 18):
        Problemas.append(f"E0301 em {Dados.get('linha')}:{Dados.get('coluna')}, esperado 2:18")
    if not str(Dados.get("arquivo", "")).replace("\\", "/").endswith("mods/com_erro.fj"):
        Problemas.append(f"E0301 em arquivo {Dados.get('arquivo')!r}, esperado terminando em mods/com_erro.fj")
    return Problemas


def CasoVazamentoRun(Forja, PastaSaidas):
    """run (dev) de vazamento.fj: imprime 7 e sai 101 com R0009 na linha 6; --release sai 0 sem checar."""
    Rodada = Rodar(Forja, ["run", "vazamento.fj"])
    Problemas = ConferirExecucao(Rodada, 101)
    if Rodada.stdout.replace("\r\n", "\n") != "7\n":
        Problemas.append(f"stdout {Rodada.stdout!r}, esperado '7\\n'")
    Dados = UltimoJson(Rodada.stderr)
    Causa = "vazamento: 1 blocos (4 bytes) alocados aqui seguem vivos no fim do programa"
    if Dados is None or Dados.get("codigo") != "R0009" or Dados.get("linha") != 6 or Dados.get("causa") != Causa:
        Problemas.append(f"stderr sem R0009 linha 6 com a causa do vazamento: {Rodada.stderr.strip()[-250:]!r}")
    Release = Rodar(Forja, ["run", "--release", "vazamento.fj"])
    Problemas += ConferirExecucao(Release, 0)
    if Release.stdout.replace("\r\n", "\n") != "7\n":
        Problemas.append(f"--release: stdout {Release.stdout!r}, esperado '7\\n'")
    return Problemas


def CasoAstImport(Forja, PastaSaidas):
    """forja ast de import_ast.fj: ast 0.8.0, nos import (std:mem como mem; mods/falha_cli como f) e um no com modulo 'f'."""
    Rodada = Rodar(Forja, ["ast", "import_ast.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Ast = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Problemas + ["forja ast nao devolveu JSON"]
    if Ast.get("ast_versao") != "1.0.0":
        Problemas.append(f"ast_versao {Ast.get('ast_versao')!r}, esperado '1.0.0'")
    Nos = list(NosDoJson(Ast))
    Imports = {(N.get("caminho"), N.get("apelido")) for N in Nos if N.get("no") == "import"}
    for Esperado in (("std:mem", "mem"), ("mods/falha_cli", "f")):
        if Esperado not in Imports:
            Problemas.append(f"AST sem no import {Esperado!r}: {sorted(Imports, key=str)!r}")
    if not any(N.get("modulo") == "f" for N in Nos):
        Problemas.append("AST sem no com \"modulo\": \"f\"")
    return Problemas


def CasoExplicarV18(Forja, PastaSaidas):
    """Os codigos novos da v1.8 existem no catalogo (explicar devolve JSON com o codigo pedido)."""
    Problemas = []
    for Codigo in ("E0316", "E0317", "E1001", "E1002", "E1003", "E1004", "R0009"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Problemas += ConferirExecucao(Rodada, 0)
        Dados = LinhaJson(Rodada.stdout)
        if Dados is None or Dados.get("codigo") != Codigo:
            Problemas.append(f"explicar {Codigo} sem JSON com o codigo: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoEngineSemVariavel(Forja, PastaSaidas):
    """import "engine:render" sem FORJA_ENGINE: E1001 em 1:8 com FORJA_ENGINE na causa."""
    Ambiente = {Chave: Valor for Chave, Valor in os.environ.items() if Chave != "FORJA_ENGINE"}
    Rodada = Rodar(Forja, ["check", "usa_engine.fj"], Ambiente)
    Problemas = ConferirExecucao(Rodada, 1)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != "E1001" or Dados.get("linha") != 1 or Dados.get("coluna") != 8:
        return Problemas + [f"stdout sem E1001 em 1:8: {Rodada.stdout.strip()[:160]!r}"]
    if "FORJA_ENGINE" not in Dados.get("causa", ""):
        Problemas.append(f"causa sem FORJA_ENGINE: {Dados.get('causa')!r}")
    return Problemas


def CasoExplicarV18b(Forja, PastaSaidas):
    """Os codigos novos da v1.8b existem no catalogo (explicar devolve JSON com o codigo pedido)."""
    Problemas = []
    for Codigo in ("E0907", "E0908", "R0010", "G0001", "G0026", "G0101", "G0105"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Problemas += ConferirExecucao(Rodada, 0)
        Dados = LinhaJson(Rodada.stdout)
        if Dados is None or Dados.get("codigo") != Codigo:
            Problemas.append(f"explicar {Codigo} sem JSON com o codigo: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def BuscarFormas(No, Achados):
    """Junta, em Achados, todo dict do JSON que tem a chave "forma" (qualquer profundidade)."""
    if isinstance(No, dict):
        if "forma" in No:
            Achados.append(No)
        for Valor in No.values():
            BuscarFormas(Valor, Achados)
    elif isinstance(No, list):
        for Item in No:
            BuscarFormas(Item, Achados)


def TemIndice2(No):
    """True se algum dict do JSON tem a chave "indice2" (m[i, j])."""
    if isinstance(No, dict):
        return "indice2" in No or any(TemIndice2(V) for V in No.values())
    if isinstance(No, list):
        return any(TemIndice2(V) for V in No)
    return False


def CasoAstV19(Forja, PastaSaidas):
    """forja ast de ast_v19.fj: ast 0.10.0 com mapa, bit_set, soa dinamica, matriz 4x4 e indice2."""
    Rodada = Rodar(Forja, ["ast", "ast_v19.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Ast = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Problemas + ["forja ast nao devolveu JSON"]
    if Ast.get("ast_versao") != "1.0.0":
        Problemas.append(f"ast_versao {Ast.get('ast_versao')!r}, esperado '1.0.0'")
    Formas = []
    BuscarFormas(Ast, Formas)
    Mapas = [F for F in Formas if F.get("forma") == "mapa" and F.get("chave", {}).get("nome") == "str"
             and F.get("valor", {}).get("nome") == "int"]
    if len(Mapas) < 1:
        Problemas.append("sem TipoRef forma \"mapa\" com chave str e valor int")
    if not [F for F in Formas if F.get("forma") == "bit_set"]:
        Problemas.append("sem TipoRef forma \"bit_set\"")
    if not [F for F in Formas if F.get("forma") == "soa" and F.get("alvo", {}).get("forma") == "dinamico"]:
        Problemas.append("sem TipoRef forma \"soa\" com alvo dinamico")
    if not [F for F in Formas if F.get("forma") == "matriz" and F.get("linhas") == 4 and F.get("colunas") == 4]:
        Problemas.append("sem TipoRef forma \"matriz\" com linhas 4 e colunas 4")
    if not TemIndice2(Ast):
        Problemas.append("sem no de indice com \"indice2\"")
    return Problemas


def CasoExplicarV19(Forja, PastaSaidas):
    """Os codigos novos da v1.9 existem no catalogo (explicar devolve JSON com o codigo pedido)."""
    Problemas = []
    for Codigo in ("E0318", "E0319", "E0320", "E0321"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Problemas += ConferirExecucao(Rodada, 0)
        Dados = LinhaJson(Rodada.stdout)
        if Dados is None or Dados.get("codigo") != Codigo:
            Problemas.append(f"explicar {Codigo} sem JSON com o codigo: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoExplicarE0330(Forja, PastaSaidas):
    """v1.11 L1: explicar E0330 devolve JSON com o codigo, tipo "tipo", explicacao e forma_certa (a forma cita o 'as')."""
    Rodada = Rodar(Forja, ["explicar", "E0330"])
    Problemas = ConferirExecucao(Rodada, 0)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != "E0330" or Dados.get("tipo") != "tipo":
        return Problemas + [f"explicar E0330 sem JSON com codigo e tipo: {Rodada.stdout.strip()[:120]!r}"]
    if "16777216" not in Dados.get("explicacao", "") or "as f32" not in Dados.get("forma_certa", ""):
        Problemas.append(f"explicar E0330 sem explicacao e forma_certa do contrato: {Rodada.stdout.strip()[:160]!r}")
    return Problemas


def CasoExplicarR0011(Forja, PastaSaidas):
    """v1.11 L16b: explicar R0011 devolve JSON com o codigo, tipo "runtime", explicacao (excecao do processador dentro de test) e forma_certa (cita 'as ^T')."""
    Rodada = Rodar(Forja, ["explicar", "R0011"])
    Problemas = ConferirExecucao(Rodada, 0)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != "R0011" or Dados.get("tipo") != "runtime":
        return Problemas + [f"explicar R0011 sem JSON com codigo e tipo runtime: {Rodada.stdout.strip()[:120]!r}"]
    if "excecao do processador" not in Dados.get("explicacao", "") or "as ^T" not in Dados.get("forma_certa", ""):
        Problemas.append(f"explicar R0011 sem explicacao e forma_certa do contrato: {Rodada.stdout.strip()[:160]!r}")
    return Problemas


def CasoSugestaoE0301(Forja, PastaSaidas):
    """v1.11 L1: check de 'let x: i32 = 1.5' da E0301 com o extra "sugestao": "1.5 as i32", depois de forma_certa."""
    Rodada = Rodar(Forja, ["check", "sugestao_real_em_inteiro.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != "E0301":
        return Problemas + [f"check nao devolveu 1 linha JSON E0301: {Rodada.stdout.strip()[:120]!r}"]
    if Dados.get("sugestao") != "1.5 as i32":
        Problemas.append(f"sugestao {Dados.get('sugestao')!r}, esperado '1.5 as i32'")
    if [K for K in Dados if K not in ("spec", "spec_ref", "relacionados", "correcao", "confianca", "id")][-2:] != ["forma_certa", "sugestao"]:  # IA1-b09: o spec fecha a linha; b14: relacionados vem antes dele; Z z5: o pacote (correcao, confianca, id) sai em todo diagnostico
        Problemas.append(f"o extra sugestao nao vem logo depois de forma_certa: {list(Dados)!r}")
    return Problemas


def CasoCadeiaE0706(Forja, PastaSaidas):
    """v1.11 L2: check de fuga_cadeia.fj (x -> p -> return) da E0706 com a causa e o extra "cadeia" logo depois de forma_certa."""
    Rodada = Rodar(Forja, ["check", "fuga_cadeia.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != "E0706":
        return Problemas + [f"check nao devolveu 1 linha JSON E0706: {Rodada.stdout.strip()[:120]!r}"]
    if Dados.get("cadeia") != ["x (local, 2:9)", "p (3:9)", "return (4:5)"]:
        Problemas.append(f"cadeia {Dados.get('cadeia')!r}, esperado ['x (local, 2:9)', 'p (3:9)', 'return (4:5)']")
    if not Dados.get("causa", "").endswith("x -> p -> return"):
        Problemas.append(f"causa sem a cadeia 'x -> p -> return': {Dados.get('causa')!r}")
    if [K for K in Dados if K not in ("spec", "spec_ref", "relacionados", "correcao", "confianca", "id")][-2:] != ["forma_certa", "cadeia"]:  # IA1-b09: o spec fecha a linha; Z z5: o pacote sai em todo diagnostico
        Problemas.append(f"o extra cadeia nao vem logo depois de forma_certa: {list(Dados)!r}")
    return Problemas


def CasoParecidoE0201(Forja, PastaSaidas):
    """v1.11 L5: check de campo_parecido.fj (g.vda) da E0201 com "(parecido: 'vida')" na causa e o extra "parecidos" depois de forma_certa."""
    Rodada = Rodar(Forja, ["check", "campo_parecido.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != "E0201":
        return Problemas + [f"check nao devolveu 1 linha JSON E0201: {Rodada.stdout.strip()[:120]!r}"]
    if Dados.get("parecidos") != ["vida"]:
        Problemas.append(f"parecidos {Dados.get('parecidos')!r}, esperado ['vida']")
    if not Dados.get("causa", "").endswith("(parecido: 'vida')"):
        Problemas.append(f"causa sem '(parecido: 'vida')': {Dados.get('causa')!r}")
    Chaves = list(Dados)
    if "forma_certa" not in Chaves or Chaves[Chaves.index("forma_certa") + 1 : Chaves.index("forma_certa") + 2] != ["parecidos"]:  # IA1-b06: o pacote (correcao...) vem depois do extra
        Problemas.append(f"o extra parecidos nao vem logo depois de forma_certa: {Chaves!r}")
    return Problemas


def CasoColunaFimE0201(Forja, PastaSaidas):
    """v1.11 L6: check de coluna_fim_e0201.fj: zzzz na linha 3 coluna 11 tem coluna_fim 15, e o campo fica entre coluna e causa."""
    Rodada = Rodar(Forja, ["check", "coluna_fim_e0201.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Dados = LinhaJson(Rodada.stdout) or {}
    if Dados.get("codigo") != "E0201" or (Dados.get("linha"), Dados.get("coluna"), Dados.get("coluna_fim")) != (3, 11, 15):
        Problemas.append(f"E0201 sem linha 3, coluna 11 e coluna_fim 15: {Rodada.stdout.strip()[:160]!r}")
    if list(Dados)[4:8] != ["coluna", "coluna_fim", "trecho", "causa"]:  # Z z5: o trecho sai em todo diagnostico, entre coluna_fim e causa
        Problemas.append(f"coluna_fim fora da ordem coluna, coluna_fim, trecho, causa: {list(Dados)!r}")
    return Problemas


def CasoStdinOk(Forja, PastaSaidas):
    """v1.11 L6: check --stdin de usa_soma.fj (texto pela entrada padrao, nome do disco) sai 0 e nao imprime nada."""
    Caminho = PastaAqui / "usa_soma.fj"
    Rodada = Rodar(Forja, ["check", "--stdin", str(Caminho)], Entrada=Caminho.read_text(encoding="utf-8"))
    Problemas = ConferirExecucao(Rodada, 0)
    if Rodada.stdout.strip() or Rodada.stderr.strip():
        Problemas.append(f"check --stdin imprimiu: {(Rodada.stdout + Rodada.stderr).strip()[:160]!r}")
    return Problemas


def CasoStdinErro(Forja, PastaSaidas):
    """v1.11 L6: check --stdin de campo_parecido.fj com um caminho que nao existe no disco: E0201 em 8:13 e arquivo = esse caminho."""
    Caminho = PastaAqui / "stdin_fingido.fj"
    Rodada = Rodar(Forja, ["check", "--stdin", str(Caminho)], Entrada=(PastaAqui / "campo_parecido.fj").read_text(encoding="utf-8"))
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "E0201")
    Dados = LinhaJson(Rodada.stdout) or {}
    if (Dados.get("linha"), Dados.get("coluna"), Dados.get("coluna_fim")) != (8, 13, 16):
        Problemas.append(f"E0201 sem 8:13 e coluna_fim 16: {Rodada.stdout.strip()[:160]!r}")
    if not str(Dados.get("arquivo", "")).replace("\\", "/").endswith("/stdin_fingido.fj"):
        Problemas.append(f"arquivo {Dados.get('arquivo')!r}, esperado o caminho dado ao --stdin")
    return Problemas


def CasoStdinImport(Forja, PastaSaidas):
    """v1.11 L6: check --stdin de modulo_com_erro.fj: o import mods/com_erro le do disco, relativo ao <caminho>; E0301 em mods/com_erro.fj 2:18."""
    Caminho = PastaAqui / "modulo_com_erro.fj"
    Rodada = Rodar(Forja, ["check", "--stdin", str(Caminho)], Entrada=Caminho.read_text(encoding="utf-8"))
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, "E0301")
    Dados = LinhaJson(Rodada.stdout) or {}
    if (Dados.get("linha"), Dados.get("coluna")) != (2, 18):
        Problemas.append(f"E0301 em {Dados.get('linha')}:{Dados.get('coluna')}, esperado 2:18")
    if not str(Dados.get("arquivo", "")).replace("\\", "/").endswith("mods/com_erro.fj"):
        Problemas.append(f"E0301 em arquivo {Dados.get('arquivo')!r}, esperado terminando em mods/com_erro.fj")
    return Problemas


def CasoMaxErrosUm(Forja, PastaSaidas):
    """v1.11 L7: check --max-erros 1 de multi_erros.fj (3 erros em 3 procs) devolve 1 linha JSON, o 1o erro (E0301 em 3:18), exit 1."""
    Rodada = Rodar(Forja, ["check", "--max-erros", "1", "multi_erros.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Linhas = [L for L in Rodada.stdout.splitlines() if L.strip()]
    if len(Linhas) != 1:
        return Problemas + [f"--max-erros 1: {len(Linhas)} linhas no stdout, esperado 1"]
    Dados = LinhaJson(Rodada.stdout) or {}
    if (Dados.get("codigo"), Dados.get("linha"), Dados.get("coluna")) != ("E0301", 3, 18):
        Problemas.append(f"--max-erros 1 sem o E0301 de 3:18: {Linhas[0][:160]!r}")
    return Problemas


def CasoMaxErrosZero(Forja, PastaSaidas):
    """v1.11 L7: check --max-erros 0 (fora de 1..100) e uso errado: exit 2, nada no stdout."""
    Rodada = Rodar(Forja, ["check", "--max-erros", "0", "multi_erros.fj"])
    Problemas = ConferirExecucao(Rodada, 2)
    if Rodada.stdout.strip():
        Problemas.append(f"--max-erros 0 imprimiu no stdout: {Rodada.stdout.strip()[:120]!r}")
    if "--max-erros" not in Rodada.stderr:
        Problemas.append(f"--max-erros 0 sem o motivo no stderr: {Rodada.stderr.strip()[:160]!r}")
    return Problemas


def CasoExplicarV112ProcValor(Forja, PastaSaidas):
    """v1.12 P21v-a: explicar E0909, E0910 e E0911 existem no catalogo (JSON com o codigo pedido e forma_certa nao vazia)."""
    Problemas = []
    for Codigo in ("E0909", "E0910", "E0911"):
        Rodada = Rodar(Forja, ["explicar", Codigo])
        Problemas += ConferirExecucao(Rodada, 0)
        Dados = LinhaJson(Rodada.stdout)
        if Dados is None or Dados.get("codigo") != Codigo or not Dados.get("forma_certa"):
            Problemas.append(f"explicar {Codigo} sem JSON com codigo e forma_certa: {Rodada.stdout.strip()[:120]!r}")
    return Problemas


def CasoCheckProcValor(Forja, PastaSaidas):
    """v1.12 P21v-a: check de proc_valor.fj (proc em let/var, parametro, retorno e chamada indireta) sai 0 e mudo."""
    Rodada = Rodar(Forja, ["check", "proc_valor.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    if Rodada.stdout.strip() or Rodada.stderr.strip():
        Problemas.append(f"check de proc_valor.fj nao ficou mudo: {Rodada.stdout.strip()[:120]!r}{Rodada.stderr.strip()[:120]!r}")
    return Problemas


def CasoRunProcValor(Forja, PastaSaidas):
    """v1.12 P21v-b: run de proc_valor.fj (V_PROC em let/var/parametro/retorno e IR_CHAMADA_IND) sai 0 e imprime 8, 10, 3, 14."""
    Rodada = Rodar(Forja, ["run", "proc_valor.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    if Rodada.stdout.replace("\r\n", "\n") != "8\n10\n3\n14\n":
        Problemas.append(f"run de proc_valor.fj imprimiu {Rodada.stdout[:80]!r}{Rodada.stderr.strip()[:120]!r}, esperado 8, 10, 3, 14")
    return Problemas


def CasoAstV18b(Forja, PastaSaidas):
    """forja ast de ast_v18b.fj: ast 0.9.0, struct opaco Janela externo, membro de enum com "valor" -4 e tipo proc com params e retorno."""
    Rodada = Rodar(Forja, ["ast", "ast_v18b.fj"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Ast = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Problemas + ["forja ast nao devolveu JSON"]
    if Ast.get("ast_versao") != "1.0.0":
        Problemas.append(f"ast_versao {Ast.get('ast_versao')!r}, esperado '1.0.0'")
    Decls = Ast.get("declaracoes", [])
    Janela = [N for N in Decls if N.get("no") == "struct" and N.get("externo") is True]
    if len(Janela) != 1:
        Problemas.append(f"esperada 1 decl struct com \"externo\": true (Janela): {Janela!r}")
    Cores = [N for N in Decls if N.get("no") == "enum"]
    Membros = Cores[0].get("membros", []) if Cores else []
    if len(Membros) != 2 or Membros[1].get("valor") != -4:
        Problemas.append(f"2o membro de Cor sem \"valor\": -4: {Membros!r}")
    Fechar = [N for N in Decls if N.get("no") == "proc" and N.get("externo") is True]
    Params = Fechar[0].get("params", []) if Fechar else []
    Tipo = Params[1].get("tipo", {}) if len(Params) == 2 else {}
    if Tipo.get("forma") != "proc" or len(Tipo.get("params", [])) != 2 or not Tipo.get("retorno"):
        Problemas.append(f"2o parametro de ao_fechar sem forma proc, 2 params e retorno: {Tipo!r}")
    return Problemas


def CasoTesteFiltroUmDeTres(Forja, PastaSaidas):
    """L16a: test --filtro dobro roda so 'dobro certo' dos 3 testes; os outros 2 contam como filtrados."""
    return ConferirExecucao(Rodar(Forja, ["test", "tres_testes.fj", "--filtro", "dobro"]), 0,
                            "1 testes, 0 falharam, 2 filtrados", "ok   dobro certo\n")


def CasoTesteFiltroZero(Forja, PastaSaidas):
    """L16a: filtro que nao casa nenhum titulo (e e sensivel a caixa: 'DOBRO'): 0 testes, exit 0 e o resumo."""
    Problemas = []
    for Trecho in ("nenhum", "DOBRO"):
        Rodada = Rodar(Forja, ["test", "tres_testes.fj", "--filtro", Trecho])
        Problemas += [f"{Trecho}: {P}" for P in ConferirExecucao(Rodada, 0, "0 testes, 0 falharam, 3 filtrados")]
        if "ok   " in Rodada.stdout:
            Problemas.append(f"{Trecho}: algum teste rodou: {Rodada.stdout!r}")
    return Problemas


def LinhasJson(Texto):
    """Cada linha nao vazia do texto como JSON; None se alguma nao for."""
    try:
        return [json.loads(L) for L in Texto.splitlines() if L.strip()]
    except json.JSONDecodeError:
        return None


def CasoTesteJsonExpect(Forja, PastaSaidas):
    """L16a: test --json com 1 teste que passa e 1 que falha em 2 expects: so o 1o expect vai ao JSON; ultima linha e o resumo."""
    Rodada = Rodar(Forja, ["test", "json_expect.fj", "--json"])
    Esperado = [
        {"teste": "passa", "resultado": "ok"},
        {"teste": "falha expect","resultado": "falhou", "codigo": "R0003", "arquivo": "json_expect.fj", "linha": 10, "causa": "expect falhou: 1 == 2"},
        {"resumo": {"total": 2, "ok": 1, "falhou": 1, "filtrados": 0}},
    ]
    Problemas = ConferirExecucao(Rodada, 1)
    if LinhasJson(Rodada.stdout) != Esperado:
        Problemas.append(f"stdout {Rodada.stdout!r}, esperado as linhas {Esperado!r}")
    return Problemas


def CasoTesteJsonR0002(Forja, PastaSaidas):
    """L16a: test --json com estouro de inteiro dentro de teste: o R0002 vira codigo/arquivo/linha/causa da linha do teste; --filtro tira o outro (filtrados 1)."""
    Rodada = Rodar(Forja, ["test", "json_estouro.fj", "--json", "--filtro", "estoura"])
    Esperado = [
        {"teste": "soma estoura", "resultado": "falhou", "codigo": "R0002", "arquivo": "json_estouro.fj", "linha": 2,
         "causa": "estouro de inteiro em soma i32: 2147483647 e 1"},
        {"resumo": {"total": 2, "ok": 0, "falhou": 1, "filtrados": 1}},
    ]
    Problemas = ConferirExecucao(Rodada, 1)
    if LinhasJson(Rodada.stdout) != Esperado:
        Problemas.append(f"stdout {Rodada.stdout!r}, esperado as linhas {Esperado!r}")
    return Problemas
