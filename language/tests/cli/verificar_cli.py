"""Verifica o driver do compilador Forja (Fase 5h): forja test, run, explicar, #line e os diagnosticos de build.

Regras:
  - test: 2 testes que passam -> exit 0 e ultima linha '2 testes, 0 falharam'; expect falso -> exit 1 e 'FALHOU <titulo>'.
  - arquivo sem test -> '0 testes, 0 falharam' e exit 0.
  - run: repassa o stdout do programa e devolve o exit dele (panic -> 101); --release tambem roda.
  - explicar: uma linha JSON do catalogo; codigo desconhecido -> exit 2.
  - build com erro de tipo -> exit 1 e 1 linha JSON no stdout; --backend c e --modo c = X0003 (exit 2, 1 linha JSON no stderr; o backend C saiu na 0.21.0).

Este arquivo e o MOTOR (Rodar, comparacao, contagem, laco e a ordem dos casos). Os casos moram em
cli_casos_frontend.py, cli_casos_build.py, cli_casos_ffi.py, cli_casos_nativo.py, cli_casos_estatica.py, cli_casos_run_args.py, cli_casos_layout.py, cli_casos_higiene.py, cli_casos_spec.py, cli_casos_limites.py, cli_casos_ast.py, cli_casos_consultar.py, cli_casos_mapa.py e cli_casos_editar.py (mesma pasta).

Versao: 1.102.1
Changelog: 1.102.1 - 0.25.1: P79 E1001 lista todos os modulos agora aceita o corte com marca ' (+N)' e conta fechada (cli_casos_consultar.py 1.2.0); mesmo numero de casos (310). 1.102.0 - 0.25.1: +2 casos (cli_casos_ast.py 1.1.0: CasoAstLoadAusente; cli_casos_fmt.py 1.4.0: CasoCheckCanonicoResumo): 308 -> 310 casos.
           1.101.2 - PREPARO 0.24.1: o rotulo de std:text passa a 58 testes (cli_casos_s4.py 1.8.4).
           1.101.1 - G3-L3: o rotulo de std:text acompanha cli_casos_s4.py 1.8.3 (54 testes); sem caso novo. 1.101.0 - L1b (modulo de pasta): +8 casos (cli_casos_pacote.py 1.0.0: ast, consultar, mapa, fmt, editar, mover, migrar e cache da pasta de um pacote): 300 -> 308 casos (77 por parte de 4).
           1.100.0 - G0d: +2 casos (cli_casos_nativo.py 1.32.0: CasoDebugPassesSecao e CasoDebugPassesDeterministico): 298 -> 300 casos.
           1.99.0 - M8 P2: +2 casos (cli_casos_rastreio.py 1.10.0: CasoPerfilOpcao, as recusas do --perfil e o exe que grava os 2 arquivos; CasoPerfilTest, test --perfil).
           1.98.0 - M8 H1: +3 casos (cli_casos_dll_info.py 1.0.0: forja_dll_info com hash igual, hash diferente e ctypes contra o --debug-code interface).
           1.97.0 - R10c (rede): os rotulos de std:text (38 testes) e std:time (23 testes) acompanham cli_casos_s4.py 1.8.0; sem caso novo, a contagem de casos nao muda.
           1.96.0 - R10a (contrato_rede): +1 caso (cli_casos_nativo.py 1.31.0: CasoTargetLinuxRedeLibsDoWindows).
           1.95.0 - 0.21.0 F4 passo 3: saem do registro 7 casos do backend C (FORJA_CC, #line do C, 3 sondas do gcc, --backend c, alvo linux via C) e +1 caso (Build.CasoX0003: --backend c, --modo c com e sem -o e com --release, --prova-layout = X0003 exit 2); os casos --modo c viram run no x64 (foreign, alias, array, memoria, v1.7, v1.8); M-D5 e injetados X0901..X0906 ajustados ao texto atual.
           1.94.0 - 0.21.0 F3: titulos dos casos S4, rastreio, --traco e b03 dizem x64 contra valor fixo (sem comparar o C ao vivo); a paridade b03 roda com --contra-congelado; sem mudar a logica nem a contagem.
           1.93.0 - 0.21.0 F3: os 2 casos do padrao do backend com --release sem --backend viram x64 (debug-code com exit 0 e build com a assinatura Forja Native); mesma contagem.
           1.92.0 - M7a F0: +1 caso (cli_casos_rastreio.py 1.5.0: CasoSemOtimOpcao).
           1.91.0 - O0 P13: +1 caso (cli_casos_idioma.py 1.1.0: CasoIdiomaXorSoComOperando).
           1.90.0 - O0 P13: +1 caso (cli_casos_rastreio.py 1.4.0: CasoChecagemOpcao).
           1.89.0 - M6e P93d: +1 caso (cli_casos_rastreio.py 1.3.0: CasoGuardaMemoriaOpcao, as 3 recusas do --guarda-memoria).
           1.88.0 - M6b P79: +9 casos (cli_casos_consultar.py 1.1.0: api std:/vendor: sem arquivo hospedeiro, E1001 com a lista completa e o parecido, spec --receitas).
           1.87.0 - M5 P29b: +2 casos (cli_casos_nativo.py 1.26.0: CasoTargetLinuxTestOpcoes e CasoTargetLinuxTestStd; test do alvo linux com --json/--filtro e com std:<modulo>); 283 -> 285.
           1.86.0 - M5 P28b-2: o caso do std:process no Linux deixa de esperar X0003 e passa a conferir que liga (CasoTargetLinuxProcessoLiga, cli_casos_nativo.py 1.25.0; mesmo total de casos).
           1.85.0 - M5 P28b-1: +2 casos (cli_casos_nativo.py 1.24.0: CasoTargetLinuxProcessoInterino, o X0003 interino do std:process; CasoTargetLinuxFileOsLigam, std:file/os/stdin ligam no x86_64-linux); fixture tests\\cli\\linux_processo.fj.
           1.84.0 - bateria fatiada: --parte K/N roda so a parte K de N dos casos (o caso i cai na parte i % N + 1); a linha final traz 'parte K/N de T'.
           1.83.0 - Portao Z b1: +2 casos (fmt --aplicar em so-leitura, cli_casos_fmt.py 1.3.0; divisao real por zero E0420, cli_casos_build.py 1.3.0): 279 -> 281 casos.
           1.81.0 - F1 P2: +6 casos do check --canonico (F0001) e explicar F0001 (cli_casos_fmt.py 1.2.0): 273 -> 279 casos.
           1.80.0 - F1 P1b: +1 caso (forja fmt em arquivo sem '\n' final, diff sem crash): 272 -> 273 casos.
           1.79.0 - F1 P1b: +9 casos (cli_casos_fmt.py: forja fmt em dry-run, --aplicar, --json, pasta com bom + ruim + erro, BOM+CRLF, caso degenerado, --stdin, usos errados e so-leitura): 263 -> 272 casos.
           1.78.0 - S4 P7: Rodar ganha Pasta (cwd da execucao; padrao tests/cli) para os casos de std que gravam no disco; +2 casos (std:file com 18 testes e std:os com 3, texto e --json iguais em C e x64) e -1 (CasoS4StdSemTestes sai: o caso 'forja test sem test' segue em cli_casos_frontend com sem_teste.fj): 262 -> 263 casos.
           1.77.0 - S4 P6: +2 casos (cli_casos_s4.py: std:fmt com 30 testes e std:time com 10, texto e --json iguais em C e x64): 260 -> 262 casos.
           1.76.0 - S4 P5: +2 casos (cli_casos_s4.py: std:text com 29 testes e std:path com 6, texto e --json iguais em C e x64); o caso "sem testes" passa a usar std:file: 258 -> 260 casos.
           1.75.0 - S4 P4: +3 casos (cli_casos_s4.py: std:math, std:rand e std:mem com N testes exato, texto e --json iguais em C e x64); std:math passa a 8 testes: 255 -> 258 casos.
           1.74.0 - S4 P2: +5 casos (cli_casos_s4.py: std:math em C e x64 com o mesmo --json, std:nada E1001, xx:y exit 2, std:text sem testes): 250 -> 255 casos.
           1.73.0 - S4 P1: +6 casos (cli_casos_s4.py: transitivo, filtro, json com rotulo, c x64 iguais, falha qualificada, titulo igual em modulos); CasoModulosTeste espera o teste do modulo (exit 1): 244 -> 250 casos.
           1.72.0 - P-IA1-LG: +3 casos (cli_casos_build.py: sonda de versao do gcc com o gcc_falso.exe: 16.1.0 leva -fdisable-tree-widening_mul e a nota some do stderr, 15.2.0 e resposta ilegivel ficam sem a flag): 241 -> 244 casos (o changelog 1.71.0 contou 240; o motor contava 241).
           1.71.0 - IA1-L3d1: +3 casos (cli_casos_migrar.py: forja migrar em dry-run, com --aplicar numa pasta e o arquivo com erro / usos errados) e +1 (cli_casos_spec.py: CasoSpecCitaMigrar): 236 -> 240 casos.
           1.70.0 - IA1-L3a: +1 caso (cli_casos_spec.py: CasoMigracaoRenomeados, a secao RENOMEADOS da migracao.txt igual a tabela de renomeados): 235 -> 236 casos.
           1.69.0 - IA1-L1: +3 casos (cli_casos_idioma.py: E0110 igual no check e nos 2 backends, forja corrigir do '&&', explicar/spec do E0110 e as fixtures err_E0110_*): 232 -> 235 casos.
           1.68.0 - IA1-I5: +3 casos (cli_casos_editar.py: forja editar inserir_decl em dry-run e com --aplicar, mover_decl entre modulos e remover_decl recusado; fixture tests\\editar\\proj2): 229 -> 232 casos.
           1.67.0 - IA1-I4: +3 casos (cli_casos_editar.py: forja editar renomear em dry-run, com --aplicar, colisao desfeita e usos errados; fixture tests\\editar\\proj): 226 -> 229 casos.
           1.66.0 - IA1-I3: +3 casos (cli_casos_mapa.py: consultar callers/callees/imports/api/testes/entrada, forja mapa --json/--resumo e usos errados; fixtures tests\\consultar\\proj e proj2): 223 -> 226 casos.
           1.65.0 - IA1-b14: +2 casos (cli_casos_relacionados.py: relacionados do E0201 com o parecido e a struct; mesmos relacionados no check e nos dois backends do build; --resumo sem eles): 221 -> 223 casos.
           1.64.0 - IA1-b13: +2 casos (backend C com --rastreio: run com coluna, valor e pilha e R0004 com a pilha do main; test) e o de uso passa a exigir a linha igual a do x64: 219 -> 221 casos.
           1.63.0 - IA1-b12: +3 casos (cli_casos_rastreio.py: run --rastreio x64 com coluna, valor e pilha de 2 quadros; sem a flag nada muda; check/ast = exit 2 e --backend c ignora): 216 -> 219 casos.
           1.62.0 - IA1-I2: forja consultar: +3 casos (cli_casos_consultar.py: as perguntas em JSON, posicoes deterministicas, usos errados e projeto com erro; projeto tests\\consultar\\proj): 213 -> 216 casos.
           1.61.0 - IA1-I1: forja ast 1.0 (ids, fim, ref; ast_versao 1.0.0 no lugar de 0.11.0): +5 casos (cli_casos_ast.py: ids/fim/ref, --sem-pos, --no, --projeto e pasta, ids estaveis; fixture ast_pasta\\): 208 -> 213 casos.
           1.60.0 - IA1-b10: +5 casos (cli_casos_limites.py: explicar R0013..R0015, --limite-tempo/--limite-mem, --limite-saida, R-code enriquecido e --args-file com usos errados): 203 -> 208 casos.
           1.59.0 - IA1-b09: +7 casos (cli_casos_spec.py: spec byte a byte, --json, --secao e usos errados, spec CODIGO e --exemplo, explicar --lista e --exemplos, spec/spec_ref no diagnostico): 196 -> 203 casos.
           1.58.0 - IA1-b05: +2 casos (cli_casos_higiene.py: forja corrigir com diff, --aplicar e caminho absoluto; uso errado = exit 2): 194 -> 196 casos.
           1.57.0 - IA1-b03: os 2 casos de foreign ausente do backend C esperam X0102/X0101 em stdout (antes B0004 em stderr) e +1 caso (cli_casos_ffi.py: CasoParidadeB03, M-D5): 193 -> 194 casos.
           1.56.0 - IA1-b01: +2 casos (cli_casos_higiene.py: explicar E1006; import std:proc = E1001 com std:os e x/proc sem as = E1006, no check e nos 2 backends): 191 -> 193 casos.
           1.55.0 - M5 P27b: o x86_64-linux grava o ELF (CasoTargetLinux/Release/Test esperam exit 0 e ELF64), +2 casos (CasoTargetLinuxDeterministico, CasoDebugJsonlLinux), esquema do debug.jsonl 0.7.0: 189 -> 191 casos.
           1.54.0 - M5 P27a (D-M5-9): +8 casos de alvo (cli_casos_nativo.py: x86_64-linux chega a Imagem e para em 'escritor ELF: P27b'; --release, --backend c/--modo c, --modo dll, run, test com e sem -o, foreign com link, --projeto; --suporte --json com os dois alvos) e o caso CasoTargetLinux reescrito: 181 -> 189 casos.
           1.53.1 - M4 P25b: os casos FORJA_CC invalido, foreign lib/simbolo ausente (B0004) e run -- args levam --backend c explicito (testam o caminho C) e +2 casos x64 das mesmas fixtures foreign (X0102, X0101): 179 -> 181 casos.
           1.53.0 - M4 P25b (D-M4-8, degrau dev): o caso "--debug-code sem --backend" vira sucesso em dev, +1 caso X0003 em --release e +2 casos "padrao do backend" (dev = x64, release = C): 176 -> 179 casos.
           1.52.0 - M4 P24p: +1 caso (cli_casos_nativo.py: registro poda do .debug.jsonl e erro de tipo em proc podado com o mesmo E-code): 175 -> 176 casos.
           1.51.0 - M4 P24a: +2 casos (cli_casos_nativo.py: registro tempos do .debug.jsonl com os 14 campos; --tempos x64 com os 13 nomes + emit_ns): 173 -> 175 casos.
           1.50.0 - M3b P23y-2: +1 caso (cli_casos_estatica.py: X0109 mais de 256 nomes importados de DLL): 172 -> 173 casos.
           1.49.0 - M3b P23g: +5 casos (cli_casos_estatica.py: X0103 arquivo que nao e biblioteca, X0105 ar sem indice, X0106, X0107, X0108): 167 -> 172 casos.
           1.48.1 - M3b P23e: o caso da biblioteca estatica (.a) espera a estatica ligada (exit 0), nao mais X0103; mesma contagem (167).
           1.48.0 - M3 P22-O2: +2 casos (build --projeto --backend c = X0003; build --projeto com -o = uso errado): 165 -> 167 casos.
           1.47.0 - M3 P22-O1a: +3 casos (--entradas lista raiz, 2 modulos e DLL com hash certo; --entradas com --backend c = X0003; --lib-pasta acha DLL fora da pasta do .fj): 162 -> 165 casos.
           1.46.0 -M3 P22a: +15 casos (13 variantes --backend x64 dos casos ctypes e PHP FFI da DLL por ComBackendX64, o .h byte a byte igual ao do C e o FreeLibrary com R0009): 147 -> 162 casos.
           1.45.0 -M3 P21v-c: +1 caso (proc_ind.fj x64 igual ao C) e o X0001 com ir volta a usar a injecao (callback.fj): 147 casos.
           1.44.0 - Linguagem v1.12 P21v-b: o run de proc_valor.fj imprime 8, 10, 3, 14 (era a falha controlada do P21v-a); o X0001 com ir e o --suporte passam a usar a chamada indireta real (x/proc_ind.fj): 146 casos.
           1.43.0 -Linguagem v1.12 P21v-a: +3 casos (explicar E0909/E0910/E0911, check e run controlado de proc_valor.fj): 146 casos.
           1.42.0 - M3 P21: o caso "X0001 com ir" usa a injecao FORJA_TESTE_INJETAR=X0001 e o --suporte confere callback.fj sem ocorrencia (callback.fj roda no x64): 143 casos.
           1.41.0 - M3 P19b: +1 caso (X0105 com import library de 32 bits, tests/nativo/x/fx_lib32.lib): 143 casos.
           1.40.0 - M3 P19: +3 casos (X0101, X0102, X0103); o caso "bir X0001: chamada a foreign" vira "x64 igual ao C: foreign" e o X0001 com ir e o --suporte passam para callback.fj: 142 casos.
           1.39.0 - Linguagem v1.11 L16b: +1 caso (explicar R0011): 139 casos.
           1.38.0 -Linguagem v1.11 L16a: +5 casos (forja test --filtro: 1 de 3 e nenhum; --json com expect falho e com R0002; --json igual entre c e x64): 138 casos.
           1.37.0 - Linguagem v1.11 L10a-bis: +3 casos (cli_casos_higiene.py: explicar E0708 com _r, E0709 e E0601 com _ = try f()): 133 casos.
           1.36.0 - Linguagem v1.11 L10a: +4 casos (cli_casos_higiene.py: explicar E0707, E0708, E0709 e E1005): 130 casos.
           1.35.0 - Linguagem v1.11 L9: +3 casos (cli_casos_layout.py: explicar E0328, explicar E0329, check de static_assert falso): 126 casos.
           1.34.0 -Linguagem v1.11 L7: +2 casos (check --max-erros 1 devolve 1 linha; --max-erros 0 e uso errado): 123 casos.
           1.33.0 - Linguagem v1.11 L6: +4 casos (coluna_fim de E0201 de 4 letras; check --stdin ok, com erro e com import relativo); Rodar aceita Entrada.
           1.32.0 - Linguagem v1.11 L2: +1 caso (E0706 com o extra "cadeia" depois de forma_certa): 116 casos.
           1.31.0 - Linguagem v1.11 L1: +2 casos (explicar E0330; E0301 com sugestao): 115 casos.
           1.30.0 - P18a: +4 casos (explicar X0801, X0802, X0803; --traco com stderr igual entre c e x64): 113 casos.
           1.29.0 - M2 P17c: +1 caso (x64: o 01_ola importa so o que usa e cabe em 20 KB; G4 do ligador): 109 casos.
           1.28.0 - M2 P13a: o caso "bir X0001: print de str" vira "x64 igual ao C: print de str" e o X0001 com ir passa para foreign.fj (mesmos 108 casos).
           1.27.0 - M2 P12b: forja test --backend x64: o caso "X0003" vira "sem testes (exit 0)" e +2 casos (passa e falha, bytes iguais ao C): 108 casos.
           1.26.0 - M2 P12a: o caso "bir X0001: resultado" vira "x64 igual ao C: resultado" (mesmos 106 casos).
           1.25.0 - M2 P11a: o caso "bir X0001: struct" vira "x64 igual ao C: struct"; esquema do debug.jsonl 0.2.0 (mesmos 106 casos).
           1.24.0 - M2 P9: o caso "bir X0001: divisao" vira "x64 igual ao C: divisao"; --suporte --json usa foreign.fj (mesmos 106 casos).
           1.23.0 - forja run -- args: +2 casos (cli_casos_run_args.py, fixture run_args.fj): 106 casos.
           1.22.0 - P9a: o arquivo de 1688 linhas virou motor + 4 arquivos de casos (mesmos 104 casos, mesma ordem e mensagens).
           1.21.0 - backend x64 P8b: +11 casos (X0302 com -o em pasta, 6 X09xx injetados com pacote de relato, X0001 com ir, debug.jsonl valido e 100% coberto,
                    builds deterministicos, --suporte --json). O caso do jsonl usa jsonschema: rodar com python sem -I.
           1.20.0 - backend x64 P8: +5 casos (--debug-code: item invalido e 'ir', sem x64, so build, 6 secoes do 01_ola; --tempos do build x64).
           1.19.0 - backend x64 P7b: o caso "--backend x64 -> X0003" vira build --modo c --backend x64; +2 casos (test e dll com x64 -> X0003).
           1.18.0 - backend x64 P6: +6 casos (teste_nativo.exe bir: X0001 exato de real, print de str, struct, resultado, divisao e foreign).
           1.17.0 - backend x64 P3: +7 casos (explicar X0302 e X0901..X0906).
           1.16.0 - backend x64 M0-R: +8 casos (--backend c ok, --backend x64 e xx, --target x86_64-linux e xx, explicar X0001..X0003).
           1.15.0 - Linguagem v1.10 (A6): +1 caso (CasoForeignAlias, fixture foreign_alias.fj).
           1.14.0 - Linguagem v1.10: ast_versao 0.11.0 (alias de simbolo do foreign).
           1.13.0 -Linguagem v1.9: ast_versao 0.10.0 (P2); CasoAstV19 e CasoExplicarV19 (P12).
           1.12.0 - Linguagem v1.8b P9: +1 caso (CasoAstV18b, fixture ast_v18b.fj).
           1.11.0 - Linguagem v1.8b P6/P7: +2 casos no P6 (CasoVendor, CasoEngineSemVariavel) e +2 no P7 (CasoLigarSemArgumento, CasoExplicarV18b).
           1.10.0 - Linguagem v1.8 P8: +7 casos de modulos, vazamento e AST de import.
           1.9.0 - Linguagem v1.7 P9: 4 casos (ast 0.7.0 de v17.fj, --modo c e run, DLL com cstr/?cstr e forja_liberar, explicar E0312..E0315).
           1.8.0- Linguagem v1.7 P1: CasoForeignAst, CasoArrayAst e CasoMemoriaAst esperam ast_versao 0.7.0.
           1.7.0 - Linguagem v1.6 P8: 6 casos de memoria (ast 0.6.0 com lista, --modo c, DLL com lista e new, delete duplo R0008,
                   indice fora de lista R0006, explicar E0310 E0311 R0007 R0008).
           1.6.0 - Linguagem v1.6 P1a: CasoForeignAst e CasoArrayAst esperam ast_versao 0.6.0.
           1.5.1 - Linguagem v1.5 P1a: CasoForeignAst espera ast_versao 0.5.0.
           1.5.0 - Linguagem v1.4 P5: 7 casos de foreign (DLL da Forja por -L/PATH, B0004 de lib e de simbolo, --modo c, ast 0.4.0,
                   DLL com foreign, explicar dos codigos novos).
           1.4.0 - Linguagem v1.3 P7: DLL com ponteiros e struct por valor (ctypes, .h, PHP FFI), NULL na DLL de dev (101, R0005) e
                   explicar dos codigos novos (E0306 E0307 E0706 R0005).
           1.3.0 - Linguagem v1.2: DLL com numericos (ctypes, .h, PHP FFI), estouro na DLL de dev (101) e explicar dos codigos novos.
           1.2.0 - Linguagem v1.1: 'pub export proc' (simbolo limpo; fj_* escondido), tipos da ABI, .h valido, PHP FFI (SKIP sem php),
                   B0003 e R0004 (main -> ! E).
           1.1.0 - Fase 5i: build --modo dll (a DLL carrega por ctypes e fj_somar/fj_fatorial respondem) e --modo invalido (exit 2).
           1.0.1 - Fase 5i: o caminho sai so no 1o #line (volume do C); checagens usam FORJA_ARQ.
           1.0.0 - Fase 5h: primeira versao (11 casos com as fixtures tests/cli/*.fj).
Verificacao: python verificar_cli.py --selftest   |   python verificar_cli.py --forja <forja.exe>
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
ForjaPadrao = PastaAqui.parent.parent / "compiler" / "forja.exe"
ExemploOla = PastaAqui.parents[2] / "examples" / "01_ola.fj"
TimeoutSegundos = 120

# Os arquivos de casos fazem "from verificar_cli import ..."; com python -I a pasta do script nao entra no sys.path,
# e rodando como script este modulo se chama __main__ (o alias evita carregar o motor duas vezes).
sys.path.insert(0, str(PastaAqui))
if __name__ == "__main__":
    sys.modules.setdefault("verificar_cli", sys.modules[__name__])


def UltimaLinha(Texto):
    Linhas = [Linha for Linha in Texto.splitlines() if Linha.strip()]
    return Linhas[-1] if Linhas else ""


def LinhaJson(Texto):
    """O texto inteiro como um dict JSON de uma linha; None se nao for isso."""
    Linhas = [Linha for Linha in Texto.splitlines() if Linha.strip()]
    if len(Linhas) != 1:
        return None
    try:
        return json.loads(Linhas[0])
    except json.JSONDecodeError:
        return None


def UltimoJson(Texto):
    """A ultima linha do texto como JSON (o ld escreve a mensagem dele antes do JSON do B0004); None se nao for JSON."""
    try:
        return json.loads(UltimaLinha(Texto))
    except json.JSONDecodeError:
        return None


def Rodar(Forja, Argumentos, Ambiente=None, Entrada=None, Pasta=None):
    """Roda o forja; Entrada (texto) vai para a entrada padrao (check --stdin, L6); Pasta = cwd (padrao: tests\\cli)."""
    return subprocess.run(
        [str(Forja)] + Argumentos, capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=TimeoutSegundos, env=Ambiente, cwd=Pasta or PastaAqui, input=Entrada,
    )


def ConferirExecucao(Rodada, CodigoEsperado, UltimaEsperada=None, TrechoStdout=None):
    Problemas = []
    if Rodada.returncode != CodigoEsperado:
        Problemas.append(f"exit {Rodada.returncode}, esperado {CodigoEsperado}")
    if UltimaEsperada is not None and UltimaLinha(Rodada.stdout) != UltimaEsperada:
        Problemas.append(f"ultima linha do stdout {UltimaLinha(Rodada.stdout)!r}, esperado {UltimaEsperada!r}")
    if TrechoStdout is not None and TrechoStdout not in Rodada.stdout:
        Problemas.append(f"stdout sem {TrechoStdout!r}")
    return Problemas


def ConferirJsonCodigo(Texto, CodigoEsperado):
    Dados = LinhaJson(Texto)
    if Dados is None or Dados.get("codigo") != CodigoEsperado:
        return [f"saida nao e 1 linha JSON com codigo {CodigoEsperado}: {Texto.strip()[:120]!r}"]
    return []


BackendDll = None  # "x64" dentro de ComBackendX64: ConstruirDll gera a DLL pelo nativo (M3 P22a); None = backend C (gcc)


def ComBackendX64(Caso):
    """Variante --backend x64 de um caso de DLL (ctypes ou PHP FFI): o mesmo caso, a DLL vem do nativo e mora em <saidas>\\x64 (o caso do C pode ter a dele carregada)."""
    def Variante(Forja, PastaSaidas):
        global BackendDll
        Pasta = PastaSaidas / "x64"
        Pasta.mkdir(exist_ok=True)
        BackendDll = "x64"
        try:
            return Caso(Forja, Pasta)
        finally:
            BackendDll = None
    return Variante


def ConstruirDll(Forja, PastaSaidas, Fonte, Nome, Release=True):
    """build --modo dll (--release por padrao) de uma fixture; devolve (Problemas, caminho da DLL, caminho do .h)."""
    Dll = PastaSaidas / f"{Nome}.dll"
    Opcoes = ["--release"] if Release else []
    if BackendDll is not None:
        Opcoes += ["--backend", BackendDll]
    Rodada = Rodar(Forja, ["build", "--modo", "dll"] + Opcoes + [Fonte, "-o", str(Dll)])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Dll.exists():
        Problemas.append(f"build --modo dll nao gerou {Dll.name}")
    return Problemas, Dll, Dll.with_suffix(".h")


# Importados aqui, depois das funcoes comuns: os arquivos de casos pegam essas funcoes deste modulo (import circular controlado).
import cli_casos_frontend as Frontend  # noqa: E402
import cli_casos_build as Build  # noqa: E402
import cli_casos_ffi as Ffi  # noqa: E402
import cli_casos_nativo as Nativo  # noqa: E402
import cli_casos_estatica as Estatica  # noqa: E402
import cli_casos_run_args as RunArgs  # noqa: E402
import cli_casos_layout as Layout  # noqa: E402
import cli_casos_higiene as Higiene  # noqa: E402
import cli_casos_spec as Spec  # noqa: E402
import cli_casos_limites as Limites  # noqa: E402
import cli_casos_ast as Ast  # noqa: E402
import cli_casos_consultar as Consultar  # noqa: E402
import cli_casos_mapa as Mapa  # noqa: E402
import cli_casos_editar as Editar  # noqa: E402
import cli_casos_rastreio as Rastreio  # noqa: E402
import cli_casos_relacionados as Relacionados  # noqa: E402
import cli_casos_idioma as Idioma  # noqa: E402
import cli_casos_migrar as Migrar  # noqa: E402
import cli_casos_s4 as S4  # noqa: E402
import cli_casos_fmt as Fmt  # noqa: E402
import cli_casos_dll_info as DllInfo  # noqa: E402
import cli_casos_pacote as Pacote  # noqa: E402

CASOS = [
    ("test passa", Frontend.CasoTestePassa),
    ("test com expect falso", Frontend.CasoTesteFalha),
    ("arquivo sem test", Frontend.CasoSemTeste),
    ("run com panic (101)", Frontend.CasoRunPanico),
    ("run --release", Frontend.CasoRunRelease),
    ("explicar E0301", Frontend.CasoExplicarE0301),
    ("explicar R0001", Frontend.CasoExplicarR0001),
    ("explicar codigo desconhecido", Frontend.CasoExplicarDesconhecido),
    ("build com erro de tipo", Build.CasoBuildComErro),
    ("Portao Z b1: divisao real por zero constante (f32 e f64): build exit 1 com E0420 e o conselho math.inf()/math.nan()", Build.CasoBuildDivisaoRealZeroE0420),
    ("0.21.0 F4: --backend c, --modo c (com e sem -o, com --release) e --prova-layout = X0003 (exit 2, stderr)", Build.CasoX0003),
    ("build --modo dll: simbolo limpo, sem fj_", Ffi.CasoDllCarrega),
    ("dll: tipos da ABI (f32, bool, enum, ^Struct)", Ffi.CasoDllTipos),
    ("dll: .h sem '#' e valido em C", Ffi.CasoCabecalhoC),
    ("dll: PHP FFI::cdef", Ffi.CasoPhpFfi),
    ("dll: numericos (i16 u8 u32 uint f64)", Ffi.CasoDllNumericos),
    ("dll: .h dos numericos valido em C", Ffi.CasoCabecalhoNumericos),
    ("dll: PHP FFI numericos", Ffi.CasoPhpFfiNumericos),
    ("dll de dev: estouro sai com 101 (R0002)", Ffi.CasoDllEstouroSai101),
    ("explicar E0303 E0304 E0305 E0420 E0609", Frontend.CasoExplicarNovos),
    ("dll: ponteiros, rawptr, cstr e struct por valor", Ffi.CasoDllPonteiros),
    ("dll: .h com struct por valor e ponteiros valido em C", Ffi.CasoCabecalhoPonteiros),
    ("dll: PHP FFI ponteiros e struct por valor", Ffi.CasoPhpFfiPonteiros),
    ("dll de dev: NULL em ^T sai com 101 (R0005)", Ffi.CasoDllNuloSai101),
    ("explicar E0306 E0307 E0706 R0005", Frontend.CasoExplicarV13),
    ("dll sem pub export (B0003)", Build.CasoDllSemExport),
    ("main -> ! E devolve erro (R0004, exit 1)", Frontend.CasoMainDevolveErro),
    ("--modo desconhecido", Build.CasoModoInvalido),
    ("foreign: DLL da Forja (-L, -Bdynamic, PATH do run)", Ffi.CasoForeignDllDaForja),
    ("foreign: biblioteca ausente (X0102 no stdout, stderr vazio; mesma fixture do x64)", Ffi.CasoForeignLibAusente),
    ("foreign: simbolo ausente (X0101 no stdout, stderr vazio; mesma fixture do x64)", Ffi.CasoForeignSimboloAusente),
    ("foreign: biblioteca ausente no x64 (X0102, mesma fixture)", Ffi.CasoForeignLibAusenteX64),
    ("foreign: simbolo ausente no x64 (X0101, mesma fixture)", Ffi.CasoForeignSimboloAusenteX64),
    ("b03: M-D5 x64 contra o congelado (esperados md5_x64_*), import sem texto solto, B0004 com ferramenta", Ffi.CasoParidadeB03),
    ("foreign: run do rotulo e comentario (foreign_libm.fj imprime 12 e 1024)", Build.CasoForeignRun),
    ("foreign: alias de simbolo C (v1.10 A6)", Build.CasoForeignAlias),
    ("foreign: ast 0.5.0 (link, externo)", Frontend.CasoForeignAst),
    ("foreign: DLL com foreign (so o export sai)", Ffi.CasoDllComForeign),
    ("explicar E0904 E0905 E0906 B0004", Frontend.CasoExplicarV14),
    ("indice constante dobrado: check passa, build E0421", Frontend.CasoIndiceConstanteDobrado),
    ("array: ast 0.5.0 (indice, fatia, literal, for por_ref)", Frontend.CasoArrayAst),
    ("array: run de array_ast.fj imprime 12 no x64", Build.CasoArrayRun),
    ("array: DLL de dev (140, 2) e pega(5) sai 101 (R0006)", Ffi.CasoArrayDll),
    ("explicar E0308 E0309 E0421 R0006", Frontend.CasoExplicarV15),
    ("memoria: ast 0.6.0 (tipo dinamico e nome)", Frontend.CasoMemoriaAst),
    ("memoria: run de memoria_modo_c.fj imprime 1 no x64 (crescer, alocar e frame_reset)", Build.CasoMemoriaRun),
    ("memoria: DLL de dev com lista e new (45, 100) e soltar duplo 101 (R0008)", Ffi.CasoListaDll),
    ("memoria: delete duplo em dev sai 101 (R0008)", Frontend.CasoDeleteDuploDev),
    ("memoria: indice fora de lista em dev sai 101 (R0006)", Frontend.CasoListaIndiceForaDev),
    ("explicar E0310 E0311 R0007 R0008", Frontend.CasoExplicarV16),
    ("v1.7: ast 0.7.0 (opcional, union, let_multiplo, ftexto, retornos)", Frontend.CasoV17Ast),
    ("v1.7: run de v17.fj (opcional, tupla, fmt) imprime '3 1-2' e 'x' no x64", Build.CasoV17Run),
    ("v1.7: DLL de dev com cstr e ?cstr, forja_liberar e liberar duplo 101 (R0008)", Ffi.CasoExportCstrDll),
    ("explicar E0312 E0313 E0314 E0315", Frontend.CasoExplicarV17),
    ("v1.8: modulos: panic no modulo (101, R0003 em mods/falha_cli.fj:3)", Frontend.CasoModulosPanico),
    ("v1.8: modulos: run de modulos_panico.fj (5, e R0003 em mods/falha_cli.fj linha 3)", Build.CasoModulosRun),
    ("S4 P1 (D1): modulos: test roda os da raiz e o do modulo (exit 1, FALHOU qualificado)", Frontend.CasoModulosTeste),
    ("v1.8: modulos: E1002 (import de si) e E0301 dentro do modulo", Frontend.CasoModulosErros),
    ("v1.8: vazamento em dev sai 101 (R0009); --release sai 0", Frontend.CasoVazamentoRun),
    ("v1.8: ast 0.8.0 com nos import e modulo", Frontend.CasoAstImport),
    ("explicar E0316 E0317 E1001 E1002 E1003 E1004 R0009", Frontend.CasoExplicarV18),
    ("v1.8b: vendor: modulo e soma.dll em vendor\\ (run -> 5)", Ffi.CasoVendor),
    ("v1.8b: engine: sem FORJA_ENGINE e E1001 1:8", Frontend.CasoEngineSemVariavel),
    ("v1.8b: forja ligar sem argumento (G0104, exit 2)", Ffi.CasoLigarSemArgumento),
    ("explicar E0907 E0908 R0010 G0001 G0026 G0101 G0105", Frontend.CasoExplicarV18b),
    ("v1.8b: ast 0.9.0 (opaque, valor de enum, tipo proc)", Frontend.CasoAstV18b),
    ("ast v1.9: map, bit_set, soa, matrix e m[i, j]", Frontend.CasoAstV19),
    ("explicar E0318..E0321", Frontend.CasoExplicarV19),
    ("v1.11: explicar E0330 (literal inteiro inexato em real)", Frontend.CasoExplicarE0330),
    ("v1.11: E0301 com sugestao \"1.5 as i32\" depois de forma_certa", Frontend.CasoSugestaoE0301),
    ("v1.11: E0706 com a cadeia x -> p -> return em JSON depois de forma_certa", Frontend.CasoCadeiaE0706),
    ("v1.11: E0201 com (parecido: 'vida') e o extra parecidos depois de forma_certa", Frontend.CasoParecidoE0201),
    ("v1.11: E0201 de nome de 4 letras com coluna_fim entre coluna e causa", Frontend.CasoColunaFimE0201),
    ("v1.11: check --stdin de usa_soma.fj sai 0 e mudo", Frontend.CasoStdinOk),
    ("v1.11: check --stdin com E0201 aponta o <caminho> dado, sem ler o disco", Frontend.CasoStdinErro),
    ("v1.11: check --stdin com import relativo ao <caminho> (mods/com_erro.fj 2:18)", Frontend.CasoStdinImport),
    ("v1.11: check --max-erros 1 devolve so o 1o dos 3 erros", Frontend.CasoMaxErrosUm),
    ("v1.11: check --max-erros 0 e uso errado (exit 2)", Frontend.CasoMaxErrosZero),
    ("v1.12 P21v-a: explicar E0909 E0910 E0911 (proc como valor)", Frontend.CasoExplicarV112ProcValor),
    ("v1.12 P21v-a: check de proc_valor.fj (proc em let/var/param/retorno e chamada indireta) sai 0 e mudo", Frontend.CasoCheckProcValor),
    ("v1.12 P21v-b: run de proc_valor.fj roda (V_PROC e chamada indireta no backend C) e imprime 8, 10, 3, 14", Frontend.CasoRunProcValor),
    ("v1.11: explicar E0328 (size_of, align_of, offset_of)", Layout.CasoExplicarE0328),
    ("v1.11: explicar E0329 (static_assert)", Layout.CasoExplicarE0329),
    ("v1.11: check de static_assert falso da E0329 com a msg na causa", Layout.CasoStaticAssertFalso),
    ("v1.11: explicar E0707 (var que nunca muda)", Higiene.CasoExplicarE0707),
    ("v1.11: explicar E0708 (let/var nunca lido)", Higiene.CasoExplicarE0708),
    ("v1.11: explicar E0709 (retorno descartado)", Higiene.CasoExplicarE0709),
    ("v1.11: explicar E1005 (import sem uso)", Higiene.CasoExplicarE1005),
    ("IA1-b01: explicar E1006 (import de ultimo segmento reservado)", Higiene.CasoExplicarE1006),
    ("IA1-b05: forja corrigir (caminho absoluto): diff sem gravar, --aplicar troca var por let e o check passa", Higiene.CasoCorrigirDiffEAplicar),
    ("IA1-b05: forja corrigir com arquivo inexistente, opcao ou codigo desconhecido = exit 2", Higiene.CasoCorrigirUsoErrado),
    ("IA1-b01: import std:proc = E1001 (sugere std:os) e x/proc sem as = E1006 na linha 1, no check e nos 2 backends", Higiene.CasoImportRenomeadoOuReservado),
    ("v1.11 L10a-bis: explicar E0708 cita o nome com sublinhado (_r)", Higiene.CasoExplicarE0708Sublinhado),
    ("v1.11 L10a-bis: explicar E0709 cita _ = try f()", Higiene.CasoExplicarE0709DescarteTry),
    ("v1.11 L10a-bis: explicar E0601 cita _ = try f()", Higiene.CasoExplicarE0601DescarteTry),
    ("IA1-b09: spec --cartao, spec e --secao migracao == os arquivos de language\\doc, byte a byte", Spec.CasoSpecBytes),
    ("IA1-L3a: a secao RENOMEADOS da migracao.txt (arquivo e forja spec --secao migracao) bate com a tabela common\\renomeados.c", Spec.CasoMigracaoRenomeados),
    ("IA1-L3d1: a secao cli do cartao e da spec e a mensagem de uso do forja citam o forja migrar", Spec.CasoSpecCitaMigrar),
    ("IA1-b09: spec --json (13 secoes por tema + migracao; versao de --versao), --cartao e --secao", Spec.CasoSpecJson),
    ("IA1-b09: spec --secao proc; secao, opcao, codigo desconhecidos e --exemplo com --cartao = exit 2", Spec.CasoSpecSecaoEUso),
    ("IA1-b09: spec CODIGO e spec --exemplo E0707 --json (errado e certo da bateria); codigo sem exemplo = exit 2", Spec.CasoSpecCodigoEExemplo),
    ("IA1-b09: explicar --lista [--json] com codigo, titulo, etapa e spec de todo codigo", Spec.CasoExplicarLista),
    ("IA1-b09: explicar [CODIGO] --exemplos (par da bateria; sem exemplo = lista vazia)", Spec.CasoExplicarExemplos),
    ("IA1-b09: todo diagnostico leva spec na 1a vez do codigo e spec_ref nas seguintes; --resumo sem spec", Spec.CasoDiagnosticoComSpec),
    ("IA1-b10: explicar R0013, R0014 e R0015 (tipo, exit, forma_certa e spec)", Limites.CasoExplicarLimites),
    ("IA1-b10: --limite-tempo 1 = R0013 e exit 102; --limite-mem 32 = R0014 e exit 103", Limites.CasoLimiteTempoEMemoria),
    ("IA1-b10: --limite-saida 4KiB corta a saida nos 2 backends = R0015 e exit 104; programa curto sob limites folgados passa", Limites.CasoLimiteSaida),
    ("IA1-b10: R-code do runtime (stderr) leva explicacao, forma_certa e spec nos 2 backends", Limites.CasoRcodeEnriquecido),
    ("IA1-b10: --args-file entrega os argumentos antes dos do '--'; arquivo ausente, valores invalidos e usos fora de run/test = exit 2", Limites.CasoArgsFileEUso),
    ("IA1-I1: forja ast 1.0 traz id estrutural, pos, fim e ref (modulo, local, embutido) em import_ast.fj; duas execucoes iguais", Ast.CasoAstIdsFimRef),
    ("IA1-I1: forja ast --sem-pos tira posicoes e o arquivo e deixa ids e ligacoes", Ast.CasoAstSemPos),
    ("IA1-I1: forja ast --no <id> devolve a subarvore (tambem de outro modulo no --projeto); id inexistente, vazio e --no fora do ast = exit 2", Ast.CasoAstNo),
    ("IA1-I1: forja ast --projeto e <pasta>: um item por modulo na ordem dos imports; ref aponta para id do documento; pasta sem main.fj = exit 2", Ast.CasoAstProjeto),
    ("IA1-I1: ids estaveis: linha em branco, comentario e proc novo antes da 1a declaracao nao mudam id nem conteudo das existentes", Ast.CasoAstIdsEstaveis),
    ("0.25.1: forja ast de #load ausente ou absoluto inexistente: AST sintatica sem ref*, exit 0, stderr vazio; check e ast --tipos seguem E1007 3:17; igual ao C com o .bin presente (sem ref*)", Ast.CasoAstLoadAusente),
    ("IA1-I2: forja consultar simbolos, assinatura, campos e refs em JSON (consultar_versao, pergunta, categorias e params certos)", Consultar.CasoConsultarPerguntas),
    ("IA1-I2: forja consultar tipo e escopo por arq:L:C; duas execucoes dao bytes iguais", Consultar.CasoConsultarPosicoes),
    ("IA1-I2: forja consultar: pergunta desconhecida lista as perguntas, nome/posicao/opcao errados = exit 2, projeto com erro = exit 1", Consultar.CasoConsultarUsoErrado),
    ("IA1-I3: forja consultar callers, callees, imports, api, testes e entrada em JSON (chaves, chamadores, ordem dos modulos, links de foreign)", Mapa.CasoConsultarParte2),
    ("IA1-I3: forja mapa --json e --resumo (chaves, texto que comeca por 'mapa main:', sem opcao = --resumo, 2 execucoes iguais, --modulo com --limite)", Mapa.CasoMapaSaidas),
    ("IA1-I3: forja mapa e consultar parte 2: opcao/argumento/nome errados = exit 2, pergunta desconhecida lista as 12, projeto com erro = exit 1", Mapa.CasoMapaUsoErrado),
    ("P79 D0653-2: consultar api std:mem, std:text, std:math e std:json sem arquivo hospedeiro (exit 0, uma proc conhecida de cada)", Consultar.CasoApiSemHostStd),
    ("P79 D0653-2: consultar api vendor:json/cjson sem arquivo hospedeiro (exit 0, CJson)", Consultar.CasoApiSemHostVendor),
    ("P79 D0653-2: consultar api --json sem host: JSON valido e bytes iguais em cwds diferentes", Consultar.CasoApiSemHostJson),
    ("P79 D0653-2: E1001 de std:nomeinexistente lista os modulos (tabela STD + vendor do disco; corte so com a marca (+N) e a conta fechando)", Consultar.CasoE1001ListaTodosOsModulos),
    ("P79 D0653-2: E1001 de std:texto sugere 'quis dizer std:text?'", Consultar.CasoE1001ParecidoTexto),
    ("P79 D0653-2: E1001 de vendor:jsn/cjson sugere 'quis dizer vendor:json/cjson?'", Consultar.CasoE1001ParecidoVendor),
    ("P79 D0653-2: E1001 de std:nomeinexistente nao sugere parecido inexistente", Consultar.CasoE1001SemParecidoNaoSugere),
    ("P79 D0653-2: spec --receitas imprime doc\\receitas.txt embutido (texto igual ao do arquivo)", Consultar.CasoReceitasEmbutido),
    ("P79 D0653-2: spec --receitas com --json e exit 2", Consultar.CasoReceitasUsoErrado),
    ("IA1-I4: forja editar renomear em dry-run: JSON com edicoes/diff/erros_antes/depois, 2 execucoes iguais e 0 arquivo gravado", Editar.CasoEditarDryRun),
    ("IA1-I4: forja editar --aplicar grava o renomear entre modulos e o check do resultado sai 0", Editar.CasoEditarAplicar),
    ("IA1-I4: forja editar: colisao de nome (E0202) e desfeita com exit 1 e 0 arquivo alterado; sem --ops, ops ruim, operacao desconhecida e opcao errada = exit 2", Editar.CasoEditarDesfazEUsoErrado),
    ("IA1-I5: forja editar inserir_decl: dry-run com a declaracao nova no diff e 0 arquivo gravado; com --aplicar grava e o check do resultado sai 0", Editar.CasoEditarInserirDecl),
    ("IA1-I5: forja editar mover_decl entre modulos: declaracao muda de arquivo, imports e chamadas ajustados, check 0 e o run igual ao da fixture", Editar.CasoEditarMoverDecl),
    ("IA1-I5: forja editar remover_decl de declaracao ainda usada e recusado (exit 1, desfeito=false, motivo com as referencias, 0 arquivo alterado, com e sem --aplicar)", Editar.CasoEditarRemoverRecusado),
    ("IA1-b12: run --rastreio x64 no R0006 = coluna, valor {indice, tamanho} e pilha pegar/main", Rastreio.CasoRastreioR0006),
    ("IA1-b12: sem --rastreio o R0006 nao leva coluna/valor/pilha; programa sem erro igual com e sem a flag", Rastreio.CasoRastreioSemFlag),
    ("IA1-b12: check/ast --rastreio = exit 2; run --backend x64 com --rastreio tem a linha R igual ao valor fixo do C (b13)", Rastreio.CasoRastreioUsoEBackendC),
    ("IA1-b13: run --rastreio no x64: coluna, valor, pilha pegar/main e R0004 com a pilha do main; sem a flag nada", Rastreio.CasoRastreioBackendC),
    ("IA1-b13: test --backend x64 --rastreio: resumo igual e R0006 do teste com pilha igual ao valor fixo do C", Rastreio.CasoRastreioTestBackendC),
    ("M6e P93d: --guarda-memoria so vale em dev, backend x64, exe (--release, --backend c e --modo dll = exit 2); em dev x64 a saida nao muda", Rastreio.CasoGuardaMemoriaOpcao),
    ("M8 P2: --perfil so vale em build, run e test de exe (--modo dll e --suporte = exit 2); o exe com --perfil roda igual e grava <exe>.perfil.json e <exe>.perfil.txt", Rastreio.CasoPerfilOpcao),
    ("M8 P2: forja test --perfil de dois_testes.fj = exit 0 e '2 testes, 0 falharam'", Rastreio.CasoPerfilTest),
    ("O0 P13: --checagem sim|nao: nao em dev e valor invalido = exit 2; build --release com sim e com nao = exit 0", Rastreio.CasoChecagemOpcao),
    ("M7a F0: --sem-otim so vale com --release (dev, passo inventado e --projeto = exit 2); build --release com tudo no x64 = exit 0", Rastreio.CasoSemOtimOpcao),
    ("IA1-b14: check do E0201 traz relacionados (parecido 'vida' na linha 3 e a struct Nave na linha 2)", Relacionados.CasoRelacionadosCampoParecido),
    ("IA1-b14: check e build nos dois backends dao os mesmos relacionados; o --resumo nao os traz", Relacionados.CasoRelacionadosIguaisNosBackends),
    ("IA1-L1: E0110 do '&&' igual no check e nos builds x64 e c (codigo, linha, coluna) e nenhum exe gerado", Idioma.CasoIdiomaMesmoDiagnosticoNosBackends),
    ("IA1-L1: forja corrigir --aplicar troca '&&' por 'and' (sem --aplicar nada e gravado) e o check do resultado sai 0", Idioma.CasoIdiomaCorrigirAplica),
    ("O0 P13: 'a ^ b' = E0110 com a troca por '~'; p^.campo, pm^[i], pn^ + 1 seguem como deref e 'x^' sozinho segue E0306", Idioma.CasoIdiomaXorSoComOperando),
    ("IA1-L1: explicar e spec do E0110; as fixtures err_E0110_* de tests\\sintaxe dao E0110 na linha esperada", Idioma.CasoIdiomaCatalogoESpec),
    ("IA1-L3d1: forja migrar em dry-run (padrao e --dry-run --json): JSON com 5 trocas, 0 arquivo gravado, bytes iguais", Migrar.CasoMigrarDryRun),
    ("IA1-L3d1: forja migrar --aplicar numa pasta: 2 arquivos iguais ao .novo.fj, check 0 e a 2a passada com 0 trocas", Migrar.CasoMigrarAplica),
    ("IA1-L3d1: forja migrar: arquivo com erro = nao_analisado, exit 1, intacto; sem entrada, --prova, --dry-run com --aplicar e entrada inexistente = exit 2", Migrar.CasoMigrarNaoAnalisadoEUsoErrado),
    ("IA1-L3d2: forja migrar --comentarios: codigo Forja em comentario reescrito (17 trocas, igual ao .novo.fj, check 0, 2a passada 0) e prosa em revisao_comentario sem edicao", Migrar.CasoMigrarComentarios),
    ("S4 P1: forja test roda os test transitivos (raiz -> a -> b), na ordem da IR", S4.CasoS4Transitivo),
    ("S4 P1: forja test --filtro \"[mods/a]\" roda so o test de mods/a (2 filtrados)", S4.CasoS4Filtro),
    ("S4 P1: forja test --json: o campo teste leva o rotulo qualificado do modulo", S4.CasoS4JsonRotulo),
    ("S4 P1: forja test transitivo: x64 contra valor fixo (stdout)", S4.CasoS4BackendIgual),
    ("S4 P1: forja test: falha em teste de modulo = exit 1, FALHOU [mods/falha] e JSON com o arquivo do modulo", S4.CasoS4FalhaQualificada),
    ("S4 P1: check: o mesmo titulo em modulos diferentes e aceito (E0802 so no mesmo modulo)", S4.CasoS4TituloIgualEmModulos),
    ("S4 P2: forja test std:math (backend c): os 8 testes da std passam, com o rotulo [std:math]", S4.CasoS4StdMathC),
    ("S4 P2: forja test std:math: x64 contra valor fixo (--json)", S4.CasoS4StdMathX64Igual),
    ("S4 P2: forja test std:nada: E1001 existente, exit 1", S4.CasoS4StdNadaE1001),
    ("S4 P2: forja test xx:y: prefixo desconhecido = uso errado, exit 2", S4.CasoS4PrefixoDesconhecido),
    ("S4 P4: forja test std:math: 8 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdMathIgual),
    ("S4 P4: forja test std:rand: 9 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdRandIgual),
    ("S4 P4: forja test std:mem: 4 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdMemIgual),
    ("S4 P5: forja test std:text: 58 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdTextIgual),
    ("S4 P5: forja test std:path: 6 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdPathIgual),
    ("S4 P6: forja test std:fmt: 30 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdFmtIgual),
    ("S4 P6: forja test std:time: 23 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdTimeIgual),
    ("S4 P7: forja test std:file: 24 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdFileIgual),
    ("S4 P7: forja test std:os: 12 testes, texto e --json x64 contra valor fixo", S4.CasoS4StdOsIgual),
    ("F1 P1b: forja fmt dry-run: exit 1, diff e rodape, arquivo intacto e 2 execucoes iguais", Fmt.CasoFmtDryRun),
    ("F1 P1b: forja fmt --aplicar: arquivo igual ao depois, rodape gravado e 2a passada canonica", Fmt.CasoFmtAplica),
    ("F1 P1b: forja fmt --json: estado, edicoes com regra, totais; --aplicar --json grava", Fmt.CasoFmtJson),
    ("F1 P1b: forja fmt pasta: bom + ruim + erro (nao_analisado intacto), exit 1; --aplicar so grava o ruim", Fmt.CasoFmtPasta),
    ("F1 P1b: forja fmt BOM+CRLF: 2 desvios, dry-run sem diff de texto e --aplicar grava LF sem BOM", Fmt.CasoFmtBomCrlf),
    ("F1 P1b: forja fmt caso degenerado (so espaco): diff de apagamento e --aplicar deixa vazio", Fmt.CasoFmtDegenerado),
    ("F1 P1b: forja fmt arquivo sem '\\n' final: dry-run mostra o diff (sem crash) e --aplicar grava o depois", Fmt.CasoFmtSemFimDeLinha),
    ("F1 P1b: forja fmt --stdin: texto canonico, --regras, erro de sintaxe e --stdin com --aplicar/--json/caminho = exit 2", Fmt.CasoFmtStdin),
    ("F1 P1b: forja fmt: sem entrada, caminho ausente, opcao desconhecida e --regras sem --stdin = exit 2", Fmt.CasoFmtUsoErrado),
    ("F1 P1b: forja fmt --aplicar em arquivo so-leitura: exit 2, arquivo intacto e sem sobra na pasta", Fmt.CasoFmtSoLeitura),
    ("Portao Z b1: forja fmt --aplicar em .fj nao canonico so-leitura: exit diferente de 0, arquivo intacto e sem sobra", Fmt.CasoFmtSoLeituraNaoSobrescreve),
    ("F1 P2: check --canonico em arquivo canonico: exit 0 sem diagnostico", Fmt.CasoCheckCanonicoPassa),
    ("F1 P2: check --canonico em arquivo fora da forma: 1 F0001 (linha, coluna, causa, correcao certa), exit 1", Fmt.CasoCheckCanonicoAcusaF0001),
    ("F1 P2: check sem --canonico: o mesmo arquivo passa (exit 0, nada muda)", Fmt.CasoCheckSemCanonicoIgual),
    ("F1 P2: check --canonico com erro de sintaxe ou semantico: so o erro, sem F0001", Fmt.CasoCheckCanonicoComErro),
    ("F1 P2: --canonico fora do check (run, build, ast): exit 2", Fmt.CasoCanonicoForaDoCheck),
    ("0.25.1: check --canonico --resumo imprime o F0001 na forma resumo (SaidaDiag unico), igual ao forja_c.exe", Fmt.CasoCheckCanonicoResumo),
    ("F1 P2: forja explicar F0001: catalogo em 1 linha JSON", Fmt.CasoExplicarF0001),
    ("backend: build --modo c --backend x64 (X0003, exit 2)", Nativo.CasoBackendX64),
    ("backend: test --backend x64 sem testes (exit 0)", Nativo.CasoBackendX64Test),
    ("backend: build --modo dll --backend x64 (X0003, exit 2)", Nativo.CasoBackendX64Dll),
    ("backend: --backend xx fora da lista (X0003, exit 2)", Nativo.CasoBackendXx),
    ("debug-code: item desconhecido e 'ir' (X0003, exit 2)", Nativo.CasoDebugCodeItemInvalido),
    ("debug-code: sem --backend em dev usa o x64 (exit 0, .debug.txt)", Nativo.CasoDebugCodeSemBackendEmDev),
    ("debug-code: --release sem --backend usa o x64 (exit 0, .debug.txt)", Nativo.CasoDebugCodeReleaseSemBackend),
    ("debug-code: item passes, a BIR depois de cada passe (G0d)", Nativo.CasoDebugPassesSecao),
    ("debug-code: 2 builds no mesmo caminho = .debug.txt (passes, bir, asm...) e exe iguais (G0d)", Nativo.CasoDebugPassesDeterministico),
    ("padrao do backend: build em dev sem --backend e x64 (assinatura Forja Native)", Build.CasoPadraoDevEhX64),
    ("padrao do backend: build --release sem --backend e x64 (com a assinatura)", Build.CasoPadraoReleaseEhX64),
    ("debug-code: so vale em build (uso, exit 2)", Nativo.CasoDebugCodeSoBuild),
    ("debug-code: 6 secoes no .debug.txt do 01_ola", Nativo.CasoDebugCodeSecoes),
    ("tempos: build x64 traz as 10 etapas e o outros_ns", Nativo.CasoDebugCodeTemposJson),
    ("tempos: registro do .debug.jsonl com os 14 campos e outros_ns >= 0", Nativo.CasoTemposRegistroJsonl),
    ("tempos: --tempos x64 com os 13 nomes do registro e o emit_ns", Nativo.CasoTemposArquivoX64),
    ("poda: registro do .debug.jsonl (5/3/2) e erro de tipo em proc podado com o mesmo E-code", Nativo.CasoPodaRegistroJsonl),
    ("alvo: --target x86_64-linux implica x64 e grava um ELF64 estatico (sem_teste.fj e 01_ola)", Nativo.CasoTargetLinux),
    ("alvo: --release com --target x86_64-linux fica no x64 e grava o ELF", Nativo.CasoTargetLinuxRelease),
    ("alvo: dois builds do 01_ola para o linux (dev e release) dao bytes iguais e < 64 KiB", Nativo.CasoTargetLinuxDeterministico),
    ("alvo: debug.jsonl do linux no esquema 0.7.0 (formato ELF64, segmentos, cobertura do .text)", Nativo.CasoDebugJsonlLinux),
    ("alvo: --modo dll com x86_64-linux = X0003 biblioteca .so: M5c", Nativo.CasoTargetLinuxDll),
    ("alvo: run com x86_64-linux = X0003 (o Windows nao executa ELF)", Nativo.CasoTargetLinuxRun),
    ("alvo: test com x86_64-linux exige -o (X0003) e so constroi (grava o ELF de teste)", Nativo.CasoTargetLinuxTest),
    ("alvo: test com x86_64-linux aceita --json e --filtro com -o e so constroi (stdout vazio)", Nativo.CasoTargetLinuxTestOpcoes),
    ("alvo: test std:<modulo> com x86_64-linux grava o exe de teste do modulo", Nativo.CasoTargetLinuxTestStd),
    ("alvo: foreign com link liga no x86_64-linux pela semente musl (ELF64 em dev e release)", Nativo.CasoTargetLinuxForeign),
    ("alvo: std:process liga no x86_64-linux (ELF64 em dev e release)", Nativo.CasoTargetLinuxProcessoLiga),
    ("alvo: std:file, std:os (A52) e stdin ligam no x86_64-linux (dev e release)", Nativo.CasoTargetLinuxFileOsLigam),
    ("alvo: vendor:net (libs so do Windows + curl) liga no x86_64-linux (ELF64 em dev e release)", Nativo.CasoTargetLinuxRedeLibsDoWindows),
    ("alvo: build --projeto com x86_64-linux = X0003", Nativo.CasoTargetLinuxProjeto),
    ("alvo: --suporte --json lista os dois alvos (pe/elf, exe/test/dll, escritor)", Nativo.CasoSuporteDoisAlvos),
    ("alvo: --target xx fora da lista (X0002, exit 2)", Nativo.CasoTargetXx),
    ("explicar X0001", Nativo.CasoExplicarX0001),
    ("explicar X0002", Nativo.CasoExplicarX0002),
    ("explicar X0003", Nativo.CasoExplicarX0003),
    ("explicar X0302", Nativo.CasoExplicarX0302),
    ("explicar X0901", Nativo.CasoExplicarX0901),
    ("explicar X0902", Nativo.CasoExplicarX0902),
    ("explicar X0903", Nativo.CasoExplicarX0903),
    ("explicar X0904", Nativo.CasoExplicarX0904),
    ("explicar X0905", Nativo.CasoExplicarX0905),
    ("explicar X0906", Nativo.CasoExplicarX0906),
    ("explicar X0801", Nativo.CasoExplicarX0801),
    ("explicar X0802", Nativo.CasoExplicarX0802),
    ("explicar X0803", Nativo.CasoExplicarX0803),
    ("--traco: stderr do x64 igual ao valor fixo do C (traco_escalar.fj)", Nativo.CasoTracoEscalarIgual),
    ("x64 igual ao C: real (let f32)", Nativo.CasoRealX64IgualAoC),
    ("x64 igual ao C: print de str", Nativo.CasoPrintStrX64IgualAoC),
    ("x64 igual ao C: struct (literal e p.x)", Nativo.CasoStructX64IgualAoC),
    ("x64 igual ao C: resultado (T ! E com or_else)", Nativo.CasoResultadoX64IgualAoC),
    ("x64 igual ao C: divisao (84 / 2)", Nativo.CasoDivisaoX64IgualAoC),
    ("x64 igual ao C: forja test passa (dois_testes.fj)", Nativo.CasoTesteX64Passa),
    ("x64 igual ao C: forja test falha (teste_falha.fj, expect em stderr)", Nativo.CasoTesteX64Falha),
    ("x64 igual ao C: foreign (labs do CRT, sem link)", Nativo.CasoForeignX64IgualAoC),
    ("x64 igual ao C: proc como valor e chamada indireta (proc_ind.fj)", Nativo.CasoProcIndX64IgualAoC),
    ("X0302: -o aponta para uma pasta existente (exit 2, stdout vazio)", Nativo.CasoX0302PastaExistente),
    ("injetado X0901 (isel): interno:true, etapa e pacote de relato", Nativo.CasoInjetadoX0901),
    ("injetado X0902 (codificar): interno:true, etapa e pacote de relato", Nativo.CasoInjetadoX0902),
    ("injetado X0903 (ligar): interno:true, etapa e pacote de relato", Nativo.CasoInjetadoX0903),
    ("injetado X0904 (bir): interno:true, etapa e pacote de relato", Nativo.CasoInjetadoX0904),
    ("injetado X0905 (escrever): interno:true, etapa e pacote de relato", Nativo.CasoInjetadoX0905),
    ("injetado X0906 (layout): interno:true, etapa e pacote de relato", Nativo.CasoInjetadoX0906),
    ("X0001 com ir, etapa e interno:false, sem sugestao (callback.fj)", Nativo.CasoX0001ComIr),
    ("M3 P19: X0101 simbolo de foreign sem biblioteca (JSON com proc, simbolo e procurei_em)", Nativo.CasoX0101SimboloSemBiblioteca),
    ("M3 P19: X0102 link sem arquivo (biblioteca e candidatos tentados)", Nativo.CasoX0102LinkSemArquivo),
    ("M3b P23e: link que acha biblioteca estatica (.a) liga e roda (antes X0103)", Nativo.CasoX0103BibliotecaEstatica),
    ("M3 P19b: X0105 link que acha import library de 32 bits (.lib do dlltool i386)", Nativo.CasoX0105ImportLibrary32Bits),
    ("M3b P23g: X0103 link que acha um arquivo que nao e biblioteca (texto .lib)", Estatica.CasoX0103ArquivoQueNaoEBiblioteca),
    ("M3b P23g: X0105 estatica (.a) sem indice de simbolos (ar rcS)", Estatica.CasoX0105ArSemIndice),
    ("M3b P23g: X0106 simbolo exigido definido em duas estaticas", Estatica.CasoX0106SimboloEmDuasEstaticas),
    ("M3b P23g: X0107 membro de estatica com secao .tls", Estatica.CasoX0107SecaoTls),
    ("M3b P23g: X0108 .drectve do membro pede biblioteca ausente", Estatica.CasoX0108DependenciaDoDrectveAusente),
    ("M3b P23y-2: X0109 mais de 256 nomes importados de DLL (era abort)", Estatica.CasoX0109ImportsAcimaDoTeto),
    ("M3 P22-O1a: --entradas lista a raiz, 2 modulos e a DLL com tamanho, mtime e Hash64 certos", Nativo.CasoEntradasListaRaizModulosEDll),
    ("M3 P22-O1a: --entradas com --backend c e X0003 (exit 2, nada gravado)", Nativo.CasoEntradasComBackendC),
    ("M3 P22-O1a: --lib-pasta acha a DLL fora da pasta do .fj (sem ela, X0102)", Nativo.CasoLibPastaAchaDllForaDaPastaDoFonte),
    ("M3 P22-O2: build --projeto --backend c e X0003 (exit 2, nada gravado)", Nativo.CasoProjetoComBackendC),
    ("M3 P22-O2: build --projeto com -o e uso errado (exit 2, nada gravado)", Nativo.CasoProjetoComSaida),
    ("debug.jsonl do 01_ola: esquema 0.2.0 e .text 100% coberto", Nativo.CasoDebugJsonl),
    ("debug.jsonl: duas builds iguais (exe e jsonl, sem tempos)", Nativo.CasoDebugJsonlDeterministico),
    ("--suporte --json: tabela e ocorrencias de um .fj (exit 0)", Nativo.CasoSuporteJson),
    ("x64: o 01_ola importa so o que usa (sem heap nem relogio) e cabe em 24 KB", Nativo.CasoOlaImportaSoOQueUsa),
    ("run -- args: espaco, aspa, barra, vazio e --release chegam intactos", RunArgs.CasoRunArgumentos),
    ("-- args fora do run (test, build, check): uso errado, exit 2", RunArgs.CasoArgumentosForaDoRun),
    ("v1.11 L16a: test --filtro dobro roda 1 dos 3 testes (2 filtrados)", Frontend.CasoTesteFiltroUmDeTres),
    ("v1.11 L16a: test --filtro sem nenhum titulo (e sensivel a caixa): 0 testes, exit 0", Frontend.CasoTesteFiltroZero),
    ("v1.11 L16a: test --json com expect falho: 1a falha do teste e resumo", Frontend.CasoTesteJsonExpect),
    ("v1.11 L16a: test --json com R0002 dentro de teste e --filtro", Frontend.CasoTesteJsonR0002),
    ("v1.11 L16a: x64 igual ao C: forja test --json (expect e R0002)", Nativo.CasoTesteJsonX64IgualAoC),
    ("v1.11 L16b: explicar R0011 (excecao do processador dentro de test)", Frontend.CasoExplicarR0011),
    # M3 P22a: os 10 casos ctypes e os 3 de PHP FFI da DLL, de novo com --backend x64 (a DLL nativa, sem gcc), mais o .h byte a byte e o FreeLibrary
    ("x64 dll: simbolo limpo, sem fj_ (ctypes)", ComBackendX64(Ffi.CasoDllCarrega)),
    ("x64 dll: tipos da ABI (f32, bool, enum, ^Struct) (ctypes)", ComBackendX64(Ffi.CasoDllTipos)),
    ("x64 dll: PHP FFI::cdef", ComBackendX64(Ffi.CasoPhpFfi)),
    ("x64 dll: numericos (i16 u8 u32 uint f64) (ctypes)", ComBackendX64(Ffi.CasoDllNumericos)),
    ("x64 dll: PHP FFI numericos", ComBackendX64(Ffi.CasoPhpFfiNumericos)),
    ("x64 dll de dev: estouro sai com 101 (R0002) (ctypes)", ComBackendX64(Ffi.CasoDllEstouroSai101)),
    ("x64 dll: ponteiros, rawptr, cstr e struct por valor (ctypes)", ComBackendX64(Ffi.CasoDllPonteiros)),
    ("x64 dll: PHP FFI ponteiros e struct por valor", ComBackendX64(Ffi.CasoPhpFfiPonteiros)),
    ("x64 dll de dev: NULL em ^T sai com 101 (R0005) (ctypes)", ComBackendX64(Ffi.CasoDllNuloSai101)),
    ("x64 dll: DLL com foreign (so o export sai) (ctypes)", ComBackendX64(Ffi.CasoDllComForeign)),
    ("x64 dll de dev: array interno, pega(5) sai 101 (R0006) (ctypes)", ComBackendX64(Ffi.CasoArrayDll)),
    ("x64 dll de dev: lista e new, soltar duas vezes sai 101 (R0008) (ctypes)", ComBackendX64(Ffi.CasoListaDll)),
    ("x64 dll de dev: cstr e ?cstr, forja_liberar e liberar duplo 101 (R0008) (ctypes)", ComBackendX64(Ffi.CasoExportCstrDll)),
    ("x64 dll: o .h sai byte a byte igual ao do backend C (todas as fixtures)", Ffi.CasoCabecalhoX64IgualAoC),
    ("x64 dll de dev: FreeLibrary com bloco vivo relata R0009 sem derrubar o hospedeiro (igual ao C)", Ffi.CasoDllFreeLibraryRelataVazamento),
    ("M8 H1 dll: forja_dll_info, mesmo hash (2 builds, dev x release, so corpo, constante, parametro renomeado)", DllInfo.CasoDllInfoHashIgual),
    ("M8 H1 dll: forja_dll_info, hash diferente (campo novo, ordem, tipo, retorno, membro de enum novo)", DllInfo.CasoDllInfoHashDiferente),
    ("M8 H1 dll: forja_dll_info por ctypes igual ao --debug-code interface (magic, formato, hash FNV-1a 64, texto)", DllInfo.CasoDllInfoCtypesIgualAoDebugCode),
    ("L1b pacote: ast --projeto traz arquivo e linha local por declaracao do pacote (sem arquivo na raiz, em modulo de 1 arquivo e com --sem-pos)", Pacote.CasoPacoteAst),
    ("L1b pacote: consultar simbolos/api/escopo mostram o arquivo certo, o comentario nao gruda entre arquivos e o escopo so ve os imports do arquivo", Pacote.CasoPacoteConsultar),
    ("L1b pacote: mapa --json e --resumo somam os arquivos do pacote e dizem de que arquivo e o teste", Pacote.CasoPacoteMapa),
    ("L1b pacote: fmt de pasta de pacote formata cada arquivo (dry-run nao grava, --aplicar restaura, 2a passada 0)", Pacote.CasoPacoteFmt),
    ("L1b pacote: editar dentro do pacote cai no arquivo certo (inserir, remover com import, trocar_corpo, renomear) e no_fim sem arquivo e recusado", Pacote.CasoPacoteEditar),
    ("L1b pacote: mover_decl de/para pacote recusado sem gravar; entre modulos de arquivo em projeto com pacotes segue valendo", Pacote.CasoPacoteMover),
    ("L1b pacote: migrar de raiz que importa pacote troca so a raiz, o texto migrado compila e a 2a passada acha 0", Pacote.CasoPacoteMigrar),
    ("L1b pacote: cache do build ve .fj acrescentado/removido na pasta do pacote (linha pasta do .entradas)", Pacote.CasoPacoteCachePasta),
]


def LerParte(Texto):
    """Converte 'K/N' em (K, N) com 1 <= K <= N; None se nao for isso."""
    Partes = Texto.split("/")
    if len(Partes) != 2 or not all(P.isdigit() for P in Partes):
        return None
    K, N = int(Partes[0]), int(Partes[1])
    return (K, N) if 1 <= K <= N else None


def Verificar(Forja, Parte=(1, 1)):
    """Roda os casos da parte K de N (o caso i cai na parte i % N + 1); (1, 1) roda todos."""
    K, N = Parte
    Escolhidos = [Caso for I, Caso in enumerate(CASOS) if I % N == K - 1]
    PastaSaidas = Path(tempfile.mkdtemp(prefix="verificar_cli_"))
    Passou = 0
    try:
        for Nome, Caso in Escolhidos:
            Problemas = Caso(Forja, PastaSaidas)
            if Problemas:
                print(f"FALHOU {Nome}")
                for Problema in Problemas:
                    print(f"    {Problema}")
            else:
                Passou += 1
    finally:
        shutil.rmtree(PastaSaidas, ignore_errors=True)
    Sufixo = f" (parte {K}/{N} de {len(CASOS)})" if N > 1 else ""
    print(f"{Passou}/{len(Escolhidos)} casos de cli{Sufixo}")
    return Passou == len(Escolhidos)


def Autoteste():
    Problemas = []
    if UltimaLinha("a\n\nb\n") != "b":
        Problemas.append("UltimaLinha ignora linha vazia")
    if LinhaJson('{"codigo": "E0301"}') != {"codigo": "E0301"}:
        Problemas.append("LinhaJson nao le uma linha JSON")
    if LinhaJson("x\ny") is not None:
        Problemas.append("LinhaJson aceita duas linhas")
    for Problema in Problemas:
        print(f"autoteste FALHOU: {Problema}")
    return not Problemas


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao, help="caminho do forja.exe")
    Analisador.add_argument("--selftest", action="store_true", help="testa as funcoes de leitura da saida e sai")
    Analisador.add_argument("--parte", default="1/1", help="K/N: roda so a parte K de N dos casos (padrao 1/1 = todos)")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return 0 if Autoteste() else 1
    Parte = LerParte(Argumentos.parte)
    if Parte is None:
        print(f"--parte {Argumentos.parte!r} invalida: use K/N com 1 <= K <= N", file=sys.stderr)
        return 2
    Forja = Argumentos.forja.resolve()
    if not Forja.exists():
        print(f"forja.exe nao encontrado em {Forja}; rode construir.bat", file=sys.stderr)
        return 2
    return 0 if Verificar(Forja, Parte) else 1


if __name__ == "__main__":
    sys.exit(Principal())
