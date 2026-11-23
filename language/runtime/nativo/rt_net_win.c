/*
 * rt_net_win.c - UDP do std:net no Windows x64 (M5 P53). Unidade propria (D-R8): nao muda rt_win_dev.o, rt_win_rel.o, rt_win_ryu.o nem rt_win_es.o.
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - R9: TCP IPv4 (forja_net_tcp_ouvir, _aceitar, _conectar, _enviar, _receber, _esperar); connect, listen, accept, send, recv, ioctlsocket e getsockopt entram na tabela fixa da WS2_32 do ligador
 *           (ligar_tabelas.c 0.9.0, 18 imports). Codigos novos NET_ERRO_CONEXAO_RECUSADA (9) e NET_ERRO_FECHADO_PELO_OUTRO (10). Conexao com prazo = FIONBIO + connect + select (escrita e excecao) + SO_ERROR, e volta a bloqueante.
 *           forja_net_tcp_esperar = forja_net_esperar (select); forja_net_local e forja_net_fechar servem tambem ao TCP.
 *   0.2.0 - M5 P54: SIO_UDP_CONNRESET = FALSE no bind (WSAIoctl) e SO_RCVBUF/SO_SNDBUF = 4 MiB (setsockopt, falha ignorada); WSAIoctl e setsockopt entram na
 *           tabela fixa da WS2_32 do ligador (ligar_tabelas.c 0.7.0, 11 imports). Erros mapeados: WSAEWOULDBLOCK/WSAECONNRESET -> timeout (sem dado), WSAEMSGSIZE -> NET_ERRO_DATAGRAMA_MAIOR (8), WSAEINTR/WSAENOTSOCK -> fechado.
 *   0.1.0 - M5 P53: forja_net_ligar, forja_net_local, forja_net_enviar, forja_net_esperar, forja_net_receber e forja_net_fechar sobre ws2_32
 *           (socket, bind, sendto, recvfrom, select, getsockname, closesocket, WSAStartup, WSAGetLastError). WSAStartup 1x sob o SRW do runtime
 *           (sys_trava_pegar). Enderecos vao num u64: ip (32 bits) << 16 | porta; erros, nos codigos NET_ERRO_* (iguais aos ERRO_* de std\net.fj).
 *
 * Os prototipos sao a mao, sem winsock2.h (como rt_sync_win.c). Objeto so entra na ligacao quando o programa referencia forja_net_* (RuntimeUsaRyu).
 * Verificacao: construir.bat e python -I tests\nativo\m5c\net\verificar_net_p53.py <forja.exe>.
 */
#include <stdbool.h>
#include <stdint.h>
#include "rt_sys.h"

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
#define NET_IPPROTO_UDP 17
#define NET_IPPROTO_TCP 6
#define NET_FIONBIO 0x8004667Eu /* _IOW('f', 126, u_long): 1 = nao bloqueante, 0 = bloqueante */
#define NET_SO_ERROR 0x1007
#define NET_TCP_TRECHO_MAXIMO 0x40000000 /* um send/recv pede no maximo 1 GiB por chamada */
#define NET_WSA_ECONNABORTED 10053
#define NET_WSA_ESHUTDOWN 10058
#define NET_WSA_ETIMEDOUT 10060
#define NET_WSA_ECONNREFUSED 10061
#define NET_SOCKET_INVALIDO (~(uintptr_t)0)
#define NET_WSA_ENDERECO_EM_USO 10048
#define NET_WSA_ENDERECO_INVALIDO 10049
#define NET_WSA_EINTR 10004
#define NET_WSA_ENOTSOCK 10038
#define NET_WSA_EWOULDBLOCK 10035
#define NET_WSA_EMSGSIZE 10040
#define NET_WSA_ECONNRESET 10054
#define NET_DATAGRAMA_MAXIMO 65507
#define NET_SIO_UDP_CONNRESET 0x9800000Cu /* _WSAIOW(IOC_VENDOR,12): BOOL FALSE desliga o ICMP que vira WSAECONNRESET no recv */
#define NET_SOL_SOCKET 0xFFFF
#define NET_SO_SNDBUF 0x1001
#define NET_SO_RCVBUF 0x1002
#define NET_BUFFER_BYTES 4194304 /* 4 MiB: pedido de buffer do SO; o sistema pode clampar */

