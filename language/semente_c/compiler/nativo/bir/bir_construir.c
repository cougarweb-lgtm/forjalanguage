/* bir_construir.c - IR -> BIR: funcoes, mapa de locais e temporarios para vregs, locais, atribuicao, chamada, retorno, print e os erros X0001/X0904.
 * Versao:0.16.10
 * Changelog: 0.16.10 - M8 P2: ProgramaInicial copia BirEntrada.perfil para BirPrograma.perfil.
 *            0.16.9 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.16.8 - M7b F3: ProgramaInicial copia BirEntrada.simbolos_dbg para BirPrograma.simbolos_dbg.
 *            0.16.7 -M7a F3: BirLiberar libera BirFuncao.reg_vreg.
 *            0.16.6 - Ryu d2fixed P4: some forja_print_f32 da tabela PRINTS (codigo morto: a semantica reescreve o print de real em f"{x}"); real em IR_IMPRIME cai no X0001 como f64.
 *            0.16.5 - M6e P93k: ProgramaInicial copia BirEntrada.guarda para BirPrograma.guarda.
 *            0.16.4 - M6 P65a: ConstruirFuncao tira o retrato dos locais nomeados no fim (BirRetratarLocais).
 *            0.16.3 - M5b P39-B1 (D0654): ProgramaInicial copia BirEntrada.nomes_mem para BirPrograma.nomes_mem (copia NULL = nomes do Windows).
 *            0.16.2 - M5b P38b: o vetor de funcoes ganha 2 por trampolim (fjt_ e a ponte Win64 fjtm_ no alvo linux).
 *            0.16.1 - M5b P37: ProgramaInicial copia BirEntrada.sysv para BirPrograma.sysv (so o alvo linux liga a ABI SysV do foreign).
 *            0.16.0 - M4 P24p: com BirEntrada.poda, RegistrarSimbolosDasFuncoes e ConstruirFuncoes pulam o proc que o BirAlcance nao marcou (FuncaoNaBir); BirPrograma.procs_total/procs_gerados.
 *            0.15.1 - M3 F9b-2: decisoes extraidas de BirClasseDoTipo, BirOperandoDe, BirRegraImprime, BirConstruirInstr e PrepararPrograma.
 *            0.15.0 - M3 P22a: BirEntrada.dll: PrepararPrograma nao exige main e ConstruirTudo constroi os involucros de export (BirConstruirExports) no lugar do forja_programa; o vetor de funcoes ganha 2 * nfuncoes.
 *            0.14.0 - M3 P21v-c: BirClasseDoTipo cobre proc (TC_PROC, BC_I64: o endereco da funcao); o X0001 de proc como valor some.
 *            0.13.0 - M3 P21: V_PROC vira operando (BirOperandoProc, bir_construir_fronteira.c); ConstruirTudo constroi os trampolins fjt_ depois das funcoes; o vetor de funcoes ganha ntrampolins.
 *            0.12.0 - P18a: com BirEntrada.traco, BirConstruirInstr chama BirTracar depois da regra e ConstruirFuncao numera a funcao (IrNumerarFuncao).
 *            0.11.0 - M2 P16: BirClasseDoTipo cobre bit_set (BC_I8/I16/I32/I64 pelo numero de membros; BirClasseDoConjunto de bir_construir_v19.c).
 *            0.10.0 - M2 P15a: BirClasseDoTipo cobre ?cstr (BC_I64, nulo = none); print de cstr em variavel ou parametro (forja_print_texto).
 *            0.9.0 - M2 P13a: BirClasseDoTipo cobre cstr (BC_I64); literal de cstr em BirOperandoDe (BirTextoCstr); print de str (BirImprimeStr).
 *            0.8.0 - M2 P12b: BirEntrada.teste: as funcoes 'test' entram no programa (simbolo fj_<nome interno>) e forja_programa e o executor (BirProgramaTeste, bir_construir_teste.c).
 *            0.7.0 - M2 P12a: BirClasseDoTipo cobre enum (BC_I32); literal de enum sai com o valor C do membro (BirInteiroDoValor); BirCtx.main_tipo e forja_programa de main -> ! E.
 *            0.6.0 - M2 P11b: assinatura, chamada e retorno saem para bir_construir_chamada.c (que cobre struct por valor); aqui ficam locais, atribuicao e print.
 *            0.5.0 - M2 P11a: BirClasseDoTipo cobre ^T, ?^T e rawptr (BC_I64); none (V_NULO) vira o imediato 0; leitura de local escalar com '&' e CARREGA do slot; IR_LOCAL e
 *                    IR_ATRIBUICAO de struct, de campo e de escalar em memoria vao para bir_construir_mem.c; BirVregTemp publica; BirLiberar libera os slots.
 *            0.4.0 - M2 P10b: BirClasseDoTipo recebe os Tipos e cobre f32, f64 e unidade (BC_F32/BC_F64); literal real em BirOperandoDe (BirImediatoReal); print de f32
 *                    chama forja_print_f32; o X0001 de real em local/parametro/retorno/chamada some.
 *            0.3.0 - M2 P9: BirClasseDoTipo cobre i8/u8 (BC_I8), i16/u16 (BC_I16) e u32 (BC_I32); PRINTS ganha i8 i16 u8 u16 u32.
 *            0.2.1 - M2 P9a: as falhas formatam a causa em buffer local (BirFalhaInconsistente sem varargs); mesmas mensagens.
 *            0.2.0 - backend x64 P8b: BirNomeDoTipo (publico) e a varredura do --suporte (BirVarrerSuporte: segue depois de cada X0001 e junta todos).
 *            0.1.0 - backend x64 P6: primeira versao. Classes bool/i32/int/uint e cstr literal so como argumento de print; forja_programa sintetico
 *                    (CHAMA fj_main; RETORNA #0:i32); funcoes de teste ficam fora (nunca sao chamadas fora do modo teste, que e X0003 no driver).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define MARCO_PENDENTE "M2"
#define TEXTO_SEM_RETORNO "fim da proc sem retorno (IR)"
#define NOME_X0001_MAX 256
#define CAUSA_INCONSISTENTE_MAX 400

/* ---- erros ---- */

