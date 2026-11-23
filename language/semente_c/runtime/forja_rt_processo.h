/*
 * forja_rt_processo.h - processo filho do std:process no Windows, nos dois backends (POS-Z A51, parecer do Opus 14/11/2026). Texto ASCII puro.
 *
 * Versao: 0.4.1
 * Changelog:
 *   0.4.1 - D-G3-L2a (bloco D): falha do spawn regrava o GetLastError do CreateProcessW por SetLastError depois do cleanup (std:os.last_error); 1 import novo no fim do ligar_tabelas.c (SetLastError, o mesmo do forja_rt_es.h).
 *   0.4.0 - D-G3-L2a (bloco C): forja_process_spawn (8 parametros) escolhe o canal de cada stdio (null, pipe, inherit, file); forja_processo_iniciar vira envoltorio com o default de sempre (ABI intacta). inherit = DuplicateHandle do
 *           handle padrao do pai; file = CreateFileW CREATE_ALWAYS com SHARE_READ, herdavel; null = "NUL"; a HANDLE_LIST leva os 3 (distintos). forja_processo_ler devolve -1 em canal que nao e pipe. 1 import novo no fim do ligar_tabelas.c (DuplicateHandle).
 *   0.3.0 - D-G3-L2a (bloco B): forja_process_pid, forja_process_wait_any (WaitForMultipleObjects, ate 64 filhos), forja_process_peak_memory (QueryInformationJobObject: PeakJobMemoryUsed do Job) e forja_process_cpu_times (GetProcessTimes);
 *           FORJA_PRC_MAX sobe de 8 para 64 (o pid fica no slot); 3 imports novos no fim do ligar_tabelas.c (WaitForMultipleObjects, QueryInformationJobObject, GetProcessTimes). Nenhum externo existente muda.
 *   0.2.1 - M5 P44: forja_prc_tabela sob forja_prc_trava (FORJA_ES_PEGAR/FORJA_ES_SOLTAR, as mesmas de forja_rt_es.h): so forja_prc_reservar e forja_prc_liberar_slot travam. Backend C: nulo.
 *   0.2.0 - M5 P28b-2b: forja_processo_iniciar ganha o 5o parametro limite_memoria (u64; 0 = sem limite). Windows: o Job leva JOB_OBJECT_LIMIT_PROCESS_MEMORY com ProcessMemoryLimit = limite (teto de memoria COMMIT de cada processo do job);
 *           o filho que estoura recebe alocacao negada (o codigo de saida e o do proprio programa dele), o pai nao e afetado. O matar continua TerminateJobObject (a arvore inteira), o que ja era o comportamento.
 *   0.1.2 - M5 P28b-2: so comentario: o runtime Linux define as forja_processo_* em runtime\nativo\rt_processo_linux.c (mesmo contrato); aqui, com RT_ALVO_LINUX, continuam so as constantes.
 *   0.1.1 - M5 P28b-1: guarda RT_ALVO_LINUX (so as constantes no runtime Linux). O Windows e o backend C nao mudam.
 *   0.1.0 - POS-Z A51: forja_processo_iniciar, _ler, _esperar, _codigo, _matar e _fechar. Filho suspenso, preso a um Job (KILL_ON_JOB_CLOSE) e solto; so 3 handles
 *           herdados (PROC_THREAD_ATTRIBUTE_HANDLE_LIST): stdin = NUL, stdout e stderr = pipes lidos sem bloquear (PeekNamedPipe).
 *
 * Contrato:
 *   forja_processo_iniciar(linha, ambiente, pasta, ^erro, limite_memoria) -> u64 (limite_memoria em bytes, 0 = sem limite): 0 = falha, com *erro = 1 nao achou o programa, 2 sem permissao, 3 outro, 4 entrada invalida.
 *     linha = linha de comando inteira em UTF-8, ja com aspas (a 1a palavra e o programa; sem caminho o sistema procura no PATH e acrescenta .exe).
 *     ambiente = variaveis "NOME=valor" separadas por LF em UTF-8, que SUBSTITUEM o ambiente do pai; "" = herda. pasta = diretorio de trabalho; "" = o do pai.
 *   forja_process_spawn(linha, ambiente, pasta, ^erro, limite_memoria, canais, saida, saida_erro) -> u64: como forja_processo_iniciar, com canais = stdin | stdout << 4 | stderr << 8 (0 null, 1 pipe, 2 inherit, 3 file);
 *     saida e saida_erro = caminho UTF-8 do arquivo do stdout e do stderr (so lidos nos canais em file). *erro = 4 (entrada) para stdin pipe ou file, file sem caminho ou canal fora de 0..3.
 *   forja_processo_ler(h, canal, destino, n) -> i64: canal 0 = stdout, 1 = stderr. > 0 = bytes lidos; 0 = nada disponivel agora; -1 = fim (o filho e seus
 *     herdeiros fecharam o canal e nao ha mais bytes) ou canal que nao e pipe; -2 = erro ou handle invalido. Nunca bloqueia.
 *   forja_processo_esperar(h, ms) -> i32: 1 = terminou; 0 = ainda roda (passou 'ms', 0 = so consulta); -1 = handle invalido.
 *   forja_processo_codigo(h) -> i64: codigo de saida do filho (so vale depois de esperar devolver 1); -1 = handle invalido.
 *   forja_processo_matar(h) -> i32: termina o filho e todos os que ele criou (o Job inteiro); 1 = feito, 0 = falhou.
 *   forja_processo_fechar(h): fecha os canais, o Job (leva o que sobrou junto) e o processo; o handle deixa de valer. Fechar duas vezes e inofensivo.
 *   forja_process_pid(h) -> i64: pid do filho (guardado no slot quando ele nasce); -1 = handle invalido.
 *   forja_process_wait_any(ids, n, ms) -> i64: espera o PRIMEIRO dos n filhos (1..64) terminar; >= 0 = indice em ids; -1 = o prazo passou (ms < 0 = sem prazo); -2 = lista vazia, > 64 ou handle fechado; -3 = outro.
 *   forja_process_peak_memory(h) -> i64: >= 0 = pico de memoria COMMIT do Job (o filho e quem ele criou) em bytes; < 0 = -codigo neutro (4 handle invalido, 3 outro).
 *   forja_process_cpu_times(h, saida) -> i32: 0 = feito, saida[0..3] = usuario, sistema, inicio, fim (ns; inicio e fim desde 1970; fim = 0 enquanto o filho roda); senao o codigo neutro (4 handle invalido, 3 outro).
 * Handle: u64 opaco = (geracao << 32) | (posicao + 1) numa tabela estatica de FORJA_PRC_MAX posicoes; a geracao sobe a cada fechamento. Limite: 64 filhos
 * abertos ao mesmo tempo. Sem trava: o runtime roda numa thread so.
 *
 * Nativo: defina FORJA_ES_NATIVO antes do include (sem libc; rt_sys.h antes, para sys_heap_*). As funcoes saem externas (rt_es_win.c).
 * C: funcoes static inline; entram no .c gerado so quando o programa importa std:process. Fora do Windows as funcoes devolvem falha (iniciar = 0, *erro = 3).
 * Imports de kernel32: __declspec(dllimport) + rotulo asm, como em forja_rt_es.h.
 */
