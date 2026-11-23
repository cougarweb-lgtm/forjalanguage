/* coff_c.c - M3b P23e: membro C de libfx_coff.a. fx_fraco e fx_fraco_so sao WEAK_EXTERNAL; o coff_d.c redefine fx_fraco como forte (a forte vence). */
int __attribute__((weak)) fx_fraco(void)
{
    return 100;
}

int __attribute__((weak)) fx_fraco_so(void)
{
    return 7;
}

int fx_chama_fraco(void)
{
    return fx_fraco() + 1;
}

/* fx_fraco_so so e citado aqui, dentro do proprio membro: o ld do mingw nao resolve um fraco de membro de estatica citado de fora dele. */
int fx_chama_so(void)
{
    return fx_fraco_so() + 2;
}