static void AcrescentarParte(char *dst, size_t cap, const char *parte)
{
    size_t n = strlen(dst);

    if (parte == NULL || strcmp(parte, "-") == 0)
        return;
    snprintf(dst + n, cap - n, "%s%s", n > 0 ? " " : "", parte);
}

/* "<IrOp> <variante> <tipo> ainda nao suportado no backend nativo (marco Mx)"; parte "-" ou NULL some. */
bool BirFalhaX0001(BirCtx *c, uint32_t pos, const char *const partes[3], const char *marco)
{
    char nome[NOME_X0001_MAX] = "";

    for (int k = 0; k < 3; k++)
        AcrescentarParte(nome, sizeof nome, partes[k]);
    return DiagRelatar(c->d, X0001, pos == BIR_SEM_POS ? 0 : pos, "%s ainda nao suportado no backend nativo (marco %s)", nome, marco);
}

static const char *NomeDaClasse(TipoClasse classe)
{
    switch (classe) {
    case TC_STRUCT: return "struct";
    case TC_ENUM: return "enum";
    case TC_PONTEIRO: return "ponteiro";
    case TC_RESULTADO: return "resultado";
    case TC_REAL: return "real";
    case TC_ARRAY: return "array";
    case TC_FATIA: return "fatia";
    case TC_DINAMICO: return "lista";
    case TC_OPCIONAL: case TC_ANULAVEL: return "opcional";
    case TC_UNIAO: return "union";
    case TC_TUPLA: return "tupla";
    case TC_PROC: return "callback";
    default: return "tipo";
    }
}

const char *BirNomeDoTipo(const Tipos *tipos, TipoId tipo)
{
    const char *fixo = TipoNomeFixo(tipo);

    return fixo != NULL ? fixo : NomeDaClasse(TipoClasseDe(tipos, tipo));
}

static const char *NomeDoTipo(const BirCtx *c, TipoId tipo)
{
    return BirNomeDoTipo(c->e->tipos, tipo);
}

bool BirFalhaTipo(BirCtx *c, const IrInstr *i, const char *variante, TipoId tipo)
{
    return BirFalhaX0001(c, i->pos, (const char *const[3]){BirNomeDoOp(i->op), variante, NomeDoTipo(c, tipo)}, MARCO_PENDENTE);
}

bool BirFalhaInconsistente(BirCtx *c, const char *causa)
{
    uint32_t pos = c->atual != NULL ? c->atual->pos : 0;

    return DiagRelatar(c->d, X0904, pos, "IR inconsistente para a BIR: %s", causa);
}

/* ---- valores da IR ---- */

