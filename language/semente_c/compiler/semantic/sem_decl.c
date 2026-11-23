/* sem_decl.c - declaracoes: passe A por categoria (unit, enum, const, struct, assinaturas de proc), passe B nos corpos, main.
 * Versao: 0.25.0
 * Changelog: 0.25.0 - Linguagem v1.13 L1a: o passe A e o passe B gravam o modulo e o arquivo da declaracao (SemEntrarNaDecl).
 *            0.24.0 - M6e P93g: CorposDoPrograma chama SemEmprestimoCorpo (E0913) no corpo sem erro que tem ponteiro e muta lista/map; IniciarCorpo zera corpo_muta_colecao.
 *            0.23.0 - M6c P85: DeclProc confere o tipo da clausula de posse do foreign (SemChecarPosseC).
 *            0.22.0 - M6c P83: SemPasseB confere o E0342 das assinaturas (SemCLongDecls) e CorposDoPrograma o do corpo (SemCLongCorpo) antes de tipar, so se o programa cita c_long.
 *            0.21.0 - M6 P65a: IniciarCorpo zera/preenche a tabela de nomes (parametros) e CorpoDaDecl a copia para Decl.nomes_locais (arena).
 *            0.20.0 - M5 P48 (S18): E0339 em parametro, retorno e variante de union por valor (SemExigeCopiavel); RegistrarAlocador liga a marca de std:sync (SemRegistrarNaoCopiavel).
 *            0.19.0 - S4 P1: TituloDoTest confere o titulo unico por modulo (titulo_visto guarda modulo + 1).
 *            0.18.0 - S3 P4: DeclProc chama SemLibmNotarDecl (foreign de libm que migra para std:math; so com o forja migrar ligado).
 * Changelog: 0.17.0 - B01a: ParamDoProc liga alocador_permitido no tipo do parametro e confere o parametro Allocator (SemAllocatorConferirParam, E0336).
 *            0.16.0 - IA1-b15: CorposDoPrograma segue com o erro dos comandos ja na lista (corpo_com_erro, SemRecuperar); corpo com erro nao confere o return nem entra na higiene/fuga; IniciarCorpo zera nomes_relatados.
 *            0.15.2 - IA1-b08: o E0311 de campo dono com padrao e de parametro por valor leva a forma e a sugestao da ocorrencia (SemFormaDoDono).
 *            0.15.1 - IA1-b06: DeclProc leva a sugestao de alias ao E0202 de proc foreign com nome reservado (SemSugerirAliasDeForeign).
 *            0.15.0- Linguagem v1.12 M3 P21v-a: CampoDaStruct adia o E0909 do campo proc (SemConferirProcsDaStruct logo depois do DeclStruct, atras de usa_proc_valor); campo [N]S com proc = E0911.
 *            0.14.0 - Linguagem v1.11 L15a: a cadeia do ciclo de struct escreve a lista fixa como [dynamic, N]T (NomeNoCiclo).
 *            0.13.0 - Linguagem v1.11 L10a: o proc fechado sem erro guarda o achado de higiene (SemHigieneGuardar) e SemPasseB o relata depois da fuga (SemHigieneRelatar).
 *            0.12.0 -Linguagem v1.11 L9: o passe A termina com o static_assert de topo (D_ASSERT, SemDeclAssert), depois das assinaturas de proc.
 *            0.11.0 - Linguagem v1.11 L8: declaracao com erro fica envenenada (SemEnvenenar) e o erro de quem toca o veneno e calado; PasseDaCategoria/CorposDoPrograma zeram o toque por declaracao; corpo de proc com assinatura invalida nao e analisado; DeclStruct/DeclUniao poem TIPO_VENENO nos campos nao resolvidos; SemPasseA marca pula_passe_b; regra "higiene (L10a) nunca roda em declaracao com erro".
 *            0.10.0- Linguagem v1.11 L7: PasseDaCategoria e CorposDoPrograma guardam o 1o erro de cada declaracao na lista (SemGuardarErro, Decl.com_erro) e seguem; SemPasseA/SemPasseB devolvem false se houve erro; o ciclo de structs e um erro so (ErroDeCiclo marca a cadeia) e encerra o passe A.
 *            0.9.2 - Linguagem v1.11 L3: SemPasseB termina com SemFugaPrograma (resumo por callee precisa de todos os corpos tipados); CorpoDaDecl nao chama mais SemFugaProc;
 *                    IniciarCorpo/CorpoDaDecl gravam Decl.fuga_relevante (corpo com ponteiro).
 *            0.9.1 -Linguagem v1.11 L2: CorpoDaDecl chama SemFugaProc ao fim de cada proc/test sem erro (E0706 por fluxo, sem_fuga.c).
 *            0.9.0 - Linguagem v1.9 P3: DeclConst recusa map, bit_set, soa e matrix (E0301, SemTipoTemV19); TipoDoCampoValido recusa faixa em campo desses tipos (E0308).
 *            0.8.2 - Linguagem v1.8b P5a: ParamDoProc liga tipo_proc_permitido no parametro de proc foreign (callback).
 *            0.8.1 - Linguagem v1.8b P4: os 2 passos de struct aninhada saem do laco do passe A quando tem_struct_aninhada e falso (o laco custava +0,7 ms no check do corpus de 200 mil linhas).
 *            0.8.0 - Linguagem v1.8b P4: struct dentro de struct por valor (TipoDoCampoValido sem o E0301; ConferirCicloDaStruct E0301, ConferirCamposStruct E0311/E0503, SemStructExigeLiteral; faixa em campo struct = E0308).
 *            0.7.4 - Linguagem v1.8b P3a: ConferirValoresDoEnum (E0420/E0202) e RegistrarStruct do opaco; saem os provisorios do P2.
 *            0.7.3 - Linguagem v1.8b P2: provisorios E0101 "enum com valor / opaque chega no P3a" (DeclEnum, RegistrarStruct).
 *            0.7.2 - Linguagem v1.8 P7: IniciarCorpo zera rastreados e marca_laco.
 *            0.7.1 - Linguagem v1.8 P6a: RegistrarStruct grava tipo_arena/tipo_pool (structs de std:mem); LocalInfo com origem.
 *            0.7.0 - Linguagem v1.8 P3: modulo_atual gravado antes de cada declaracao (passe A e B); DimDaUnidade confere a visibilidade.
 *            0.6.3- Linguagem v1.7 (desempenho): o passe A pula a categoria sem declaracao (bitmask de SemChecarModificadores); sao 10 passos sobre a lista de Decl.
 *            0.6.2 - Linguagem v1.7 P4: DeclProc resolve o retorno multiplo (RetornoMultiplo: 2 a 8 elementos nomeados, sem '! E', E0314).
 *            0.6.1 - Linguagem v1.7 P3: union (RegistrarUniao, DeclUniao, ConferirDonoDaUniao no passe A; saiu o provisorio); DeclConst recusa ?T e union; TipoDoCampoValido (E0301 de ?T/union com struct por valor).
 *            0.6.0 - Linguagem v1.7 P1: PASSOS_A ganha {D_UNIAO, UniaoProvisoria} (E0101 "union chega no P3", provisorio).
 *            0.5.0 - Linguagem v1.6 P2: Decl.contem_dono; campo dono com padrao e parametro por valor de tipo dono = E0311; faixa em campo
 *                    e const recusam [dynamic]T.
 *            0.4.0 - Linguagem v1.5 P2: campo [N]struct = E0301, faixa em campo array/fatia = E0308 (TipoDoCampoValido); ConstEhComposta
 *                    (const de array vai com as de struct); const de fatia recusada.
 *            0.3.0 - Linguagem v1.4 P2: DeclProc de foreign (sem caixa no nome, SemChecarExterno); SemPasseB pula proc sem corpo.
 *            0.2.1 - Linguagem v1.3: const recusa ^T, ?^T e rawptr (E0301).
 *            0.2.0 - Linguagem v1.3: TipoRefSoErro e ConstEhDeStruct conferem a forma do TipoRef.
 *            0.1.1 - Linguagem v1.1: modificadores pub/export conferidos antes do passe A; proc exportado conferido em DeclProc.
 *            0.1.0 - Fase 5c: ordem do subconjunto.txt + adendo 1.0.1 (declaracao fora de ordem funciona); enum entra depois de
 *                    unit e antes de const; nome das structs entra antes das consts, e const de tipo struct vem depois dos campos
 *                    das structs; campo de tipo struct recusado (E0301); main aceita 'proc main() -> ! E'.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_fuga.h"

#include <stdio.h>

typedef bool (*ChecaDecl)(Semantica *s, Decl *d);

/* ---------- unit ---------- */

