/*
 * forja_rt_es.h - handle de arquivo e sistema de arquivos do std:file no Windows, nos dois backends (S3 P5a, decisoes D6, D8 e D9). Texto ASCII puro.
 *
 * Versao: 0.1.4
 * Changelog:
 *   0.1.4 - D-G3-L2a (bloco D): forja_fs_abrir ganha o modo FORJA_FS_MODO_TRAVA (4: GENERIC_WRITE | DELETE, share 0, OPEN_ALWAYS, DELETE_ON_CLOSE); ERROR_SHARING_VIOLATION vira FORJA_FS_ERR_OCUPADO (10) so nesse modo.
 *           Na falha, o GetLastError do CreateFileW e regravado por SetLastError (std:os.last_error); 1 import novo no fim do ligar_tabelas.c (SetLastError). A ABI de forja_fs_abrir nao muda.
 *   0.1.3 - M5 P44: forja_fs_tabela sob forja_fs_trava (FORJA_ES_PEGAR/FORJA_ES_SOLTAR): so forja_fs_guardar e forja_fs_liberar travam (o E/S em si fica fora). Nativo: SysTrava de rt_sys.h, so trava com rt_multithread; backend C: nulo.
 *   0.1.2 - M5 P28b-1: com -DRT_ALVO_LINUX (o gcc MinGW define _WIN32 mesmo assim) o ramo do Windows e o stub saem; sobram so as constantes (FORJA_FS_*). O Windows e o backend C nao mudam.
 *   0.1.1 - S3 P6: push/pop_options com no-reorder-blocks-and-partition (o .pdata.unlikely de forja_fs_pasta_abrir fazia o ligar_pe recusar o x64 com X0905).
 *   0.1.0 - S3 P5a: handle (forja_fs_abrir, _ler, _gravar, _posicionar, _descarregar, _fechar), sistema de arquivos (forja_fs_info, _remover, _renomear,
 *           _copiar, _criar_pasta, _remover_pasta) e pastas (forja_fs_pasta_abrir, _pasta_proximo, _pasta_fechar), com as assinaturas do contrato D9.
 *
 * Codigos neutros de erro (iguais nos dois backends): 1 nao existe, 2 sem permissao, 3 outro (1..3 = SYS_ARQ_* de rt_sys.h), 4 ja existe, 5 e pasta,
 * 6 nao e pasta, 7 pasta nao vazia, 8 nao suportado (fora do Windows), 9 fechado (handle invalido, ja fechado ou de outro tipo).
 *
 * Handle: u64 opaco = (geracao << 32) | (posicao + 1) numa tabela estatica de FORJA_FS_HANDLES_MAX posicoes. A geracao sobe a cada fechamento: um handle
 * antigo nunca casa com a posicao reusada. 0 = falha. Sem trava: o runtime roda numa thread so. Limite: 32 handles abertos ao mesmo tempo.
 * Caminho: UTF-8 terminado em NUL; convertido para UTF-16 com MB_ERR_INVALID_CHARS; vazio = nao existe; sem prefixo \\?\ (limite de 260 caracteres).
 * Erro so e escrito em *erro quando a funcao falha. Pasta: a ordenacao dos nomes e de quem chama (aqui sai na ordem do sistema).
 *
 * Nativo: defina FORJA_ES_NATIVO antes do include (sem libc; rt_sys.h antes, para sys_heap_*). As funcoes saem externas (rt_es_win.c).
 * C: funcoes static inline; entram no .c gerado so quando o programa importa std:file. Macros FORJA_ES_ALOCAR e FORJA_ES_LIBERAR podem ser redefinidas antes do include.
 * Imports de kernel32: __declspec(dllimport) + rotulo asm (FORJA_ES_ROTULO): o simbolo e o do Windows e o nome C e interno (nao colide com o programa).
 */
#ifndef FORJA_RT_ES_H
#define FORJA_RT_ES_H

#include <stdint.h>

#if defined(FORJA_ES_NATIVO)
#define FORJA_ES_API
#ifndef FORJA_ES_ALOCAR
#define FORJA_ES_ALOCAR(n) sys_heap_alocar(n)
#endif
#ifndef FORJA_ES_LIBERAR
#define FORJA_ES_LIBERAR(p) sys_heap_liberar(p)
#endif
/* Trava SRW do estado compartilhado (rt_sys.h): so trava com rt_multithread ligado. Pegar devolve se travou; soltar recebe esse valor. */
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
/* Backend C: sem thread nesta etapa (P44), a trava e nula. */
#ifndef FORJA_ES_TRAVA_T
#define FORJA_ES_TRAVA_T char
#define FORJA_ES_PEGAR(t) ((void)(t), 0)
#define FORJA_ES_SOLTAR(t, tomou) ((void)(t), (void)(tomou))
#endif
#endif

/* Codigos neutros de erro: a ordem do FileError de std:file. 1..3 = SYS_ARQ_* de rt_sys.h. */
#define FORJA_FS_ERR_NAO_EXISTE 1
#define FORJA_FS_ERR_SEM_PERMISSAO 2
#define FORJA_FS_ERR_OUTRO 3
#define FORJA_FS_ERR_JA_EXISTE 4
#define FORJA_FS_ERR_DIRETORIO 5
#define FORJA_FS_ERR_NAO_DIRETORIO 6
#define FORJA_FS_ERR_NAO_VAZIO 7
#define FORJA_FS_ERR_NAO_SUPORTADO 8
#define FORJA_FS_ERR_FECHADO 9
#define FORJA_FS_ERR_OCUPADO 10 /* so no modo trava: outro handle ja tem o arquivo (ERROR_SHARING_VIOLATION) */

