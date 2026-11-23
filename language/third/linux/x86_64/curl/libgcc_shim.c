/* libgcc_shim.c - o unico simbolo de libgcc que o libcurl.a pede e o musl (libc.a) nao tem: __popcountdi2.
 * Motivo: lib/uint-bset.c e uint-spbset.c do curl usam __builtin_popcountll; sem -mpopcnt (a CPU minima do alvo e o x86-64 base) o gcc chama __popcountdi2 da libgcc, e o
 * ligador da Forja so junta o libcurl.a e a libc.a do musl. Implementacao SWAR (sem laco e sem tabela, para o gcc nao a reconhecer de volta como __builtin_popcount e
 * se chamar). Licenca: este arquivo e nosso, escrito aqui; nao copia codigo da libgcc.
 * Versao: 0.1.0
 * Changelog: 0.1.0 (19/11/2026) R10a: primeira versao.
 * Verificacao: construir_curl_linux.sh (o script confere por nm que o .o nao pede __popcountdi2 de volta).
 */
int __popcountdi2(long long valor)
{
    unsigned long long x = (unsigned long long)valor;

    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (int)((x * 0x0101010101010101ULL) >> 56);
}
