/* curl_ponte.c - implementacao da cola curlf_* (ver curl_ponte.h). Compilar com:
 *   gcc -O2 -Wall -DCURL_STATICLIB -I curl/include -c curl_ponte.c -o curl_ponte.o
 *   ar rcs ..\libcurl.a <libcurl.a do cmake> e depois  ar r ..\libcurl.a curl_ponte.o   (a cola entra na mesma libcurl.a)
 * Verificar: o proprio gcc recusa a compilacao se algum numero do curl_ponte.h divergir do curl.h (_Static_assert). */
#include <curl/curl.h>

#include "curl_ponte.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

_Static_assert(CURLF_OPT_WRITEDATA == CURLOPT_WRITEDATA, "WRITEDATA");
_Static_assert(CURLF_OPT_URL == CURLOPT_URL, "URL");
_Static_assert(CURLF_OPT_TIMEOUT == CURLOPT_TIMEOUT, "TIMEOUT");
_Static_assert(CURLF_OPT_USERAGENT == CURLOPT_USERAGENT, "USERAGENT");
_Static_assert(CURLF_OPT_FAILONERROR == CURLOPT_FAILONERROR, "FAILONERROR");
_Static_assert(CURLF_OPT_FOLLOWLOCATION == CURLOPT_FOLLOWLOCATION, "FOLLOWLOCATION");
_Static_assert(CURLF_OPT_NOSIGNAL == CURLOPT_NOSIGNAL, "NOSIGNAL");
_Static_assert(CURLF_OPT_MAXREDIRS == CURLOPT_MAXREDIRS, "MAXREDIRS");
_Static_assert(CURLF_OPT_CONNECTTIMEOUT == CURLOPT_CONNECTTIMEOUT, "CONNECTTIMEOUT");
_Static_assert(CURLF_OPT_WRITEFUNCTION == CURLOPT_WRITEFUNCTION, "WRITEFUNCTION");
_Static_assert(CURLF_INFO_RESPONSE_CODE == CURLINFO_RESPONSE_CODE, "RESPONSE_CODE");
_Static_assert(CURLF_E_OK == CURLE_OK, "OK");
_Static_assert(CURLF_E_UNSUPPORTED_PROTOCOL == CURLE_UNSUPPORTED_PROTOCOL, "UNSUPPORTED_PROTOCOL");
_Static_assert(CURLF_E_URL_MALFORMAT == CURLE_URL_MALFORMAT, "URL_MALFORMAT");
_Static_assert(CURLF_E_COULDNT_RESOLVE_HOST == CURLE_COULDNT_RESOLVE_HOST, "COULDNT_RESOLVE_HOST");
_Static_assert(CURLF_E_COULDNT_CONNECT == CURLE_COULDNT_CONNECT, "COULDNT_CONNECT");
_Static_assert(CURLF_E_HTTP_RETURNED_ERROR == CURLE_HTTP_RETURNED_ERROR, "HTTP_RETURNED_ERROR");
_Static_assert(CURLF_E_WRITE_ERROR == CURLE_WRITE_ERROR, "WRITE_ERROR");
_Static_assert(CURLF_E_OPERATION_TIMEDOUT == CURLE_OPERATION_TIMEDOUT, "OPERATION_TIMEDOUT");
_Static_assert(CURLF_E_SSL_CONNECT_ERROR == CURLE_SSL_CONNECT_ERROR, "SSL_CONNECT_ERROR");
_Static_assert(CURLF_E_TOO_MANY_REDIRECTS == CURLE_TOO_MANY_REDIRECTS, "TOO_MANY_REDIRECTS");
_Static_assert(CURLF_E_PEER_FAILED_VERIFICATION == CURLE_PEER_FAILED_VERIFICATION, "PEER_FAILED_VERIFICATION");
_Static_assert(CURLF_OPT_CUSTOMREQUEST == CURLOPT_CUSTOMREQUEST, "CUSTOMREQUEST");
_Static_assert(CURLF_OPT_POSTFIELDSIZE_LARGE == CURLOPT_POSTFIELDSIZE_LARGE, "POSTFIELDSIZE_LARGE");
_Static_assert(CURLF_OPT_COPYPOSTFIELDS == CURLOPT_COPYPOSTFIELDS, "COPYPOSTFIELDS");
_Static_assert(CURLF_OPT_HTTPHEADER == CURLOPT_HTTPHEADER, "HTTPHEADER");
_Static_assert(CURLF_OPT_UPLOAD == CURLOPT_UPLOAD, "UPLOAD");
_Static_assert(CURLF_OPT_INFILESIZE_LARGE == CURLOPT_INFILESIZE_LARGE, "INFILESIZE_LARGE");
_Static_assert(CURLF_OPT_READFUNCTION == CURLOPT_READFUNCTION, "READFUNCTION");
_Static_assert(CURLF_OPT_READDATA == CURLOPT_READDATA, "READDATA");
_Static_assert(CURLF_OPT_HEADERFUNCTION == CURLOPT_HEADERFUNCTION, "HEADERFUNCTION");
_Static_assert(CURLF_OPT_HEADERDATA == CURLOPT_HEADERDATA, "HEADERDATA");
_Static_assert(CURLF_OPT_NOBODY == CURLOPT_NOBODY, "NOBODY");
_Static_assert(CURLF_OPT_TIMEOUT_MS == CURLOPT_TIMEOUT_MS, "TIMEOUT_MS");
_Static_assert(CURLF_OPT_CONNECTTIMEOUT_MS == CURLOPT_CONNECTTIMEOUT_MS, "CONNECTTIMEOUT_MS");
_Static_assert(CURLF_OPT_USERNAME == CURLOPT_USERNAME, "USERNAME");
_Static_assert(CURLF_OPT_PASSWORD == CURLOPT_PASSWORD, "PASSWORD");
_Static_assert(CURLF_OPT_LOGIN_OPTIONS == CURLOPT_LOGIN_OPTIONS, "LOGIN_OPTIONS");
_Static_assert(CURLF_OPT_MAIL_FROM == CURLOPT_MAIL_FROM, "MAIL_FROM");
_Static_assert(CURLF_OPT_MAIL_RCPT == CURLOPT_MAIL_RCPT, "MAIL_RCPT");
_Static_assert(CURLF_OPT_USE_SSL == CURLOPT_USE_SSL, "USE_SSL");
_Static_assert(CURLF_USESSL_ALL == CURLUSESSL_ALL, "USESSL_ALL");
_Static_assert(CURLF_OPT_CAINFO == CURLOPT_CAINFO, "CAINFO");
_Static_assert(CURLF_OPT_SSL_OPTIONS == CURLOPT_SSL_OPTIONS, "SSL_OPTIONS");
_Static_assert(CURLF_SSLOPT_NO_REVOKE == CURLSSLOPT_NO_REVOKE, "SSLOPT_NO_REVOKE");
_Static_assert(CURLF_OPT_ERRORBUFFER == CURLOPT_ERRORBUFFER, "ERRORBUFFER");
_Static_assert(CURLF_OPT_HTTPGET == CURLOPT_HTTPGET, "HTTPGET");
_Static_assert(CURLF_E_WEIRD_SERVER_REPLY == CURLE_WEIRD_SERVER_REPLY, "WEIRD_SERVER_REPLY");
_Static_assert(CURLF_E_REMOTE_ACCESS_DENIED == CURLE_REMOTE_ACCESS_DENIED, "REMOTE_ACCESS_DENIED");
_Static_assert(CURLF_E_QUOTE_ERROR == CURLE_QUOTE_ERROR, "QUOTE_ERROR");
_Static_assert(CURLF_E_UPLOAD_FAILED == CURLE_UPLOAD_FAILED, "UPLOAD_FAILED");
_Static_assert(CURLF_E_GOT_NOTHING == CURLE_GOT_NOTHING, "GOT_NOTHING");
_Static_assert(CURLF_E_SEND_ERROR == CURLE_SEND_ERROR, "SEND_ERROR");
_Static_assert(CURLF_E_RECV_ERROR == CURLE_RECV_ERROR, "RECV_ERROR");
_Static_assert(CURLF_E_USE_SSL_FAILED == CURLE_USE_SSL_FAILED, "USE_SSL_FAILED");
_Static_assert(CURLF_E_LOGIN_DENIED == CURLE_LOGIN_DENIED, "LOGIN_DENIED");
_Static_assert(CURLF_E_REMOTE_FILE_NOT_FOUND == CURLE_REMOTE_FILE_NOT_FOUND, "REMOTE_FILE_NOT_FOUND");
_Static_assert(sizeof(unsigned long long) == sizeof(size_t), "size_t de 64 bits");