/* Modo de abertura (ordem do OpenMode), origem do seek (ordem do SeekFrom) e tipo do item (ordem do FileKind). */
#define FORJA_FS_MODO_LER 0
#define FORJA_FS_MODO_GRAVAR 1
#define FORJA_FS_MODO_ANEXAR 2
#define FORJA_FS_MODO_CRIAR 3
#define FORJA_FS_MODO_TRAVA 4 /* file.lock: GENERIC_WRITE | DELETE, share 0, OPEN_ALWAYS, DELETE_ON_CLOSE */
#define FORJA_FS_ORIGEM_INICIO 0
#define FORJA_FS_ORIGEM_ATUAL 1
#define FORJA_FS_ORIGEM_FIM 2
#define FORJA_FS_TIPO_ARQUIVO 0
#define FORJA_FS_TIPO_PASTA 1
#define FORJA_FS_TIPO_LINK 2
#define FORJA_FS_TIPO_OUTRO 3
#define FORJA_FS_TIPO_BUSCA 4 /* interno: posicao de busca de pasta, nunca sai para fora */

#define FORJA_FS_HANDLES_MAX 32
#define FORJA_FS_LOTE_MAX 0x40000000UL /* 1 GiB por ReadFile/WriteFile, como o rt_arquivo */
#define FORJA_FS_UTF8 65001u
/* FILETIME de 1970-01-01 (100 ns desde 1601-01-01): mtime_ns = (FILETIME - esta epoca) * 100. */
#define FORJA_FS_EPOCA_FILETIME 116444736000000000ULL

/* S3 P6 (0.1.1): sem a particao quente/fria do gcc. forja_fs_pasta_abrir tinha um .pdata.unlikely fora de ordem e o ligar_pe recusava o programa com X0905 (so quem usa read_dir no x64). */
#pragma GCC push_options
#pragma GCC optimize("no-reorder-blocks-and-partition")

/* P28b-1: o gcc MinGW define _WIN32 tambem para o runtime Linux (-DRT_ALVO_LINUX): ali so valem as constantes acima (as funcoes vivem em rt_es_linux.c). */
#if defined(_WIN32) && !defined(RT_ALVO_LINUX)
#ifndef FORJA_ES_ROTULO
#define FORJA_ES_ROTULO_TXT_(x) #x
#define FORJA_ES_ROTULO_XTXT_(x) FORJA_ES_ROTULO_TXT_(x)
#define FORJA_ES_ROTULO(nome) __asm__(FORJA_ES_ROTULO_XTXT_(__USER_LABEL_PREFIX__) nome)
#endif

#define FORJA_FS_INVALIDO ((void *)(intptr_t)-1) /* INVALID_HANDLE_VALUE */
#define FORJA_FS_GENERIC_READ 0x80000000UL
#define FORJA_FS_GENERIC_WRITE 0x40000000UL
#define FORJA_FS_FILE_APPEND_DATA 0x4UL
#define FORJA_FS_COMPARTILHA 0x7UL /* FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE */
#define FORJA_FS_DELETE 0x10000UL /* acesso DELETE: o apagar ao fechar do modo trava */
#define FORJA_FS_APAGAR_AO_FECHAR 0x04000000UL /* FILE_FLAG_DELETE_ON_CLOSE */
#define FORJA_FS_ERRO_COMPARTILHAMENTO 32UL /* ERROR_SHARING_VIOLATION */
#define FORJA_FS_CRIAR_NOVO 1UL /* CREATE_NEW */
#define FORJA_FS_CRIAR_ZERAR 2UL /* CREATE_ALWAYS */
#define FORJA_FS_ABRIR_EXISTENTE 3UL /* OPEN_EXISTING */
#define FORJA_FS_ABRIR_OU_CRIAR 4UL /* OPEN_ALWAYS */
#define FORJA_FS_ATRIBUTO_NORMAL 0x80UL
#define FORJA_FS_ATRIBUTO_PASTA 0x10UL
#define FORJA_FS_ATRIBUTO_DISPOSITIVO 0x40UL
#define FORJA_FS_ATRIBUTO_LINK 0x400UL /* FILE_ATTRIBUTE_REPARSE_POINT: juncao, link simbolico e afins */
#define FORJA_FS_ERRO_ACESSO 5UL
#define FORJA_FS_ERRO_SEM_MAIS 18UL /* ERROR_NO_MORE_FILES: fim da busca de pasta */
#define FORJA_FS_MOVER_SOBRESCREVER 0x1UL /* MOVEFILE_REPLACE_EXISTING */
#define FORJA_FS_MB_ERR_INVALIDO 0x8UL /* MB_ERR_INVALID_CHARS */
#define FORJA_FS_INFO_PADRAO 0 /* GetFileExInfoStandard */

/* WIN32_FILE_ATTRIBUTE_DATA (36 bytes). FILETIME = dois u32: baixo, alto. */
typedef struct {
    unsigned long atributos;
    unsigned long criado[2];
    unsigned long acessado[2];
    unsigned long escrito[2];
    unsigned long tamanho_alto;
    unsigned long tamanho_baixo;
} ForjaFsAtributos;

