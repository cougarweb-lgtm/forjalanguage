/*
 * rt_processo_linux.c - processo filho do std:process no Linux x86-64: as funcoes forja_processo_* de forja_rt_processo.h (M5 P28b-2), o espelho Linux do trecho Windows do mesmo header.
 *
 * Versao: 0.5.0
 * Changelog:
 *   0.5.0 - D-G3-L2b: forja_process_spawn real (Stdio por canal: null = /dev/null, pipe, inherit = o fd do pai, file = open O_TRUNC 0644; tudo aberto pelo pai ANTES do fork, o filho so faz dup2; stdin pipe/file,
 *           file sem caminho ou canal fora de 0..3 = entrada invalida 4) e forja_processo_iniciar vira o envoltorio com os canais 0x110. O stdin padrao agora e /dev/null (antes, um pipe sem escritor: o mesmo fim de arquivo).
 *           prl_colher passa o rusage ao wait4 (usuario, sistema, pico, hora do fim no slot); o slot guarda a hora do spawn. forja_processo_ler devolve -1 em canal que nao e pipe. Falha de open/exec grava o errno
 *           em rt_os_ultimo_erro (std:os.last_error; do exec so o equivalente do codigo neutro: nao achou = ENOENT, sem permissao = EACCES, entrada = EINVAL). pid, wait_any, peak_memory e cpu_times:
 *           rt_processo_espera_linux.c. Limite de filhos FICA em 8 (vazamento exige o 9o recusado).
 *   0.4.1 - D-G3-L2a (bloco B): prl_tabela deixa de usar FORJA_PRC_MAX (64 desde o bloco B no Windows) e passa a PRL_FILHOS_MAXIMO = 8, o limite de sempre do Linux (tests\nativo\linux\processo\vazamento).
 *   0.4.0 - D-G3-L2a (bloco B): stubs de falha de forja_process_pid, _wait_any, _peak_memory e _cpu_times (sem syscall; o L2b faz o real).
 *   0.3.0 - M5c-b P63 (D-R7): trava da tabela de processos (prl_trava: prl_reservar e prl_liberar_slot) e a trava do ambiente segura do prl_preparar ate logo depois do fork no pai (o envp/PATH apontam para o vetor vivo); wait4/kill/read fora da trava.
 *   0.2.0 - M5 P28b-2b: forja_processo_iniciar ganha limite_memoria (u64; 0 = sem limite): o filho chama setrlimit 160 (RLIMIT_AS = 9, atual = maximo) depois de ligar os canais e antes do chdir e do execve; o limite vale
 *           para o programa executado. Filho que estoura: o malloc/mmap dele falha e o codigo de saida e o do programa dele (dd/sh: 1; Forja: 101); o pai nao e afetado. setrlimit que falha = FORJA_PRC_ERR_OUTRO.
 *           O grupo do filho (setpgid) continua SEMPRE feito: kill e fechar matam o grupo como no 0.1.0.
 *   0.1.0 - M5 P28b-2: primeira versao. Mesmo contrato e mesmos codigos neutros do Windows (forja_rt_processo.h), por syscall direta: pipe2 293 (O_CLOEXEC), fork 57 (nao vfork nem clone), dup2 33, chdir 80,
 *           setpgid 109, execve 59, wait4 61, kill 62. Fluxo: o pai prepara tudo no heap ANTES do fork (argv pelo rt_linha_dividir, envp, buffer do PATH), porque o filho so usa syscalls; o filho faz
 *           setpgid(0,0), stdin = ponta de leitura de um pipe sem escritor, stdout e stderr = os pipes, chdir, execve (em cada pasta do PATH VIVO do pai quando o programa nao tem "/"), e se algo falha
 *           escreve o codigo neutro (4 bytes) no pipe de controle (O_CLOEXEC: o exec bem-sucedido o fecha, o pai le 0 bytes) e sai por exit_group CRU (nunca o flush do runtime). O pai deixa as pontas de
 *           leitura sem bloquear por ioctl FIONBIO (so a ponta do pai: pipe2 com O_NONBLOCK deixaria a escrita do filho nao bloqueante) e espera por wait4 WNOHANG + nanosleep de 1 ms.
 *           matar = kill(-pid, SIGKILL) no grupo do filho (o equivalente do Job do Windows); fechar mata o grupo, colhe o filho (sem zumbi) e fecha os canais. Morto por sinal = 128 + sinal.
 *           Diferencas de SO conhecidas (nao testadas): programa relativo com "/" e pasta de trabalho resolvem contra a PASTA NOVA (o Windows resolve contra a do pai); stdin/stdout/stderr do
 *           pai fechados antes do spawn (fd 0, 1 ou 2 livres) fazem o dup2 ser no-op e o fd fechar no exec; texto UTF-8 invalido o Linux entrega ao kernel (o Windows recusa).
 *           Nenhum /proc. Referencias indefinidas: sys_heap_alocar e sys_heap_liberar (objeto comum).
 *
 * Incluido por rt_linux_es.c depois de rt_es_linux.c (que traz forja_rt_processo.h, rt_sys.h e rt_syscall_linux.h) e de rt_linha.c; nao compila sozinho.
 */