typedef struct {
    uint16_t familia;
    uint16_t porta;
    uint8_t endereco[4];
    uint8_t zero[8];
} NetSockaddrIn;

typedef struct {
    long segundos;
    long microssegundos;
} NetTimeval;

typedef struct {
    uint32_t quantos;
    uintptr_t soquete[1];
} NetConjunto;

__declspec(dllimport) int WSAStartup(uint16_t versao, void *dados);
__declspec(dllimport) int WSAGetLastError(void);
__declspec(dllimport) uintptr_t socket(int familia, int tipo, int protocolo);
__declspec(dllimport) int bind(uintptr_t s, const void *nome, int tamanho);
__declspec(dllimport) int sendto(uintptr_t s, const char *dados, int n, int sinal, const void *destino, int tamanho);
__declspec(dllimport) int recvfrom(uintptr_t s, char *dados, int n, int sinal, void *origem, int *tamanho);
__declspec(dllimport) int select(int ignorado, NetConjunto *leitura, void *escrita, void *excecao, const NetTimeval *tempo);
__declspec(dllimport) int getsockname(uintptr_t s, void *nome, int *tamanho);
__declspec(dllimport) int closesocket(uintptr_t s);
__declspec(dllimport) int WSAIoctl(uintptr_t s, uint32_t codigo, void *entrada, uint32_t tamanho_entrada, void *saida, uint32_t tamanho_saida, uint32_t *devolvido, void *sobreposto, void *rotina);
__declspec(dllimport) int setsockopt(uintptr_t s, int nivel, int opcao, const char *valor, int tamanho);
__declspec(dllimport) int connect(uintptr_t s, const void *nome, int tamanho);
__declspec(dllimport) int listen(uintptr_t s, int pendentes);
__declspec(dllimport) uintptr_t accept(uintptr_t s, void *nome, int *tamanho);
__declspec(dllimport) int send(uintptr_t s, const char *dados, int n, int sinal);
__declspec(dllimport) int recv(uintptr_t s, char *dados, int n, int sinal);
__declspec(dllimport) int ioctlsocket(uintptr_t s, uint32_t comando, uint32_t *argumento);
__declspec(dllimport) int getsockopt(uintptr_t s, int nivel, int opcao, char *valor, int *tamanho);

/* Trava do WSAStartup (1x no processo): so trava com rt_multithread ligado (sys_trava_pegar). */
static SysTrava net_trava; /* compartilhado: trava do WSAStartup (so trava com rt_multithread ligado) */
static int32_t net_pronto; /* compartilhado: 1 depois do WSAStartup bem-sucedido; so muda sob net_trava */

/* Chama WSAStartup uma vez. 0 = ok. */
static int NetIniciar(void)
{
    uint8_t dados[512];
    bool tomou = sys_trava_pegar(&net_trava);
    int resultado = 0;

    if (!net_pronto) {
        resultado = WSAStartup(0x0202, dados);
        if (resultado == 0)
            net_pronto = 1;
    }
    sys_trava_soltar(&net_trava, tomou);
    return resultado;
}

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

/* Codigo de erro do bind: 10048 = porta em uso; 10049 = endereco que nao e deste computador; o resto e falha do socket. */
static int32_t NetErroDeBind(void)
{
    int codigo = WSAGetLastError();

    if (codigo == NET_WSA_ENDERECO_EM_USO)
        return NET_ERRO_PORTA_EM_USO;
    if (codigo == NET_WSA_ENDERECO_INVALIDO)
        return NET_ERRO_ENDERECO_INVALIDO;
    return NET_ERRO_FALHA_SOCKET;
}

/* Traduz o WSAGetLastError de uma operacao: EINTR/ENOTSOCK = socket fechado; EMSGSIZE = datagrama maior que o buffer; o resto = padrao. */
static int32_t NetErroDoCodigo(int codigo, int32_t padrao)
{
    if (codigo == NET_WSA_EINTR || codigo == NET_WSA_ENOTSOCK)
        return NET_ERRO_FECHADO;
    if (codigo == NET_WSA_EMSGSIZE)
        return NET_ERRO_DATAGRAMA_MAIOR;
    return padrao;
}

