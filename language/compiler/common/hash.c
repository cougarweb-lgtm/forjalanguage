/* hash.c - Hash64: FNV-1a de 64 bits.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22-O1a: primeira versao (sem XXH3: plano v2 5.2).
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/hash.h"

#define HASH64_PRIMO UINT64_C(0x100000001b3)

uint64_t Hash64(const void *dados, size_t n, uint64_t semente)
{
    const unsigned char *p = dados;
    uint64_t h = semente;

    for (size_t k = 0; k < n; k++) {
        h ^= p[k];
        h *= HASH64_PRIMO;
    }
    return h;
}
