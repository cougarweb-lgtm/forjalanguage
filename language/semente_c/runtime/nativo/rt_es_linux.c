/*
 * rt_es_linux.c - handle e sistema de arquivos do std:file e entrada padrao do std:os no Linux x86-64: as funcoes forja_fs_* e forja_stdin_* de forja_rt_es.h e forja_rt_stdin.h (M5 P28b-1).
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - D-G3-L2b: FORJA_FS_MODO_TRAVA (file.lock; rt_es_trava_linux.c: flock 73 + unlink no close pelo caminho guardado no buffer do slot) e falha de open grava o errno em rt_os_ultimo_erro (std:os.last_error).
 *   0.2.0 - M5c-b P63 (D-R7): trava da tabela de handles (es_trava: es_guardar e es_liberar) e do stdin (es_stdin_trava segura a leitura inteira; forja_stdin_ler e _ler_linha viram cascas de _dentro); no-op com 1 thread.
 *   0.1.1 - M5 P28b-1: create em cima de uma pasta (EEXIST com o caminho sendo pasta) = DIRETORIO, como o Windows (CREATE_NEW em pasta = acesso negado); achado na checagem cruzada de es\handle.fj.
 *   0.1.0 - M5 P28b-1: primeira versao. Mesmos nomes, assinaturas e codigos neutros de erro do rt_es_win.c, por syscall direta (openat 257, read 0, write 1, lseek 8, fsync 74, close 3, newfstatat 262,
 *           unlink 87, rename 82, mkdir 83, rmdir 84, getdents64 217). Os erros tipados reproduzem os do Windows na mesma situacao (forja_fs_falha_arquivo e forja_fs_falha_pasta de forja_rt_es.h):
 *           pasta aberta em qualquer modo = DIRETORIO; ENOTDIR com o proprio caminho sendo arquivo = NAO_DIRETORIO; o resto pelo errno (ENOENT/ENOTDIR/ENAMETOOLONG = nao existe; EACCES/EPERM/EROFS = sem permissao).
 *           ler/gravar num handle aberto sem esse acesso (EBADF) e descarregar um handle so de leitura = sem permissao, como o Windows (acesso negado). Diferenca de SO conhecida (nao testada):
 *           caminho com UTF-8 invalido o Windows recusa (OUTRO) e o Linux entrega ao kernel.
 *           Buffers grandes (copy_file 64 KiB, getdents64 32 KiB) vem do heap, nunca da pilha (-Wframe-larger-than=4000). Tabela de 32 handles com geracao, como no Windows.
 *
 * Incluido por rt_linux_es.c (objeto rt_linux_es.o, compilado uma vez); nao compila sozinho. Referencias indefinidas: sys_heap_alocar, sys_heap_liberar (objeto comum) e rt_saida_descarregar (runtime).
 */
#define FORJA_ES_NATIVO 1
#include "rt_sys.h"
#include "rt_syscall_linux.h"
#include "../forja_rt_es.h"
#include "../forja_rt_stdin.h"
#include "../forja_rt_processo.h"

#define ES_AT_FDCWD (-100LL)
#define ES_AT_SEM_SEGUIR_LINK 0x100LL
#define ES_O_LER 0x80000LL      /* O_RDONLY | O_CLOEXEC */
#define ES_O_GRAVAR 0x80241LL   /* O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC */
#define ES_O_ANEXAR 0x80441LL   /* O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC */
#define ES_O_CRIAR 0x800C1LL    /* O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC */
#define ES_O_PASTA 0x90000LL    /* O_RDONLY | O_DIRECTORY | O_CLOEXEC */
#define ES_O_TRAVA 0x80041LL    /* O_WRONLY | O_CREAT | O_CLOEXEC: file.lock cria se faltar e NAO zera (o OPEN_ALWAYS do Windows) */
#define ES_LOCK_EX_NB 6LL       /* LOCK_EX | LOCK_NB */
#define ES_TENTATIVAS_TRAVA 3   /* abrir de novo se o dono anterior apagou o arquivo entre o open e o flock */
#define ES_MODO_NOVO 0666LL     /* o umask do processo recorta */
#define ES_MODO_PASTA_NOVA 0777LL
#define ES_MODO_PERMISSOES 07777LL

#define ES_EPERM 1LL
#define ES_ENOENT 2LL
#define ES_EBADF 9LL
#define ES_EAGAIN 11LL /* EWOULDBLOCK: o flock LOCK_NB de quem perdeu a disputa */
#define ES_EACCES 13LL
#define ES_EEXIST 17LL
#define ES_EXDEV 18LL
#define ES_ENOTDIR 20LL
#define ES_EISDIR 21LL
#define ES_EROFS 30LL
#define ES_ENAMETOOLONG 36LL
#define ES_ENOTEMPTY 39LL