/* WIN32_FIND_DATAW (592 bytes): os campos que o runtime le tem nome; o resto e reservado no mesmo tamanho. */
typedef struct {
    unsigned long atributos;
    unsigned long tempos[6];
    unsigned long tamanho_alto;
    unsigned long tamanho_baixo;
    unsigned long reservado[2];
    unsigned short nome[260];
    unsigned short nome_curto[14];
} ForjaFsBusca;

/* Posicao da tabela de handles: um arquivo aberto ou uma busca de pasta. h = 0 quer dizer posicao livre. */
typedef struct {
    void *h;
    uint32_t geracao;
    int32_t tipo;
    int32_t pendente; /* busca: o item em busca ainda nao foi entregue a quem chama */
    ForjaFsBusca busca;
} ForjaFsSlot;

static ForjaFsSlot forja_fs_tabela[FORJA_FS_HANDLES_MAX]; /* compartilhado: posicao livre achada e tomada (guardar), e devolvida (liberar), sob forja_fs_trava */
static FORJA_ES_TRAVA_T forja_fs_trava; /* compartilhado: so trava com rt_multithread ligado */

/* kernel32 sem windows.h (colidiria com nomes do programa). Assinaturas iguais em forja_rt_stdin.h: redeclarar igual e permitido. */
/* LIMITE: params: assinatura do CreateFileW (7 argumentos) */
__declspec(dllimport) void *forja_es_k32_CreateFileW(const unsigned short *nome, unsigned long acesso, unsigned long compartilha, void *seguranca,
                                                     unsigned long criacao, unsigned long atributos, void *modelo) FORJA_ES_ROTULO("CreateFileW");
/* LIMITE: params: assinatura do ReadFile (5 argumentos) */
__declspec(dllimport) int forja_es_k32_ReadFile(void *h, void *buf, unsigned long n, unsigned long *lidos, void *sobreposto) FORJA_ES_ROTULO("ReadFile");
/* LIMITE: params: assinatura do WriteFile (5 argumentos) */
__declspec(dllimport) int forja_es_k32_WriteFile(void *h, const void *buf, unsigned long n, unsigned long *escritos, void *sobreposto)
    FORJA_ES_ROTULO("WriteFile");
__declspec(dllimport) int forja_es_k32_SetFilePointerEx(void *h, int64_t distancia, int64_t *nova, unsigned long metodo)
    FORJA_ES_ROTULO("SetFilePointerEx");
__declspec(dllimport) int forja_es_k32_FlushFileBuffers(void *h) FORJA_ES_ROTULO("FlushFileBuffers");
__declspec(dllimport) int forja_es_k32_CloseHandle(void *h) FORJA_ES_ROTULO("CloseHandle");
__declspec(dllimport) unsigned long forja_es_k32_GetLastError(void) FORJA_ES_ROTULO("GetLastError");
__declspec(dllimport) void forja_es_k32_SetLastError(unsigned long codigo) FORJA_ES_ROTULO("SetLastError");
__declspec(dllimport) int forja_fs_k32_GetFileAttributesExW(const unsigned short *nome, int nivel, void *info)
    FORJA_ES_ROTULO("GetFileAttributesExW");
__declspec(dllimport) int forja_fs_k32_DeleteFileW(const unsigned short *nome) FORJA_ES_ROTULO("DeleteFileW");
__declspec(dllimport) int forja_fs_k32_MoveFileExW(const unsigned short *origem, const unsigned short *destino, unsigned long flags)
    FORJA_ES_ROTULO("MoveFileExW");
__declspec(dllimport) int forja_fs_k32_CopyFileW(const unsigned short *origem, const unsigned short *destino, int falha_se_existe)
    FORJA_ES_ROTULO("CopyFileW");
__declspec(dllimport) int forja_fs_k32_CreateDirectoryW(const unsigned short *nome, void *seguranca) FORJA_ES_ROTULO("CreateDirectoryW");
__declspec(dllimport) int forja_fs_k32_RemoveDirectoryW(const unsigned short *nome) FORJA_ES_ROTULO("RemoveDirectoryW");
__declspec(dllimport) void *forja_fs_k32_FindFirstFileW(const unsigned short *padrao, ForjaFsBusca *dados) FORJA_ES_ROTULO("FindFirstFileW");
__declspec(dllimport) int forja_fs_k32_FindNextFileW(void *h, ForjaFsBusca *dados) FORJA_ES_ROTULO("FindNextFileW");
__declspec(dllimport) int forja_fs_k32_FindClose(void *h) FORJA_ES_ROTULO("FindClose");
/* LIMITE: params: assinatura do MultiByteToWideChar (6 argumentos) */
__declspec(dllimport) int forja_es_k32_MultiByteToWideChar(unsigned int pagina, unsigned long flags, const char *origem, int norigem,
                                                           unsigned short *destino, int ndestino) FORJA_ES_ROTULO("MultiByteToWideChar");
/* LIMITE: params: assinatura do WideCharToMultiByte (8 argumentos) */
__declspec(dllimport) int forja_es_k32_WideCharToMultiByte(unsigned int pagina, unsigned long flags, const unsigned short *origem, int norigem,
                                                           char *destino, int ndestino, const char *padrao, int *usou_padrao)
    FORJA_ES_ROTULO("WideCharToMultiByte");