static bool DimDaUnidade(Semantica *s, int32_t nome, uint32_t pos, DimId *dim)
{
    const Simbolo *y = SemAcharSimbolo(s, nome, 0);
    NomeUsado uso = {nome, pos, 0}; /* D31: a unidade derivada usa unidades do mesmo modulo */
    if (!SemVisivel(s, y, &uso)) return false;
    if (y->categoria == SIM_UNIDADE && y->envenenado) return SemErroCalado(s, pos); /* L8: unidade com erro nao da dimensao */
    if (y->categoria != SIM_UNIDADE) return SemErro(s, E0201, pos, "unidade '%s' nao declarada", SemNomeTexto(s, nome));
    *dim = TipoDe(&s->tipos, y->tipo)->a;
    return true;
}

static bool DeclUnidade(Semantica *s, Decl *d)
{
    DimId dim, a, b;
    if (!SemVerificarCaixa(s, d->nome, d->pos, CAIXA_PASCAL)) return false;
    if (d->tipo_no == D_UNIDADE_BASE) {
        dim = DimNovaBase(&s->dims, d->chave);
    } else {
        if (!DimDaUnidade(s, d->nome_a, d->pos_a, &a) || !DimDaUnidade(s, d->nome_b, d->pos_b, &b)) return false;
        dim = DimCombinar(&s->dims, a, b, d->op == OP_MUL ? 1 : -1);
        DimNomear(&s->dims, dim, d->chave);
    }
    Simbolo y = {SIM_UNIDADE, 0, d, TipoReal(&s->tipos, dim), 0};
    return SemDeclararGlobal(s, d, &y);
}

/* ---------- enum ---------- */

static bool MembroDoEnum(Semantica *s, const Decl *d, uint32_t i)
{
    const Membro *m = &d->membros[i];
    if (!SemVerificarCaixa(s, m->nome, m->pos, CAIXA_SNAKE)) return false;
    for (uint32_t j = 0; j < i; j++) {
        if (d->membros[j].nome == m->nome) {
            return SemErro(s, E0202, m->pos, "o membro '%s' ja existe no enum '%s'", SemNomeTexto(s, m->nome),
                           SemNomeTexto(s, d->nome));
        }
    }
    return true;
}

/* v1.8b: enum com valor ('= v'): cada valor cabe em i32 (o enum vira int no C) e nenhum se repete (switch). So roda com d->op == 1. */
static bool ConferirValoresDoEnum(Semantica *s, const Decl *d)
{
    for (uint32_t k = 0; k < d->nmembros; k++) {
        const Membro *m = &d->membros[k];
        if (m->valor < INT32_MIN || m->valor > INT32_MAX) {
            return SemErro(s, E0420, m->pos_valor, "o valor %lld do membro '%s' nao cabe em i32 (-2147483648..=2147483647): o enum vira int no C",
                           (long long)m->valor, SemNomeTexto(s, m->nome));
        }
        for (uint32_t j = 0; j < k; j++) {
            if (d->membros[j].valor != m->valor) continue;
            return SemErro(s, E0202, m->pos, "o membro '%s' repete o valor %lld de '%s': dois nomes com o mesmo valor colidem no switch; use o membro original",
                           SemNomeTexto(s, m->nome), (long long)m->valor, SemNomeTexto(s, d->membros[j].nome));
        }
    }
    return true;
}

