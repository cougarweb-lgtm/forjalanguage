/*
 * rt_sys.h - camada sys do runtime nativo da Forja (P2a, Windows x64).
 *
 * Versao: 0.12.1
 * Changelog:
 *   0.12.1 - M6e P93f: sys_guarda_limite_mapas (commit recusado por limite de mapeamentos do kernel) e offsets do Linux na doc de SysFalha.
 *   0.12.0 - M6e P93e: SysFalha e sys_falha_ligar (pre-tratador da guarda de memoria), sys_guarda_reservar/commit/decommit.
 *   0.11.1 - M5c-b P63: sys_trava_pegar/soltar ganham RT_BARREIRA() no Linux (o gcc lia o static protegido antes da trava; ver rt_compat.h 0.5.0); inclui rt_compat.h.
 *   0.11.0 - M5c-b P63 (D-R7): sys_ambiente_travar/sys_ambiente_destravar (so Linux).
 *   0.10.0 - M5c-b P57: o Linux ganha rt_multithread, sys_multithread_ligar e a trava (sys_trava_pegar_srw/soltar_srw, futex proprio de 3 estados em rt_sys_linux.c); mesmas funcoes inline do Windows. O backend C segue sem trava.
 *   0.9.0 - M5 P44: SysTrava (SRWLOCK de uma palavra, zero = livre), sys_trava_pegar e sys_trava_soltar: so travam com rt_multithread ligado (flag pegajoso, ligado por
 *           sys_multithread_ligar quando uma thread que o runtime nao criou entra); desligado custa um teste de byte. Pegar devolve se travou: soltar recebe esse valor, e a
 *           flag virar no meio de uma secao nao desequilibra. Linux (ainda monothread): no-ops. No Windows: AcquireSRWLockExclusive e ReleaseSRWLockExclusive (65o e 66o imports).
 *   0.8.0 - M5 P43: sys_tls_alocar, sys_tls_ler e sys_tls_gravar (so RT_ALVO_WIN): o slot do RtFio por thread.
 *   0.7.0 - M5 P28b-1: sys_ambiente_vetor (so sob RT_ALVO_LINUX).
 *   0.6.0 - M5 P26c: sys_arquivo_abrir_ler, sys_arquivo_criar, sys_arquivo_tamanho, sys_arquivo_ler, sys_arquivo_gravar e sys_arquivo_fechar (std:arquivo); erro em codigo neutro
 *           SYS_ARQ_NAO_EXISTE / SYS_ARQ_SEM_PERMISSAO / SYS_ARQ_OUTRO. No Windows moram em rt_arquivo_win.c (unidade do rt_win_ryu.o), nao em rt_sys_win.c (D-M5-11).
 *   0.5.0 - M5 P26b: sys_argumentos (linha de comando dividida em argumentos UTF-8) e sys_ambiente_utf8 (variavel de ambiente com nome e valor UTF-8), para o std:proc.
 *           No Windows moram em rt_proc_win.c (unidade do rt_win_ryu.o), nao em rt_sys_win.c: so os declara aqui quem usa (rt_proc.c).
 *   0.4.0- Linguagem v1.11 L16b: sys_excecao_ligar (tratador de excecao do processador; so o rt_teste.c a usa).
 *   0.3.0 -Linguagem v1.11 L16a: sys_ambiente (variavel de ambiente; so o rt_teste.c a usa).
 *   0.2.0 - sys_console_ligar_utf8 e sys_console_restaurar: pagina de codigo do console em UTF-8 durante o programa.
 *   0.1.0 - primeira versao: escrita, console, saida, heap, paginas e relogio.
 *
 * Unica parte do runtime que fala com o sistema operacional. Sem libc: so tipos de
 * stdint.h e stdbool.h. A implementacao Windows esta em rt_sys_win.c (15 imports de kernel32).
 * Os .c do runtime sao incluidos por rt_win.c; este header so declara.
 */
#ifndef RT_SYS_H
#define RT_SYS_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_compat.h"

/* Escreve n bytes no fd 1 ou 2, repetindo a chamada ate sair tudo. Falha de escrita para sem erro. */
void sys_escrever(int fd, const void *buf, int64_t n);