#define ES_S_IFMT 0170000LL
#define ES_S_IFREG 0100000LL
#define ES_S_IFDIR 0040000LL
#define ES_S_IFLNK 0120000LL
/* struct stat do x86-64 como 18 palavras de 8 bytes: [3] = st_mode (+ st_uid), [6] = st_size, [11] = st_mtime, [12] = st_mtime_nsec. */
#define ES_STAT_PALAVRAS 18
#define ES_STAT_NLINK 2
#define ES_STAT_MODO 3
#define ES_STAT_TAMANHO 6
#define ES_STAT_MTIME 11
#define ES_STAT_MTIME_NS 12
#define ES_NS_POR_S 1000000000LL

#define ES_BLOCO_COPIA (64 * 1024)
#define ES_BLOCO_PASTA (32 * 1024)
/* linux_dirent64: d_ino 0, d_off 8, d_reclen 16 (u16), d_type 18, d_name 19. */
#define ES_DIRENT_RECLEN 16
#define ES_DIRENT_TIPO 18
#define ES_DIRENT_NOME 19
#define ES_DT_DESCONHECIDO 0
#define ES_DT_PASTA 4
#define ES_DT_ARQUIVO 8
#define ES_DT_LINK 10

typedef struct {
    int64_t h; /* fd + 1; 0 = posicao livre */
    uint32_t geracao;
    int32_t tipo;
    int32_t so_leitura; /* aberto so para ler: o fsync passa no Linux, o FlushFileBuffers recusa no Windows (sem permissao) */
    uint8_t *buffer; /* so busca de pasta: bloco do getdents64 */
    int64_t pos;
    int64_t fim;
} EsSlot;

extern int64_t rt_os_ultimo_erro; /* rt_os_linux.c (o objeto da variante): o errno que std:os.last_error devolve */

static EsSlot es_tabela[FORJA_FS_HANDLES_MAX];
/* Trava da tabela (D-R7, uma por grupo como o forja_fs_trava do Windows): so o trecho que ocupa ou solta uma posicao; nunca segura syscall nem o heap. */
static SysTrava es_trava; /* compartilhado: so com rt_multithread ligado */

/* ---- chamadas e erros ---- */

static int64_t es_abrir_fd(const char *caminho, int64_t flags, int64_t modo)
{
    int64_t v[7] = {SYSL_OPENAT, ES_AT_FDCWD, (int64_t)caminho, flags, modo, 0, 0};

    return sysl_chamar6(v);
}

static int64_t es_estatistica(int64_t dirfd, const char *caminho, int64_t *st)
{
    int64_t v[7] = {SYSL_NEWFSTATAT, dirfd, (int64_t)caminho, (int64_t)st, ES_AT_SEM_SEGUIR_LINK, 0, 0};

    return sysl_chamar6(v);
}

static int64_t es_mkdir(const char *caminho)
{
    return sysl_chamar2(SYSL_MKDIR, (int64_t)caminho, ES_MODO_PASTA_NOVA);
}

/* Tipo FileKind (FORJA_FS_TIPO_*) a partir do st_mode. */
static int32_t es_tipo_do_modo(int64_t modo)
{
    int64_t formato = modo & ES_S_IFMT;

    if (formato == ES_S_IFREG)
        return FORJA_FS_TIPO_ARQUIVO;
    if (formato == ES_S_IFDIR)
        return FORJA_FS_TIPO_PASTA;
    return formato == ES_S_IFLNK ? FORJA_FS_TIPO_LINK : FORJA_FS_TIPO_OUTRO;
}

/* Tipo do que esta no caminho, sem seguir link; -1 se nao existe ou nao da para ver. */
static int32_t es_tipo_caminho(const char *caminho)
{
    int64_t st[ES_STAT_PALAVRAS];

    if (es_estatistica(ES_AT_FDCWD, caminho, st) != 0)
        return -1;
    return es_tipo_do_modo(st[ES_STAT_MODO]);
}

/* -errno do kernel -> codigo neutro (a tabela do forja_fs_erro_win do Windows). */
static int32_t es_erro_base(int64_t retorno)
{
    switch (-retorno) {
    case ES_ENOENT:
    case ES_ENOTDIR:
    case ES_ENAMETOOLONG:
        return FORJA_FS_ERR_NAO_EXISTE;
    case ES_EACCES:
    case ES_EPERM:
    case ES_EROFS:
    case ES_EISDIR:
        return FORJA_FS_ERR_SEM_PERMISSAO;
    case ES_EEXIST:
        return FORJA_FS_ERR_JA_EXISTE;
    case ES_ENOTEMPTY:
        return FORJA_FS_ERR_NAO_VAZIO;
    default:
        return FORJA_FS_ERR_OUTRO;
    }
}