/* Falha de recvfrom/select. EWOULDBLOCK e ECONNRESET viram timeout (sem dado): ECONNRESET nunca sobe como erro. */
static int32_t NetErroDeRecebimento(void)
{
    int codigo = WSAGetLastError();

    if (codigo == NET_WSA_EWOULDBLOCK || codigo == NET_WSA_ECONNRESET)
        return NET_ERRO_TIMEOUT;
    return NetErroDoCodigo(codigo, NET_ERRO_FALHA_RECEBIMENTO);
}

/* Falha de sendto. */
static int32_t NetErroDeEnvio(void)
{
    return NetErroDoCodigo(WSAGetLastError(), NET_ERRO_FALHA_ENVIO);
}

/* Antes do bind: SIO_UDP_CONNRESET = FALSE (ICMP "porta inalcancavel" de um envio nao vira erro no recv seguinte) e buffers de 4 MiB. Falha ignorada (o SO pode clampar). */
static void NetAjustarSocket(uintptr_t s)
{
    uint32_t desligado = 0;
    uint32_t devolvido = 0;
    int buffer = NET_BUFFER_BYTES;

    (void)WSAIoctl(s, NET_SIO_UDP_CONNRESET, &desligado, (uint32_t)sizeof desligado, 0, 0, &devolvido, 0, 0);
    (void)setsockopt(s, NET_SOL_SOCKET, NET_SO_RCVBUF, (const char *)&buffer, (int)sizeof buffer);
    (void)setsockopt(s, NET_SOL_SOCKET, NET_SO_SNDBUF, (const char *)&buffer, (int)sizeof buffer);
}

/* Cria o socket UDP IPv4 e liga ao pacote. Devolve o handle (0 = erro, e *erro diz qual). */
uint64_t forja_net_ligar(uint64_t pacote, int32_t *erro)
{
    NetSockaddrIn sa;
    uintptr_t s;

    *erro = 0;
    if (NetIniciar() != 0) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    s = socket(NET_AF_INET, NET_SOCK_DGRAM, NET_IPPROTO_UDP);
    if (s == NET_SOCKET_INVALIDO) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    NetAjustarSocket(s);
    NetPreencher(&sa, pacote);
    if (bind(s, &sa, (int)sizeof sa) != 0) {
        *erro = NetErroDeBind();
        closesocket(s);
        return 0;
    }
    return (uint64_t)s;
}

/* Endereco local do socket, empacotado em *saida. 0 = ok; senao o codigo NET_ERRO_*. */
int32_t forja_net_local(uint64_t h, uint64_t *saida)
{
    NetSockaddrIn sa;
    int tamanho = (int)sizeof sa;

    if (h == 0)
        return NET_ERRO_FECHADO;
    if (getsockname((uintptr_t)h, &sa, &tamanho) != 0)
        return NET_ERRO_FALHA_SOCKET;
    *saida = NetEmpacotar(&sa);
    return 0;
}

/* Envia n bytes ao destino. >= 0 = bytes enviados; < 0 = -codigo NET_ERRO_*. */
int64_t forja_net_enviar(uint64_t h, uint64_t destino, const uint8_t *dados, int64_t n)
{
    NetSockaddrIn sa;
    int enviados;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (n < 0 || n > NET_DATAGRAMA_MAXIMO)
        return -NET_ERRO_FALHA_ENVIO;
    NetPreencher(&sa, destino);
    enviados = sendto((uintptr_t)h, (const char *)dados, (int)n, 0, &sa, (int)sizeof sa);
    if (enviados < 0)
        return -NetErroDeEnvio();
    return enviados;
}

/* Espera ate ms milissegundos (ms < 0 = sem limite) ate o socket ter datagrama. 1 = pronto; 0 = expirou; < 0 = -codigo. */
int32_t forja_net_esperar(uint64_t h, int32_t ms)
{
    NetConjunto conjunto;
    NetTimeval tempo;
    int pronto;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    conjunto.quantos = 1;
    conjunto.soquete[0] = (uintptr_t)h;
    if (ms < 0) {
        pronto = select(0, &conjunto, 0, 0, 0);
    } else {
        tempo.segundos = ms / 1000;
        tempo.microssegundos = (ms % 1000) * 1000;
        pronto = select(0, &conjunto, 0, 0, &tempo);
    }
    if (pronto < 0)
        return -NetErroDeRecebimento();
    return pronto > 0 ? 1 : 0;
}