#define PRL_O_CLOEXEC 0x80000LL
#define PRL_FIONBIO 0x5421LL
#define PRL_SIGKILL 9LL
#define PRL_RLIMIT_AS 9LL
#define PRL_WNOHANG 1LL
#define PRL_RELOGIO_MONOTONICO 1LL
#define PRL_SAIDA_FALHA 127LL
#define PRL_STILL_ACTIVE 259LL    /* o GetExitCodeProcess do Windows devolve 259 enquanto o filho roda */
#define PRL_MORTO_POR_SINAL 128LL /* codigo de saida de quem morreu por sinal: 128 + sinal */
#define PRL_PASSO_ESPERA_NS 1000000LL
#define PRL_NS_POR_MS 1000000LL
#define PRL_NS_POR_S 1000000000LL
#define PRL_MS_MAXIMO 1000000000LL /* 11 dias: evita estourar a conta em ns */
#define PRL_PATH_PADRAO "/usr/local/bin:/bin:/usr/bin"

#define PRL_RELOGIO_PAREDE 0LL    /* CLOCK_REALTIME: inicio e fim do cpu_times (desde 1970) */
#define PRL_AT_FDCWD (-100LL)
#define PRL_O_LEITURA_CLOEXEC 0x80000LL /* O_RDONLY | O_CLOEXEC: /dev/null do stdin */
#define PRL_O_ESCRITA_CLOEXEC 0x80001LL /* O_WRONLY | O_CLOEXEC: /dev/null de stdout e stderr */
#define PRL_O_ARQUIVO 0x80241LL         /* O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC: Stdio.file */
#define PRL_MODO_ARQUIVO 0644LL
#define PRL_DEV_NULL "/dev/null"
#define PRL_RUSAGE_PALAVRAS 18 /* struct rusage = 144 bytes: ru_utime (s, us) nas palavras 0-1, ru_stime (s, us) nas 2-3, ru_maxrss (KiB) na 4 */
#define PRL_RUSAGE_UTIME 0
#define PRL_RUSAGE_STIME 2
#define PRL_RUSAGE_MAXRSS 4
#define PRL_NS_POR_US 1000LL
#define PRL_BYTES_POR_KIB 1024LL
#define PRL_WAIT_ANY_INVALIDO (-2LL)
#define PRL_WAIT_ANY_OUTRO (-3LL)

#define PRL_EPERM 1LL
#define PRL_ENOENT 2LL
#define PRL_ESRCH 3LL
#define PRL_EAGAIN 11LL
#define PRL_EACCES 13LL
#define PRL_ENOTDIR 20LL
#define PRL_EINVAL 22LL

typedef struct {
    int64_t pid;
    int32_t canal[2]; /* leitura de stdout e de stderr; -1 = o canal nao e pipe */
    uint32_t geracao;
    int32_t usado;
    int32_t colhido; /* wait4 ja devolveu o filho: status e os campos abaixo valem */
    int32_t status;
    int64_t inicio_ns; /* hora de parede (desde 1970) do spawn */
    int64_t fim_ns;    /* hora de parede em que o wait4 colheu o filho; 0 enquanto roda */
    int64_t usuario_ns; /* rusage do wait4: so existe depois da colheita */
    int64_t sistema_ns;
    int64_t pico_bytes;
} PrlSlot;

/* Tudo o que o filho precisa, montado no heap pelo pai antes do fork. */
typedef struct {
    char *argbuf;
    char **argv;
    char *envbuf; /* so quando o ambiente foi substituido */
    char **envp;
    char *candidato; /* "<pasta do PATH>/<programa>" */
    const char *path;
    const char *pasta;
    int32_t com_barra;
    uint64_t limite_memoria; /* RLIMIT_AS do filho em bytes; 0 = sem limite */
    int32_t env_preso; /* a trava do ambiente (D-R7) esta tomada por este lancamento: o envp/path apontam para o vetor vivo */
    int32_t env_tomou;
    int32_t canais;        /* stdin | stdout << 4 | stderr << 8 (FORJA_PRC_CANAL_*) */
    const char *saida;     /* caminhos do Stdio.file de stdout e stderr */
    const char *saida_erro;
    int32_t alvo[3];       /* fd que o filho recebe (dup2) em 0, 1 e 2; -1 = herda o do pai */
    int32_t leitura[2];    /* pontas de leitura do pai (stdout, stderr); -1 = o canal nao e pipe */
} PrlLancamento;

