/* migrar_nomes.h - as regras por NOME do 'forja migrar --comentarios' (IA1-L3d2): quais formas antigas da tabela de renomeados um texto de comentario que parseia como Forja usa.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L3d2: NomesDoCodigo (tokens do Forja) e NomesSoltos (texto qualquer, so a lista).
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 *
 * Um comentario nao tem arquivo em volta: nao ha import para a semantica resolver, entao a resolucao e por TOKEN, sobre as formas da tabela que aparecem QUALIFICADAS ou rotuladas:
 *   - import "std:arquivo" / "vendor:banco/banco" / "vendor:banco/x" (caminho do modulo e pasta do pacote);
 *   - rotulo em = / para = dentro da chamada de make new delete format format_c / transmute, e o valor escopo de alloc = ;
 *   - qualificador antigo (arquivo. banco. compressao. rede.) quando o membro depois do ponto e conhecido (std:file, tabela do modulo) ou o proprio texto importa o modulo antigo;
 *   - proc, tipo e membro de vendor depois do qualificador (banco.abrir, db.ErroBanco.nao_abriu) e enum sem qualificador (ErroBanco.nao_abriu);
 *   - rotulo de argumento (REN_PARAM) em chamada qualificada de vendor (json.dump(bonito = true)).
 * Nome solto (uma palavra que so por acaso e igual a um nome antigo) nunca troca.
 */
#ifndef FORJA_DRIVER_MIGRAR_NOMES_H
#define FORJA_DRIVER_MIGRAR_NOMES_H

#include "common/arena.h"
#include "common/renomeados.h"
#include "common/vec.h"
#include "frontend/token.h"

/* Uma forma antiga: onde esta (off e tam, em bytes, no texto lexado), a classe e as duas grafias. Nas duvidas, novo = NULL. */
typedef struct { uint32_t off, tam; RenClasse classe; const char *antigo, *novo; } NomeTroca;

/* Os tokens de um texto ja lexado; so valem os de off em [ini, fim) (o resto e moldura que o chamador acrescentou). */
typedef struct {
    const char *texto;
    const Token *tokens;
    size_t ntokens;
    uint32_t ini, fim;
} NomesEntrada;

/* Soma em 'trocas' (Vec de NomeTroca) as formas antigas que valem troca, na ordem dos tokens. Em 'duvidas' (Vec de NomeTroca com novo NULL): qualificador antigo com membro
 * desconhecido e sem import no texto (pode ser variavel): fica para revisao manual. */
void NomesDoCodigo(const NomesEntrada *e, Arena *arena, Vec *trocas, Vec *duvidas);

/* Sem lexer: as formas antigas num texto qualquer (NUL no fim, 'tam' bytes), cada uma uma vez, em 'formas' (Vec de const char *, na Arena). So a lista; nunca troca. */
void NomesSoltos(const char *texto, size_t tam, Arena *arena, Vec *formas);

#endif
