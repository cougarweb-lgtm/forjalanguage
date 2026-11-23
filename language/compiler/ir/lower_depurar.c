/* lower_depurar.c - lowering de {expr:?} (FMT_DEPURAR) em f"...": a parte vira pares (formato, valor) de um IR_FORMATA comum.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - Ryu P2: real (f32/f64) em {x:?} vai com FMT_CURTO (MostrarReal) e marca usa_real_curto.
 *            0.1.0 - Linguagem v1.11 L11: LwDepurar (sem IrOp nova).
 * Interface: LwDepurar(c, pares, valor, pos) empilha em 'pares' (Vec de Valor, 2 por parte: formato u32 0 e valor) o texto de depuracao
 *   de 'valor': escalar como {x}; str entre aspas (sem escape); struct "Nome{campo = v, ...}" (recursivo); [N]T "{a, b, c}" (N > 16: os
 *   16 primeiros e ", ... (+K)"); enum "Cor.verde" por um proc OCULTO por enum (0depurar_<TipoId>; nome com digito na frente, que o
 *   lexer nao deixa o usuario escrever; membro sem nome devolve "Cor(7)", texto no frame). Os campos e elementos saem como IR_CAMPO e
 *   IR_ELEMENTO ANTES do IR_FORMATA (quem chama monta a instrucao depois). O tipo foi provado depuravel pela semantica (sem_formato.c).
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py (183_depurar) && python ..\tests\nativo\verificar_nativo.py --pasta v111
 */
#include "ir/lower_interno.h"

enum { MAX_ELEMENTOS_DEPURAR = 16, ALOCADOR_FRAME = 2 };

/* texto: literais vizinhos acumulam aqui e viram UMA parte de texto quando chega um valor. */
typedef struct { LowerCtx *c; Vec *pares; Buf texto; uint32_t pos; } Depura;

static void EmpurrarPar(Depura *d, Valor valor)
{
    *(Valor *)VecEmpurrar(d->pares) = LwValorInt(0, TIPO_U32);
    *(Valor *)VecEmpurrar(d->pares) = valor;
}

static void FecharTexto(Depura *d)
{
    if (d->texto.tam == 0) return;
    EmpurrarPar(d, LwValorTexto(Internar(d->c->sem->nomes, d->texto.dados, (uint32_t)d->texto.tam), TIPO_STR));
    d->texto.tam = 0;
}

static void Texto(Depura *d, const char *literal)
{
    BufAnexar(&d->texto, literal);
}

static void TextoNome(Depura *d, int32_t nome)
{
    BufAnexarN(&d->texto, InternerTexto(d->c->sem->nomes, nome), InternerTam(d->c->sem->nomes, nome));
}

static void Parte(Depura *d, Valor valor)
{
    FecharTexto(d);
    EmpurrarPar(d, valor);
}

static void Mostrar(Depura *d, Valor v);

/* ---------- enum: proc oculto ---------- */

/* O que as funcoes do proc oculto compartilham: o ctx, o enum (tipo e declaracao) e a posicao do 1o uso. */
typedef struct { LowerCtx *c; TipoId tipo; const Decl *decl; uint32_t pos; } ProcEnum;

static int32_t NomeDoProcDeEnum(LowerCtx *c, TipoId enumerado)
{
    char nome[32];
    int n = snprintf(nome, sizeof nome, "0depurar_%u", (unsigned)enumerado);
    return Internar(c->sem->nomes, nome, (uint32_t)n);
}

static const Decl *DeclDoTipo(const LowerCtx *c, TipoId t)
{
    return c->sem->globais[TipoDe(&c->sem->tipos, t)->a].decl;
}

static void AnexarNome(Buf *texto, const Interner *nomes, int32_t nome)
{
    BufAnexarN(texto, InternerTexto(nomes, nome), InternerTam(nomes, nome));
}

static Valor TextoInternado(LowerCtx *c, const Buf *texto)
{
    return LwValorTexto(Internar(c->sem->nomes, texto->dados, (uint32_t)texto->tam), TIPO_STR);
}

/* case Cor.verde: return "Cor.verde" */
static void CasoDoMembro(const ProcEnum *p, uint32_t k, IrCaso *caso)
{
    Buf texto = {0};
    caso->membro = (int32_t)k;
    caso->ligacao = -1;
    caso->tipo = p->tipo;
    AnexarNome(&texto, p->c->sem->nomes, p->decl->nome);
    BufAnexar(&texto, ".");
    AnexarNome(&texto, p->c->sem->nomes, p->decl->membros[k].nome);
    IrBloco *anterior = LwTrocarBloco(p->c, &caso->corpo);
    LwNovaInstr(p->c, IR_RETORNA, p->pos)->a = TextoInternado(p->c, &texto);
    LwTrocarBloco(p->c, anterior);
    BufLiberar(&texto);
}