/* Os tipos escalares nomeados (inteiros, bool e f64). */
static bool ClasseDoTipoPrimitivo(TipoId tipo, uint8_t *classe)
{
    switch (tipo) {
    case TIPO_BOOL: case TIPO_I8: case TIPO_U8: *classe = BC_I8; return true;
    case TIPO_I16: case TIPO_U16: *classe = BC_I16; return true;
    case TIPO_I32: case TIPO_U32: *classe = BC_I32; return true;
    case TIPO_INT: case TIPO_UINT: *classe = BC_I64; return true;
    case TIPO_F64: *classe = BC_F64; return true;
    default: return false;
    }
}

/* ^T, ?^T, rawptr, cstr, ?cstr e proc: o valor e um endereco. */
static bool TipoEhEndereco(const Tipos *tipos, TipoId tipo)
{
    switch (TipoClasseDe(tipos, tipo)) {
    case TC_PONTEIRO: case TC_ANULAVEL: case TC_RAWPTR: case TC_CSTR: case TC_PROC: return true;
    default: return TipoOpcionalNicho(tipos, tipo);
    }
}

bool BirClasseDoTipo(const Tipos *tipos, TipoId tipo, uint8_t *classe)
{
    if (ClasseDoTipoPrimitivo(tipo, classe))
        return true;
    if (TipoEhEndereco(tipos, tipo)) {
        *classe = BC_I64; /* ^T, ?^T, rawptr, cstr, ?cstr e proc: o endereco */
        return true;
    }
    if (TipoClasseDe(tipos, tipo) == TC_ENUM) {
        *classe = BC_I32; /* enum = int32 de 4 bytes (layout/), valor C do membro */
        return true;
    }
    if (BirClasseDoConjunto(tipos, tipo, classe))
        return true; /* bit_set: o inteiro sem sinal da largura do numero de membros */
    if (!TipoEhReal(tipos, tipo))
        return false;
    *classe = BC_F32; /* f32 e unidade (TC_REAL de 32 bits) */
    return true;
}

static bool IndiceValido(int32_t id, uint32_t n)
{
    return id >= 0 && (uint32_t)id < n;
}

/* Declara o vreg de um local ou temporario; uma 2a definicao (mesmo numero na IR) reaproveita o vreg, que nao e SSA. */
static bool DeclararNoMapa(BirCtx *c, int32_t *mapa, int32_t id, uint8_t classe)
{
    if (mapa[id] < 0)
        mapa[id] = (int32_t)BirNovoVreg(c, classe);
    if (c->f->classe_vreg[mapa[id]] != classe) {
        char causa[CAUSA_INCONSISTENTE_MAX];

        snprintf(causa, sizeof causa, "numero %d redefinido com outra classe", id);
        return BirFalhaInconsistente(c, causa);
    }
    return true;
}

bool BirVregDeclarar(BirCtx *c, int32_t local, uint8_t classe, uint32_t *vreg)
{
    if (!IndiceValido(local, c->n_local)) {
        char causa[CAUSA_INCONSISTENTE_MAX];

        snprintf(causa, sizeof causa, "local %d fora de nlocais (%u)", local, (unsigned)c->n_local);
        return BirFalhaInconsistente(c, causa);
    }
    if (!DeclararNoMapa(c, c->vreg_local, local, classe))
        return false;
    *vreg = (uint32_t)c->vreg_local[local];
    return true;
}

bool BirDefinirDestino(BirCtx *c, uint8_t classe, uint32_t *vreg)
{
    int32_t destino = c->atual->destino;

    if (!IndiceValido(destino, c->n_temp)) {
        char causa[CAUSA_INCONSISTENTE_MAX];

        snprintf(causa, sizeof causa, "temporario %d fora de ntemps (%u)", destino, (unsigned)c->n_temp);
        return BirFalhaInconsistente(c, causa);
    }
    if (!DeclararNoMapa(c, c->vreg_temp, destino, classe))
        return false;
    *vreg = (uint32_t)c->vreg_temp[destino];
    return true;
}

bool BirVregLocal(BirCtx *c, int32_t local, uint32_t *vreg)
{
    if (!IndiceValido(local, c->n_local) || c->vreg_local[local] < 0) {
        char causa[CAUSA_INCONSISTENTE_MAX];

        snprintf(causa, sizeof causa, "local %d usado antes de declarado", local);
        return BirFalhaInconsistente(c, causa);
    }
    *vreg = (uint32_t)c->vreg_local[local];
    return true;
}