/* Caminho UTF-8 -> UTF-16 com NUL, num bloco do alocador com 'extra' unidades de folga no fim (quem chama libera). 0 com *erro em falha. */
static inline unsigned short *forja_fs_caminho_larga(const char *caminho, int32_t extra, int32_t *erro)
{
    int n;
    unsigned short *larga;

    if (caminho[0] == '\0') {
        *erro = FORJA_FS_ERR_NAO_EXISTE;
        return 0;
    }
    n = forja_es_k32_MultiByteToWideChar(FORJA_FS_UTF8, FORJA_FS_MB_ERR_INVALIDO, caminho, -1, 0, 0);
    larga = n > 0 ? FORJA_ES_ALOCAR(((int64_t)n + extra) * 2) : 0;
    if (larga == 0) {
        *erro = FORJA_FS_ERR_OUTRO;
        return 0;
    }
    if (forja_es_k32_MultiByteToWideChar(FORJA_FS_UTF8, FORJA_FS_MB_ERR_INVALIDO, caminho, -1, larga, n) <= 0) {
        FORJA_ES_LIBERAR(larga);
        *erro = FORJA_FS_ERR_OUTRO;
        return 0;
    }
    return larga;
}

static inline int forja_fs_comprimento(const unsigned short *larga)
{
    int n = 0;

    while (larga[n] != 0)
        n++;
    return n;
}

/* Tipo do item a partir dos atributos: ponto de reparse (link) antes de pasta, para juncao ser link. */
static inline int32_t forja_fs_tipo_de(unsigned long atributos)
{
    if (atributos & FORJA_FS_ATRIBUTO_LINK)
        return FORJA_FS_TIPO_LINK;
    if (atributos & FORJA_FS_ATRIBUTO_PASTA)
        return FORJA_FS_TIPO_PASTA;
    if (atributos & FORJA_FS_ATRIBUTO_DISPOSITIVO)
        return FORJA_FS_TIPO_OUTRO;
    return FORJA_FS_TIPO_ARQUIVO;
}

/* Tipo do que esta no caminho, sem seguir link; -1 se nao existe ou nao da para ver. */
static inline int32_t forja_fs_tipo_caminho(const unsigned short *larga)
{
    ForjaFsAtributos atrib;

    if (!forja_fs_k32_GetFileAttributesExW(larga, FORJA_FS_INFO_PADRAO, &atrib))
        return -1;
    return forja_fs_tipo_de(atrib.atributos);
}

/* Codigo neutro de um GetLastError (a tabela de rt_arquivo_win.c, mais JA_EXISTE, NAO_VAZIO e DIRETORIO). */
static inline int32_t forja_fs_erro_win(unsigned long codigo)
{
    switch (codigo) {
    case 2UL:
    case 3UL:
    case 15UL:
    case 161UL:
    case 206UL:
        return FORJA_FS_ERR_NAO_EXISTE;
    case 5UL:
        return FORJA_FS_ERR_SEM_PERMISSAO;
    case 80UL:
    case 183UL:
        return FORJA_FS_ERR_JA_EXISTE;
    case 145UL:
        return FORJA_FS_ERR_NAO_VAZIO;
    case 267UL:
        return FORJA_FS_ERR_DIRETORIO;
    default:
        return codigo >= 19UL && codigo <= 36UL ? FORJA_FS_ERR_SEM_PERMISSAO : FORJA_FS_ERR_OUTRO;
    }
}

/* Falha em arquivo: acesso negado sobre uma pasta vira DIRETORIO (DeleteFileW e CreateFileW dao acesso negado em pasta). */
static inline int32_t forja_fs_falha_arquivo(const unsigned short *larga, unsigned long falha)
{
    if (falha == FORJA_FS_ERRO_ACESSO && forja_fs_tipo_caminho(larga) == FORJA_FS_TIPO_PASTA)
        return FORJA_FS_ERR_DIRETORIO;
    return forja_fs_erro_win(falha);
}

/* Falha em pasta: arquivo no lugar de pasta vira NAO_DIRETORIO (o Windows devolve ERROR_DIRECTORY, 267, que e ambiguo). */
static inline int32_t forja_fs_falha_pasta(const unsigned short *larga, unsigned long falha)
{
    if (forja_fs_tipo_caminho(larga) == FORJA_FS_TIPO_ARQUIVO)
        return FORJA_FS_ERR_NAO_DIRETORIO;
    return forja_fs_erro_win(falha);
}

/* Modo -> acesso e criacao do CreateFileW. 0 se o modo nao existe. */
static inline int32_t forja_fs_modo_win(int32_t modo, unsigned long *acesso, unsigned long *criacao)
{
    switch (modo) {
    case FORJA_FS_MODO_LER:
        *acesso = FORJA_FS_GENERIC_READ;
        *criacao = FORJA_FS_ABRIR_EXISTENTE;
        return 1;
    case FORJA_FS_MODO_GRAVAR:
        *acesso = FORJA_FS_GENERIC_WRITE;
        *criacao = FORJA_FS_CRIAR_ZERAR;
        return 1;
    case FORJA_FS_MODO_ANEXAR:
        *acesso = FORJA_FS_FILE_APPEND_DATA;
        *criacao = FORJA_FS_ABRIR_OU_CRIAR;
        return 1;
    case FORJA_FS_MODO_CRIAR:
        *acesso = FORJA_FS_GENERIC_WRITE;
        *criacao = FORJA_FS_CRIAR_NOVO;
        return 1;
    case FORJA_FS_MODO_TRAVA:
        *acesso = FORJA_FS_GENERIC_WRITE | FORJA_FS_DELETE;
        *criacao = FORJA_FS_ABRIR_OU_CRIAR;
        return 1;
    default:
        return 0;
    }
}

