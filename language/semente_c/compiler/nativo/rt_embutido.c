/* rt_embutido.c - runtime nativo Windows (rt_win_dev.o e rt_win_rel.o) copiado para dentro do forja.exe por .incbin (P2b, P10b).
 * Os objetos sao gerados pelo construir.bat ao lado do forja.exe e nao sao versionados: os caminhos vem de -DFORJA_RTN_WIN_DEV_CAMINHO e
 * -DFORJA_RTN_WIN_REL_CAMINHO (barras normais, sem aspas).
 * Versao: 0.24.0
 * Changelog: 0.24.0 - M8 P2: rt_win_perfil.o e rt_linux_perfil.o (o runtime do --perfil) embutidos (padrao do proprio arquivo, sem -D); RtNativoWinPerfil, RtNativoLinuxPerfil.
 *            0.23.0 - M6 P69: saem rt_win_dev_rast.o e rt_win_dev_rast_crt.o e RtNativoWinRastreio; rt_win_dev.o e rt_win_dev_crt.o passam a ser compilados com o rastreio (pilha .fj padrao em dev).
 *            0.22.0- M5c-b P59/P60: rt_linux_varena.o e rt_linux_net.o embutidos (padrao do proprio arquivo); RtNativoLinuxVarena, RtNativoLinuxNet.
 *            0.21.0 - M5c-b P58: rt_linux_thread.o, rt_linux_sync.o e rt_linux_atomic.o embutidos (padrao do proprio arquivo); RtNativoLinuxThread, RtNativoLinuxSync, RtNativoLinuxAtomic.
 *            0.20.0 - M5b P41 (--pie): rt_linux_entrada_pie.o (o _start com a auto-relocacao) embutido por -DFORJA_RTN_LINUX_ENTRADA_PIE_CAMINHO (padrao no proprio arquivo); RtNativoLinuxEntradaPie.
 *            0.19.0 - M5b P39-B2 (D0654): rt_linux_entrada_libc.o embutido por -DFORJA_RTN_LINUX_ENTRADA_LIBC_CAMINHO; RtNativoLinuxEntradaLibc.
 *            0.18.0 - M5b P39-B1 (D0654): rt_linux_curto.o embutido por -DFORJA_RTN_LINUX_CURTO_CAMINHO; RtNativoLinuxCurto.
 *            0.17.0 - M5b P39: rt_linux_entrada_cru.o (o _start do ELF, D-R3) embutido por -DFORJA_RTN_LINUX_ENTRADA_CAMINHO; RtNativoLinuxEntrada.
 *            0.16.0 - M5 P53: rt_win_net.o (std:net UDP) embutido por -DFORJA_RTN_WIN_NET_CAMINHO; RtNativoWinNet.
 *            0.15.0 - M5 P52: rt_win_varena.o (std:mem VirtualArena) embutido por -DFORJA_RTN_WIN_VARENA_CAMINHO; RtNativoWinVarena.
 *            0.14.0 - M5 P50: rt_win_atomic.o (std:atomic) embutido por -DFORJA_RTN_WIN_ATOMIC_CAMINHO; RtNativoWinAtomic.
 *            0.13.0 - M5 P49: rt_win_sync.o (std:sync) embutido por -DFORJA_RTN_WIN_SYNC_CAMINHO; RtNativoWinSync.
 *            0.12.0 - M5 P47: rt_win_thread.o (std:thread) embutido por -DFORJA_RTN_WIN_THREAD_CAMINHO; RtNativoWinThread.
 *            0.11.0 - M5 P28b-1: rt_linux_es.o (handle, fs e stdin do Linux) embutido uma vez (-DFORJA_RTN_LINUX_ES_CAMINHO); RtNativoLinuxEs.
 *            0.10.0 - S3 P5b-1: rt_win_es.o (handle e fs do std:file) embutido uma vez (-DFORJA_RTN_WIN_ES_CAMINHO); RtNativoWinEs.
 *            0.9.0 - L11a-2: rt_math.o (libm do musl, std:math) embutido uma vez (-DFORJA_RTN_MATH_CAMINHO); RtNativoMath.
 *            0.8.0 - IA1-L5a: rt_win_curto.o embutido uma vez (-DFORJA_RTN_WIN_CURTO_CAMINHO); RtNativoWinCurto.
 *            0.7.0 - IA1-b12: dois objetos do runtime dev com o rastreio embutidos (-DFORJA_RTN_WIN_RAST_CAMINHO e -DFORJA_RTN_WIN_RAST_CRT_CAMINHO); RtNativoWinRastreio.
 *            0.6.0 - M5 P26d: tres objetos do runtime Linux embutidos (-DFORJA_RTN_LINUX_DEV_CAMINHO, -DFORJA_RTN_LINUX_REL_CAMINHO, -DFORJA_RTN_LINUX_COMUM_CAMINHO); RtNativoLinux, RtNativoLinuxComum.
 *            0.5.0 - M3b P23f: rt_win_legado.o embutido uma vez (-DFORJA_RTN_WIN_LEGADO_CAMINHO); RtNativoWinLegado.
 *            0.4.0 - M3 F5: rt_win_ryu.o embutido uma vez (-DFORJA_RTN_WIN_RYU_CAMINHO); RtNativoWinRyu.
 *            0.3.0 - M3 P19c: mais dois objetos, as variantes -DFORJA_RT_CRT (rt_win_dev_crt.o e rt_win_rel_crt.o, -DFORJA_RTN_WIN_DEV_CRT_CAMINHO e -DFORJA_RTN_WIN_REL_CRT_CAMINHO); RtNativoWinCrt.
 *            0.2.0 - M2 P10b: dois objetos (dev com -DFORJA_DEV, release); RtNativoWin recebe o BirModo.
 *            0.1.0 - P2b: primeira versao; RtNativoWin.
 * Verificacao: construir.bat && teste_common.exe (RtNativoWin: n > 0 e cabecalho 64 86, nos dois modos).
 */