#ifndef FORJA_RT_PROCESSO_H
#define FORJA_RT_PROCESSO_H

#include <stdint.h>

#if defined(FORJA_ES_NATIVO)
#define FORJA_ES_API
#ifndef FORJA_ES_ALOCAR
#define FORJA_ES_ALOCAR(n) sys_heap_alocar(n)
#endif
#ifndef FORJA_ES_LIBERAR
#define FORJA_ES_LIBERAR(p) sys_heap_liberar(p)
#endif
#ifndef FORJA_ES_TRAVA_T
#define FORJA_ES_TRAVA_T SysTrava
#define FORJA_ES_PEGAR(t) sys_trava_pegar(&(t))
#define FORJA_ES_SOLTAR(t, tomou) sys_trava_soltar(&(t), (tomou))
#endif
#else
#include <stdlib.h>
#define FORJA_ES_API static inline
#ifndef FORJA_ES_ALOCAR
#define FORJA_ES_ALOCAR(n) malloc(n)
#endif
#ifndef FORJA_ES_LIBERAR
#define FORJA_ES_LIBERAR(p) free(p)
#endif
#ifndef FORJA_ES_TRAVA_T
#define FORJA_ES_TRAVA_T char
#define FORJA_ES_PEGAR(t) ((void)(t), 0)
#define FORJA_ES_SOLTAR(t, tomou) ((void)(t), (void)(tomou))
#endif
#endif

#define FORJA_PRC_ERR_NAO_ACHOU 1
#define FORJA_PRC_ERR_SEM_PERMISSAO 2
#define FORJA_PRC_ERR_OUTRO 3
#define FORJA_PRC_ERR_ENTRADA 4
#define FORJA_PRC_MAX 64 /* tambem o teto de WaitForMultipleObjects (MAXIMUM_WAIT_OBJECTS) */
#define FORJA_PRC_CANAL_NULO 0       /* Stdio.null */
#define FORJA_PRC_CANAL_PIPE 1       /* Stdio.pipe */
#define FORJA_PRC_CANAL_HERDA 2      /* Stdio.inherit */
#define FORJA_PRC_CANAL_ARQUIVO 3    /* Stdio.file */
#define FORJA_PRC_CANAIS_PADRAO 0x110 /* stdin nulo, stdout e stderr em pipe: o que o forja_processo_iniciar sempre fez */

/* Sem a particao quente/fria do gcc (ver o 0.1.1 de forja_rt_es.h: o .pdata.unlikely fora de ordem faz o ligar_pe recusar o x64). */
#pragma GCC push_options
#pragma GCC optimize("no-reorder-blocks-and-partition")

/* P28b-1: com -DRT_ALVO_LINUX o gcc MinGW tambem define _WIN32; ali so valem as constantes acima (o processo do Linux vive em rt_processo_linux.c). */
#if defined(_WIN32) && !defined(RT_ALVO_LINUX)
#ifndef FORJA_ES_ROTULO
#define FORJA_ES_ROTULO_TXT_(x) #x
#define FORJA_ES_ROTULO_XTXT_(x) FORJA_ES_ROTULO_TXT_(x)
#define FORJA_ES_ROTULO(nome) __asm__(FORJA_ES_ROTULO_XTXT_(__USER_LABEL_PREFIX__) nome)
#endif