/* read/write num fd aberto sem esse acesso (EBADF) = sem permissao, como o ReadFile/WriteFile do Windows (acesso negado). */
static int32_t es_erro_acesso(int64_t retorno)
{
    return retorno == -ES_EBADF ? FORJA_FS_ERR_SEM_PERMISSAO : es_erro_base(retorno);
}

/* Falha em arquivo: acesso negado (EISDIR, EACCES, EPERM) sobre uma pasta vira DIRETORIO, como o forja_fs_falha_arquivo do Windows. */
static int32_t es_falha_arquivo(const char *caminho, int64_t retorno)
{
    int64_t e = -retorno;

    if ((e == ES_EISDIR || e == ES_EACCES || e == ES_EPERM || e == ES_EEXIST) && es_tipo_caminho(caminho) == FORJA_FS_TIPO_PASTA)
        return FORJA_FS_ERR_DIRETORIO;
    return es_erro_base(retorno);
}

/* Falha em pasta: arquivo no lugar de pasta vira NAO_DIRETORIO; EEXIST do rmdir (alguns sistemas de arquivos) = NAO_VAZIO. */
static int32_t es_falha_pasta(const char *caminho, int64_t retorno)
{
    if (es_tipo_caminho(caminho) == FORJA_FS_TIPO_ARQUIVO)
        return FORJA_FS_ERR_NAO_DIRETORIO;
    return retorno == -ES_EEXIST ? FORJA_FS_ERR_NAO_VAZIO : es_erro_base(retorno);
}

/* ---- tabela de handles (arquivo aberto ou busca de pasta) ---- */

/* Posicao do handle, ou 0 se nao vale (fechado, de outro tipo ou de outra geracao). */
static EsSlot *es_achar(uint64_t h, int32_t tipo)
{
    uint64_t posicao = h & 0xFFFFFFFFUL;
    EsSlot *s;

    if (posicao == 0 || posicao > FORJA_FS_HANDLES_MAX)
        return 0;
    s = &es_tabela[posicao - 1];
    if (s->h == 0 || s->tipo != tipo || (uint64_t)s->geracao != (h >> 32))
        return 0;
    return s;
}

/* Fecha o fd, solta o bloco da busca e sobe a geracao: o handle antigo deixa de valer. */
static void es_liberar(EsSlot *s)
{
    uint8_t *buffer = s->buffer;
    bool tomou;

    if (s->tipo == FORJA_FS_TIPO_ARQUIVO && buffer != 0) /* arquivo de file.lock: o buffer e o caminho; o DELETE_ON_CLOSE do Windows aqui e unlink ANTES do close (ainda com o flock) */
        sysl_chamar1(SYSL_UNLINK, (int64_t)buffer);
    sysl_chamar1(SYSL_CLOSE, s->h - 1);
    if (buffer != 0)
        sys_heap_liberar(buffer);
    tomou = sys_trava_pegar(&es_trava);
    s->buffer = 0;
    s->h = 0;
    s->geracao++;
    sys_trava_soltar(&es_trava, tomou);
}

/* Guarda o fd (+1) numa posicao livre e devolve o handle opaco. Tabela cheia: fecha o fd, solta o bloco e devolve 0 com OUTRO.
 * LIMITE: parametros 5 posicionais: 1 definicao e 3 chamadores (arquivo, pasta, stdin), sem struct para nao criar tipo de uso unico. */
static uint64_t es_guardar(int64_t fd, int32_t tipo, uint8_t *buffer, int32_t so_leitura, int32_t *erro)
{
    bool tomou = sys_trava_pegar(&es_trava);

    for (uint32_t i = 0; i < FORJA_FS_HANDLES_MAX; i++) {
        EsSlot *s = &es_tabela[i];

        if (s->h == 0) {
            s->h = fd + 1;
            s->tipo = tipo;
            s->buffer = buffer;
            s->so_leitura = so_leitura;
            s->pos = 0;
            s->fim = 0;
            sys_trava_soltar(&es_trava, tomou);
            return ((uint64_t)s->geracao << 32) | (uint64_t)(i + 1);
        }
    }
    sys_trava_soltar(&es_trava, tomou);
    sysl_chamar1(SYSL_CLOSE, fd);
    if (buffer != 0)
        sys_heap_liberar(buffer);
    *erro = FORJA_FS_ERR_OUTRO;
    return 0;
}

