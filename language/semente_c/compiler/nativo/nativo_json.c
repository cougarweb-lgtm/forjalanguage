/* nativo_json.c - escrita de JSON em Buf para o backend nativo: texto escapado, caminho com '/' e o hash FNV-1a de 32 bits.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - backend x64 P8b: JsonTexto, JsonCaminho e Fnv32.
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#include <string.h>

#include "nativo/nativo_json.h"

#define FNV32_PRIMO 16777619u

void JsonTexto(Buf *saida, const char *texto, size_t n)
{
    BufAnexarN(saida, "\"", 1);
    for (size_t k = 0; k < n; k++) {
        unsigned char ch = (unsigned char)texto[k];

        if (ch == '"' || ch == '\\')
            BufFormatar(saida, "\\%c", ch);
        else if (ch < 0x20)
            BufFormatar(saida, "\\u%04x", (unsigned)ch);
        else
            BufAnexarN(saida, (const char *)&ch, 1);
    }
    BufAnexarN(saida, "\"", 1);
}

void JsonCaminho(Buf *saida, const char *caminho)
{
    Buf normal = {0};

    for (const char *p = caminho; *p != '\0'; p++)
        BufAnexarN(&normal, *p == '\\' ? "/" : p, 1);
    JsonTexto(saida, normal.dados != NULL ? normal.dados : "", normal.tam);
    BufLiberar(&normal);
}

uint32_t Fnv32(uint32_t h, const void *dados, size_t n)
{
    const unsigned char *p = dados;

    for (size_t k = 0; k < n; k++)
        h = (h ^ p[k]) * FNV32_PRIMO;
    return h;
}
