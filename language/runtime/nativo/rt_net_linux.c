/*
 * rt_net_linux.c - UDP do std:net no Linux x86-64 (M5c-b P60): syscalls cruas (socket 41, bind 49, sendto 44, recvfrom 45, setsockopt 54, getsockname 51, close 3), sem libc e sem WSAStartup. Unidade propria (D-R3).
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - R9: TCP IPv4 (forja_net_tcp_ouvir, _aceitar, _conectar, _enviar, _receber, _esperar) com socket/bind/listen/accept4/connect/poll/getsockopt/fcntl/sendto/recvfrom crus; codigos novos NET_ERRO_CONEXAO_RECUSADA (9) e
 *           NET_ERRO_FECHADO_PELO_OUTRO (10). Conexao com prazo = socket nao bloqueante + poll(POLLOUT) + SO_ERROR, e volta a bloqueante. Envio com MSG_NOSIGNAL (EPIPE vira codigo, nunca SIGPIPE).
 *           forja_net_tcp_esperar usa poll (o MSG_PEEK do forja_net_esperar nao serve a socket em escuta). forja_net_local e forja_net_fechar servem tambem ao TCP.
 *   0.2.0 - M5c-b P60: forja_net_esperar olha a fila sem timeout primeiro (MSG_PEEK | MSG_DONTWAIT); so sem dado e com ms > 0 ajusta o SO_RCVTIMEO e bloqueia. Mesma resposta
 *           (1/0/erro); o volume_udp perdia datagramas no receptor com 3 syscalls por datagrama (setsockopt + 2 recvfrom).
 *   0.1.0 - M5c-b P60: as 6 funcoes forja_net_* de rt_net_win.c 0.2.0 (mesmos nomes, assinaturas e codigos NET_ERRO_*). Handle = fd + 1 (0 = erro, como no Windows).
 *           Mapa de erros igual ao do Windows: EAGAIN, ECONNRESET e ECONNREFUSED na leitura = timeout (sem dado); EMSGSIZE = NET_ERRO_DATAGRAMA_MAIOR;
 *           EINTR, EBADF e ENOTSOCK = fechado; bind: EADDRINUSE = porta em uso, EADDRNOTAVAIL = endereco invalido.
 *           forja_net_esperar: SO_RCVTIMEO = ms (ms 0 = MSG_DONTWAIT, ms < 0 = sem limite) e recvfrom com MSG_PEEK de 1 byte: o datagrama fica na fila para o receber.
 *           forja_net_receber: MSG_TRUNC devolve o tamanho real; maior que cap = NET_ERRO_DATAGRAMA_MAIOR (o Linux trunca em silencio sem a flag; o Windows recusa com WSAEMSGSIZE).
 *           SO_RCVBUF/SO_SNDBUF = 4 MiB no bind (falha ignorada), igual ao Windows.
 *
 * Os numeros dos erros e das flags sao os do Linux x86-64 (errno negativo no retorno da syscall). Socket so em 127.0.0.1 porta 0 nos testes (0.0.0.0 abre o Firewall no Windows).
 * Verificacao: construir.bat e python -I tests\nativo\m5c\net\verificar_net_p54.py <forja.exe> (e o verificar_m5c_linux.py no WSL).
 */
#include <stdint.h>

#include "rt_syscall_linux.h"