#include "nativo/rt_embutido.h"

#ifndef FORJA_RTN_WIN_DEV_CAMINHO
#error "defina -DFORJA_RTN_WIN_DEV_CAMINHO=<caminho de rt_win_dev.o, com barras normais>"
#endif
#ifndef FORJA_RTN_WIN_REL_CAMINHO
#error "defina -DFORJA_RTN_WIN_REL_CAMINHO=<caminho de rt_win_rel.o, com barras normais>"
#endif

#ifndef FORJA_RTN_WIN_DEV_CRT_CAMINHO
#error "defina -DFORJA_RTN_WIN_DEV_CRT_CAMINHO=<caminho de rt_win_dev_crt.o, com barras normais>"
#endif
#ifndef FORJA_RTN_WIN_REL_CRT_CAMINHO
#error "defina -DFORJA_RTN_WIN_REL_CRT_CAMINHO=<caminho de rt_win_rel_crt.o, com barras normais>"
#endif


#ifndef FORJA_RTN_WIN_RYU_CAMINHO
#error "defina -DFORJA_RTN_WIN_RYU_CAMINHO=<caminho de rt_win_ryu.o, com barras normais>"
#endif

#ifndef FORJA_RTN_WIN_LEGADO_CAMINHO
#error "defina -DFORJA_RTN_WIN_LEGADO_CAMINHO=<caminho de rt_win_legado.o, com barras normais>"
#endif

#ifndef FORJA_RTN_WIN_CURTO_CAMINHO
#error "defina -DFORJA_RTN_WIN_CURTO_CAMINHO=<caminho de rt_win_curto.o, com barras normais>"
#endif

#ifndef FORJA_RTN_MATH_CAMINHO
#error "defina -DFORJA_RTN_MATH_CAMINHO=<caminho de rt_math.o, com barras normais>"
#endif

#ifndef FORJA_RTN_WIN_ES_CAMINHO
#error "defina -DFORJA_RTN_WIN_ES_CAMINHO=<caminho de rt_win_es.o, com barras normais>"
#endif