/* Recebe um datagrama em destino (cap bytes) e o remetente em *origem. >= 0 = bytes recebidos; < 0 = -codigo. */
int64_t forja_net_receber(uint64_t h, uint8_t *destino, int64_t cap, uint64_t *origem)
{
    NetSockaddrIn sa;
    int tamanho = (int)sizeof sa;
    int recebidos;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (cap <= 0 || cap > NET_DATAGRAMA_MAXIMO)
        return -NET_ERRO_FALHA_RECEBIMENTO;
    recebidos = recvfrom((uintptr_t)h, (char *)destino, (int)cap, 0, &sa, &tamanho);
    if (recebidos < 0)
        return -NetErroDeRecebimento();
    *origem = NetEmpacotar(&sa);
    return recebidos;
}

/* Fecha o socket. 0 = ok; senao o codigo NET_ERRO_*. */
int32_t forja_net_fechar(uint64_t h)
{
    if (h == 0)
        return NET_ERRO_FECHADO;
    return closesocket((uintptr_t)h) == 0 ? 0 : NET_ERRO_FALHA_SOCKET;
}

/* Falha de connect: recusada, timeout, endereco que nao serve (0.0.0.0); o resto = falha do socket. */
static int32_t NetErroDeConexao(int codigo)
{
    if (codigo == NET_WSA_ECONNREFUSED)
        return NET_ERRO_CONEXAO_RECUSADA;
    if (codigo == NET_WSA_ETIMEDOUT)
        return NET_ERRO_TIMEOUT;
    if (codigo == NET_WSA_ENDERECO_INVALIDO)
        return NET_ERRO_ENDERECO_INVALIDO;
    return NET_ERRO_FALHA_SOCKET;
}

/* O outro lado ter fechado/derrubado a conexao (RST, abort, shutdown) e codigo proprio no TCP. */
static bool NetFechadoPeloOutro(int codigo)
{
    return codigo == NET_WSA_ECONNRESET || codigo == NET_WSA_ECONNABORTED || codigo == NET_WSA_ESHUTDOWN;
}

/* Espera ate ms milissegundos por dado (ou conexao pendente, no socket em escuta). 1 = pronto; 0 = expirou; < 0 = -codigo. */
int32_t forja_net_tcp_esperar(uint64_t h, int32_t ms)
{
    return forja_net_esperar(h, ms);
}

/* Socket TCP IPv4 em escuta no pacote. Devolve o handle (0 = erro, e *erro diz qual). */
uint64_t forja_net_tcp_ouvir(uint64_t pacote, int32_t backlog, int32_t *erro)
{
    NetSockaddrIn sa;
    uintptr_t s;

    *erro = 0;
    if (NetIniciar() != 0) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    s = socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);
    if (s == NET_SOCKET_INVALIDO) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    NetPreencher(&sa, pacote);
    if (bind(s, &sa, (int)sizeof sa) != 0)
        *erro = NetErroDeBind();
    else if (listen(s, backlog) != 0)
        *erro = NET_ERRO_FALHA_SOCKET;
    if (*erro != 0) {
        closesocket(s);
        return 0;
    }
    return (uint64_t)s;
}

/* Tira uma conexao da fila do socket em escuta (chamar depois de forja_net_tcp_esperar: sem conexao pendente bloqueia). Handle novo (0 = erro). */
uint64_t forja_net_tcp_aceitar(uint64_t h, int32_t *erro)
{
    uintptr_t s;

    *erro = 0;
    if (h == 0) {
        *erro = NET_ERRO_FECHADO;
        return 0;
    }
    s = accept((uintptr_t)h, 0, 0);
    if (s == NET_SOCKET_INVALIDO) {
        *erro = NetErroDoCodigo(WSAGetLastError(), NET_ERRO_FALHA_SOCKET);
        return 0;
    }
    return (uint64_t)s;
}

/* Depois do connect nao bloqueante: espera ate ms a conexao acabar (select: escrita = conectou, excecao = falhou) e le o SO_ERROR. 0 = conectado; senao o codigo NET_ERRO_*. */
static int32_t NetAguardarConexao(uintptr_t s, int32_t ms)
{
    NetConjunto escrita;
    NetConjunto falha;
    NetTimeval tempo;
    int pendente = 0;
    int tamanho = (int)sizeof pendente;
    int pronto;

    escrita.quantos = 1;
    escrita.soquete[0] = s;
    falha.quantos = 1;
    falha.soquete[0] = s;
    tempo.segundos = ms / 1000;
    tempo.microssegundos = (ms % 1000) * 1000;
    pronto = select(0, 0, &escrita, &falha, ms < 0 ? 0 : &tempo);
    if (pronto < 0)
        return NET_ERRO_FALHA_SOCKET;
    if (pronto == 0)
        return NET_ERRO_TIMEOUT;
    if (getsockopt(s, NET_SOL_SOCKET, NET_SO_ERROR, (char *)&pendente, &tamanho) != 0)
        return NET_ERRO_FALHA_SOCKET;
    return pendente == 0 ? 0 : NetErroDeConexao(pendente);
}

