/*
 * forja_rt_net.h - std:net UDP no backend C (M5 P53), o oraculo: as mesmas 6 funcoes de runtime\nativo\rt_net_win.c, com os mesmos nomes, tipos e codigos NET_ERRO_*.
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M5 P54: espelho de rt_net_win.c: SIO_UDP_CONNRESET = FALSE e SO_RCVBUF/SO_SNDBUF = 4 MiB no bind; erros WSA mapeados (EWOULDBLOCK/ECONNRESET
 *           -> timeout, EMSGSIZE -> FORJA_NET_ERRO_DATAGRAMA_MAIOR 8, EINTR/ENOTSOCK -> fechado).
 *   0.1.0 - M5 P53: forja_net_ligar, forja_net_local, forja_net_enviar, forja_net_esperar, forja_net_receber e forja_net_fechar. Windows: ws2_32 por
 *           FORJA_ROTULO_C (sem winsock2.h), WSAStartup 1x sob a ForjaTrava. POSIX: stub (ligar devolve falha_socket; o Linux do std:net e o P60).
 *
 * Texto ASCII puro. Entra no .c gerado so quando o programa tem foreign interno forja_net_* (emit_c_runtime.c). O programa liga -lws2_32 nesse caso (driver\cc.c).
 */
#include <stdint.h>

#define FORJA_NET_ERRO_ENDERECO_INVALIDO 1
#define FORJA_NET_ERRO_PORTA_EM_USO 2
#define FORJA_NET_ERRO_TIMEOUT 3
#define FORJA_NET_ERRO_FALHA_SOCKET 4
#define FORJA_NET_ERRO_FALHA_ENVIO 5
#define FORJA_NET_ERRO_FALHA_RECEBIMENTO 6
#define FORJA_NET_ERRO_FECHADO 7
#define FORJA_NET_ERRO_DATAGRAMA_MAIOR 8
#define FORJA_NET_DATAGRAMA_MAXIMO 65507
#define FORJA_NET_WSA_EINTR 10004
#define FORJA_NET_WSA_ENOTSOCK 10038
#define FORJA_NET_WSA_EWOULDBLOCK 10035
#define FORJA_NET_WSA_EMSGSIZE 10040
#define FORJA_NET_WSA_ECONNRESET 10054
#define FORJA_NET_SIO_UDP_CONNRESET 0x9800000Cu
#define FORJA_NET_BUFFER_BYTES 4194304

#if defined(_WIN32)
typedef struct {
    uint16_t familia;
    uint16_t porta;
    uint8_t endereco[4];
    uint8_t zero[8];
} ForjaNetSockaddr;

typedef struct {
    long segundos;
    long microssegundos;
} ForjaNetTempo;

typedef struct {
    uint32_t quantos;
    uintptr_t soquete[1];
} ForjaNetConjunto;

int forja_net_ws_iniciar(uint16_t versao, void *dados) FORJA_ROTULO_C("WSAStartup");
int forja_net_ws_erro(void) FORJA_ROTULO_C("WSAGetLastError");
uintptr_t forja_net_ws_socket(int familia, int tipo, int protocolo) FORJA_ROTULO_C("socket");
int forja_net_ws_bind(uintptr_t s, const void *nome, int tamanho) FORJA_ROTULO_C("bind");
int forja_net_ws_sendto(uintptr_t s, const char *dados, int n, int sinal, const void *destino, int tamanho) FORJA_ROTULO_C("sendto");
int forja_net_ws_recvfrom(uintptr_t s, char *dados, int n, int sinal, void *origem, int *tamanho) FORJA_ROTULO_C("recvfrom");
int forja_net_ws_select(int ignorado, ForjaNetConjunto *leitura, void *escrita, void *excecao, const ForjaNetTempo *tempo) FORJA_ROTULO_C("select");
int forja_net_ws_nome(uintptr_t s, void *nome, int *tamanho) FORJA_ROTULO_C("getsockname");
int forja_net_ws_fechar(uintptr_t s) FORJA_ROTULO_C("closesocket");
int forja_net_ws_ioctl(uintptr_t s, uint32_t codigo, void *entrada, uint32_t tamanho_entrada, void *saida, uint32_t tamanho_saida, uint32_t *devolvido, void *sobreposto, void *rotina) FORJA_ROTULO_C("WSAIoctl");
int forja_net_ws_setsockopt(uintptr_t s, int nivel, int opcao, const char *valor, int tamanho) FORJA_ROTULO_C("setsockopt");

static ForjaTrava forja_net_trava;
static int forja_net_pronto;