/* curl_global_init nao e seguro se chamado de duas threads: a cola faz uma vez so (InitOnceExecuteOnce no Windows,
 * pthread_once no Linux) e guarda o codigo para as chamadas seguintes. Motivo: o libcurl passa a ser usado por varias threads. */
#if defined(_WIN32)
static INIT_ONCE global_init_once = INIT_ONCE_STATIC_INIT;
static int global_init_codigo = CURLE_OK;

static BOOL CALLBACK global_init_executar(PINIT_ONCE uma_vez, PVOID parametro, PVOID *contexto)
{
    (void)uma_vez;
    (void)parametro;
    (void)contexto;
    global_init_codigo = (int)curl_global_init(CURL_GLOBAL_ALL);
    return TRUE;
}
#else
static pthread_once_t global_init_once = PTHREAD_ONCE_INIT;
static int global_init_codigo = CURLE_OK;

static void global_init_executar(void)
{
    global_init_codigo = (int)curl_global_init(CURL_GLOBAL_ALL);
}
#endif

int curlf_global_uma_vez(void)
{
#if defined(_WIN32)
    if (!InitOnceExecuteOnce(&global_init_once, global_init_executar, NULL, NULL)) {
        return CURLE_FAILED_INIT;
    }
#else
    if (pthread_once(&global_init_once, global_init_executar) != 0) {
        return CURLE_FAILED_INIT;
    }
#endif
    return global_init_codigo;
}