/* Tamanho de uma leitura ou escrita: no maximo 1 GiB por chamada do sistema. */
static inline unsigned long forja_fs_lote(int64_t quantos)
{
    return quantos > (int64_t)FORJA_FS_LOTE_MAX ? FORJA_FS_LOTE_MAX : (unsigned long)quantos;
}

static inline void forja_fs_soltar(void *h, int32_t tipo)
{
    if (tipo == FORJA_FS_TIPO_BUSCA)
        forja_fs_k32_FindClose(h);
    else
        forja_es_k32_CloseHandle(h);
}

/* Posicao do handle, ou 0 se o handle nao vale (fechado, de outro tipo ou de outra geracao). */
static inline ForjaFsSlot *forja_fs_achar(uint64_t h, int32_t tipo)
{
    uint64_t posicao = h & 0xFFFFFFFFUL;
    ForjaFsSlot *s;

    if (posicao == 0 || posicao > FORJA_FS_HANDLES_MAX)
        return 0;
    s = &forja_fs_tabela[posicao - 1];
    if (s->h == 0 || s->tipo != tipo || (uint64_t)s->geracao != (h >> 32))
        return 0;
    return s;
}

/* Libera a posicao e sobe a geracao: o handle antigo deixa de valer. */
static inline void forja_fs_liberar(ForjaFsSlot *s)
{
    int tomou = FORJA_ES_PEGAR(forja_fs_trava);

    forja_fs_soltar(s->h, s->tipo);
    s->h = 0;
    s->geracao++;
    FORJA_ES_SOLTAR(forja_fs_trava, tomou);
}

/* Guarda o handle numa posicao livre e devolve o handle opaco. Tabela cheia: fecha o h e devolve 0 com OUTRO. */
static inline uint64_t forja_fs_guardar(void *h, int32_t tipo, int32_t *erro)
{
    uint32_t i;
    int tomou = FORJA_ES_PEGAR(forja_fs_trava);

    for (i = 0; i < FORJA_FS_HANDLES_MAX; i++) {
        ForjaFsSlot *s = &forja_fs_tabela[i];

        if (s->h == 0) {
            s->h = h;
            s->tipo = tipo;
            s->pendente = 0;
            FORJA_ES_SOLTAR(forja_fs_trava, tomou);
            return ((uint64_t)s->geracao << 32) | (uint64_t)(i + 1);
        }
    }
    FORJA_ES_SOLTAR(forja_fs_trava, tomou);
    forja_fs_soltar(h, tipo);
    *erro = FORJA_FS_ERR_OUTRO;
    return 0;
}

/* Posicao do handle de arquivo aberto; 0 com *erro = FECHADO. */
static inline ForjaFsSlot *forja_fs_arquivo_aberto(uint64_t h, int32_t *erro)
{
    ForjaFsSlot *s = forja_fs_achar(h, FORJA_FS_TIPO_ARQUIVO);

    if (s == 0)
        *erro = FORJA_FS_ERR_FECHADO;
    return s;
}

/* FILETIME -> ns desde 1970 (100 ns por unidade): FORJA_FS_EPOCA_FILETIME e o ponto de partida. */
static inline int64_t forja_fs_mtime_ns(const unsigned long *filetime)
{
    uint64_t ft = ((uint64_t)filetime[1] << 32) | (uint64_t)filetime[0];

    return (int64_t)(ft - FORJA_FS_EPOCA_FILETIME) * 100;
}

/* Abre o arquivo: 0 e o handle opaco, ou 0 com *erro. */
FORJA_ES_API uint64_t forja_fs_abrir(const char *caminho, int32_t modo, int32_t *erro)
{
    unsigned long acesso = 0;
    unsigned long criacao = 0;
    unsigned long compartilha = modo == FORJA_FS_MODO_TRAVA ? 0UL : FORJA_FS_COMPARTILHA;
    unsigned long atributos = modo == FORJA_FS_MODO_TRAVA ? FORJA_FS_ATRIBUTO_NORMAL | FORJA_FS_APAGAR_AO_FECHAR : FORJA_FS_ATRIBUTO_NORMAL;
    unsigned short *larga;
    void *h;
    int32_t codigo = 0;
    unsigned long ultimo = 0;

    if (!forja_fs_modo_win(modo, &acesso, &criacao)) {
        *erro = FORJA_FS_ERR_OUTRO;
        return 0;
    }
    larga = forja_fs_caminho_larga(caminho, 0, erro);
    if (larga == 0)
        return 0;
    h = forja_es_k32_CreateFileW(larga, acesso, compartilha, 0, criacao, atributos, 0);
    if (h == FORJA_FS_INVALIDO) {
        ultimo = forja_es_k32_GetLastError();
        codigo = modo == FORJA_FS_MODO_TRAVA && ultimo == FORJA_FS_ERRO_COMPARTILHAMENTO ? FORJA_FS_ERR_OCUPADO : forja_fs_falha_arquivo(larga, ultimo);
    }
    FORJA_ES_LIBERAR(larga);
    if (h == FORJA_FS_INVALIDO) {
        forja_es_k32_SetLastError(ultimo); /* o liberar e o tipo_caminho acima podem ter mexido: os.last_error() devolve a falha do CreateFileW */
        *erro = codigo;
        return 0;
    }
    return forja_fs_guardar(h, FORJA_FS_TIPO_ARQUIVO, erro);
}