static int forja_net_iniciar(void)
{
    uint8_t dados[512];
    int tomou = forja_trava_pegar(&forja_net_trava);
    int resultado = 0;

    if (!forja_net_pronto) {
        resultado = forja_net_ws_iniciar(0x0202, dados);
        if (resultado == 0)
            forja_net_pronto = 1;
    }
    forja_trava_soltar(&forja_net_trava, tomou);
    return resultado;
}

static void forja_net_preencher(ForjaNetSockaddr *sa, uint64_t pacote)
{
    uint32_t ip = (uint32_t)(pacote >> 16);
    uint16_t porta = (uint16_t)(pacote & 0xFFFFu);
    int i;

    sa->familia = 2;
    sa->porta = (uint16_t)((porta >> 8) | (porta << 8));
    sa->endereco[0] = (uint8_t)(ip >> 24);
    sa->endereco[1] = (uint8_t)(ip >> 16);
    sa->endereco[2] = (uint8_t)(ip >> 8);
    sa->endereco[3] = (uint8_t)ip;
    for (i = 0; i < 8; i++)
        sa->zero[i] = 0;
}

static uint64_t forja_net_empacotar(const ForjaNetSockaddr *sa)
{
    uint32_t ip = ((uint32_t)sa->endereco[0] << 24) | ((uint32_t)sa->endereco[1] << 16) | ((uint32_t)sa->endereco[2] << 8) | (uint32_t)sa->endereco[3];
    uint16_t porta = (uint16_t)((sa->porta >> 8) | (sa->porta << 8));

    return ((uint64_t)ip << 16) | porta;
}

/* Antes do bind: SIO_UDP_CONNRESET = FALSE e buffers de 4 MiB. Falha ignorada (o SO pode clampar). */
static void forja_net_ajustar(uintptr_t s)
{
    uint32_t desligado = 0;
    uint32_t devolvido = 0;
    int buffer = FORJA_NET_BUFFER_BYTES;

    (void)forja_net_ws_ioctl(s, FORJA_NET_SIO_UDP_CONNRESET, &desligado, (uint32_t)sizeof desligado, 0, 0, &devolvido, 0, 0);
    (void)forja_net_ws_setsockopt(s, 0xFFFF, 0x1002, (const char *)&buffer, (int)sizeof buffer);
    (void)forja_net_ws_setsockopt(s, 0xFFFF, 0x1001, (const char *)&buffer, (int)sizeof buffer);
}

static int32_t forja_net_erro_do_codigo(int codigo, int32_t padrao)
{
    if (codigo == FORJA_NET_WSA_EINTR || codigo == FORJA_NET_WSA_ENOTSOCK)
        return FORJA_NET_ERRO_FECHADO;
    if (codigo == FORJA_NET_WSA_EMSGSIZE)
        return FORJA_NET_ERRO_DATAGRAMA_MAIOR;
    return padrao;
}

static int32_t forja_net_erro_recebimento(void)
{
    int codigo = forja_net_ws_erro();

    if (codigo == FORJA_NET_WSA_EWOULDBLOCK || codigo == FORJA_NET_WSA_ECONNRESET)
        return FORJA_NET_ERRO_TIMEOUT;
    return forja_net_erro_do_codigo(codigo, FORJA_NET_ERRO_FALHA_RECEBIMENTO);
}

static int32_t forja_net_erro_envio(void)
{
    return forja_net_erro_do_codigo(forja_net_ws_erro(), FORJA_NET_ERRO_FALHA_ENVIO);
}

static int32_t forja_net_erro_bind(void)
{
    int codigo = forja_net_ws_erro();

    if (codigo == 10048)
        return FORJA_NET_ERRO_PORTA_EM_USO;
    if (codigo == 10049)
        return FORJA_NET_ERRO_ENDERECO_INVALIDO;
    return FORJA_NET_ERRO_FALHA_SOCKET;
}

