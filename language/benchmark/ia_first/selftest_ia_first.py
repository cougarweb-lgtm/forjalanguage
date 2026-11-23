# -*- coding: utf-8 -*-
"""selftest_ia_first.py - selftest SEM modelo do benchmark IA FIRST (P-IA1-a07, contrato 4.8). Entra na bateria como suite "ia_first".

Versao 0.1.3
Changelog:
  0.1.3 (14/11/2026) Portao Z: contaminacao ganha 6 casos para o caminho em Bash (contaminacao.py 0.1.3): `sed -i 's/let p/var p/'` e `ls build/tmp/x`
        sao VALIDOS; `cp prog.fj /tmp/x`, `cd /var/log` e `cat "/tmp/x"` seguem INVALIDOS.
  0.1.2 (11/11/2026) P-IA1-a10: teste "stdin_aberto_nao_trava" (ia.py 0.2.1): wrapper com stdin = pipe aberto sem EOF termina; com pipe
        fechado e dados tambem.
  0.1.1 (11/11/2026) P-IA1-a10: contaminacao ganha 2 casos para o `..` em Bash: `cd <celula>/entrada && ... ../prog.fj` e VALIDO;
        `cd <celula> && cat ../x` e INVALIDO (contaminacao.py 0.1.1).
  0.1.0 (11/11/2026) primeira versao. Agente falso que grava a referencia -> pass@1 100% e idas 1 (Forja e C); agente falso que
        deixa o erro conhecido (Cx01) -> pass@1 0, o codigo certo no histograma (E0301 e -Wint-conversion); contaminacao plantada
        (leitura fora, ferramenta mcp, Bash fora, marca injetada, Grep sem path) -> celula invalida, refeita uma vez e na 2a PARA;
        transcript achado pelo caminho da celula (r1 nao pega r10); LIMITE na 6a ida; recusa em celula L; espera do lock;
        relatorio e notas; metricas_mecanicas imprime so JSON; lint_casa.

Uso:
  python selftest_ia_first.py [--forja <forja.exe>]     (padrao: %TEMP%\\forja_ia1_base\\forja.exe)
Saida: uma linha JSON por teste {teste, ok, detalhe} e uma linha final {"selftest": {...}}. Exit: 0 tudo ok, 1 algum falhou.
Escrita: so uma pasta temporaria propria, apagada no fim. Reversao: apagar este arquivo. Depende de: Python 3.9+, gcc no PATH.
"""
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import coleta_ia_first
import contaminacao
import lint_casa
from ia_comum import FORJA_CONGELADA, LerJson, LerJsonl, RAIZ

PY = [sys.executable, "-I"]


def Cli(Script, *Argumentos):
    P = subprocess.run(PY + [str(RAIZ / Script)] + list(Argumentos), capture_output=True, text=True, timeout=600)
    return P.returncode, P.stdout, P.stderr


def Preparar(Base, Rodada, Forja, Escopo):
    """Escopo = (tarefas separadas por virgula, linguas separadas por virgula)."""
    Rc, Out, Err = Cli("rodar_ia_first.py", "preparar", "--base", str(Base), "--rodada", Rodada, "--forja", str(Forja),
                       "--tarefas", Escopo[0], "--linguas", Escopo[1], "--conjuntos", "E,L,Cx")
    if Rc != 0:
        raise RuntimeError("preparar falhou: " + Err + Out)
    return LerJson(Base / "rodadas" / Rodada / "rodada.json")


def Referencia(Cel):
    Dir = RAIZ / "referencias" / Cel["lingua"] / (Cel["tarefa"] if Cel["conjunto"] == "E" else "%s/%s" % (Cel["conjunto"], Cel["tarefa"]))
    return Dir / ("prog" + Cel["extensao"])


def Agente(Entrada, GravarReferencia):
    """Agente falso: opcionalmente grava a referencia em prog.<ext> e faz UMA ida pelo wrapper (como o bench_ia faria)."""
    Celula = Path(Entrada["pasta"])
    Cel = LerJson(Celula / "celula.json")
    Prog = Celula / ("prog" + Cel["extensao"])
    if GravarReferencia:
        shutil.copyfile(Referencia(Cel), Prog)
    Comando = "check" if not GravarReferencia else "run"
    return subprocess.run(PY + [str(Celula / "ia.py"), Comando, str(Prog)], capture_output=True, text=True, timeout=120)


