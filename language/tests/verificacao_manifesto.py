# -*- coding: utf-8 -*-
"""verificacao_manifesto.py -- fotografia (manifesto) do compilador e classificacao do diff em nivel de verificacao (Emenda 4 do IA1).

Importado so por compiler\\construir_rapido.py (VersaoGcc, Sha256Arquivo, HashArvore); o verificacao_enxuta.py e o sha_massa.py sairam na bateria 2.0.0. Nao roda nada do compilador.
Manifesto = sha256 de compiler\\**\\*.{c,h,bat,py}, runtime, std, vendor, third\\*.{c,h}, tools\\forja_ligar, tests\\**\\*.{fj,py} (sem ia_first),
benchmark\\corpus\\gerado\\*.fj e examples\\*.fj, mais o sha256 do forja.exe e a versao do gcc. Chaves relativas a C:\\ENGINE_GAME3D.
Uso: python -I verificacao_manifesto.py --selftest
Versao: 0.1.2
Changelog: 0.1.2 - 0.21.0 F4 passo 3: driver/cc.c vira driver/processo.c (COMPLETO_ARQUIVOS e o caso de teste de nivel 2); o caso de teste emit_c.c vira emit_h.c (so um caminho de classificacao, arquivo emit_c apagado).
           0.1.1 - IA1-b09: language\\doc (*.txt, *.tsv) entra no manifesto e conta como fonte do forja.exe (o texto da spec e embutido por .incbin).
           0.1.0 - IA1-b02v: manifesto, diff, classificacao rapido/padrao/completo (D-IA1-15 e regras 3 e 4 da Emenda 4).
Verificacao: python -I verificacao_manifesto.py --selftest
"""

import hashlib
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

RAIZ = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA")
LANG = RAIZ / "language"
COPIAR_FIEL = Path(r"C:\CLAUDE_AGI\tools\copiar_fiel\copiar_fiel.py")
NIVEIS = ("rapido", "padrao", "completo")
GRUPOS = [  # (pasta, extensoes; None = todo arquivo)
    (LANG / "compiler", {".c", ".h", ".bat", ".py"}),
    (LANG / "runtime", None),
    (LANG / "std", None),
    (LANG / "vendor", None),
    (LANG / "third", {".c", ".h"}),
    (LANG / "tools" / "forja_ligar", None),
    (LANG / "doc", {".txt", ".tsv"}),
    (LANG / "tests", {".fj", ".py"}),
    (LANG / "benchmark" / "corpus" / "gerado", {".fj"}),
    (RAIZ / "examples", {".fj"}),
]
PARTES_IGNORADAS = frozenset({"__pycache__", "ia_first", ".git"})
COMPLETO_PASSOS = frozenset({"b04", "b08", "i2", "i5", "l1", "l4", "l5b", "l7b", "l7c", "f2", "z0"})
CLI_PASSOS = frozenset({"b02", "b03", "b05", "b09", "b10", "i2", "i3", "i4", "i5", "l3"})
COMPLETO_ARQUIVOS = frozenset({"compiler/driver/processo.c", "compiler/driver/ligar.c", "compiler/ir/ir.h", "compiler/common/diag.h"})
COMPLETO_PREFIXOS = ("runtime/", "third/", "tools/forja_ligar/", "compiler/nativo/fmt/")
RAPIDO_PREFIXOS = ("compiler/frontend/", "compiler/semantic/", "compiler/common/diag.c")
EXTENSOES_FONTE = (".c", ".h", ".bat")
PREFIXOS_FONTE = ("runtime/", "std/", "vendor/", "third/", "tools/forja_ligar/", "doc/")


def Sha256Arquivo(Caminho):
    """sha256 de um arquivo; '-' se sumiu ou esta travado no meio da leitura (outro agente gravando)."""
    try:
        return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()
    except OSError:
        return "-"


def ListarGrupo(Pasta, Extensoes):
    """Arquivos de uma pasta do manifesto (recursivo), sem as partes ignoradas."""
    if not Pasta.is_dir():
        return []
    Achados = (Arq for Arq in Pasta.rglob("*") if Arq.is_file() and not PARTES_IGNORADAS.intersection(Arq.parts))
    return [Arq for Arq in Achados if Extensoes is None or Arq.suffix in Extensoes]


def ListarArquivos(Grupos=GRUPOS):
    """Todos os arquivos do manifesto, ordenados."""
    return sorted({Arq for Pasta, Ext in Grupos for Arq in ListarGrupo(Pasta, Ext)})


