"""Canario do GCC: diz se o gcc do PATH ainda precisa de -fdisable-tree-widening_mul (P-IA1-LG).

Proposito: o erro do tree-widening_mul (.SAT_SUB estendido com sinal, gcc 16.1.0) e contornado pelo driver
(driver\\processo.c) e pelo construir.bat. Este canario compila o repro com -O2 sem a flag e com a flag, roda os
dois e diz se a flag ainda e necessaria. Relatorio do erro: gaveta\\gcc16_sat_sub_relatorio.txt.

Saida (ultima linha):
  "GCC AINDA COM ERRO (flag necessaria)"  rc 0 (sem a flag aborta, com a flag passa)
  "GCC CORRIGIU: pode remover ..."         rc 0 (sem a flag ja passa)
  "FALHA: ..."                             rc 1 (com a flag tambem aborta, ou gcc nao compila)
Arquivos temporarios ficam em %TEMP% e somem no fim.

Versao: 0.1.1
Changelog: 0.1.1 - 0.21.0 F4 passo 3: driver\\cc.c vira driver\\processo.c nas duas mencoes (o canario segue valido: construir.bat ainda compila o forja.exe com gcc).
Verificacao: python tests\\verificacao_gcc_canario.py
"""

import subprocess
import sys
import tempfile
from pathlib import Path

FlagCorrecao = "-fdisable-tree-widening_mul"
TimeoutSegundos = 60
CodigoRepro = (
    "__attribute__((noipa)) unsigned long long f(unsigned char a, unsigned char b)"
    "{return a>=b?(unsigned long long)(unsigned char)(a-b):0;}\n"
    "int main(void){volatile unsigned char x=255,y=1;if(f(x,y)!=254)__builtin_abort();return 0;}\n"
)


def CompilarERodar(PastaTemp, Nome, Flags):
    """True se o repro passa (rc 0), False se aborta. Levanta RuntimeError se o gcc nao compila."""
    Fonte = PastaTemp / f"{Nome}.c"
    Executavel = PastaTemp / f"{Nome}.exe"
    Fonte.write_text(CodigoRepro, encoding="utf-8")
    Compilacao = subprocess.run(["gcc", "-O2", *Flags, "-o", str(Executavel), str(Fonte)], capture_output=True)
    if Compilacao.returncode != 0:
        Erro = Compilacao.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"gcc nao compilou {Nome} (rc {Compilacao.returncode}): {Erro}")
    Execucao = subprocess.run([str(Executavel)], capture_output=True, timeout=TimeoutSegundos)
    return Execucao.returncode == 0


def Principal():
    with tempfile.TemporaryDirectory(prefix="canario_gcc_") as Pasta:
        PastaTemp = Path(Pasta)
        try:
            SemFlagPassa = CompilarERodar(PastaTemp, "sem_flag", [])
            ComFlagPassa = CompilarERodar(PastaTemp, "com_flag", [FlagCorrecao])
        except (RuntimeError, OSError, subprocess.TimeoutExpired) as Erro:
            print(f"FALHA: {Erro}")
            return 1
    if SemFlagPassa:
        print(r"GCC CORRIGIU: pode remover -fdisable-tree-widening_mul de driver\processo.c e construir.bat")
        return 0
    if ComFlagPassa:
        print("GCC AINDA COM ERRO (flag necessaria)")
        return 0
    print("FALHA: com a flag tambem aborta")
    return 1


if __name__ == "__main__":
    sys.exit(Principal())
