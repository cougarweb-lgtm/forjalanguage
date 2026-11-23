/* ligar_tabelas.c - os dados das tabelas FIXAS de __imp_ do ligador nativo (extraidos do ligar.c, que estava em 799 linhas).
 * Versao: 0.15.0
 * Changelog: 0.15.0 - D-G3-L2a (bloco D): KERNEL32 de 84 para 85 imports (SetLastError, no fim: o GetLastError de forja_fs_abrir e forja_process_spawn sobrevive ao cleanup, para std:os.last_error; so entra no exe que o alcanca).
 *            0.14.0 - D-G3-L2a (bloco C): KERNEL32 de 83 para 84 imports (DuplicateHandle, no fim: Stdio.inherit do process.spawn; so entra no exe que o alcanca).
 *            0.13.0 - D-G3-L2a (bloco B): KERNEL32 de 80 para 83 imports (WaitForMultipleObjects, QueryInformationJobObject e GetProcessTimes, no fim: process.wait_any, peak_memory e cpu_times; so entram no exe que os alcanca).
 *            0.12.0 - D-G3-L2a (bloco A): KERNEL32 de 77 para 80 imports (GlobalMemoryStatusEx, OpenProcess e FormatMessageW, no fim; so entram no exe que os alcanca).
 *            0.11.0 - M8 H2: KERNEL32 de 74 para 77 imports (LoadLibraryExW, GetProcAddress e FreeLibrary, no fim: o std:reload; so entram no exe que o alcanca).
 *            0.10.0 - M8 K1 (D-M8-8): KERNEL32 de 72 para 74 imports (GetCurrentProcessId e GetActiveProcessorCount, no fim: os.pid e os.cpu_count do std:os; so entram no exe que os alcanca).
 *            0.9.0 - R9: WS2_32 fixa de 11 para 18 imports (connect, listen, accept, send, recv, ioctlsocket e getsockopt, no fim), usados pelo TCP de rt_net_win.c.
 *            0.8.0 - R2: KERNEL32 de 71 para 72 imports (SystemTimeToTzSpecificLocalTime, no fim; so o forja_os_fuso_s do std:time o referencia).
 *            0.7.0 - M5 P54: WS2_32 fixa de 9 para 11 imports (WSAIoctl e setsockopt, no fim), usados por rt_net_win.c (SIO_UDP_CONNRESET e buffers).
 *            0.6.0 - M5 P53: LigarMesmaDll e LigarFundirWs2 saem de ligar.c (o arquivo passou de 800 linhas): a WS2_32 fixa funde-se com a 'link' da mesma DLL.
 *            0.5.0 - M5 P53: WS2_32.dll (9 imports do std:net) como 3a tabela fixa, depois da ADVAPI32; so entra com LigarPedido.net.
 *            0.4.0 - M5 P49: KERNEL32 de 67 para 71 imports (TryAcquireSRWLockExclusive, SleepConditionVariableSRW, WakeConditionVariable e WakeAllConditionVariable, no fim; so o rt_win_sync.o os referencia).
 *            0.3.0 - M5 P47: KERNEL32 de 66 para 67 imports (CreateThread, no fim; so o rt_win_thread.o o referencia). 0.2.0 - M5 P44 (T0-2): KERNEL32 de 64 para 66 imports (AcquireSRWLockExclusive e ReleaseSRWLockExclusive, no fim: a trava do estado compartilhado do runtime). 0.1.0 - M5 P43 (T0-1): primeira versao; ver ligar_tabelas.h.
 * Verificacao: construir.bat && teste_nativo.exe obj <pasta> e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#include <string.h>
#include "nativo/obj/ligar_tabelas.h"

/* Os 66 imports de kernel32 que o runtime usa: a tabela FIXA de __imp_ (as DLLs dos 'link' entram depois dela, em LigarJuntarTabelas) (KERNEL32_M1 do contrato M1; o 16o,
 * GetEnvironmentVariableA, e o 17o, AddVectoredExceptionHandler, so do rt de teste; do 18o ao 21o, so do std:proc; do 22o ao 26o, so do std:arquivo; o 27o, Sleep, so do sleep_ms
 * do std:os; o 28o, GetSystemTimePreciseAsFileTime, so do wall_time_ns do std:os; do 29o ao 40o, so do std:file, S3 P5b-1; do 41o ao 55o, so do std:process, POS-Z A51; do 56o ao 61o,
 * std:os; os 3 seguintes, TlsAlloc, TlsGetValue e TlsSetValue, do slot do RtFio por thread; os 2 ultimos, AcquireSRWLockExclusive e ReleaseSRWLockExclusive, da trava SRW do estado compartilhado, P44). */
