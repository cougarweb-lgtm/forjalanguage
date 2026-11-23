/* modulos_std.c - tabela da biblioteca padrao embutida (import "std:<nome>").
 * O texto de cada modulo entra no forja.exe por .incbin: a pasta vem de -DFORJA_STD_PASTA (barras normais, sem aspas).
 * Versao: 0.29.5
 * Changelog: 0.29.5 - L3 do marco G3: std:text ganha o pedaco text/parse_float_tabela.fj (tabela do Eisel-Lemire de parse_float) depois de text/parse.fj (.incbin + .byte 10, mesma forma).
 *            0.29.4 - M8 H2: std:reload (language\std\reload.fj + std\reload\testes.fj por FORJA_STD_PURO) nas tabelas STD e TESTES_STD.
 *            0.29.3 - R1 do marco rede: std:url (language\std\url.fj + std\url\testes.fj por FORJA_STD_PURO) nas tabelas STD e TESTES_STD; std:text ganha o 4o pedaco, text/html.fj, depois de text/utf8.fj (.incbin + .byte 10, mesma forma).
 *            0.29.2 - R2: std:time vira a concatenacao de time.fj e time/fuso.fj (mesma forma do std:text).
 *0.29.1 - 0.21.0 F5-a: comentario da secao de leitura aponta o rt_embutido.c (emit_c_runtime.c saiu); codigo igual.
 *            0.29.0 - M6b P79: ModulosStdNomes (os nomes da std, em vetor) troca ModulosStdListar, que cortava a lista em 128 bytes; o E1001 lista todos.
 *            0.28.0 - M5 P53: std:net (language\std\net.fj + std\net\testes.fj por FORJA_STD_PURO) nas tabelas STD e TESTES_STD; foreign interno forja_net_* (rt_net_win.c).
 *            0.27.0 - M5 P51: std:async (language\std\async.fj + std\async\testes.fj por FORJA_STD_PURO) nas tabelas STD e TESTES_STD; Forja pura, sem runtime novo.
 *            0.26.0 - M5 P50: std:atomic (language\std\atomic.fj por .incbin, depois do sync) na tabela STD.
 *            0.25.0 - M5 P48: std:sync (language\std\sync.fj por .incbin, depois do thread) na tabela STD; so Mutex e Cond (tipos marcados, E0339).
 *            0.24.0 - M5 P47: std:thread (language\std\thread.fj por .incbin, entre os e file) na tabela STD.
 *            0.23.0 - POS-Z std: entram binary, hash, sha, base64, uuid, csv, linalg, flags e json (macro FORJA_STD_PURO: texto do modulo e testes.fj por .incbin); process (A51) ja estava.
 *            0.22.0 - S4 P7: TESTES_STD ganha file (std\file\testes.fj) e os (std\os\testes.fj) por .incbin, mesma forma do math; os 9 modulos std tem testes.
 *            0.21.0 - S4 P6: TESTES_STD ganha fmt (std\fmt\testes.fj) e time (std\time\testes.fj) por .incbin, mesma forma do math.
 *            0.20.0 - S4 P5: TESTES_STD ganha text (std\text\testes.fj) e path (std\path\testes.fj) por .incbin, mesma forma do math.
 *            0.19.0 - S4 P4: TESTES_STD ganha rand (std\rand\testes.fj) e mem (std\mem\testes.fj) por .incbin, mesma forma do math.
 *            0.18.0 - S4 P2: tabela TESTES_STD (std\<m>\testes.fj por .incbin, depois do texto do modulo; so math nesta fatia) e ModulosTextoStdTestes/ModulosTextoStdComTestes.
 *            0.17.0 - L11a-2: std:math (std\math.fj por .incbin, 1 arquivo; foreign f64 sobre a libm do musl).
 *            0.16.0 - IA1-S2h: std:fmt ganha fmt/date.fj depois de fmt/parse.fj (.incbin + .byte 10, mesma forma; 6o pedaco).
 *            0.15.0 - IA1-S2f: std:fmt ganha fmt/money.fj e fmt/parse.fj depois de fmt/number.fj (.incbin + .byte 10, mesma forma).
 *            0.14.0 - IA1-S2e: std:fmt (language\std\fmt.fj, .byte 10, fmt/digitos.fj, .byte 10, fmt/number.fj, .byte 10 por .incbin; mesma forma do std:text).
 *            0.13.0 - IA1-S2d2: std:path (language\std\path.fj por .incbin, 1 arquivo).
 *            0.12.0 - IA1-S2d1: std:rand (language\std\rand.fj por .incbin, 1 arquivo).
 *            0.11.0- IA1-S2c: std:text ganha o 3o pedaco, text/utf8.fj, depois de text/parse.fj (.incbin + .byte 10, mesma forma).
 *            0.10.0 - IA1-S2b2a (D-S2-ORG): std:text vira a concatenacao de varios .fj (text.fj, .byte 10, text/parse.fj, .byte 10); ordem literal, sem glob; a linha em std:text:L conta a sequencia inteira.
 *            0.9.0 - IA1-L9b: ModulosTextoStdAlvo (std:os com os.TARGET do alvo linux).
 *            0.8.0 - IA1-LT: std:time (language\std\time.fj por .incbin).
 *            0.7.0 - IA1-L6: std:text (language\std\text.fj por .incbin).
 *            0.6.0 - IA1-L3c: std:arquivo passa a std:file (language\std\file.fj); "arquivo" vale como alias na tabela de renomeados (modulos.c), nao aqui.
 *            0.5.0 - IA1-b01: std:proc passa a std:os (language\std\os.fj); "proc" deixa de existir na tabela (import "std:proc" = E1001 com sugestao, em modulos.c).
 *            0.4.0 - M5 P26c: std:arquivo (language\std\arquivo.fj por .incbin).
 *            0.3.0 - M5 P26b: std:proc (language\std\proc.fj por .incbin).
 *            0.2.0- Linguagem v1.8 P6a: texto de std:mem por .incbin de language\std\mem.fj; ModulosTextoStd le a tabela.
 *            0.1.0 - Linguagem v1.8 P2a: tabela STD {"mem"}; ModulosTextoStd devolve NULL (o .incbin do mem.fj chega no P6a).
 * Verificacao: construir.bat && python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/modulos.h"

#ifndef FORJA_STD_PASTA
#error "defina -DFORJA_STD_PASTA=<pasta language/std, com barras normais>"
#endif

#define FORJA_TEXTO_AUX(x) #x
#define FORJA_TEXTO(x) FORJA_TEXTO_AUX(x)

/* Secao de leitura: os dois simbolos marcam o inicio e o fim do mem.fj copiado pelo montador (mesmo padrao do rt_embutido.c). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_mem_ini\n"
    "forja_std_mem_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/mem.fj\"\n"
    ".globl forja_std_mem_fim\n"
    "forja_std_mem_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_os_ini\n"
    "forja_std_os_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/os.fj\"\n"
    ".globl forja_std_os_fim\n"
    "forja_std_os_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P47: std:thread (spawn e join sobre forja_thread_*). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_thread_ini\n"
    "forja_std_thread_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/thread.fj\"\n"
    ".globl forja_std_thread_fim\n"
    "forja_std_thread_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P50: std:atomic (AtomicI32/I64/U64/Ptr e fence; o runtime fica em rt_atomic.c). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_atomic_ini\n"
    "forja_std_atomic_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/atomic.fj\"\n"
    ".globl forja_std_atomic_fim\n"
    "forja_std_atomic_fim:\n"
    ".byte 0\n"
    ".text\n");

/* M5 P48: std:sync (Mutex e Cond, so o tipo ate o P49). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_sync_ini\n"
    "forja_std_sync_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/sync.fj\"\n"
    ".globl forja_std_sync_fim\n"
    "forja_std_sync_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_file_ini\n"
    "forja_std_file_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/file.fj\"\n"
    ".globl forja_std_file_fim\n"
    "forja_std_file_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_text_ini\n"
    "forja_std_text_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/text.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/text/parse.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/text/parse_float_tabela.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/text/utf8.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/text/html.fj\"\n"
    ".byte 10\n"
    ".globl forja_std_text_fim\n"
    "forja_std_text_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_time_ini\n"
    "forja_std_time_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/time.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/time/fuso.fj\"\n"
    ".byte 10\n"
    ".globl forja_std_time_fim\n"
    "forja_std_time_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_rand_ini\n"
    "forja_std_rand_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/rand.fj\"\n"
    ".globl forja_std_rand_fim\n"
    "forja_std_rand_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_path_ini\n"
    "forja_std_path_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/path.fj\"\n"
    ".globl forja_std_path_fim\n"
    "forja_std_path_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_fmt_ini\n"
    "forja_std_fmt_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/fmt.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/fmt/digitos.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/fmt/number.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/fmt/money.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/fmt/parse.fj\"\n"
    ".byte 10\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/fmt/date.fj\"\n"
    ".byte 10\n"
    ".globl forja_std_fmt_fim\n"
    "forja_std_fmt_fim:\n"
    ".byte 0\n"
    ".text\n");

/* L11a-2: std:math (std\math.fj, foreign f64 sobre a libm do musl). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_math_ini\n"
    "forja_std_math_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/math.fj\"\n"
    ".globl forja_std_math_fim\n"
    "forja_std_math_fim:\n"
    ".byte 0\n"
    ".text\n");

/* S4 P2: testes da std (std\<m>\testes.fj): so forja test <std:modulo> os anexa ao texto do modulo (ModulosTextoStdComTestes). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_math_testes_ini\n"
    "forja_std_math_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/math/testes.fj\"\n"
    ".globl forja_std_math_testes_fim\n"
    "forja_std_math_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

/* S4 P4: testes de std:rand e std:mem, mesma forma do math. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_rand_testes_ini\n"
    "forja_std_rand_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/rand/testes.fj\"\n"
    ".globl forja_std_rand_testes_fim\n"
    "forja_std_rand_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_mem_testes_ini\n"
    "forja_std_mem_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/mem/testes.fj\"\n"
    ".globl forja_std_mem_testes_fim\n"
    "forja_std_mem_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

/* S4 P5: testes de std:text e std:path, mesma forma do math. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_text_testes_ini\n"
    "forja_std_text_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/text/testes.fj\"\n"
    ".globl forja_std_text_testes_fim\n"
    "forja_std_text_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_path_testes_ini\n"
    "forja_std_path_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/path/testes.fj\"\n"
    ".globl forja_std_path_testes_fim\n"
    "forja_std_path_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

/* S4 P6: testes de std:fmt e std:time, mesma forma do math. */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_fmt_testes_ini\n"
    "forja_std_fmt_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/fmt/testes.fj\"\n"
    ".globl forja_std_fmt_testes_fim\n"
    "forja_std_fmt_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_time_testes_ini\n"
    "forja_std_time_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/time/testes.fj\"\n"
    ".globl forja_std_time_testes_fim\n"
    "forja_std_time_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