#define FORJA_PRC_INVALIDO ((void *)(intptr_t)-1) /* INVALID_HANDLE_VALUE */
#define FORJA_PRC_UTF8 65001u
#define FORJA_PRC_PIPE_TAM 65536UL                /* buffer de cada pipe: o filho so bloqueia depois de 64 KiB sem leitura */
#define FORJA_PRC_MB_ERR_INVALIDO 0x8UL           /* MB_ERR_INVALID_CHARS */
#define FORJA_PRC_GENERIC_READ 0x80000000UL
#define FORJA_PRC_GENERIC_WRITE 0x40000000UL
#define FORJA_PRC_COMPARTILHA 0x3UL               /* FILE_SHARE_READ | FILE_SHARE_WRITE */
#define FORJA_PRC_COMPARTILHA_LEITURA 0x1UL       /* FILE_SHARE_READ: o arquivo do Stdio.file pode ser lido enquanto o filho escreve */
#define FORJA_PRC_CRIAR_SEMPRE 2UL                /* CREATE_ALWAYS */
#define FORJA_PRC_ATRIBUTO_NORMAL 0x80UL          /* FILE_ATTRIBUTE_NORMAL */
#define FORJA_PRC_DUP_MESMO_ACESSO 2UL            /* DUPLICATE_SAME_ACCESS */
#define FORJA_PRC_NIVEL_STDIN 0xFFFFFFF6UL        /* STD_INPUT_HANDLE (-10); stdout = -11 e stderr = -12: nivel - indice do canal */
#define FORJA_PRC_PSEUDO_PROCESSO ((void *)(intptr_t)-1) /* o GetCurrentProcess() */
#define FORJA_PRC_ABRIR_EXISTENTE 3UL             /* OPEN_EXISTING */
#define FORJA_PRC_HERDAR_FLAG 0x1UL               /* HANDLE_FLAG_INHERIT */
#define FORJA_PRC_CRIAR_SUSPENSO 0x4UL
#define FORJA_PRC_CRIAR_AMBIENTE_LARGO 0x400UL    /* CREATE_UNICODE_ENVIRONMENT */
#define FORJA_PRC_CRIAR_STARTUPINFOEX 0x80000UL   /* EXTENDED_STARTUPINFO_PRESENT */
#define FORJA_PRC_CRIAR_SEM_JANELA 0x08000000UL   /* CREATE_NO_WINDOW */
#define FORJA_PRC_USAR_HANDLES 0x100UL            /* STARTF_USESTDHANDLES */
#define FORJA_PRC_ATRIBUTO_HANDLES 0x20002UL      /* PROC_THREAD_ATTRIBUTE_HANDLE_LIST */
#define FORJA_PRC_JOB_ESTENDIDO 9UL               /* JobObjectExtendedLimitInformation */
#define FORJA_PRC_JOB_MATAR_AO_FECHAR 0x2000UL    /* JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE */
#define FORJA_PRC_JOB_LIMITE_PROCESSO 0x100UL     /* JOB_OBJECT_LIMIT_PROCESS_MEMORY */
#define FORJA_PRC_JOB_INDICE_LIMITE_PROCESSO 11   /* ProcessMemoryLimit fica no deslocamento 112 = 24 + 11 * 8 (resto[] comeca no 24) */
#define FORJA_PRC_JOB_INDICE_PICO 14              /* PeakJobMemoryUsed fica no deslocamento 136 = 24 + 14 * 8 */
#define FORJA_PRC_ESPERA_TERMINOU 0UL
#define FORJA_PRC_ESPERA_TEMPO 0x102UL
#define FORJA_PRC_SEM_PRAZO 0xFFFFFFFFUL          /* INFINITE */
#define FORJA_PRC_PRAZO_MAXIMO 0xFFFFFFFEUL
#define FORJA_PRC_FILETIME_1970 116444736000000000ULL /* 100 ns de 1601 a 1970 */
#define FORJA_PRC_NS_POR_TICK 100LL
#define FORJA_PRC_ERRO_CANO_FECHADO 109UL         /* ERROR_BROKEN_PIPE */

typedef struct {
    unsigned long tam;
    void *seguranca;
    int herdavel;
} ForjaPrcAtributos;

/* STARTUPINFOEXW (112 bytes no x64): os campos usados tem nome, o resto e reservado no mesmo lugar. */
typedef struct {
    unsigned long cb;
    unsigned short *reservado1;
    unsigned short *area;
    unsigned short *titulo;
    unsigned long posicao_tamanho[7];
    unsigned long flags;
    unsigned short mostrar;
    unsigned short reservado2_tam;
    unsigned char *reservado2;
    void *entrada;
    void *saida;
    void *erro;
    void *lista;
} ForjaPrcInicio;

typedef struct {
    void *processo;
    void *thread;
    unsigned long pid;
    unsigned long tid;
} ForjaPrcInfo;

/* JOBOBJECT_EXTENDED_LIMIT_INFORMATION (144 bytes no x64): LimitFlags fica no deslocamento 16. */
typedef struct {
    int64_t limites[2];
    uint32_t flags;
    uint32_t reservado;
    int64_t resto[15];
} ForjaPrcJob;

typedef struct {
    void *processo;
    void *job;
    void *canal[2]; /* leitura de stdout e de stderr */
    uint32_t geracao;
    int32_t usado;
    int64_t pid;
} ForjaPrcSlot;

static ForjaPrcSlot forja_prc_tabela[FORJA_PRC_MAX]; /* compartilhado: reservar e liberar_slot sob forja_prc_trava */
static FORJA_ES_TRAVA_T forja_prc_trava; /* compartilhado: so trava com rt_multithread ligado */

/* kernel32 sem windows.h (colidiria com nomes do programa). */
__declspec(dllimport) void *forja_prc_k32_CreateFileW(const unsigned short *nome, unsigned long acesso, unsigned long compartilha, void *seguranca,
                                                      unsigned long criacao, unsigned long atributos, void *modelo) FORJA_ES_ROTULO("CreateFileW");
__declspec(dllimport) int forja_prc_k32_ReadFile(void *h, void *buf, unsigned long n, unsigned long *lidos, void *sobreposto) FORJA_ES_ROTULO("ReadFile");
__declspec(dllimport) int forja_prc_k32_CloseHandle(void *h) FORJA_ES_ROTULO("CloseHandle");
__declspec(dllimport) unsigned long forja_prc_k32_GetLastError(void) FORJA_ES_ROTULO("GetLastError");
__declspec(dllimport) void forja_prc_k32_SetLastError(unsigned long codigo) FORJA_ES_ROTULO("SetLastError");
__declspec(dllimport) void *forja_prc_k32_GetStdHandle(unsigned long nivel) FORJA_ES_ROTULO("GetStdHandle");
/* LIMITE: params: assinatura do DuplicateHandle (7 argumentos) */
__declspec(dllimport) int forja_prc_k32_DuplicateHandle(void *origem_processo, void *origem, void *destino_processo, void **destino, unsigned long acesso,
                                                        int herdavel, unsigned long opcoes) FORJA_ES_ROTULO("DuplicateHandle");