/* Solta a trava do ambiente se este lancamento a segura (idempotente: o pai solta logo apos o fork; o filho so faz syscall ate o execve). */
static void prl_soltar_ambiente(PrlLancamento *l)
{
    if (l->env_preso) {
        l->env_preso = 0;
        sys_ambiente_destravar(l->env_tomou != 0);
    }
}

#define PRL_FILHOS_MAXIMO 8 /* o limite do Linux segue 8 (o FORJA_PRC_MAX do Windows e 64): tests\nativo\linux\processo\vazamento exige o 9o spawn recusado; wait_any aceita ate 8 aqui */
static PrlSlot prl_tabela[PRL_FILHOS_MAXIMO];

/* ---- texto ---- */

static int64_t prl_tam(const char *s)
{
    int64_t n = 0;

    while (s[n] != '\0')
        n++;
    return n;
}

static int32_t prl_tem_barra(const char *s)
{
    for (int64_t k = 0; s[k] != '\0'; k++) {
        if (s[k] == '/')
            return 1;
    }
    return 0;
}

static int32_t prl_prefixo(const char *s, const char *prefixo)
{
    for (int64_t k = 0; prefixo[k] != '\0'; k++) {
        if (s[k] != prefixo[k])
            return 0;
    }
    return 1;
}

/* O PATH vivo do pai (o CreateProcessW tambem procura no do pai). */
static const char *prl_achar_path(void)
{
    char **e = sys_ambiente_vetor();

    for (; e != 0 && *e != 0; e++) {
        if (prl_prefixo(*e, "PATH="))
            return *e + 5;
    }
    return PRL_PATH_PADRAO;
}

static void prl_liberar_bloco(void *p)
{
    if (p != 0)
        sys_heap_liberar(p);
}

static void prl_liberar_lancamento(PrlLancamento *l)
{
    prl_liberar_bloco(l->argbuf);
    prl_liberar_bloco(l->argv);
    prl_liberar_bloco(l->envbuf);
    if (l->envbuf != 0)
        prl_liberar_bloco(l->envp); /* sem envbuf o envp e o vetor vivo do pai: nao e nosso */
    prl_liberar_bloco(l->candidato);
}

/* "A=1\nB=2\n" -> envp com cada item terminado em NUL (itens vazios somem). */
static int32_t prl_montar_ambiente(PrlLancamento *l, const char *ambiente)
{
    int64_t n = prl_tam(ambiente);
    int64_t inicio = 0;
    int64_t itens = 0;

    l->envbuf = sys_heap_alocar(n + 2);
    l->envp = sys_heap_alocar((n + 2) * (int64_t)sizeof(char *));
    if (l->envbuf == 0 || l->envp == 0)
        return FORJA_PRC_ERR_OUTRO;
    for (int64_t k = 0; k <= n; k++) {
        if (ambiente[k] != '\n' && ambiente[k] != '\0') {
            l->envbuf[k] = ambiente[k];
            continue;
        }
        l->envbuf[k] = '\0';
        if (k > inicio)
            l->envp[itens++] = &l->envbuf[inicio];
        inicio = k + 1;
    }
    l->envp[itens] = 0;
    return 0;
}

/* Divide a linha, monta o ambiente e o buffer do PATH. 0 = pronto; senao o codigo neutro de erro. */
static int32_t prl_preparar(PrlLancamento *l, const char *linha, const char *ambiente, const char *pasta)
{
    int64_t n = prl_tam(linha);
    int32_t falha = 0;

    l->argbuf = sys_heap_alocar(2 * n + 2);
    l->argv = sys_heap_alocar((n + 2) * (int64_t)sizeof(char *));
    if (l->argbuf == 0 || l->argv == 0)
        return FORJA_PRC_ERR_OUTRO;
    rt_linha_dividir(linha, l->argbuf, l->argv);
    if (l->argv[0][0] == '\0')
        return FORJA_PRC_ERR_ENTRADA;
    l->pasta = pasta;
    l->com_barra = prl_tem_barra(l->argv[0]);
    l->envp = sys_ambiente_vetor();
    if (ambiente[0] != '\0')
        falha = prl_montar_ambiente(l, ambiente);
    if (falha == 0 && !l->com_barra) {
        l->path = prl_achar_path();
        l->candidato = sys_heap_alocar(prl_tam(l->path) + prl_tam(l->argv[0]) + 3);
        falha = l->candidato == 0 ? FORJA_PRC_ERR_OUTRO : 0;
    }
    return falha;
}

