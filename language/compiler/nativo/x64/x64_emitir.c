/* x64_emitir.c - emissao de MInstr em .text: codificacao, relocacoes (call, lea [rip]), saltos locais, padding e erro X0901.
 * Versao: 0.1.4
 * Changelog: 0.1.4 - IA1-b12: X64Emitir avisa o x64_rastro.c de cada call emitido, so com c->rastreio.
 *            0.1.3 - portao M1: PosDoErro unico para X0901/X0902; prologo e epilogo (sem instrucao) reportam a pos da 1a instrucao do corpo (c->f->pos), nao 0.
 *            0.1.2 - backend x64 P8b: X64ListarInstr sem o parametro dos bytes.
 *            0.1.1 - backend x64 P8: X64Emitir alimenta a listagem do --debug-code (so com c->lista).
 *            0.1.0 - backend x64 P7a: X64Emitir, X64Offset, X64CorrigirSaltos (jcc/jmp rel32 sempre), X64Padding (0xCC) e X64SemRegra.
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 */
#include "nativo/x64/x64_interno.h"

#define EMITIR_CAMPO_REL 4u

static ObjSecao *SecaoTexto(const X64Ctx *c)
{
    return VecEm(&c->o->secoes, c->texto);
}

uint32_t X64Offset(const X64Ctx *c)
{
    return (uint32_t)SecaoTexto(c)->bytes.tam;
}

/* O rel32 e sempre os 4 ultimos bytes de jmp, jcc, call e lea [rip+d32]: a relocacao REL32 cai em fim-4. */
static void RegistrarOperando(X64Ctx *c, const MOperando *op, uint32_t fim)
{
    ObjAlvoReloc onde = {c->texto, fim - EMITIR_CAMPO_REL};

    if (op->tipo == MO_BLOCO) {
        SaltoLocal *s = VecEmpurrar(&c->saltos);
        s->fim = fim;
        s->bloco = (uint32_t)op->imed;
    } else if (op->tipo == MO_SIMBOLO || (op->tipo == MO_MEM && op->base == X64_RIP)) {
        ObjRelocar(c->o, onde, (ObjCorrecao){OR_REL32, op->simbolo, 0});
    }
}

/* O codificador diz so o molde da forma; a causa final traz a instrucao inteira (registradores, deslocamento, imediato). */
static bool RecusadaComTexto(const X64Ctx *c, const MInstr *mi)
{
    Buf texto = {0};
    bool resultado;

    X64Formatar(mi, &texto);
    resultado = DiagRelatar(c->erro, X0902, mi->pos, "codificador x64 recusou a instrucao '%s'", texto.dados != NULL ? texto.dados : "?");
    BufLiberar(&texto);
    return resultado;
}

/* Posicao dos erros: a da instrucao em selecao; no prologo e no epilogo (sem instrucao) a da 1a instrucao do corpo; 0 se nenhuma tem. */
static uint32_t PosDoErro(const X64Ctx *c)
{
    uint32_t pos = c->instr != NULL ? c->instr->pos : (c->f != NULL ? c->f->pos : BIR_SEM_POS);

    return pos != BIR_SEM_POS ? pos : 0;
}

bool X64Emitir(X64Ctx *c, MInstr mi)
{
    uint8_t bytes[X64_TAM_MAX_INSTR];
    uint32_t tam = 0;

    mi.pos = PosDoErro(c);
    if (!X64CodificarInstr(&mi, bytes, &tam, c->erro))
        return RecusadaComTexto(c, &mi);
    BufAnexarN(&SecaoTexto(c)->bytes, (const char *)bytes, tam);
    for (uint8_t k = 0; k < mi.nops; k++)
        RegistrarOperando(c, &mi.op[k], X64Offset(c));
    if (c->lista != NULL)
        X64ListarInstr(c, &mi, tam);
    if (c->rastreio)
        X64RastroChamada(c, &mi);
    return true;
}

/* Saltos locais sao sempre rel32: destino (inicio do bloco) menos o fim da instrucao, little-endian, nos 4 ultimos bytes dela. */
void X64CorrigirSaltos(X64Ctx *c)
{
    ObjSecao *texto = SecaoTexto(c);

    for (size_t k = 0; k < c->saltos.n; k++) {
        const SaltoLocal *s = VecEm(&c->saltos, k);
        int32_t rel = (int32_t)(c->bloco_off[s->bloco] - s->fim);

        memcpy(texto->bytes.dados + s->fim - EMITIR_CAMPO_REL, &rel, EMITIR_CAMPO_REL);
    }
    c->saltos.n = 0;
}

void X64Padding(X64Ctx *c)
{
    ObjSecao *texto = SecaoTexto(c);
    const char padding = (char)X64_BYTE_PADDING;

    while (texto->bytes.tam % X64_ALINHAMENTO_FUNCAO != 0)
        BufAnexarN(&texto->bytes, &padding, 1);
}

bool X64SemRegra(const X64Ctx *c, const char *nome, const char *motivo)
{
    return DiagRelatar(c->erro, X0901, PosDoErro(c),"selecao de instrucoes sem regra para a BirOp %s: %s", nome, motivo);
}
