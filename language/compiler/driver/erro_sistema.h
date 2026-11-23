/* erro_sistema.h - texto da causa de um erro do sistema para as mensagens do driver (orquestrador e processo_win).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 F6: ErroWinTexto (GetLastError + FormatMessageA) e ErroCrtTexto (errno + strerror).
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#ifndef FORJA_DRIVER_ERRO_SISTEMA_H
#define FORJA_DRIVER_ERRO_SISTEMA_H

#include <stddef.h>

#define ERRO_TEXTO_MAX 256

/* "erro 5: Access is denied" (texto em ingles e ASCII, igual ao strerror). Nunca falha: sem texto do sistema sai so o numero. */
void ErroWinTexto(unsigned long codigo, char *destino, size_t tam);

/* "errno 2: No such file or directory"; errno 0 = "sem codigo de erro". */
void ErroCrtTexto(int codigo, char *destino, size_t tam);

#endif
