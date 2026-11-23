/* sem_lista_busca.c - embutidos de busca e movimento sobre lista, array e fatia (IA1-L9d, K03, K04 e K05): index_of, last_index_of, contains, count, copy, fill,
 * clone, swap, reverse, replace_value. 's' e uma lista ([dynamic]T, ^[dynamic]T), um array ([N]T, ^[N]T, [dynamic, N]T; variavel, campo ou elemento) ou uma fatia
 * ([]T); a semantica troca lista e array por s[..] (EX_FATIA sintetica, sem exigir var quando so le), entao o resto sempre ve uma fatia. Os que alteram elementos pedem
 * var (E0705) em lista e array por valor; a fatia e ponteiro nao pedem (como no sort).
 * Sintaxe (argumento posicional na ordem, ou nomeado em ordem crescente):
 *   index_of(s, value = v) -> ?int        1a posicao de v (none se nao ha)        last_index_of(s, value = v) -> ?int   a ultima
 *   contains(s, value = v) -> bool        count(s, value = v) -> int            elemento comparavel: inteiro, real, bool, str (por conteudo) ou enum
 *   copy(dst = d, src = s) -> int         copia min(len d, len s) elementos; vale com sobreposicao (como memmove)
 *   fill(s, value = v)                    todos os elementos = v
 *   clone(s, alloc = heap) -> [dynamic]T  lista dona nova com copia RASA dos elementos; o chamador da delete
 *   swap(s, i = a, j = b)                 troca os elementos a e b (fora = R0006 em dev)
 *   reverse(s)                            inverte a ordem (lista, array, fatia); reverse(str, alloc = A) -> str (S2a, sem_texto_busca.c)
 *   replace_value(s, old = a, new = b, n = k)   troca ate k ocorrencias de a por b (sem n: todas)
 * Elemento dono de memoria nao entra em copy, fill e clone (duplicaria o dono): E0311; struct nao entra nos que comparam: E0301.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - S2a: index_of, last_index_of, contains e count com str no 1o argumento desviam para sem_texto_busca.c; reverse ganha ASS_COLECAO_ALLOC (alloc com lista = E0502, sobra argumento).
 *            0.2.0 - IA1-L9e: ColecaoDoArgumento se divide em SemColecaoJaChecada (publica; sem_estat.c e sem_texto_bytes.c) e a checagem do argumento; clone aceita str (SemChamarCloneTexto).
 *            0.1.0 - IA1-L9d: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

#include <stdio.h>

/* Variavel, campo, elemento ou desreferencia: o que o array precisa ser para virar s[..] (o endereco vem do lowering). */
static bool ArrayEnderecavel(const Expr *v)
{
    return v->tipo_no == EX_NOME || v->tipo_no == EX_CAMPO || v->tipo_no == EX_INDICE || (v->tipo_no == EX_UNARIO && v->op == OP_DEREF);
}

/* s[..] sem a checagem de var do SemFatia: a semantica ja decidiu quem pode alterar. */
static Expr *FatiaInteira(Semantica *s, Expr *base, TipoId elemento)
{
    Expr *fatia = ArenaAlocar(s->arena, sizeof *fatia);
    fatia->tipo_no = EX_FATIA;
    fatia->op = FATIA_INTEIRA;
    fatia->a = base;
    fatia->pos = base->pos;
    fatia->pos_op = base->pos_op;
    fatia->tipo = TipoFatia(&s->tipos, elemento);
    return fatia;
}

/* A colecao do parametro k de e: confere a forma e o var, troca o argumento por uma fatia e devolve o elemento. alteracao: o embutido muda os elementos. */
bool SemColecaoJaChecada(Semantica *s, Expr *e, int32_t k, bool alteracao, TipoId *elemento)
{
    char texto[96], motivo[96];
    ColecaoForma forma;
    Arg *arg = ArgumentoDoParametro(e, k);
    Expr *v = arg->valor;
    *elemento = SemElementoDe(s, v->tipo, &forma);
    if (forma == FORMA_NENHUMA) {
        SemDescrever(s, v->tipo, texto, sizeof texto);
        return SemErro(s, E0310, v->pos, "%s pede lista, array ou fatia (tipo %s)", SemNomeTexto(s, e->nome), texto);
    }
    if (forma == FORMA_FATIA) return true;
    snprintf(motivo, sizeof motivo, "%s altera os elementos: declare com var", SemNomeTexto(s, e->nome));
    if (alteracao && (forma == FORMA_ARRAY || forma == FORMA_DINAMICO) && !SemLvalorMutavelDe(s, v, motivo)) return false;
    if (forma == FORMA_ARRAY && !ArrayEnderecavel(v)) {
        return SemErro(s, E0310, v->pos, "%s precisa de um array que seja variavel, campo ou elemento (nao um valor temporario)", SemNomeTexto(s, e->nome));
    }
    arg->valor = FatiaInteira(s, v, *elemento);
    return true;
}

/* Idem, checando o argumento antes. */
static bool ColecaoDoArgumento(Semantica *s, Expr *e, int32_t k, bool alteracao, TipoId *elemento)
{
    return SemChecar(s, ArgumentoDoParametro(e, k)->valor, TIPO_VAZIO) && SemColecaoJaChecada(s, e, k, alteracao, elemento);
}