/* Le ate n bytes: quantos leu, 0 = fim do arquivo, -1 com *erro. */
FORJA_ES_API int64_t forja_fs_ler(uint64_t h, uint8_t *destino, int64_t n, int32_t *erro)
{
    ForjaFsSlot *s = forja_fs_arquivo_aberto(h, erro);
    unsigned long lidos = 0;

    if (s == 0)
        return -1;
    if (n <= 0)
        return 0;
    if (!forja_es_k32_ReadFile(s->h, destino, forja_fs_lote(n), &lidos, 0)) {
        *erro = forja_fs_erro_win(forja_es_k32_GetLastError());
        return -1;
    }
    return (int64_t)lidos;
}

/* Grava os n bytes inteiros (repete o WriteFile ate sair tudo): quantos gravou (= n) ou -1 com *erro. */
FORJA_ES_API int64_t forja_fs_gravar(uint64_t h, const uint8_t *origem, int64_t n, int32_t *erro)
{
    ForjaFsSlot *s = forja_fs_arquivo_aberto(h, erro);
    int64_t feito = 0;

    if (s == 0)
        return -1;
    while (feito < n) {
        unsigned long escritos = 0;

        if (!forja_es_k32_WriteFile(s->h, origem + feito, forja_fs_lote(n - feito), &escritos, 0)) {
            *erro = forja_fs_erro_win(forja_es_k32_GetLastError());
            return -1;
        }
        if (escritos == 0) {
            *erro = FORJA_FS_ERR_OUTRO;
            return -1;
        }
        feito += (int64_t)escritos;
    }
    return feito;
}

/* Move o ponteiro (origem: inicio, atual ou fim): devolve a posicao nova ou -1 com *erro. */
FORJA_ES_API int64_t forja_fs_posicionar(uint64_t h, int64_t desloc, int32_t origem, int32_t *erro)
{
    ForjaFsSlot *s = forja_fs_arquivo_aberto(h, erro);
    int64_t nova = 0;

    if (s == 0)
        return -1;
    if (origem < FORJA_FS_ORIGEM_INICIO || origem > FORJA_FS_ORIGEM_FIM) {
        *erro = FORJA_FS_ERR_OUTRO;
        return -1;
    }
    if (!forja_es_k32_SetFilePointerEx(s->h, desloc, &nova, (unsigned long)origem)) {
        *erro = forja_fs_erro_win(forja_es_k32_GetLastError());
        return -1;
    }
    return nova;
}

/* Descarrega o arquivo no disco (FlushFileBuffers): 0, ou -1 com *erro. */
FORJA_ES_API int32_t forja_fs_descarregar(uint64_t h, int32_t *erro)
{
    ForjaFsSlot *s = forja_fs_arquivo_aberto(h, erro);

    if (s == 0)
        return -1;
    if (!forja_es_k32_FlushFileBuffers(s->h)) {
        *erro = forja_fs_erro_win(forja_es_k32_GetLastError());
        return -1;
    }
    return 0;
}

/* Fecha o handle. Sem retorno: um handle ja fechado ou invalido nao faz nada. */
FORJA_ES_API void forja_fs_fechar(uint64_t h)
{
    ForjaFsSlot *s = forja_fs_achar(h, FORJA_FS_TIPO_ARQUIVO);

    if (s != 0)
        forja_fs_liberar(s);
}

/* Consulta o caminho sem seguir link: tamanho em bytes, mtime_ns (UTC desde 1970) e tipo. 0 ou o codigo neutro. */
FORJA_ES_API int32_t forja_fs_info(const char *caminho, int64_t *tamanho, int64_t *mtime_ns, int32_t *tipo)
{
    int32_t codigo = 0;
    unsigned short *larga = forja_fs_caminho_larga(caminho, 0, &codigo);
    ForjaFsAtributos atrib;
    int ok;

    if (larga == 0)
        return codigo;
    ok = forja_fs_k32_GetFileAttributesExW(larga, FORJA_FS_INFO_PADRAO, &atrib);
    if (!ok)
        codigo = forja_fs_erro_win(forja_es_k32_GetLastError());
    FORJA_ES_LIBERAR(larga);
    if (!ok)
        return codigo;
    *tamanho = (int64_t)(((uint64_t)atrib.tamanho_alto << 32) | (uint64_t)atrib.tamanho_baixo);
    *mtime_ns = forja_fs_mtime_ns(atrib.escrito);
    *tipo = forja_fs_tipo_de(atrib.atributos);
    return 0;
}

/* Apaga um arquivo (nunca uma pasta: pasta devolve DIRETORIO). 0 ou o codigo neutro. */
FORJA_ES_API int32_t forja_fs_remover(const char *caminho)
{
    int32_t codigo = 0;
    unsigned short *larga = forja_fs_caminho_larga(caminho, 0, &codigo);
    int ok;

    if (larga == 0)
        return codigo;
    ok = forja_fs_k32_DeleteFileW(larga);
    if (!ok)
        codigo = forja_fs_falha_arquivo(larga, forja_es_k32_GetLastError());
    FORJA_ES_LIBERAR(larga);
    return ok ? 0 : codigo;
}

/* Renomeia de origem para destino, sobrescrevendo o destino (MoveFileExW). 0 ou o codigo neutro. */
FORJA_ES_API int32_t forja_fs_renomear(const char *origem, const char *destino)
{
    int32_t codigo = 0;
    unsigned short *de = forja_fs_caminho_larga(origem, 0, &codigo);
    unsigned short *para;
    int ok;

    if (de == 0)
        return codigo;
    para = forja_fs_caminho_larga(destino, 0, &codigo);
    if (para == 0) {
        FORJA_ES_LIBERAR(de);
        return codigo;
    }
    ok = forja_fs_k32_MoveFileExW(de, para, FORJA_FS_MOVER_SOBRESCREVER);
    if (!ok)
        codigo = forja_fs_falha_arquivo(de, forja_es_k32_GetLastError());
    FORJA_ES_LIBERAR(de);
    FORJA_ES_LIBERAR(para);
    return ok ? 0 : codigo;
}