/* true quando fd e char device (FILE_TYPE_CHAR). NUL conta como char, como _isatty do UCRT. */
bool sys_e_console(int fd);

/* Com stdout ou stderr num console cuja pagina de codigo de saida nao e UTF-8, passa para 65001 e guarda a original.
 * Sem console nao chama nada. sys_sair restaura a pagina guardada (sys_console_restaurar e idempotente). */
void sys_console_ligar_utf8(void);
void sys_console_restaurar(void);

/* Encerra o processo com o codigo. Nao volta. Quem chama descarrega o stdout antes. */
_Noreturn void sys_sair(int codigo);

/* Heap do processo. Devolvem NULL quando falta memoria; quem chama decide a falha. */
void *sys_heap_alocar(int64_t n);
void *sys_heap_realocar(void *p, int64_t n);
void sys_heap_liberar(void *p);

/* Paginas do sistema (reserva e commit juntos). n em bytes; liberar devolve a regiao inteira. */
void *sys_paginas_reservar(int64_t n);
void sys_paginas_liberar(void *p, int64_t n);

/* Valor da variavel de ambiente `nome` em destino (com NUL). Devolve o tamanho sem o NUL, 0 se ausente ou vazia, -1 se nao coube em n bytes. */
int64_t sys_ambiente(const char *nome, char *destino, int64_t n);

/* Argumentos do processo em UTF-8, cada um terminado em NUL, validos ate o fim do processo; argv[argc] = NULL. No Windows e a linha de comando crua dividida
 * pelas regras do CommandLineToArgvW (rt_linha.c). Sem memoria: argc = 0 e argv = NULL. Cada chamada monta um conjunto novo (quem chama guarda). */
void sys_argumentos(int64_t *argc, char ***argv);

/* Variavel de ambiente `nome` (UTF-8): devolve o tamanho do valor em bytes sem o NUL (0 = definida e vazia) ou -1 se nao existe. Copia o valor (com NUL) para
 * destino so quando cabe em n bytes; n = 0 so mede (destino pode ser NULL). */
int64_t sys_ambiente_utf8(const char *nome, char *destino, int64_t n);

#ifdef RT_ALVO_LINUX
/* P28b-1: o vetor do ambiente VIVO (envp terminado em NULL; set_env/unset_env o trocam por uma copia no heap), para o execve do filho (P28b-2). So o runtime Linux o define. */
char **sys_ambiente_vetor(void);
/* D-R7: trava do ambiente vivo (no-op com 1 thread). Quem le o vetor e o usa depois (std:process entre montar o execve e o fork) segura a trava; o par devolve o `tomou`. */
bool sys_ambiente_travar(void);
void sys_ambiente_destravar(bool tomou);
#endif

/* Arquivos inteiros em modo binario (sem traducao de fim de linha). O caminho e UTF-8, terminado em NUL. Os dois "abrir" devolvem o identificador (nunca 0) ou 0 com um
 * dos codigos abaixo em *erro. Tamanhos e posicoes em int64_t. Quem chama passa lotes de no maximo 1 GiB (forja_arquivo_* em rt_arquivo.c). */
#define SYS_ARQ_NAO_EXISTE 1
#define SYS_ARQ_SEM_PERMISSAO 2
#define SYS_ARQ_OUTRO 3

/* Abre arquivo existente so para ler / cria (ou zera) para gravar. */
uint64_t sys_arquivo_abrir_ler(const char *caminho, int32_t *erro);
uint64_t sys_arquivo_criar(const char *caminho, int32_t *erro);
/* Tamanho em bytes; negativo = nao sei (dispositivo, falha). */
int64_t sys_arquivo_tamanho(uint64_t arquivo);
/* Le ate n bytes: devolve quantos leu (0 = fim do arquivo) ou -1 em falha. */
int64_t sys_arquivo_ler(uint64_t arquivo, void *destino, int64_t n);
/* Grava ate n bytes: devolve quantos gravou (n, salvo falha) ou -1 em falha. */
int64_t sys_arquivo_gravar(uint64_t arquivo, const void *origem, int64_t n);
/* Fecha; 0 = ok, -1 = falha. */
int32_t sys_arquivo_fechar(uint64_t arquivo);

