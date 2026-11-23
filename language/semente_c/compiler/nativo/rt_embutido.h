/* rt_embutido.h - runtime nativo Windows (rt_win_dev.o e rt_win_rel.o) embutido no forja.exe por .incbin (P2b, P10b).
 * Versao: 0.18.0
 * Changelog M8 P2: RtNativoWinPerfil (rt_win_perfil.o) e RtNativoLinuxPerfil (rt_linux_perfil.o): o runtime do --perfil.
 * Changelog M6 P69: sai RtNativoWinRastreio; o runtime dev (RtNativoWin/RtNativoWinCrt/RtNativoLinux) ja leva o rastreio.
 * Changelog M5c-b P59/P60: RtNativoLinuxVarena (rt_linux_varena.o) e RtNativoLinuxNet (rt_linux_net.o).
 * Changelog M5c-b P58: RtNativoLinuxThread, RtNativoLinuxSync e RtNativoLinuxAtomic (os 3 objetos do Linux para std:thread, std:sync e std:atomic).
 * Changelog M5b P39-B2 (D0654): RtNativoLinuxEntradaLibc (rt_linux_entrada_libc.o).
 * Changelog M5b P39-B1 (D0654): RtNativoLinuxCurto (rt_linux_curto.o).
 * Changelog M5b P39: RtNativoLinuxEntrada (rt_linux_entrada_cru.o, o _start do ELF, D-R3).
 * Changelog: 0.13.0 - M5 P52: RtNativoWinVarena (rt_win_varena.o, std:mem VirtualArena).
 *            0.12.0 - M5 P50: RtNativoWinAtomic (rt_win_atomic.o, std:atomic).
 *            0.11.0 - M5 P49: RtNativoWinSync (rt_win_sync.o, std:sync).
 *            0.9.0 - M5 P28b-1: RtNativoLinuxEs (rt_linux_es.o, handle, fs e stdin do Linux).
 *            0.8.3 - S3 P5b-1: RtNativoWinEs (rt_win_es.o, handle e fs do std:file).
 *            0.8.2 - L11a-2: RtNativoMath (rt_math.o, libm do musl).
 *            0.8.1 - IA1-LX: o curto (rt_win_curto.o) e neutro de alvo: tambem liga no ELF; so o comentario de RtNativoWinCurto mudou.
 *            0.8.0 - IA1-L5a: RtNativoWinCurto (rt_win_curto.o).
 *            0.7.0 - IA1-b12: RtNativoWinRastreio(crt, n) (rt_win_dev_rast.o e rt_win_dev_rast_crt.o: o runtime dev com -DFORJA_RASTREIO, so para o forja ... --rastreio).
 *            0.6.0 - M5 P26d: RtNativoLinux(modo, n) e RtNativoLinuxComum(n) (rt_linux_dev.o, rt_linux_rel.o e o objeto comum rt_linux_comum.o: heap dlmalloc + Ryu); nada os usa ainda (o ELF vem no P27b).
 *            0.5.0 - M3b P23f: RtNativoWinLegado (rt_win_legado.o).
 *            0.4.0 - M3 F5: RtNativoWinRyu (rt_win_ryu.o, o Ryu embutido uma vez; as variantes do runtime ficam com referencias indefinidas a ele).
 *            0.3.0 - M3 P19c: RtNativoWinCrt (as variantes -DFORJA_RT_CRT, para o programa com foreign de fora).
 *            0.2.0 - M2 P10b: RtNativoWin(modo, n) escolhe o objeto dev ou release (a faixa de 'as' de real e a unica diferenca).
 *            0.1.0 - P2b: primeira versao; RtNativoWin devolve os bytes do rt_win.o gerado pelo construir.bat.
 * Verificacao: construir.bat && teste_common.exe (2 verificacoes: tamanho > 0 e cabecalho COFF AMD64).
 */
#ifndef FORJA_NATIVO_RT_EMBUTIDO_H
#define FORJA_NATIVO_RT_EMBUTIDO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nativo/bir/bir.h"

/* Devolve o inicio do objeto COFF AMD64 do runtime do modo (BirModo: BM_DEV usa rt_win_dev.o, BM_RELEASE usa rt_win_rel.o) e o tamanho em *n.
 * Os bytes ficam em .rdata do forja.exe: quem chama so le, nunca escreve. */
const uint8_t *RtNativoWin(uint8_t modo, size_t *n);

/* O mesmo, da variante CRT (rt_win_dev_crt.o / rt_win_rel_crt.o): o stdout do programa e o FILE stdout do UCRT; importa __acrt_iob_func, fwrite e fflush da ucrtbase.dll. */
const uint8_t *RtNativoWinCrt(uint8_t modo, size_t *n);

/* O Ryu (rt_win_ryu.o, igual para todas as variantes): objeto COFF AMD64 a ligar junto do runtime; as variantes tem os simbolos dele indefinidos. */
const uint8_t *RtNativoWinRyu(size_t *n);

