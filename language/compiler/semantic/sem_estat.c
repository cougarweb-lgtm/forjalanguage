/* sem_estat.c - embutidos de estatistica de lista, array e fatia (IA1-L9e, K07) e a porta dos 8 embutidos novos da L9e: min_of, max_of, sum, avg, keys, values,
 * from_bytes, as_bytes. Os de map (sem_mapa_vistas.c) e de texto (sem_texto_bytes.c) moram nos seus arquivos; aqui ficam os nomes, o registro e o despacho.
 * Sintaxe (argumento posicional; s e uma lista, um array ou uma fatia de inteiros ou reais, sem exigir var porque so le):
 *   min_of(s) -> ?T       o menor elemento; s vazia = none        max_of(s) -> ?T       o maior; s vazia = none
 *   sum(s) -> T           a soma na largura de T; s vazia = 0     avg(s) -> f64         a media (soma em f64 / len); s vazia = 0.0 (nao e erro)
 * Real com NaN: min_of e max_of ignoram NaN (o resultado so e NaN se todos forem); sum e avg propagam NaN. Inteiro: sum estoura = R0002 em dev e da a volta em release
 * (como `a + b`); avg nao estoura (soma em f64, e inteiro de 64 bits acima de 2^53 perde precisao).
 * Elemento que nao e inteiro nem real (bool, str, enum, struct...) = E0301.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-L9f: to_str (EMB_TO_STR) entra na porta: nome reservado e despacho para SemChamarToStr (sem_texto_builder.c).
 *            0.1.0 - IA1-L9e: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

#include <string.h>

/* Na ordem de EMB_MIN_OF..EMB_TO_STR (ast.h). */
static const char *const NOMES[] = {"min_of", "max_of", "sum", "avg", "keys", "values", "from_bytes", "as_bytes", "to_str"};
enum { NOMES_QTD = sizeof NOMES / sizeof NOMES[0] };

_Static_assert(NOMES_QTD == EMB_TO_STR - EMB_MIN_OF + 1, "NOMES precisa seguir EmbutidoId");

static int32_t NomeInterno(Semantica *s, const char *texto)
{
    return Internar(s->nomes, texto, (uint32_t)strlen(texto));
}

void SemEstatInternar(Semantica *s)
{
    for (size_t k = 0; k < NOMES_QTD; k++) NomeInterno(s, NOMES[k]);
}

void SemEstatRegistrar(Semantica *s)
{
    for (int k = 0; k < NOMES_QTD; k++) {
        Simbolo *y = &s->globais[NomeInterno(s, NOMES[k])];
        y->categoria = (uint8_t)SIM_PROC_EMBUTIDO;
        y->indice = EMB_MIN_OF + k;
        y->tipo = TIPO_VAZIO;
    }
}

/* min_of, max_of, sum e avg: (colecao) de elemento numerico. */
bool SemChamarEstatistica(Semantica *s, Expr *e, int32_t embutido)
{
    char texto[96];
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_COLECAO], 1)) return false;
    if (!SemChecar(s, ArgumentoDoParametro(e, 0)->valor, TIPO_VAZIO)) return false;
    if (!SemColecaoJaChecada(s, e, 0, false, &elemento)) return false;
    if (!TipoEhInteiro(elemento) && !TipoEhReal(&s->tipos, elemento)) {
        SemDescrever(s, elemento, texto, sizeof texto);
        return SemErro(s, E0301, e->pos_op, "%s pede elemento numerico (inteiro ou real); %s nao serve", SemNomeTexto(s, e->nome), texto);
    }
    if (embutido == EMB_SUM) e->tipo = elemento;
    else if (embutido == EMB_AVG) e->tipo = TIPO_F64;
    else e->tipo = TipoOpcional(&s->tipos, elemento);
    return true;
}

bool SemChamarEstat(Semantica *s, Expr *e, int32_t embutido)
{
    switch (embutido) {
    case EMB_MIN_OF:
    case EMB_MAX_OF:
    case EMB_SUM:
    case EMB_AVG: return SemChamarEstatistica(s, e, embutido);
    case EMB_KEYS:
    case EMB_VALUES: return SemChamarVistasDoMapa(s, e, embutido);
    case EMB_FROM_BYTES: return SemChamarFromBytes(s, e);
    case EMB_AS_BYTES: return SemChamarAsBytes(s, e);
    case EMB_TO_STR: return SemChamarToStr(s, e);
    default: FalhaInterna("SemChamarEstat: embutido %d desconhecido", (int)embutido);
    }
}
