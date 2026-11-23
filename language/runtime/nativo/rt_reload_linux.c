/*
 * rt_reload_linux.c - std:reload no Linux x86-64: so stubs (M8 H2, D-M8-4). O ELF estatico nao tem loader dinamico, entao nao ha hot reload no Linux.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M8 H2: primeira versao. forja_reload_estado, forja_reload_livre, forja_reload_copiar e forja_reload_limpar devolvem 4 (unsupported_target); forja_reload_carregar e forja_reload_simbolo devolvem 0/NULL;
 *           forja_reload_descarregar nao faz nada; forja_reload_info devolve 4 e forja_reload_texto -4. O std:reload mapeia o 4 para ReloadError.unsupported_target logo no open (o 1o passo e o estado do arquivo).
 *
 * Os mesmos nomes e assinaturas do rt_reload_win.c. Incluido por rt_linux.c depois de rt_os_linux.c; nao compila sozinho.
 */
#include <stdint.h>

#define RT_RELOAD_NAO_SUPORTADO 4

int32_t forja_reload_estado(const char *caminho, uint64_t *tamanho, uint64_t *escrita)
{
    (void)caminho;
    (void)tamanho;
    (void)escrita;
    return RT_RELOAD_NAO_SUPORTADO;
}

int32_t forja_reload_livre(const char *caminho)
{
    (void)caminho;
    return RT_RELOAD_NAO_SUPORTADO;
}

int32_t forja_reload_copiar(const char *origem, const char *pasta, const char *destino)
{
    (void)origem;
    (void)pasta;
    (void)destino;
    return RT_RELOAD_NAO_SUPORTADO;
}

uint64_t forja_reload_carregar(const char *caminho)
{
    (void)caminho;
    return 0;
}

void *forja_reload_simbolo(uint64_t modulo, const char *nome)
{
    (void)modulo;
    (void)nome;
    return 0;
}

void forja_reload_descarregar(uint64_t modulo)
{
    (void)modulo;
}

int32_t forja_reload_info(uint64_t modulo, uint64_t *hash, int64_t *texto_len)
{
    (void)modulo;
    (void)hash;
    (void)texto_len;
    return RT_RELOAD_NAO_SUPORTADO;
}

int64_t forja_reload_texto(uint64_t modulo, unsigned char *destino, int64_t n)
{
    (void)modulo;
    (void)destino;
    (void)n;
    return -RT_RELOAD_NAO_SUPORTADO;
}

int32_t forja_reload_limpar(const char *pasta, const char *padrao)
{
    (void)pasta;
    (void)padrao;
    return RT_RELOAD_NAO_SUPORTADO;
}