bool BirVregTemp(BirCtx *c, int32_t id, uint32_t *vreg)
{
    if (!IndiceValido(id, c->n_temp) || c->vreg_temp[id] < 0) {
        char causa[CAUSA_INCONSISTENTE_MAX];

        snprintf(causa, sizeof causa, "temporario %d usado antes de definido", id);
        return BirFalhaInconsistente(c, causa);
    }
    *vreg = (uint32_t)c->vreg_temp[id];
    return true;
}

static bool EhCstrLiteral(const Valor *v)
{
    return v->tipo_no == V_TEXTO && v->tipo == TIPO_CSTR;
}

/* Texto que nao e cstr ou valor sem conteudo: nao vira operando. */
static bool ValorSemOperando(const Valor *v)
{
    return (v->tipo_no == V_TEXTO && v->tipo != TIPO_CSTR) || v->tipo_no == V_NENHUM;
}

static bool RealEmClasseInteira(const Valor *v, uint8_t classe)
{
    return v->tipo_no == V_REAL && !BirClasseEhReal(classe);
}

static bool EhImediatoReal(const Valor *v, uint8_t classe)
{
    return BirClasseEhReal(classe) && (v->tipo_no == V_REAL || v->tipo_no == V_INT);
}

static bool EhImediatoInteiro(const Valor *v)
{
    return v->tipo_no == V_INT || v->tipo_no == V_BOOL || v->tipo_no == V_NULO;
}

static bool EhLocalEmMemoria(const BirCtx *c, const Valor *v)
{
    return v->tipo_no == V_LOCAL && v->id >= 0 && (uint32_t)v->id < c->n_local && c->em_memoria[v->id];
}

/* O vreg do local ou temporario como operando. */
static bool OperandoDeVreg(BirCtx *c, const Valor *v, BirOperando *saida)
{
    uint32_t vreg = 0;

    if (v->tipo_no == V_TEMP ? !BirVregTemp(c, v->id, &vreg) : !BirVregLocal(c, v->id, &vreg))
        return false;
    *saida = OpVreg(vreg, c->f->classe_vreg[vreg]);
    return true;
}

/* Valor numerico, bool, local ou temporario (a classe ja saiu do tipo). */
static bool OperandoNumerico(BirCtx *c, const Valor *v, uint8_t classe, BirOperando *saida)
{
    if (EhImediatoReal(v, classe))
        return BirImediatoReal(v, classe, saida);
    if (EhImediatoInteiro(v)) { /* none = endereco 0 */
        *saida = OpImed(v->tipo_no == V_NULO ? 0 : BirInteiroDoValor(c, v), classe);
        return true;
    }
    if (EhLocalEmMemoria(c, v))
        return BirLerLocalEmMemoria(c, v, saida);
    return OperandoDeVreg(c, v, saida);
}

/* Valor da IR -> operando. Tipo fora do M2 (e valor que nao e numero, bool, local ou temporario) sai X0001. */
bool BirOperandoDe(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida)
{
    uint8_t classe = BC_NADA;

    if (v->tipo_no == V_PROC)
        return BirOperandoProc(c, v, saida);
    if (EhCstrLiteral(v))
        return BirTextoCstr(c, v, saida);
    if (!BirClasseDoTipo(c->e->tipos, v->tipo, &classe) || ValorSemOperando(v) || RealEmClasseInteira(v, classe))
        return BirFalhaTipo(c, i, BirVarianteDe(c, i), v->tipo);
    return OperandoNumerico(c, v, classe, saida);
}

bool BirOperandoVreg(BirCtx *c, BirOperando o, BirOperando *saida)
{
    uint32_t vreg;

    if (o.tipo != BO_IMED) {
        *saida = o;
        return true;
    }
    vreg = BirNovoVreg(c, o.classe);
    BirMover(c, vreg, o);
    *saida = OpVreg(vreg, o.classe);
    return true;
}

/* ---- simbolos de texto ---- */

void BirArquivoELinha(const BirCtx *c, uint32_t pos, const char **caminho, uint32_t *linha)
{
    FontePosicao p = FonteLocalizarNoArquivo(c->e->fonte, pos);

    *caminho = p.caminho;
    *linha = p.linha;
}