/* O legado (rt_win_legado.o, igual para todas as variantes): stubs da toolchain (___chkstk_ms, __chkstk, ...) e sprintf/sscanf sobre a UCRT; so entra na ligacao se uma estatica de terceiros os pede. */
const uint8_t *RtNativoWinLegado(size_t *n);

/* O print curto de real e o eprint (rt_win_curto.o, igual para todas as variantes e para os 2 alvos: Windows e Linux): so entra na ligacao se o programa referencia simbolo dele (forja_fmt_f32_curto, forja_fmt_f64_curto, forja_fmt_imprimir_erro). */
const uint8_t *RtNativoWinCurto(size_t *n);

/* L11a-2: a libm do musl (rt_math.o, std:math): objeto neutro de alvo, so entra na ligacao se o programa referencia forja_math_*. */
const uint8_t *RtNativoMath(size_t *n);

/* S3 P5b-1: o handle e o fs do std:file (rt_win_es.o, igual para todas as variantes Windows): so entra na ligacao se o programa referencia forja_fs_*. */
const uint8_t *RtNativoWinEs(size_t *n);
const uint8_t *RtNativoWinThread(size_t *n);
/* M5 P49: rt_win_sync.o (std:sync), so entra na ligacao se o programa referencia forja_sync_*. */
const uint8_t *RtNativoWinSync(size_t *n);
/* M5 P50: rt_win_atomic.o (std:atomic), so entra na ligacao se o programa referencia forja_atomic_*. */
const uint8_t *RtNativoWinAtomic(size_t *n);
/* M5 P52: rt_win_varena.o (std:mem VirtualArena), so entra na ligacao se o programa referencia forja_varena_*. */
const uint8_t *RtNativoWinVarena(size_t *n);
/* M5 P53: rt_win_net.o (std:net UDP), so entra na ligacao se o programa referencia forja_net_*. */
const uint8_t *RtNativoWinNet(size_t *n);

/* Runtime Linux (M5 P26d): objeto COFF AMD64 sem .pdata/.xdata/.idata (BM_DEV = rt_linux_dev.o, qualquer outro = rt_linux_rel.o), com _start e a convencao Win64 por dentro (D-M5-2). */
const uint8_t *RtNativoLinux(uint8_t modo, size_t *n);

/* O objeto comum do runtime Linux (rt_linux_comum.o, igual para as duas variantes): sys_heap_* (dlmalloc) e o Ryu; as variantes tem os simbolos deles indefinidos. */
const uint8_t *RtNativoLinuxComum(size_t *n);

/* P28b-1: o handle, o fs e o stdin do runtime Linux (rt_linux_es.o, igual em dev e release): so entra na ligacao se o programa referencia forja_fs_* ou forja_stdin_*. */
const uint8_t *RtNativoLinuxEs(size_t *n);

/* M5b P39 (D-R3): o _start do Linux sem libc (rt_linux_entrada_cru.o): objeto de entrada do ELF, sempre ligado no alvo linux. */
const uint8_t *RtNativoLinuxEntrada(size_t *n);

/* M5b P39-B2 (D-R3): o objeto de entrada da variante libc (rt_linux_entrada_libc.o, main): so com foreign no alvo linux. */
const uint8_t *RtNativoLinuxEntradaLibc(size_t *n);

/* M5b P41 (--pie): o _start com a auto-relocacao (rt_linux_entrada_pie.o); sem pie o ELF usa RtNativoLinuxEntrada. */
const uint8_t *RtNativoLinuxEntradaPie(size_t *n);

/* P39-B1 (D0654): o curto do alvo linux (rt_linux_curto.o): o mesmo COFF do Windows com memcpy renomeado para forja_memcpy. */
const uint8_t *RtNativoLinuxCurto(size_t *n);

/* M5c-b P58: std:thread (rt_linux_thread.o, pthread do musl): so entra com forja_thread_* e a variante libc. */
const uint8_t *RtNativoLinuxThread(size_t *n);

/* M5c-b P58: std:sync (rt_linux_sync.o, futex proprio): so entra com forja_sync_*. */
const uint8_t *RtNativoLinuxSync(size_t *n);

/* M5c-b P58: std:atomic (rt_linux_atomic.o, o rt_atomic.c para o alvo linux): so entra com forja_atomic_*. */
const uint8_t *RtNativoLinuxAtomic(size_t *n);

/* M5c-b P59: std:mem VirtualArena no Linux (rt_linux_varena.o): so entra com forja_varena_*. */
const uint8_t *RtNativoLinuxVarena(size_t *n);

/* M5c-b P60: std:net UDP no Linux (rt_linux_net.o): so entra com forja_net_*. */
const uint8_t *RtNativoLinuxNet(size_t *n);

/* M8 P2: o runtime do perfil (rt_win_perfil.o e rt_linux_perfil.o): so entra na ligacao com --perfil. */
const uint8_t *RtNativoWinPerfil(size_t *n);
const uint8_t *RtNativoLinuxPerfil(size_t *n);

#endif
