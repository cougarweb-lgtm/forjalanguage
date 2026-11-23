/* migrar_comentario.h - 'forja migrar --comentarios': os comentarios '//' de um arquivo (IA1-L3d2).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L3d2: MigrarComentarios.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 *
 * O lexer entrega onde cada '//' comeca e termina (Lexer.comentarios). Comentarios de linha inteira em linhas seguidas formam um BLOCO; comentario depois de codigo e um bloco de uma
 * linha. O texto de um bloco (o que vem depois de '//', uma linha por linha) e testado como Forja de tres jeitos, o primeiro que o parser aceitar vence: arquivo (declaracoes),
 * corpo de um proc (comandos) e valor de um 'let' (expressao). Bloco que nao parseia e desfeito em linhas, e cada linha tem a mesma chance. Parseou: as formas antigas dele saem
 * de migrar_nomes.c e viram trocas na MESMA linha e coluna (em bytes) do comentario, como as do codigo. Nao parseou (prosa, mistura): nada e editado; se o texto tem forma antiga
 * (NomesSoltos), vai para a lista de revisao. Qualificador antigo duvidoso dentro de comentario que parseia tambem vai para a lista.
 */
#ifndef FORJA_DRIVER_MIGRAR_COMENTARIO_H
#define FORJA_DRIVER_MIGRAR_COMENTARIO_H

#include "driver/migrar_analise.h"

/* trocas: ordenadas por (linha, coluna), com comentario = true; revisoes: na ordem do texto. Tudo na Arena de quem chamou. */
typedef struct {
    MigrarTroca *trocas;
    uint32_t ntrocas;
    MigrarRevisao *revisoes;
    uint32_t nrevisoes;
} ComentariosSaida;

/* 'texto' (tam bytes, sem BOM) e o conteudo do arquivo 'caminho', que compila. */
void MigrarComentarios(const char *caminho, const char *texto, size_t tam, Arena *arena, ComentariosSaida *saida);

#endif
