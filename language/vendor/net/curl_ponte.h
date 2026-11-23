/* curl_ponte.h 0.2.0 - cola MINIMA entre a Forja e o libcurl 8.23.0 (licenca curl).
 *
 * Por que existe: curl_easy_setopt/getinfo sao variadicas (a Forja nao chama variadica: E0904/G0008) e o CURL e um
 * `void`. Estas 17 funcoes com prefixo curlf_ (a Forja recusa fj_/forja_) repassam para a API do libcurl com tipo fixo; curl_global_cleanup (sem prefixo, 18a) so libera o estado global e entra na Forja com `ends`.
 * Gere a ponte com:  forja ligar curl_ponte.h --link curl -o curl_ponte.fj
 * A implementacao esta em curl_ponte.c e CONFERE com _Static_assert que cada numero abaixo e o do curl.h real. */

typedef struct curlf_maneja CurlfManeja;
typedef struct curlf_lista CurlfLista;

/* opcoes (CURLoption) */
#define CURLF_OPT_WRITEDATA 10001
#define CURLF_OPT_URL 10002
#define CURLF_OPT_TIMEOUT 13
#define CURLF_OPT_USERAGENT 10018
#define CURLF_OPT_FAILONERROR 45
#define CURLF_OPT_FOLLOWLOCATION 52
#define CURLF_OPT_NOSIGNAL 99
#define CURLF_OPT_MAXREDIRS 68
#define CURLF_OPT_CONNECTTIMEOUT 78
#define CURLF_OPT_WRITEFUNCTION 20011
#define CURLF_OPT_CUSTOMREQUEST 10036
#define CURLF_OPT_POSTFIELDSIZE_LARGE 30120
#define CURLF_OPT_COPYPOSTFIELDS 10165
#define CURLF_OPT_HTTPHEADER 10023
#define CURLF_OPT_UPLOAD 46
#define CURLF_OPT_INFILESIZE_LARGE 30115
#define CURLF_OPT_READFUNCTION 20012
#define CURLF_OPT_READDATA 10009
#define CURLF_OPT_HEADERFUNCTION 20079
#define CURLF_OPT_HEADERDATA 10029
#define CURLF_OPT_NOBODY 44
#define CURLF_OPT_TIMEOUT_MS 155
#define CURLF_OPT_CONNECTTIMEOUT_MS 156
#define CURLF_OPT_USERNAME 10173
#define CURLF_OPT_PASSWORD 10174
#define CURLF_OPT_LOGIN_OPTIONS 10224
#define CURLF_OPT_MAIL_FROM 10186
#define CURLF_OPT_MAIL_RCPT 10187
#define CURLF_OPT_USE_SSL 119
#define CURLF_OPT_CAINFO 10065
#define CURLF_OPT_SSL_OPTIONS 216
#define CURLF_OPT_ERRORBUFFER 10010
#define CURLF_OPT_HTTPGET 80
/* valores usados dentro de opcoes (CURLUSESSL_ALL, CURLSSLOPT_NO_REVOKE) */
#define CURLF_USESSL_ALL 3
#define CURLF_SSLOPT_NO_REVOKE 2
/* informacoes (CURLINFO) */
#define CURLF_INFO_RESPONSE_CODE 2097154
/* codigos de retorno (CURLcode) */
#define CURLF_E_OK 0
#define CURLF_E_UNSUPPORTED_PROTOCOL 1
#define CURLF_E_URL_MALFORMAT 3
#define CURLF_E_COULDNT_RESOLVE_HOST 6
#define CURLF_E_COULDNT_CONNECT 7
#define CURLF_E_HTTP_RETURNED_ERROR 22
#define CURLF_E_WRITE_ERROR 23
#define CURLF_E_OPERATION_TIMEDOUT 28
#define CURLF_E_SSL_CONNECT_ERROR 35
#define CURLF_E_TOO_MANY_REDIRECTS 47
#define CURLF_E_PEER_FAILED_VERIFICATION 60
#define CURLF_E_WEIRD_SERVER_REPLY 8
#define CURLF_E_REMOTE_ACCESS_DENIED 9
#define CURLF_E_QUOTE_ERROR 21
#define CURLF_E_UPLOAD_FAILED 25
#define CURLF_E_GOT_NOTHING 52
#define CURLF_E_SEND_ERROR 55
#define CURLF_E_RECV_ERROR 56
#define CURLF_E_USE_SSL_FAILED 64
#define CURLF_E_LOGIN_DENIED 67
#define CURLF_E_REMOTE_FILE_NOT_FOUND 78

CurlfManeja *curlf_iniciar(void);
void curlf_fechar(CurlfManeja *maneja);
int curlf_executar(CurlfManeja *maneja);
const char *curlf_erro_texto(int codigo);
const char *curlf_versao(void);
int curlf_opt_texto(CurlfManeja *maneja, int opcao, const char *valor);
int curlf_opt_inteiro(CurlfManeja *maneja, int opcao, long valor);
int curlf_opt_ponteiro(CurlfManeja *maneja, int opcao, void *valor);
int curlf_opt_escrita(CurlfManeja *maneja, int opcao, unsigned long long (*retorno)(void *dados, unsigned long long tamanho, unsigned long long quantidade, void *usuario));
int curlf_info_inteiro(CurlfManeja *maneja, int informacao, long *saida);
int curlf_opt_lista(CurlfManeja *maneja, int opcao, CurlfLista *lista);
int curlf_opt_inteiro64(CurlfManeja *maneja, int opcao, long long valor);
int curlf_opt_leitura(CurlfManeja *maneja, int opcao, unsigned long long (*retorno)(void *dados, unsigned long long tamanho, unsigned long long quantidade, void *usuario));
int curlf_opt_cabecalho(CurlfManeja *maneja, int opcao, unsigned long long (*retorno)(void *dados, unsigned long long tamanho, unsigned long long quantidade, void *usuario));
CurlfLista *curlf_lista_acrescentar(CurlfLista *lista, const char *texto);
void curlf_lista_liberar(CurlfLista *lista);
int curlf_global_uma_vez(void);
/* fim do estado global do libcurl (Mbed TLS/PSA). Chamar so depois que TODAS as threads pararam de usar a lib.
 * Sem esta chamada o runtime do dev conta o estado global como vazamento R0009 (ver net.shutdown). */
void curl_global_cleanup(void);