#ifndef FORJA_RTN_LINUX_DEV_CAMINHO
#error "defina -DFORJA_RTN_LINUX_DEV_CAMINHO=<caminho de rt_linux_dev.o, com barras normais>"
#endif
#ifndef FORJA_RTN_LINUX_REL_CAMINHO
#error "defina -DFORJA_RTN_LINUX_REL_CAMINHO=<caminho de rt_linux_rel.o, com barras normais>"
#endif
#ifndef FORJA_RTN_LINUX_COMUM_CAMINHO
#error "defina -DFORJA_RTN_LINUX_COMUM_CAMINHO=<caminho de rt_linux_comum.o, com barras normais>"
#endif
#ifndef FORJA_RTN_LINUX_ES_CAMINHO
#error "defina -DFORJA_RTN_LINUX_ES_CAMINHO=<caminho de rt_linux_es.o, com barras normais>"
#endif
#ifndef FORJA_RTN_LINUX_ENTRADA_CAMINHO
#error "defina -DFORJA_RTN_LINUX_ENTRADA_CAMINHO=<caminho de rt_linux_entrada_cru.o, com barras normais>"
#endif
/* Padrao: o nome do objeto no diretorio do build (construir.bat roda em compiler\). Sem -D: a linha do FLAGS do construir.bat ja esta perto do limite de 8191 do cmd. */
#ifndef FORJA_RTN_LINUX_ENTRADA_LIBC_CAMINHO
#define FORJA_RTN_LINUX_ENTRADA_LIBC_CAMINHO rt_linux_entrada_libc.o
#endif
/* P41: o _start do --pie (auto-relocacao antes de sys_entrada); o padrao e o nome do objeto no diretorio do build, como o da variante libc. */
#ifndef FORJA_RTN_LINUX_ENTRADA_PIE_CAMINHO
#define FORJA_RTN_LINUX_ENTRADA_PIE_CAMINHO rt_linux_entrada_pie.o
#endif
#ifndef FORJA_RTN_LINUX_CURTO_CAMINHO
#error "defina -DFORJA_RTN_LINUX_CURTO_CAMINHO=<caminho de rt_linux_curto.o, com barras normais>"
#endif
/* P58 (M5c-b): threads, sync e atomic do Linux; padrao = nome do objeto no diretorio do build (construir.bat roda em compiler\), como o da variante libc. */
#ifndef FORJA_RTN_LINUX_THREAD_CAMINHO
#define FORJA_RTN_LINUX_THREAD_CAMINHO rt_linux_thread.o
#endif
#ifndef FORJA_RTN_LINUX_SYNC_CAMINHO
#define FORJA_RTN_LINUX_SYNC_CAMINHO rt_linux_sync.o
#endif
#ifndef FORJA_RTN_LINUX_ATOMIC_CAMINHO
#define FORJA_RTN_LINUX_ATOMIC_CAMINHO rt_linux_atomic.o
#endif
/* P59/P60 (M5c-b): std:mem VirtualArena e std:net no Linux (syscalls cruas); mesmo padrao: nome do objeto no diretorio do build. */
#ifndef FORJA_RTN_LINUX_VARENA_CAMINHO
#define FORJA_RTN_LINUX_VARENA_CAMINHO rt_linux_varena.o
#endif
#ifndef FORJA_RTN_LINUX_NET_CAMINHO
#define FORJA_RTN_LINUX_NET_CAMINHO rt_linux_net.o
#endif
/* M8 P2: o runtime do --perfil (rt_perfil.c por alvo); mesmo padrao: nome do objeto no diretorio do build. So entra na ligacao com --perfil. */
#ifndef FORJA_RTN_WIN_PERFIL_CAMINHO
#define FORJA_RTN_WIN_PERFIL_CAMINHO rt_win_perfil.o
#endif
#ifndef FORJA_RTN_LINUX_PERFIL_CAMINHO
#define FORJA_RTN_LINUX_PERFIL_CAMINHO rt_linux_perfil.o
#endif

#define FORJA_RTN_TEXTO_AUX(x) #x
#define FORJA_RTN_TEXTO(x) FORJA_RTN_TEXTO_AUX(x)

