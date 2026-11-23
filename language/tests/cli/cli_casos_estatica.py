"""Casos de cli dos diagnosticos de biblioteca ESTATICA do backend x64: X0103, X0105 (ar sem indice), X0106, X0107 e X0108.

Cada caso monta na hora, na pasta de saidas, o .fj e a(s) biblioteca(s) com o gcc e o ar do PC (o mesmo precedente do CasoX0103BibliotecaEstatica):
build --backend x64 tem de sair com exit 1, UMA linha JSON do codigo, sem pacote de relato, interno:false, e nenhum exe gravado.

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la.

Versao: 1.1.0
Changelog: 1.1.0 - M3b P23y-2: +1 caso (X0109 mais de 256 nomes importados de DLL).
           1.0.0 - M3b P23g: +5 casos (X0103 arquivo que nao e biblioteca, X0105 ar sem indice, X0106 simbolo em duas estaticas, X0107 secao .tls, X0108 dependencia do .drectve ausente).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import subprocess

from verificar_cli import ConferirExecucao, ConferirJsonCodigo, LinhaJson, Rodar

FONTE_FJ = "foreign {\n%s    proc fx_soma(a: i32, b: i32) -> i32\n}\n\nproc main() {\n    print(fx_soma(1, b = 2))\n}\n"
SOMA_C = "int fx_soma(int a, int b) { return a + b; }\n"
# Secoes escritas a mao no objeto (o gcc do PC usa TLS emulado e nao emite .tls nem .drectve de -l sozinho).
TLS_C = SOMA_C + '__asm__(".section .tls,\\"dw\\"\\n\\t.long 1\\n\\t.text");\n'
DRECTVE_C = SOMA_C + '__asm__(".section .drectve,\\"n\\"\\n\\t.ascii \\" -lfx_dep_ausente\\"\\n\\t.text");\n'


def PastaDoCaso(PastaSaidas, Nome):
    """Subpasta propria do caso dentro da pasta de saidas (que os casos do cli dividem)."""
    Pasta = PastaSaidas / Nome
    Pasta.mkdir()
    return Pasta


def CriarEstatica(Pasta, Nome, CodigoC, FlagsAr="rcs"):
    """<Pasta>\\lib<Nome>.a com um membro <Nome>.o compilado de CodigoC; devolve a lista de problemas (vazia = criada)."""
    Fonte, Objeto = Pasta / f"{Nome}.c", Pasta / f"{Nome}.o"
    Fonte.write_text(CodigoC, encoding="utf-8", newline="\n")
    Passos = [["gcc", "-c", str(Fonte), "-o", str(Objeto)], ["ar", FlagsAr, str(Pasta / f"lib{Nome}.a"), str(Objeto)]]
    if any(subprocess.run(P, capture_output=True).returncode != 0 for P in Passos):
        return [f"gcc ou ar nao criou a lib{Nome}.a"]
    return []


def ConstruirComLinks(Forja, Pasta, Links):
    """Grava <Pasta>\\caso.fj com um 'link' por nome de Links e roda o build x64; devolve a Rodada. O exe, se saisse, seria <Pasta>\\caso.exe."""
    (Pasta / "caso.fj").write_text(FONTE_FJ % "".join(f'    link "{L}"\n' for L in Links), encoding="utf-8", newline="\n")
    return Rodar(Forja, ["build", "--backend", "x64", str(Pasta / "caso.fj"), "-o", str(Pasta / "caso.exe")])


def ConferirDiagnosticoEstatica(Rodada, Pasta, Codigo, Trechos):
    """exit 1, 1 linha JSON do Codigo, tipo backend, interno:false, sem relato, causa com todos os Trechos e nenhum exe gravado."""
    Problemas = ConferirExecucao(Rodada, 1) + ConferirJsonCodigo(Rodada.stdout, Codigo)
    Dados = LinhaJson(Rodada.stdout) or {}
    if Dados.get("tipo") != "backend" or Dados.get("interno") is not False or "relato" in Dados:
        Problemas.append(f"{Codigo}: tipo {Dados.get('tipo')!r}, interno {Dados.get('interno')!r}, relato {'relato' in Dados}")
    Faltam = [T for T in Trechos if T not in Dados.get("causa", "")]
    if Faltam:
        Problemas.append(f"{Codigo}: causa {Dados.get('causa')!r} sem {Faltam!r}")
    if (Pasta / "caso.exe").exists():
        Problemas.append(f"{Codigo} gravou o exe")
    return Problemas


def CasoX0103ArquivoQueNaoEBiblioteca(Forja, PastaSaidas):
    """link "fx_lixo" acha fx_lixo.lib que e so texto (nem PE, nem ar, nem import library): X0103 com o arquivo na causa."""
    Pasta = PastaDoCaso(PastaSaidas, "x0103")
    (Pasta / "fx_lixo.lib").write_text("isto nao e uma biblioteca\n", encoding="utf-8", newline="\n")
    Rodada = ConstruirComLinks(Forja, Pasta, ["fx_lixo"])
    return ConferirDiagnosticoEstatica(Rodada, Pasta, "X0103", ["fx_lixo.lib", "fx_lixo"])


def CasoX0105ArSemIndice(Forja, PastaSaidas):
    """libfx_semidx.a criada com 'ar rcS' (sem tabela de simbolos): X0105 diz que falta o indice."""
    Pasta = PastaDoCaso(PastaSaidas, "x0105_semidx")
    Problemas = CriarEstatica(Pasta, "fx_semidx", SOMA_C, "rcS")
    if Problemas:
        return Problemas
    Rodada = ConstruirComLinks(Forja, Pasta, ["fx_semidx"])
    return ConferirDiagnosticoEstatica(Rodada, Pasta, "X0105", ["indice de simbolos"])


def CasoX0106SimboloEmDuasEstaticas(Forja, PastaSaidas):
    """libfx_dup_a.a e libfx_dup_b.a definem fx_soma, os dois 'link' valem: X0106 lista o simbolo e as duas bibliotecas."""
    Pasta = PastaDoCaso(PastaSaidas, "x0106")
    Problemas = CriarEstatica(Pasta, "fx_dup_a", SOMA_C) + CriarEstatica(Pasta, "fx_dup_b", SOMA_C)
    if Problemas:
        return Problemas
    Rodada = ConstruirComLinks(Forja, Pasta, ["fx_dup_a", "fx_dup_b"])
    return ConferirDiagnosticoEstatica(Rodada, Pasta, "X0106", ["fx_soma", "libfx_dup_a.a", "libfx_dup_b.a", "fx_dup_a.o", "fx_dup_b.o"])


def CasoX0107SecaoTls(Forja, PastaSaidas):
    """O membro de libfx_tls.a traz uma secao .tls: X0107 com o membro e a secao."""
    Pasta = PastaDoCaso(PastaSaidas, "x0107")
    Problemas = CriarEstatica(Pasta, "fx_tls", TLS_C)
    if Problemas:
        return Problemas
    Rodada = ConstruirComLinks(Forja, Pasta, ["fx_tls"])
    return ConferirDiagnosticoEstatica(Rodada, Pasta, "X0107", ["fx_tls.o", ".tls"])


QTD_IMPORTS_ACIMA_DO_TETO = 330  # o teto do ligador nativo e 320 nomes importados no total (61 sao da KERNEL32 e da ADVAPI32 do runtime)


def CasoX0109ImportsAcimaDoTeto(Forja, PastaSaidas):
    """fx_muitos.dll (gcc) exporta 330 funcoes e o .fj chama as 330: mais de 320 nomes importados e X0109 (antes era FalhaInterna: abort do forja)."""
    Pasta = PastaDoCaso(PastaSaidas, "x0109")
    Nomes = [f"fx_m{K}" for K in range(QTD_IMPORTS_ACIMA_DO_TETO)]
    CodigoC = "".join(f"__declspec(dllexport) int {N}(int a) {{ return a + 1; }}\n" for N in Nomes)
    (Pasta / "fx_muitos.c").write_text(CodigoC, encoding="utf-8", newline="\n")
    if subprocess.run(["gcc", "-shared", "-o", str(Pasta / "fx_muitos.dll"), str(Pasta / "fx_muitos.c")], capture_output=True).returncode != 0:
        return ["gcc nao criou a fx_muitos.dll"]
    Procs = "".join(f"    proc {N}(a: i32) -> i32\n" for N in Nomes)
    Soma = "".join(f"    s = s + {N}(1)\n" for N in Nomes)
    Fonte = f'foreign {{\n    link "fx_muitos"\n{Procs}}}\n\nproc main() {{\n    var s: i32 = 0\n{Soma}    print(s)\n}}\n'
    (Pasta / "caso.fj").write_text(Fonte, encoding="utf-8", newline="\n")
    Rodada = Rodar(Forja, ["build", "--backend", "x64", str(Pasta / "caso.fj"), "-o", str(Pasta / "caso.exe")])
    return ConferirDiagnosticoEstatica(Rodada, Pasta, "X0109", ["320", "nomes importados"])


def CasoX0108DependenciaDoDrectveAusente(Forja, PastaSaidas):
    """O .drectve do membro de libfx_dep.a pede -lfx_dep_ausente, que nao existe nas pastas de busca: X0108 com a biblioteca e o membro."""
    Pasta = PastaDoCaso(PastaSaidas, "x0108")
    Problemas = CriarEstatica(Pasta, "fx_dep", DRECTVE_C)
    if Problemas:
        return Problemas
    Rodada = ConstruirComLinks(Forja, Pasta, ["fx_dep"])
    return ConferirDiagnosticoEstatica(Rodada, Pasta, "X0108", ["fx_dep_ausente", "fx_dep.o"])
