# verificar_spec.py -- metricas M-U1, M-U2 e M-U3 do IA1: a doc fonte (spec.txt, cartao.txt, mapa_spec.tsv) contra o binario
"""Confere a documentacao em language\\doc contra um forja.exe (copia congelada). Sem modelo; roda na reconferencia do main.
Uso:
    python -I verificar_spec.py --forja <forja.exe> [--doc <pasta>] [--saida-doc <pasta>] [--lang <pasta language>]
    python -I verificar_spec.py --selftest [--forja <forja.exe>]
Saida: uma linha JSON por metrica {id, valor, meta, verde}; exit 0 so se todas verdes (1 = alguma vermelha, 2 = uso).
  M-U1a blocos ```fj de cartao e spec compilam (ou dao o codigo da marca: Exxxx = 1o erro do check, Rxxxx = erro de execucao,
        exit=N = codigo de saida); bloco sem marca roda `test` (se tem teste) ou `run` (se tem proc main) e tem de sair 0
  M-U1b contradicao cruzada: codigos Exxxx/Rxxxx/Bxxxx/Gxxxx/Xxxxx citados nas docs que `forja explicar` desconhece (meta 0)
  M-U1c cabecalho (1a linha) de cartao e spec == `forja --versao`
  M-U1d MEM-LJ-SPEC, MEM-FORJA-GUIA-NUCLEO (e regra nativa e GUIA-05) em --saida-doc == o que gerar_doc_mem.py gera agora (so
        confere o arquivo gerado; quem grava no .db de .mem e o main, no z1; arquivo ausente = vermelho: rode gerar_doc_mem.py antes)
  M-U2a palavras-chave e nomes embutidos do lexer.c, embutidos de proc e modulos std (std\\*.fj) aparecem no cartao
  M-U2b codigos do mapa_spec.tsv apontam para secao que existe em spec e em cartao
  M-U2c cobertura do enum de diag.h: todo codigo do enum esta no mapa e no catalogo do binario (e vice-versa)
  M-U3a cartao <= 12.288 bytes . M-U3b secoes por TEMA (as 13 de TEMAS, mesma ordem em cartao e spec) . M-U3c 0 marcas vN.N no corpo
--selftest: planta uma contradicao por vez numa copia da doc (bloco que nao compila, codigo inexistente, cartao estourado, secao
  inexistente no mapa, marca vN.N, palavra-chave sumida, hash velho) e exige a metrica certa vermelha; a copia sem planta tem de ficar verde.
Versao: 0.1.1
Changelog: 0.1.1 - Portao Z z5: M-U2c aceita no mapa o codigo fora do enum de diag.h que o catalogo do binario explica (F0001).
0.1.0 - P-IA1-a04: primeira versao, a partir de vb.py e cov.py do a03 (blocos, cobertura de palavras, marcas vN.N); acrescenta
  contradicao cruzada por `forja explicar`, hash dos textos gerados, mapa x secoes, enum de diag.h e --selftest.
Verificacao: python -I verificar_spec.py --selftest
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I nao poe a pasta do script no caminho; gerar_doc_mem mora ao lado
import gerar_doc_mem as Gdm  # noqa: E402

LANG = Gdm.LANG
TEMAS = "formato declarar proc fluxo erro memoria colecoes texto foreign modulos testes saida cli".split()
LIMITE_CARTAO = 12288
WORKERS = 6
PADRAO_BLOCO = re.compile(r"^```fj([^\n]*)\n(.*?)^```", re.S | re.M)
PADRAO_CODIGO = re.compile(r"\b[EGRBX]\d{4}\b")
PADRAO_VERSAO = re.compile(r"forja \d+\.\d+\.\d+ \(Linguagem v\d+\.\d+\)")
PADRAO_MARCA_VERSAO = re.compile(r"\bv\d+\.\d+\b")
# Embutidos de proc: nao estao no lexer.c (a semantica os conhece); lista do cov.py do a03. LIMITE: lista a mao ate a b09 expor `forja spec --json`.
EMBUTIDOS_PROC = ("len append delete insert get get_ptr has remove frame_reset assert panic format format_c transmute slice_from "
                  "sqrt min max clamp abs transpose conj").split()


def Metrica(Id, Valor, Meta, Verde):
    """Uma linha de resultado: id da metrica, valor medido, meta e verde (bool)."""
    return {"id": Id, "valor": Valor, "meta": Meta, "verde": bool(Verde)}


def Percentual(Parte, Total):
    """Percentual com 1 casa; total 0 conta como 100 (nada a cobrir)."""
    return 100.0 if Total == 0 else round(100.0 * Parte / Total, 1)


def SecoesDe(Texto):
    """Nomes das secoes '== nome ==' na ordem em que aparecem."""
    return re.findall(r"^== (\w+) ==$", Texto, re.M)


def DiagnosticoDe(Saida):
    """Codigo do 1o objeto JSON da saida (diagnostico do check) ou None."""
    for Linha in Saida.splitlines():
        if Linha.strip().startswith("{"):
            return json.loads(Linha).get("codigo")
    return None


def ObservarExecucao(Forja, Arquivo, Cwd):
    """Roda o bloco e devolve o que se viu: 'exit=N' e o codigo R do stderr (ou None)."""
    Res = subprocess.run([str(Forja), "run", str(Arquivo)], capture_output=True, text=True, encoding="utf-8", errors="replace",
                         stdin=subprocess.DEVNULL, cwd=Cwd, timeout=60)
    Ultima = [Linha for Linha in Res.stderr.splitlines() if Linha.strip().startswith("{")]
    Codigo = json.loads(Ultima[-1]).get("codigo") if Ultima else None
    return Res.returncode, Codigo


def ObservarSemMarca(Forja, Arquivo, Codigo, Cwd):
    """Bloco sem marca: 'ok' se o teste (ou a execucao do main) sai 0; senao o que falhou."""
    if re.search(r'^test "', Codigo, re.M):
        Res = Gdm.ChamarForja(Forja, ["test", str(Arquivo)])
        return "ok" if Res.returncode == 0 else f"test_rc{Res.returncode}"
    if "proc main" in Codigo:
        Retorno, CodigoR = ObservarExecucao(Forja, Arquivo, Cwd)
        return "ok" if Retorno == 0 else f"run_rc{Retorno}:{CodigoR}"
    return "ok"


def Observar(Forja, Arquivo, Marca, Codigo):
    """O que o binario faz com o bloco, no vocabulario da marca (Exxxx, Rxxxx, exit=N ou ok)."""
    Cwd = str(Arquivo.parent)
    if Marca.startswith("R") or Marca.startswith("exit="):
        Retorno, CodigoR = ObservarExecucao(Forja, Arquivo, Cwd)
        return CodigoR if Marca.startswith("R") else f"exit={Retorno}"
    Diag = DiagnosticoDe(Gdm.ChamarForja(Forja, ["check", str(Arquivo)]).stdout)
    return Diag if Diag else ObservarSemMarca(Forja, Arquivo, Codigo, Cwd)


def ColetarBlocos(DocDir):
    """Lista (doc, indice, marca, codigo) de todos os blocos ```fj de cartao e spec."""
    Blocos = []
    for Nome in ("cartao.txt", "spec.txt"):
        Texto = Gdm.LerTexto(Path(DocDir) / Nome)
        for Indice, Achado in enumerate(PADRAO_BLOCO.finditer(Texto)):
            Marca = Achado.group(1).strip()
            Blocos.append((Nome.split(".")[0], Indice, Marca, Achado.group(2)))
    return Blocos


def MetricaBlocos(Amb):
    """M-U1a: 100% dos blocos ```fj dao o resultado da marca (sem marca = ok)."""
    Blocos = ColetarBlocos(Amb.doc)
    with tempfile.TemporaryDirectory(prefix="ia1_blocos_") as Nome:
        Pasta = Path(Nome)

        def Conferir(Bloco):
            Doc, Indice, Marca, Codigo = Bloco
            Arquivo = Pasta / f"{Doc}_{Indice:02d}.fj"
            Arquivo.write_bytes(Codigo.encode("utf-8"))
            return Observar(Amb.forja, Arquivo, Marca, Codigo) == (Marca or "ok")

        with ThreadPoolExecutor(max_workers=WORKERS) as Pool:
            Resultados = list(Pool.map(Conferir, Blocos))
    return Metrica("M-U1a", Percentual(sum(Resultados), len(Blocos)), 100.0, all(Resultados))


def MetricaContradicao(Amb):
    """M-U1b: todo codigo citado em cartao, spec e mapa existe no catalogo (`forja explicar`)."""
    Citados = set()
    for Nome in ("cartao.txt", "spec.txt", "mapa_spec.tsv"):
        Citados.update(PADRAO_CODIGO.findall(Gdm.LerTexto(Amb.doc / Nome)))
    Lista = sorted(Citados)
    with ThreadPoolExecutor(max_workers=Gdm.WORKERS) as Pool:
        Itens = list(Pool.map(lambda Cod: Gdm.ExplicarCodigo(Amb.forja, Cod), Lista))
    Fantasmas = [Cod for Cod, Item in zip(Lista, Itens) if Item is None]
    return Metrica("M-U1b", len(Fantasmas), 0, not Fantasmas) | ({"inexistentes": Fantasmas} if Fantasmas else {})


def MetricaCabecalho(Amb):
    """M-U1c: a 1a linha de cartao e spec cita exatamente a versao de `forja --versao`."""
    Versao = Gdm.VersaoDoForja(Amb.forja)
    Certos = 0
    for Nome in ("cartao.txt", "spec.txt"):
        Achado = PADRAO_VERSAO.search(Gdm.LerTexto(Amb.doc / Nome).split("\n", 1)[0])
        Certos += 1 if Achado and Achado.group(0) == Versao else 0
    return Metrica("M-U1c", f"{Certos}/2", "2/2", Certos == 2)


def MetricaHash(Amb):
    """M-U1d: os arquivos gerados em --saida-doc batem com o que gerar_doc_mem gera agora (sha256)."""
    Esperado = {Nome: Gdm.Sha256(Texto) for Nome, Texto in Gdm.Gerar(Amb.forja, Amb.doc).items()}
    Gravado = Gdm.LerHashesGravados(Amb.saida_doc)
    Iguais = 0 if Gravado is None else sum(1 for Nome in Esperado if Gravado[Nome] == Esperado[Nome])
    return Metrica("M-U1d", f"{Iguais}/{len(Esperado)}", f"{len(Esperado)}/{len(Esperado)}", Iguais == len(Esperado))


def NomesDoLexer(Amb):
    """Palavras-chave e nomes embutidos lidos dos dois vetores do lexer.c (fonte unica dos nomes)."""
    Fonte = Gdm.LerTexto(Amb.lang / "compiler" / "frontend" / "lexer.c")
    Nomes = []
    for Vetor in ("PALAVRAS_CHAVE", "NOMES_EMBUTIDOS"):
        Achado = re.search(Vetor + r"\[[^\]]*\]\s*=\s*\{(.*?)\};", Fonte, re.S)
        if not Achado:
            raise SystemExit(f"vetor {Vetor} nao achado em lexer.c")
        Nomes += re.findall(r'"(\w+)"', Achado.group(1))
    return Nomes


def MetricaPalavras(Amb):
    """M-U2a: 100% das palavras do lexer, embutidos de proc e modulos std aparecem no cartao."""
    Cartao = Gdm.LerTexto(Amb.doc / "cartao.txt")
    Palavras = NomesDoLexer(Amb) + EMBUTIDOS_PROC
    Faltam = [Pal for Pal in Palavras if not re.search(rf"(?<![A-Za-z0-9_]){re.escape(Pal)}(?![A-Za-z0-9_])", Cartao)]
    Modulos = ["std:" + Arq.stem for Arq in sorted((Amb.lang / "std").glob("*.fj"))]
    Faltam += [Mod for Mod in Modulos if Mod not in Cartao]
    Total = len(Palavras) + len(Modulos)
    return Metrica("M-U2a", Percentual(Total - len(Faltam), Total), 100.0, not Faltam) | ({"faltam": Faltam} if Faltam else {})


def LerMapa(Amb):
    """Pares (codigo, secao) do mapa_spec.tsv."""
    Linhas = Gdm.LerTexto(Amb.doc / "mapa_spec.tsv").splitlines()[1:]
    return [tuple(Linha.split("\t")[:2]) for Linha in Linhas if Linha.strip()]


def MetricaMapa(Amb):
    """M-U2b: toda secao apontada pelo mapa existe em spec e em cartao."""
    Existentes = set(SecoesDe(Gdm.LerTexto(Amb.doc / "spec.txt"))) & set(SecoesDe(Gdm.LerTexto(Amb.doc / "cartao.txt")))
    Pares = LerMapa(Amb)
    Ruins = [Cod for Cod, Sec in Pares if Sec not in Existentes]
    return Metrica("M-U2b", Percentual(len(Pares) - len(Ruins), len(Pares)), 100.0, not Ruins and Pares) | ({"sem_secao": Ruins} if Ruins else {})


def CodigosDoEnum(Amb):
    """Codigos do enum DiagCodigo em compiler\\common\\diag.h."""
    Fonte = Gdm.LerTexto(Amb.lang / "compiler" / "common" / "diag.h")
    Achado = re.search(r"typedef enum \{(.*?)\} DiagCodigo;", Fonte, re.S)
    if not Achado:
        raise SystemExit("enum DiagCodigo nao achado em diag.h")
    return PADRAO_CODIGO.findall(Achado.group(1))


def MetricaEnum(Amb):
    """M-U2c: cobertura do enum de diag.h pelo mapa, e do mapa pelo catalogo do binario."""
    Enum = set(CodigosDoEnum(Amb))
    Mapa = {Cod for Cod, _ in LerMapa(Amb)}
    with ThreadPoolExecutor(max_workers=Gdm.WORKERS) as Pool:
        Itens = list(Pool.map(lambda Cod: Gdm.ExplicarCodigo(Amb.forja, Cod), sorted(Enum)))
    SemCatalogo = [Cod for Cod, Item in zip(sorted(Enum), Itens) if Item is None]
    FaltamNoMapa = sorted(Enum - Mapa)
    Sobram = [Cod for Cod in sorted(Mapa - Enum) if Gdm.ExplicarCodigo(Amb.forja, Cod) is None]  # z5: codigo fora do enum (F0001) vale se o catalogo do binario o explica
    Verde = not (SemCatalogo or FaltamNoMapa or Sobram)
    Extra = {} if Verde else {"sem_catalogo": SemCatalogo, "faltam_no_mapa": FaltamNoMapa, "fora_do_enum": Sobram}
    return Metrica("M-U2c", Percentual(len(Enum & Mapa), len(Enum)), 100.0, Verde) | Extra


def MetricasCartao(Amb):
    """M-U3: tamanho do cartao, secoes por tema (iguais no spec) e 0 marcas vN.N no corpo."""
    Bruto = (Amb.doc / "cartao.txt").read_bytes()
    Cartao = Gdm.LerTexto(Amb.doc / "cartao.txt")
    Spec = Gdm.LerTexto(Amb.doc / "spec.txt")
    Corpo = Cartao.split("\n", 1)[1] if "\n" in Cartao else ""
    Marcas = PADRAO_MARCA_VERSAO.findall(Corpo)
    Temas = SecoesDe(Cartao) == TEMAS and SecoesDe(Spec) == TEMAS
    return [Metrica("M-U3a", len(Bruto), f"<= {LIMITE_CARTAO}", len(Bruto) <= LIMITE_CARTAO),
            Metrica("M-U3b", "sim" if Temas else "nao", "sim", Temas),
            Metrica("M-U3c", len(Marcas), 0, not Marcas)]


def Verificar(Amb):
    """Todas as metricas, na ordem M-U1a..M-U3c."""
    Lista = [MetricaBlocos(Amb), MetricaContradicao(Amb), MetricaCabecalho(Amb), MetricaHash(Amb),
             MetricaPalavras(Amb), MetricaMapa(Amb), MetricaEnum(Amb)]
    return Lista + MetricasCartao(Amb)


def Ambiente(Forja, Doc, SaidaDoc, Lang):
    """Contexto das metricas (caminhos como Path)."""
    return SimpleNamespace(forja=Path(Forja), doc=Path(Doc), saida_doc=Path(SaidaDoc), lang=Path(Lang))


def Imprimir(Metricas):
    """Uma linha JSON por metrica; devolve o exit code (0 so se todas verdes)."""
    for Item in Metricas:
        print(json.dumps(Item, ensure_ascii=False))
    return 0 if all(Item["verde"] for Item in Metricas) else 1


def Anexar(DocDir, Arquivo, Texto):
    """Planta: acrescenta Texto ao fim do arquivo da doc."""
    Alvo = Path(DocDir) / Arquivo
    Alvo.write_bytes(Alvo.read_bytes() + Texto.encode("utf-8"))


def Trocar(DocDir, Arquivo, De, Para):
    """Planta: troca todas as ocorrencias de De por Para (De tem de existir)."""
    Alvo = Path(DocDir) / Arquivo
    Texto = Alvo.read_bytes().decode("utf-8")
    assert De in Texto, f"planta invalida: {De!r} nao esta em {Arquivo}"
    Alvo.write_bytes(Texto.replace(De, Para).encode("utf-8"))


BLOCO_RUIM = '\n```fj\nproc main() {\n    let x: i32 = "texto"\n}\n```\n'
# (nome, metrica que tem de ficar vermelha, mutacao, gerar os textos DEPOIS da mutacao?)
PLANTAS = [
    ("sem planta (controle)", None, lambda Doc: None, True),
    ("bloco que nao compila", "M-U1a", lambda Doc: Anexar(Doc, "cartao.txt", BLOCO_RUIM), True),
    ("codigo inexistente E9999", "M-U1b", lambda Doc: Anexar(Doc, "cartao.txt", "\nCitado: E9999.\n"), True),
    ("cabecalho de outra versao", "M-U1c", lambda Doc: Trocar(Doc, "cartao.txt", "forja 0.19.2", "forja 0.18.1"), True),
    ("texto gerado velho (hash)", "M-U1d", lambda Doc: Anexar(Doc, "cartao.txt", "\nlinha nova\n"), False),
    ("palavra-chave sumida", "M-U2a", lambda Doc: Trocar(Doc, "cartao.txt", "union", "uniao"), True),
    ("secao inexistente no mapa", "M-U2b", lambda Doc: Trocar(Doc, "mapa_spec.tsv", "E0001\tformato", "E0001\tnao_existe"), True),
    ("cartao estourado", "M-U3a", lambda Doc: Anexar(Doc, "cartao.txt", "\n" + "x" * 1300 + "\n"), True),
    ("marca vN.N no corpo", "M-U3c", lambda Doc: Anexar(Doc, "cartao.txt", "\nIntroduzido na v1.9.\n"), True),
]


def RodarPlanta(Forja, Planta):
    """Roda Verificar numa copia da doc com a planta; devolve {id: verde}."""
    _, _, Mutar, GerarDepois = Planta
    with tempfile.TemporaryDirectory(prefix="ia1_planta_") as Nome:
        Doc = Path(Nome) / "doc"
        shutil.copytree(Gdm.DOC_PADRAO, Doc)
        if GerarDepois:
            Mutar(Doc)
        Gdm.Gravar(Gdm.Gerar(Forja, Doc), Path(Nome) / "saida")
        if not GerarDepois:
            Mutar(Doc)
        return {Item["id"]: Item["verde"] for Item in Verificar(Ambiente(Forja, Doc, Path(Nome) / "saida", LANG))}


def PlantaAcusada(Planta, Verdes):
    """Controle: tudo verde; planta: so a metrica esperada fica vermelha (as demais verdes)."""
    Esperada = Planta[1]
    Vermelhas = sorted(Id for Id, Verde in Verdes.items() if not Verde)
    return Vermelhas == ([Esperada] if Esperada else [])


def Selftest(Forja):
    """Controle verde e cada planta acusada pela metrica certa; devolve o exit code."""
    Falhas = 0
    for Planta in PLANTAS:
        Verdes = RodarPlanta(Forja, Planta)
        Certo = PlantaAcusada(Planta, Verdes)
        Falhas += 0 if Certo else 1
        Vermelhas = sorted(Id for Id, Verde in Verdes.items() if not Verde)
        print(json.dumps({"planta": Planta[0], "esperada": Planta[1], "vermelhas": Vermelhas, "acusada": Certo}, ensure_ascii=False))
    print(json.dumps({"id": "selftest", "valor": f"{len(PLANTAS) - Falhas}/{len(PLANTAS)}", "meta": f"{len(PLANTAS)}/{len(PLANTAS)}",
                      "verde": Falhas == 0}))
    return 0 if Falhas == 0 else 1


def Principal():
    """Entrada: --selftest ou verificacao da doc contra o binario."""
    Args = argparse.ArgumentParser(description="Verifica a doc fonte (M-U1, M-U2, M-U3) contra um forja.exe")
    Args.add_argument("--forja", type=Path, default=Gdm.FORJA_PADRAO)
    Args.add_argument("--doc", type=Path, default=Gdm.DOC_PADRAO)
    Args.add_argument("--saida-doc", type=Path, default=Gdm.SAIDA_PADRAO)
    Args.add_argument("--lang", type=Path, default=LANG)
    Args.add_argument("--selftest", action="store_true")
    Opcoes = Args.parse_args()
    if Opcoes.selftest:
        return Selftest(Opcoes.forja)
    return Imprimir(Verificar(Ambiente(Opcoes.forja, Opcoes.doc, Opcoes.saida_doc, Opcoes.lang)))


if __name__ == "__main__":
    sys.exit(Principal())