static bool DeclEnum(Semantica *s, Decl *d)
{
    if (!SemVerificarCaixa(s, d->nome, d->pos, CAIXA_PASCAL)) return false;
    for (uint32_t i = 0; i < d->nmembros; i++) {
        if (!MembroDoEnum(s, d, i)) return false;
    }
    if (d->op == 1 && !ConferirValoresDoEnum(s, d)) return false;
    d->indice = (int32_t)s->qtd_enum++;
    Simbolo y = {SIM_ENUM, d->indice, d, TipoEnum(&s->tipos, d->chave), 0};
    return SemDeclararGlobal(s, d, &y);
}

/* ---------- const ---------- */

static bool DeclConst(Semantica *s, Decl *d)
{
    if (!SemVerificarCaixa(s, d->nome, d->pos, CAIXA_MAIUSCULA) || !SemResolverTipoRef(s, &d->tipo)) return false;
    if (__builtin_expect(TiposTemV19(&s->tipos), 0) && SemTipoTemV19(s, d->tipo.tipo)) {
        return SemErro(s, E0301, d->tipo.pos, "constante nao guarda map, bit_set, soa nem matrix (nem dentro de struct ou array): use let dentro do proc");
    }
    TipoClasse classe = TipoClasseDe(&s->tipos, d->tipo.tipo);
    if (classe == TC_PONTEIRO || classe == TC_ANULAVEL || classe == TC_RAWPTR || classe == TC_FATIA ||
        classe == TC_DINAMICO || classe == TC_OPCIONAL || classe == TC_UNIAO) {
        return SemErro(s, E0301, d->tipo.pos, "constante so guarda numero, bool, enum, str, cstr, struct ou array (nao ?T nem union)");
    }
    if (!SemExprCom(s, d->init, d->tipo.tipo)) return false;
    d->indice = (int32_t)s->qtd_const++;
    /* L8: tipo ou init que tocou o veneno (ex.: init = membro de enum com erro) deixa a const sem valor conferivel: os usos valem TIPO_VENENO */
    Simbolo y = {SIM_CONST, d->indice, d, s->tocou_veneno ? TIPO_VENENO : d->tipo.tipo, s->tocou_veneno};
    return SemDeclararGlobal(s, d, &y);
}

/* const de tipo struct ('const MALHA_CUBO: Malha = Malha{...}') ou array ('[N]T') precisa dos campos das structs resolvidos: vem depois delas. */
static bool ConstEhComposta(const Semantica *s, const Decl *d)
{
    if (d->tipo.forma == TR_ARRAY) return true;
    return d->tipo.forma == TR_NOME && SemAcharSimbolo(s, d->tipo.nome, d->tipo.modulo)->categoria == SIM_STRUCT;
}

static bool DeclConstEscalar(Semantica *s, Decl *d)
{
    return ConstEhComposta(s, d) || DeclConst(s, d);
}

static bool DeclConstStruct(Semantica *s, Decl *d)
{
    return !ConstEhComposta(s, d) || DeclConst(s, d);
}

/* ---------- struct ---------- */

/* v1.2: cada limite de 'in a..=b' precisa caber na largura do campo (u8 in 0..=300 = E0420 no limite). Os limites ja vem em int64. */
static bool LimitesCabemNoCampo(Semantica *s, const Campo *f)
{
    TipoId tipo = f->tipo.tipo;
    const int64_t limites[2] = {f->faixa_min, f->faixa_max};
    const uint32_t posicoes[2] = {f->pos_min, f->pos_max};

    if (!f->tem_faixa || !TipoEhInteiro(tipo)) return true;
    for (int k = 0; k < 2; k++) {
        bool cabe = TipoBitsCabem(tipo, limites[k]) && !(TipoEhSemSinal(tipo) && limites[k] < 0);
        if (!cabe) {
            return SemErro(s, E0420, posicoes[k], "o limite %lld da faixa nao cabe em %s (%lld..=%llu)", (long long)limites[k],
                           TipoNomeFixo(tipo), (long long)TipoInteiroMin(tipo), (unsigned long long)TipoInteiroMax(tipo));
        }
    }
    return true;
}

/* Tipo do campo sem os '[N]' de fora: struct em struct por valor (nem dentro de array) ainda nao existe; faixa so em escalar. */
static bool TipoDoCampoValido(Semantica *s, const Campo *f)
{
    TipoId elemento = f->tipo.tipo;
    char texto[96];
    while (TipoClasseDe(&s->tipos, elemento) == TC_ARRAY) elemento = TipoDe(&s->tipos, elemento)->a;
    TipoClasse classe_elemento = TipoClasseDe(&s->tipos, elemento);
    if ((classe_elemento == TC_OPCIONAL || classe_elemento == TC_UNIAO) && SemTipoTemStructPorValor(s, elemento)) {
        SemDescrever(s, f->tipo.tipo, texto, sizeof texto);
        return SemErro(s, E0301, f->tipo.pos, "campo de tipo %s guarda struct por valor (ainda nao suportado): guarde ^Struct na variante ou o valor fora da struct", texto);
    }
    if (classe_elemento == TC_STRUCT) s->tem_struct_aninhada = true;
    TipoClasse classe = TipoClasseDe(&s->tipos, f->tipo.tipo);
    if (f->tem_faixa && classe == TC_STRUCT) {
        return SemErro(s, E0308, f->pos_min, "faixa em campo struct nao existe: ponha a faixa nos campos da struct de dentro");
    }
    if (f->tem_faixa && (classe == TC_ARRAY || classe == TC_FATIA || classe == TC_DINAMICO)) {
        return SemErro(s, E0308, f->pos_min,
                       "faixa em campo array nao existe: ponha a faixa num struct do elemento ou confira com assert");
    }
    if (f->tem_faixa && classe >= TC_MAPA) {
        return SemErro(s, E0308, f->pos_min, "faixa em campo map, bit_set, soa ou matrix nao existe: confira com assert");
    }
    return true;
}

/* Campo dono marca a struct (D3); o padrao dele seria uma alocacao escondida em todo literal, entao e recusado antes de avaliar. */
static bool MarcarCampoDono(Semantica *s, Decl *d, const Campo *f)
{
    if (!SemTipoEhDono(s, f->tipo.tipo)) return true;
    d->contem_dono = true;
    if (f->padrao == NULL) return true;
    SemErro(s, E0311, f->padrao->pos, "campo dono com padrao alocaria escondido a cada literal: tire o padrao (a lista comeca vazia, sem alocador) e passe make(...) no literal");
    SemFormaDoDono(s, DONO_CAMPO_PADRAO, f->padrao->pos, TIPO_VAZIO);
    return false;
}

