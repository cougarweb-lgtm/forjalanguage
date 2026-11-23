"""gerar_oraculo.py - compila oraculo_libmem.c contra as .a do vendor e grava os numeros dos .esperado (M6d, P92a).

Liga a MESMA libcjson.a e libz.a que as fixtures usam (vendor\\libcjson.a, vendor\\libz.a no Windows;
vendor\\linux\\*.a no Linux via WSL alpine-build, com teto de tempo). Roda o oraculo sobre cjson_entrada.json
(o mesmo texto que a fixture le por #load) e zlib, e grava esperado_windows.txt e esperado_linux.txt.

Versao: 0.1.0
Changelog:
  0.1.0 - M6d P92a: primeira versao (Windows por gcc no PATH; Linux por wsl -d alpine-build, teto de 180 s).
Uso: python tests\\nativo\\m6d\\oraculo\\gerar_oraculo.py
Depende de: stdlib do Python; gcc no PATH (Windows); distro WSL alpine-build com gcc.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

PastaOraculo = Path(__file__).resolve().parent
PastaForja = PastaOraculo.parents[3]
PastaVendor = PastaForja / "vendor"
PastaCjsonHeaders = PastaVendor / "json" / "cjson"
PastaZlibHeaders = PastaVendor / "compression" / "zlib"
JsonEntrada = PastaForja / "tests" / "nativo" / "m6d" / "lib" / "cjson_entrada.json"
TetoSegundosWsl = 180


def CaminhoWsl(Caminho):
    Texto = str(Caminho).replace("\\", "/")
    return "/mnt/" + Texto[0].lower() + Texto[2:]


def ExecutarOraculoWindows(Tmp):
    Exe = Path(Tmp) / "oraculo.exe"
    Fonte = PastaOraculo / "oraculo_libmem.c"
    subprocess.run(
        ["gcc", "-std=gnu11", "-O1", f"-I{PastaCjsonHeaders}", f"-I{PastaZlibHeaders}", str(Fonte),
         str(PastaVendor / "libcjson.a"), str(PastaVendor / "libz.a"), "-o", str(Exe)],
        check=True, timeout=TetoSegundosWsl,
    )
    Saidas = []
    for Argumentos in (["cjson", str(JsonEntrada)], ["zlib"]):
        Saidas.append(subprocess.run([str(Exe), *Argumentos], check=True, capture_output=True, text=True, timeout=TetoSegundosWsl).stdout.strip())
    return Saidas


def ExecutarOraculoLinux():
    Fonte = CaminhoWsl(PastaOraculo / "oraculo_libmem.c")
    Vendor = CaminhoWsl(PastaVendor / "linux")
    Json = CaminhoWsl(JsonEntrada)
    Comando = (
        "T=$(mktemp -d) && trap 'rm -rf \"$T\"' EXIT && set -e; "
        f"gcc -std=gnu11 -O1 -no-pie -I'{CaminhoWsl(PastaCjsonHeaders)}' -I'{CaminhoWsl(PastaZlibHeaders)}' '{Fonte}' "
        f"'{Vendor}/libcjson.a' '{Vendor}/libz.a' -o \"$T/oraculo\"; "
        f"\"$T/oraculo\" cjson '{Json}'; \"$T/oraculo\" zlib"
    )
    Resultado = subprocess.run(["wsl", "-d", "alpine-build", "--", "sh", "-c", Comando], check=True, capture_output=True, text=True, timeout=TetoSegundosWsl)
    return [Linha for Linha in Resultado.stdout.splitlines() if Linha.strip()]


def GravarEsperado(Nome, Linhas):
    (PastaOraculo / Nome).write_text("\n".join(Linhas) + "\n", encoding="utf-8")
    print(Nome + ": " + " | ".join(Linhas))


def Principal():
    with tempfile.TemporaryDirectory() as Tmp:
        GravarEsperado("esperado_windows.txt", ExecutarOraculoWindows(Tmp))
    GravarEsperado("esperado_linux.txt", ExecutarOraculoLinux())
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