/* S4 P7: testes de std:file e std:os, mesma forma do math (os 9 modulos std tem testes). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_file_testes_ini\n"
    "forja_std_file_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/file/testes.fj\"\n"
    ".globl forja_std_file_testes_fim\n"
    "forja_std_file_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_os_testes_ini\n"
    "forja_std_os_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/os/testes.fj\"\n"
    ".globl forja_std_os_testes_fim\n"
    "forja_std_os_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

/* POS-Z A51: std:process (std\process.fj, processo filho; foreign forja_processo_*). */
__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_process_ini\n"
    "forja_std_process_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/process.fj\"\n"
    ".globl forja_std_process_fim\n"
    "forja_std_process_fim:\n"
    ".byte 0\n"
    ".text\n");

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_std_process_testes_ini\n"
    "forja_std_process_testes_ini:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/process/testes.fj\"\n"
    ".globl forja_std_process_testes_fim\n"
    "forja_std_process_testes_fim:\n"
    ".byte 0\n"
    ".text\n");

/* POS-Z std: modulos em Forja pura (binary hash sha base64 uuid csv linalg flags json); texto do modulo e testes.fj por .incbin, na mesma forma dos blocos acima. */
#define FORJA_STD_PURO(nome)                                                       \
    __asm__(".section .rdata,\"dr\"\n"                                             \
            ".globl forja_std_" #nome "_ini\n"                                     \
            "forja_std_" #nome "_ini:\n"                                           \
            ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/" #nome ".fj\"\n"          \
            ".globl forja_std_" #nome "_fim\n"                                     \
            "forja_std_" #nome "_fim:\n"                                           \
            ".byte 0\n"                                                            \
            ".globl forja_std_" #nome "_testes_ini\n"                              \
            "forja_std_" #nome "_testes_ini:\n"                                    \
            ".incbin \"" FORJA_TEXTO(FORJA_STD_PASTA) "/" #nome "/testes.fj\"\n"   \
            ".globl forja_std_" #nome "_testes_fim\n"                              \
            "forja_std_" #nome "_testes_fim:\n"                                    \
            ".byte 0\n"                                                            \
            ".text\n");                                                            \
    extern const char forja_std_##nome##_ini[];                                    \
    extern const char forja_std_##nome##_fim[];                                    \
    extern const char forja_std_##nome##_testes_ini[];                             \
    extern const char forja_std_##nome##_testes_fim[]

