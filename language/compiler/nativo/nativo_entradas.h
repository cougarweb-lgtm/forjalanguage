/* nativo_entradas.h - o arquivo .entradas do build x64 (--entradas): tudo o que o build leu, com tamanho, mtime e hash, para o orquestrador de projeto decidir o que recompilar (P22-O1a do M3).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b: o tipo "pasta" (e o "load" do B12, ate aqui so no codigo): a pasta de um modulo de pasta; o hash e o Hash64 da lista dos nomes dos .fj dela (common\pasta_fj.h).
 *            0.1.0 - M3 P22-O1a: NativoEntradasGravar.
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#ifndef FORJA_NATIVO_NATIVO_ENTRADAS_H
#define FORJA_NATIVO_NATIVO_ENTRADAS_H

#include "nativo/nativo.h"
#include "nativo/obj/bibliotecas.h"

/* Grava pedido->entradas de forma atomica (ArquivoGravarAtomico). Formato (texto, UTF-8, LF):
 *   # forja-entradas 1
 *   <fj|lib> TAB <caminho absoluto> TAB <tamanho em bytes> TAB <mtime em unidades de 100 ns (FILETIME)> TAB <hash64 em 16 digitos hex minusculos>
 * uma linha por arquivo lido, em ordem de caminho (strcmp), sem repeticao:
 *   fj  = o .fj raiz e cada modulo de arquivo (relativo, vendor:, engine:); a std embutida fica de fora (o hash do forja.exe a cobre). O hash e o Hash64 do TEXTO que o
 *         compilador leu, ja sem o BOM UTF-8 (nao relê o arquivo); tamanho e mtime vem do arquivo, no fim do build.
 *   pasta = a pasta de um modulo de pasta (import "dir"); tamanho 0, mtime da pasta, hash = Hash64 da lista dos nomes dos .fj que a carga viu (cada nome e um LF): o orquestrador
 *         recalcula pela pasta de hoje (PastaHashDeHoje), entao .fj acrescentado ou removido invalida o build; o conteudo de cada .fj tem a linha "fj" propria.
 *   lib = cada DLL, exe ou import library que o ligador leu por inteiro; o hash e o dos bytes lidos. As do System32 ficam de fora (sao do Windows, nao do projeto).
 * libs pode ser NULL (build sem resolucao de bibliotecas). false = nao gravou; *errno_saida (NULL = nao quer) recebe o errno. */
bool NativoEntradasGravar(const NativoPedido *pedido, const BibliotecasResolvidas *libs, int *errno_saida);

#endif