/* Secao de leitura: os pares de simbolos marcam o inicio e o fim de cada objeto copiado pelo montador.
 * Volta para .text no fim: o montador do MinGW desta casa nao aceita .pushsection/.popsection. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_dev_ini\n"
    "forja_rtn_win_dev_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_DEV_CAMINHO) "\"\n"
    ".globl forja_rtn_win_dev_fim\n"
    "forja_rtn_win_dev_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_win_rel_ini\n"
    "forja_rtn_win_rel_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_REL_CAMINHO) "\"\n"
    ".globl forja_rtn_win_rel_fim\n"
    "forja_rtn_win_rel_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_win_dev_crt_ini\n"
    "forja_rtn_win_dev_crt_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_DEV_CRT_CAMINHO) "\"\n"
    ".globl forja_rtn_win_dev_crt_fim\n"
    "forja_rtn_win_dev_crt_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_win_rel_crt_ini\n"
    "forja_rtn_win_rel_crt_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_REL_CRT_CAMINHO) "\"\n"
    ".globl forja_rtn_win_rel_crt_fim\n"
    "forja_rtn_win_rel_crt_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_win_ryu_ini\n"
    "forja_rtn_win_ryu_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_RYU_CAMINHO) "\"\n"
    ".globl forja_rtn_win_ryu_fim\n"
    "forja_rtn_win_ryu_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_win_legado_ini\n"
    "forja_rtn_win_legado_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_LEGADO_CAMINHO) "\"\n"
    ".globl forja_rtn_win_legado_fim\n"
    "forja_rtn_win_legado_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_win_curto_ini\n"
    "forja_rtn_win_curto_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_CURTO_CAMINHO) "\"\n"
    ".globl forja_rtn_win_curto_fim\n"
    "forja_rtn_win_curto_fim:\n"
    ".byte 0\n"
    ".text\n");

/* L11a-2: a libm do musl (rt_math.o, std:math): so entra na ligacao se o programa referencia forja_math_*. Neutro de alvo, como o curto. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_math_ini\n"
    "forja_rtn_math_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_MATH_CAMINHO) "\"\n"
    ".globl forja_rtn_math_fim\n"
    "forja_rtn_math_fim:\n"
    ".byte 0\n"
    ".text\n");

/* S3 P5b-1: o handle e o fs do std:file (rt_win_es.o): so entra na ligacao se o programa referencia forja_fs_*. Neutro de alvo na leitura, como a libm. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_es_ini\n"
    "forja_rtn_win_es_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_ES_CAMINHO) "\"\n"
    ".globl forja_rtn_win_es_fim\n"
    "forja_rtn_win_es_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P47: rt_win_thread.o (std:thread): so entra na ligacao se o programa referencia forja_thread_*. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_thread_ini\n"
    "forja_rtn_win_thread_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_THREAD_CAMINHO) "\"\n"
    ".globl forja_rtn_win_thread_fim\n"
    "forja_rtn_win_thread_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P52: rt_win_varena.o (std:mem VirtualArena): so entra na ligacao se o programa referencia forja_varena_*. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_varena_ini\n"
    "forja_rtn_win_varena_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_VARENA_CAMINHO) "\"\n"
    ".globl forja_rtn_win_varena_fim\n"
    "forja_rtn_win_varena_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M8 P2: o runtime do perfil (rt_win_perfil.o e rt_linux_perfil.o): so entra na ligacao com --perfil. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_perfil_ini\n"
    "forja_rtn_win_perfil_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_PERFIL_CAMINHO) "\"\n"
    ".globl forja_rtn_win_perfil_fim\n"
    "forja_rtn_win_perfil_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_perfil_ini\n"
    "forja_rtn_linux_perfil_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_PERFIL_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_perfil_fim\n"
    "forja_rtn_linux_perfil_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P53: rt_win_net.o (std:net UDP, ws2_32): so entra na ligacao se o programa referencia forja_net_*. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_net_ini\n"
    "forja_rtn_win_net_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_NET_CAMINHO) "\"\n"
    ".globl forja_rtn_win_net_fim\n"
    "forja_rtn_win_net_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P50: rt_win_atomic.o (std:atomic): so entra na ligacao se o programa referencia forja_atomic_*. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_atomic_ini\n"
    "forja_rtn_win_atomic_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_ATOMIC_CAMINHO) "\"\n"
    ".globl forja_rtn_win_atomic_fim\n"
    "forja_rtn_win_atomic_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P49: rt_win_sync.o (std:sync): so entra na ligacao se o programa referencia forja_sync_*. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_win_sync_ini\n"
    "forja_rtn_win_sync_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_WIN_SYNC_CAMINHO) "\"\n"
    ".globl forja_rtn_win_sync_fim\n"
    "forja_rtn_win_sync_fim:\n"
    ".byte 0\n"
    ".text\n");

/* O runtime Linux (M5 P26d): os mesmos pares de simbolos, num bloco a parte. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_rtn_linux_dev_ini\n"
    "forja_rtn_linux_dev_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_DEV_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_dev_fim\n"
    "forja_rtn_linux_dev_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_rel_ini\n"
    "forja_rtn_linux_rel_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_REL_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_rel_fim\n"
    "forja_rtn_linux_rel_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_comum_ini\n"
    "forja_rtn_linux_comum_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_COMUM_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_comum_fim\n"
    "forja_rtn_linux_comum_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_es_ini\n"
    "forja_rtn_linux_es_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_ES_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_es_fim\n"
    "forja_rtn_linux_es_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_entrada_ini\n"
    "forja_rtn_linux_entrada_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_ENTRADA_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_entrada_fim\n"
    "forja_rtn_linux_entrada_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_curto_ini\n"
    "forja_rtn_linux_curto_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_CURTO_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_curto_fim\n"
    "forja_rtn_linux_curto_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_thread_ini\n"
    "forja_rtn_linux_thread_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_THREAD_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_thread_fim\n"
    "forja_rtn_linux_thread_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_sync_ini\n"
    "forja_rtn_linux_sync_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_SYNC_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_sync_fim\n"
    "forja_rtn_linux_sync_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_atomic_ini\n"
    "forja_rtn_linux_atomic_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_ATOMIC_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_atomic_fim\n"
    "forja_rtn_linux_atomic_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_varena_ini\n"
    "forja_rtn_linux_varena_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_VARENA_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_varena_fim\n"
    "forja_rtn_linux_varena_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_net_ini\n"
    "forja_rtn_linux_net_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_NET_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_net_fim\n"
    "forja_rtn_linux_net_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_entrada_libc_ini\n"
    "forja_rtn_linux_entrada_libc_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_ENTRADA_LIBC_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_entrada_libc_fim\n"
    "forja_rtn_linux_entrada_libc_fim:\n"
    ".byte 0\n"
    ".globl forja_rtn_linux_entrada_pie_ini\n"
    "forja_rtn_linux_entrada_pie_ini:\n"
    ".incbin \"" FORJA_RTN_TEXTO(FORJA_RTN_LINUX_ENTRADA_PIE_CAMINHO) "\"\n"
    ".globl forja_rtn_linux_entrada_pie_fim\n"
    "forja_rtn_linux_entrada_pie_fim:\n"
    ".byte 0\n"
    ".text\n");

extern const uint8_t forja_rtn_linux_thread_ini[];
extern const uint8_t forja_rtn_linux_thread_fim[];
extern const uint8_t forja_rtn_linux_sync_ini[];
extern const uint8_t forja_rtn_linux_sync_fim[];
extern const uint8_t forja_rtn_linux_atomic_ini[];
extern const uint8_t forja_rtn_linux_atomic_fim[];
extern const uint8_t forja_rtn_linux_varena_ini[];
extern const uint8_t forja_rtn_linux_varena_fim[];
extern const uint8_t forja_rtn_linux_net_ini[];
extern const uint8_t forja_rtn_linux_net_fim[];
extern const uint8_t forja_rtn_linux_es_ini[];
extern const uint8_t forja_rtn_linux_es_fim[];
extern const uint8_t forja_rtn_linux_entrada_ini[];
extern const uint8_t forja_rtn_linux_entrada_fim[];
extern const uint8_t forja_rtn_linux_entrada_libc_ini[];
extern const uint8_t forja_rtn_linux_entrada_libc_fim[];
extern const uint8_t forja_rtn_linux_entrada_pie_ini[];
extern const uint8_t forja_rtn_linux_entrada_pie_fim[];
extern const uint8_t forja_rtn_linux_dev_ini[];
extern const uint8_t forja_rtn_linux_dev_fim[];
extern const uint8_t forja_rtn_linux_rel_ini[];
extern const uint8_t forja_rtn_linux_rel_fim[];
extern const uint8_t forja_rtn_linux_comum_ini[];
extern const uint8_t forja_rtn_linux_comum_fim[];
extern const uint8_t forja_rtn_win_dev_ini[];
extern const uint8_t forja_rtn_win_dev_fim[];
extern const uint8_t forja_rtn_win_rel_ini[];
extern const uint8_t forja_rtn_win_rel_fim[];
extern const uint8_t forja_rtn_win_dev_crt_ini[];
extern const uint8_t forja_rtn_win_dev_crt_fim[];
extern const uint8_t forja_rtn_win_rel_crt_ini[];
extern const uint8_t forja_rtn_win_rel_crt_fim[];
extern const uint8_t forja_rtn_win_ryu_ini[];
extern const uint8_t forja_rtn_win_ryu_fim[];
extern const uint8_t forja_rtn_win_legado_ini[];
extern const uint8_t forja_rtn_win_legado_fim[];
extern const uint8_t forja_rtn_win_curto_ini[];
extern const uint8_t forja_rtn_win_curto_fim[];
extern const uint8_t forja_rtn_math_ini[];
extern const uint8_t forja_rtn_math_fim[];
extern const uint8_t forja_rtn_win_es_ini[];
extern const uint8_t forja_rtn_win_es_fim[];

/* Devolve o objeto do modo (BM_DEV = dev, qualquer outro = release): ponteiro para o primeiro byte e tamanho em *n (sem o .byte 0 final). */
const uint8_t *RtNativoWin(uint8_t modo, size_t *n)
{
    if (modo == BM_DEV) {
        *n = (size_t)(forja_rtn_win_dev_fim - forja_rtn_win_dev_ini);
        return forja_rtn_win_dev_ini;
    }
    *n = (size_t)(forja_rtn_win_rel_fim - forja_rtn_win_rel_ini);
    return forja_rtn_win_rel_ini;
}