#define NET_ERRO_ENDERECO_INVALIDO 1
#define NET_ERRO_PORTA_EM_USO 2
#define NET_ERRO_TIMEOUT 3
#define NET_ERRO_FALHA_SOCKET 4
#define NET_ERRO_FALHA_ENVIO 5
#define NET_ERRO_FALHA_RECEBIMENTO 6
#define NET_ERRO_FECHADO 7
#define NET_ERRO_DATAGRAMA_MAIOR 8
#define NET_ERRO_CONEXAO_RECUSADA 9
#define NET_ERRO_FECHADO_PELO_OUTRO 10
#define NET_AF_INET 2
#define NET_SOCK_DGRAM 2
#define NET_SOCK_STREAM 1
#define NET_SOCK_NAO_BLOQUEANTE 0x800LL
#define NET_IPPROTO_UDP 17
#define NET_IPPROTO_TCP 6
#define NET_SO_REUSEADDR 2
#define NET_SO_ERROR 4
#define NET_MSG_NOSIGNAL 0x4000LL
#define NET_POLLIN 1
#define NET_POLLOUT 4
#define NET_F_SETFL 4
#define NET_TCP_TRECHO_MAXIMO 0x40000000LL /* um send/recv pede no maximo 1 GiB por chamada */
#define NET_E_EPIPE 32
#define NET_E_EINPROGRESS 115
#define NET_E_ETIMEDOUT 110
#define NET_SOL_SOCKET 1
#define NET_SO_SNDBUF 7
#define NET_SO_RCVBUF 8
#define NET_SO_RCVTIMEO 20
#define NET_MSG_PEEK 0x2LL
#define NET_MSG_TRUNC 0x20LL
#define NET_MSG_DONTWAIT 0x40LL
#define NET_DATAGRAMA_MAXIMO 65507
#define NET_BUFFER_BYTES 4194304 /* 4 MiB: pedido de buffer do SO; o sistema pode clampar */
#define NET_E_EINTR 4
#define NET_E_EBADF 9
#define NET_E_EAGAIN 11
#define NET_E_ENOTSOCK 88
#define NET_E_EMSGSIZE 90
#define NET_E_ECONNRESET 104
#define NET_E_EADDRINUSE 98
#define NET_E_EADDRNOTAVAIL 99
#define NET_E_ECONNREFUSED 111

typedef struct {
    uint16_t familia;
    uint16_t porta;
    uint8_t endereco[4];
    uint8_t zero[8];
} NetSockaddrIn;

/* struct timeval do SO_RCVTIMEO no x86-64 (dois inteiros de 64 bits). */
typedef struct {
    int64_t segundos;
    int64_t microssegundos;
} NetTimeval;

/* Preenche o sockaddr_in a partir do pacote (ip << 16 | porta): a porta vai em ordem de rede (troca dos bytes). */
static void NetPreencher(NetSockaddrIn *sa, uint64_t pacote)
{
    uint32_t ip = (uint32_t)(pacote >> 16);
    uint16_t porta = (uint16_t)(pacote & 0xFFFFu);
    int i;

    sa->familia = NET_AF_INET;
    sa->porta = (uint16_t)((porta >> 8) | (porta << 8));
    sa->endereco[0] = (uint8_t)(ip >> 24);
    sa->endereco[1] = (uint8_t)(ip >> 16);
    sa->endereco[2] = (uint8_t)(ip >> 8);
    sa->endereco[3] = (uint8_t)ip;
    for (i = 0; i < 8; i++)
        sa->zero[i] = 0;
}

/* Inverso de NetPreencher: o pacote de um sockaddr_in preenchido pelo sistema. */
static uint64_t NetEmpacotar(const NetSockaddrIn *sa)
{
    uint32_t ip = ((uint32_t)sa->endereco[0] << 24) | ((uint32_t)sa->endereco[1] << 16) | ((uint32_t)sa->endereco[2] << 8) | (uint32_t)sa->endereco[3];
    uint16_t porta = (uint16_t)((sa->porta >> 8) | (sa->porta << 8));

    return ((uint64_t)ip << 16) | porta;
}

/* O handle e fd + 1: o 0 fica para o erro, como no Windows (que nao tem fd 0 de SOCKET). */
static int64_t NetFd(uint64_t h)
{
    return (int64_t)(h - 1);
}

