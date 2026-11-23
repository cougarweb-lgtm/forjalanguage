/* erro_sistema.c - texto da causa de um erro do sistema (ver erro_sistema.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 F6: ErroWinTexto e ErroCrtTexto.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "driver/erro_sistema.h"

/* O FormatMessage termina o texto com ".\r\n": tira o fim de linha e os espacos. */
static void AparaFim(char *texto)
{
    size_t n = strlen(texto);

    while (n > 0 && (texto[n - 1] == '\n' || texto[n - 1] == '\r' || texto[n - 1] == ' ')) texto[--n] = '\0';
}

void ErroWinTexto(unsigned long codigo, char *destino, size_t tam)
{
    char sistema[ERRO_TEXTO_MAX] = "";
    DWORD lingua = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, (DWORD)codigo, lingua, sistema, sizeof sistema, NULL);

    AparaFim(sistema);
    if (n == 0 || sistema[0] == '\0')
        snprintf(destino, tam, "erro %lu", codigo);
    else
        snprintf(destino, tam, "erro %lu: %s", codigo, sistema);
}

void ErroCrtTexto(int codigo, char *destino, size_t tam)
{
    if (codigo == 0)
        snprintf(destino, tam, "sem codigo de erro");
    else
        snprintf(destino, tam, "errno %d: %s", codigo, strerror(codigo));
}
