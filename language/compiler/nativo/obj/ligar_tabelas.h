/* ligar_tabelas.h - as tabelas FIXAS de __imp_ do ligador do backend nativo (KERNEL32 do runtime e ADVAPI32), extraidas do ligar.c no M5 P43.
 * Versao: 0.14.0
 * Changelog: 0.14.0 - D-G3-L2a (bloco D): LIGAR_QTD_IMPORTS de 84 para 85 (SetLastError, no fim da KERNEL32 fixa: forja_fs_abrir e forja_process_spawn regravam o erro do sistema na falha).
 *            0.13.0 - D-G3-L2a (bloco C): LIGAR_QTD_IMPORTS de 83 para 84 (DuplicateHandle, no fim da KERNEL32 fixa: Stdio.inherit do process.spawn).
 *            0.12.0 - D-G3-L2a (bloco B): LIGAR_QTD_IMPORTS de 80 para 83 (WaitForMultipleObjects, QueryInformationJobObject e GetProcessTimes, no fim da KERNEL32 fixa: process.wait_any, peak_memory e cpu_times).
 *            0.11.0 - D-G3-L2a (bloco A): LIGAR_QTD_IMPORTS de 77 para 80 (GlobalMemoryStatusEx, OpenProcess e FormatMessageW, no fim da KERNEL32 fixa: os.mem_available, os.pid_alive e os.error_text).
 *            0.10.0 - M8 H2: LIGAR_QTD_IMPORTS de 74 para 77 (LoadLibraryExW, GetProcAddress e FreeLibrary, no fim da KERNEL32 fixa: o std:reload).
 *            0.9.0 - M8 K1 (D-M8-8): LIGAR_QTD_IMPORTS de 72 para 74 (GetCurrentProcessId e GetActiveProcessorCount, no fim da KERNEL32 fixa).
 *            0.8.0 - R9: LIGAR_QTD_IMPORTS_WS2_32 de 11 para 18 (connect, listen, accept, send, recv, ioctlsocket e getsockopt, no fim: o TCP de rt_net_win.c).
 *            0.7.0 - M5 P54: LIGAR_QTD_IMPORTS_WS2_32 de 9 para 11 (WSAIoctl e setsockopt, no fim da WS2_32 fixa: SIO_UDP_CONNRESET e SO_RCVBUF/SO_SNDBUF de rt_net_win.c).
 *            0.6.0 - M5 P53: LigarMesmaDll e LigarFundirWs2 (fusao da WS2_32 fixa com a 'link' da mesma DLL) moram aqui, fora de ligar.c.
 *            0.5.0 - M5 P53: a WS2_32.dll (9 imports do std:net, rt_win_net.o) e a 3a tabela fixa: so entra quando o programa tem o objeto net (LigarPedido.net); LIGAR_QTD_TABELAS_BASE e as 2 do runtime.
 *            0.4.0 - M5 P49: LIGAR_QTD_IMPORTS 71 (TryAcquireSRWLockExclusive, SleepConditionVariableSRW, WakeConditionVariable, WakeAllConditionVariable).
 *            0.3.0 - M5 P47: LIGAR_QTD_IMPORTS 67 (CreateThread). 0.2.0 - M5 P44 (T0-2): LIGAR_QTD_IMPORTS de 64 para 66 (AcquireSRWLockExclusive e ReleaseSRWLockExclusive). 0.1.0 - M5 P43 (T0-1): primeira versao. A tabela da KERNEL32 passa de 61 para 64 imports (TlsAlloc, TlsGetValue e TlsSetValue, no fim da tabela: o slot do RtFio de cada thread,
 *            rt_fio.h); a ADVAPI32 (1 import) e as constantes vieram do ligar.c 0.13.1 sem mudanca. So entram no .idata de quem os referencia.
 * Verificacao: construir.bat && teste_nativo.exe obj <pasta> e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#ifndef FORJA_LIGAR_TABELAS_H
#define FORJA_LIGAR_TABELAS_H

#include "nativo/obj/ligar.h"

#define LIGAR_QTD_IMPORTS 85
#define LIGAR_NOME_DLL "KERNEL32.dll"
#define LIGAR_QTD_IMPORTS_ADVAPI32 1
#define LIGAR_NOME_DLL_ADVAPI32 "ADVAPI32.dll"
#define LIGAR_QTD_TABELAS_BASE 2
#define LIGAR_QTD_IMPORTS_WS2_32 18
#define LIGAR_NOME_DLL_WS2_32 "WS2_32.dll"
#define LIGAR_QTD_TABELAS 3

/* As DLLs de que o exe pode importar, na ordem dos descritores do .idata: a KERNEL32 do runtime primeiro, a ADVAPI32 depois e a WS2_32 por ultimo (so com o std:net; so ganha descritor se alcancada). */
extern const ImportTabela LIGAR_TABELAS[LIGAR_QTD_TABELAS];

/* Nome de DLL sem distincao de caixa: ws2_32.dll (do libcurl) e WS2_32.dll (a fixa) sao a mesma DLL. */
bool LigarMesmaDll(const char *a, const char *b);

/* Funde a 'link' extra na tabela fixa: os nomes da fixa e os da extra, sem repetir, em fundidos (vive ate o fim do ligador); a fixa passa a apontar para ele. */
void LigarFundirWs2(ImportTabela *fixa, const ImportTabela *extra, const char **fundidos);

#endif