uint32_t BirVregDoTexto(BirCtx *c, const char *bytes, uint32_t tam)
{
    uint32_t simbolo = BirSimboloTexto(c, bytes, tam);
    uint32_t vreg = BirNovoVreg(c, BC_I64);

    BirEmitirPedido(c, (BirPedido){.op = BIR_END_SIMBOLO, .dst = vreg, .a = OpSimbolo(simbolo)});
    return vreg;
}

/* ---- locais, atribuicao, chamada, retorno e print ---- */

bool BirRegraLocal(BirCtx *c, const IrInstr *i)
{
    uint8_t classe = BC_NADA;
    uint32_t vreg = 0;
    BirOperando origem;

    BirRegistrarTipoLocal(c, i->destino, i->tipo);
    if (BirLocalEmMemoria(c, i))
        return BirRegraLocalMem(c, i);
    if (!BirClasseDoTipo(c->e->tipos, i->tipo, &classe))
        return BirFalhaTipo(c, i, "-", i->tipo);
    if (!BirVregDeclarar(c, i->destino, classe, &vreg))
        return false;
    origem = OpImed(0, classe); /* a = V_NENHUM: o local nasce zerado */
    if (i->a.tipo_no != V_NENHUM && !BirOperandoDe(c, i, &i->a, &origem))
        return false;
    BirMover(c, vreg, origem);
    return true;
}

/* O local inteiro em registrador; campo, struct inteira e escalar com '&' (memoria) vao para BirRegraAtribuicaoMem. */
bool BirRegraAtribuicao(BirCtx *c, const IrInstr *i)
{
    uint32_t vreg = 0;
    BirOperando origem;

    if (BirAtribuicaoEmMemoria(c, i))
        return BirRegraAtribuicaoMem(c, i);
    if (!BirVregLocal(c, i->destino, &vreg) || !BirOperandoDe(c, i, &i->a, &origem))
        return false;
    BirMover(c, vreg, origem);
    return true;
}

typedef struct { TipoId tipo; const char *funcao; uint8_t extensao; } BirPrint;

/* forja_print_* do runtime nativo: o argumento de 8, 16 ou 32 bits segue estendido a 64 (com sinal em i8 i16 i32, sem sinal nos demais). */
static const BirPrint PRINTS[] = {
    {TIPO_I8, "forja_print_i32", BE_SINAL},
    {TIPO_I16, "forja_print_i32", BE_SINAL},
    {TIPO_I32, "forja_print_i32", BE_SINAL},
    {TIPO_INT, "forja_print_i64", BE_NENHUMA},
    {TIPO_U8, "forja_print_u64", BE_ZERO},
    {TIPO_U16, "forja_print_u64", BE_ZERO},
    {TIPO_U32, "forja_print_u64", BE_ZERO},
    {TIPO_UINT, "forja_print_u64", BE_NENHUMA},
    {TIPO_BOOL, "forja_print_bool", BE_ZERO},
};

static void ChamarRuntimeUmArg(BirCtx *c, const char *funcao, BirOperando arg)
{
    BirOperando *args = BirArgs(c, 1);
    BirInstr *chama;

    args[0] = arg;
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, funcao);
    chama->args = args;
    chama->nargs = 1;
}

static bool ImprimirTexto(BirCtx *c, const IrInstr *i)
{
    const char *texto = InternerTexto(c->e->nomes, i->a.id);
    uint32_t vreg = BirVregDoTexto(c, texto, InternerTam(c->e->nomes, i->a.id));

    ChamarRuntimeUmArg(c, "forja_print_texto", OpVreg(vreg, BC_I64));
    return true;
}

/* cstr em variavel ou parametro: o mesmo forja_print_texto do literal. */
static bool ImprimirCstrVariavel(BirCtx *c, const IrInstr *i)
{
    BirOperando valor;

    if (!BirOperandoDe(c, i, &i->a, &valor) || !BirOperandoVreg(c, valor, &valor))
        return false;
    ChamarRuntimeUmArg(c, "forja_print_texto", valor);
    return true;
}

/* Escalar pela tabela PRINTS; real nao tem print por aqui (a semantica o reescreve em f"{x}") e segue X0001. */
static bool ImprimirEscalar(BirCtx *c, const IrInstr *i)
{
    BirOperando valor;
    TipoId tipo = i->a.tipo != TIPO_F64 && TipoEhReal(c->e->tipos, i->a.tipo) ? TIPO_F32 : i->a.tipo;

    for (uint32_t k = 0; k < sizeof PRINTS / sizeof PRINTS[0]; k++) {
        if (PRINTS[k].tipo != tipo)
            continue;
        if (!BirOperandoDe(c, i, &i->a, &valor))
            return false;
        ChamarRuntimeUmArg(c, PRINTS[k].funcao, OpComExtensao(valor, PRINTS[k].extensao));
        return true;
    }
    return BirFalhaTipo(c, i, "-", i->a.tipo);
}