/* ---- filho: so syscalls ---- */

static int32_t prl_codigo_do_errno(int64_t retorno)
{
    switch (-retorno) {
    case PRL_ENOENT:
    case PRL_ENOTDIR:
        return FORJA_PRC_ERR_NAO_ACHOU;
    case PRL_EACCES:
    case PRL_EPERM:
        return FORJA_PRC_ERR_SEM_PERMISSAO;
    default:
        return FORJA_PRC_ERR_OUTRO;
    }
}

static int32_t prl_exec_direto(const char *caminho, const PrlLancamento *l)
{
    return prl_codigo_do_errno(sysl_chamar3(SYSL_EXECVE, (int64_t)caminho, (int64_t)l->argv, (int64_t)l->envp));
}

/* "<pasta>/<programa>" com a pasta de 'tam' bytes em p (0 = "."), terminado em NUL. */
static void prl_montar_candidato(char *saida, const char *p, int64_t tam, const char *programa)
{
    int64_t fim = 0;

    for (int64_t k = 0; k < tam; k++)
        saida[fim++] = p[k];
    if (tam == 0)
        saida[fim++] = '.';
    saida[fim++] = '/';
    for (int64_t k = 0; programa[k] != '\0'; k++)
        saida[fim++] = programa[k];
    saida[fim] = '\0';
}

/* Tenta o programa em cada pasta do PATH (pasta vazia = "."); devolve o pior erro: sem permissao se alguma pasta o achou sem poder executar, nao achou se nenhuma o tem. */
static int32_t prl_exec_no_path(const PrlLancamento *l)
{
    const char *p = l->path;
    const char *programa = l->argv[0];
    int32_t pior = FORJA_PRC_ERR_NAO_ACHOU;

    for (;;) {
        int64_t dir = 0;
        int32_t codigo;

        while (p[dir] != ':' && p[dir] != '\0')
            dir++;
        prl_montar_candidato(l->candidato, p, dir, programa);
        codigo = prl_exec_direto(l->candidato, l);
        if (codigo == FORJA_PRC_ERR_OUTRO)
            return codigo;
        if (codigo == FORJA_PRC_ERR_SEM_PERMISSAO)
            pior = codigo;
        if (p[dir] == '\0')
            return pior;
        p += dir + 1;
    }
}

/* Falha do filho antes do exec: o codigo neutro (4 bytes) vai ao pai pelo pipe de controle e o filho sai por exit_group cru (o flush do runtime e do pai, nao dele). */
static _Noreturn void prl_filho_falhar(int64_t fd_controle, int32_t codigo)
{
    sysl_chamar3(SYSL_WRITE, fd_controle, (int64_t)&codigo, (int64_t)sizeof codigo);
    sysl_chamar1(SYSL_EXIT_GROUP, PRL_SAIDA_FALHA);
    __builtin_unreachable();
}

/* Liga os fds que o pai abriu (l->alvo: /dev/null, pontas de escrita dos pipes, arquivos do Stdio.file) em 0, 1 e 2; alvo -1 (Stdio.inherit) deixa o fd do pai como esta. */
static void prl_filho_ligar_canais(const PrlLancamento *l, int64_t fd_controle)
{
    for (int32_t i = 0; i < 3; i++) {
        if (l->alvo[i] >= 0 && sysl_chamar2(SYSL_DUP2, l->alvo[i], i) < 0)
            prl_filho_falhar(fd_controle, FORJA_PRC_ERR_OUTRO);
    }
}

static _Noreturn void prl_filho(const PrlLancamento *l, int64_t fd_controle)
{
    sysl_chamar2(SYSL_SETPGID, 0, 0); /* grupo proprio: o kill(-pid) do pai pega o filho e os netos */
    prl_filho_ligar_canais(l, fd_controle);
    if (l->limite_memoria > 0) { /* o limite sobrevive ao execve; so o filho o recebe (o pai nunca chama setrlimit) */
        uint64_t limite[2] = {l->limite_memoria, l->limite_memoria};

        if (sysl_chamar2(SYSL_SETRLIMIT, PRL_RLIMIT_AS, (int64_t)limite) < 0)
            prl_filho_falhar(fd_controle, FORJA_PRC_ERR_OUTRO);
    }
    if (l->pasta[0] != '\0' && sysl_chamar1(SYSL_CHDIR, (int64_t)l->pasta) < 0)
        prl_filho_falhar(fd_controle, FORJA_PRC_ERR_OUTRO); /* como o ERROR_DIRECTORY do Windows */
    prl_filho_falhar(fd_controle, l->com_barra ? prl_exec_direto(l->argv[0], l) : prl_exec_no_path(l));
}