/* Igual a RtNativoWin, mas a variante -DFORJA_RT_CRT (programa com foreign de fora). */
const uint8_t *RtNativoWinCrt(uint8_t modo, size_t *n)
{
    if (modo == BM_DEV) {
        *n = (size_t)(forja_rtn_win_dev_crt_fim - forja_rtn_win_dev_crt_ini);
        return forja_rtn_win_dev_crt_ini;
    }
    *n = (size_t)(forja_rtn_win_rel_crt_fim - forja_rtn_win_rel_crt_ini);
    return forja_rtn_win_rel_crt_ini;
}

/* O Ryu (rt_win_ryu.o): um objeto so para as quatro variantes. */
const uint8_t *RtNativoWinRyu(size_t *n)
{
    *n = (size_t)(forja_rtn_win_ryu_fim - forja_rtn_win_ryu_ini);
    return forja_rtn_win_ryu_ini;
}

/* O runtime Linux do modo (BM_DEV = dev, qualquer outro = release). */
const uint8_t *RtNativoLinux(uint8_t modo, size_t *n)
{
    if (modo == BM_DEV) {
        *n = (size_t)(forja_rtn_linux_dev_fim - forja_rtn_linux_dev_ini);
        return forja_rtn_linux_dev_ini;
    }
    *n = (size_t)(forja_rtn_linux_rel_fim - forja_rtn_linux_rel_ini);
    return forja_rtn_linux_rel_ini;
}