bool BirRegraImprime(BirCtx *c, const IrInstr *i)
{
    if (i->a.tipo_no == V_TEXTO && i->a.tipo == TIPO_CSTR)
        return ImprimirTexto(c, i);
    if (i->a.tipo == TIPO_STR)
        return BirImprimeStr(c, i);
    if (i->a.tipo == TIPO_CSTR)
        return ImprimirCstrVariavel(c, i);
    return ImprimirEscalar(c, i);
}

/* ---- despacho ---- */

static bool RegraUtilizavel(const BirRegra *regra)
{
    return regra != NULL && regra->estado == BR_SUPORTADO && regra->fn != NULL;
}

static bool BirConstruirInstr(BirCtx *c, const IrInstr *i)
{
    const IrInstr *anterior = c->atual;
    const char *variante = BirVarianteDe(c, i);
    const BirRegra *regra = BirAcharRegra(i->op, variante);
    bool ok;

    c->atual = i;
    if (!RegraUtilizavel(regra))
        ok = BirFalhaX0001(c, i->pos, (const char *const[3]){BirNomeDoOp(i->op), variante, NULL},
                           regra != NULL ? regra->marco : MARCO_PENDENTE);
    else
        ok = regra->fn(c, i) && (!c->e->traco || BirTracar(c, i)); /* P18a: o traco entra com a posicao da propria instrucao */
    c->atual = anterior;
    return ok;
}

/* Varredura do --suporte: guarda o X0001 que acabou de sair (os X0904 em cascata de uma construcao recusada nao interessam) e segue adiante. */
static void RegistrarOcorrencia(BirCtx *c)
{
    if (c->d->codigo == X0001) {
        BirOcorrencia *o = VecEmpurrar(c->varredura);

        o->pos = c->d->off;
        snprintf(o->causa, sizeof o->causa, "%s", c->d->causa);
    }
    c->d->tem = false;
}

bool BirConstruirBloco(BirCtx *c, const IrBloco *bloco)
{
    for (const IrInstr *i = bloco->primeira; i != NULL; i = i->prox) {
        if (BirConstruirInstr(c, i))
            continue;
        if (c->varredura == NULL)
            return false;
        RegistrarOcorrencia(c);
    }
    return true;
}

/* ---- funcoes ---- */

static uint32_t PosDaFuncao(const IrFuncao *irf)
{
    return irf->corpo.primeira != NULL ? irf->corpo.primeira->pos : BIR_SEM_POS;
}

static int32_t *RedimensionarMapa(int32_t *mapa, uint32_t *cap, uint32_t n)
{
    if (n > *cap) {
        mapa = realloc(mapa, n * sizeof *mapa);
        if (mapa == NULL)
            FalhaInterna("bir: memoria esgotada nos mapas de vreg");
        *cap = n;
    }
    for (uint32_t k = 0; k < n; k++)
        mapa[k] = -1;
    return mapa;
}

static void PrepararMapas(BirCtx *c, const IrFuncao *irf)
{
    c->n_local = irf->nlocais;
    c->n_temp = irf->ntemps;
    c->vreg_local = RedimensionarMapa(c->vreg_local, &c->cap_local, irf->nlocais);
    c->vreg_temp = RedimensionarMapa(c->vreg_temp, &c->cap_temp, irf->ntemps);
    for (uint32_t k = 0; k < irf->nparams && k < irf->nlocais; k++)
        c->vreg_local[k] = (int32_t)k;
    BirMemPrepararFuncao(c, irf);
}

/* Corpo que acaba sem RETORNA: proc vazio devolve; com retorno cai no panic. */
static void FecharFuncao(BirCtx *c)
{
    BirOperando args[3];
    const char *arquivo = c->e->fonte->caminho;

    if (c->cur == BIR_SEM_BLOCO)
        return;
    if (c->f->retorno == BC_NADA) {
        BirRetorna(c, NULL);
        return;
    }
    args[0] = OpVreg(BirVregDoTexto(c, TEXTO_SEM_RETORNO, sizeof TEXTO_SEM_RETORNO - 1), BC_I64);
    args[1] = OpVreg(BirVregDoTexto(c, arquivo, (uint32_t)strlen(arquivo)), BC_I64);
    args[2] = OpImed(0, BC_I32);
    BirArmadilha(c, BirSimboloRuntime(c, "forja_panic"), args, 3);
}

