/* bir_construir_sysv.c - descritor SysV de um agregado por valor que cruza para um foreign de fora no alvo linux: as folhas (escalares) com o deslocamento
 * e o alinhamento de cada uma, pelo layout/ (nada escrito a mao). Agregado acima de 16 bytes e MEMORY no SysV: leva uma folha falsa so para passar
 * a conferencia do classificador. Tipo sem folha conhecida (lista, str, union...) = NULL, e o chamador recusa com X0001.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.1.0 - M5b P37: BirDescritorSysv (struct aninhada e escalar; ate 16 folhas, pois com 16 bytes ha no maximo 16 escalares).
 * Verificacao: construir.bat && teste_nativo.exe (casos linux do P37); o descritor nao entra no dump da BIR (verificar_bir nos dourados).
 */
#include "nativo/bir/bir_interno.h"

#define BIR_SYSV_FOLHAS_MAX 16u
#define BIR_SYSV_TAM_REG 16u

typedef struct { BirFolhaSysv *folhas; uint32_t n; } FolhasSysv;

static uint32_t TamDaClasseSysv(uint8_t classe)
{
    switch (classe) {
    case BC_I8: return 1;
    case BC_I16: return 2;
    case BC_I32: case BC_F32: return 4;
    case BC_I64: case BC_F64: return 8;
    default: return 0;
    }
}

static bool AcrescentarFolha(FolhasSysv *f, uint8_t classe, uint32_t deslocamento)
{
    uint32_t ali = TamDaClasseSysv(classe);

    if (ali == 0 || f->n >= BIR_SYSV_FOLHAS_MAX)
        return false;
    f->folhas[f->n++] = (BirFolhaSysv){.sse = BirClasseEhReal(classe), .deslocamento = deslocamento, .alinhamento = ali};
    return true;
}

static bool FolhasDoTipo(BirCtx *c, TipoId tipo, uint32_t base, FolhasSysv *f);

/* Cada campo da struct, no deslocamento que o layout/ deu a ele dentro do pai. */
static bool FolhasDaStruct(BirCtx *c, TipoId tipo, uint32_t base, FolhasSysv *f)
{
    const IrStruct *s = LayoutEstrutura(c->e->tipos, c->e->ir, tipo);
    const Layouts *l = BirLayouts(c);

    if (s == NULL)
        return false;
    for (uint32_t k = 0; k < s->ncampos; k++) {
        if (!FolhasDoTipo(c, s->campos[k].tipo, base + LayoutOffsetCampo(l, tipo, k), f))
            return false;
    }
    return true;
}

static bool FolhasDoTipo(BirCtx *c, TipoId tipo, uint32_t base, FolhasSysv *f)
{
    uint8_t classe;

    if (BirClasseDoTipo(c->e->tipos, tipo, &classe))
        return AcrescentarFolha(f, classe, base);
    if (TipoClasseDe(c->e->tipos, tipo) != TC_STRUCT)
        return false;
    return FolhasDaStruct(c, tipo, base, f);
}

/* Preenche o descritor de um tipo agregado; NULL se o tipo tem forma que o SysV desta versao nao descreve. */
static bool PreencherDescritor(BirCtx *c, TipoId tipo, uint32_t tam, FolhasSysv *f)
{
    if (tam > BIR_SYSV_TAM_REG) {
        f->folhas[0] = (BirFolhaSysv){.sse = 0, .deslocamento = 0, .alinhamento = 8};
        f->n = 1;
        return true;
    }
    return FolhasDoTipo(c, tipo, 0, f);
}

const BirSysv *BirDescritorSysv(BirCtx *c, TipoId tipo)
{
    BirSysv *descritor;
    FolhasSysv f;
    BirSlot forma;

    if (!BirForma(c, tipo, &forma))
        return NULL;
    descritor = ArenaAlocar(&c->p->arena, sizeof *descritor);
    f.folhas = ArenaAlocar(&c->p->arena, BIR_SYSV_FOLHAS_MAX * sizeof *f.folhas);
    f.n = 0;
    if (!PreencherDescritor(c, tipo, forma.tam, &f))
        return NULL;
    *descritor = (BirSysv){.tam = forma.tam, .n = f.n, .folhas = f.folhas};
    return descritor;
}
