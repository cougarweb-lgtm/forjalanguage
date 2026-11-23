/* objeto.c - objeto relocavel abstrato: secoes, simbolos e relocacoes; os nomes ficam na Arena do objeto.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M5b P35: ObjNovoSimbolo zera ObjSimbolo.fraco (binding WEAK do ELF; COFF fica false).
 *            0.2.0 - M2 P17c: ObjAcharSimbolo por tabela hash (Interner); era busca linear, O(S^2) no corpus de 200 mil funcoes.
 *            0.1.0 - backend x64 P3: ObjIniciar, ObjLiberar, ObjSecaoAbrir, ObjSimboloDefinir/Referir e ObjRelocar.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include "nativo/obj/objeto.h"

/* Potencia de 2: o alinhamento de secao de COFF e de ELF so aceita isso. */
static bool ObjEPotenciaDeDois(uint32_t v)
{
    return v != 0 && (v & (v - 1)) == 0;
}

void ObjIniciar(Objeto *o)
{
    memset(o, 0, sizeof *o);
    VecIniciar(&o->secoes, sizeof(ObjSecao));
    VecIniciar(&o->simbolos, sizeof(ObjSimbolo));
    VecIniciar(&o->relocs, sizeof(ObjReloc));
    InternerIniciar(&o->indice, &o->nomes);
}

void ObjLiberar(Objeto *o)
{
    for (size_t i = 0; i < o->secoes.n; i++)
        BufLiberar(&((ObjSecao *)VecEm(&o->secoes, i))->bytes);
    VecLiberar(&o->secoes);
    VecLiberar(&o->simbolos);
    VecLiberar(&o->relocs);
    InternerLiberar(&o->indice);
    ArenaLiberar(&o->nomes);
}

uint32_t ObjSecaoAbrir(Objeto *o, const char *nome, ObjTipoSecao tipo, uint32_t alinhamento)
{
    if (!ObjEPotenciaDeDois(alinhamento))
        FalhaInterna("objeto: alinhamento de secao '%s' nao e potencia de 2 (%u)", nome, alinhamento);
    ObjSecao *s = VecEmpurrar(&o->secoes);
    s->nome = ArenaCopiarTexto(&o->nomes, nome, strlen(nome));
    s->tipo = tipo;
    s->alinhamento = alinhamento;
    return (uint32_t)(o->secoes.n - 1);
}

int64_t ObjAcharSimbolo(const Objeto *o, const char *nome)
{
    return InternerAchar(&o->indice, nome, (uint32_t)strlen(nome));
}

/* O nome entra no Interner (que o copia para a arena do objeto): o id novo e sempre o proximo indice de simbolo. */
static uint32_t ObjNovoSimbolo(Objeto *o, const char *nome, int32_t secao, uint32_t off)
{
    o->indice.arena = &o->nomes;
    int32_t id = Internar(&o->indice, nome, (uint32_t)strlen(nome));
    if ((size_t)id != o->simbolos.n)
        FalhaInterna("objeto: indice de nomes fora de sincronia no simbolo '%s' (id %d, simbolos %zu)", nome, (int)id, o->simbolos.n);
    ObjSimbolo *s = VecEmpurrar(&o->simbolos);
    s->nome = InternerTexto(&o->indice, id);
    s->secao = secao;
    s->off = off;
    s->global = nome[0] != '.';
    s->fraco = false;
    return (uint32_t)(o->simbolos.n - 1);
}

uint32_t ObjSimboloDefinir(Objeto *o, const char *nome, int32_t secao, uint32_t off)
{
    int64_t i = ObjAcharSimbolo(o, nome);
    if (i < 0)
        return ObjNovoSimbolo(o, nome, secao, off);
    ObjSimbolo *s = VecEm(&o->simbolos, (size_t)i);
    if (s->secao != OBJ_SECAO_INDEFINIDA)
        FalhaInterna("objeto: simbolo '%s' definido duas vezes", nome);
    s->secao = secao;
    s->off = off;
    return (uint32_t)i;
}

uint32_t ObjSimboloReferir(Objeto *o, const char *nome)
{
    int64_t i = ObjAcharSimbolo(o, nome);
    if (i >= 0)
        return (uint32_t)i;
    return ObjNovoSimbolo(o, nome, OBJ_SECAO_INDEFINIDA, 0);
}

void ObjRelocar(Objeto *o, ObjAlvoReloc alvo, ObjCorrecao correcao)
{
    if (alvo.secao >= o->secoes.n)
        FalhaInterna("objeto: relocacao numa secao inexistente (%u)", alvo.secao);
    if (correcao.simbolo >= o->simbolos.n)
        FalhaInterna("objeto: relocacao para um simbolo inexistente (%u)", correcao.simbolo);
    ObjReloc *r = VecEmpurrar(&o->relocs);
    r->alvo = alvo;
    r->correcao = correcao;
}