/* ---- pai ---- */

static int64_t prl_agora_ns(void)
{
    int64_t ts[2] = {0, 0};

    sysl_chamar2(SYSL_CLOCK_GETTIME, PRL_RELOGIO_MONOTONICO, (int64_t)ts);
    return ts[0] * PRL_NS_POR_S + ts[1];
}

/* Hora de parede em ns desde 1970 (o inicio e o fim do cpu_times). */
static int64_t prl_hora_ns(void)
{
    int64_t ts[2] = {0, 0};

    sysl_chamar2(SYSL_CLOCK_GETTIME, PRL_RELOGIO_PAREDE, (int64_t)ts);
    return ts[0] * PRL_NS_POR_S + ts[1];
}

static void prl_dormir_ns(int64_t ns)
{
    int64_t ts[2] = {0, ns};

    sysl_chamar2(SYSL_NANOSLEEP, (int64_t)ts, 0);
}

/* Posicao da tabela para o handle opaco, ou -1 (fechado, de outra geracao ou fora da faixa). */
static int32_t prl_posicao(uint64_t h)
{
    uint64_t pos = (h & 0xFFFFFFFFULL);

    if (pos == 0 || pos > PRL_FILHOS_MAXIMO)
        return -1;
    if (!prl_tabela[pos - 1].usado || prl_tabela[pos - 1].geracao != (uint32_t)(h >> 32))
        return -1;
    return (int32_t)(pos - 1);
}

/* Trava da tabela de processos (D-R7, como o forja_prc_trava do Windows): so o trecho que ocupa ou devolve um slot; kill, wait4, fork e read ficam fora dela. */
static SysTrava prl_trava; /* compartilhado: so com rt_multithread ligado */

static int32_t prl_reservar(void)
{
    bool tomou = sys_trava_pegar(&prl_trava);

    for (int32_t k = 0; k < PRL_FILHOS_MAXIMO; k++) {
        if (!prl_tabela[k].usado) {
            prl_tabela[k].usado = 1;
            prl_tabela[k].pid = 0;
            prl_tabela[k].colhido = 0;
            prl_tabela[k].status = 0;
            prl_tabela[k].canal[0] = -1;
            prl_tabela[k].canal[1] = -1;
            prl_tabela[k].inicio_ns = 0;
            prl_tabela[k].fim_ns = 0;
            prl_tabela[k].usuario_ns = 0;
            prl_tabela[k].sistema_ns = 0;
            prl_tabela[k].pico_bytes = 0;
            sys_trava_soltar(&prl_trava, tomou);
            return k;
        }
    }
    sys_trava_soltar(&prl_trava, tomou);
    return -1;
}

/* Devolve o slot (usado = 0) e sobe a geracao: o handle antigo deixa de valer. */
static void prl_liberar_slot(PrlSlot *s)
{
    bool tomou = sys_trava_pegar(&prl_trava);

    s->usado = 0;
    s->geracao++;
    sys_trava_soltar(&prl_trava, tomou);
}

static void prl_fechar_fd(int32_t fd)
{
    if (fd >= 0)
        sysl_chamar1(SYSL_CLOSE, fd);
}

/* struct timeval (segundos e microssegundos, 2 palavras) em ns. */
static int64_t prl_timeval_ns(const int64_t *tv)
{
    return tv[0] * PRL_NS_POR_S + tv[1] * PRL_NS_POR_US;
}

/* wait4 do filho do slot; so marca 'colhido' quando o kernel devolve o proprio pid, e nesse instante guarda o rusage (usuario, sistema, pico) e a hora do fim.
 * Devolve o retorno cru do wait4. O rusage do wait4 e o do filho e dos descendentes que ele colheu (ru_maxrss = o maior pico deles). */
static int64_t prl_colher(PrlSlot *s, int64_t opcoes)
{
    int32_t status = 0;
    int64_t uso[PRL_RUSAGE_PALAVRAS] = {0};
    int64_t v[7] = {SYSL_WAIT4, s->pid, (int64_t)&status, opcoes, (int64_t)uso, 0, 0};
    int64_t r = sysl_chamar6(v);

    if (r == s->pid) {
        s->colhido = 1;
        s->status = status;
        s->fim_ns = prl_hora_ns();
        s->usuario_ns = prl_timeval_ns(&uso[PRL_RUSAGE_UTIME]);
        s->sistema_ns = prl_timeval_ns(&uso[PRL_RUSAGE_STIME]);
        s->pico_bytes = uso[PRL_RUSAGE_MAXRSS] * PRL_BYTES_POR_KIB;
    }
    return r;
}

