/* bir_construir_soa_mapa_valor.c - chave de map por valor na construcao da BIR: chave inteira (i32, u32, int, uint, enum) de 4 ou 8 bytes sem texto vai ao runtime por valor (forja_mapa_<nome>_k4|k8), sem slot temporario.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M1 split: ChaveEhPorValor e ChamarMapaPorValor saem de bir_construir_soa_mapa.c (no limite de 800 linhas), sem mudar o codigo; ArquivoELinha entra como copia static local (como em bir_construir_aloca.c e bir_construir_afirma.c).
 * Verificacao: construir.bat && python -I ..\tests\nativo\conferir_regras.py; a saida dos programas com map nao muda.
 */
#include "nativo/bir/bir_interno.h"

/* Arquivo (vreg com o texto) e linha (imediato) do .fj: os dois ultimos argumentos de todo servico. Copia static local: a versao de bir_construir_soa_mapa.c nao e publica. */
static void ArquivoELinha(BirCtx *c, const IrInstr *i, BirOperando saida[2])
{
    uint32_t linha;

    saida[0] = BirVregDoArquivo(c, i->pos, &linha);
    saida[1] = OpImed(linha, BC_I32);
}

/* Chave inteira (i32, u32, int, uint ou enum) de 4 ou 8 bytes, sem texto: vai por valor ao runtime (k4 ou k8). */
bool ChaveEhPorValor(BirCtx *c, const MapaRef *r)
{
    bool inteira = r->chave == TIPO_I32 || r->chave == TIPO_U32 || r->chave == TIPO_INT || r->chave == TIPO_UINT
                   || TipoClasseDe(c->e->tipos, r->chave) == TC_ENUM;

    return !r->texto && inteira && (r->forma_chave.tam == 4 || r->forma_chave.tam == 8);
}

/* forja_mapa_<nome>_k4|k8(m, chave, [SV], arquivo, linha): a chave (imediato vira vreg) vai no registrador, sem slot. */
bool ChamarMapaPorValor(BirCtx *c, const PedidoMapa *p, uint32_t *resultado)
{
    uint32_t nargs = p->servico->com_valor ? 5u : 4u, k = 2;
    BirOperando *args = BirArgs(c, nargs);
    BirOperando chave;
    const char *funcao = p->mapa->forma_chave.tam == 8 ? p->servico->funcao_k8 : p->servico->funcao_k4;

    if (!BirOperandoDe(c, p->i, &p->i->b, &chave) || !BirOperandoVreg(c, chave, &chave))
        return false;
    args[0] = OpVreg(p->mapa->endereco, BC_I64);
    args[1] = chave;
    if (p->servico->com_valor) {
        args[2] = OpImed(p->mapa->forma_valor.tam, BC_I64);
        k = 3;
    }
    ArquivoELinha(c, p->i, &args[k]);
    *resultado = BirChamarRuntime(c, &(BirChamadaRt){funcao, p->servico->retorno, nargs}, args);
    return true;
}