def Resultados(Base, Rodada):
    return {R["id"]: R for R in (LerJson(P) for P in sorted((Base / "rodadas" / Rodada / "celulas").glob("*/resultado.json")))}


def TesteReferencias(Base, Forja):
    Rodada = Preparar(Base, "S1", Forja, ("E01,Cx01", "forja,c"))
    Alvo = [E for E in Rodada["celulas"] if E["conjunto"] in ("E", "Cx")]
    for Entrada in Alvo:
        Agente(Entrada, True)
    Rc, Out, Err = Cli("rodar_ia_first.py", "coletar", "--base", str(Base), "--rodada", "S1")
    Res = Resultados(Base, "S1")
    Ok = len(Res) == len(Alvo) and all(R["pass1"] and R["idas"] == 1 and R["idas_ate_verde"] == 1 and R["verde_final"] for R in Res.values())
    return Ok, "%d celulas, ids: %s" % (len(Res), sorted(Res))


def TesteErroConhecido(Base, Forja):
    Rodada = Preparar(Base, "S2", Forja, ("Cx01", "forja,c"))
    for Entrada in Rodada["celulas"]:
        Agente(Entrada, False)
    Cli("rodar_ia_first.py", "coletar", "--base", str(Base), "--rodada", "S2")
    Res = Resultados(Base, "S2")
    Forja_, C = Res["Cx_Cx01_forja_P0_r1"], Res["Cx_Cx01_c_c_r1"]
    Ok = (not Forja_["pass1"] and "E0301" in Forja_["codigos"] and not C["pass1"] and "[-Wint-conversion]" in C["codigos"]
          and Forja_["idas_ate_verde"] is None)
    return Ok, "forja %s | c %s" % (Forja_["codigos"], C["codigos"])


# ---------- transcripts plantados ----------

def Registro(Tipo, Conteudo, Indice, Usage=None):
    Mensagem = {"id": "m%d" % Indice, "role": Tipo, "content": Conteudo}
    if Usage:
        Mensagem["usage"] = Usage
    return {"type": Tipo, "uuid": "u%d" % Indice, "timestamp": "2026-11-11T10:00:%02dZ" % Indice, "message": Mensagem}


def Uso(Nome, Entrada, Indice):
    Bloco = [{"type": "tool_use", "name": Nome, "input": Entrada}]
    return Registro("assistant", Bloco, Indice, {"input_tokens": 10, "cache_creation_input_tokens": 0, "cache_read_input_tokens": 100, "output_tokens": 5})


def Pedido(Celula):
    return Registro("user", "Celula: %s\nLinguagem: Forja" % str(Celula), 0)


def Gravar(Destino, Registros):
    Destino.parent.mkdir(parents=True, exist_ok=True)
    Destino.write_text("\n".join(json.dumps(R) for R in Registros) + "\n", encoding="utf-8")
    return Destino


