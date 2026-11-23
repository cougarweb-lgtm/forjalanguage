/* sem_texto_busca.c - embutidos de busca e reverse com str no 1o argumento (S2a): index_of, last_index_of, contains e count, e reverse(str, alloc = A).
 * Sintaxe (argumento posicional na ordem, ou nomeado em ordem crescente):
 *   index_of(s, value = sub) -> ?int       last_index_of(s, value = sub) -> ?int     contains(s, value = sub) -> bool     count(s, value = sub) -> int
 *   reverse(s, alloc = A) -> str           str nova POR RUNA (igual a utf8.str_reverse); o alloc e obrigatorio, o texto novo e dono dele.
 * A busca compara BYTES (igual a text.str_*); value tambem e str. value que nao e str = E0301 (o byte unico e text.index_byte). reverse sem alloc = E0503.
 * O 1o argumento ja foi checado e e str; cstr no 1o argumento segue E0310 (sem_lista_busca.c). O lowering e ir\lower_texto_busca.c.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - S2a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

#include <stdio.h>

/* value de index_of, last_index_of, contains e count: o argumento 1 tem de ser str. */
static bool ValorDeTextoEhStr(Semantica *s, Expr *e)
{
    char texto[96];
    Expr *valor = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, valor, TIPO_VAZIO)) return false;
    if (valor->tipo == TIPO_STR) return true;
    SemDescrever(s, valor->tipo, texto, sizeof texto);
    return SemErro(s, E0301, valor->pos, "%s de str pede value str (tipo %s); byte unico: text.index_byte", SemNomeTexto(s, e->nome), texto);
}

/* index_of, last_index_of, contains e count sobre str: o tipo do resultado e o de cada um (igual a versao de lista). */
bool SemChamarBuscaTexto(Semantica *s, Expr *e, int32_t embutido)
{
    if (!ValorDeTextoEhStr(s, e)) return false;
    if (embutido == EMB_CONTAINS) e->tipo = TIPO_BOOL;
    else if (embutido == EMB_COUNT) e->tipo = TIPO_INT;
    else e->tipo = TipoOpcional(&s->tipos, TIPO_INT);
    return true;
}

/* reverse(str, alloc = A) -> str: o alloc e obrigatorio (E0503, como o do ConferirFaltantes), e o alocador do texto novo. */
bool SemChamarReverseTexto(Semantica *s, Expr *e)
{
    Arg *alocador = ArgumentoDoParametro(e, 1);
    if (alocador == NULL) return SemErro(s, E0503, e->pos_op, "falta o argumento 'alloc'");
    if (!SemConferirAlocador(s, e, alocador)) return false;
    e->tipo = TIPO_STR;
    return true;
}
