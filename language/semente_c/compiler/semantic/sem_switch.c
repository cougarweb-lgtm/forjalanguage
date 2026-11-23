/* sem_switch.c - switch sobre enum ou resultado: cobertura total ou default, sem fallthrough, ligacao que estreita o tipo.
 * Versao: 0.5.2
 * Changelog: 0.5.2 - B01b: case Allocator: = E0336 (SemTipoDoCaso).
 *            0.5.1 - IA1-L4: switch sobre 'T ! E' com T dono confere o movimento do alvo (E0311 se for lvalor).
 *            0.5.0 - IA1-L3c2: CasoDeEnum acha o membro pela forma antiga da tabela de renomeados (SemAcharMembro).
 *            0.4.1 - Linguagem v1.8 P6a: NovoLocal com origem (inicializador posicional completo).
 *            0.4.0 - Linguagem v1.8 P3: o tipo do case confere a visibilidade entre modulos (Caso.modulo; E1003).
 *            0.3.0- Linguagem v1.7 P3: switch sobre union (CasoDeUniao, cobertura por variante, E0607/E0608).
 *            0.2.1 - Linguagem v1.7 P2: switch sobre ?T e ?^T (case T / case none), SemTipoDoCaso (tipo escrito tambem em resultado); saiu o provisorio do P1.
 *            0.2.0 - Linguagem v1.7 P1: SemCaso recusa 'case none' e case de tipo escrito com E0101 "chega no P2" (provisorio).
 *            0.1.0 - Fase 5c: enum (case Enum.membro) e resultado (case Tipo / case Erro); em '! E' o lado do valor so se cobre
 *                    com default; posicao dos erros de case = a palavra 'case'; faltou cobrir = a palavra 'switch'.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

typedef struct {
    const Decl *enumeracao; /* NULL quando o alvo e um resultado */
    uint8_t *membros;       /* por membro do enum: ja coberto */
    uint32_t cobertos;
    bool padrao, valor, erro;
    bool opcional; /* v1.7: alvo ?T ou ?^T; 'erro' guarda o 'case none' */
    const Decl *uniao; /* v1.7: alvo union (membros = por variante: ja coberta) */
} Cobertura;

static bool Repetido(Semantica *s, const Caso *c, bool *marca)
{
    if (*marca) return SemErro(s, E0608, c->pos, "case repetido");
    *marca = true;
    return true;
}

/* Posicao do nome do tipo depois de 'case': com 'apelido.' (v1.8), o nome depois do ponto. */
static uint32_t PosDoTipoDoCaso(const Semantica *s, const Caso *c)
{
    uint32_t pos = SemPosDepoisDaPalavra(s, c->pos);
    return c->modulo != 0 ? SemPosDoNomeQualificado(s, pos) : pos;
}

static bool CasoDeEnum(Semantica *s, Caso *c, Cobertura *cob)
{
    const Decl *en = cob->enumeracao;
    if (c->tipo_nome != en->nome || c->membro < 0) {
        return SemErro(s, E0608, c->pos, "case de enum precisa ser 'case %s.membro:'", SemNomeTexto(s, en->nome));
    }
    NomeUsado uso = {c->tipo_nome, PosDoTipoDoCaso(s, c), c->modulo};
    const Simbolo *y = SemAcharSimbolo(s, c->tipo_nome, c->modulo);
    if (!SemVisivel(s, y, &uso)) return false;
    if (y->decl != en) { /* IA1-L8: homonimo de outro modulo (o nome puro bate, o enum nao) */
        return SemErro(s, E0608, c->pos, "case de enum precisa ser 'case %s.membro:'", SemNomeTexto(s, en->nome));
    }
    int32_t idx = SemAcharMembro(s, en, c->membro, c->pos_membro);
    if (idx == SEM_MEMBRO_RECUSADO) return false;
    if (idx < 0) {
        return SemErro(s, E0608, c->pos, "o enum '%s' nao tem o membro '%s'", SemNomeTexto(s, en->nome),
                       SemNomeTexto(s, c->membro));
    }
    if (cob->membros[idx]) return SemErro(s, E0608, c->pos, "membro '%s' repetido", SemNomeTexto(s, c->membro));
    cob->membros[idx] = 1;
    cob->cobertos++;
    c->indice_membro = idx;
    return true;
}