CurlfManeja *curlf_iniciar(void)
{
    if (curlf_global_uma_vez() != CURLE_OK) {
        return NULL;
    }
    return (CurlfManeja *)curl_easy_init();
}

void curlf_fechar(CurlfManeja *maneja)
{
    curl_easy_cleanup((CURL *)maneja);
}

int curlf_executar(CurlfManeja *maneja)
{
    return (int)curl_easy_perform((CURL *)maneja);
}

const char *curlf_erro_texto(int codigo)
{
    return curl_easy_strerror((CURLcode)codigo);
}

const char *curlf_versao(void)
{
    return curl_version();
}

int curlf_opt_texto(CurlfManeja *maneja, int opcao, const char *valor)
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, valor);
}

int curlf_opt_inteiro(CurlfManeja *maneja, int opcao, long valor)
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, valor);
}

int curlf_opt_ponteiro(CurlfManeja *maneja, int opcao, void *valor)
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, valor);
}

int curlf_opt_escrita(CurlfManeja *maneja, int opcao,
                      unsigned long long (*retorno)(void *dados, unsigned long long tamanho, unsigned long long quantidade, void *usuario))
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, retorno);
}

int curlf_info_inteiro(CurlfManeja *maneja, int informacao, long *saida)
{
    return (int)curl_easy_getinfo((CURL *)maneja, (CURLINFO)informacao, saida);
}

/* A lista e do chamador e precisa viver ate o fim da transferencia: o libcurl so guarda o ponteiro. */
int curlf_opt_lista(CurlfManeja *maneja, int opcao, CurlfLista *lista)
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, (struct curl_slist *)lista);
}

int curlf_opt_inteiro64(CurlfManeja *maneja, int opcao, long long valor)
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, (curl_off_t)valor);
}

/* Callback de leitura: mesma forma do de escrita (ABI igual em x64; ver curlf_opt_escrita). */
int curlf_opt_leitura(CurlfManeja *maneja, int opcao,
                      unsigned long long (*retorno)(void *dados, unsigned long long tamanho, unsigned long long quantidade, void *usuario))
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, retorno);
}

int curlf_opt_cabecalho(CurlfManeja *maneja, int opcao,
                        unsigned long long (*retorno)(void *dados, unsigned long long tamanho, unsigned long long quantidade, void *usuario))
{
    return (int)curl_easy_setopt((CURL *)maneja, (CURLoption)opcao, retorno);
}

/* Em falha devolve NULL e NAO libera a lista de entrada: quem chama guarda o ponteiro antigo para liberar. */
CurlfLista *curlf_lista_acrescentar(CurlfLista *lista, const char *texto)
{
    return (CurlfLista *)curl_slist_append((struct curl_slist *)lista, texto);
}

void curlf_lista_liberar(CurlfLista *lista)
{
    curl_slist_free_all((struct curl_slist *)lista);
}
