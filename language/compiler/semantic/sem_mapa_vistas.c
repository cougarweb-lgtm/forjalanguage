/* sem_mapa_vistas.c - embutidos de map que devolvem ou esvaziam o conjunto (IA1-L9e, K12): keys, values e clear.
 * Sintaxe (m e um map[K]V ou ^map[K]V; argumento posicional na ordem, ou nomeado em ordem crescente):
 *   keys(m, alloc = heap) -> [dynamic]K     lista dona NOVA com as chaves (copia rasa: str continua apontando para os bytes do map); o chamador da delete
 *   values(m, alloc = heap) -> [dynamic]V   lista dona NOVA com os valores (copia rasa)
 *   clear(&m)                               tira todos os pares; o map segue usavel (e vale delete(m) depois como sempre)
 * Ordem das chaves e dos valores = a ordem de iteracao do map (`for k, v in m`): a de INSERCAO enquanto nao ha remove; um remove poe o ultimo par no lugar do removido
 * e a ordem muda (IR_MAPA_REMOVE). keys e values numa mesma chamada de cada vem alinhadas (keys[i] e a chave de values[i]) se o map nao mudar entre elas.
 * m vazio = lista vazia (len 0). keys e values so leem o map (nao pedem var); clear altera (E0705 em let; dentro do `for` do mesmo map = E0318, como insert e remove).
 * Chave e valor de map nunca sao donos (E0318 ja barra no tipo); a conferencia E0311 existe por garantia caso a regra do tipo mude.
 * get_or(m, key, d) nao e embutido: `get(m, key = k) or_else d` ja faz (so doc).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9e: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

/* keys e values: (m, alloc). */
bool SemChamarVistasDoMapa(Semantica *s, Expr *e, int32_t embutido)
{
    char texto[96];
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_MAPA], 2)) return false;
    Expr *m = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, m, TIPO_VAZIO)) return false;
    TipoId mapa = TipoClasseDe(&s->tipos, m->tipo) == TC_PONTEIRO ? (TipoId)TipoDe(&s->tipos, m->tipo)->a : m->tipo;
    if (TipoClasseDe(&s->tipos, mapa) != TC_MAPA) {
        SemDescrever(s, m->tipo, texto, sizeof texto);
        return SemErro(s, E0301, m->pos, "%s pede map[K]V (tipo %s)", SemNomeTexto(s, e->nome), texto);
    }
    TipoId elemento = (TipoId)(embutido == EMB_KEYS ? TipoDe(&s->tipos, mapa)->a : TipoDe(&s->tipos, mapa)->b);
    if (!SemListaSemDono(s, e, elemento, "duplicaria") || !SemConferirAlocador(s, e, ArgumentoDoParametro(e, 1))) return false;
    e->tipo = TipoDinamico(&s->tipos, elemento);
    return true;
}

/* clear(&m): alvo e o 1o argumento, ja checado, de tipo map ou ^map (sem_lista.c decide pelo tipo). */
bool SemChamarClearMapa(Semantica *s, Expr *e, Expr *alvo)
{
    if (TipoClasseDe(&s->tipos, alvo->tipo) != TC_PONTEIRO) {
        return SemErro(s, E0318, alvo->pos, "clear recebe o endereco do map: clear(&m)");
    }
    if (!SemConferirClearMapa(s, alvo)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}
