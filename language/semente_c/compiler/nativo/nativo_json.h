/* nativo_json.h - escrita de JSON em Buf para o backend nativo: texto escapado, caminho com '/' e o hash FNV-1a de 32 bits.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - backend x64 P8b: JsonTexto, JsonCaminho e Fnv32 (campos extras dos erros, pacote de relato e debug.jsonl).
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#ifndef FORJA_NATIVO_NATIVO_JSON_H
#define FORJA_NATIVO_NATIVO_JSON_H

#include "common/buf.h"

/* Anexa o texto entre aspas: '"' e '\' com barra, controles (< 0x20) como \u00xx; bytes >= 0x80 (UTF-8) seguem crus. */
void JsonTexto(Buf *saida, const char *texto, size_t n);

/* Como JsonTexto sobre uma string C, trocando '\' por '/' (o mesmo caminho em qualquer maquina). */
void JsonCaminho(Buf *saida, const char *caminho);

#define FNV32_INICIO 2166136261u

/* FNV-1a de 32 bits: continua o hash 'h' (FNV32_INICIO no primeiro bloco) com os n bytes. */
uint32_t Fnv32(uint32_t h, const void *dados, size_t n);

#endif