/* Tipo do 'case': escrito inteiro ('case ^Grifo:', 'case [3]int:') ou so o nome ('case Grifo:'). */
bool SemTipoDoCaso(Semantica *s, Caso *c, TipoId *t)
{
    if (c->tipo_escrito == NULL) {
        if (!SemResolverNomeTipo(s, c->tipo_nome, PosDoTipoDoCaso(s, c), c->modulo, t)) return false;
        return !SemEhAllocator(s, *t) || SemAllocatorNomeForaDeParametro(s, PosDoTipoDoCaso(s, c)); /* B01b: case Allocator: = E0336 */
    }
    if (!SemResolverTipoRef(s, c->tipo_escrito)) return false;
    *t = c->tipo_escrito->tipo;
    return true;
}

static bool CasoDeResultado(Semantica *s, Caso *c, TipoId alvo, Cobertura *cob)
{
    TipoId t;
    char texto[96], texto_caso[96];
    if (c->membro >= 0) return SemErro(s, E0608, c->pos, "case de resultado e 'case Tipo:' ou 'case Erro:', sem membro");
    if (!SemTipoDoCaso(s, c, &t)) return false;
    const TipoInfo resultado = *TipoDe(&s->tipos, alvo);
    c->tipo = t;
    if (t == resultado.a) return Repetido(s, c, &cob->valor);
    if (t == resultado.b) return Repetido(s, c, &cob->erro);
    SemDescrever(s, alvo, texto, sizeof texto);
    SemDescrever(s, t, texto_caso, sizeof texto_caso);
    return SemErro(s, E0608, c->pos, "o case '%s' nao e o valor nem o erro do resultado %s", texto_caso, texto);
}

/* switch sobre ?T ou ?^T: 'case T:' (ou 'case ^T:') liga o valor, 'case none:' liga o valor inteiro. */
static bool CasoDeOpcional(Semantica *s, Caso *c, TipoId alvo, Cobertura *cob)
{
    TipoId t;
    char texto[96];
    if (c->membro >= 0) return SemErro(s, E0608, c->pos, "case de ?T e 'case T:' ou 'case none:'");
    if (c->tipo_escrito == NULL && c->tipo_nome == KW_NONE - KW_CONST) {
        c->indice_membro = 0;
        c->tipo = alvo;
        return Repetido(s, c, &cob->erro);
    }
    if (!SemTipoDoCaso(s, c, &t)) return false;
    const TipoInfo k = *TipoDe(&s->tipos, alvo);
    TipoId valor = k.classe == TC_OPCIONAL ? (TipoId)k.a : TipoPonteiro(&s->tipos, k.a);
    if (t != valor) {
        SemDescrever(s, t, texto, sizeof texto);
        char texto_alvo[96];
        SemDescrever(s, alvo, texto_alvo, sizeof texto_alvo);
        return SemErro(s, E0608, c->pos, "o case '%s' nao e o valor de %s nem none", texto, texto_alvo);
    }
    c->indice_membro = 1;
    c->tipo = t;
    return Repetido(s, c, &cob->valor);
}

/* switch sobre union: 'case Variante:' liga o valor da variante; cada variante uma vez. */
static bool CasoDeUniao(Semantica *s, Caso *c, TipoId alvo, Cobertura *cob)
{
    TipoId t;
    char texto[96], nome[96];
    if (c->membro >= 0 || (c->tipo_escrito == NULL && c->tipo_nome == KW_NONE - KW_CONST)) {
        return SemErro(s, E0608, c->pos, "case de union e 'case Variante:'");
    }
    if (!SemTipoDoCaso(s, c, &t)) return false;
    int32_t k = SemVarianteDe(s, alvo, t);
    SemDescrever(s, t, texto, sizeof texto);
    SemDescrever(s, alvo, nome, sizeof nome);
    if (k < 0) return SemErro(s, E0608, c->pos, "'%s' nao e variante da union '%s'", texto, nome);
    if (cob->membros[k]) return SemErro(s, E0608, c->pos, "variante '%s' repetida", texto);
    cob->membros[k] = 1;
    cob->cobertos++;
    c->indice_membro = k;
    c->tipo = t;
    return true;
}

/* Corpo do case: a ligacao 'switch r in ...' vira um local do tipo estreitado, so dentro deste case. */
static bool CorpoDoCaso(Semantica *s, const Stmt *st, Caso *c)
{
    uint32_t altura = EscopoAltura(&s->escopo);
    if (st->nome >= 0) {
        NovoLocal novo = {st->nome, SemPosDepoisDaPalavra(s, st->pos), c->tipo, LOC_LIGACAO, 0};
        if (!SemDeclararLocal(s, &novo, &c->local_id)) return false;
    }
    bool ok = SemBloco(s, c->corpo);
    EscopoRecuar(&s->escopo, altura);
    return ok;
}