static const char *const LIGAR_KERNEL32_M1[LIGAR_QTD_IMPORTS] = {
    "GetStdHandle", "WriteFile", "GetFileType", "ExitProcess", "GetProcessHeap", "HeapAlloc",
    "HeapReAlloc", "HeapFree", "VirtualAlloc", "VirtualFree", "QueryPerformanceCounter", "QueryPerformanceFrequency",
    "GetConsoleMode", "GetConsoleOutputCP", "SetConsoleOutputCP", "GetEnvironmentVariableA",
    "AddVectoredExceptionHandler",
    "GetCommandLineW", "WideCharToMultiByte", "MultiByteToWideChar", "GetEnvironmentVariableW",
    "CreateFileW", "ReadFile", "GetFileSizeEx", "CloseHandle", "GetLastError",
    "Sleep",
    "GetSystemTimePreciseAsFileTime",
    "SetFilePointerEx", "FlushFileBuffers", "GetFileAttributesExW", "DeleteFileW", "MoveFileExW", "CopyFileW",
    "CreateDirectoryW", "RemoveDirectoryW", "FindFirstFileW", "FindNextFileW", "FindClose", "ReadConsoleW",
    "CreatePipe", "SetHandleInformation", "CreateProcessW", "InitializeProcThreadAttributeList", "UpdateProcThreadAttribute",
    "DeleteProcThreadAttributeList", "CreateJobObjectW", "SetInformationJobObject", "AssignProcessToJobObject", "ResumeThread",
    "TerminateJobObject", "TerminateProcess", "WaitForSingleObject", "GetExitCodeProcess", "PeekNamedPipe",
    "GetCurrentDirectoryW", "GetFullPathNameW", "GetModuleFileNameW", "GetTempPathW", "SetEnvironmentVariableW", "K32GetProcessMemoryInfo",
    "TlsAlloc", "TlsGetValue", "TlsSetValue",
    "AcquireSRWLockExclusive", "ReleaseSRWLockExclusive",
    "CreateThread",
    "TryAcquireSRWLockExclusive", "SleepConditionVariableSRW", "WakeConditionVariable", "WakeAllConditionVariable",
    "SystemTimeToTzSpecificLocalTime",
    "GetCurrentProcessId", "GetActiveProcessorCount",
    "LoadLibraryExW", "GetProcAddress", "FreeLibrary",
    "GlobalMemoryStatusEx", "OpenProcess", "FormatMessageW",
    "WaitForMultipleObjects", "QueryInformationJobObject", "GetProcessTimes",
    "DuplicateHandle",
    "SetLastError",
};

/* O unico import da ADVAPI32.dll: o gerador aleatorio do sistema (RtlGenRandom), so do random_bytes do std:os (A52). */
static const char *const LIGAR_ADVAPI32_M1[LIGAR_QTD_IMPORTS_ADVAPI32] = {
    "SystemFunction036",
};

/* Os 18 imports da WS2_32.dll que o std:net usa (rt_win_net.o): o 3o descritor fixo, so quando o programa tem o objeto net. */
static const char *const LIGAR_WS2_32_M1[LIGAR_QTD_IMPORTS_WS2_32] = {
    "WSAStartup", "WSAGetLastError", "socket", "bind", "sendto", "recvfrom", "select", "getsockname", "closesocket", "WSAIoctl", "setsockopt",
    "connect", "listen", "accept", "send", "recv", "ioctlsocket", "getsockopt",
};

const ImportTabela LIGAR_TABELAS[LIGAR_QTD_TABELAS] = {
    {LIGAR_NOME_DLL, LIGAR_KERNEL32_M1, LIGAR_QTD_IMPORTS},
    {LIGAR_NOME_DLL_ADVAPI32, LIGAR_ADVAPI32_M1, LIGAR_QTD_IMPORTS_ADVAPI32},
    {LIGAR_NOME_DLL_WS2_32, LIGAR_WS2_32_M1, LIGAR_QTD_IMPORTS_WS2_32},
};

static char LigarMinuscula(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c;
}

bool LigarMesmaDll(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (LigarMinuscula(*a) != LigarMinuscula(*b))
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

void LigarFundirWs2(ImportTabela *fixa, const ImportTabela *extra, const char **fundidos)
{
    uint32_t qtd = 0;

    for (uint32_t s = 0; s < fixa->qtd; s++)
        fundidos[qtd++] = fixa->simbolos[s];
    for (uint32_t s = 0; s < extra->qtd; s++) {
        bool ja = false;

        for (uint32_t u = 0; u < qtd && !ja; u++)
            ja = strcmp(fundidos[u], extra->simbolos[s]) == 0;
        if (!ja)
            fundidos[qtd++] = extra->simbolos[s];
    }
    fixa->simbolos = fundidos;
    fixa->qtd = qtd;
}
