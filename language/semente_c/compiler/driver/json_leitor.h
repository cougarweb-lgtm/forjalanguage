/* json_leitor.h - leitor minimo de JSON do driver: texto -> arvore de valores na Arena (IA1-b05: o 'forja corrigir' le o JSON que o proprio forja check imprime).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b05: JsonLer, JsonMembro, JsonTextoDe e JsonNumeroDe.
 * Verificacao: construir.bat && teste_common.exe
 *
 * Aceita o JSON padrao (RFC 8259) com profundidade ate JSON_PROFUNDIDADE_MAX. Numero guarda so a parte inteira (o forja so imprime inteiros nos campos que o
 * corrigir le); texto sai decodificado em UTF-8 (\uXXXX e pares substitutos incluidos). Tudo vive na Arena de quem chama: nada a liberar por valor.
 */
#ifndef FORJA_DRIVER_JSON_LEITOR_H
#define FORJA_DRIVER_JSON_LEITOR_H

#include "common/arena.h"

enum { JSON_PROFUNDIDADE_MAX = 32 };

typedef enum { JSON_NULO, JSON_BOOL, JSON_NUMERO, JSON_TEXTO, JSON_LISTA, JSON_OBJETO } JsonTipo;

typedef struct JsonValor JsonValor;
struct JsonValor {
    JsonTipo tipo;
    const char *chave;      /* nome do membro, se o valor e membro de objeto; senao NULL */
    const char *texto;      /* JSON_TEXTO: decodificado, com NUL final */
    int64_t numero;         /* JSON_NUMERO: parte inteira; JSON_BOOL: 0 ou 1 */
    JsonValor *filho;       /* JSON_LISTA e JSON_OBJETO: primeiro item ou membro (NULL se vazio) */
    JsonValor *prox;        /* proximo irmao */
};

/* Le UM valor JSON de texto[0..tam) (espacos antes e depois sao ignorados; qualquer outra coisa depois do valor invalida). NULL = JSON invalido. */
JsonValor *JsonLer(Arena *arena, const char *texto, size_t tam);

/* Membro 'chave' do objeto (o primeiro, se repetida); NULL se 'objeto' nao e objeto ou nao tem a chave. */
const JsonValor *JsonMembro(const JsonValor *objeto, const char *chave);

/* Texto do membro 'chave'; NULL se ausente ou se nao for texto. */
const char *JsonTextoDe(const JsonValor *objeto, const char *chave);

/* Numero inteiro do membro 'chave'; 'padrao' se ausente ou se nao for numero. */
int64_t JsonNumeroDe(const JsonValor *objeto, const char *chave, int64_t padrao);

#endif