static bool CampoDaStruct(Semantica *s, Decl *d, Campo *f, uint32_t i)
{
    if (!SemVerificarCaixa(s, f->nome, f->pos, CAIXA_SNAKE)) return false;
    for (uint32_t j = 0; j < i; j++) {
        if (d->campos[j].nome == f->nome) {
            return SemErro(s, E0202, f->pos, "o campo '%s' ja existe na struct '%s'", SemNomeTexto(s, f->nome),
                           SemNomeTexto(s, d->nome));
        }
    }
    s->adia_assinatura_proc = true; /* v1.12: o E0909 do campo proc vem em SemConferirProcsDaStruct, quando todas as structs ja tem campos */
    bool resolvido = SemResolverTipoRef(s, &f->tipo);
    s->adia_assinatura_proc = false;
    if (!resolvido) return false;
    if (!TipoDoCampoValido(s, f) || !LimitesCabemNoCampo(s, f)) return false;
    if (!MarcarCampoDono(s, d, f)) return false;
    if (f->padrao == NULL) return true;
    return SemExprCom(s, f->padrao, f->tipo.tipo) && SemFaixaLiteral(s, f, f->padrao);
}

/* Primeiro so o nome da struct entra na tabela (para um const poder ter tipo struct); os campos vem em DeclStruct. */
/* v1.8: as structs Arena e Pool de std:mem (o 'em' de new/make/format/delete as reconhece pelo tipo). */
static void RegistrarAlocador(Semantica *s, const Decl *d, TipoId tipo)
{
    SemRegistrarNaoCopiavel(s, d); /* P48 S18: Mutex, Cond e Atomic* da std:sync/std:atomic */
    if (s->modulos->itens[d->modulo].origem != MOD_STD) return;
    if (d->nome == s->nome_arena) s->tipo_arena = tipo;
    if (d->nome == s->nome_pool) s->tipo_pool = tipo;
}

static bool RegistrarStruct(Semantica *s, Decl *d)
{
    if (!SemVerificarCaixa(s, d->nome, d->pos, CAIXA_PASCAL)) return false;
    if (d->externo) s->tem_opaco = true;
    d->indice = (int32_t)s->qtd_struct++;
    Simbolo y = {SIM_STRUCT, d->indice, d, TipoStruct(&s->tipos, d->chave), 0};
    if (d->modulo != 0) RegistrarAlocador(s, d, y.tipo);
    return SemDeclararGlobal(s, d, &y);
}

static bool DeclStruct(Semantica *s, Decl *d)
{
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (CampoDaStruct(s, d, &d->campos[i], i)) continue;
        SemVenenarCampos(d, i); /* L8: o que a analise nao resolveu vale TIPO_VENENO */
        return false;
    }
    return true;
}

/* ---------- struct dentro de struct (v1.8b) ---------- */

/* Decl da struct de um campo (atravessando os '[N]'); NULL se o campo nao guarda struct por valor. */
static Decl *StructDoCampo(const Semantica *s, TipoId t)
{
    while (TipoClasseDe(&s->tipos, t) == TC_ARRAY) t = (TipoId)TipoDe(&s->tipos, t)->a;
    if (TipoClasseDe(&s->tipos, t) != TC_STRUCT) return NULL;
    return s->globais[TipoDe(&s->tipos, t)->a].decl;
}

typedef struct { int32_t nomes[64]; uint32_t n; } CaminhoCiclo;

/* Algum campo struct de 'atual' leva de volta a 'alvo'? O caminho guarda os nomes percorridos (estourou 64 = false). */
static bool Alcanca(const Semantica *s, const Decl *atual, const Decl *alvo, CaminhoCiclo *caminho)
{
    if (caminho->n >= 64) return false;
    caminho->nomes[caminho->n++] = atual->chave; /* IA1-L8: o caminho guarda chaves (globais[chave]) */
    if (atual == alvo) return true;
    for (uint32_t i = 0; i < atual->ncampos; i++) {
        const Decl *proxima = StructDoCampo(s, atual->campos[i].tipo.tipo);
        if (proxima != NULL && Alcanca(s, proxima, alvo, caminho)) return true;
    }
    caminho->n--;
    return false;
}

/* Nome da struct na cadeia do ciclo; a struct oculta de [dynamic, N]T (v1.11 L15a) aparece como o tipo escrito. */
static void NomeNoCiclo(const Semantica *s, int32_t nome, char *saida, size_t cap)
{
    TipoId tipo = s->globais[nome].tipo;
    if (TipoEhListaFixa(&s->tipos, tipo)) SemDescrever(s, tipo, saida, cap);
    else snprintf(saida, cap, "%s", SemNomeTexto(s, nome));
}

/* O ciclo e um erro so: as structs da cadeia ficam com_erro e a Alfa -> Beta -> Alfa nao e relatada de novo a partir da Beta (L7). */
static bool ErroDeCiclo(Semantica *s, const Decl *d, const Campo *f, const CaminhoCiclo *caminho)
{
    char cadeia[256], nome[96];
    size_t uso = 0;
    cadeia[0] = '\0';
    for (uint32_t k = 0; k < caminho->n && uso + 1 < sizeof cadeia; k++) {
        NomeNoCiclo(s, caminho->nomes[k], nome, sizeof nome);
        uso += (size_t)snprintf(cadeia + uso, sizeof cadeia - uso, k == 0 ? "%s" : " -> %s", nome);
    }
    for (uint32_t k = 0; k < caminho->n; k++) s->globais[caminho->nomes[k]].decl->com_erro = true;
    return SemErro(s, E0301, f->tipo.pos, "a struct '%s' contem a si mesma por valor (%s): troque um dos campos por ponteiro (^T)",
                   SemNomeTexto(s, d->nome), cadeia);
}