/* Traduz o retorno (-errno) de uma operacao: EINTR/EBADF/ENOTSOCK = socket fechado; EMSGSIZE = datagrama maior que o buffer; o resto = padrao. */
static int32_t NetErroDoRetorno(int64_t r, int32_t padrao)
{
    int64_t e = -r;

    if (e == NET_E_EINTR || e == NET_E_EBADF || e == NET_E_ENOTSOCK)
        return NET_ERRO_FECHADO;
    if (e == NET_E_EMSGSIZE)
        return NET_ERRO_DATAGRAMA_MAIOR;
    return padrao;
}

/* Falha de bind: EADDRINUSE = porta em uso; EADDRNOTAVAIL = endereco que nao e deste computador; o resto e falha do socket. */
static int32_t NetErroDeBind(int64_t r)
{
    if (-r == NET_E_EADDRINUSE)
        return NET_ERRO_PORTA_EM_USO;
    if (-r == NET_E_EADDRNOTAVAIL)
        return NET_ERRO_ENDERECO_INVALIDO;
    return NET_ERRO_FALHA_SOCKET;
}

/* Falha de recvfrom: EAGAIN, ECONNRESET e ECONNREFUSED viram timeout (sem dado), como no Windows (WSAEWOULDBLOCK e WSAECONNRESET). */
static int32_t NetErroDeRecebimento(int64_t r)
{
    int64_t e = -r;

    if (e == NET_E_EAGAIN || e == NET_E_ECONNRESET || e == NET_E_ECONNREFUSED)
        return NET_ERRO_TIMEOUT;
    return NetErroDoRetorno(r, NET_ERRO_FALHA_RECEBIMENTO);
}

/* Antes do bind: buffers de 4 MiB. Falha ignorada (o SO pode clampar; um fd invalido falha no bind com EBADF). */
static void NetAjustarSocket(int64_t fd)
{
    int32_t buffer = NET_BUFFER_BYTES;
    int64_t v[7];

    v[0] = SYSL_SETSOCKOPT;
    v[1] = fd;
    v[2] = NET_SOL_SOCKET;
    v[3] = NET_SO_RCVBUF;
    v[4] = (int64_t)(uintptr_t)&buffer;
    v[5] = (int64_t)sizeof buffer;
    v[6] = 0;
    (void)sysl_chamar6(v);
    v[3] = NET_SO_SNDBUF;
    (void)sysl_chamar6(v);
}

/* SO_RCVTIMEO do socket: ms > 0 = espera ate ms; ms <= 0 = 0 (sem limite; ms 0 quem trata e MSG_DONTWAIT no recvfrom). */
static int64_t NetDefinirTimeout(int64_t fd, int32_t ms)
{
    NetTimeval tempo;
    int64_t v[7];

    tempo.segundos = ms > 0 ? ms / 1000 : 0;
    tempo.microssegundos = ms > 0 ? (int64_t)(ms % 1000) * 1000 : 0;
    v[0] = SYSL_SETSOCKOPT;
    v[1] = fd;
    v[2] = NET_SOL_SOCKET;
    v[3] = NET_SO_RCVTIMEO;
    v[4] = (int64_t)(uintptr_t)&tempo;
    v[5] = (int64_t)sizeof tempo;
    v[6] = 0;
    return sysl_chamar6(v);
}

/* Cria o socket UDP IPv4 e liga ao pacote. Devolve o handle (0 = erro, e *erro diz qual). Sem WSAStartup: o Linux nao tem init de rede. */
uint64_t forja_net_ligar(uint64_t pacote, int32_t *erro)
{
    NetSockaddrIn sa;
    int64_t fd;
    int64_t r;

    *erro = 0;
    fd = sysl_chamar3(SYSL_SOCKET, NET_AF_INET, NET_SOCK_DGRAM, NET_IPPROTO_UDP);
    if (sysl_falhou(fd)) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    NetAjustarSocket(fd);
    NetPreencher(&sa, pacote);
    r = sysl_chamar3(SYSL_BIND, fd, (int64_t)(uintptr_t)&sa, (int64_t)sizeof sa);
    if (sysl_falhou(r)) {
        *erro = NetErroDeBind(r);
        sysl_chamar1(SYSL_CLOSE, fd);
        return 0;
    }
    return (uint64_t)fd + 1;
}