static EsSlot *es_arquivo_aberto(uint64_t h, int32_t *erro)
{
    EsSlot *s = es_achar(h, FORJA_FS_TIPO_ARQUIVO);

    if (s == 0)
        *erro = FORJA_FS_ERR_FECHADO;
    return s;
}

/* ---- handle de arquivo ---- */

static int64_t es_flags_do_modo(int32_t modo)
{
    switch (modo) {
    case FORJA_FS_MODO_LER:
        return ES_O_LER;
    case FORJA_FS_MODO_GRAVAR:
        return ES_O_GRAVAR;
    case FORJA_FS_MODO_ANEXAR:
        return ES_O_ANEXAR;
    case FORJA_FS_MODO_CRIAR:
        return ES_O_CRIAR;
    case FORJA_FS_MODO_TRAVA:
        return ES_O_TRAVA;
    default:
        return -1;
    }
}

/* O fd aberto e uma pasta? (open O_RDONLY de pasta funciona no Linux; o Windows recusa com acesso negado = DIRETORIO.) */
static bool es_fd_e_pasta(int64_t fd)
{
    int64_t st[ES_STAT_PALAVRAS];

    return sysl_chamar2(SYSL_FSTAT, fd, (int64_t)st) == 0 && es_tipo_do_modo(st[ES_STAT_MODO]) == FORJA_FS_TIPO_PASTA;
}

/* file.lock (rt_es_trava_linux.c, incluido depois deste arquivo): fd com o flock ou -errno; e o guardar do fd com o caminho no slot. */
static int64_t es_abrir_trava(const char *caminho);
static uint64_t es_guardar_trava(int64_t fd, const char *caminho, int32_t *erro);

/* Abre o arquivo: o handle opaco, ou 0 com *erro. Falha grava o errno em rt_os_ultimo_erro (std:os.last_error). */
uint64_t forja_fs_abrir(const char *caminho, int32_t modo, int32_t *erro)
{
    int64_t flags = es_flags_do_modo(modo);
    int64_t fd;

    if (flags < 0) {
        *erro = FORJA_FS_ERR_OUTRO;
        return 0;
    }
    if (caminho[0] == '\0') {
        *erro = FORJA_FS_ERR_NAO_EXISTE;
        rt_os_ultimo_erro = ES_ENOENT;
        return 0;
    }
    fd = modo == FORJA_FS_MODO_TRAVA ? es_abrir_trava(caminho) : es_abrir_fd(caminho, flags, ES_MODO_NOVO);
    if (sysl_falhou(fd)) {
        rt_os_ultimo_erro = -fd;
        *erro = modo == FORJA_FS_MODO_TRAVA && fd == -ES_EAGAIN ? FORJA_FS_ERR_OCUPADO : es_falha_arquivo(caminho, fd);
        return 0;
    }
    if (modo == FORJA_FS_MODO_TRAVA)
        return es_guardar_trava(fd, caminho, erro);
    if (modo == FORJA_FS_MODO_LER && es_fd_e_pasta(fd)) {
        sysl_chamar1(SYSL_CLOSE, fd);
        *erro = FORJA_FS_ERR_DIRETORIO;
        return 0;
    }
    return es_guardar(fd, FORJA_FS_TIPO_ARQUIVO, 0, modo == FORJA_FS_MODO_LER, erro);
}

/* Le ate n bytes (no maximo 1 GiB por chamada): quantos leu, 0 = fim do arquivo, -1 com *erro. */
int64_t forja_fs_ler(uint64_t h, uint8_t *destino, int64_t n, int32_t *erro)
{
    EsSlot *s = es_arquivo_aberto(h, erro);
    int64_t r;

    if (s == 0)
        return -1;
    if (n <= 0)
        return 0;
    if (n > (int64_t)FORJA_FS_LOTE_MAX)
        n = (int64_t)FORJA_FS_LOTE_MAX;
    do {
        r = sysl_chamar3(SYSL_READ, s->h - 1, (int64_t)destino, n);
    } while (r == SYSL_EINTR);
    if (sysl_falhou(r)) {
        *erro = es_erro_acesso(r);
        return -1;
    }
    return r;
}