def VariantesPlantadas(Celula):
    Dentro = str(Celula / "pedido.txt")
    Limpa = [Uso("Read", {"file_path": Dentro}, 1), Uso("Bash", {"command": 'python "%s/ia.py" run prog.fj' % str(Celula).replace("\\", "/")}, 2)]
    Fora = lambda Extra: [Uso("Read", {"file_path": Dentro}, 1)] + Extra  # lint-ok: iaf-nome-generico montador curto
    Raiz = str(Celula).replace("\\", "/")
    LimpaCd = [Uso("Read", {"file_path": Dentro}, 1),
               Uso("Bash", {"command": 'cd "%s/entrada" && python "%s/ia.py" run ../prog.fj -- numeros.txt' % (Raiz, Raiz)}, 2)]
    LimpaSed = [Uso("Read", {"file_path": Dentro}, 1),
                Uso("Bash", {"command": 'cd "%s" && sed -i \'s/let p/var p/\' prog.fj && python ia.py run prog.fj' % Raiz}, 2)]
    LimpaRelativo = [Uso("Read", {"file_path": Dentro}, 1), Uso("Bash", {"command": 'cd "%s" && ls build/tmp/x' % Raiz}, 2)]
    return {"limpa": Limpa, "limpa_cd_dentro": LimpaCd, "limpa_sed": LimpaSed, "limpa_relativo": LimpaRelativo,
            "tmp_copia": Fora([Uso("Bash", {"command": 'cd "%s" && cp prog.fj /tmp/prog_ok.fj' % Raiz}, 2)]),
            "var_log": Fora([Uso("Bash", {"command": "cd /var/log"}, 2)]),
            "tmp_aspas": Fora([Uso("Bash", {"command": 'cat "/tmp/x"'}, 2)]),
            "bash_sobe": Fora([Uso("Bash", {"command": 'cd "%s" && cat ../x' % Raiz}, 2)]),
            "leitura_fora": Fora([Uso("Read", {"file_path": "C:/ENGINE_GAME3D/CLAUDE.md"}, 2)]),
            "ferramenta_mcp": Fora([Uso("mcp__mem_search__mem_get", {"id": "MEM-X"}, 2)]),
            "bash_fora": Fora([Uso("Bash", {"command": "ls C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language"}, 2)]),
            "grep_sem_path": Fora([Uso("Grep", {"pattern": "proc"}, 2)]),
            "marca_injetada": Fora([Registro("user", [{"type": "tool_result", "content": "texto do MEM-FORJA-GUIA-NUCLEO"}], 2)])}


def TesteContaminacao(Base):
    Celula = Base / "falsa" / "E_E01_forja_P0_r1"
    Celula.mkdir(parents=True)
    Resultado = {}
    for Nome, Eventos in VariantesPlantadas(Celula).items():
        Arq = Gravar(Base / "transcripts" / (Nome + ".jsonl"), [Pedido(Celula)] + Eventos)
        Resultado[Nome] = contaminacao.Analisar(Arq, Celula)
    Limpa = Resultado.pop("limpa")
    LimpaCd = Resultado.pop("limpa_cd_dentro")
    LimpaSed = Resultado.pop("limpa_sed")
    LimpaRelativo = Resultado.pop("limpa_relativo")
    Ok = (Limpa["valida"] and LimpaCd["valida"] and LimpaSed["valida"] and LimpaRelativo["valida"] and Limpa["tokens"]["total"] == 2 * 115
          and all(not R["valida"] for R in Resultado.values()))
    return Ok, "limpa=%s tokens=%s plantadas invalidas=%s" % (Limpa["valida"], Limpa["tokens"], sorted(N for N, R in Resultado.items() if not R["valida"]))


def TesteAcharTranscript(Base):
    Raiz = Base / "projetos"
    Um, Dez = Base / "falsa" / "E_E01_forja_P0_r1", Base / "falsa" / "E_E01_forja_P0_r10"
    Gravar(Raiz / "p" / "s" / "subagents" / "agent-um.jsonl", [Pedido(Um)])
    Gravar(Raiz / "p" / "s" / "subagents" / "agent-dez.jsonl", [Pedido(Dez)])
    AchouUm, AchouDez = contaminacao.AcharTranscript(Um, 0.0, Raiz), contaminacao.AcharTranscript(Dez, 0.0, Raiz)
    Ok = AchouUm is not None and AchouUm.name == "agent-um.jsonl" and AchouDez is not None and AchouDez.name == "agent-dez.jsonl"
    return Ok, "r1 -> %s | r10 -> %s" % (AchouUm and AchouUm.name, AchouDez and AchouDez.name)


