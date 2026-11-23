/* libms.c - biblioteca estatica de teste do M5b (ligador ELF e ABI SysV no call site do foreign): funcoes com a convencao padrao do gcc
 * no alvo Linux (System V AMD64, sem __attribute__((ms_abi))), um global lido e escrito, e um construtor (.init_array) que grava outro global.
 * Compilada no WSL (alpine-build) por construir_ms.sh: libms.a (sem -fPIC) e libms_pic.a (com -fPIC, exercita GOTPCREL).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5b P37: sai __attribute__((ms_abi)); a Forja chama estas funcoes pela ABI SysV (x64_chamada_sysv.c), sem variavel de ambiente.
 *            0.1.0 - M5b P36: primeira versao (com ms_abi, so para o ligador).
 */
int ms_global = 7;
int ms_ctor = 0;

__attribute__((constructor)) static void ms_construtor(void)
{
    ms_ctor = 42;
}

int ms_soma(int a, int b)
{
    return a + b;
}

int ms_troca_global(int v)
{
    int antigo = ms_global;
    ms_global = v;
    return antigo;
}

int ms_ler_ctor(void)
{
    return ms_ctor;
}