/* "Cor(" + numero + ")" no frame: o valor sem nome (vindo de C ou de transmute). */
static IrInstr *FormataSemNome(const ProcEnum *p, int32_t numero)
{
    Buf abre = {0};
    LowerCtx *c = p->c;
    IrInstr *formata = LwNovaInstr(c, IR_FORMATA, p->pos);
    AnexarNome(&abre, c->sem->nomes, p->decl->nome);
    BufAnexar(&abre, "(");
    formata->op_aux = 1;
    formata->tipo = TIPO_STR;
    formata->destino = LwNovoTemp(c);
    formata->a = LwValorInt(ALOCADOR_FRAME, TIPO_U32);
    formata->nargs = 6;
    formata->args = LwArgs(c, 6);
    formata->args[0] = formata->args[2] = formata->args[4] = LwValorInt(0, TIPO_U32);
    formata->args[1] = TextoInternado(c, &abre);
    formata->args[3] = LwValorTemp(numero, TIPO_I32);
    formata->args[5] = LwValorTexto(Internar(c->sem->nomes, ")", 1), TIPO_STR);
    BufLiberar(&abre);
    return formata;
}

/* default: return "Cor(7)" */
static void CasoSemNome(const ProcEnum *p, IrCaso *caso)
{
    LowerCtx *c = p->c;
    caso->padrao = true;
    caso->ligacao = -1;
    caso->tipo = p->tipo;
    IrBloco *anterior = LwTrocarBloco(c, &caso->corpo);
    IrInstr *numero = LwNovaInstr(c, IR_TRANSMUTA, p->pos); /* o enum e um int do C; IR_CONVERSAO de enum ainda nao existe no x64 */
    numero->destino = LwNovoTemp(c);
    numero->tipo = TIPO_I32;
    numero->a = LwValorLocal(0, p->tipo);
    IrInstr *formata = FormataSemNome(p, numero->destino);
    LwNovaInstr(c, IR_RETORNA, p->pos)->a = LwValorTemp(formata->destino, TIPO_STR);
    LwTrocarBloco(c, anterior);
}

static void CorpoDoProcDeEnum(LowerCtx *c, TipoId enumerado, uint32_t pos)
{
    ProcEnum p = {c, enumerado, DeclDoTipo(c, enumerado), pos};
    IrInstr *chave = LwNovaInstr(c, IR_CHAVEIA, pos);
    chave->a = LwValorLocal(0, enumerado);
    chave->tipo = enumerado;
    chave->ncasos = p.decl->nmembros + 1;
    chave->casos = LwAlocar(c, chave->ncasos * sizeof(IrCaso));
    for (uint32_t k = 0; k < p.decl->nmembros; k++) CasoDoMembro(&p, k, &chave->casos[k]);
    CasoSemNome(&p, &chave->casos[p.decl->nmembros]);
}

/* Cria o proc oculto no meio do lowering de outra funcao: guarda o estado da funcao em construcao e o devolve no fim. */
static void CriarProcDeEnum(LowerCtx *c, TipoId enumerado, int32_t nome, uint32_t pos)
{
    IrFuncao *anterior_funcao = c->funcao;
    IrBloco *anterior_bloco = c->atual;
    uint32_t anterior_temps = c->ntemps, anterior_locais = c->prox_local;
    IrFuncao *f = &c->prog->funcoes[c->prog->nfuncoes++];
    f->nome = nome;
    f->retorno = TIPO_STR;
    f->nparams = 1;
    f->params = LwAlocar(c, sizeof(IrParam));
    f->params[0] = (IrParam){Internar(c->sem->nomes, "0valor", 6), enumerado};
    c->funcao = f;
    c->atual = &f->corpo;
    c->ntemps = 0;
    c->prox_local = 1;
    CorpoDoProcDeEnum(c, enumerado, pos);
    f->ntemps = c->ntemps;
    f->nlocais = c->prox_local;
    c->funcao = anterior_funcao;
    c->atual = anterior_bloco;
    c->ntemps = anterior_temps;
    c->prox_local = anterior_locais;
}