/* Endereco local do socket, empacotado em *saida. 0 = ok; senao o codigo NET_ERRO_*. */
int32_t forja_net_local(uint64_t h, uint64_t *saida)
{
    NetSockaddrIn sa;
    uint32_t tamanho = (uint32_t)sizeof sa;
    int64_t r;

    if (h == 0)
        return NET_ERRO_FECHADO;
    r = sysl_chamar3(SYSL_GETSOCKNAME, NetFd(h), (int64_t)(uintptr_t)&sa, (int64_t)(uintptr_t)&tamanho);
    if (sysl_falhou(r))
        return NET_ERRO_FALHA_SOCKET;
    *saida = NetEmpacotar(&sa);
    return 0;
}

/* Envia n bytes ao destino. >= 0 = bytes enviados; < 0 = -codigo NET_ERRO_*. */
int64_t forja_net_enviar(uint64_t h, uint64_t destino, const uint8_t *dados, int64_t n)
{
    NetSockaddrIn sa;
    int64_t v[7];
    int64_t r;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (n < 0 || n > NET_DATAGRAMA_MAXIMO)
        return -NET_ERRO_FALHA_ENVIO;
    NetPreencher(&sa, destino);
    v[0] = SYSL_SENDTO;
    v[1] = NetFd(h);
    v[2] = (int64_t)(uintptr_t)dados;
    v[3] = n;
    v[4] = 0;
    v[5] = (int64_t)(uintptr_t)&sa;
    v[6] = (int64_t)sizeof sa;
    r = sysl_chamar6(v);
    if (sysl_falhou(r))
        return -NetErroDoRetorno(r, NET_ERRO_FALHA_ENVIO);
    return r;
}

/* Olha o datagrama da fila sem tira-lo (MSG_PEEK de 1 byte). 1 = ha dado; 0 = nada agora; < 0 = -codigo NET_ERRO_*. */
static int32_t NetVigiar(int64_t fd, int64_t flags)
{
    uint8_t vigia[1];
    int64_t v[7];
    int64_t r;
    int32_t erro;

    v[0] = SYSL_RECVFROM;
    v[1] = fd;
    v[2] = (int64_t)(uintptr_t)vigia;
    v[3] = 1;
    v[4] = NET_MSG_PEEK | flags;
    v[5] = 0;
    v[6] = 0;
    r = sysl_chamar6(v);
    if (!sysl_falhou(r))
        return 1;
    erro = NetErroDeRecebimento(r);
    return erro == NET_ERRO_TIMEOUT ? 0 : -erro;
}

/* Espera ate ms milissegundos (ms < 0 = sem limite) ate o socket ter datagrama. 1 = pronto; 0 = expirou; < 0 = -codigo. O datagrama NAO sai da fila (MSG_PEEK).
 * Dado ja na fila responde sem tocar no SO_RCVTIMEO (uma syscall a menos por datagrama no caminho quente); so a espera real ajusta o timeout e bloqueia. */
int32_t forja_net_esperar(uint64_t h, int32_t ms)
{
    int32_t pronto;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    pronto = NetVigiar(NetFd(h), NET_MSG_DONTWAIT);
    if (pronto != 0 || ms == 0)
        return pronto;
    if (sysl_falhou(NetDefinirTimeout(NetFd(h), ms)))
        return -NET_ERRO_FALHA_RECEBIMENTO;
    return NetVigiar(NetFd(h), 0);
}