/* Grava os n bytes inteiros (repete o write ate sair tudo): quantos gravou (= n) ou -1 com *erro. */
int64_t forja_fs_gravar(uint64_t h, const uint8_t *origem, int64_t n, int32_t *erro)
{
    EsSlot *s = es_arquivo_aberto(h, erro);
    int64_t feito = 0;

    if (s == 0)
        return -1;
    while (feito < n) {
        int64_t lote = n - feito > (int64_t)FORJA_FS_LOTE_MAX ? (int64_t)FORJA_FS_LOTE_MAX : n - feito;
        int64_t r = sysl_chamar3(SYSL_WRITE, s->h - 1, (int64_t)(origem + feito), lote);

        if (r == SYSL_EINTR)
            continue;
        if (sysl_falhou(r)) {
            *erro = es_erro_acesso(r);
            return -1;
        }
        if (r == 0) {
            *erro = FORJA_FS_ERR_OUTRO;
            return -1;
        }
        feito += r;
    }
    return feito;
}

/* Move o ponteiro (origem: inicio, atual ou fim = SEEK_SET, SEEK_CUR, SEEK_END): devolve a posicao nova ou -1 com *erro. */
int64_t forja_fs_posicionar(uint64_t h, int64_t desloc, int32_t origem, int32_t *erro)
{
    EsSlot *s = es_arquivo_aberto(h, erro);
    int64_t r;

    if (s == 0)
        return -1;
    if (origem < FORJA_FS_ORIGEM_INICIO || origem > FORJA_FS_ORIGEM_FIM) {
        *erro = FORJA_FS_ERR_OUTRO;
        return -1;
    }
    r = sysl_chamar3(SYSL_LSEEK, s->h - 1, desloc, origem);
    if (sysl_falhou(r)) {
        *erro = es_erro_base(r);
        return -1;
    }
    return r;
}

/* Descarrega o arquivo no disco (fsync): 0, ou -1 com *erro. */
int32_t forja_fs_descarregar(uint64_t h, int32_t *erro)
{
    EsSlot *s = es_arquivo_aberto(h, erro);
    int64_t r;

    if (s == 0)
        return -1;
    if (s->so_leitura) {
        *erro = FORJA_FS_ERR_SEM_PERMISSAO;
        return -1;
    }
    r = sysl_chamar1(SYSL_FSYNC, s->h - 1);
    if (sysl_falhou(r)) {
        *erro = es_erro_base(r);
        return -1;
    }
    return 0;
}

/* Fecha o handle. Sem retorno: um handle ja fechado ou invalido nao faz nada. */
void forja_fs_fechar(uint64_t h)
{
    EsSlot *s = es_achar(h, FORJA_FS_TIPO_ARQUIVO);

    if (s != 0)
        es_liberar(s);
}

/* ---- sistema de arquivos ---- */

/* Consulta o caminho sem seguir link: tamanho (so de arquivo regular; pasta e link = 0, como no Windows), mtime_ns (UTC desde 1970) e tipo. 0 ou o codigo neutro. */
int32_t forja_fs_info(const char *caminho, int64_t *tamanho, int64_t *mtime_ns, int32_t *tipo)
{
    int64_t st[ES_STAT_PALAVRAS];
    int64_t r;

    if (caminho[0] == '\0')
        return FORJA_FS_ERR_NAO_EXISTE;
    r = es_estatistica(ES_AT_FDCWD, caminho, st);
    if (r != 0)
        return es_erro_base(r);
    *tipo = es_tipo_do_modo(st[ES_STAT_MODO]);
    *tamanho = *tipo == FORJA_FS_TIPO_ARQUIVO ? st[ES_STAT_TAMANHO] : 0;
    *mtime_ns = st[ES_STAT_MTIME] * ES_NS_POR_S + st[ES_STAT_MTIME_NS];
    return 0;
}

/* Apaga um arquivo (nunca uma pasta: pasta devolve DIRETORIO). 0 ou o codigo neutro. */
int32_t forja_fs_remover(const char *caminho)
{
    int64_t r;

    if (caminho[0] == '\0')
        return FORJA_FS_ERR_NAO_EXISTE;
    r = sysl_chamar1(SYSL_UNLINK, (int64_t)caminho);
    return r == 0 ? 0 : es_falha_arquivo(caminho, r);
}

/* Copia o arquivo de origem para destino, sobrescrevendo, em blocos de 64 KiB do heap e com as permissoes da origem. Erro na origem = como o Windows (acesso negado
 * sobre pasta = DIRETORIO); erro no destino = so pelo errno (o Windows classifica pela origem, que e arquivo). 0 ou o codigo neutro.
 * LIMITE: cognitiva laco de leitura com EINTR, escrita parcial e dois fechamentos; separar duplicaria o estado do laco (blocos, contadores, fds). */
