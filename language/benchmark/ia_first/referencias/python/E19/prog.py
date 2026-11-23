import sys
from typing import Optional

IDADES = {"ana": 31, "bruno": 45, "carla": 28}


def BuscarIdade(Nome: str) -> Optional[int]:
    return IDADES.get(Nome)


for Nome in sys.argv[1:]:
    Idade = BuscarIdade(Nome)
    print(f"{Nome}: {'ausente' if Idade is None else Idade}")
