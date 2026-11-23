# projeto_enxuto.py - roda tests\projeto\verificar_projeto.py SEM editar o arquivo, com o fonte grande reduzido (o E1 dev compila ~100x mais devagar que o E0:
# 100000 exports = 4 min e 1,2 GB por filho; N menor devolve ao caso a premissa "o build leva alguns segundos"). Uso: python projeto_enxuto.py N --forja <exe>
import sys
sys.path.insert(0, 'C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/tests/projeto')
import verificar_projeto
verificar_projeto.PROCEDIMENTOS_GRANDES = int(sys.argv.pop(1))
sys.exit(verificar_projeto.Principal())
