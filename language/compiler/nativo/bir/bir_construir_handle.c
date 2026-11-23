/* bir_construir_handle.c - IR -> BIR: IR_HANDLE_NASCE e IR_HANDLE_SOLTA, o rastreio automatico do foreign com clausula creates/destroys/takes (M6c P88, D-M6c-6).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6d P92e: BirRegraLibEncerra (IR_LIB_ENCERRA): dev chama forja_libmem_encerrou(cstr simbolo); release nao emite nada.
 *            0.1.0 - M6c P88: primeira versao. Dev: forja_handle_nasce(kind, h, indireto, destruidor) e forja_handle_solta(h) do rt_handles.c. Release: as duas regras nao emitem NADA
 *            (nenhuma instrucao BIR, nenhum slot, nenhum texto no .rdata, nenhum operando lido): o exe de quem tem a clausula sai byte a byte igual ao de quem nao tem.
 * Verificacao: python tests\nativo\m6c\verificar_m6c.py --forja compiler\forja.exe --alvo ambos --grupo handle --release-igual
 */
#include "nativo/bir/bir_interno.h"

/* O argumento 64 bits do handle: ponteiro ou inteiro (estendido pelo tipo), sempre num vreg. */
static bool HandleEmVreg(BirCtx *c, const IrInstr *i, BirOperando *saida)
{
    return BirInteiro64(c, i, &i->a, saida) && BirOperandoVreg(c, *saida, saida);
}

/* Texto de nome internado (kind ou destruidor) como endereco de string C num vreg; destruidor ausente = imediato 0 (NULL). */
static BirOperando TextoDoNome(BirCtx *c, int32_t nome)
{
    if (nome < 0)
        return OpImed(0, BC_I64);
    return OpVreg(BirVregDoTexto(c, InternerTexto(c->e->nomes, nome), InternerTam(c->e->nomes, nome)), BC_I64);
}

/* IR_HANDLE_NASCE: nome = criador (kind) | a = handle devolvido, ou o ponteiro de saida se op_aux = 1 | b = V_TEXTO do destruidor (V_NENHUM = nenhum). */
bool BirRegraHandleNasce(BirCtx *c, const IrInstr *i)
{
    BirOperando *args;
    BirOperando handle;

    if (c->p->modo != BM_DEV)
        return true;
    if (!HandleEmVreg(c, i, &handle))
        return false;
    args = BirArgs(c, 4);
    args[0] = TextoDoNome(c, i->nome);
    args[1] = handle;
    args[2] = OpImed(i->op_aux, BC_I32);
    args[3] = TextoDoNome(c, i->b.tipo_no == V_TEXTO ? i->b.id : -1);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_handle_nasce", BC_NADA, 4}, args);
    return true;
}

/* IR_LIB_ENCERRA: nome = simbolo C do proc `ends`; dev avisa o runtime que a lib encerrou. */
bool BirRegraLibEncerra(BirCtx *c, const IrInstr *i)
{
    BirOperando *args;

    if (c->p->modo != BM_DEV)
        return true;
    args = BirArgs(c, 1);
    args[0] = TextoDoNome(c, i->nome);
    BirChamarRuntime(c, &(BirChamadaRt){"forja_libmem_encerrou", BC_NADA, 1}, args);
    return true;
}

/* IR_HANDLE_SOLTA: a = o handle que o destroys/takes recebe. */
bool BirRegraHandleSolta(BirCtx *c, const IrInstr *i)
{
    BirOperando *args;
    BirOperando handle;

    if (c->p->modo != BM_DEV)
        return true;
    if (!HandleEmVreg(c, i, &handle))
        return false;
    args = BirArgs(c, 1);
    args[0] = handle;
    BirChamarRuntime(c, &(BirChamadaRt){"forja_handle_solta", BC_NADA, 1}, args);
    return true;
}
