# gerar_dll_pacote.py - monta o PACOTE DE PROVA dos dylibs iOS (fatia I5) para o workflow dll.yml do repo forja-ios-prova
# Versao: 1.0.0
# Changelog: 1.0.0 - I5: 3 fontes (dll_aritmetica, dll_heap, dll_texto) x dev/release em aarch64-apple-ios-sim; esperados do ORACULO (dll_oraculo.fj rodando no x64 do Windows);
#                    casos por caminho absoluto e por @rpath; 2 canarios plantados (stdout errado e exit errado); verificar_macho.py roda em todo dylib (sim e aparelho) antes de montar.
# Uso: python -I gerar_dll_pacote.py <compilador.exe> <pasta_saida>   (a pasta e recriada)
# Depende de: verificar_macho.py (mesma pasta; lief/capstone opcionais la)
import hashlib
import os
import shutil
import subprocess
import sys

Aqui = os.path.dirname(os.path.abspath(__file__))
Fontes = ["aritmetica", "heap", "texto"]
Modos = [("dev", []), ("release", ["--release"])]
Exports = {"aritmetica": "soma,fatorial,contar", "heap": "heap_soma,heap_mapa", "texto": "ola_texto,tamanho_texto,forja_liberar"}


def Executar(Comando):
    Resultado = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if Resultado.returncode != 0:
        raise SystemExit(f"falhou ({Resultado.returncode}): {' '.join(Comando)}\n{Resultado.stdout}\n{Resultado.stderr}")
    return Resultado.stdout


def EsperadosDoOraculo(Compilador):
    """Roda o oraculo no x64 do Windows e separa a saida por secao `== suite`."""
    Saida = Executar([Compilador, "run", os.path.join(Aqui, "dll", "dll_oraculo.fj")])
    Secoes = {}
    Atual = None
    for Linha in Saida.replace("\r\n", "\n").split("\n"):
        if Linha.startswith("== "):
            Atual = Linha[3:]
            Secoes[Atual] = []
        elif Linha and Atual:
            Secoes[Atual].append(Linha)
    Secoes["texto"].append("forja_dll_info=nao-nulo")  # o host confere so que o ponteiro nao e nulo
    return {Nome: "\n".join(Linhas) + "\n" for Nome, Linhas in Secoes.items()}


def Compilar(Compilador, Alvo, Suite, Modo, Extra, Destino):
    Fonte = os.path.join(Aqui, "dll", f"dll_{Suite}.fj")
    Executar([Compilador, "build", *Extra, "--target", Alvo, "--modo", "dll", Fonte, "-o", Destino])
    Executar([sys.executable, "-I", os.path.join(Aqui, "verificar_macho.py"), "--exports", Exports[Suite], Destino])


def Gravar(Caminho, Texto):
    with open(Caminho, "w", encoding="utf-8", newline="\n") as Arquivo:
        Arquivo.write(Texto)


def Principal(Args):
    Compilador, Pasta = os.path.abspath(Args[0]), os.path.abspath(Args[1])
    if os.path.isdir(Pasta):
        shutil.rmtree(Pasta)
    os.makedirs(Pasta)
    Esperados = EsperadosDoOraculo(Compilador)
    Casos = []
    for Suite in Fontes:
        for Modo, Extra in Modos:
            Arquivo = f"libdll_{Suite}_{Modo}.dylib"
            Compilar(Compilador, "aarch64-apple-ios-sim", Suite, Modo, Extra, os.path.join(Pasta, Arquivo))
            Nome = f"{Suite}_{Modo}"
            Casos.append(f"{Nome} {Suite} {Arquivo} abs")
            Gravar(os.path.join(Pasta, Nome + ".esperado.stdout"), Esperados[Suite])
    Casos.append("aritmetica_rpath_dev aritmetica libdll_aritmetica_dev.dylib rpath")
    Gravar(os.path.join(Pasta, "aritmetica_rpath_dev.esperado.stdout"), Esperados["aritmetica"])
    Casos.append("canario_stdout_dev aritmetica libdll_aritmetica_dev.dylib abs")
    Gravar(os.path.join(Pasta, "canario_stdout_dev.esperado.stdout"), Esperados["aritmetica"].replace("soma(2,3)=5", "soma(2,3)=6"))
    Casos.append("canario_exit_dev aritmetica libdll_aritmetica_dev.dylib abs")
    Gravar(os.path.join(Pasta, "canario_exit_dev.esperado.stdout"), Esperados["aritmetica"])
    Gravar(os.path.join(Pasta, "canario_exit_dev.esperado.exit"), "7\n")
    Gravar(os.path.join(Pasta, "casos.txt"), "\n".join(Casos) + "\n")
    Gravar(os.path.join(Pasta, "canarios_esperados.txt"), "canario_stdout_dev\ncanario_exit_dev\n")
    # estrutural do alvo de aparelho (nao vai ao pacote): so prova que o build e a verificacao passam
    for Suite in Fontes:
        Temporario = os.path.join(Pasta, f"_aparelho_{Suite}.dylib")
        Compilar(Compilador, "aarch64-apple-ios", Suite, "dev", [], Temporario)
        os.remove(Temporario)
    Linhas = []
    for Nome in sorted(os.listdir(Pasta)):
        if Nome == "SHA256SUMS.txt" or Nome.endswith(".h"):
            continue
        Linhas.append(f"{hashlib.sha256(open(os.path.join(Pasta, Nome), 'rb').read()).hexdigest()}  {Nome}")
    for Nome in os.listdir(Pasta):
        if Nome.endswith(".h"):
            os.remove(os.path.join(Pasta, Nome))
    Gravar(os.path.join(Pasta, "SHA256SUMS.txt"), "\n".join(Linhas) + "\n")
    print(f"pacote em {Pasta}: {len(Casos)} casos, {len(Linhas)} arquivos")
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