uint64_t forja_net_ligar(uint64_t pacote, int32_t *erro)
{
    ForjaNetSockaddr sa;
    uintptr_t s;

    *erro = 0;
    if (forja_net_iniciar() != 0) {
        *erro = FORJA_NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    s = forja_net_ws_socket(2, 2, 17);
    if (s == ~(uintptr_t)0) {
        *erro = FORJA_NET_ERRO_FALHA_SOCKET;
        return 0;
    }
    forja_net_ajustar(s);
    forja_net_preencher(&sa, pacote);
    if (forja_net_ws_bind(s, &sa, (int)sizeof sa) != 0) {
        *erro = forja_net_erro_bind();
        forja_net_ws_fechar(s);
        return 0;
    }
    return (uint64_t)s;
}

int32_t forja_net_local(uint64_t h, uint64_t *saida)
{
    ForjaNetSockaddr sa;
    int tamanho = (int)sizeof sa;

    if (h == 0)
        return FORJA_NET_ERRO_FECHADO;
    if (forja_net_ws_nome((uintptr_t)h, &sa, &tamanho) != 0)
        return FORJA_NET_ERRO_FALHA_SOCKET;
    *saida = forja_net_empacotar(&sa);
    return 0;
}

int64_t forja_net_enviar(uint64_t h, uint64_t destino, const uint8_t *dados, int64_t n)
{
    ForjaNetSockaddr sa;
    int enviados;

    if (h == 0)
        return -FORJA_NET_ERRO_FECHADO;
    if (n < 0 || n > FORJA_NET_DATAGRAMA_MAXIMO)
        return -FORJA_NET_ERRO_FALHA_ENVIO;
    forja_net_preencher(&sa, destino);
    enviados = forja_net_ws_sendto((uintptr_t)h, (const char *)dados, (int)n, 0, &sa, (int)sizeof sa);
    if (enviados < 0)
        return -forja_net_erro_envio();
    return enviados;
}

int32_t forja_net_esperar(uint64_t h, int32_t ms)
{
    ForjaNetConjunto conjunto;
    ForjaNetTempo tempo;
    int pronto;

    if (h == 0)
        return -FORJA_NET_ERRO_FECHADO;
    conjunto.quantos = 1;
    conjunto.soquete[0] = (uintptr_t)h;
    if (ms < 0) {
        pronto = forja_net_ws_select(0, &conjunto, 0, 0, 0);
    } else {
        tempo.segundos = ms / 1000;
        tempo.microssegundos = (ms % 1000) * 1000;
        pronto = forja_net_ws_select(0, &conjunto, 0, 0, &tempo);
    }
    if (pronto < 0)
        return -forja_net_erro_recebimento();
    return pronto > 0 ? 1 : 0;
}

int64_t forja_net_receber(uint64_t h, uint8_t *destino, int64_t cap, uint64_t *origem)
{
    ForjaNetSockaddr sa;
    int tamanho = (int)sizeof sa;
    int recebidos;

    if (h == 0)
        return -FORJA_NET_ERRO_FECHADO;
    if (cap <= 0 || cap > FORJA_NET_DATAGRAMA_MAXIMO)
        return -FORJA_NET_ERRO_FALHA_RECEBIMENTO;
    recebidos = forja_net_ws_recvfrom((uintptr_t)h, (char *)destino, (int)cap, 0, &sa, &tamanho);
    if (recebidos < 0)
        return -forja_net_erro_recebimento();
    *origem = forja_net_empacotar(&sa);
    return recebidos;
}

int32_t forja_net_fechar(uint64_t h)
{
    if (h == 0)
        return FORJA_NET_ERRO_FECHADO;
    return forja_net_ws_fechar((uintptr_t)h) == 0 ? 0 : FORJA_NET_ERRO_FALHA_SOCKET;
}
#else
/* POSIX: stub. O Linux do std:net (syscalls cruas) e o P60; o programa nao chega aqui no alvo Linux (X0003). */
uint64_t forja_net_ligar(uint64_t pacote, int32_t *erro)
{
    (void)pacote;
    *erro = FORJA_NET_ERRO_FALHA_SOCKET;
    return 0;
}

int32_t forja_net_local(uint64_t h, uint64_t *saida)
{
    (void)h;
    (void)saida;
    return FORJA_NET_ERRO_FALHA_SOCKET;
}

int64_t forja_net_enviar(uint64_t h, uint64_t destino, const uint8_t *dados, int64_t n)
{
    (void)h;
    (void)destino;
    (void)dados;
    (void)n;
    return -FORJA_NET_ERRO_FALHA_ENVIO;
}

int32_t forja_net_esperar(uint64_t h, int32_t ms)
{
    (void)h;
    (void)ms;
    return -FORJA_NET_ERRO_FALHA_RECEBIMENTO;
}

int64_t forja_net_receber(uint64_t h, uint8_t *destino, int64_t cap, uint64_t *origem)
{
    (void)h;
    (void)destino;
    (void)cap;
    (void)origem;
    return -FORJA_NET_ERRO_FALHA_RECEBIMENTO;
}

int32_t forja_net_fechar(uint64_t h)
{
    (void)h;
    return FORJA_NET_ERRO_FALHA_SOCKET;
}
#endif