/* Mata o grupo, colhe o filho (bloqueia, sem zumbi) e devolve o slot. Fechar duas vezes e inofensivo (o handle ja nao vale). */
static void prl_encerrar(PrlSlot *s)
{
    if (s->pid > 0) { /* fork que falhou nao deixou filho */
        sysl_chamar2(SYSL_KILL, -s->pid, PRL_SIGKILL);
        while (!s->colhido && prl_colher(s, 0) == SYSL_EINTR) {
        }
    }
    prl_fechar_fd(s->canal[0]);
    prl_fechar_fd(s->canal[1]);
    prl_liberar_slot(s);
}

/* ---- canais do filho (Stdio): tudo aberto pelo pai ANTES do fork, para a falha voltar como erro do spawn ---- */

static int64_t prl_abrir(const char *caminho, int64_t flags, int64_t modo)
{
    int64_t v[7] = {SYSL_OPENAT, PRL_AT_FDCWD, (int64_t)caminho, flags, modo, 0, 0};

    return sysl_chamar6(v);
}

/* Canal 0 = stdin, 1 = stdout, 2 = stderr. stdin so aceita null e inherit; pipe e file so valem em stdout e stderr; file exige caminho; tipo fora de 0..3 e entrada invalida. */
static int32_t prl_validar_canais(int32_t canais, const char *saida, const char *saida_erro)
{
    int32_t tipo[3] = {canais & 0xF, (canais >> 4) & 0xF, (canais >> 8) & 0xF};
    const char *caminho[3] = {"", saida, saida_erro};

    if (canais < 0 || (canais >> 12) != 0)
        return FORJA_PRC_ERR_ENTRADA;
    if (tipo[0] == FORJA_PRC_CANAL_PIPE || tipo[0] == FORJA_PRC_CANAL_ARQUIVO)
        return FORJA_PRC_ERR_ENTRADA;
    for (int32_t i = 0; i < 3; i++) {
        if (tipo[i] > FORJA_PRC_CANAL_ARQUIVO)
            return FORJA_PRC_ERR_ENTRADA;
        if (tipo[i] == FORJA_PRC_CANAL_ARQUIVO && (caminho[i] == 0 || caminho[i][0] == '\0'))
            return FORJA_PRC_ERR_ENTRADA;
    }
    return 0;
}

/* Abre o destino do canal i: null = /dev/null, file = o caminho (criado e zerado, 0644), pipe = pipe2 (a ponta de leitura fica em l->leitura), inherit = nada (alvo -1).
 * 0 = ok com l->alvo[i] preenchido; senao -errno. */
static int64_t prl_abrir_canal(PrlLancamento *l, int32_t i, int32_t tipo, const char *caminho)
{
    int32_t par[2];
    int64_t r;

    switch (tipo) {
    case FORJA_PRC_CANAL_NULO:
        r = prl_abrir(PRL_DEV_NULL, i == 0 ? PRL_O_LEITURA_CLOEXEC : PRL_O_ESCRITA_CLOEXEC, 0);
        break;
    case FORJA_PRC_CANAL_ARQUIVO:
        r = prl_abrir(caminho, PRL_O_ARQUIVO, PRL_MODO_ARQUIVO);
        break;
    case FORJA_PRC_CANAL_PIPE:
        r = sysl_chamar2(SYSL_PIPE2, (int64_t)par, PRL_O_CLOEXEC);
        if (r == 0) {
            l->leitura[i - 1] = par[0];
            r = par[1];
        }
        break;
    default:
        return 0; /* inherit */
    }
    if (sysl_falhou(r))
        return r;
    l->alvo[i] = (int32_t)r;
    return 0;
}

/* O pai fecha as copias dos fds que o filho recebeu (so o filho fica com as pontas de escrita: o fim dos pipes depende disso). */
static void prl_fechar_alvos(PrlLancamento *l)
{
    for (int32_t i = 0; i < 3; i++) {
        prl_fechar_fd(l->alvo[i]);
        l->alvo[i] = -1;
    }
}

/* Falha ao abrir: fecha tudo o que ja abriu, inclusive as pontas de leitura. */
static void prl_fechar_canais(PrlLancamento *l)
{
    prl_fechar_alvos(l);
    for (int32_t k = 0; k < 2; k++) {
        prl_fechar_fd(l->leitura[k]);
        l->leitura[k] = -1;
    }
}

