/* perfil.h - passe `perfil` (M8 P2, D-M8-5): instrumenta cada proc de usuario com forja_perfil_entrar(id) no inicio e forja_perfil_sair(id) antes de cada RETORNA.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M8 P2: primeira versao (perfil.c). Roda so com --perfil (BirPrograma.perfil), em dev e em release, por ULTIMO entre os passes que mudam a BIR (antes do alocador, que ja salva o que vive
 *                    na chamada); nao esta na tabela PASSES (nao e otimizacao: --sem-otim nao o aceita nem o desliga).
 * Verificacao: construir_rapido.py && python tests\nativo\perfil\verificar_perfil.py   (P3; ate la: fib.fj --perfil e o .perfil.txt diz fib com 21891 chamadas para fib(20))
 *
 * Proc de usuario = BirFuncao nao sintetica cujo simbolo comeca por "fj_" (o forja_programa, os trampolins fjt_* e os involucros de DLL ficam de fora). O id e o indice da proc entre elas, na ordem de
 * BirPrograma.funcoes; o backend (nativo_perfil.c) emite a tabela FPF1 com a mesma ordem. Toda CHAMA a forja_proc_sair (os.exit) ganha antes uma CHAMA a forja_perfil_finalizar(): o os.exit nao roda os
 * handlers de saida do runtime e o perfil precisa gravar os arquivos.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_PERFIL_H
#define FORJA_NATIVO_OPTIMIZER_PERFIL_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

#define PERFIL_NOME_MAX 512u /* bytes de "modulo.proc" com o NUL (o nome mais longo que a tabela FPF1 e o item perfil do --debug-code mostram) */

/* A funcao e uma proc de usuario (tem id na tabela FPF1)? */
bool OtimPerfilProc(const BirPrograma *programa, const BirFuncao *funcao);

/* "modulo.proc" da funcao (proc da raiz: so "proc"), em saida[cap]. */
void OtimPerfilNome(const BirPrograma *programa, const BirFuncao *funcao, char *saida, size_t cap);

/* Passe de programa (passes.c): so com programa->perfil. O relato ganha `perfil <funcao>: id N` por proc instrumentada. */
void OtimPerfilAplicar(BirPrograma *programa, Buf *relato);

#endif