__declspec(dllimport) int forja_prc_k32_MultiByteToWideChar(unsigned int pagina, unsigned long flags, const char *origem, int norigem,
                                                            unsigned short *destino, int ndestino) FORJA_ES_ROTULO("MultiByteToWideChar");
__declspec(dllimport) int forja_prc_k32_CreatePipe(void **leitura, void **escrita, ForjaPrcAtributos *atributos, unsigned long tam) FORJA_ES_ROTULO("CreatePipe");
__declspec(dllimport) int forja_prc_k32_SetHandleInformation(void *h, unsigned long mascara, unsigned long flags) FORJA_ES_ROTULO("SetHandleInformation");
/* LIMITE: params: assinatura do CreateProcessW (10 argumentos) */
__declspec(dllimport) int forja_prc_k32_CreateProcessW(const unsigned short *aplicativo, unsigned short *linha, void *seg_processo, void *seg_thread,
                                                       int herdar, unsigned long flags, void *ambiente, const unsigned short *pasta, void *inicio,
                                                       ForjaPrcInfo *info) FORJA_ES_ROTULO("CreateProcessW");
__declspec(dllimport) int forja_prc_k32_InitializeProcThreadAttributeList(void *lista, unsigned long quantos, unsigned long flags, uint64_t *tam)
    FORJA_ES_ROTULO("InitializeProcThreadAttributeList");
/* LIMITE: params: assinatura do UpdateProcThreadAttribute (7 argumentos) */
__declspec(dllimport) int forja_prc_k32_UpdateProcThreadAttribute(void *lista, unsigned long flags, uint64_t atributo, void *valor, uint64_t tam,
                                                                  void *anterior, uint64_t *retorno) FORJA_ES_ROTULO("UpdateProcThreadAttribute");
__declspec(dllimport) void forja_prc_k32_DeleteProcThreadAttributeList(void *lista) FORJA_ES_ROTULO("DeleteProcThreadAttributeList");
__declspec(dllimport) void *forja_prc_k32_CreateJobObjectW(void *seguranca, const unsigned short *nome) FORJA_ES_ROTULO("CreateJobObjectW");
__declspec(dllimport) int forja_prc_k32_SetInformationJobObject(void *job, unsigned long classe, void *info, unsigned long tam)
    FORJA_ES_ROTULO("SetInformationJobObject");
__declspec(dllimport) int forja_prc_k32_AssignProcessToJobObject(void *job, void *processo) FORJA_ES_ROTULO("AssignProcessToJobObject");
__declspec(dllimport) unsigned long forja_prc_k32_ResumeThread(void *thread) FORJA_ES_ROTULO("ResumeThread");
__declspec(dllimport) int forja_prc_k32_TerminateJobObject(void *job, unsigned int codigo) FORJA_ES_ROTULO("TerminateJobObject");
__declspec(dllimport) int forja_prc_k32_TerminateProcess(void *processo, unsigned int codigo) FORJA_ES_ROTULO("TerminateProcess");
__declspec(dllimport) unsigned long forja_prc_k32_WaitForSingleObject(void *h, unsigned long ms) FORJA_ES_ROTULO("WaitForSingleObject");
__declspec(dllimport) int forja_prc_k32_GetExitCodeProcess(void *processo, unsigned long *codigo) FORJA_ES_ROTULO("GetExitCodeProcess");
/* LIMITE: params: assinatura do PeekNamedPipe (6 argumentos) */
__declspec(dllimport) int forja_prc_k32_PeekNamedPipe(void *cano, void *buf, unsigned long n, unsigned long *lidos, unsigned long *disponivel,
                                                      unsigned long *restante) FORJA_ES_ROTULO("PeekNamedPipe");
__declspec(dllimport) unsigned long forja_prc_k32_WaitForMultipleObjects(unsigned long n, void *const *handles, int todos, unsigned long ms)
    FORJA_ES_ROTULO("WaitForMultipleObjects");
__declspec(dllimport) int forja_prc_k32_QueryInformationJobObject(void *job, unsigned long classe, void *info, unsigned long tam, unsigned long *retorno)
    FORJA_ES_ROTULO("QueryInformationJobObject");
__declspec(dllimport) int forja_prc_k32_GetProcessTimes(void *processo, uint64_t *criacao, uint64_t *saida, uint64_t *nucleo, uint64_t *usuario)
    FORJA_ES_ROTULO("GetProcessTimes");

/* UTF-8 -> UTF-16 com NUL, num bloco do alocador com 'extra' unidades de folga no fim (quem chama libera). 0 com *erro em falha; texto invalido = entrada. */
static inline unsigned short *forja_prc_larga(const char *texto, int32_t extra, int32_t *erro)
{
    int n = forja_prc_k32_MultiByteToWideChar(FORJA_PRC_UTF8, FORJA_PRC_MB_ERR_INVALIDO, texto, -1, 0, 0);
    unsigned short *larga;

    if (n <= 0) {
        *erro = FORJA_PRC_ERR_ENTRADA;
        return 0;
    }
    larga = FORJA_ES_ALOCAR(((int64_t)n + extra) * 2);
    if (larga == 0) {
        *erro = FORJA_PRC_ERR_OUTRO;
        return 0;
    }
    forja_prc_k32_MultiByteToWideChar(FORJA_PRC_UTF8, FORJA_PRC_MB_ERR_INVALIDO, texto, -1, larga, n);
    for (int32_t k = 0; k < extra; k++)
        larga[n + k] = 0;
    return larga;
}

/* Bloco de ambiente "A=1\0B=2\0\0": as variaveis vem separadas por LF e o LF vira NUL. */
static inline unsigned short *forja_prc_ambiente_larga(const char *ambiente, int32_t *erro)
{
    unsigned short *bloco = forja_prc_larga(ambiente, 2, erro);

    for (int64_t k = 0; bloco != 0 && bloco[k] != 0; k++) {
        if (bloco[k] == 10)
            bloco[k] = 0;
    }
    return bloco;
}