/* O objeto comum do runtime Linux (heap e Ryu): um objeto so para as duas variantes. */
const uint8_t *RtNativoLinuxComum(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_comum_fim - forja_rtn_linux_comum_ini);
    return forja_rtn_linux_comum_ini;
}

/* P28b-1: o handle, o fs e o stdin do Linux (rt_linux_es.o): so entra na ligacao se o programa referencia forja_fs_* ou forja_stdin_*. */
const uint8_t *RtNativoLinuxEs(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_es_fim - forja_rtn_linux_es_ini);
    return forja_rtn_linux_es_ini;
}

/* M5b P39 (D-R3): o _start do Linux sem libc (rt_linux_entrada_cru.o): o objeto de entrada do ELF, sempre ligado no alvo linux. */
const uint8_t *RtNativoLinuxEntrada(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_entrada_fim - forja_rtn_linux_entrada_ini);
    return forja_rtn_linux_entrada_ini;
}

/* M5b P39-B2 (D-R3): o objeto de entrada da variante libc (rt_linux_entrada_libc.o, o main chamado pelo musl): so com foreign no alvo linux. */
const uint8_t *RtNativoLinuxEntradaLibc(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_entrada_libc_fim - forja_rtn_linux_entrada_libc_ini);
    return forja_rtn_linux_entrada_libc_ini;
}