static bool ConstruirFuncao(BirCtx *c, const IrFuncao *irf)
{
    BirFuncao *f = &c->p->funcoes[c->p->nfuncoes++];

    *f = (BirFuncao){.simbolo = c->simbolo_do_nome[irf->nome], .nparams = irf->nparams, .pos = PosDaFuncao(irf)};
    c->f = f;
    c->cap_vregs = 0;
    c->nlacos = 0;
    c->atual = NULL;
    if (!BirPrepararAssinatura(c, irf))
        return false;
    PrepararMapas(c, irf);
    if (c->e->traco) { /* P18a: a numeracao e o slot dos bits de real sao por funcao */
        c->funcao_ir = irf;
        c->slot_traco = -1;
        IrNumerarFuncao(irf, &c->p->arena, &c->num);
    }
    c->cur = BirNovoBloco(c, false);
    if (!BirConstruirBloco(c, &irf->corpo))
        return false;
    FecharFuncao(c);
    BirRetratarLocais(c, irf);
    if (irf->e_main) {
        c->main_retorno = f->retorno;
        c->main_tipo = irf->retorno;
    }
    return true;
}

/* forja_programa: a entrada do PE chama fj_main e sai com 0 (o resultado de main e descartado);
 * main -> T ! E sai com 1 e R0004 se veio erro (BirMainResultado). */
static bool ConstruirPrograma(BirCtx *c)
{
    BirFuncao *f = &c->p->funcoes[c->p->nfuncoes++];
    BirInstr *chama;
    BirOperando zero = OpImed(0, BC_I32);

    *f = (BirFuncao){.simbolo = BirSimboloGlobal(c, "forja_programa"), .retorno = BC_I32, .pos = BIR_SEM_POS, .sintetica = true};
    c->f = f;
    c->cap_vregs = 0;
    c->atual = NULL;
    c->cur = BirNovoBloco(c, false);
    if (c->e->teste)
        return BirProgramaTeste(c);
    if (TipoClasseDe(c->e->tipos, c->main_tipo) == TC_RESULTADO)
        return BirMainResultado(c);
    chama = BirEmitir(c, BIR_CHAMA, c->main_retorno);
    chama->simbolo = c->main_simbolo;
    BirRetorna(c, &zero);
    return true;
}

/* Aloca o mapa nome -> simbolo (todo SEM_SIMBOLO) e o vetor de funcoes da BIR. */
static void AlocarTabelasDoPrograma(BirCtx *c)
{
    const IrPrograma *ir = c->e->ir;

    c->n_nomes = (uint32_t)c->e->nomes->nomes.n;
    c->simbolo_do_nome = malloc((c->n_nomes + 1) * sizeof *c->simbolo_do_nome);
    c->p->funcoes = calloc(2 * ir->nfuncoes + 2 * ir->ntrampolins + 2, sizeof *c->p->funcoes); /* + o involucro de cada export e o forja_liberar da DLL; 2 por trampolim no alvo linux (fjt_ e fjtm_) */
    if (c->simbolo_do_nome == NULL || c->p->funcoes == NULL)
        FalhaInterna("bir: memoria esgotada preparando o programa");
    for (uint32_t k = 0; k < c->n_nomes; k++)
        c->simbolo_do_nome[k] = BIR_SEM_SIMBOLO;
}

static void MarcarExternos(BirCtx *c)
{
    const IrPrograma *ir = c->e->ir;

    for (uint32_t k = 0; k < ir->nexternos; k++)
        c->simbolo_do_nome[ir->externos[k].nome] = BIR_SIMBOLO_EXTERNO;
}

/* A IrFuncao k entra na BIR? As de teste so no modo teste; com poda, so as que o alcance marcou (o proc podado fica sem simbolo). */
static bool FuncaoNaBir(const BirCtx *c, uint32_t k)
{
    if (c->e->ir->funcoes[k].e_teste && !c->e->teste)
        return false;
    return c->alcance == NULL || BirAlcanceFuncao(c->alcance, k);
}