static inline int32_t forja_prc_erro_win(unsigned long codigo)
{
    switch (codigo) {
    case 2UL:
    case 3UL:
        return FORJA_PRC_ERR_NAO_ACHOU;
    case 5UL:
        return FORJA_PRC_ERR_SEM_PERMISSAO;
    case 87UL:
    case 123UL:
    case 206UL:
        return FORJA_PRC_ERR_ENTRADA;
    default:
        return FORJA_PRC_ERR_OUTRO;
    }
}

static inline void forja_prc_fechar_handle(void **h)
{
    if (*h != 0 && *h != FORJA_PRC_INVALIDO)
        forja_prc_k32_CloseHandle(*h);
    *h = 0;
}

/* Posicao da tabela para o handle opaco, ou -1 (fechado, de outra geracao ou fora da faixa). */
static inline int32_t forja_prc_posicao(uint64_t h)
{
    uint64_t pos = (h & 0xFFFFFFFFULL);

    if (pos == 0 || pos > FORJA_PRC_MAX)
        return -1;
    if (!forja_prc_tabela[pos - 1].usado || forja_prc_tabela[pos - 1].geracao != (uint32_t)(h >> 32))
        return -1;
    return (int32_t)(pos - 1);
}

static inline void forja_prc_liberar_slot(ForjaPrcSlot *s)
{
    forja_prc_fechar_handle(&s->canal[0]);
    forja_prc_fechar_handle(&s->canal[1]);
    forja_prc_fechar_handle(&s->job);
    forja_prc_fechar_handle(&s->processo);
    {
        int tomou = FORJA_ES_PEGAR(forja_prc_trava);

        s->usado = 0;
        s->geracao++;
        FORJA_ES_SOLTAR(forja_prc_trava, tomou);
    }
}

/* Job que mata tudo o que estiver nele quando fechar (e no matar). limite > 0 = teto de memoria COMMIT por processo do job (ProcessMemoryLimit). 0 = nao deu. */
static inline void *forja_prc_criar_job(uint64_t limite)
{
    ForjaPrcJob info = {{0, 0}, FORJA_PRC_JOB_MATAR_AO_FECHAR, 0, {0}};
    void *job = forja_prc_k32_CreateJobObjectW(0, 0);

    if (job == 0)
        return 0;
    if (limite > 0) {
        info.flags |= FORJA_PRC_JOB_LIMITE_PROCESSO;
        info.resto[FORJA_PRC_JOB_INDICE_LIMITE_PROCESSO] = (int64_t)limite;
    }
    if (forja_prc_k32_SetInformationJobObject(job, FORJA_PRC_JOB_ESTENDIDO, &info, (unsigned long)sizeof info))
        return job;
    forja_prc_k32_CloseHandle(job);
    return 0;
}

/* Cria o pipe do filho para o pai: a ponta de leitura fica so com o pai, a de escrita (herdavel) vai ao filho. */
static inline int forja_prc_criar_canal(void **leitura, void **escrita)
{
    ForjaPrcAtributos sa = {(unsigned long)sizeof(ForjaPrcAtributos), 0, 1};

    if (!forja_prc_k32_CreatePipe(leitura, escrita, &sa, FORJA_PRC_PIPE_TAM))
        return 0;
    forja_prc_k32_SetHandleInformation(*leitura, FORJA_PRC_HERDAR_FLAG, 0);
    return 1;
}

/* Lista de atributos com HANDLE_LIST: so os 3 handles dados passam para o filho. 0 = nao deu. */
static inline void *forja_prc_lista_handles(void **h)
{
    uint64_t tam = 0;
    void *lista;

    forja_prc_k32_InitializeProcThreadAttributeList(0, 1, 0, &tam);
    lista = tam > 0 ? FORJA_ES_ALOCAR((int64_t)tam) : 0;
    if (lista == 0)
        return 0;
    if (forja_prc_k32_InitializeProcThreadAttributeList(lista, 1, 0, &tam)) {
        if (forja_prc_k32_UpdateProcThreadAttribute(lista, 0, FORJA_PRC_ATRIBUTO_HANDLES, h, 3 * sizeof(void *), 0, 0))
            return lista;
        forja_prc_k32_DeleteProcThreadAttributeList(lista);
    }
    FORJA_ES_LIBERAR(lista);
    return 0;
}

/* CreateProcessW suspenso com stdin/stdout/stderr em h[0..2]. 0 = falhou (GetLastError diz por que). */
static inline int forja_prc_criar_filho(unsigned short *linha, void *ambiente, const unsigned short *pasta, void **h, ForjaPrcInfo *info)
{
    ForjaPrcInicio si = {(unsigned long)sizeof(ForjaPrcInicio), 0, 0, 0, {0}, FORJA_PRC_USAR_HANDLES, 0, 0, 0, h[0], h[1], h[2], 0};
    unsigned long flags = FORJA_PRC_CRIAR_SUSPENSO | FORJA_PRC_CRIAR_STARTUPINFOEX | FORJA_PRC_CRIAR_SEM_JANELA | (ambiente != 0 ? FORJA_PRC_CRIAR_AMBIENTE_LARGO : 0);
    int ok;

    si.lista = forja_prc_lista_handles(h);
    if (si.lista == 0)
        return 0;
    ok = forja_prc_k32_CreateProcessW(0, linha, 0, 0, 1, flags, ambiente, pasta, &si, info);
    forja_prc_k32_DeleteProcThreadAttributeList(si.lista);
    FORJA_ES_LIBERAR(si.lista);
    return ok;
}

/* Prende o filho suspenso ao job e o solta. false = o filho foi morto. */
static inline int forja_prc_soltar(ForjaPrcSlot *s, ForjaPrcInfo *info)
{
    int ok = forja_prc_k32_AssignProcessToJobObject(s->job, info->processo) && forja_prc_k32_ResumeThread(info->thread) != 0xFFFFFFFFUL;

    if (!ok)
        forja_prc_k32_TerminateProcess(info->processo, 1);
    forja_prc_fechar_handle(&info->thread);
    return ok;
}

