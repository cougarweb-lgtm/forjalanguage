/* processo.h - execucao de programas pela Forja (forja ligar, forja run e forja test) e os caminhos temporarios do run/test.
 * Versao: 0.10.0
 * Changelog: 0.10.0 - 0.21.0 F4 passo 2b: cc.h virou processo.h. Saem CcPedido, CcStatus, CcCompilar e CcCaminhoDoC (o driver nao chama mais o gcc);
 *                    Cc* viram Proc* (ProcExecutarComArgumentos, ProcArquivoTemporario, ProcExeDoRun, ProcApagarExeDoRun).
 *            0.9.0 - O0 P13: CcPedido.sem_checagem (-DFORJA_SEM_CHECAGEM).
 *            0.8.0 - M6 P68: CcPedido.simbolos (-g no gcc, na compilacao do .c; a ligacao segue igual; D-M6-5, sem PDB).
 *            0.7.0 - M5 P53: CcPedido.winsock (-lws2_32 na ligacao do C, so com std:net).
 *            0.6.0 - IA1-b02: CcCaminhoDoC, CcExeDoRun e CcApagarExeDoRun (nomes temporarios sem pid, para a saida igual byte a byte).
 *            0.5.1 - forja run -- args: CcExecutarPrograma removida (orfa); CcExecutarComArgumentos roda o .exe do run.
 *            0.5.0 -Linguagem v1.8b P6/P7: CcPedido ganha pastas_extra/npastas_extra (-L de vendor:/engine:); CcExecutarComArgumentos (forja ligar).
 *            0.4.0 - Linguagem v1.4 P5: CcPedido (substitui dev/biblioteca) com bibliotecas de 'foreign'; CC_LIGACAO (B0004).
 *            0.3.0 - Fase 5i: CcCompilar ganha 'biblioteca' (-shared) para o build --modo dll.
 *            0.2.0 - Fase 5h: CcExecutarPrograma (roda o .exe temporario); CcArquivoTemporario recebe a extensao.
 *            0.1.0 - Fase 5d: CcCompilar (gcc -std=c11 -O2 -static -Wall -Wextra -Werror) e CcArquivoTemporario (%TEMP%\forja_<pid>.c).
 * Verificacao: python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_PROCESSO_H
#define FORJA_PROCESSO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Roda 'programa' (busca no PATH) com os 'nargs' argumentos de 'args' (cada um citado pela regra do argv do Windows, sem perder espaco,
 * aspa ou barra) e devolve o exit code em 'codigo'. nargs 0 = sem argumentos. false = nao iniciou. */
bool ProcExecutarComArgumentos(const char *programa, const char *const *args, int nargs, int *codigo);

/* Caminho de um arquivo temporario unico do processo, em %TEMP%, com a extensao dada (ex.: ".c"). false = nao coube no buffer. */
bool ProcArquivoTemporario(char *destino, size_t tam, const char *extensao);

/* Exe temporario do forja run/test: %TEMP%\forja_run\<hash do caminho absoluto do fonte e da variante>\<nome do fonte sem .fj>.exe (sem pid: o argv[0] do programa
 * e o mesmo em toda execucao). 'variante' diferencia comando, backend e release do mesmo fonte (execucoes simultaneas nao se pisam). Cria as pastas. false = nao coube ou nao criou. */
bool ProcExeDoRun(char *destino, size_t tam, const char *fonte, const char *variante);

/* Apaga o exe de ProcExeDoRun e a pasta do hash, se ficou vazia. */
void ProcApagarExeDoRun(const char *arq_exe);

#endif
