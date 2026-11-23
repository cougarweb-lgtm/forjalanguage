/* coff_f.c - M3b P23e: membro F de libfx_coff.a (-fcommon): mesmos simbolos comuns do coff_e.c; o ligador faz um objeto so de cada. */
int fx_contador;
char fx_buf[100];

void fx_inc_f(void)
{
    fx_contador += 5;
}

int fx_ler_contador(void)
{
    return fx_contador * 1000 + fx_buf[3];
}