static int32_t es_copiar_blocos(int64_t entrada, int64_t saida)
{
    uint8_t *bloco = sys_heap_alocar(ES_BLOCO_COPIA);
    int32_t codigo = 0;

    if (bloco == 0)
        return FORJA_FS_ERR_OUTRO;
    for (;;) {
        int64_t lidos = sysl_chamar3(SYSL_READ, entrada, (int64_t)bloco, ES_BLOCO_COPIA);
        int64_t feitos = 0;

        if (lidos == SYSL_EINTR)
            continue;
        if (sysl_falhou(lidos))
            codigo = es_erro_base(lidos);
        if (lidos <= 0)
            break;
        while (feitos < lidos && codigo == 0) {
            int64_t r = sysl_chamar3(SYSL_WRITE, saida, (int64_t)(bloco + feitos), lidos - feitos);

            if (r == SYSL_EINTR)
                continue;
            if (sysl_falhou(r) || r == 0)
                codigo = sysl_falhou(r) ? es_erro_base(r) : FORJA_FS_ERR_OUTRO;
            else
                feitos += r;
        }
        if (codigo != 0)
            break;
    }
    sys_heap_liberar(bloco);
    return codigo;
}

int32_t forja_fs_copiar(const char *origem, const char *destino)
{
    int64_t st[ES_STAT_PALAVRAS];
    int64_t entrada;
    int64_t saida;
    int32_t codigo;

    if (origem[0] == '\0' || destino[0] == '\0')
        return FORJA_FS_ERR_NAO_EXISTE;
    entrada = es_abrir_fd(origem, ES_O_LER, 0);
    if (sysl_falhou(entrada))
        return es_falha_arquivo(origem, entrada);
    if (sysl_chamar2(SYSL_FSTAT, entrada, (int64_t)st) != 0) {
        sysl_chamar1(SYSL_CLOSE, entrada);
        return FORJA_FS_ERR_OUTRO;
    }
    if (es_tipo_do_modo(st[ES_STAT_MODO]) == FORJA_FS_TIPO_PASTA) {
        sysl_chamar1(SYSL_CLOSE, entrada);
        return FORJA_FS_ERR_DIRETORIO;
    }
    saida = es_abrir_fd(destino, ES_O_GRAVAR, st[ES_STAT_MODO] & ES_MODO_PERMISSOES);
    if (sysl_falhou(saida)) {
        sysl_chamar1(SYSL_CLOSE, entrada);
        return es_erro_base(saida);
    }
    codigo = es_copiar_blocos(entrada, saida);
    sysl_chamar1(SYSL_CLOSE, entrada);
    sysl_chamar1(SYSL_CLOSE, saida);
    return codigo;
}

/* Renomeia de origem para destino, sobrescrevendo o destino. Entre sistemas de arquivos (EXDEV) de arquivo regular, copia e apaga a origem, como o MoveFileExW. 0 ou o codigo neutro. */
int32_t forja_fs_renomear(const char *origem, const char *destino)
{
    int64_t r;
    int32_t codigo;

    if (origem[0] == '\0' || destino[0] == '\0')
        return FORJA_FS_ERR_NAO_EXISTE;
    r = sysl_chamar2(SYSL_RENAME, (int64_t)origem, (int64_t)destino);
    if (r == 0)
        return 0;
    if (r == -ES_EXDEV && es_tipo_caminho(origem) == FORJA_FS_TIPO_ARQUIVO) {
        codigo = forja_fs_copiar(origem, destino);
        if (codigo == 0)
            sysl_chamar1(SYSL_UNLINK, (int64_t)origem);
        return codigo;
    }
    if (r == -ES_ENOTDIR && es_tipo_caminho(origem) == FORJA_FS_TIPO_PASTA)
        return FORJA_FS_ERR_DIRETORIO;
    return es_falha_arquivo(origem, r);
}

/* Cria uma pasta (so o ultimo nivel). 0 ou o codigo neutro. */
int32_t forja_fs_criar_pasta(const char *caminho)
{
    int64_t r;

    if (caminho[0] == '\0')
        return FORJA_FS_ERR_NAO_EXISTE;
    r = es_mkdir(caminho);
    return r == 0 ? 0 : es_erro_base(r);
}

/* Remove uma pasta vazia: cheia = NAO_VAZIO, arquivo = NAO_DIRETORIO. 0 ou o codigo neutro. */
int32_t forja_fs_remover_pasta(const char *caminho)
{
    int64_t r;

    if (caminho[0] == '\0')
        return FORJA_FS_ERR_NAO_EXISTE;
    r = sysl_chamar1(SYSL_RMDIR, (int64_t)caminho);
    return r == 0 ? 0 : es_falha_pasta(caminho, r);
}