/* Copia o arquivo de origem para destino, sobrescrevendo (CopyFileW). 0 ou o codigo neutro. */
FORJA_ES_API int32_t forja_fs_copiar(const char *origem, const char *destino)
{
    int32_t codigo = 0;
    unsigned short *de = forja_fs_caminho_larga(origem, 0, &codigo);
    unsigned short *para;
    int ok;

    if (de == 0)
        return codigo;
    para = forja_fs_caminho_larga(destino, 0, &codigo);
    if (para == 0) {
        FORJA_ES_LIBERAR(de);
        return codigo;
    }
    ok = forja_fs_k32_CopyFileW(de, para, 0);
    if (!ok)
        codigo = forja_fs_falha_arquivo(de, forja_es_k32_GetLastError());
    FORJA_ES_LIBERAR(de);
    FORJA_ES_LIBERAR(para);
    return ok ? 0 : codigo;
}

/* Cria uma pasta (so o ultimo nivel: quem quer varios niveis faz em Forja). 0 ou o codigo neutro. */
FORJA_ES_API int32_t forja_fs_criar_pasta(const char *caminho)
{
    int32_t codigo = 0;
    unsigned short *larga = forja_fs_caminho_larga(caminho, 0, &codigo);
    int ok;

    if (larga == 0)
        return codigo;
    ok = forja_fs_k32_CreateDirectoryW(larga, 0);
    if (!ok)
        codigo = forja_fs_erro_win(forja_es_k32_GetLastError());
    FORJA_ES_LIBERAR(larga);
    return ok ? 0 : codigo;
}

/* Remove uma pasta vazia (RemoveDirectoryW): cheia = NAO_VAZIO, arquivo = NAO_DIRETORIO. 0 ou o codigo neutro. */
FORJA_ES_API int32_t forja_fs_remover_pasta(const char *caminho)
{
    int32_t codigo = 0;
    unsigned short *larga = forja_fs_caminho_larga(caminho, 0, &codigo);
    int ok;

    if (larga == 0)
        return codigo;
    ok = forja_fs_k32_RemoveDirectoryW(larga);
    if (!ok)
        codigo = forja_fs_falha_pasta(larga, forja_es_k32_GetLastError());
    FORJA_ES_LIBERAR(larga);
    return ok ? 0 : codigo;
}

/* Acrescenta "\*" (ou so "*" se o caminho ja termina em separador) no buffer de forja_fs_caminho_larga com extra 2.
 * Devolve o tamanho do caminho antes do padrao, para quem precisa classificar o caminho puro depois. */
static inline int forja_fs_montar_padrao(unsigned short *larga)
{
    int tamanho = forja_fs_comprimento(larga);
    int original = tamanho;

    if (tamanho == 0 || (larga[tamanho - 1] != '\\' && larga[tamanho - 1] != '/'))
        larga[tamanho++] = '\\';
    larga[tamanho++] = '*';
    larga[tamanho] = 0;
    return original;
}

/* Guarda a busca aberta numa posicao: o primeiro item ja vem em busca e fica pendente. */
static inline uint64_t forja_fs_guardar_busca(void *h, const ForjaFsBusca *busca, int32_t *erro)
{
    uint64_t handle = forja_fs_guardar(h, FORJA_FS_TIPO_BUSCA, erro);
    ForjaFsSlot *s;

    if (handle == 0)
        return 0;
    s = forja_fs_achar(handle, FORJA_FS_TIPO_BUSCA);
    s->busca = *busca;
    s->pendente = 1;
    return handle;
}

/* Abre a pasta para listar (sem "." nem ".."). 0 = handle opaco, ou 0 com *erro. */
FORJA_ES_API uint64_t forja_fs_pasta_abrir(const char *caminho, int32_t *erro)
{
    unsigned short *larga = forja_fs_caminho_larga(caminho, 2, erro);
    ForjaFsBusca busca;
    void *h;
    int32_t codigo = 0;
    int comprimento;

    if (larga == 0)
        return 0;
    comprimento = forja_fs_montar_padrao(larga);
    h = forja_fs_k32_FindFirstFileW(larga, &busca);
    if (h == FORJA_FS_INVALIDO) {
        larga[comprimento] = 0;
        codigo = forja_fs_falha_pasta(larga, forja_es_k32_GetLastError());
    }
    FORJA_ES_LIBERAR(larga);
    if (h == FORJA_FS_INVALIDO) {
        *erro = codigo;
        return 0;
    }
    return forja_fs_guardar_busca(h, &busca, erro);
}

/* Nome "." ou ".." (sempre no comeco de uma busca de pasta; nao e item). */
static inline int32_t forja_fs_nome_oculto(const unsigned short *nome)
{
    if (nome[0] == '.' && nome[1] == 0)
        return 1;
    return nome[0] == '.' && nome[1] == '.' && nome[2] == 0;
}

