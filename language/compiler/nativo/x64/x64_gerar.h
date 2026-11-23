/* x64_gerar.h - API do gerador de codigo x86-64 (Windows) do backend nativo: BIR -> Objeto.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M4 P24a: o 4o parametro e a AlvoColheita (lista da listagem e AlvoTempos); NULL continua valendo (nem listagem nem medicao).
 *            0.2.0 - backend x64 P8: X64Gerar recebe a AlvoListagem opcional (--debug-code) e gera .pdata/.xdata de cada funcao.
 *            0.1.0 - backend x64 P7a: X64Gerar.
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe> e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#ifndef FORJA_NATIVO_X64_X64_GERAR_H
#define FORJA_NATIVO_X64_X64_GERAR_H

#include "common/diag.h"
#include "nativo/x64/alvo.h"
#include "nativo/bir/bir.h"
#include "nativo/obj/objeto.h"

/* Gera .text (uma funcao por BirFuncao, simbolo global com o nome do BirSimbolo), .rdata (textos com NUL), .pdata e .xdata (um
 * RUNTIME_FUNCTION e um UNWIND_INFO por funcao) em o, que tem de vir de ObjIniciar. lista NULL = sem listagem. false = X0901 (operacao
 * sem regra), X0902 (codificador recusou) ou X0001/X0904 herdados; em falha o Objeto fica parcial. colheita NULL = nem listagem nem medicao;
 * colheita->tempos recebe as fatias isel/regs/codificar (alvo.h), 3 leituras de relogio por funcao. */
bool X64Gerar(const BirPrograma *programa, Objeto *o, Diag *erro, const AlvoColheita *colheita);

#endif
