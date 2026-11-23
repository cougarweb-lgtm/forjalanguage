/*
 * rt_saida_erro.c - eprint(...) do runtime nativo da Forja: o buffer do f"..." mais '\n' em stderr, numa escrita so (IA1-L5a, L-04).
 *
 * Versao: 0.2.1
 * Changelog:
 *   0.2.1 - IA1-LX: o objeto rt_win_curto.o e neutro de alvo e tambem liga no ELF (x86_64-linux): o eprint e o print curto de real valem nos 2 alvos; so os comentarios mudaram.
 *   0.2.0 - IA1-L5a: mora no objeto rt_win_curto.o (rt_curto.c), junto do print curto de real; nem no rt_formata.c (um chamador novo de forja_fmt_bytes muda o inline do gcc no rt_win.o) nem
 *           no rt_win_ryu.o (a ordem do .rdata mudava): os exes dos programas sem eprint ficam byte a byte iguais (D-IA1-13; medido nos dois casos).
 *   0.1.0 - IA1-L5a: primeira versao, forja_fmt_imprimir_erro.
 *
 * Referencias indefinidas que o ligador resolve no runtime: forja_fmt_bytes, forja_fmt_buffer (rt_formata.c) e rt_saida_stderr (rt_saida.c; o '\n' sai CRLF no Windows).
 * Incluido por rt_curto.c; nao compila sozinho. Alvo Linux: liga o mesmo objeto; as referencias acima o rt_linux_*.o define (rt_saida_stderr sem o CRLF).
 */
#include "forja_rt_nativo.h"

void rt_saida_stderr(const void *p, int64_t n);

void forja_fmt_imprimir_erro(void)
{
    int64_t n;
    const char *dados;

    forja_fmt_bytes("\n", 1);
    dados = forja_fmt_buffer(&n);
    rt_saida_stderr(dados, n);
}