/* Liga o tratador de excecoes do processador (AddVectoredExceptionHandler, antes de qualquer tratador do programa). O tratador recebe o codigo da excecao (o do
 * EXCEPTION_RECORD) e devolve 0 para seguir a busca (o processo cai como antes); se tratar a excecao, salta e nao volta. */
void sys_excecao_ligar(int (*tratador)(unsigned long codigo));

/* M6e P93e (D-M6e-8): a falha de memoria como o tratador a ve. codigo e o do EXCEPTION_RECORD (0xC0000005 = acesso invalido), endereco o que a instrucao tentou tocar, rip/rsp/rbp os
 * registradores do CONTEXTO da thread que falhou (nunca os do tratador). O pre-tratador devolve 0 se a falha nao e dele; se e, relata e sai (nao volta). Roda ANTES do tratador do
 * sys_excecao_ligar. Windows: offsets do censo c5 (EXCEPTION_RECORD +40, CONTEXT Rip +248, Rsp +152, Rbp +160). Linux (P93f): si_addr +16, ucontext gregs +40 (RIP 16, RSP 15, RBP 10). */
typedef struct {
    unsigned long codigo;
    uint64_t endereco, rip, rsp, rbp;
} SysFalha;
void sys_falha_ligar(int (*pre)(const SysFalha *f));

/* Reserva n bytes de enderecos SEM commit (qualquer acesso falha); NULL se nao ha. commit torna [p, p+n) leitura/escrita (false = sem memoria); decommit o devolve ao sistema e
 * o endereco continua reservado (acesso depois disso falha). Alocador de guarda (rt_guarda.c, so em dev). */
void *sys_guarda_reservar(int64_t n);
bool sys_guarda_commit(void *p, int64_t n);
void sys_guarda_decommit(void *p, int64_t n);
/* true quando um commit falhou por limite de mapeamentos do kernel (Linux: ENOMEM do mprotect, vm.max_map_count); Windows: sempre false. */
bool sys_guarda_limite_mapas(void);

/* Relogio monotonico em nanossegundos desde um ponto fixo do sistema. */
int64_t sys_tempo_ns(void);

#ifdef RT_ALVO_WIN
/* Slot de armazenamento por thread (TlsAlloc/TlsGetValue/TlsSetValue), o do RtFio (rt_fio.h). Alocar devolve SYS_TLS_SEM_INDICE se o processo nao tem indice livre;
 * gravar devolve false em falha. Ler com indice < 64 e feito direto no TEB por rt_fio.h (TlsGetValue zera o GetLastError, que o std:file e o std:os leem). */
#define SYS_TLS_SEM_INDICE 0xFFFFFFFFu
uint32_t sys_tls_alocar(void);
void *sys_tls_ler(uint32_t indice);
bool sys_tls_gravar(uint32_t indice, void *valor);
#endif

/* Trava do estado compartilhado entre threads (SRW exclusivo). Uso: bool tomou = sys_trava_pegar(&t); ...; sys_trava_soltar(&t, tomou); nao e recursiva. */
typedef struct { void *srw; } SysTrava;

#if defined(RT_ALVO_WIN) || defined(RT_ALVO_LINUX)
extern volatile uint8_t rt_multithread; /* compartilhado: flag pegajoso (0 -> 1), ligado por sys_multithread_ligar */
void sys_multithread_ligar(void);
void sys_trava_pegar_srw(SysTrava *t);
void sys_trava_soltar_srw(SysTrava *t);

static inline bool sys_trava_pegar(SysTrava *t)
{
    if (!rt_multithread)
        return false;
    sys_trava_pegar_srw(t);
#ifdef RT_ALVO_LINUX
    RT_BARREIRA(); /* P63: o gcc nao pode usar leitura do estado travado feita antes da trava */
#endif
    return true;
}

static inline void sys_trava_soltar(SysTrava *t, bool tomou)
{
#ifdef RT_ALVO_LINUX
    RT_BARREIRA(); /* P63: as escritas do estado travado saem antes da trava soltar */
#endif
    if (tomou)
        sys_trava_soltar_srw(t);
}
#else
static inline bool sys_trava_pegar(SysTrava *t)
{
    (void)t;
    return false;
}

static inline void sys_trava_soltar(SysTrava *t, bool tomou)
{
    (void)t;
    (void)tomou;
}
#endif

#endif