def HashArquivos(Arquivos, Raiz=RAIZ):
    """{caminho relativo a Raiz (posix): sha256}, hashing em paralelo."""
    with ThreadPoolExecutor(max_workers=16) as Pool:
        Hashes = list(Pool.map(Sha256Arquivo, Arquivos))
    return {Arq.relative_to(Raiz).as_posix(): Sha for Arq, Sha in zip(Arquivos, Hashes)}


def HashArvore(Pastas):
    """Um sha256 so para varias pastas (nomes relativos e conteudos): o 'ambiente' que o forja le em runtime."""
    Resumo = hashlib.sha256()
    for Pasta in Pastas:
        Arquivos = [Arq for Arq in Pasta.rglob("*") if Arq.is_file()] if Pasta.is_dir() else []
        for Chave, Sha in sorted(HashArquivos(sorted(Arquivos), Pasta).items()):
            Resumo.update(f"{Pasta.name}/{Chave}={Sha}\n".encode("utf-8"))
    return Resumo.hexdigest()


def VersaoGcc():
    """Primeira linha de 'gcc --version' (a regra 3 da Emenda 4 inclui o gcc); 'sem-gcc' se nao achar."""
    try:
        return subprocess.run(["gcc", "--version"], capture_output=True, timeout=30).stdout.decode("utf-8", "replace").splitlines()[0]
    except (OSError, IndexError, subprocess.TimeoutExpired):
        return "sem-gcc"


def CopiarFiel(Origem, Destino):
    """Copia byte a byte pela ferramenta da casa (copiar_fiel); levanta se nao copiou."""
    Cp = subprocess.run([sys.executable, "-I", str(COPIAR_FIEL), str(Origem), str(Destino), "--criar-pastas"], capture_output=True)
    if Cp.returncode != 0:
        raise RuntimeError(f"copiar_fiel {Origem} -> {Destino} falhou (rc {Cp.returncode}): {Cp.stderr.decode('utf-8', 'replace')[:300]}")
    return Path(Destino)


def MontarManifesto(Forja):
    """O manifesto de agora: forja.exe, gcc e os hashes dos arquivos."""
    return {"versao": 1, "forja_sha256": Sha256Arquivo(Forja), "gcc": VersaoGcc(), "arquivos": HashArquivos(ListarArquivos())}