/* Recebe um datagrama em destino (cap bytes) e o remetente em *origem. >= 0 = bytes recebidos; < 0 = -codigo. Maior que cap = DATAGRAMA_MAIOR (o datagrama sai da fila, como no Windows). */
int64_t forja_net_receber(uint64_t h, uint8_t *destino, int64_t cap, uint64_t *origem)
{
    NetSockaddrIn sa;
    uint32_t tamanho = (uint32_t)sizeof sa;
    int64_t v[7];
    int64_t r;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (cap <= 0 || cap > NET_DATAGRAMA_MAXIMO)
        return -NET_ERRO_FALHA_RECEBIMENTO;
    v[0] = SYSL_RECVFROM;
    v[1] = NetFd(h);
    v[2] = (int64_t)(uintptr_t)destino;
    v[3] = cap;
    v[4] = NET_MSG_TRUNC;
    v[5] = (int64_t)(uintptr_t)&sa;
    v[6] = (int64_t)(uintptr_t)&tamanho;
    r = sysl_chamar6(v);
    if (sysl_falhou(r))
        return -NetErroDeRecebimento(r);
    if (r > cap)
        return -NET_ERRO_DATAGRAMA_MAIOR;
    *origem = NetEmpacotar(&sa);
    return r;
}

/* Fecha o socket. 0 = ok; senao o codigo NET_ERRO_*. */
int32_t forja_net_fechar(uint64_t h)
{
    if (h == 0)
        return NET_ERRO_FECHADO;
    return sysl_chamar1(SYSL_CLOSE, NetFd(h)) == 0 ? 0 : NET_ERRO_FALHA_SOCKET;
}

/* struct pollfd do Linux x86-64. */
typedef struct {
    int32_t fd;
    int16_t eventos;
    int16_t devolvidos;
} NetPollfd;

/* Falha de connect (errno positivo): ECONNREFUSED = conexao recusada; ETIMEDOUT = timeout; EADDRNOTAVAIL = endereco invalido; o resto = falha do socket. */
static int32_t NetErroDeConexao(int64_t e)
{
    if (e == NET_E_ECONNREFUSED)
        return NET_ERRO_CONEXAO_RECUSADA;
    if (e == NET_E_ETIMEDOUT)
        return NET_ERRO_TIMEOUT;
    if (e == NET_E_EADDRNOTAVAIL)
        return NET_ERRO_ENDERECO_INVALIDO;
    return NET_ERRO_FALHA_SOCKET;
}

/* Falha de sendto no TCP: o outro lado ter fechado (EPIPE, ECONNRESET) e codigo proprio; o resto segue o mapa comum. */
static int32_t NetErroDeEnvioTcp(int64_t r)
{
    if (-r == NET_E_EPIPE || -r == NET_E_ECONNRESET)
        return NET_ERRO_FECHADO_PELO_OUTRO;
    return NetErroDoRetorno(r, NET_ERRO_FALHA_ENVIO);
}

/* Falha de recvfrom no TCP: ECONNRESET/EPIPE = fechado pelo outro (nunca vira timeout, ao contrario do UDP); EAGAIN = timeout. */
static int32_t NetErroDeRecebimentoTcp(int64_t r)
{
    if (-r == NET_E_ECONNRESET || -r == NET_E_EPIPE)
        return NET_ERRO_FECHADO_PELO_OUTRO;
    if (-r == NET_E_EAGAIN)
        return NET_ERRO_TIMEOUT;
    return NetErroDoRetorno(r, NET_ERRO_FALHA_RECEBIMENTO);
}

/* poll de 1 socket por ms (ms < 0 = sem limite). 1 = pronto; 0 = expirou; < 0 = -codigo. EINTR repete a chamada. */
static int32_t NetSondar(int64_t fd, int16_t eventos, int32_t ms)
{
    NetPollfd p;
    int64_t r;

    p.fd = (int32_t)fd;
    p.eventos = eventos;
    p.devolvidos = 0;
    do {
        r = sysl_chamar3(SYSL_POLL, (int64_t)(uintptr_t)&p, 1, ms);
    } while (r == SYSL_EINTR);
    if (sysl_falhou(r))
        return -NET_ERRO_FALHA_SOCKET;
    return r > 0 ? 1 : 0;
}

