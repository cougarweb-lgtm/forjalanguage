import os
import sys

# python -I nao poe a pasta do script no caminho de modulos: o modulo vizinho e localizado a mao.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import matematica  # noqa: E402

A, B = int(sys.argv[1]), int(sys.argv[2])
print(f"soma_quadrados({A}, {B}) = {matematica.soma_quadrados(A, B)}")