/* P41 (--pie): o objeto de entrada com a auto-relocacao (rt_linux_entrada_pie.o); sem --pie o ELF usa RtNativoLinuxEntrada. */
const uint8_t *RtNativoLinuxEntradaPie(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_entrada_pie_fim - forja_rtn_linux_entrada_pie_ini);
    return forja_rtn_linux_entrada_pie_ini;
}

/* M5c-b P58: std:thread no Linux (rt_linux_thread.o, pthread do musl): so entra na ligacao se o programa referencia forja_thread_* (e liga a variante libc). */
const uint8_t *RtNativoLinuxThread(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_thread_fim - forja_rtn_linux_thread_ini);
    return forja_rtn_linux_thread_ini;
}

/* M5c-b P58: std:sync no Linux (rt_linux_sync.o, futex proprio, syscall 202): so entra na ligacao se o programa referencia forja_sync_*. */
const uint8_t *RtNativoLinuxSync(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_sync_fim - forja_rtn_linux_sync_ini);
    return forja_rtn_linux_sync_ini;
}

/* M5c-b P58: std:atomic no Linux (rt_linux_atomic.o, o rt_atomic.c compilado para o alvo linux): so entra na ligacao se o programa referencia forja_atomic_*. */
const uint8_t *RtNativoLinuxAtomic(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_atomic_fim - forja_rtn_linux_atomic_ini);
    return forja_rtn_linux_atomic_ini;
}

/* M5c-b P59: std:mem VirtualArena no Linux (rt_linux_varena.o, mmap e mprotect por syscall): so entra na ligacao se o programa referencia forja_varena_*. */
const uint8_t *RtNativoLinuxVarena(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_varena_fim - forja_rtn_linux_varena_ini);
    return forja_rtn_linux_varena_ini;
}

/* M5c-b P60: std:net UDP no Linux (rt_linux_net.o, syscalls cruas de socket): so entra na ligacao se o programa referencia forja_net_*. */
const uint8_t *RtNativoLinuxNet(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_net_fim - forja_rtn_linux_net_ini);
    return forja_rtn_linux_net_ini;
}

/* P39-B1 (D0654): o curto do alvo linux (rt_linux_curto.o, copia do rt_win_curto.o com memcpy renomeado para forja_memcpy). Mesmo COFF, outro renome. */
extern const uint8_t forja_rtn_linux_curto_ini[];
extern const uint8_t forja_rtn_linux_curto_fim[];
const uint8_t *RtNativoLinuxCurto(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_curto_fim - forja_rtn_linux_curto_ini);
    return forja_rtn_linux_curto_ini;
}

/* O legado (rt_win_legado.o): stubs da toolchain e sprintf/sscanf sobre a UCRT, um objeto so para todas as variantes. */
const uint8_t *RtNativoWinLegado(size_t *n)
{
    *n = (size_t)(forja_rtn_win_legado_fim - forja_rtn_win_legado_ini);
    return forja_rtn_win_legado_ini;
}

/* L11a-2: o objeto da libm do musl (rt_math.o): so entra na ligacao se o programa referencia forja_math_*. */
const uint8_t *RtNativoMath(size_t *n)
{
    *n = (size_t)(forja_rtn_math_fim - forja_rtn_math_ini);
    return forja_rtn_math_ini;
}