/* Valida e abre os tres canais. 0 = pronto; senao o codigo neutro, com o errno em rt_os_ultimo_erro (std:os.last_error) e nada aberto. */
static int32_t prl_abrir_canais(PrlLancamento *l)
{
    int32_t invalido = prl_validar_canais(l->canais, l->saida, l->saida_erro);
    const char *caminho[3] = {"", l->saida, l->saida_erro};

    for (int32_t i = 0; i < 3; i++) {
        l->alvo[i] = -1;
    }
    l->leitura[0] = -1;
    l->leitura[1] = -1;
    if (invalido != 0) {
        rt_os_ultimo_erro = PRL_EINVAL;
        return invalido;
    }
    for (int32_t i = 0; i < 3; i++) {
        int64_t r = prl_abrir_canal(l, i, (l->canais >> (4 * i)) & 0xF, caminho[i]);

        if (r != 0) {
            rt_os_ultimo_erro = -r;
            prl_fechar_canais(l);
            return prl_codigo_do_errno(r);
        }
    }
    return 0;
}

/* Falha do exec que o filho relatou (codigo neutro): grava o errno equivalente em rt_os_ultimo_erro. Sem libc o filho so devolve o codigo neutro (o pipe de controle leva 4 bytes), entao
 * "outro" nao tem errno certo e deixa o valor anterior. */
static void prl_registrar_falha_do_exec(int32_t codigo)
{
    if (codigo == FORJA_PRC_ERR_NAO_ACHOU)
        rt_os_ultimo_erro = PRL_ENOENT;
    else if (codigo == FORJA_PRC_ERR_SEM_PERMISSAO)
        rt_os_ultimo_erro = PRL_EACCES;
    else if (codigo == FORJA_PRC_ERR_ENTRADA)
        rt_os_ultimo_erro = PRL_EINVAL;
}

/* Espera o exec do filho: o pipe de controle fecha sem bytes (0) quando o exec deu certo; 4 bytes = codigo neutro da falha. */
static int32_t prl_ler_resultado_do_exec(int64_t fd)
{
    int32_t codigo = 0;
    int64_t r;

    do {
        r = sysl_chamar3(SYSL_READ, fd, (int64_t)&codigo, (int64_t)sizeof codigo);
    } while (r == SYSL_EINTR);
    if (r == (int64_t)sizeof codigo)
        return codigo;
    return r == 0 ? 0 : FORJA_PRC_ERR_OUTRO;
}

/* Cria o filho com os pipes feitos. Devolve o handle ou 0 com *erro. O fork e o ponto sem volta: no filho nada alem de syscall. */
static uint64_t prl_fork_e_aguardar(PrlLancamento *l, PrlSlot *s, int32_t pos, int32_t *erro)
{
    int32_t controle[2];
    int32_t um = 1;
    int64_t pid;

    *erro = prl_abrir_canais(l);
    if (*erro == 0 && sysl_chamar2(SYSL_PIPE2, (int64_t)controle, PRL_O_CLOEXEC) < 0) {
        prl_fechar_canais(l);
        *erro = FORJA_PRC_ERR_OUTRO;
    }
    if (*erro != 0) {
        prl_liberar_slot(s);
        return 0;
    }
    pid = sysl_chamar1(SYSL_FORK, 0);
    if (pid == 0)
        prl_filho(l, controle[1]);
    prl_soltar_ambiente(l);
    prl_fechar_alvos(l);
    prl_fechar_fd(controle[1]);
    s->pid = pid;
    s->canal[0] = l->leitura[0];
    s->canal[1] = l->leitura[1];
    s->inicio_ns = prl_hora_ns();
    *erro = pid < 0 ? FORJA_PRC_ERR_OUTRO : prl_ler_resultado_do_exec(controle[0]);
    prl_fechar_fd(controle[0]);
    if (*erro != 0) {
        prl_registrar_falha_do_exec(*erro);
        prl_encerrar(s);
        return 0;
    }
    for (int32_t k = 0; k < 2; k++) {
        if (s->canal[k] >= 0)
            sysl_chamar3(SYSL_IOCTL, s->canal[k], PRL_FIONBIO, (int64_t)&um);
    }
    return ((uint64_t)s->geracao << 32) | (uint64_t)(pos + 1);
}

/* LIMITE: parametros o mesmo externo de 8 argumentos do forja_rt_processo.h (contrato do std:process) */
FORJA_ES_API uint64_t forja_process_spawn(const char *linha, const char *ambiente, const char *pasta, int32_t *erro, uint64_t limite_memoria, int32_t canais,
                                          const char *saida, const char *saida_erro);

