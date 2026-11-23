import sys

Nomes = sys.argv[1:]
if not Nomes:
    print("Ola, mundo!")
for Nome in Nomes:
    print(f"Ola, {Nome}!")