/* Um simbolo por proc que entra na BIR; a main guarda o seu. */
static void RegistrarSimbolosDasFuncoes(BirCtx *c)
{
    const IrPrograma *ir = c->e->ir;

    for (uint32_t k = 0; k < ir->nfuncoes; k++) {
        const IrFuncao *irf = &ir->funcoes[k];

        if (!FuncaoNaBir(c, k))
            continue;
        c->simbolo_do_nome[irf->nome] = BirSimboloFuncao(c, InternerTexto(c->e->nomes, irf->nome));
        if (irf->e_main) {
            c->tem_main = true;
            c->main_simbolo = c->simbolo_do_nome[irf->nome];
        }
    }
}

static bool PrepararPrograma(BirCtx *c)
{
    AlocarTabelasDoPrograma(c);
    MarcarExternos(c);
    RegistrarSimbolosDasFuncoes(c);
    return c->tem_main || c->e->dll || BirFalhaInconsistente(c, "programa sem proc main");
}

static bool ConstruirFuncoes(BirCtx *c)
{
    const IrPrograma *ir = c->e->ir;

    for (uint32_t k = 0; k < ir->nfuncoes; k++) {
        if (!FuncaoNaBir(c, k) || ConstruirFuncao(c, &ir->funcoes[k]))
            continue;
        if (c->varredura == NULL)
            return false;
        RegistrarOcorrencia(c);
    }
    return true;
}

static void LiberarCtx(BirCtx *c)
{
    free(c->vreg_local);
    free(c->vreg_temp);
    free(c->lacos);
    free(c->simbolo_do_nome);
    free(c->hash);
    BirMemLiberar(c);
}

/* Nomes do runtime de copia e zera sem alvo escolhido (DLL, --suporte e testes): os do Windows. */
static const BirNomesMem NOMES_MEM_WINDOWS = {"memcpy", "memset"};

/* O BirPrograma vazio, com os procs do programa e quantos a poda deixa entrar. */
static BirPrograma ProgramaInicial(const BirEntrada *entrada, const BirAlcance *alcance)
{
    return (BirPrograma){.modo = entrada->modo, .fonte = entrada->fonte, .procs_total = alcance->total,
                         .procs_gerados = entrada->poda ? alcance->gerados : alcance->total, .sysv = entrada->sysv, .guarda = entrada->guarda, .checagem = entrada->checagem, .simbolos_dbg = entrada->simbolos_dbg, .perfil = entrada->perfil,
                         .nomes_mem = entrada->nomes_mem.copia != NULL ? entrada->nomes_mem : NOMES_MEM_WINDOWS};
}

/* varredura NULL = construcao normal (para no 1o erro); senao e o Vec de BirOcorrencia do --suporte. */
static bool ConstruirTudo(const BirEntrada *entrada, BirPrograma *saida, Diag *erro, Vec *varredura)
{
    BirCtx c = {.e = entrada, .p = saida, .d = erro, .varredura = varredura};
    BirAlcance alcance;
    bool ok;

    BirAlcanceCalcular(entrada, &alcance);
    c.alcance = entrada->poda ? &alcance : NULL;
    *saida = ProgramaInicial(entrada, &alcance);
    ok = PrepararPrograma(&c) && ConstruirFuncoes(&c) && BirConstruirTrampolins(&c);
    if (ok)
        ok = entrada->dll ? BirConstruirExports(&c) : ConstruirPrograma(&c);
    LiberarCtx(&c);
    BirAlcanceLiberar(&alcance);
    return ok;
}

bool BirConstruir(const BirEntrada *entrada, BirPrograma *saida, Diag *erro)
{
    return ConstruirTudo(entrada, saida, erro, NULL);
}

void BirVarrerSuporte(const BirEntrada *entrada, Vec *ocorrencias)
{
    BirPrograma descarte;
    Diag erro = {0};

    ConstruirTudo(entrada, &descarte, &erro, ocorrencias);
    BirLiberar(&descarte);
}

void BirLiberar(BirPrograma *programa)
{
    for (uint32_t k = 0; k < programa->nfuncoes; k++) {
        BirFuncao *f = &programa->funcoes[k];

        for (uint32_t b = 0; b < f->nblocos; b++)
            free(f->blocos[b].instrs);
        free(f->blocos);
        free(f->classe_vreg);
        free(f->reg_vreg);
        free(f->slots);
    }
    free(programa->funcoes);
    free(programa->simbolos);
    ArenaLiberar(&programa->arena);
    *programa = (BirPrograma){0};
}
