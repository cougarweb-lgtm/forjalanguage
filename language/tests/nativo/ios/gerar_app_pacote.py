#!/usr/bin/env python3
"""gerar_app_pacote.py - I6: monta o pacote de prova dos apps iOS (ios_prova\\app_pacote) com o candidato: app real (ForjaApp, br.forja.app) e canario (Canario, br.forja.canario).
Uso: python -I gerar_app_pacote.py --forja <forja.exe> [--saida <dir>]   (padrao: gaveta\\ios_prova\\app_pacote)
Conteudo: ForjaApp.app.tar, Canario.app.tar, app/canario.esperado.contem ("forja: subiu"), casos.txt, canarios_esperados.txt, SHA256SUMS.txt. Reversao: apague a pasta de saida.
Versao: 0.1.0
Changelog: 0.1.0 (23/11/2026) I6 - criacao.
"""
import hashlib
import shutil
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
sys.path.insert(0, r"C:\CLAUDE_AGI\tools\forja_ios_app")  # lint-ok: py-lib-pessoal-path-absoluto a ferramenta da casa tem caminho oficial fixo
import forja_ios_app  # noqa: E402

LINHA_ESPERADA = "forja: subiu"
APPS = (("app", "ForjaApp", "br.forja.app", AQUI / "app" / "app_ola.fj"), ("canario", "Canario", "br.forja.canario", AQUI / "app" / "app_canario.fj"))


def Principal(Args):
    if "--forja" not in Args:
        print(__doc__)
        return 2
    Forja = Args[Args.index("--forja") + 1]
    Saida = Path(Args[Args.index("--saida") + 1]) if "--saida" in Args else LANGUAGE / "gaveta" / "ios_prova" / "app_pacote"
    if Saida.exists():
        shutil.rmtree(Saida)
    Saida.mkdir(parents=True)
    Casos = []
    with tempfile.TemporaryDirectory() as Tmp:
        for Caso, Nome, BundleId, Fj in APPS:
            _, Tar = forja_ios_app.Gerar(Fj, Nome, BundleId, "1.0", Forja, Path(Tmp) / Nome)
            shutil.copyfile(Tar, Saida / Tar.name)
            (Saida / f"{Caso}.esperado.contem").write_text(LINHA_ESPERADA, encoding="utf-8", newline="\n")
            Casos.append(f"{Caso} {Tar.name} {BundleId}\n")
    (Saida / "casos.txt").write_text("".join(Casos), encoding="utf-8", newline="\n")
    (Saida / "canarios_esperados.txt").write_text("canario\n", encoding="utf-8", newline="\n")
    Linhas = []
    for Arq in sorted(Item.name for Item in Saida.iterdir() if Item.name != "SHA256SUMS.txt"):
        Linhas.append(f"{hashlib.sha256((Saida / Arq).read_bytes()).hexdigest()}  {Arq}\n")
    (Saida / "SHA256SUMS.txt").write_text("".join(Linhas), encoding="utf-8", newline="\n")
    print("pacote:", Saida)
    print("".join(Linhas), end="")
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