def Fotografar(Pasta, Forja, Manifesto=None):
    """Congela o 'antes': copia fiel do forja.exe + manifesto.json em Pasta (Manifesto ja pronto evita refazer os hashes). Devolve o manifesto."""
    Pasta = Path(Pasta)
    Pasta.mkdir(parents=True, exist_ok=True)
    CopiarFiel(Forja, Pasta / "forja.exe")
    Manifesto = Manifesto or MontarManifesto(Pasta / "forja.exe")
    (Pasta / "manifesto.json").write_text(json.dumps(Manifesto, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    return Manifesto


def LerManifestoAoLado(Forja):
    """manifesto.json ao lado do forja.exe congelado; None se nao houver (sem diff possivel)."""
    Arquivo = Path(Forja).parent / "manifesto.json"
    return json.loads(Arquivo.read_text(encoding="utf-8")) if Arquivo.is_file() else None


def Relativo(Chave):
    """Chave do manifesto sem o prefixo 'language/' (as regras de nivel falam em compiler/, runtime/...)."""
    return Chave[len("language/"):] if Chave.startswith("language/") else Chave


def EhFonteDoCompilador(Chave):
    """True para o que entra no forja.exe: compiler/*.{c,h,bat}, runtime, std, vendor, third, tools/forja_ligar."""
    Rel = Relativo(Chave)
    if Rel.startswith(PREFIXOS_FONTE):
        return True
    return Rel.startswith("compiler/") and Rel.endswith(EXTENSOES_FONTE)


def DiferencaManifestos(Antigo, Atual):
    """{'tocados': todos os caminhos novos/mudados/sumidos, 'fontes': so o que entra no forja.exe,
    'fj': .fj novos ou mudados, 'py': .py mudados, 'gcc_mudou': bool}."""
    A, B = Antigo["arquivos"], Atual["arquivos"]
    Tocados = sorted(Chave for Chave in set(A) | set(B) if A.get(Chave) != B.get(Chave))
    return {"tocados": Tocados, "fontes": [C for C in Tocados if EhFonteDoCompilador(C)],
            "fj": [C for C in Tocados if C.endswith(".fj") and C in B],
            "py": [C for C in Tocados if C.endswith(".py")], "gcc_mudou": Antigo.get("gcc") != Atual.get("gcc")}


def NivelDoCaminho(Chave):
    """0 rapido, 1 padrao, 2 completo, para um arquivo de fonte tocado (regras 3 e 4 da Emenda 4)."""
    Rel = Relativo(Chave)
    if Rel in COMPLETO_ARQUIVOS or Rel.startswith(COMPLETO_PREFIXOS) or Rel.endswith(".bat") or "abi" in Rel.rsplit("/", 1)[-1].lower():
        return 2
    return 0 if Rel.startswith(RAPIDO_PREFIXOS) else 1


def IdCurto(Passo):
    """'P-IA1-b04' -> 'b04'; minusculo para comparar com as tabelas."""
    Texto = (Passo or "").strip().lower()
    return Texto[len("p-ia1-"):] if Texto.startswith("p-ia1-") else Texto


def MotivoDeCompleto(Diff, Curto, ForjaMudou):
    """Motivo que obriga o nivel completo (regras 3 e 4 da Emenda 4), ou None. Diff None = sem manifesto antigo."""
    if Diff is None:
        return "sem manifesto do antes (passe pela --fotografar): sem diff possivel"
    if Curto in COMPLETO_PASSOS:
        return f"passo {Curto} e completo por contrato"
    if ForjaMudou and not Diff["fontes"]:
        return "forja.exe mudou com diff de fonte vazio"
    return "a versao do gcc mudou" if Diff["gcc_mudou"] else None


def Classificar(Diff, Passo, ForjaMudou):
    """(indice do nivel, motivo)."""
    Motivo = MotivoDeCompleto(Diff, IdCurto(Passo), ForjaMudou)
    if Motivo:
        return 2, Motivo
    Pior = max((NivelDoCaminho(Chave) for Chave in Diff["fontes"]), default=0)
    Causa = next((Chave for Chave in Diff["fontes"] if NivelDoCaminho(Chave) == Pior), None)
    return Pior, (f"maior impacto: {Causa}" if Causa else "diff sem fonte do compilador")


def ComFontes(*Chaves):
    """Diff de teste: so estas chaves de fonte tocadas."""
    return {"tocados": list(Chaves), "fontes": list(Chaves), "fj": [], "py": [], "gcc_mudou": False}


def CasosDeClassificacao():
    """(diff, passo, forja_mudou, nivel esperado) -- um por regra de NivelDoCaminho e de Classificar."""
    Vazio = ComFontes()
    C = "language/compiler/"
    return [
        (Vazio, "b02v", False, 0), (None, "b02v", False, 2),
        (ComFontes(C + "frontend/parser.c"), "x", True, 0), (ComFontes(C + "semantic/sem.c"), "x", True, 0),
        (ComFontes(C + "common/diag.c"), "x", True, 0), (ComFontes(C + "common/diag.h"), "x", True, 2),
        (ComFontes(C + "ir/lower.c"), "x", True, 1), (ComFontes(C + "backend/emit_h.c", C + "frontend/a.c"), "x", True, 1),
        (ComFontes(C + "nativo/x64/x64_abi_win.c"), "x", True, 2), (ComFontes(C + "nativo/fmt/pe_escrever.c"), "x", True, 2),
        (ComFontes("language/runtime/forja_rt.h"), "x", True, 2), (ComFontes(C + "driver/processo.c"), "x", True, 2),
        (ComFontes(C + "driver/main.c"), "x", True, 1), (Vazio, "x", True, 2), (Vazio, "P-IA1-b04", False, 2),
        ({**Vazio, "gcc_mudou": True}, "x", False, 2), (ComFontes(C + "frontend/a.c"), "P-IA1-z0", False, 2),
    ]


def FalhasDeClassificacao():
    return [Pos for Pos, (Diff, Passo, Mudou, Esperado) in enumerate(CasosDeClassificacao())
            if Classificar(Diff, Passo, Mudou)[0] != Esperado]


def FalhaDeDiff():
    """True se DiferencaManifestos nao separa tocados, .fj e fontes como esperado."""
    Antigo = {"arquivos": {"a.fj": "1", "b.c": "2", "c.fj": "3"}, "gcc": "g"}
    Atual = {"arquivos": {"a.fj": "9", "b.c": "2", "d.fj": "4"}, "gcc": "g"}
    Dif = DiferencaManifestos(Antigo, Atual)
    return Dif["tocados"] != ["a.fj", "c.fj", "d.fj"] or Dif["fj"] != ["a.fj", "d.fj"] or bool(Dif["fontes"])


def Autoteste():
    Falhas = FalhasDeClassificacao() + (["diff"] if FalhaDeDiff() else [])
    print(f"verificacao_manifesto selftest: {len(CasosDeClassificacao()) + 1} casos, {len(Falhas)} falhas {Falhas}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(Autoteste())
    print("uso: python -I verificacao_manifesto.py --selftest  (modulo importado por construir_rapido.py)")