static bool ConferirCicloDaStruct(Semantica *s, Decl *d)
{
    if (!s->tem_struct_aninhada) return true;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        const Decl *dentro = StructDoCampo(s, d->campos[i].tipo.tipo);
        CaminhoCiclo caminho = {{0}, 0};
        if (dentro != NULL && Alcanca(s, dentro, d, &caminho)) {
            /* o caminho comeca em 'dentro'; a cadeia do aviso comeca e termina em d */
            CaminhoCiclo completo = {{d->chave}, 1};
            for (uint32_t k = 0; k < caminho.n && completo.n < 64; k++) completo.nomes[completo.n++] = caminho.nomes[k];
            return ErroDeCiclo(s, d, &d->campos[i], &completo);
        }
    }
    return true;
}

/* A struct (ou alguma struct de dentro) tem campo dono. */
static bool StructContemDono(const Semantica *s, const Decl *d)
{
    if (d->contem_dono) return true;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        const Decl *dentro = StructDoCampo(s, d->campos[i].tipo.tipo);
        if (dentro != NULL && StructContemDono(s, dentro)) return true;
    }
    return false;
}

/* Algum campo sem padrao exige valor no literal: ^T, union ou struct de dentro que exige (sem ciclo: ja recusado). */
bool SemStructExigeLiteral(Semantica *s, const Decl *d)
{
    for (uint32_t i = 0; i < d->ncampos; i++) {
        const Campo *f = &d->campos[i];
        if (f->padrao != NULL) continue;
        if (SemTipoSemZero(s, f->tipo.tipo)) return true;
        if (TipoClasseDe(&s->tipos, f->tipo.tipo) == TC_STRUCT && SemStructExigeLiteral(s, SemDeclDaStruct(s, f->tipo.tipo))) return true;
    }
    return false;
}

static bool ConferirCampoStruct(Semantica *s, Decl *d, const Campo *f)
{
    const Decl *dentro = StructDoCampo(s, f->tipo.tipo);
    char texto[96];
    if (dentro == NULL) return true;
    if (StructContemDono(s, dentro)) {
        d->contem_dono = true;
        if (f->padrao != NULL) {
            SemErro(s, E0311, f->padrao->pos, "campo dono com padrao alocaria escondido a cada literal: tire o padrao (a lista comeca vazia, sem alocador) e passe make(...) no literal");
            SemFormaDoDono(s, DONO_CAMPO_PADRAO, f->padrao->pos, TIPO_VAZIO);
            return false;
        }
    }
    if (TipoClasseDe(&s->tipos, f->tipo.tipo) != TC_ARRAY || !SemStructExigeLiteral(s, dentro)) return true;
    SemDescrever(s, f->tipo.tipo, texto, sizeof texto);
    if (SemProcSemZero(s, f->tipo.tipo)) {
        return SemErro(s, E0911, f->tipo.pos, "o campo '%s' guarda %s em array, mas %s tem campo proc, que nao tem valor zero (nao ha proc nulo): guarde ^%s",
                       SemNomeTexto(s, f->nome), texto, SemNomeTexto(s, dentro->nome), SemNomeTexto(s, dentro->nome));
    }
    return SemErro(s, E0503, f->tipo.pos,
                   "o campo '%s' guarda %s em array, mas %s nao tem valor zero (campo ^T ou union sem padrao); use ?^T ou de padrao na struct de dentro",
                   SemNomeTexto(s, f->nome), texto, SemNomeTexto(s, dentro->nome));
}

static bool ConferirCamposStruct(Semantica *s, Decl *d)
{
    if (!s->tem_struct_aninhada) return true;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (!ConferirCampoStruct(s, d, &d->campos[i])) return false;
    }
    return true;
}

/* ---------- union ---------- */

/* O nome da union entra na tabela antes dos campos das structs (campo e variante podem citar a union). */
static bool RegistrarUniao(Semantica *s, Decl *d)
{
    if (!SemVerificarCaixa(s, d->nome, d->pos, CAIXA_PASCAL)) return false;
    d->indice = (int32_t)s->qtd_uniao++;
    Simbolo y = {SIM_UNIAO, d->indice, d, TipoUniao(&s->tipos, d->chave), 0};
    return SemDeclararGlobal(s, d, &y);
}

/* Variante proibida (E0313): ?T, union, 'T ! E' e lista (dono direto); a struct com lista fica para ConferirDonoDaUniao. */
static bool VarianteProibida(Semantica *s, const Campo *v)
{
    TipoId t = v->tipo.tipo;
    TipoClasse classe = TipoClasseDe(&s->tipos, t);
    char texto[96];
    if (!SemExigeCopiavel(s, t, v->tipo.pos, "variante de union por valor: a union guarda o valor")) return false;
    if (classe == TC_OPCIONAL) {
        SemDescrever(s, (TipoId)TipoDe(&s->tipos, t)->a, texto, sizeof texto);
        return SemErro(s, E0313, v->tipo.pos, "variante ?T nao existe: a union ja escolhe; use ?%s fora dela", texto);
    }
    if (classe == TC_UNIAO) return SemErro(s, E0313, v->tipo.pos, "union dentro de union: liste as variantes direto");
    if (classe == TC_RESULTADO) return SemErro(s, E0313, v->tipo.pos, "variante 'T ! E' nao existe");
    if (SemTipoEhDono(s, t)) {
        return SemErro(s, E0313, v->tipo.pos, "variante dona nao existe: a union nao libera a lista; guarde ^[dynamic]T");
    }
    return true;
}

static bool VarianteDaUniao(Semantica *s, const Decl *d, uint32_t i)
{
    Campo *v = &d->campos[i];
    char texto[96];
    if (!SemResolverTipoRef(s, &v->tipo) || !VarianteProibida(s, v)) return false;
    for (uint32_t j = 0; j < i; j++) {
        if (d->campos[j].tipo.tipo != v->tipo.tipo) continue;
        SemDescrever(s, v->tipo.tipo, texto, sizeof texto);
        return SemErro(s, E0313, v->tipo.pos, "variante %s repetida", texto);
    }
    return true;
}