/* Espera ate ms milissegundos por dado (ou conexao pendente, no socket em escuta). 1 = pronto; 0 = expirou; < 0 = -codigo. Fim da conexao tambem acorda (o receber devolve 0). */
int32_t forja_net_tcp_esperar(uint64_t h, int32_t ms)
{
    if (h == 0)
        return -NET_ERRO_FECHADO;
    return NetSondar(NetFd(h), NET_POLLIN, ms);
}

/* SO_REUSEADDR = 1 no socket em escuta (o reinicio do servidor nao espera o TIME_WAIT). Falha ignorada. */
static void NetReusarEndereco(int64_t fd)
{
    int32_t um = 1;
    int64_t v[7];

    v[0] = SYSL_SETSOCKOPT;
    v[1] = fd;
    v[2] = NET_SOL_SOCKET;
    v[3] = NET_SO_REUSEADDR;
    v[4] = (int64_t)(uintptr_t)&um;
    v[5] = (int64_t)sizeof um;
    v[6] = 0;
    (void)sysl_chamar6(v);
}

/* Socket TCP IPv4 em escuta no pacote. Devolve o handle (0 = erro, e *erro diz qual). */
uint64_t forja_net_tcp_ouvir(uint64_t pacote, int32_t backlog, int32_t *erro)
{
    NetSockaddrIn sa;
    int64_t fd;
    int64_t r;

    *erro = 0;
    fd = sysl_chamar3(SYSL_SOCKET, NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);
    if (sysl_falhou(fd)) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    NetReusarEndereco(fd);
    NetPreencher(&sa, pacote);
    r = sysl_chamar3(SYSL_BIND, fd, (int64_t)(uintptr_t)&sa, (int64_t)sizeof sa);
    if (sysl_falhou(r))
        *erro = NetErroDeBind(r);
    else if (sysl_falhou(sysl_chamar2(SYSL_LISTEN, fd, backlog)))
        *erro = NET_ERRO_FALHA_SOCKET;
    if (*erro != 0) {
        sysl_chamar1(SYSL_CLOSE, fd);
        return 0;
    }
    return (uint64_t)fd + 1;
}

/* Tira uma conexao da fila do socket em escuta (chamar depois de forja_net_tcp_esperar: sem conexao pendente bloqueia). Handle novo (0 = erro). */
uint64_t forja_net_tcp_aceitar(uint64_t h, int32_t *erro)
{
    int64_t v[7];
    int64_t r;

    *erro = 0;
    if (h == 0) {
        *erro = NET_ERRO_FECHADO;
        return 0;
    }
    v[0] = SYSL_ACCEPT4;
    v[1] = NetFd(h);
    v[2] = 0;
    v[3] = 0;
    v[4] = 0;
    v[5] = 0;
    v[6] = 0;
    r = sysl_chamar6(v);
    if (sysl_falhou(r)) {
        *erro = NetErroDoRetorno(r, NET_ERRO_FALHA_SOCKET);
        return 0;
    }
    return (uint64_t)r + 1;
}

/* Depois do connect nao bloqueante: espera POLLOUT por ms e le o SO_ERROR. 0 = conectado; senao o codigo NET_ERRO_*. */
static int32_t NetAguardarConexao(int64_t fd, int32_t ms)
{
    int32_t pronto = NetSondar(fd, NET_POLLOUT, ms);
    int32_t pendente = 0;
    uint32_t tamanho = (uint32_t)sizeof pendente;
    int64_t v[7];

    if (pronto < 0)
        return -pronto;
    if (pronto == 0)
        return NET_ERRO_TIMEOUT;
    v[0] = SYSL_GETSOCKOPT;
    v[1] = fd;
    v[2] = NET_SOL_SOCKET;
    v[3] = NET_SO_ERROR;
    v[4] = (int64_t)(uintptr_t)&pendente;
    v[5] = (int64_t)(uintptr_t)&tamanho;
    v[6] = 0;
    if (sysl_falhou(sysl_chamar6(v)))
        return NET_ERRO_FALHA_SOCKET;
    return pendente == 0 ? 0 : NetErroDeConexao(pendente);
}