/* LIMITE: parametros contrato do std:process (4 do 0.1.x + limite de memoria: 5 argumentos) */
FORJA_ES_API uint64_t forja_processo_iniciar(const char *linha, const char *ambiente, const char *pasta, int32_t *erro, uint64_t limite_memoria)
{
    return forja_process_spawn(linha, ambiente, pasta, erro, limite_memoria, FORJA_PRC_CANAIS_PADRAO, "", "");
}

FORJA_ES_API int64_t forja_processo_ler(uint64_t h, int32_t canal, uint8_t *destino, int64_t n)
{
    int32_t pos = prl_posicao(h);
    int64_t r;

    if (pos < 0 || (canal != 0 && canal != 1) || n <= 0)
        return -2;
    if (prl_tabela[pos].canal[canal] < 0)
        return -1; /* o canal nao e pipe (null, inherit ou file): nao ha o que ler */
    do {
        r = sysl_chamar3(SYSL_READ, prl_tabela[pos].canal[canal], (int64_t)destino, n);
    } while (r == SYSL_EINTR);
    if (r > 0)
        return r;
    if (r == 0)
        return -1; /* todos os escritores fecharam e nao ha mais bytes */
    return r == -PRL_EAGAIN ? 0 : -2;
}

FORJA_ES_API int32_t forja_processo_esperar(uint64_t h, int64_t ms)
{
    int32_t pos = prl_posicao(h);
    PrlSlot *s = &prl_tabela[pos < 0 ? 0 : pos];
    int64_t limite;
    int64_t restante;
    int64_t r;

    if (pos < 0)
        return -1;
    limite = prl_agora_ns() + (ms < 0 ? 0 : (ms > PRL_MS_MAXIMO ? PRL_MS_MAXIMO : ms)) * PRL_NS_POR_MS;
    for (;;) {
        r = prl_colher(s, PRL_WNOHANG);
        if (s->colhido)
            return 1;
        if (r < 0 && r != SYSL_EINTR)
            return -1;
        restante = limite - prl_agora_ns();
        if (restante <= 0)
            return 0;
        prl_dormir_ns(restante < PRL_PASSO_ESPERA_NS ? restante : PRL_PASSO_ESPERA_NS);
    }
}

FORJA_ES_API int64_t forja_processo_codigo(uint64_t h)
{
    int32_t pos = prl_posicao(h);
    PrlSlot *s = &prl_tabela[pos < 0 ? 0 : pos];

    if (pos < 0)
        return -1;
    if (!s->colhido)
        prl_colher(s, PRL_WNOHANG);
    if (!s->colhido)
        return PRL_STILL_ACTIVE;
    if ((s->status & 0x7f) == 0)
        return (s->status >> 8) & 0xff;
    return PRL_MORTO_POR_SINAL + (s->status & 0x7f);
}

FORJA_ES_API int32_t forja_processo_matar(uint64_t h)
{
    int32_t pos = prl_posicao(h);
    int64_t r;

    if (pos < 0)
        return 0;
    r = sysl_chamar2(SYSL_KILL, -prl_tabela[pos].pid, PRL_SIGKILL);
    return r == 0 || r == -PRL_ESRCH ? 1 : 0; /* grupo ja vazio = nada a matar */
}

FORJA_ES_API void forja_processo_fechar(uint64_t h)
{
    int32_t pos = prl_posicao(h);

    if (pos >= 0)
        prl_encerrar(&prl_tabela[pos]);
}

/* D-G3-L2b: o spawn completo (a forja_processo_iniciar e o envoltorio com os canais 0x110, o default de sempre). Canais: null = /dev/null, pipe, inherit (o fd do pai), file (open O_TRUNC, 0644). */
/* LIMITE: parametros o mesmo externo de 8 argumentos do forja_rt_processo.h (contrato do std:process) */
FORJA_ES_API uint64_t forja_process_spawn(const char *linha, const char *ambiente, const char *pasta, int32_t *erro, uint64_t limite_memoria, int32_t canais,
                                          const char *saida, const char *saida_erro)
{
    PrlLancamento l = {0};
    int32_t pos = prl_reservar();
    uint64_t h = 0;

    if (pos < 0) {
        *erro = FORJA_PRC_ERR_OUTRO;
        return 0;
    }
    l.limite_memoria = limite_memoria;
    l.canais = canais;
    l.saida = saida;
    l.saida_erro = saida_erro;
    l.env_tomou = sys_ambiente_travar();
    l.env_preso = 1;
    *erro = prl_preparar(&l, linha, ambiente, pasta);
    if (*erro == 0)
        h = prl_fork_e_aguardar(&l, &prl_tabela[pos], pos, erro);
    else
        prl_liberar_slot(&prl_tabela[pos]);
    prl_soltar_ambiente(&l);
    prl_liberar_lancamento(&l);
    return h;
}