static bool SemCaso(Semantica *s, const Stmt *st, Caso *c, TipoId alvo, Cobertura *cob)
{
    bool ok = true;
    c->tipo = alvo;
    c->indice_membro = -1;
    c->local_id = -1;
    if (c->padrao) ok = Repetido(s, c, &cob->padrao);
    else if (cob->enumeracao != NULL) ok = CasoDeEnum(s, c, cob);
    else if (cob->opcional) ok = CasoDeOpcional(s, c, alvo, cob);
    else if (cob->uniao != NULL) ok = CasoDeUniao(s, c, alvo, cob);
    else ok = CasoDeResultado(s, c, alvo, cob);
    return ok && CorpoDoCaso(s, st, c);
}

static bool CoberturaDoEnum(Semantica *s, const Stmt *st, const Cobertura *cob)
{
    const Decl *en = cob->enumeracao;
    if (cob->cobertos == en->nmembros) return true;
    for (uint32_t i = 0; i < en->nmembros; i++) {
        if (cob->membros[i]) continue;
        return SemErro(s, E0607, st->pos, "o switch nao cobre o membro '%s' do enum '%s' (faltam %u): cubra todos ou use default",
                       SemNomeTexto(s, en->membros[i].nome), SemNomeTexto(s, en->nome), en->nmembros - cob->cobertos);
    }
    return true;
}

static bool CoberturaDaUniao(Semantica *s, const Stmt *st, const Cobertura *cob)
{
    const Decl *un = cob->uniao;
    char texto[96];
    if (cob->cobertos == un->ncampos) return true;
    for (uint32_t i = 0; i < un->ncampos; i++) {
        if (cob->membros[i]) continue;
        SemDescrever(s, un->campos[i].tipo.tipo, texto, sizeof texto);
        return SemErro(s, E0607, st->pos, "o switch nao cobre a variante '%s' da union '%s' (faltam %u): cubra todas ou use default",
                       texto, SemNomeTexto(s, un->nome), un->ncampos - cob->cobertos);
    }
    return true;
}

static bool ConferirCobertura(Semantica *s, const Stmt *st, const Cobertura *cob)
{
    if (cob->padrao) return true;
    if (cob->enumeracao != NULL) return CoberturaDoEnum(s, st, cob);
    if (cob->uniao != NULL) return CoberturaDaUniao(s, st, cob);
    if (cob->valor && cob->erro) return true;
    if (cob->opcional) {
        char texto[96];
        SemDescrever(s, st->expr->tipo, texto, sizeof texto);
        return SemErro(s, E0607, st->pos, "o switch sobre %s precisa de 'case T:' e 'case none:' (ou default)", texto);
    }
    return SemErro(s, E0607, st->pos, "o switch sobre resultado precisa cobrir o valor e o erro ('case Tipo:' e 'case Erro:'); "
                   "em '! E' o lado do valor so se cobre com default");
}

static void IniciarCobertura(Semantica *s, TipoId alvo, Cobertura *cob)
{
    cob->enumeracao = s->globais[TipoDe(&s->tipos, alvo)->a].decl;
    cob->membros = ArenaAlocar(s->arena, cob->enumeracao->nmembros + 1u);
}

static void IniciarCoberturaUniao(Semantica *s, TipoId alvo, Cobertura *cob)
{
    cob->uniao = s->globais[TipoDe(&s->tipos, alvo)->a].decl;
    cob->membros = ArenaAlocar(s->arena, cob->uniao->ncampos + 1u);
}

bool SemSwitch(Semantica *s, Stmt *st)
{
    Cobertura cob = {0};
    char texto[96];
    if (!SemExpr(s, st->expr, TIPO_VAZIO)) return false;
    TipoId alvo = st->expr->tipo;
    TipoClasse classe = TipoClasseDe(&s->tipos, alvo);
    cob.opcional = classe == TC_OPCIONAL || classe == TC_ANULAVEL;
    if (classe != TC_ENUM && classe != TC_RESULTADO && classe != TC_UNIAO && !cob.opcional) {
        SemDescrever(s, alvo, texto, sizeof texto);
        return SemErro(s, E0608, st->expr->pos, "switch aceita enum, resultado, ?T, ?^T ou union; recebeu %s", texto);
    }
    if (classe == TC_RESULTADO && !SemConferirMovimento(s, st->expr)) return false; /* L-01: a ligacao recebe o dono; de um lvalor seria copia */
    if (classe == TC_ENUM) IniciarCobertura(s, alvo, &cob);
    if (classe == TC_UNIAO) IniciarCoberturaUniao(s, alvo, &cob);
    for (uint32_t i = 0; i < st->ncasos; i++) {
        if (!SemCaso(s, st, &st->casos[i], alvo, &cob)) return false;
    }
    return ConferirCobertura(s, st, &cob);
}