/* Conecta ao pacote esperando ate ms (ms < 0 = sem limite). Handle do socket (0 = erro; *erro: recusada, timeout, endereco invalido...). O socket volta a ser bloqueante. */
uint64_t forja_net_tcp_conectar(uint64_t pacote, int32_t ms, int32_t *erro)
{
    NetSockaddrIn sa;
    int64_t fd;
    int64_t r;

    *erro = 0;
    fd = sysl_chamar3(SYSL_SOCKET, NET_AF_INET, NET_SOCK_STREAM | NET_SOCK_NAO_BLOQUEANTE, NET_IPPROTO_TCP);
    if (sysl_falhou(fd)) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    NetPreencher(&sa, pacote);
    r = sysl_chamar3(SYSL_CONNECT, fd, (int64_t)(uintptr_t)&sa, (int64_t)sizeof sa);
    if (r == -NET_E_EINPROGRESS)
        *erro = NetAguardarConexao(fd, ms);
    else if (sysl_falhou(r))
        *erro = NetErroDeConexao(-r);
    if (*erro == 0 && sysl_falhou(sysl_chamar3(SYSL_FCNTL, fd, NET_F_SETFL, 0)))
        *erro = NET_ERRO_FALHA_SOCKET;
    if (*erro != 0) {
        sysl_chamar1(SYSL_CLOSE, fd);
        return 0;
    }
    return (uint64_t)fd + 1;
}

/* Envia os n bytes todos (repete o sendto ate acabar). >= 0 = n; < 0 = -codigo NET_ERRO_* (o outro lado fechado = FECHADO_PELO_OUTRO). */
int64_t forja_net_tcp_enviar(uint64_t h, const uint8_t *dados, int64_t n)
{
    int64_t total = 0;
    int64_t v[7];
    int64_t r;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (n < 0)
        return -NET_ERRO_FALHA_ENVIO;
    while (total < n) {
        v[0] = SYSL_SENDTO;
        v[1] = NetFd(h);
        v[2] = (int64_t)(uintptr_t)(dados + total);
        v[3] = n - total > NET_TCP_TRECHO_MAXIMO ? NET_TCP_TRECHO_MAXIMO : n - total;
        v[4] = NET_MSG_NOSIGNAL;
        v[5] = 0;
        v[6] = 0;
        r = sysl_chamar6(v);
        if (r == SYSL_EINTR)
            continue;
        if (sysl_falhou(r))
            return -NetErroDeEnvioTcp(r);
        if (r == 0)
            return -NET_ERRO_FALHA_ENVIO;
        total += r;
    }
    return total;
}

/* Le ate cap bytes (chamar depois de forja_net_tcp_esperar). >= 0 = bytes lidos, 0 = o outro lado fechou; < 0 = -codigo NET_ERRO_*. */
int64_t forja_net_tcp_receber(uint64_t h, uint8_t *destino, int64_t cap)
{
    int64_t v[7];
    int64_t r;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (cap <= 0)
        return -NET_ERRO_FALHA_RECEBIMENTO;
    do {
        v[0] = SYSL_RECVFROM;
        v[1] = NetFd(h);
        v[2] = (int64_t)(uintptr_t)destino;
        v[3] = cap > NET_TCP_TRECHO_MAXIMO ? NET_TCP_TRECHO_MAXIMO : cap;
        v[4] = 0;
        v[5] = 0;
        v[6] = 0;
        r = sysl_chamar6(v);
    } while (r == SYSL_EINTR);
    if (sysl_falhou(r))
        return -NetErroDeRecebimentoTcp(r);
    return r;
}