/* ---- pastas ---- */

/* Abre a pasta para listar (sem "." nem ".."). O handle e a busca; o bloco do getdents64 e do heap. 0 = handle opaco, ou 0 com *erro. */
uint64_t forja_fs_pasta_abrir(const char *caminho, int32_t *erro)
{
    uint8_t *bloco;
    int64_t fd;

    if (caminho[0] == '\0') {
        *erro = FORJA_FS_ERR_NAO_EXISTE;
        return 0;
    }
    fd = es_abrir_fd(caminho, ES_O_PASTA, 0);
    if (sysl_falhou(fd)) {
        *erro = es_falha_pasta(caminho, fd);
        return 0;
    }
    bloco = sys_heap_alocar(ES_BLOCO_PASTA);
    if (bloco == 0) {
        sysl_chamar1(SYSL_CLOSE, fd);
        *erro = FORJA_FS_ERR_OUTRO;
        return 0;
    }
    return es_guardar(fd, FORJA_FS_TIPO_BUSCA, bloco, 0, erro);
}

static bool es_nome_oculto(const char *nome)
{
    if (nome[0] != '.')
        return false;
    return nome[1] == '\0' || (nome[1] == '.' && nome[2] == '\0');
}

/* Tipo do item pelo d_type; DT_UNKNOWN (alguns sistemas de arquivos, como o 9p) consulta o item relativo ao fd da pasta. */
static int32_t es_tipo_do_item(const EsSlot *s, uint8_t dtipo, const char *nome)
{
    int64_t st[ES_STAT_PALAVRAS];

    if (dtipo == ES_DT_ARQUIVO)
        return FORJA_FS_TIPO_ARQUIVO;
    if (dtipo == ES_DT_PASTA)
        return FORJA_FS_TIPO_PASTA;
    if (dtipo == ES_DT_LINK)
        return FORJA_FS_TIPO_LINK;
    if (dtipo == ES_DT_DESCONHECIDO && es_estatistica(s->h - 1, nome, st) == 0)
        return es_tipo_do_modo(st[ES_STAT_MODO]);
    return FORJA_FS_TIPO_OUTRO;
}

/* Proximo registro do bloco, enchendo o bloco com getdents64 quando acaba. 1 = registro em *registro; 0 = fim; negativo = -codigo. */
static int32_t es_proximo_registro(EsSlot *s, const uint8_t **registro)
{
    int64_t tam_registro;

    if (s->pos >= s->fim) {
        int64_t r;

        do {
            r = sysl_chamar3(SYSL_GETDENTS64, s->h - 1, (int64_t)s->buffer, ES_BLOCO_PASTA);
        } while (r == SYSL_EINTR);
        if (sysl_falhou(r))
            return -es_erro_base(r);
        if (r == 0)
            return 0;
        s->pos = 0;
        s->fim = r;
    }
    *registro = s->buffer + s->pos;
    tam_registro = (int64_t)(*registro)[ES_DIRENT_RECLEN] | ((int64_t)(*registro)[ES_DIRENT_RECLEN + 1] << 8);
    s->pos += tam_registro;
    return 1;
}

/* Proximo item da pasta: tamanho do nome em UTF-8 no destino (com NUL logo depois; 1024 bytes bastam), 0 = fim, negativo = -codigo. *tipo recebe o FileKind. */
int64_t forja_fs_pasta_proximo(uint64_t h, uint8_t *destino, int64_t n, int32_t *tipo)
{
    EsSlot *s = es_achar(h, FORJA_FS_TIPO_BUSCA);

    if (s == 0)
        return -FORJA_FS_ERR_FECHADO;
    for (;;) {
        const uint8_t *registro = 0;
        const char *nome;
        int64_t tam = 0;
        int32_t achou = es_proximo_registro(s, &registro);

        if (achou <= 0)
            return achou;
        nome = (const char *)(registro + ES_DIRENT_NOME);
        if (es_nome_oculto(nome))
            continue;
        while (nome[tam] != '\0')
            tam++;
        if (tam + 1 > n)
            return -FORJA_FS_ERR_OUTRO;
        for (int64_t i = 0; i <= tam; i++)
            destino[i] = (uint8_t)nome[i];
        *tipo = es_tipo_do_item(s, registro[ES_DIRENT_TIPO], nome);
        return tam;
    }
}