/* Abre o slot livre e devolve a posicao, ou -1. */
static inline int32_t forja_prc_reservar(void)
{
    int tomou = FORJA_ES_PEGAR(forja_prc_trava);

    for (int32_t k = 0; k < FORJA_PRC_MAX; k++) {
        if (!forja_prc_tabela[k].usado) {
            forja_prc_tabela[k].usado = 1;
            FORJA_ES_SOLTAR(forja_prc_trava, tomou);
            return k;
        }
    }
    FORJA_ES_SOLTAR(forja_prc_trava, tomou);
    return -1;
}

/* O que o spawn pede, ja em UTF-16: textos, limite de memoria do job, canais (stdin | stdout << 4 | stderr << 8, cada um FORJA_PRC_CANAL_*) e o arquivo do stdout e do stderr. */
typedef struct {
    unsigned short *linha;
    void *ambiente;
    const unsigned short *pasta;
    uint64_t limite;
    int32_t canais;
    const unsigned short *caminho[2];
} ForjaPrcPedido;

/* "NUL" aberto para o filho (herdavel): leitura no stdin, escrita nos outros. */
static inline void *forja_prc_abrir_nul(int leitura)
{
    ForjaPrcAtributos sa = {(unsigned long)sizeof(ForjaPrcAtributos), 0, 1};
    static const unsigned short nul[4] = {'N', 'U', 'L', 0};

    return forja_prc_k32_CreateFileW(nul, leitura ? FORJA_PRC_GENERIC_READ : FORJA_PRC_GENERIC_WRITE, FORJA_PRC_COMPARTILHA, &sa, FORJA_PRC_ABRIR_EXISTENTE, 0, 0);
}

/* Stdio.inherit: copia herdavel do handle padrao do pai (nivel - indice); pai sem handle padrao (GUI, fechado) vira NUL. */
static inline void *forja_prc_herdar(int32_t indice)
{
    void *origem = forja_prc_k32_GetStdHandle(FORJA_PRC_NIVEL_STDIN - (unsigned long)indice);
    void *copia = FORJA_PRC_INVALIDO;

    if (origem == 0 || origem == FORJA_PRC_INVALIDO)
        return forja_prc_abrir_nul(indice == 0);
    if (!forja_prc_k32_DuplicateHandle(FORJA_PRC_PSEUDO_PROCESSO, origem, FORJA_PRC_PSEUDO_PROCESSO, &copia, 0, 1, FORJA_PRC_DUP_MESMO_ACESSO))
        return FORJA_PRC_INVALIDO;
    return copia;
}

/* Stdio.file: arquivo criado (ou zerado) para o filho escrever, herdavel, que o pai ainda pode ler. */
static inline void *forja_prc_abrir_arquivo(const unsigned short *caminho)
{
    ForjaPrcAtributos sa = {(unsigned long)sizeof(ForjaPrcAtributos), 0, 1};

    return forja_prc_k32_CreateFileW(caminho, FORJA_PRC_GENERIC_WRITE, FORJA_PRC_COMPARTILHA_LEITURA, &sa, FORJA_PRC_CRIAR_SEMPRE, FORJA_PRC_ATRIBUTO_NORMAL, 0);
}

/* Abre a ponta do FILHO do canal 'indice' (0 stdin, 1 stdout, 2 stderr) em *h; o pipe deixa a ponta de leitura do pai em s->canal[indice - 1]. 0 = nao deu (GetLastError diz por que). */
static inline int forja_prc_abrir_canal(ForjaPrcSlot *s, int32_t indice, const ForjaPrcPedido *pedido, void **h)
{
    int32_t modo = (pedido->canais >> (4 * indice)) & 0xF;

    if (modo == FORJA_PRC_CANAL_PIPE)
        return forja_prc_criar_canal(&s->canal[indice - 1], h);
    if (modo == FORJA_PRC_CANAL_HERDA)
        *h = forja_prc_herdar(indice);
    else if (modo == FORJA_PRC_CANAL_ARQUIVO)
        *h = forja_prc_abrir_arquivo(pedido->caminho[indice - 1]);
    else
        *h = forja_prc_abrir_nul(indice == 0);
    return *h != FORJA_PRC_INVALIDO;
}

/* Com o pedido ja em UTF-16, monta o slot: job, canais, filho solto. Devolve o handle opaco ou 0 (com *erro). */
static inline uint64_t forja_prc_disparar(const ForjaPrcPedido *pedido, int32_t *erro)
{
    int32_t pos = forja_prc_reservar();
    ForjaPrcSlot *s = &forja_prc_tabela[pos < 0 ? 0 : pos];
    void *h[3] = {0, 0, 0};
    ForjaPrcInfo info = {0, 0, 0, 0};
    unsigned long ultimo;
    int ok;

    if (pos < 0) {
        *erro = FORJA_PRC_ERR_OUTRO;
        return 0;
    }
    s->job = forja_prc_criar_job(pedido->limite);
    ok = s->job != 0;
    for (int32_t k = 0; ok && k < 3; k++)
        ok = forja_prc_abrir_canal(s, k, pedido, &h[k]);
    ok = ok && forja_prc_criar_filho(pedido->linha, pedido->ambiente, pedido->pasta, h, &info);
    ultimo = ok ? 0 : forja_prc_k32_GetLastError();
    *erro = ok ? 0 : forja_prc_erro_win(ultimo);
    for (int k = 0; k < 3; k++)
        forja_prc_fechar_handle(&h[k]); /* so o filho fica com as pontas dele: o fim dos pipes depende disso */
    if (ok)
        ok = forja_prc_soltar(s, &info);
    if (!ok) {
        *erro = *erro != 0 ? *erro : FORJA_PRC_ERR_OUTRO;
        forja_prc_fechar_handle(&info.processo);
        forja_prc_liberar_slot(s);
        if (ultimo != 0)
            forja_prc_k32_SetLastError(ultimo); /* o cleanup acima pode ter mexido: std:os.last_error() devolve a falha do CreateProcessW */
        return 0;
    }
    s->processo = info.processo;
    s->pid = (int64_t)info.pid;
    return ((uint64_t)s->geracao << 32) | (uint64_t)(pos + 1);
}

