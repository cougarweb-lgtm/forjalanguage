/*
 * rt_ryu.c - a unidade propria do runtime nativo (M3 F5) que o construir.bat compila UMA vez em rt_win_ryu.o. Desde o Ryu P4 ela nao leva mais o Ryu d2fixed/d2exp
 * (a tabela POW10 saiu; {x:.N} e o rt_fmt_fixo.c do musl): sobram nela o std:os, o std:proc e o std:arquivo.
 *
 * Versao: 0.5.0
 * Changelog:
 *   0.5.0 - M8 H2: a unidade tambem leva o std:reload (rt_reload_win.c, depois do rt_os_win.c: reusa o rt_os_para_larga dele); 3 imports novos de kernel32 (LoadLibraryExW, GetProcAddress, FreeLibrary).
 *   0.4.0 - IA1-LX: a unidade tambem leva o std:os (rt_os_win.c: forja_proc_dormir_ms e forja_proc_tempo_ns), que saiu do rt_curto.c: o rt_win_curto.o ficou neutro de alvo e liga tambem no ELF, onde o par vem do rt_linux.c
 *           (simbolo repetido = X0903). Importa Sleep (o ligador so o poe no .idata do exe que o alcanca); o rt_win.o nao ganha import (verificar_rt_nativo).
 *   0.3.0 - M5 P26c: a unidade tambem leva o std:arquivo (rt_arquivo_win.c, rt_arquivo.c), junto do std:proc; o nativo.c ja junta este objeto quando o programa chama simbolo dele
 *           (forja_arquivo_*). Pelo mesmo motivo do 0.2.0: o rt_win.o dos exes sem o modulo nao muda (D-M5-11).
 *   0.2.0 - M5 P26b: a unidade tambem leva o std:proc (rt_linha.c, rt_proc_win.c, rt_proc.c). O nativo.c junta este objeto quando o runtime OU o programa tem simbolo indefinido
 *           que ele define (forja_proc_*); o ligador descarta o que nao e alcancado. Fora do rt_win.o para os exes release sem std:proc ficarem byte a byte iguais (D-M5-11).
 *   0.1.0- M3 F5: o construir.bat compila esta unidade UMA vez, em rt_win_ryu.o (flags de release do runtime, sem -DFORJA_DEV), e as
 *           quatro variantes de rt_win.c saem com -DFORJA_RT_SEM_RYU (referencias indefinidas aos simbolos do Ryu). O forja.exe embute
 *           o rt_win_ryu.o uma vez e o ligador o junta ao runtime do programa. Sem estado mutavel (so tabelas const).
 *
 * Referencias indefinidas que o ligador resolve no runtime: forja_panic (malloc do envelope) e memcpy/memset/strlen que o Ryu chame.
 * Compilar exige -I <language>\third (o rt_terceiros.c ainda inclui ryu/ryu.h para o rt_real_curto.c).
 */
#ifndef RT_ALVO_WIN
#error "rt_ryu.c so compila para Windows: use -DRT_ALVO_WIN"
#endif

/* std:proc (M5 P26b) mora nesta unidade, e nao no rt_win.o: ver o cabecalho de rt_proc_win.c. Vem ANTES do rt_terceiros.c, que redefine malloc. */
#include "rt_linha.c"
#include "rt_proc_win.c"
#include "rt_proc.c"
/* std:arquivo (M5 P26c) mora aqui pelo mesmo motivo; rt_arquivo_win.c reusa a MultiByteToWideChar do rt_proc_win.c. */
#include "rt_arquivo_win.c"
#include "rt_arquivo.c"
/* std:os (IA1-L6) tambem mora aqui: o Windows e o unico alvo deste objeto, e o ELF tem o par em rt_os_linux.c. */
#include "rt_os_win.c"
/* std:reload (M8 H2) reusa o rt_os_para_larga do rt_os_win.c. */
#include "rt_reload_win.c"

#include "rt_terceiros.c"