/* connect nao bloqueante e espera. 0 = conectado; senao o codigo NET_ERRO_*. Deixa o socket de novo bloqueante. */
static int32_t NetConectarComPrazo(uintptr_t s, const NetSockaddrIn *sa, int32_t ms)
{
    uint32_t modo = 1;
    int32_t erro = 0;
    int codigo;

    if (ioctlsocket(s, NET_FIONBIO, &modo) != 0)
        return NET_ERRO_FALHA_SOCKET;
    if (connect(s, sa, (int)sizeof *sa) != 0) {
        codigo = WSAGetLastError();
        erro = codigo == NET_WSA_EWOULDBLOCK ? NetAguardarConexao(s, ms) : NetErroDeConexao(codigo);
    }
    modo = 0;
    if (erro == 0 && ioctlsocket(s, NET_FIONBIO, &modo) != 0)
        erro = NET_ERRO_FALHA_SOCKET;
    return erro;
}

/* Conecta ao pacote esperando ate ms (ms < 0 = sem limite). Handle do socket (0 = erro; *erro: recusada, timeout, endereco invalido...). */
uint64_t forja_net_tcp_conectar(uint64_t pacote, int32_t ms, int32_t *erro)
{
    NetSockaddrIn sa;
    uintptr_t s;

    *erro = 0;
    if (NetIniciar() != 0) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    s = socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);
    if (s == NET_SOCKET_INVALIDO) {
        *erro = NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    NetPreencher(&sa, pacote);
    *erro = NetConectarComPrazo(s, &sa, ms);
    if (*erro != 0) {
        closesocket(s);
        return 0;
    }
    return (uint64_t)s;
}

/* Envia os n bytes todos (repete o send ate acabar). >= 0 = n; < 0 = -codigo NET_ERRO_* (o outro lado fechado = FECHADO_PELO_OUTRO). */
int64_t forja_net_tcp_enviar(uint64_t h, const uint8_t *dados, int64_t n)
{
    int64_t total = 0;
    int resto;
    int enviados;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (n < 0)
        return -NET_ERRO_FALHA_ENVIO;
    while (total < n) {
        resto = n - total > NET_TCP_TRECHO_MAXIMO ? NET_TCP_TRECHO_MAXIMO : (int)(n - total);
        enviados = send((uintptr_t)h, (const char *)(dados + total), resto, 0);
        if (enviados < 0)
            return -(NetFechadoPeloOutro(WSAGetLastError()) ? NET_ERRO_FECHADO_PELO_OUTRO : NetErroDeEnvio());
        if (enviados == 0)
            return -NET_ERRO_FALHA_ENVIO;
        total += enviados;
    }
    return total;
}

/* Le ate cap bytes (chamar depois de forja_net_tcp_esperar). >= 0 = bytes lidos, 0 = o outro lado fechou; < 0 = -codigo NET_ERRO_*. */
int64_t forja_net_tcp_receber(uint64_t h, uint8_t *destino, int64_t cap)
{
    int recebidos;
    int codigo;

    if (h == 0)
        return -NET_ERRO_FECHADO;
    if (cap <= 0)
        return -NET_ERRO_FALHA_RECEBIMENTO;
    recebidos = recv((uintptr_t)h, (char *)destino, cap > NET_TCP_TRECHO_MAXIMO ? NET_TCP_TRECHO_MAXIMO : (int)cap, 0);
    if (recebidos >= 0)
        return recebidos;
    codigo = WSAGetLastError();
    if (NetFechadoPeloOutro(codigo))
        return -NET_ERRO_FECHADO_PELO_OUTRO;
    return -NetErroDoCodigo(codigo, codigo == NET_WSA_EWOULDBLOCK ? NET_ERRO_TIMEOUT : NET_ERRO_FALHA_RECEBIMENTO);
}
