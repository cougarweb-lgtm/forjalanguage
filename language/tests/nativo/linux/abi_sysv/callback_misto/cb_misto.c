/* cb_misto.c - lib da fixture de callback misto SysV (M5b P38b), build no alpine por construir_abi_sysv_cb.sh.
 * cbm_chama / cbm_chama8: o C chama o valor de proc que a Forja passou (o stub fjt_ SysV), com 3 e com 8 argumentos (os 2 ultimos de 8 vao para a PILHA).
 * cbm_devolve: devolve o mesmo ponteiro (a Forja o chama por valor de proc). cbm_fn_c: um ponteiro de funcao C genuino que a Forja chama por valor de proc (SysV).
 * cbm_chama_panico: chama cb() e, se ele voltasse, devolveria 99 (o callback da fixture de panico nunca volta).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P38b: primeira versao.
 */
static long soma_c(long a, long b)
{
    return a * 100 + b;
}

double cbm_chama(double (*cb)(int, double, long), int a, double b, long c)
{
    return cb(a, b, c);
}

long cbm_chama8(long (*cb)(long, long, long, long, long, long, long, long))
{
    return cb(1, 2, 3, 4, 5, 6, 7, 8);
}

long (*cbm_devolve(long (*cb)(long, long, long, long, long, long, long, long)))(long, long, long, long, long, long, long, long)
{
    return cb;
}

long (*cbm_fn_c(void))(long, long)
{
    return soma_c;
}

long cbm_chama_panico(long (*cb)(long))
{
    cb(5);
    return 99;
}