/* Canais validos: stdin so nulo ou herdado (pipe e arquivo de entrada nao existem), stdout e stderr de 0 a 3, arquivo pede caminho, nada acima do bit 12. */
static inline int forja_prc_canais_validos(int32_t canais, const char *saida, const char *saida_erro)
{
    int32_t entrada = canais & 0xF, fora = (canais >> 4) & 0xF, erro = (canais >> 8) & 0xF;

    if (canais < 0 || (canais >> 12) != 0 || (entrada != FORJA_PRC_CANAL_NULO && entrada != FORJA_PRC_CANAL_HERDA))
        return 0;
    return (fora != FORJA_PRC_CANAL_ARQUIVO || saida[0] != '\0') && (erro != FORJA_PRC_CANAL_ARQUIVO || saida_erro[0] != '\0');
}

/* UTF-16 do texto so quando 'usar'; falha de conversao zera *ok (o erro ja foi gravado). */
static inline unsigned short *forja_prc_larga_se(int usar, const char *texto, int32_t *erro, int *ok)
{
    unsigned short *larga = usar ? forja_prc_larga(texto, 0, erro) : 0;

    if (usar && larga == 0)
        *ok = 0;
    return larga;
}

static inline void forja_prc_liberar_pedido(ForjaPrcPedido *p)
{
    void *blocos[5] = {p->linha, p->ambiente, (void *)p->pasta, (void *)p->caminho[0], (void *)p->caminho[1]};

    for (int k = 0; k < 5; k++) {
        if (blocos[k] != 0)
            FORJA_ES_LIBERAR(blocos[k]);
    }
}

/* LIMITE: params: forja_processo_iniciar (5) + canais e os 2 arquivos de saida (8 argumentos; contrato do std:process, sem struct de uso unico) */
FORJA_ES_API uint64_t forja_process_spawn(const char *linha, const char *ambiente, const char *pasta, int32_t *erro, uint64_t limite_memoria, int32_t canais,
                                          const char *saida, const char *saida_erro)
{
    ForjaPrcPedido p = {0, 0, 0, limite_memoria, canais, {0, 0}};
    int ok = 1;
    uint64_t h = 0;

    if (!forja_prc_canais_validos(canais, saida, saida_erro)) {
        *erro = FORJA_PRC_ERR_ENTRADA;
        return 0;
    }
    p.linha = forja_prc_larga(linha, 0, erro);
    if (p.linha == 0)
        return 0;
    if (ambiente[0] != '\0') {
        p.ambiente = forja_prc_ambiente_larga(ambiente, erro);
        ok = p.ambiente != 0;
    }
    p.pasta = forja_prc_larga_se(pasta[0] != '\0', pasta, erro, &ok);
    p.caminho[0] = forja_prc_larga_se(((canais >> 4) & 0xF) == FORJA_PRC_CANAL_ARQUIVO, saida, erro, &ok);
    p.caminho[1] = forja_prc_larga_se(((canais >> 8) & 0xF) == FORJA_PRC_CANAL_ARQUIVO, saida_erro, erro, &ok);
    if (ok)
        h = forja_prc_disparar(&p, erro);
    forja_prc_liberar_pedido(&p);
    return h;
}

/* LIMITE: params: contrato do std:process (4 do 0.1.x + limite de memoria: 5 argumentos); ABI intacta: o default de sempre (stdin nulo, stdout e stderr em pipe) */
FORJA_ES_API uint64_t forja_processo_iniciar(const char *linha, const char *ambiente, const char *pasta, int32_t *erro, uint64_t limite_memoria)
{
    return forja_process_spawn(linha, ambiente, pasta, erro, limite_memoria, FORJA_PRC_CANAIS_PADRAO, "", "");
}

FORJA_ES_API int64_t forja_processo_ler(uint64_t h, int32_t canal, uint8_t *destino, int64_t n)
{
    int32_t pos = forja_prc_posicao(h);
    unsigned long disponivel = 0, lidos = 0;
    void *cano;

    if (pos < 0 || (canal != 0 && canal != 1) || n <= 0)
        return -2;
    cano = forja_prc_tabela[pos].canal[canal];
    if (cano == 0)
        return -1; /* canal que nao e pipe (null, inherit, file): nada a ler */
    if (!forja_prc_k32_PeekNamedPipe(cano, 0, 0, 0, &disponivel, 0))
        return forja_prc_k32_GetLastError() == FORJA_PRC_ERRO_CANO_FECHADO ? -1 : -2;
    if (disponivel == 0)
        return 0;
    if ((int64_t)disponivel > n)
        disponivel = (unsigned long)n;
    if (!forja_prc_k32_ReadFile(cano, destino, disponivel, &lidos, 0))
        return -2;
    return (int64_t)lidos;
}

FORJA_ES_API int32_t forja_processo_esperar(uint64_t h, int64_t ms)
{
    int32_t pos = forja_prc_posicao(h);
    unsigned long r;

    if (pos < 0)
        return -1;
    r = forja_prc_k32_WaitForSingleObject(forja_prc_tabela[pos].processo, ms < 0 ? 0UL : (unsigned long)ms);
    if (r == FORJA_PRC_ESPERA_TERMINOU)
        return 1;
    return r == FORJA_PRC_ESPERA_TEMPO ? 0 : -1;
}

FORJA_ES_API int64_t forja_processo_codigo(uint64_t h)
{
    int32_t pos = forja_prc_posicao(h);
    unsigned long codigo = 0;

    if (pos < 0 || !forja_prc_k32_GetExitCodeProcess(forja_prc_tabela[pos].processo, &codigo))
        return -1;
    return (int64_t)codigo;
}

