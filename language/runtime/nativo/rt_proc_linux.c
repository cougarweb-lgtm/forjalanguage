/*
 * rt_proc_linux.c - camada sys do std:proc no Linux x86-64: argumentos e variavel de ambiente da pilha inicial (M5 P26d).
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - M5c-b P63 (D-R7): trava do ambiente vivo (sys_ambiente_travar/destravar); set_env/unset_env inteiros e as leituras sys_ambiente/sys_ambiente_utf8 (que copiam o valor) sob a trava.
 *   0.2.0 - M5 P28b-1: leitura pelo ambiente VIVO (sysl_envp_vivo); sysl_ambiente_definir (set_env/unset_env: a 1a mutacao copia vetor e textos do envp para o heap, o sysl_envp original nunca e tocado),
 *           sys_ambiente_vetor (o vetor vivo, para o execve do P28b-2) e sysl_execfn (AT_EXECFN do auxv, lido depois do NULL do envp ORIGINAL; NULL se o kernel nao o entregou).
 *   0.1.0 - M5 P26d: sys_ambiente (servico do rt_teste.c), sys_argumentos e sys_ambiente_utf8 (std:proc, rt_proc.c). O Linux entrega argv e envp prontos em UTF-8 na pilha inicial
 *           (rt_sys_linux.c guarda os ponteiros): nao ha linha de comando para dividir (rt_linha.c e so do Windows) nem conversao. Nada e copiado nem alocado aqui.
 *
 * Incluido por rt_linux.c depois de rt_sys_linux.c; nao compila sozinho.
 */
#include <stddef.h>

#include "rt_sys.h"

/* Se a entrada "NOME=valor" do ambiente tem o nome pedido, devolve o ponteiro para o valor; senao NULL. */
static const char *sysl_entrada_casa(const char *entrada, const char *nome)
{
    while (*nome != '\0' && *entrada == *nome) {
        entrada++;
        nome++;
    }
    return (*nome == '\0' && *entrada == '=') ? entrada + 1 : NULL;
}

/* Valor da variavel `nome` (UTF-8, terminado em NUL, dentro da pilha inicial), ou NULL se nao existe. */
static const char *sysl_valor_ambiente(const char *nome)
{
    const char *valor;

    if (sysl_envp_vivo == NULL)
        return NULL;
    for (char **entrada = sysl_envp_vivo; *entrada != NULL; entrada++) {
        valor = sysl_entrada_casa(*entrada, nome);
        if (valor != NULL)
            return valor;
    }
    return NULL;
}

static int64_t sysl_tamanho_texto(const char *texto)
{
    int64_t n = 0;

    while (texto[n] != '\0')
        n++;
    return n;
}

/* Copia tam bytes de origem para destino e fecha com NUL (destino tem de ter tam + 1). */
static void sysl_copiar_texto(char *destino, const char *origem, int64_t tam)
{
    for (int64_t i = 0; i < tam; i++)
        destino[i] = origem[i];
    destino[tam] = '\0';
}

/* ---- ambiente mutavel (P28b-1) ---- */

#define SYSL_AT_EXECFN 31LL
#define SYSL_TAMANHO_ENTRADA_AUXV 2 /* pares {tipo, valor} de 8 bytes; AT_NULL (0) fecha */

static bool sysl_ambiente_copiado;

static int64_t sysl_contar_ambiente(void)
{
    int64_t n = 0;

    while (sysl_envp_vivo[n] != NULL)
        n++;
    return n;
}

/* Texto novo no heap: nome, "=" opcional e valor. NULL se faltar memoria.
 * LIMITE: parametros 5 posicionais: 1 definicao e 2 chamadores (definir e acrescentar), sem struct para nao criar tipo de uso unico. */
static char *sysl_texto_heap(const char *a, int64_t na, const char *b, int64_t nb, bool com_igual)
{
    char *texto = sys_heap_alocar(na + nb + 2);

    if (texto == NULL)
        return NULL;
    for (int64_t i = 0; i < na; i++)
        texto[i] = a[i];
    if (com_igual)
        texto[na++] = '=';
    for (int64_t i = 0; i < nb; i++)
        texto[na + i] = b[i];
    texto[na + nb] = '\0';
    return texto;
}

/* 1a mutacao: o vetor e os textos da pilha inicial passam a copias no heap. false = sem memoria (nada muda). */
static bool sysl_ambiente_copiar(void)
{
    int64_t n;
    char **copia;

    if (sysl_ambiente_copiado)
        return true;
    n = sysl_contar_ambiente();
    copia = sys_heap_alocar((n + 1) * (int64_t)sizeof(char *));
    if (copia == NULL)
        return false;
    for (int64_t i = 0; i < n; i++) {
        copia[i] = sysl_texto_heap(sysl_envp_vivo[i], sysl_tamanho_texto(sysl_envp_vivo[i]), "", 0, false);
        if (copia[i] == NULL) {
            while (i-- > 0)
                sys_heap_liberar(copia[i]);
            sys_heap_liberar(copia);
            return false;
        }
    }
    copia[n] = NULL;
    sysl_envp_vivo = copia;
    sysl_ambiente_copiado = true;
    return true;
}