/* S3 P5b-1: o handle e o fs do std:file (rt_win_es.o): so entra na ligacao se o programa referencia forja_fs_*. */
const uint8_t *RtNativoWinEs(size_t *n)
{
    *n = (size_t)(forja_rtn_win_es_fim - forja_rtn_win_es_ini);
    return forja_rtn_win_es_ini;
}

extern const uint8_t forja_rtn_win_thread_ini[];
extern const uint8_t forja_rtn_win_thread_fim[];

/* M5 P47: std:thread (rt_win_thread.o: CreateThread, espera e fecha, com o MXCSR da criadora). */
const uint8_t *RtNativoWinThread(size_t *n)
{
    *n = (size_t)(forja_rtn_win_thread_fim - forja_rtn_win_thread_ini);
    return forja_rtn_win_thread_ini;
}

extern const uint8_t forja_rtn_win_sync_ini[];
extern const uint8_t forja_rtn_win_sync_fim[];

/* M5 P49: std:sync (rt_win_sync.o: SRWLOCK e CONDITION_VARIABLE do Mutex e da Cond). */
const uint8_t *RtNativoWinSync(size_t *n)
{
    *n = (size_t)(forja_rtn_win_sync_fim - forja_rtn_win_sync_ini);
    return forja_rtn_win_sync_ini;
}

extern const uint8_t forja_rtn_win_atomic_ini[];
extern const uint8_t forja_rtn_win_atomic_fim[];

/* M5 P50: std:atomic (rt_win_atomic.o: lock xadd, lock cmpxchg, xchg e mfence por __atomic). */
const uint8_t *RtNativoWinAtomic(size_t *n)
{
    *n = (size_t)(forja_rtn_win_atomic_fim - forja_rtn_win_atomic_ini);
    return forja_rtn_win_atomic_ini;
}

extern const uint8_t forja_rtn_win_varena_ini[];
extern const uint8_t forja_rtn_win_varena_fim[];

/* M5 P52: std:mem VirtualArena (rt_win_varena.o: VirtualAlloc e VirtualFree por forja_varena_*). */
const uint8_t *RtNativoWinVarena(size_t *n)
{
    *n = (size_t)(forja_rtn_win_varena_fim - forja_rtn_win_varena_ini);
    return forja_rtn_win_varena_ini;
}

extern const uint8_t forja_rtn_win_net_ini[];
extern const uint8_t forja_rtn_win_net_fim[];

/* M5 P53: std:net UDP (rt_win_net.o: socket, bind, sendto, recvfrom, select e WSAStartup por forja_net_*). */
const uint8_t *RtNativoWinNet(size_t *n)
{
    *n = (size_t)(forja_rtn_win_net_fim - forja_rtn_win_net_ini);
    return forja_rtn_win_net_ini;
}

extern const uint8_t forja_rtn_win_perfil_ini[];
extern const uint8_t forja_rtn_win_perfil_fim[];
extern const uint8_t forja_rtn_linux_perfil_ini[];
extern const uint8_t forja_rtn_linux_perfil_fim[];

/* M8 P2: o runtime do perfil (rt_win_perfil.o): so entra na ligacao com --perfil. */
const uint8_t *RtNativoWinPerfil(size_t *n)
{
    *n = (size_t)(forja_rtn_win_perfil_fim - forja_rtn_win_perfil_ini);
    return forja_rtn_win_perfil_ini;
}

/* M8 P2: o runtime do perfil no alvo linux (rt_linux_perfil.o): so entra na ligacao com --perfil. */
const uint8_t *RtNativoLinuxPerfil(size_t *n)
{
    *n = (size_t)(forja_rtn_linux_perfil_fim - forja_rtn_linux_perfil_ini);
    return forja_rtn_linux_perfil_ini;
}

/* O objeto do print curto de real e do eprint (rt_win_curto.o, IA1-L5a): so entra na ligacao se o programa referencia simbolo dele. */
const uint8_t *RtNativoWinCurto(size_t *n)
{
    *n = (size_t)(forja_rtn_win_curto_fim - forja_rtn_win_curto_ini);
    return forja_rtn_win_curto_ini;
}
