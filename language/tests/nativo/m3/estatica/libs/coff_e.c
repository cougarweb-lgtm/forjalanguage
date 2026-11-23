/* coff_e.c - M3b P23e: membro E de libfx_coff.a (compilado com -fcommon): fx_contador e fx_buf sao simbolos COMUNS, o coff_f.c os declara de novo. */
int fx_contador;
char fx_buf[100];

void fx_inc_e(void)
{
    fx_contador += 2;
    fx_buf[3] = 'e';
}
