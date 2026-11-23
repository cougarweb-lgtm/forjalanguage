/*
 * stubs_posix_a52.h - SO para o oraculo C-musl (oraculo_c.py, -include): o forja_rt_proc.h ainda nao tem o ramo POSIX das funcoes A52 do std:os (cwd, abs_path,
 * exe_path, temp_dir, set_env, random_bytes: M5b). O .c de todo programa que importa std:os as chama dentro de wrappers, entao sem elas ele nao compila no musl.
 * Os programas da prova nunca chamam essas funcoes; aqui so devolvem "erro". Quando o forja_rt_proc.h ganhar o ramo POSIX, apagar este arquivo e o -include.
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 (14/11/2026) M5 P28 - primeira versao.
 */
#include <stdint.h>

static inline int64_t forja_proc_pasta_atual(uint8_t *destino, int64_t n)
{
    (void)destino;
    (void)n;
    return -1;
}

static inline int64_t forja_proc_caminho_absoluto(const char *caminho, uint8_t *destino, int64_t n)
{
    (void)caminho;
    (void)destino;
    (void)n;
    return -1;
}

static inline int64_t forja_proc_caminho_exe(uint8_t *destino, int64_t n)
{
    (void)destino;
    (void)n;
    return -1;
}

static inline int64_t forja_proc_pasta_temp(uint8_t *destino, int64_t n)
{
    (void)destino;
    (void)n;
    return -1;
}

static inline int32_t forja_proc_definir_ambiente(const char *nome, const char *valor)
{
    (void)nome;
    (void)valor;
    return -1;
}

static inline int32_t forja_proc_bytes_aleatorios(uint8_t *destino, int64_t n)
{
    (void)destino;
    (void)n;
    return -1;
}