FORJA_STD_PURO(binary);
FORJA_STD_PURO(hash);
FORJA_STD_PURO(sha);
FORJA_STD_PURO(base64);
FORJA_STD_PURO(uuid);
FORJA_STD_PURO(csv);
FORJA_STD_PURO(linalg);
FORJA_STD_PURO(flags);
FORJA_STD_PURO(json);

/* M5 P51: std:async (Pool e Group em Forja pura sobre std:thread e std:sync). */
FORJA_STD_PURO(async);

/* M5 P53: std:net (UDP): foreign interno forja_net_* (rt_net_win.c) sob a API em Forja. */
FORJA_STD_PURO(net);

/* R1 do marco rede: std:url (RFC 3986, Forja pura). */
FORJA_STD_PURO(url);

/* M8 H2: std:reload (hot reload de DLL Forja): foreign interno forja_reload_* (rt_reload_win.c). */
FORJA_STD_PURO(reload);

extern const char forja_std_process_ini[];
extern const char forja_std_process_fim[];
extern const char forja_std_process_testes_ini[];
extern const char forja_std_process_testes_fim[];
extern const char forja_std_mem_ini[];
extern const char forja_std_mem_fim[];
extern const char forja_std_os_ini[];
extern const char forja_std_os_fim[];
extern const char forja_std_thread_ini[];
extern const char forja_std_thread_fim[];
extern const char forja_std_sync_ini[];
extern const char forja_std_sync_fim[];
extern const char forja_std_atomic_ini[];
extern const char forja_std_atomic_fim[];
extern const char forja_std_file_ini[];
extern const char forja_std_file_fim[];
extern const char forja_std_text_ini[];
extern const char forja_std_text_fim[];
extern const char forja_std_time_ini[];
extern const char forja_std_time_fim[];
extern const char forja_std_rand_ini[];
extern const char forja_std_rand_fim[];
extern const char forja_std_path_ini[];
extern const char forja_std_path_fim[];
extern const char forja_std_fmt_ini[];
extern const char forja_std_fmt_fim[];
extern const char forja_std_math_ini[];
extern const char forja_std_math_fim[];
extern const char forja_std_math_testes_ini[];
extern const char forja_std_math_testes_fim[];
extern const char forja_std_rand_testes_ini[];
extern const char forja_std_rand_testes_fim[];
extern const char forja_std_mem_testes_ini[];
extern const char forja_std_mem_testes_fim[];
extern const char forja_std_text_testes_ini[];
extern const char forja_std_text_testes_fim[];
extern const char forja_std_path_testes_ini[];
extern const char forja_std_path_testes_fim[];
extern const char forja_std_fmt_testes_ini[];
extern const char forja_std_fmt_testes_fim[];
extern const char forja_std_time_testes_ini[];
extern const char forja_std_time_testes_fim[];
extern const char forja_std_file_testes_ini[];
extern const char forja_std_file_testes_fim[];
extern const char forja_std_os_testes_ini[];
extern const char forja_std_os_testes_fim[];