/* index_of, last_index_of, contains e count: (colecao, value). Com str no 1o argumento, vai para sem_texto_busca.c (S2a). */
static bool ChamarProcura(Semantica *s, Expr *e, int32_t embutido)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_COLECAO_VALOR], 2) || !SemChecar(s, ArgumentoDoParametro(e, 0)->valor, TIPO_VAZIO)) return false;
    if (ArgumentoDoParametro(e, 0)->valor->tipo == TIPO_STR) return SemChamarBuscaTexto(s, e, embutido);
    if (!SemColecaoJaChecada(s, e, 0, false, &elemento)) return false;
    if (!SemListaComparavel(s, e, elemento) || !SemListaValorDoElemento(s, ArgumentoDoParametro(e, 1)->valor, elemento)) return false;
    if (embutido == EMB_CONTAINS) e->tipo = TIPO_BOOL;
    else if (embutido == EMB_COUNT) e->tipo = TIPO_INT;
    else e->tipo = TipoOpcional(&s->tipos, TIPO_INT);
    return true;
}

/* copy(dst = d, src = s): mesmo tipo de elemento nos dois; dst e alterado. */
static bool ChamarCopy(Semantica *s, Expr *e)
{
    char a[96], b[96];
    TipoId destino, origem;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_COPY], 2)) return false;
    if (!ColecaoDoArgumento(s, e, 0, true, &destino) || !ColecaoDoArgumento(s, e, 1, false, &origem)) return false;
    if (destino != origem) {
        SemDescrever(s, destino, a, sizeof a);
        SemDescrever(s, origem, b, sizeof b);
        return SemErro(s, E0301, ArgumentoDoParametro(e, 1)->valor->pos, "copy pede o mesmo tipo de elemento nos dois: %s e %s", a, b);
    }
    if (!SemListaSemDono(s, e, destino, "duplicaria")) return false;
    e->tipo = TIPO_INT;
    return true;
}

/* fill(s, value = v). */
static bool ChamarFill(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_COLECAO_VALOR], 2) || !ColecaoDoArgumento(s, e, 0, true, &elemento)) return false;
    if (!SemListaSemDono(s, e, elemento, "duplicaria") || !SemListaValorDoElemento(s, ArgumentoDoParametro(e, 1)->valor, elemento)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* clone(s, alloc = A): [dynamic]T novo (o alocador como no make); com s do tipo str, a copia dos bytes (str dona, sem_texto_bytes.c). */
static bool ChamarClone(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_CLONE], 2)) return false;
    Expr *alvo = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, alvo, TIPO_VAZIO)) return false;
    if (alvo->tipo == TIPO_STR) return SemChamarCloneTexto(s, e);
    if (!SemColecaoJaChecada(s, e, 0, false, &elemento)) return false;
    if (!SemListaSemDono(s, e, elemento, "duplicaria") || !SemConferirAlocador(s, e, ArgumentoDoParametro(e, 1))) return false;
    e->tipo = TipoDinamico(&s->tipos, elemento);
    return true;
}

/* swap(s, i = a, j = b). */
static bool ChamarSwap(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_SWAP], 3) || !ColecaoDoArgumento(s, e, 0, true, &elemento)) return false;
    if (!SemListaInteiro(s, ArgumentoDoParametro(e, 1)->valor, "o indice") || !SemListaInteiro(s, ArgumentoDoParametro(e, 2)->valor, "o indice")) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* reverse(s): lista, array ou fatia inverte no lugar (sem alloc); str devolve outra str (alloc obrigatorio, sem_texto_busca.c). */
static bool ChamarReverse(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_COLECAO_ALLOC], 2)) return false;
    Expr *alvo = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, alvo, TIPO_VAZIO)) return false;
    if (alvo->tipo == TIPO_STR) return SemChamarReverseTexto(s, e);
    if (ArgumentoDoParametro(e, 1) != NULL) return SemErro(s, E0502, ArgumentoDoParametro(e, 1)->valor->pos, "sobra argumento: reverse de lista, array ou fatia so leva a colecao (alloc so existe em str)");
    if (!SemColecaoJaChecada(s, e, 0, true, &elemento)) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* replace_value(s, old = a, new = b, n = k): n opcional (quantidade maxima de trocas). */
static bool ChamarReplace(Semantica *s, Expr *e)
{
    TipoId elemento;
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_REPLACE], 4) || !ColecaoDoArgumento(s, e, 0, true, &elemento)) return false;
    if (!SemListaComparavel(s, e, elemento)) return false;
    if (!SemListaValorDoElemento(s, ArgumentoDoParametro(e, 1)->valor, elemento) || !SemListaValorDoElemento(s, ArgumentoDoParametro(e, 2)->valor, elemento)) return false;
    Arg *n = ArgumentoDoParametro(e, 3);
    if (n != NULL && !SemListaInteiro(s, n->valor, "n")) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

bool SemChamarBusca(Semantica *s, Expr *e, int32_t embutido)
{
    switch (embutido) {
    case EMB_INDEX_OF:
    case EMB_LAST_INDEX_OF:
    case EMB_CONTAINS:
    case EMB_COUNT: return ChamarProcura(s, e, embutido);
    case EMB_COPY: return ChamarCopy(s, e);
    case EMB_FILL: return ChamarFill(s, e);
    case EMB_CLONE: return ChamarClone(s, e);
    case EMB_SWAP: return ChamarSwap(s, e);
    case EMB_REVERSE: return ChamarReverse(s, e);
    case EMB_REPLACE_VALUE: return ChamarReplace(s, e);
    default: FalhaInterna("SemChamarBusca: embutido %d desconhecido", (int)embutido);
    }
}