/* Avanca a busca ate um item que nao e "." nem "..": 1 = item em s->busca; 0 = fim; negativo = -codigo neutro. */
static inline int32_t forja_fs_avancar_busca(ForjaFsSlot *s)
{
    unsigned long falha;

    while (s->pendente || forja_fs_k32_FindNextFileW(s->h, &s->busca)) {
        s->pendente = 0;
        if (!forja_fs_nome_oculto(s->busca.nome))
            return 1;
    }
    falha = forja_es_k32_GetLastError();
    return falha == FORJA_FS_ERRO_SEM_MAIS ? 0 : -forja_fs_erro_win(falha);
}

/* Nome UTF-16 em UTF-8 no destino, sem NUL: devolve o tamanho, ou -OUTRO se nao coube. */
static inline int64_t forja_fs_nome_utf8(const unsigned short *nome, uint8_t *destino, int64_t n)
{
    int tam = forja_es_k32_WideCharToMultiByte(FORJA_FS_UTF8, 0, nome, -1, (char *)destino, (int)n, 0, 0);

    return tam > 0 ? (int64_t)tam - 1 : -FORJA_FS_ERR_OUTRO;
}

/* Proximo item da pasta: tamanho do nome em UTF-8 no destino (1024 bytes bastam), 0 = fim, negativo = -codigo.
 * *tipo recebe o tipo do item (FileKind). */
FORJA_ES_API int64_t forja_fs_pasta_proximo(uint64_t h, uint8_t *destino, int64_t n, int32_t *tipo)
{
    ForjaFsSlot *s = forja_fs_achar(h, FORJA_FS_TIPO_BUSCA);
    int32_t codigo;

    if (s == 0)
        return -FORJA_FS_ERR_FECHADO;
    codigo = forja_fs_avancar_busca(s);
    if (codigo != 1)
        return codigo;
    *tipo = forja_fs_tipo_de(s->busca.atributos);
    return forja_fs_nome_utf8(s->busca.nome, destino, n);
}

/* Fecha a busca de pasta. Sem retorno: handle ja fechado nao faz nada. */
FORJA_ES_API void forja_fs_pasta_fechar(uint64_t h)
{
    ForjaFsSlot *s = forja_fs_achar(h, FORJA_FS_TIPO_BUSCA);

    if (s != 0)
        forja_fs_liberar(s);
}

#elif !defined(RT_ALVO_LINUX)
/* Fora do Windows nada disto existe ainda (o POSIX e o M5). Todo caminho devolve "nao suportado". */
FORJA_ES_API uint64_t forja_fs_abrir(const char *caminho, int32_t modo, int32_t *erro) { (void)caminho; (void)modo; *erro = FORJA_FS_ERR_NAO_SUPORTADO; return 0; }
FORJA_ES_API int64_t forja_fs_ler(uint64_t h, uint8_t *destino, int64_t n, int32_t *erro) { (void)h; (void)destino; (void)n; *erro = FORJA_FS_ERR_NAO_SUPORTADO; return -1; }
FORJA_ES_API int64_t forja_fs_gravar(uint64_t h, const uint8_t *origem, int64_t n, int32_t *erro) { (void)h; (void)origem; (void)n; *erro = FORJA_FS_ERR_NAO_SUPORTADO; return -1; }
FORJA_ES_API int64_t forja_fs_posicionar(uint64_t h, int64_t desloc, int32_t origem, int32_t *erro) { (void)h; (void)desloc; (void)origem; *erro = FORJA_FS_ERR_NAO_SUPORTADO; return -1; }
FORJA_ES_API int32_t forja_fs_descarregar(uint64_t h, int32_t *erro) { (void)h; *erro = FORJA_FS_ERR_NAO_SUPORTADO; return -1; }
FORJA_ES_API void forja_fs_fechar(uint64_t h) { (void)h; }
FORJA_ES_API int32_t forja_fs_info(const char *caminho, int64_t *tamanho, int64_t *mtime_ns, int32_t *tipo) { (void)caminho; (void)tamanho; (void)mtime_ns; (void)tipo; return FORJA_FS_ERR_NAO_SUPORTADO; }
FORJA_ES_API int32_t forja_fs_remover(const char *caminho) { (void)caminho; return FORJA_FS_ERR_NAO_SUPORTADO; }
FORJA_ES_API int32_t forja_fs_renomear(const char *origem, const char *destino) { (void)origem; (void)destino; return FORJA_FS_ERR_NAO_SUPORTADO; }
FORJA_ES_API int32_t forja_fs_copiar(const char *origem, const char *destino) { (void)origem; (void)destino; return FORJA_FS_ERR_NAO_SUPORTADO; }
FORJA_ES_API int32_t forja_fs_criar_pasta(const char *caminho) { (void)caminho; return FORJA_FS_ERR_NAO_SUPORTADO; }
FORJA_ES_API int32_t forja_fs_remover_pasta(const char *caminho) { (void)caminho; return FORJA_FS_ERR_NAO_SUPORTADO; }
FORJA_ES_API uint64_t forja_fs_pasta_abrir(const char *caminho, int32_t *erro) { (void)caminho; *erro = FORJA_FS_ERR_NAO_SUPORTADO; return 0; }
FORJA_ES_API int64_t forja_fs_pasta_proximo(uint64_t h, uint8_t *destino, int64_t n, int32_t *tipo) { (void)h; (void)destino; (void)n; (void)tipo; return -FORJA_FS_ERR_NAO_SUPORTADO; }
FORJA_ES_API void forja_fs_pasta_fechar(uint64_t h) { (void)h; }
#endif

#pragma GCC pop_options

#endif