static const struct { const char *nome; const char *ini, *fim; } STD[] = {
    {"mem", forja_std_mem_ini, forja_std_mem_fim},
    {"os", forja_std_os_ini, forja_std_os_fim},
    {"thread", forja_std_thread_ini, forja_std_thread_fim},
    {"sync", forja_std_sync_ini, forja_std_sync_fim},
    {"atomic", forja_std_atomic_ini, forja_std_atomic_fim},
    {"file", forja_std_file_ini, forja_std_file_fim},
    {"text", forja_std_text_ini, forja_std_text_fim},
    {"time", forja_std_time_ini, forja_std_time_fim},
    {"rand", forja_std_rand_ini, forja_std_rand_fim},
    {"path", forja_std_path_ini, forja_std_path_fim},
    {"fmt", forja_std_fmt_ini, forja_std_fmt_fim},
    {"math", forja_std_math_ini, forja_std_math_fim},
    {"process", forja_std_process_ini, forja_std_process_fim},
    {"binary", forja_std_binary_ini, forja_std_binary_fim},
    {"hash", forja_std_hash_ini, forja_std_hash_fim},
    {"sha", forja_std_sha_ini, forja_std_sha_fim},
    {"base64", forja_std_base64_ini, forja_std_base64_fim},
    {"uuid", forja_std_uuid_ini, forja_std_uuid_fim},
    {"csv", forja_std_csv_ini, forja_std_csv_fim},
    {"linalg", forja_std_linalg_ini, forja_std_linalg_fim},
    {"flags", forja_std_flags_ini, forja_std_flags_fim},
    {"json", forja_std_json_ini, forja_std_json_fim},
    {"async", forja_std_async_ini, forja_std_async_fim},
    {"net", forja_std_net_ini, forja_std_net_fim},
    {"url", forja_std_url_ini, forja_std_url_fim},
    {"reload", forja_std_reload_ini, forja_std_reload_fim},
};

