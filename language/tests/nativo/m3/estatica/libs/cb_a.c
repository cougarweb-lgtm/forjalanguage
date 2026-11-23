/* cb_a.c - M3b P23f: membro de libfx_cb.a para a bateria ABI item j. fx_cb_descer(cb, x, n) desce n quadros de C (cada um com seu .pdata/.xdata) e so no fundo chama o callback da Forja; o
 * callback captura a pilha (RtlCaptureStackBackTrace) e devolve quantos quadros viu. -O0 e volatile: o gcc nao funde os quadros nem faz chamada em cauda. */
typedef int (*FxRetorno)(int);

static int fx_nivel(FxRetorno cb, int x, int n)
{
    volatile int guarda = n;
    int r;

    if (n == 0)
        return cb(x);
    r = fx_nivel(cb, x, n - 1);
    return r + guarda - n;
}

int fx_cb_descer(FxRetorno cb, int x, int n)
{
    int r = fx_nivel(cb, x, n);

    return r;
}
