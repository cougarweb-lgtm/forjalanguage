import sys
from dataclasses import dataclass


@dataclass
class Conta:
    titular: str  # lint-ok: nome-fora-do-padrao campo com o nome exigido pelo enunciado E05
    saldo: int  # lint-ok: nome-fora-do-padrao campo com o nome exigido pelo enunciado E05


def Depositar(Alvo: Conta, Valor: int) -> bool:
    if Valor <= 0:
        return False
    Alvo.saldo += Valor
    return True


def Sacar(Alvo: Conta, Valor: int) -> bool:
    if Valor <= 0 or Valor > Alvo.saldo:
        return False
    Alvo.saldo -= Valor
    return True


def Autoteste() -> int:
    """Devolve quantas verificacoes falharam."""
    Falhas = 0
    A = Conta("t", 100)
    Falhas += 0 if (Depositar(A, 50) and A.saldo == 150) else 1
    Falhas += 0 if (not Depositar(A, 0) and A.saldo == 150) else 1
    Falhas += 0 if (not Sacar(A, 200) and A.saldo == 150) else 1
    Falhas += 0 if (Sacar(A, 150) and A.saldo == 0) else 1
    return Falhas


def Cenario() -> None:
    C = Conta("Rita", 1000)
    for Nome, Operacao, Valor in (("deposito", Depositar, 500), ("saque", Sacar, 2000),
                                  ("saque", Sacar, 700), ("deposito", Depositar, -5)):
        Resultado = "ok" if Operacao(C, Valor) else "recusado"
        print(f"{Nome} {Valor}: {Resultado}, saldo {C.saldo}")
    print(f"titular {C.titular} saldo final {C.saldo}")


if len(sys.argv) > 1 and sys.argv[1] == "autoteste":
    Falhou = Autoteste()
    print("autoteste: 4 ok" if Falhou == 0 else f"autoteste: falhou {Falhou}")
    sys.exit(0 if Falhou == 0 else 1)
Cenario()