enum { STD_QTD = sizeof STD / sizeof STD[0] };

bool ModulosStdTem(const char *nome, uint32_t tam)
{
    for (size_t k = 0; k < STD_QTD; k++) {
        if (strlen(STD[k].nome) == tam && memcmp(STD[k].nome, nome, tam) == 0) return true;
    }
    return false;
}

size_t ModulosStdNomes(const char **nomes, size_t cap)
{
    size_t n = 0;
    for (size_t k = 0; k < STD_QTD && n < cap; k++) nomes[n++] = STD[k].nome;
    return n;
}

const char *ModulosTextoStd(const char *nome, size_t *tam)
{
    for (size_t k = 0; k < STD_QTD; k++) {
        if (strcmp(STD[k].nome, nome) != 0) continue;
        *tam = (size_t)(STD[k].fim - STD[k].ini);
        return STD[k].ini;
    }
    *tam = 0;
    return NULL;
}

/* S4 P2/P4/P5/P6/P7: so os modulos com testes.fj entram aqui; os 9 modulos std tem entrada. */
static const struct { const char *nome; const char *ini, *fim; } TESTES_STD[] = {
    {"math", forja_std_math_testes_ini, forja_std_math_testes_fim},
    {"rand", forja_std_rand_testes_ini, forja_std_rand_testes_fim},
    {"mem", forja_std_mem_testes_ini, forja_std_mem_testes_fim},
    {"text", forja_std_text_testes_ini, forja_std_text_testes_fim},
    {"path", forja_std_path_testes_ini, forja_std_path_testes_fim},
    {"fmt", forja_std_fmt_testes_ini, forja_std_fmt_testes_fim},
    {"time", forja_std_time_testes_ini, forja_std_time_testes_fim},
    {"file", forja_std_file_testes_ini, forja_std_file_testes_fim},
    {"os", forja_std_os_testes_ini, forja_std_os_testes_fim},
    {"process", forja_std_process_testes_ini, forja_std_process_testes_fim},
    {"binary", forja_std_binary_testes_ini, forja_std_binary_testes_fim},
    {"hash", forja_std_hash_testes_ini, forja_std_hash_testes_fim},
    {"sha", forja_std_sha_testes_ini, forja_std_sha_testes_fim},
    {"base64", forja_std_base64_testes_ini, forja_std_base64_testes_fim},
    {"uuid", forja_std_uuid_testes_ini, forja_std_uuid_testes_fim},
    {"csv", forja_std_csv_testes_ini, forja_std_csv_testes_fim},
    {"linalg", forja_std_linalg_testes_ini, forja_std_linalg_testes_fim},
    {"flags", forja_std_flags_testes_ini, forja_std_flags_testes_fim},
    {"json", forja_std_json_testes_ini, forja_std_json_testes_fim},
    {"async", forja_std_async_testes_ini, forja_std_async_testes_fim},
    {"net", forja_std_net_testes_ini, forja_std_net_testes_fim},
    {"url", forja_std_url_testes_ini, forja_std_url_testes_fim},
    {"reload", forja_std_reload_testes_ini, forja_std_reload_testes_fim},
};

