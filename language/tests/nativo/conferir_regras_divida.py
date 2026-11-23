# -*- coding: utf-8 -*-
"""Divida congelada do conferir_regras.py (Emenda 5): funcoes que ja violavam a regra nova quando ela entrou.

Entrada: (arquivo_relativo, funcao, metrica, valor_aceito). Acima do valor viola; abaixo falha com 'divida paga' (baixe o valor).
Gerada por: python -I conferir_regras.py --listar-divida
Versao: 1.1.0
Changelog: 1.1.0 - D-G3-L2a (bloco C): forja_process_spawn do Linux (8 parametros, o contrato do externo do std:process; o L2b o troca pelo real, mesmo numero de parametros) entra como divida (a). 1.0.0 - P-IA1-reg1c (11/11/2026): linha de base da regra nova: 0 entradas acima do teto (a) e 2 entre alvo e teto sem 'LIMITE: niveis' (b).
"""

DividaRegras = (
    ("compiler/nativo/bir/bir_validar.c", "ArestaUnica", "niveis", 4),
    ("compiler/nativo/nativo_erro.c", "FuncaoDaBirNaPosicao", "niveis", 4),
    ("runtime/nativo/rt_processo_linux.c", "forja_process_spawn", "parametros", 8),
)