FORJA_ES_API int32_t forja_processo_matar(uint64_t h)
{
    int32_t pos = forja_prc_posicao(h);

    return pos >= 0 && forja_prc_k32_TerminateJobObject(forja_prc_tabela[pos].job, 1) ? 1 : 0;
}

FORJA_ES_API void forja_processo_fechar(uint64_t h)
{
    int32_t pos = forja_prc_posicao(h);

    if (pos >= 0)
        forja_prc_liberar_slot(&forja_prc_tabela[pos]);
}

/* D-G3-L2a (bloco B). Pid guardado no slot quando o filho nasce; -1 = handle invalido. */
FORJA_ES_API int64_t forja_process_pid(uint64_t h)
{
    int32_t pos = forja_prc_posicao(h);

    return pos < 0 ? -1 : forja_prc_tabela[pos].pid;
}

/* Primeiro filho a terminar entre ids[0..n-1]: indice, -1 prazo, -2 lista/handle invalido, -3 outro. ms < 0 = sem prazo. */
FORJA_ES_API int64_t forja_process_wait_any(const uint64_t *ids, int64_t n, int64_t ms)
{
    void *hs[FORJA_PRC_MAX];
    unsigned long prazo = ms < 0 ? FORJA_PRC_SEM_PRAZO : (ms > (int64_t)FORJA_PRC_PRAZO_MAXIMO ? FORJA_PRC_PRAZO_MAXIMO : (unsigned long)ms);
    unsigned long r;

    if (n <= 0 || n > FORJA_PRC_MAX)
        return -2;
    for (int64_t k = 0; k < n; k++) {
        int32_t pos = forja_prc_posicao(ids[k]);

        if (pos < 0)
            return -2;
        hs[k] = forja_prc_tabela[pos].processo;
    }
    r = forja_prc_k32_WaitForMultipleObjects((unsigned long)n, hs, 0, prazo);
    if (r < (unsigned long)n)
        return (int64_t)r;
    return r == FORJA_PRC_ESPERA_TEMPO ? -1 : -3;
}

/* Pico de memoria COMMIT do Job inteiro (o filho e quem ele criou): PeakJobMemoryUsed. < 0 = -codigo neutro. */
FORJA_ES_API int64_t forja_process_peak_memory(uint64_t h)
{
    int32_t pos = forja_prc_posicao(h);
    ForjaPrcJob info = {{0, 0}, 0, 0, {0}};

    if (pos < 0)
        return -FORJA_PRC_ERR_ENTRADA;
    if (!forja_prc_k32_QueryInformationJobObject(forja_prc_tabela[pos].job, FORJA_PRC_JOB_ESTENDIDO, &info, (unsigned long)sizeof info, 0))
        return -FORJA_PRC_ERR_OUTRO;
    return info.resto[FORJA_PRC_JOB_INDICE_PICO];
}

/* 100 ns desde 1601 -> ns desde 1970 (0 antes de 1970 ou sem valor). */
static inline int64_t forja_prc_ns_desde_1970(uint64_t filetime)
{
    return filetime <= FORJA_PRC_FILETIME_1970 ? 0 : (int64_t)(filetime - FORJA_PRC_FILETIME_1970) * FORJA_PRC_NS_POR_TICK;
}

/* saida[0..3] = usuario, sistema, inicio, fim (ns). fim = 0 enquanto o filho roda. 0 = feito, senao o codigo neutro. */
FORJA_ES_API int32_t forja_process_cpu_times(uint64_t h, int64_t *saida)
{
    int32_t pos = forja_prc_posicao(h);
    uint64_t criacao = 0, fim = 0, nucleo = 0, usuario = 0;
    void *processo;

    if (pos < 0)
        return FORJA_PRC_ERR_ENTRADA;
    processo = forja_prc_tabela[pos].processo;
    if (!forja_prc_k32_GetProcessTimes(processo, &criacao, &fim, &nucleo, &usuario))
        return FORJA_PRC_ERR_OUTRO;
    saida[0] = (int64_t)usuario * FORJA_PRC_NS_POR_TICK;
    saida[1] = (int64_t)nucleo * FORJA_PRC_NS_POR_TICK;
    saida[2] = forja_prc_ns_desde_1970(criacao);
    saida[3] = forja_prc_k32_WaitForSingleObject(processo, 0) == FORJA_PRC_ESPERA_TERMINOU ? forja_prc_ns_desde_1970(fim) : 0;
    return 0;
}

#elif !defined(RT_ALVO_LINUX) /* fora do Windows: o std:process ainda nao tem runtime (M5 Linux) */

/* LIMITE: params: contrato do std:process (4 do 0.1.x + limite de memoria: 5 argumentos) */
FORJA_ES_API uint64_t forja_processo_iniciar(const char *linha, const char *ambiente, const char *pasta, int32_t *erro, uint64_t limite_memoria)
{
    (void)linha;
    (void)ambiente;
    (void)pasta;
    (void)limite_memoria;
    *erro = FORJA_PRC_ERR_OUTRO;
    return 0;
}

FORJA_ES_API int64_t forja_processo_ler(uint64_t h, int32_t canal, uint8_t *destino, int64_t n)
{
    (void)h;
    (void)canal;
    (void)destino;
    (void)n;
    return -2;
}

FORJA_ES_API int32_t forja_processo_esperar(uint64_t h, int64_t ms)
{
    (void)h;
    (void)ms;
    return -1;
}

FORJA_ES_API int64_t forja_processo_codigo(uint64_t h)
{
    (void)h;
    return -1;
}

FORJA_ES_API int32_t forja_processo_matar(uint64_t h)
{
    (void)h;
    return 0;
}

FORJA_ES_API void forja_processo_fechar(uint64_t h)
{
    (void)h;
}

#endif

#pragma GCC pop_options

#endif
