/* dbg_coletar.c - coleta do DbgPrograma (D-M6-2): procs, linhas (offset, pos), variaveis e chamadas, chamados pelos ganchos do gerador x64.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6b P76d: DbgPosicaoComDica (a busca de linha pela dica, antes em pdb_escrever.c; agora tambem do DWARF).
 *            0.1.0 - M6 P65b: primeira versao.
 * Verificacao: construir.bat && python ..\tests\nativo\m6\verificar_linhas.py --forja .\forja.exe
 */
#include <string.h>

#include "common/nome_modulo.h"
#include "nativo/dbg/dbg.h"

#define DBG_PREFIXO_FUNCAO "fj_"

void DbgIniciar(DbgPrograma *d)
{
    memset(d, 0, sizeof *d);
    VecIniciar(&d->procs, sizeof(DbgProc));
    VecIniciar(&d->linhas, sizeof(DbgLinha));
    VecIniciar(&d->vars, sizeof(DbgVar));
    VecIniciar(&d->chamadas, sizeof(DbgChamada));
    d->main_pos = BIR_SEM_POS;
}

void DbgLiberar(DbgPrograma *d)
{
    VecLiberar(&d->procs);
    VecLiberar(&d->linhas);
    VecLiberar(&d->vars);
    VecLiberar(&d->chamadas);
}

/* Nome Forja do proc: o do simbolo sem o "fj_", e o de modulo com o nome puro. */
static const char *NomeForja(const char *simbolo)
{
    size_t pre = strlen(DBG_PREFIXO_FUNCAO);
    const char *nome = strncmp(simbolo, DBG_PREFIXO_FUNCAO, pre) == 0 ? simbolo + pre : simbolo;

    return NomeModuloExibicao(nome);
}

static DbgProc *ProcEmCurso(DbgPrograma *d)
{
    return VecEm(&d->procs, d->procs.n - 1);
}

void DbgProcAbrir(DbgPrograma *d, const BirFuncao *f, const DbgAbertura *a)
{
    const char *simbolo = a->simbolo;
    DbgProc *p;

    d->aberto = !f->sintetica;
    if (!d->aberto)
        return;
    if (strcmp(simbolo, "fj_main") == 0)
        d->main_pos = f->pos;
    p = VecEmpurrar(&d->procs);
    p->nome = NomeForja(simbolo);
    p->simbolo = simbolo;
    p->pos = f->pos;
    p->inicio = a->inicio;
    p->fim = a->inicio;
    p->linha_ini = (uint32_t)d->linhas.n;
    p->var_ini = (uint32_t)d->vars.n;
    p->rbp_quadro = a->quadro;
    d->ultima_pos = BIR_SEM_POS;
}

void DbgVariavel(DbgPrograma *d, const DbgVar *v)
{
    DbgProc *p;

    if (!d->aberto)
        return;
    *(DbgVar *)VecEmpurrar(&d->vars) = *v;
    p = ProcEmCurso(d);
    p->nvars++;
    if (v->parametro)
        p->nparams++;
}

void DbgLinhaVista(DbgPrograma *d, uint32_t off, uint32_t pos)
{
    DbgLinha *l;

    if (!d->aberto || pos == BIR_SEM_POS || pos == d->ultima_pos)
        return;
    l = VecEmpurrar(&d->linhas);
    l->off = off;
    l->pos = pos;
    d->ultima_pos = pos;
    ProcEmCurso(d)->nlinhas++;
}

void DbgProcFechar(DbgPrograma *d, uint32_t fim)
{
    if (!d->aberto)
        return;
    ProcEmCurso(d)->fim = fim;
    d->aberto = false;
}

/* Linha da dica (a da posicao anterior) ou a seguinte: inicio_linha[k] e o 1o byte da linha k+1. Acertar aqui poupa a busca binaria da Fonte. */
static bool LinhaDaDica(const Fonte *f, uint32_t pos, uint32_t *dica)
{
    uint32_t base = *dica;

    for (uint32_t k = base; k <= base + 1 && k < f->ninicios; k++) {
        bool na_linha = f->inicio_linha[k] <= pos && (k + 1 >= f->ninicios || pos < f->inicio_linha[k + 1]);

        if (na_linha) {
            *dica = k;
            return true;
        }
    }
    return false;
}

FontePosicao DbgPosicaoComDica(const Fonte *f, uint32_t pos, uint32_t *dica)
{
    static const char CAMINHO_VAZIO[] = "";
    FontePosicao fp;

    if (f->inicio_linha != NULL && f->ntrechos <= 1 && pos < f->tam && LinhaDaDica(f, pos, dica)) {
        fp.caminho = f->caminho != NULL ? f->caminho : CAMINHO_VAZIO;
        fp.linha = *dica + 1;
        fp.coluna = 0;
        return fp;
    }
    fp = FonteLocalizarNoArquivo(f, pos);
    if (fp.caminho == NULL)
        fp.caminho = CAMINHO_VAZIO;
    if (f->inicio_linha != NULL)
        *dica = fp.linha - 1;
    return fp;
}

void DbgChamadaVista(DbgPrograma *d, uint32_t ra, uint32_t pos)
{
    DbgChamada *c = VecEmpurrar(&d->chamadas);

    c->ra = ra;
    c->pos = pos;
}