/* Fecha a busca de pasta. Sem retorno: handle ja fechado nao faz nada. */
void forja_fs_pasta_fechar(uint64_t h)
{
    EsSlot *s = es_achar(h, FORJA_FS_TIPO_BUSCA);

    if (s != 0)
        es_liberar(s);
}

/* ---- entrada padrao (D7): buffer interno de 64 KiB partilhado por ler e ler_linha ---- */

static uint8_t es_stdin_buffer[FORJA_STDIN_BUF];
static int64_t es_stdin_pos;
static int64_t es_stdin_fim;

/* Garante dado no buffer (o stdout sai antes de bloquear): 1 = ha dado; 0 = fim (ou fd 0 fechado); -1 = erro. EINTR repete. */
static int64_t es_stdin_garantir(void)
{
    int64_t r;

    if (es_stdin_pos < es_stdin_fim)
        return 1;
    FORJA_ES_DESCARREGAR_SAIDA();
    do {
        r = sysl_chamar3(SYSL_READ, 0, (int64_t)es_stdin_buffer, FORJA_STDIN_BUF);
    } while (r == SYSL_EINTR);
    es_stdin_pos = 0;
    es_stdin_fim = 0;
    if (sysl_falhou(r))
        return r == -ES_EBADF ? 0 : -1;
    es_stdin_fim = r;
    return r > 0 ? 1 : 0;
}

/* Trava do stdin (D-R7, como o forja_stdin_trava do Windows): o buffer e um so, entao ela segura a leitura inteira, inclusive o read bloqueante. */
static SysTrava es_stdin_trava; /* compartilhado: so com rt_multithread ligado */

/* Ate n bytes crus: 0 = fim da entrada; -1 = erro (tambem para n <= 0). */
static int64_t es_stdin_ler_dentro(uint8_t *destino, int64_t n)
{
    int64_t r;
    int64_t k;

    if (n <= 0)
        return -1;
    r = es_stdin_garantir();
    if (r <= 0)
        return r;
    k = es_stdin_fim - es_stdin_pos;
    if (k > n)
        k = n;
    for (int64_t i = 0; i < k; i++)
        destino[i] = es_stdin_buffer[es_stdin_pos + i];
    es_stdin_pos += k;
    return k;
}

/* Copia do buffer ate LF (consumido, nao copiado), ate encher n ou esgotar o buffer. 1 = achou LF. */
static int32_t es_stdin_varrer(uint8_t *destino, int64_t n, int64_t *k)
{
    while (es_stdin_pos < es_stdin_fim && *k < n) {
        uint8_t c = es_stdin_buffer[es_stdin_pos++];

        if (c == FORJA_STDIN_LF)
            return 1;
        destino[(*k)++] = c;
    }
    return 0;
}

/* A linha SEM o LF e SEM um CR logo antes dele. Linha maior que n vem em partes (cada uma com *achou_lf = 0; um CR no fim de uma parte cheia volta ao buffer para sair junto do seu LF).
 * 0 com *achou_lf = 0 = fim; 0 com *achou_lf = 1 = linha vazia; -1 = erro (tambem para n < 2).
 * LIMITE: cognitiva mesma logica do forja_stdin_ler_linha do Windows (CR antes do LF, linha em partes); fica espelhada para a paridade ser conferivel. */
static int64_t es_stdin_ler_linha_dentro(uint8_t *destino, int64_t n, int32_t *achou_lf)
{
    int64_t k = 0;

    *achou_lf = 0;
    if (n < 2)
        return -1;
    for (;;) {
        int64_t r = es_stdin_garantir();

        if (r < 0)
            return -1;
        if (r == 0)
            return k;
        if (es_stdin_varrer(destino, n, &k)) {
            if (k > 0 && destino[k - 1] == FORJA_STDIN_CR)
                k--;
            *achou_lf = 1;
            return k;
        }
        if (k == n) {
            if (destino[k - 1] == FORJA_STDIN_CR) {
                es_stdin_pos--;
                k--;
            }
            return k;
        }
    }
}

int64_t forja_stdin_ler(uint8_t *destino, int64_t n)
{
    bool tomou = sys_trava_pegar(&es_stdin_trava);
    int64_t r = es_stdin_ler_dentro(destino, n);

    sys_trava_soltar(&es_stdin_trava, tomou);
    return r;
}

int64_t forja_stdin_ler_linha(uint8_t *destino, int64_t n, int32_t *achou_lf)
{
    bool tomou = sys_trava_pegar(&es_stdin_trava);
    int64_t r = es_stdin_ler_linha_dentro(destino, n, achou_lf);

    sys_trava_soltar(&es_stdin_trava, tomou);
    return r;
}