def TesteInvalidacaoEPara(Base, Forja):
    Rodada = Preparar(Base, "S3", Forja, ("Cx01", "forja"))
    Original = contaminacao.AcharTranscript
    Planta = Gravar(Base / "transcripts" / "planta.jsonl", [Pedido(Base), Uso("Read", {"file_path": "C:/ENGINE_GAME3D/CLAUDE.md"}, 1)])
    contaminacao.AcharTranscript = lambda *Args, **Kw: Planta  # lint-ok: iaf-nome-generico substituto de teste
    try:
        Agente(Rodada["celulas"][0], True)
        Primeira = coleta_ia_first.Coletar(Base / "rodadas" / "S3")
        Novas = LerJson(Base / "rodadas" / "S3" / "rodada.json")["celulas"]
        Agente(Novas[-1], True)
        Segunda = coleta_ia_first.Coletar(Base / "rodadas" / "S3")
    finally:
        contaminacao.AcharTranscript = Original
    Ok = (not Primeira[0]["valida"] and Primeira[0]["refeita_na_tentativa"] == 2 and len(Novas) == 2 and Novas[-1]["id"].endswith("_t2")
          and not Segunda[0]["valida"] and Segunda[0].get("PARA") is True)
    return Ok, "1a invalida refeita=%s; 2a PARA=%s" % (Primeira[0].get("refeita_na_tentativa"), Segunda[0].get("PARA"))


# ---------- wrapper ----------

def TesteLimiteERecusa(Base):
    Celula = Base / "rodadas" / "S1" / "celulas" / "E_E01_forja_P0_r1"
    Prog = str(Celula / "prog.fj")
    Codigos = [subprocess.run(PY + [str(Celula / "ia.py"), "check", Prog], capture_output=True, text=True).returncode for _ in range(5)]
    Linhas = LerJsonl(Celula / "idas.jsonl")
    Tipos = [L["tipo"] for L in Linhas]
    Ok = Codigos[-1] == 99 and Tipos.count("ida") == 5 and Tipos.count("limite") >= 1
    return Ok, "rc=%s tipos=%s" % (Codigos, {T: Tipos.count(T) for T in set(Tipos)})


def TesteRecusaL(Base, Forja):
    Rodada = Preparar(Base, "S4", Forja, ("L01", "forja"))
    Celula = Path([E for E in Rodada["celulas"] if E["conjunto"] == "L"][0]["pasta"])
    P = subprocess.run(PY + [str(Celula / "ia.py"), "run", str(Celula / "prog.fj")], capture_output=True, text=True)
    Tipos = [L["tipo"] for L in LerJsonl(Celula / "idas.jsonl")]
    return P.returncode == 2 and Tipos == ["proibido"], "rc=%d tipos=%s" % (P.returncode, Tipos)