static bool DeclUniao(Semantica *s, Decl *d)
{
    if (d->ncampos < 2 || d->ncampos > 255) {
        SemVenenarCampos(d, 0); /* L8 */
        return SemErro(s, E0313, d->pos, "union precisa de 2 a 255 variantes (tem %u)", d->ncampos);
    }
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (VarianteDaUniao(s, d, i)) continue;
        SemVenenarCampos(d, i); /* L8 */
        return false;
    }
    return true;
}

/* Depois do DeclStruct (contem_dono ja existe): variante struct com lista nao existe. */
static bool ConferirDonoDaUniao(Semantica *s, Decl *d)
{
    for (uint32_t i = 0; i < d->ncampos; i++) {
        const Campo *v = &d->campos[i];
        char texto[96];
        if (!SemTipoEhDono(s, v->tipo.tipo)) continue;
        SemDescrever(s, v->tipo.tipo, texto, sizeof texto);
        return SemErro(s, E0313, v->tipo.pos, "variante dona nao existe: a struct '%s' tem lista; guarde ^%s", texto, texto);
    }
    return true;
}

/* ---------- assinatura de proc ---------- */

static bool ParamDoProc(Semantica *s, const Decl *d, Param *p, uint32_t i)
{
    if (!SemVerificarCaixa(s, p->nome, p->pos, CAIXA_SNAKE)) return false;
    for (uint32_t j = 0; j < i; j++) {
        if (d->params[j].nome == p->nome) {
            return SemErro(s, E0202, p->pos, "o parametro '%s' ja existe no proc '%s'", SemNomeTexto(s, p->nome),
                           SemNomeTexto(s, d->nome));
        }
    }
    if (d->externo) s->tipo_proc_permitido = &p->tipo;
    s->alocador_permitido = &p->tipo; /* B01a: o parametro e o unico lugar onde Allocator vale */
    bool resolvido = SemResolverTipoRef(s, &p->tipo);
    s->tipo_proc_permitido = NULL;
    s->alocador_permitido = NULL;
    if (!resolvido || !SemAllocatorConferirParam(s, d, p, i)) return false;
    if (SemTipoEhDono(s, p->tipo.tipo)) {
        SemErro(s, E0311, p->tipo.pos, "parametro nao recebe lista por valor: use ^[dynamic]T para alterar ou []T (l[..]) para ler e alterar");
        SemFormaDoDono(s, DONO_PARAMETRO, p->tipo.pos, TIPO_VAZIO);
        return false;
    }
    if (!SemExigeCopiavel(s, p->tipo.tipo, p->tipo.pos, "parametro por valor: o proc recebe o endereco (^T)")) return false;
    return p->padrao == NULL || SemExprCom(s, p->padrao, p->tipo.tipo);
}

/* Elemento do retorno multiplo: caixa snake, nome unico entre os elementos, tipo que nao seja 'T ! E' (E0314). */
static bool ElementoDoRetorno(Semantica *s, const Decl *d, Param *el, uint32_t i)
{
    if (!SemVerificarCaixa(s, el->nome, el->pos, CAIXA_SNAKE)) return false;
    for (uint32_t j = 0; j < i; j++) {
        if (d->retornos[j].nome == el->nome) {
            return SemErro(s, E0202, el->pos, "o elemento '%s' ja existe no retorno do proc '%s'", SemNomeTexto(s, el->nome),
                           SemNomeTexto(s, d->nome));
        }
    }
    if (!SemResolverTipoRef(s, &el->tipo)) return false;
    if (TipoClasseDe(&s->tipos, el->tipo.tipo) == TC_RESULTADO) {
        return SemErro(s, E0314, el->tipo.pos, "elemento 'T ! E' nao existe: use ?E num elemento");
    }
    return true;
}

/* '-> (q: T, r: U)' (D11, D13): 2 a 8 elementos, sem '! E'; retorno.tipo vira a tupla. */
static bool RetornoMultiplo(Semantica *s, Decl *d)
{
    TipoId elementos[8];
    if (d->retorno.erro >= 0) {
        return SemErro(s, E0314, d->retorno.pos_tamanho,
                       "retorno multiplo nao tem '! E': ponha o erro num elemento ?E (-> (valor: T, erro: ?E))");
    }
    if (d->nretornos < 2 || d->nretornos > 8) {
        return SemErro(s, E0314, d->retorno.pos, "retorno multiplo tem de 2 a 8 valores (tem %u); um so valor e '-> T'", d->nretornos);
    }
    for (uint32_t i = 0; i < d->nretornos; i++) {
        if (!ElementoDoRetorno(s, d, &d->retornos[i], i)) return false;
        elementos[i] = d->retornos[i].tipo.tipo;
    }
    d->retorno.tipo = TipoTupla(&s->tipos, elementos, d->nretornos);
    return true;
}

static bool RetornoDoProc(Semantica *s, Decl *d)
{
    d->retorno.tipo = TIPO_VAZIO;
    if (!d->tem_retorno) return true;
    if (d->retorno.forma == TR_TUPLA) return RetornoMultiplo(s, d);
    return SemResolverTipoRef(s, &d->retorno) &&
           SemExigeCopiavel(s, d->retorno.tipo, d->retorno.pos, "retorno por valor: devolva o endereco (^T)");
}

static bool DeclProc(Semantica *s, Decl *d)
{
    if ((!d->externo || d->simbolo >= 0) && !SemVerificarCaixa(s, d->nome, d->pos, CAIXA_SNAKE)) return false; /* foreign sem alias: nome = simbolo C */
    if (SemSimboloCEhAlias(s, d->modulo, d->nome)) return ErroSimboloCDeAlias(s, d->nome, d->pos); /* v1.10: o nome de um proc nao pode ser o simbolo C de um alias do mesmo modulo */
    for (uint32_t i = 0; i < d->nparams; i++) {
        if (!ParamDoProc(s, d, &d->params[i], i)) return false;
    }
    if (!RetornoDoProc(s, d)) return false;
    if (!(d->externo ? SemChecarExterno(s, d) : SemChecarExportado(s, d))) return false;
    if (d->posse_c != POSSE_C_NENHUMA && !SemChecarPosseC(s, d)) return false; /* M6c P85: tipo do handle da clausula (E0912) */
    if (d->externo) SemLibmNotarDecl(s, d); /* S3 P4: foreign de libm para o std:math (so com o forja migrar) */
    d->indice = (int32_t)s->qtd_proc++;
    Simbolo y = {SIM_PROC, d->indice, d, TIPO_VAZIO, 0};
    if (SemDeclararGlobal(s, d, &y)) return true;
    SemSugerirAliasDeForeign(s, d); /* IA1-b06: foreign sem alias cujo nome a linguagem reserva */
    return false;
}

