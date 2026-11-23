/* snp_a.c - M3b P23f: membro de libfx_snp.a. Chama o vsnprintf EXTERNO (declarado aqui, sem stdio.h: o gcc nao o troca por inline da UCRT), a mesma importacao que o libcurl.a faz; a ucrtbase nao
 * o exporta, entao a Forja o liga do rt_win_legado.o. fx_snp_formatar(dst, cap, n, texto) escreve "%05d-%s" em dst (cap bytes, com NUL) e devolve o tamanho que o texto inteiro exige,
 * o contrato do C99 (com cap menor que o exigido, o texto sai cortado e o retorno continua o inteiro). */
typedef __builtin_va_list FxLista;

int vsnprintf(char *destino, unsigned long long tamanho, const char *formato, FxLista argumentos);

static int fx_snp_variadica(char *dst, unsigned long long cap, const char *formato, ...)
{
    FxLista argumentos;
    int exigidos;

    __builtin_va_start(argumentos, formato);
    exigidos = vsnprintf(dst, cap, formato, argumentos);
    __builtin_va_end(argumentos);
    return exigidos;
}

int fx_snp_formatar(char *dst, int cap, int n, const char *texto)
{
    int exigidos = fx_snp_variadica(dst, (unsigned long long)cap, "%05d-%s", n, texto);

    return exigidos;
}