const char *ModulosTextoStdTestes(const char *nome, size_t *tam)
{
    for (size_t k = 0; k < sizeof TESTES_STD / sizeof TESTES_STD[0]; k++) {
        if (strcmp(TESTES_STD[k].nome, nome) != 0) continue;
        *tam = (size_t)(TESTES_STD[k].fim - TESTES_STD[k].ini);
        return TESTES_STD[k].ini;
    }
    *tam = 0;
    return NULL;
}

/* S4 P2: o pedaco de testes vai DEPOIS do texto do modulo (com '\n' entre os dois), entao as linhas do modulo nao mudam. Copia na arena. */
const char *ModulosTextoStdComTestes(const char *nome, const char *texto, Arena *arena, size_t *tam)
{
    size_t tam_testes = 0, novo_tam;
    const char *testes = ModulosTextoStdTestes(nome, &tam_testes);
    char *novo;

    if (testes == NULL) return texto;
    novo_tam = *tam + 1 + tam_testes;
    novo = ArenaAlocar(arena, novo_tam + 1);
    memcpy(novo, texto, *tam);
    novo[*tam] = '\n';
    memcpy(novo + *tam + 1, testes, tam_testes);
    novo[novo_tam] = '\0';
    *tam = novo_tam;
    return novo;
}

/* IA1-L9b (B11): os.TARGET e resolvido na compilacao. O std\os.fj escreve o alvo padrao entre aspas; no alvo linux o texto sai com o outro no lugar (copia na arena). */
#define OS_ALVO_WINDOWS "\"x86_64-windows\""
#define OS_ALVO_LINUX "\"x86_64-linux\""

const char *ModulosTextoStdAlvo(const char *nome, bool alvo_linux, Arena *arena, size_t *tam)
{
    const char *texto = ModulosTextoStd(nome, tam);
    if (texto == NULL || !alvo_linux || strcmp(nome, "os") != 0) return texto;
    const char *achado = strstr(texto, OS_ALVO_WINDOWS); /* o .incbin termina com um byte 0 */
    if (achado == NULL) return texto;
    size_t antes = (size_t)(achado - texto), tira = strlen(OS_ALVO_WINDOWS), poe = strlen(OS_ALVO_LINUX);
    size_t novo_tam = *tam - tira + poe;
    char *novo = ArenaAlocar(arena, novo_tam + 1);
    memcpy(novo, texto, antes);
    memcpy(novo + antes, OS_ALVO_LINUX, poe);
    memcpy(novo + antes + poe, achado + tira, *tam - antes - tira);
    novo[novo_tam] = '\0';
    *tam = novo_tam;
    return novo;
}
