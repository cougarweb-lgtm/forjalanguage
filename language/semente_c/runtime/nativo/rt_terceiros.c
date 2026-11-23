/*
 * rt_terceiros.c - o formatador de precisao fixa (rt_fmt_fixo.c, porte do fmt_fp do musl) no runtime nativo da Forja.
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - Ryu d2fixed P4: o Ryu d2fixed (d2fixed.c e a tabela de 329 KB de fonte) saiu; o nome do arquivo fica porque rt_ryu.c, rt_comum_linux.c e conferir_fmt_real.c o incluem. Sem envelope de malloc:
 *           rt_fmt_fixo.c nao aloca.
 *   0.2.1 - M2 P10b: so o comentario (incluido por rt_win.c).
 *   0.2.0 - malloc do Ryu vira rt_terceiros_malloc (R0003 se chamado).
 *   0.1.0 - M2 P10a: NDEBUG e #include do d2fixed.c vendorizado em language\third\ryu.
 *
 * Incluido por rt_ryu.c (Windows, rt_win_ryu.o) e rt_comum_linux.c (rt_linux_comum.o); nao compila sozinho.
 */
#include "rt_fmt_fixo.c"