/* ---------- passe A ---------- */

static bool PasseDaCategoria(Semantica *s, DeclTipo de, DeclTipo ate, ChecaDecl checa, uint32_t presentes)
{
    uint32_t faixa = ((2u << ate) - 1u) & ~((1u << de) - 1u);
    bool ok = true;
    if ((presentes & faixa) == 0) return true; /* nenhuma declaracao da categoria: o passe nao teria o que visitar */
    for (Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no < de || d->tipo_no > ate || d->com_erro) continue; /* com_erro: o 1o erro da declaracao ja esta na lista */
        SemEntrarNaDecl(s, d);
        s->tocou_veneno = false;
        if (checa(s, d)) continue;
        d->com_erro = true;
        SemEnvenenar(s, d); /* L8: o uso desta declaracao vira TIPO_VENENO e o erro que viria dele e calado */
        ok = false;
        if (!SemGuardarErro(s, d->modulo)) break; /* lista cheia: o resto seria cortado */
    }
    return ok;
}

enum { PASSO_SEMPRE, PASSO_SO_COM_STRUCT_ANINHADA, PASSO_SO_COM_PROC_VALOR }; /* v1.12: false/true antigos = SEMPRE/STRUCT_ANINHADA */
typedef struct { DeclTipo de, ate; ChecaDecl checa; uint8_t condicao; } PassoA;

/* Ordem do passe A: unit, enum, nome das structs e das unions, const escalar, variantes das unions, campos das structs,
 * variantes donas, const de struct, assinaturas de proc. */
static const PassoA PASSOS_A[] = {
    {D_UNIDADE_BASE, D_UNIDADE_DERIV, DeclUnidade, false}, {D_ENUM, D_ENUM, DeclEnum, false},
    {D_STRUCT, D_STRUCT, RegistrarStruct, false}, {D_UNIAO, D_UNIAO, RegistrarUniao, false},
    {D_CONST, D_CONST, DeclConstEscalar, false}, {D_UNIAO, D_UNIAO, DeclUniao, false},
    {D_STRUCT, D_STRUCT, DeclStruct, false}, {D_STRUCT, D_STRUCT, SemConferirProcsDaStruct, PASSO_SO_COM_PROC_VALOR},
    {D_STRUCT, D_STRUCT, ConferirCicloDaStruct, true},
    {D_STRUCT, D_STRUCT, ConferirCamposStruct, true},
    {D_UNIAO, D_UNIAO, ConferirDonoDaUniao, false}, {D_CONST, D_CONST, DeclConstStruct, false},
    {D_PROC, D_PROC, DeclProc, false}, {D_ASSERT, D_ASSERT, SemDeclAssert, false},
};

bool SemPasseA(Semantica *s)
{
    uint32_t presentes = 0;
    bool ok = true;
    if (!SemChecarModificadores(s, &presentes)) { /* fatal: o diagnostico fica pendente em s->diag (SemanticaAnalisar o guarda) */
        s->pula_passe_b = true;
        return false;
    }
    for (size_t i = 0; i < sizeof PASSOS_A / sizeof PASSOS_A[0] && !s->lista.cortado; i++) {
        if (PASSOS_A[i].condicao == PASSO_SO_COM_STRUCT_ANINHADA && !s->tem_struct_aninhada) continue; /* sem struct dentro de struct: nao percorre as Decl */
        if (PASSOS_A[i].condicao == PASSO_SO_COM_PROC_VALOR && !s->usa_proc_valor) continue;           /* v1.12: sem tipo proc de campo, nao percorre as Decl */
        bool passo_ok = PasseDaCategoria(s, PASSOS_A[i].de, PASSOS_A[i].ate, PASSOS_A[i].checa, presentes);
        ok = passo_ok && ok;
        if (!passo_ok && PASSOS_A[i].checa == ConferirCicloDaStruct) { /* fatal: struct que contem a si mesma faz as conferencias seguintes recursarem sem fim */
            s->pula_passe_b = true;
            break;
        }
    }
    return ok;
}

/* ---------- passe B ---------- */

/* Estado do corpo em analise; os parametros entram no escopo como locais 0..nparams-1. */
static void IniciarCorpo(Semantica *s, Decl *d)
{
    s->proc = d;
    s->prox_local = (int32_t)d->nparams;
    s->prof_laco = 0;
    s->marca_laco = 0;
    s->rastreados.n = 0;
    s->nomes_locais.n = 0;
    s->em_teste = d->tipo_no == D_TEST;
    s->em_defer = false;
    if (s->em_teste) d->retorno.tipo = TIPO_VAZIO;
    const TipoInfo *r = TipoDe(&s->tipos, d->retorno.tipo);
    s->erro_do_proc = r->classe == TC_RESULTADO ? r->b : 0;
    s->corpo_tem_ponteiro = false;
    s->corpo_muta_colecao = false;
    s->corpo_com_erro = false;
    s->nomes_relatados.n = 0;
    SemHigieneIniciarCorpo(s);
    SemMarcarPonteiroDoCorpo(s, d->retorno.tipo);
    for (uint32_t i = 0; i < d->nparams; i++) {
        LocalInfo local = {d->params[i].nome, d->params[i].tipo.tipo, LOC_PARAM, 0, (int32_t)i, 0, d->params[i].pos, 0, 0};
        EscopoEmpurrar(&s->escopo, &local);
        SemGravarNomeLocal(s, local.nome, local.tipo, local.pos);
        SemMarcarPonteiroDoCorpo(s, local.tipo);
    }
}