/* Indice da variavel `nome` no ambiente vivo, ou -1. */
static int64_t sysl_achar_ambiente(const char *nome)
{
    for (int64_t i = 0; sysl_envp_vivo[i] != NULL; i++) {
        if (sysl_entrada_casa(sysl_envp_vivo[i], nome) != NULL)
            return i;
    }
    return -1;
}

/* Acrescenta `texto` ao fim do vetor vivo (novo vetor, o antigo, que ja e do heap, e liberado). false = sem memoria. */
static bool sysl_ambiente_acrescentar(char *texto)
{
    int64_t n = sysl_contar_ambiente();
    char **novo = sys_heap_alocar((n + 2) * (int64_t)sizeof(char *));

    if (novo == NULL)
        return false;
    for (int64_t i = 0; i < n; i++)
        novo[i] = sysl_envp_vivo[i];
    novo[n] = texto;
    novo[n + 1] = NULL;
    sys_heap_liberar(sysl_envp_vivo);
    sysl_envp_vivo = novo;
    return true;
}

/* Trava do ambiente vivo (D-R7: uma por grupo): set_env/unset_env inteiros, as leituras que copiam o valor e o trecho do std:process entre montar o execve e o fork. */
static SysTrava sysl_amb_trava; /* compartilhado: so com rt_multithread ligado */

bool sys_ambiente_travar(void)
{
    return sys_trava_pegar(&sysl_amb_trava);
}

void sys_ambiente_destravar(bool tomou)
{
    sys_trava_soltar(&sysl_amb_trava, tomou);
}

/* valor NULL remove (remover a que nao existe e sucesso). 0 = ok, 3 = sem memoria. Chamar com a trava do ambiente tomada. */
static int32_t sysl_ambiente_definir_dentro(const char *nome, const char *valor)
{
    int64_t indice;
    char *texto = NULL;

    if (!sysl_ambiente_copiar())
        return 3;
    indice = sysl_achar_ambiente(nome);
    if (valor != NULL) {
        texto = sysl_texto_heap(nome, sysl_tamanho_texto(nome), valor, sysl_tamanho_texto(valor), true);
        if (texto == NULL)
            return 3;
    }
    if (indice < 0)
        return (texto == NULL || sysl_ambiente_acrescentar(texto)) ? 0 : 3;
    sys_heap_liberar(sysl_envp_vivo[indice]);
    if (texto != NULL) {
        sysl_envp_vivo[indice] = texto;
        return 0;
    }
    for (int64_t i = indice; sysl_envp_vivo[i] != NULL; i++)
        sysl_envp_vivo[i] = sysl_envp_vivo[i + 1];
    return 0;
}

static int32_t sysl_ambiente_definir(const char *nome, const char *valor)
{
    bool tomou = sys_ambiente_travar();
    int32_t r = sysl_ambiente_definir_dentro(nome, valor);

    sys_ambiente_destravar(tomou);
    return r;
}

/* O vetor do ambiente VIVO (terminado em NULL), para o execve do filho (P28b-2). */
char **sys_ambiente_vetor(void)
{
    return sysl_envp_vivo;
}

/* AT_EXECFN: o caminho que o kernel passou ao execve (texto na pilha inicial), ou NULL. O auxv vem depois do NULL do envp ORIGINAL. */
static const char *sysl_execfn(void)
{
    const int64_t *auxv;
    char **fim = sysl_envp;

    if (fim == NULL)
        return NULL;
    while (*fim != NULL)
        fim++;
    auxv = (const int64_t *)(fim + 1);
    for (; auxv[0] != 0; auxv += SYSL_TAMANHO_ENTRADA_AUXV) {
        if (auxv[0] == SYSL_AT_EXECFN)
            return (const char *)auxv[1];
    }
    return NULL;
}

int64_t sys_ambiente(const char *nome, char *destino, int64_t n)
{
    const char *valor;
    int64_t tam = 0;
    bool tomou;

    if (n <= 0)
        return -1;
    tomou = sys_ambiente_travar();
    valor = sysl_valor_ambiente(nome);
    if (valor == NULL) {
        destino[0] = '\0';
    } else if (sysl_tamanho_texto(valor) >= n) {
        destino[0] = '\0';
        tam = -1;
    } else {
        tam = sysl_tamanho_texto(valor);
        sysl_copiar_texto(destino, valor, tam);
    }
    sys_ambiente_destravar(tomou);
    return tam;
}

void sys_argumentos(int64_t *argc, char ***argv)
{
    *argc = sysl_argc;
    *argv = sysl_argv;
}

int64_t sys_ambiente_utf8(const char *nome, char *destino, int64_t n)
{
    bool tomou = sys_ambiente_travar();
    const char *valor = sysl_valor_ambiente(nome);
    int64_t tam = -1;

    if (valor != NULL) {
        tam = sysl_tamanho_texto(valor);
        if (n > tam)
            sysl_copiar_texto(destino, valor, tam);
    }
    sys_ambiente_destravar(tomou);
    return tam;
}
