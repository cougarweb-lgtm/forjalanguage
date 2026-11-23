/*
 * rt_es_win.c - handle e sistema de arquivos do std:file no Windows x64 (S3 P5b-1, decisoes D6, D8 e D9): as funcoes forja_fs_* de runtime/forja_rt_es.h.
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - POS-Z A51: inclui forja_rt_processo.h (forja_processo_*, std:process) no mesmo objeto; entra na ligacao pelo mesmo simbolo (nativo.c usa_es).
 *   0.2.0 - S3 P5b-2: inclui forja_rt_stdin.h (forja_stdin_ler e forja_stdin_ler_linha, D7) no mesmo objeto; entra na ligacao pelo mesmo simbolo (nativo.c usa_es).
 *   0.1.0 - S3 P5b-1: primeira versao. Inclui rt_sys.h (sys_heap_alocar e sys_heap_liberar) e forja_rt_es.h com FORJA_ES_NATIVO (as funcoes saem externas).
 *           Objeto rt_win_es.o = 8o objeto da ligacao: so entra se o programa referencia forja_fs_* (nativo.c LerEs e usa_es; a deteccao e pelo simbolo, como o math).
 *           Compilado com %FLAGS_RT% (construir.bat).
 *
 * Verificacao: python -I tests\nativo\s3\handle\verificar_handle.py; python -I tests\nativo\s3\stdin\verificar_stdin.py
 */
#define FORJA_ES_NATIVO 1
#include "rt_sys.h"
#include "../forja_rt_es.h"
#include "../forja_rt_stdin.h"
#include "../forja_rt_processo.h"