def TesteLock(Base):
    Celula = Base / "rodadas" / "S2" / "celulas" / "Cx_Cx01_forja_P0_r1"
    Lock = Base / "trava.lock"
    Dados = LerJson(Celula / "celula.json")
    Dados["lock"] = str(Lock)
    (Celula / "celula.json").write_text(json.dumps(Dados), encoding="utf-8")
    Lock.write_text("medindo", encoding="utf-8")
    Proc = subprocess.Popen(PY + [str(Celula / "ia.py"), "check", str(Celula / "prog.fj")], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(2.0)
    Esperou = Proc.poll() is None
    Lock.unlink()
    Proc.communicate(timeout=60)
    return Esperou and Proc.returncode is not None, "esperou o lock: %s" % Esperou


def TesteStdinAberto(Base):
    """ia.py nao pode prender em stdin que e pipe aberto sem EOF (ferramenta Bash do agente); pipe que fecha continua valendo."""
    Copia = Base / "copia_stdin"
    shutil.copytree(Base / "rodadas" / "S2" / "celulas" / "Cx_Cx01_forja_P0_r1", Copia)
    (Copia / "idas.jsonl").unlink(missing_ok=True)
    Comando = PY + [str(Copia / "ia.py"), "check", str(Copia / "prog.fj")]
    Aberto = subprocess.Popen(Comando, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        Aberto.wait(timeout=30)
        Terminou = True
    except subprocess.TimeoutExpired:
        Aberto.kill()
        Terminou = False
    Fechado = subprocess.run(Comando, input=b"dados\n", capture_output=True, timeout=60)
    return Terminou and Fechado.returncode in (0, 1), "pipe aberto terminou=%s | pipe fechado rc=%s" % (Terminou, Fechado.returncode)


# ---------- relatorio, notas, metricas, lint ----------

def TesteRelatorioENotas(Base):
    Rc, Out, Err = Cli("rodar_ia_first.py", "relatorio", "--base", str(Base), "--rodada", "S1", "--json")
    Linhas = [json.loads(L) for L in Out.splitlines()]
    Metricas = {L["id"]: L for L in Linhas if "id" in L}
    Arq = Base / "bench.jsonl"
    Arq.write_text(Out, encoding="utf-8")
    Rc2, Out2, Err2 = Cli("notas.py", "--bench", str(Arq), "--json")
    Notas = json.loads(Out2)["notas"]
    Ok = (Rc == 0 and Metricas["B-W2"]["verde"] is True and Metricas["B-W5"]["verde"] is True and Metricas["B-W7"]["verde"] is None
          and "B-W2" not in Notas["ESCREVER"]["pendentes"] and Notas["ESCREVER"]["verdes"] >= 2)
    return Ok, "B-W2=%s B-W5=%s B-W7=%s escrever=%s" % (Metricas["B-W2"]["verde"], Metricas["B-W5"]["verde"], Metricas["B-W7"]["verde"], Notas["ESCREVER"]["nota"])


def TesteMetricasMecanicas(Forja):
    Rc, Out, Err = Cli("metricas_mecanicas.py", "--forja", str(Forja), "--json", "--sem-fixtures")
    Linhas = [json.loads(L) for L in Out.splitlines()]
    Chaves = all(set(L) >= {"id", "valor", "meta", "verde"} for L in Linhas)
    Ok = len(Linhas) == 29 and Chaves and len({L["id"] for L in Linhas}) == 29 and Rc in (0, 1)
    return Ok, "%d linhas, rc=%d, pendentes=%d" % (len(Linhas), Rc, sum(1 for L in Linhas if L["verde"] is None))


def TesteLint():
    Rc, Out, Err = Cli("lint_casa.py", "--selftest")
    return Rc == 0, Out.strip()[:160]


def Executar(Forja):
    Base = Path(tempfile.mkdtemp(prefix="a07_self_"))
    Testes = [("referencias_dao_pass1_100", lambda: TesteReferencias(Base, Forja)), ("erro_conhecido_no_histograma", lambda: TesteErroConhecido(Base, Forja)),
              ("limite_na_6a_ida", lambda: TesteLimiteERecusa(Base)), ("recusa_em_celula_L", lambda: TesteRecusaL(Base, Forja)),
              ("espera_do_lock", lambda: TesteLock(Base)), ("stdin_aberto_nao_trava", lambda: TesteStdinAberto(Base)),
              ("contaminacao_plantada", lambda: TesteContaminacao(Base)),
              ("transcript_pelo_caminho", lambda: TesteAcharTranscript(Base)), ("invalida_refaz_e_para", lambda: TesteInvalidacaoEPara(Base, Forja)),
              ("relatorio_e_notas", lambda: TesteRelatorioENotas(Base)), ("metricas_mecanicas_json", lambda: TesteMetricasMecanicas(Forja)),
              ("lint_casa", TesteLint)]
    Falhas = 0
    try:
        for Nome, Funcao in Testes:
            Ok, Detalhe = Rodar(Funcao)
            Falhas += not Ok
            print(json.dumps({"teste": Nome, "ok": Ok, "detalhe": Detalhe}, ensure_ascii=False))
    finally:
        shutil.rmtree(Base, ignore_errors=True)
    print(json.dumps({"selftest": {"testes": len(Testes), "falhas": Falhas}}))
    return 1 if Falhas else 0


def Rodar(Funcao):
    try:
        Ok, Detalhe = Funcao()
    except Exception as Erro:  # um teste quebrado nao esconde os outros
        return False, "excecao %s: %s" % (type(Erro).__name__, Erro)
    return bool(Ok), Detalhe


def Principal():
    Analisador = argparse.ArgumentParser(description="Selftest do benchmark IA FIRST (sem modelo)")
    Analisador.add_argument("--forja", default=str(FORJA_CONGELADA))
    Args = Analisador.parse_args()
    if not Path(Args.forja).is_file():
        print("uso: --forja <forja.exe> (nao achei %s)" % Args.forja, file=sys.stderr)
        return 2
    return Executar(Path(Args.forja))


if __name__ == "__main__":
    sys.exit(Principal())
