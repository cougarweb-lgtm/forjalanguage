/* nativo_perfil.h - nativo_perfil.c: o que o backend faz por conta do --perfil (M8 P2, D-M8-5): ler o runtime do perfil e emitir a tabela FPF1 de nomes.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M8 P2: primeira versao.
 * Verificacao: construir_rapido.py && forja build --perfil fib.fj -o fib.exe && fib.exe (o fib.exe.perfil.txt diz fib com 21891 chamadas para fib(20))
 */
#ifndef FORJA_NATIVO_NATIVO_PERFIL_H
#define FORJA_NATIVO_NATIVO_PERFIL_H

#include "common/diag.h"
#include "nativo/bir/bir.h"
#include "nativo/nativo.h"
#include "nativo/obj/objeto.h"

#define PERFIL_TABELA_SIMBOLO "forja_perfil_tabela"

/* Com pedido->perfil le o runtime do perfil do alvo (rt_win_perfil.o ou rt_linux_perfil.o) em *saida (de ObjIniciar); sem ele nao faz nada e devolve true. false = o erro do CoffLer em *erro. */
bool NativoPerfilLer(const NativoPedido *pedido, bool alvo_elf, Objeto *saida, Diag *erro);

/* Com pedido->perfil acrescenta ao .rdata do programa a tabela FPF1 (u32 magic 0x31465046, u32 contagem, u32 offsets[contagem] desde o inicio da tabela, os textos "modulo.proc" com NUL; o id
 * e o indice da proc de usuario na ordem de BirPrograma.funcoes, a mesma do passe `perfil`) e define PERFIL_TABELA_SIMBOLO. Sem pedido->perfil nao faz nada. Devolve sempre true (para encadear com o gerar). */
bool NativoPerfilTabela(Objeto *programa, const BirPrograma *bir, const NativoPedido *pedido);

#endif
