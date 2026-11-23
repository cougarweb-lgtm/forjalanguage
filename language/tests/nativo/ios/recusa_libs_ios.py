#!/usr/bin/env python3
"""recusa_libs_ios.py - I6: prova que o ligador Mach-O (aarch64-apple-ios-sim) recusa `link` de lib fora da libSystem e dos frameworks de sistema (X0001, com o nome da lib)
e aceita os frameworks (UIKit, Foundation, CoreFoundation, QuartzCore, CoreGraphics) e a libobjc, gerando um Mach-O que passa no verificar_macho.
Uso: python -I recusa_libs_ios.py --forja <forja.exe>     (exit 0 = tudo certo)
Versao: 0.1.0
Changelog: 0.1.0 (23/11/2026) I6 - criacao.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
import verificar_macho  # noqa: E402

ALVO = "aarch64-apple-ios-sim"
RECUSADAS = ("sqlite3", "z", "cjson", "curl")
ACEITAS = ("objc", "UIKit", "Foundation", "CoreFoundation", "QuartzCore", "CoreGraphics")


def FonteMinima(Lib):
    """Programa que de fato usa um simbolo da lib (o ligador so liga o que o programa chama)."""
    Prefixo = {"objc": "objc_getClass", "UIKit": "UIApplicationMain_x", "Foundation": "NSLog", "CoreFoundation": "CFRelease", "QuartzCore": "CATransform3DIdentity_x", "CoreGraphics": "CGColorSpaceCreateDeviceRGB"}
    Nome = Prefixo.get(Lib, f"{Lib}_init")
    Tipo = "rawptr"
    return f"""foreign {{
    link "{Lib}"
    proc chamada = "{Nome}"(a: {Tipo}) -> rawptr
}}

proc main() {{
    _ = chamada(transmute(0 as uint, to = {Tipo}))
}}
"""


def Compilar(Forja, Fonte_, Pasta, Nome):
    Arquivo = Pasta / f"{Nome}.fj"
    Arquivo.write_text(Fonte_, encoding="utf-8", newline="\n")
    Saida = Pasta / Nome
    Proc = subprocess.run([Forja, "build", "--target", ALVO, str(Arquivo), "-o", str(Saida)], capture_output=True, text=True, timeout=120)
    return Proc, Saida


def Principal(Args):
    if len(Args) != 2 or Args[0] != "--forja":
        print(__doc__)
        return 2
    Forja = Args[1]
    Falhas = []
    with tempfile.TemporaryDirectory() as Tmp:
        Pasta = Path(Tmp)
        for Lib in RECUSADAS:
            Proc, _ = Compilar(Forja, FonteMinima(Lib), Pasta, f"recusa_{Lib}")
            try:
                Diag = json.loads(Proc.stdout.strip().splitlines()[0])
            except (IndexError, ValueError):
                Diag = {}
            Ok = Proc.returncode == 1 and Diag.get("codigo") == "X0001" and f"({Lib})" in Diag.get("causa", "")
            print(f"recusa {Lib}: {'OK' if Ok else 'FALHA'} (exit {Proc.returncode}, {Diag.get('codigo')})")
            if not Ok:
                Falhas.append(f"recusa {Lib}")
        for Lib in ACEITAS:
            Proc, Saida = Compilar(Forja, FonteMinima(Lib), Pasta, f"aceita_{Lib}")
            Ok = Proc.returncode == 0 and Saida.exists()
            if Ok:
                Lista, Info = verificar_macho.VerificarArquivo(str(Saida), False, (), False, [verificar_macho.CaminhoDylib(Lib)])
                Ok = not Lista
            print(f"aceita {Lib}: {'OK' if Ok else 'FALHA'} (exit {Proc.returncode}) {Proc.stdout[:200] if not Ok else ''}")
            if not Ok:
                Falhas.append(f"aceita {Lib}")
    print("recusa_libs_ios:", "OK" if not Falhas else f"{len(Falhas)} falha(s): {Falhas}")
    return 0 if not Falhas else 1


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