static int32_t ProcDeEnum(LowerCtx *c, TipoId enumerado, uint32_t pos)
{
    int32_t nome = NomeDoProcDeEnum(c, enumerado);
    for (uint32_t k = 0; k < c->prog->nfuncoes; k++) {
        if (c->prog->funcoes[k].nome == nome) return nome;
    }
    CriarProcDeEnum(c, enumerado, nome, pos);
    return nome;
}

static void MostrarEnum(Depura *d, Valor v)
{
    IrInstr *i = LwNovaInstr(d->c, IR_CHAMADA, d->pos);
    i->nome = ProcDeEnum(d->c, v.tipo, d->pos);
    i->destino = LwNovoTemp(d->c);
    i->tipo = TIPO_STR;
    i->args = LwArgs(d->c, 1);
    i->args[0] = v;
    i->nargs = 1;
    Parte(d, LwValorTemp(i->destino, TIPO_STR));
}

/* ---------- struct, array e str ---------- */

static Valor LerCampo(Depura *d, Valor v, const Campo *campo)
{
    IrInstr *i = LwNovaInstr(d->c, IR_CAMPO, d->pos);
    i->destino = LwNovoTemp(d->c);
    i->tipo = campo->tipo.tipo;
    i->a = v;
    i->nome = campo->nome;
    return LwValorTemp(i->destino, i->tipo);
}

/* Nome{campo = valor, ...} */
static void MostrarStruct(Depura *d, Valor v)
{
    const Decl *estrutura = DeclDoTipo(d->c, v.tipo);
    TextoNome(d, estrutura->nome);
    Texto(d, "{");
    for (uint32_t k = 0; k < estrutura->ncampos; k++) {
        const Campo *campo = &estrutura->campos[k];
        if (k > 0) Texto(d, ", ");
        TextoNome(d, campo->nome);
        Texto(d, " = ");
        Mostrar(d, LerCampo(d, v, campo));
    }
    Texto(d, "}");
}

static Valor LerElemento(Depura *d, Valor v, TipoId elemento, uint32_t indice)
{
    IrInstr *i = LwNovaInstr(d->c, IR_ELEMENTO, d->pos);
    i->destino = LwNovoTemp(d->c);
    i->tipo = elemento;
    i->a = v;
    i->b = LwValorInt(indice, TIPO_INT);
    i->op_aux = 1; /* indice constante dentro de N: provado */
    return LwValorTemp(i->destino, i->tipo);
}

/* {a, b, c}; acima de 16 elementos: os 16 primeiros e ", ... (+K)" */
static void MostrarArray(Depura *d, Valor v)
{
    const TipoInfo *tipo = TipoDe(&d->c->sem->tipos, v.tipo);
    uint32_t mostrados = tipo->b < MAX_ELEMENTOS_DEPURAR ? tipo->b : MAX_ELEMENTOS_DEPURAR;
    Texto(d, "{");
    for (uint32_t k = 0; k < mostrados; k++) {
        if (k > 0) Texto(d, ", ");
        Mostrar(d, LerElemento(d, v, tipo->a, k));
    }
    if (tipo->b > mostrados) {
        Texto(d, ", ... (+");
        BufAnexarU64(&d->texto, tipo->b - mostrados);
        Texto(d, ")");
    }
    Texto(d, "}");
}

static void MostrarStr(Depura *d, Valor v)
{
    Texto(d, "\"");
    Parte(d, v);
    Texto(d, "\"");
}

/* Ryu P2: real em {x:?} sai como {x}: Display curto (FMT_CURTO no formato do par). */
static void MostrarReal(Depura *d, Valor v)
{
    FecharTexto(d);
    *(Valor *)VecEmpurrar(d->pares) = LwValorInt(FMT_CURTO, TIPO_U32);
    *(Valor *)VecEmpurrar(d->pares) = v;
    d->c->prog->usa_real_curto = true;
}

static void Mostrar(Depura *d, Valor v)
{
    switch (TipoClasseDe(&d->c->sem->tipos, v.tipo)) {
    case TC_STRUCT: MostrarStruct(d, v); break;
    case TC_ARRAY: MostrarArray(d, v); break;
    case TC_ENUM: MostrarEnum(d, v); break;
    case TC_TEXTO: MostrarStr(d, v); break;
    case TC_REAL: case TC_F64: MostrarReal(d, v); break;
    default: Parte(d, v); break;
    }
}

void LwDepurar(LowerCtx *c, Vec *pares, Valor v, uint32_t pos)
{
    Depura d = {.c = c, .pares = pares, .pos = pos};
    Mostrar(&d, v);
    FecharTexto(&d);
    BufLiberar(&d.texto);
}
