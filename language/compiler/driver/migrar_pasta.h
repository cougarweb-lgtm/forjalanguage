/* migrar_pasta.h - quais arquivos o 'forja migrar' le: um arquivo, ou os *.fj de uma pasta (recursivo) (IA1-L3d1).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L3d1: MigrarColetar.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_MIGRAR_PASTA_H
#define FORJA_DRIVER_MIGRAR_PASTA_H

#include "common/arena.h"
#include "common/vec.h"

/* Acrescenta a 'lista' (Vec de const char *, texto na Arena) o que 'entrada' pede: um arquivo vale como esta; uma pasta vira os *.fj dela e das subpastas, em ordem de nome
 * (sem diferenciar maiuscula), ignorando as subpastas cujo nome comeca por "language_", ".git" ou "backup" e os atalhos de pasta (reparse points). A propria pasta da
 * entrada nunca e ignorada. false = a entrada nao existe. */
bool MigrarColetar(const char *entrada, Arena *arena, Vec *lista);

#endif