/* M6 P65a: copia a tabela do corpo (um LocalNome por local_id) para a arena; a Decl a guarda para a depuracao. */
static void GuardarNomesDosLocais(Semantica *s, Decl *d)
{
    size_t bytes = s->nomes_locais.n * sizeof(LocalNome);
    d->nomes_locais = ArenaAlocar(s->arena, bytes);
    if (bytes > 0) memcpy(d->nomes_locais, s->nomes_locais.dados, bytes);
}

/* proc com valor de retorno ('T' ou 'T ! E') precisa terminar em return; '! E' nao. */
static bool TerminaEmReturn(Semantica *s, const Decl *d)
{
    const Stmt *ultimo = d->corpo;
    if (!d->tem_retorno || TipoRefSoErro(&d->retorno)) return true;
    while (ultimo != NULL && ultimo->prox != NULL) ultimo = ultimo->prox;
    if (ultimo != NULL && ultimo->tipo_no == ST_RETURN) return true;
    return SemErro(s, E0302, d->pos_fim, "proc com retorno precisa terminar em return");
}

static bool TituloDoTest(Semantica *s, const Decl *d)
{
    if (InternerTam(s->nomes, d->nome) == 0) return SemErro(s, E0802, d->pos_titulo, "o titulo do test esta vazio");
    /* S4 P1: o titulo e unico por modulo (modulo + 1 no byte; as Decl de cada modulo sao contiguas, ModulosOrdenarDecls). */
    uint8_t dono = (uint8_t)(d->modulo + 1);
    if (s->titulo_visto[d->nome] == dono) {
        return SemErro(s, E0802, d->pos_titulo, "ja existe um test com o titulo \"%s\"", SemNomeTexto(s, d->nome));
    }
    s->titulo_visto[d->nome] = dono;
    return true;
}

static bool CorpoDaDecl(Semantica *s, Decl *d)
{
    if (d->tipo_no == D_TEST && !TituloDoTest(s, d)) return false;
    uint32_t altura = EscopoAltura(&s->escopo);
    IniciarCorpo(s, d);
    bool ok = SemBloco(s, d->corpo) && (s->corpo_com_erro || TerminaEmReturn(s, d)); /* b15: corpo com erro ja guardado nao confere o return (cascata) */
    d->nlocais = (uint32_t)s->prox_local;
    GuardarNomesDosLocais(s, d);
    d->fuga_relevante = s->corpo_tem_ponteiro;
    EscopoRecuar(&s->escopo, altura);
    return ok;
}

/* Todos os corpos tipados, um erro por corpo (o 1o, guardado na lista); cada corpo sem erro que tem ponteiro entra na fuga logo depois de tipado.
 * Corpo com erro fica de fora da fuga (a AST dele nao ficou toda tipada).
 * REGRA (L8, contrato v1.11): a higiene (L10a: import nao usado, local nao usado, retorno descartado, var nunca alterada) NUNCA roda em
 * declaracao com erro. Quando o L10a entrar, o ponto e este: so apos CorpoDaDecl devolver true E com d->com_erro e s->tocou_veneno falsos
 * (o `continue` de com_erro acima ja pula o proc de assinatura invalida; o ramo de erro abaixo nunca chega la). */
static bool CorposDoPrograma(Semantica *s, CorposDaFuga *fuga)
{
    bool ok = true;
    for (Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        if (d->externo) continue; /* foreign: sem corpo */
        if (d->tipo_no != D_PROC && d->tipo_no != D_TEST) continue;
        if (d->com_erro) continue; /* L8: assinatura invalida (passe A): o erro ja esta na lista e o corpo seria cascata */
        SemEntrarNaDecl(s, d);
        s->tocou_veneno = false;
        s->corpo_com_erro = false;
        bool clong_ok = !s->usa_clong || SemCLongCorpo(s, d); /* M6c P83: E0342 antes de tipar (a tipagem por alvo diria outro erro) */
        if (clong_ok && CorpoDaDecl(s, d) && !s->corpo_com_erro) {
            SemHigieneGuardar(s, d); /* L10a: o achado de higiene espera a fuga (so vale em declaracao sem outro erro) */
            if (s->tocou_veneno) continue; /* corpo que tocou o veneno tem a AST meio tipada: fica fora da fuga */
            FugaCorposRegistrar(fuga, d);
            if (d->fuga_relevante && s->corpo_muta_colecao && !SemEmprestimoCorpo(s, d)) { /* M6e P93g: E0913 em ordem do fonte */
                d->com_erro = true;
                ok = false;
                if (s->lista.cortado) break;
            }
            continue;
        }
        d->com_erro = true;
        SemEnvenenar(s, d); /* L8: proc com erro no corpo mantem a assinatura (so Simbolo.envenenado) */
        ok = false;
        /* b15: os erros dos comandos ja estao na lista (SemRecuperar); sobra o pendente de fora de comando (titulo do test, return) */
        if (s->diag->tem && !SemGuardarErro(s, d->modulo)) break; /* lista cheia: o resto seria cortado */
        if (s->lista.cortado) break;
    }
    return ok;
}

bool SemPasseB(Semantica *s)
{
    CorposDaFuga fuga;
    bool clong_ok = SemCLongDecls(s); /* M6c P83: liga s->usa_clong; true sem c_long */
    FugaCorposIniciar(&fuga, s);
    bool corpos_ok = CorposDoPrograma(s, &fuga) && clong_ok;
    s->tocou_veneno = false; /* os E0706 da fuga sao de corpos que nao tocaram o veneno */
    bool fuga_ok = !s->lista.cortado && SemFugaPrograma(&fuga); /* E0706 por fluxo e por callee precisa de todos os corpos tipados */
    FugaCorposLiberar(&fuga);
    bool higiene_ok = SemHigieneRelatar(s);
    return corpos_ok && fuga_ok && higiene_ok;
}

bool SemVerificarMain(Semantica *s)
{
    const Simbolo *y = &s->globais[SYM_MAIN];
    if (y->categoria == SIM_PROC && y->decl->nparams == 0 && (!y->decl->tem_retorno || TipoRefSoErro(&y->decl->retorno))) {
        return true;
    }
    return SemErro(s, E0201, 0, "falta 'proc main()' sem parametros e sem retorno de valor");
}
