/* editar_decl.c - as operacoes 'inserir_decl' e 'remover_decl' do 'forja editar' (ver editar_interno.h) e a insercao de texto que o 'mover_decl' tambem usa.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b (modulo de pasta): a insercao e a remocao valem no ARQUIVO (trecho da Fonte): EditarInserirTexto recebe o arquivo, os "imports do topo" sao os do arquivo, e o inserir_decl num pacote de 2+ arquivos pede "arquivo" (o .fj)
 *            ou uma declaracao vizinha.
 *            0.1.0 - IA1-I5: primeira versao. inserir_decl poe o texto depois de uma declaracao, antes dela (antes do comentario colado nela), no fim do modulo ou depois dos imports, com uma linha
 *            em branco de cada lado. O texto nao e parseado a parte: o parse do arquivo inteiro e o do "forja check" da copia, antes de qualquer gravacao (erro = desfeito com o diagnostico).
 *            remover_decl tira a declaracao com o comentario colado e a linha em branco sobrante, e os imports que so ela usava; recusa se ainda ha referencia, a menos que "forcar": true
 *            (entao o check da copia decide, e a lista pode trocar as referencias na mesma chamada).
 * Verificacao: construir.bat && python tests\editar\verificar_editar_i5.py --forja <forja.exe>
 */
#include <stdio.h>
#include <string.h>

#include "driver/editar_interno.h"

/* ---------- texto a inserir ---------- */

/* O texto sem linhas em branco no comeco e sem espacos e quebras no fim; NULL (erro em p->erro) se sobra nada. */
static const char *TextoAparado(Plano *p, const char *texto)
{
    size_t ini = 0, fim = strlen(texto);

    while (ini < fim && (texto[ini] == '\n' || texto[ini] == '\r' || texto[ini] == ' ' || texto[ini] == '\t')) ini++;
    while (fim > ini && (texto[fim - 1] == '\n' || texto[fim - 1] == '\r' || texto[fim - 1] == ' ' || texto[fim - 1] == '\t')) fim--;
    if (fim == ini) {
        PlanoErro(p, "o texto a inserir esta vazio");
        return NULL;
    }
    return ArenaCopiarTexto(p->arena, texto + ini, fim - ini);
}

/* Depois da linha cujo conteudo acaba em 'apos': uma linha em branco, o texto e uma linha em branco. Devolve o ponto: o comeco de uma linha de baixo quando ha (o diff so mostra linhas novas),
 * senao o fim da linha (fim do arquivo). */
static uint32_t TextoDepoisDeLinha(const Consulta *q, uint32_t apos, const char *texto, Buf *ins)
{
    uint32_t seguinte = 0, outra = 0;
    bool branco = false, outra_branca = false;

    if (EditarLinhaSeguinte(q, apos, &seguinte, &branco) && !branco) {
        BufFormatar(ins, "\n%s\n\n", texto);
        return seguinte;
    }
    if (branco && EditarLinhaSeguinte(q, seguinte, &outra, &outra_branca)) {
        BufFormatar(ins, "%s\n\n", texto);
        return outra;
    }
    BufFormatar(ins, "\n\n%s", texto);
    return apos;
}

/* O que uma insercao carrega: onde (modulo, lugar, vizinha), o texto, o ponto de gravacao e o texto montado com as linhas em branco. */
typedef struct {
    Plano *p;
    uint32_t arquivo;   /* o trecho da Fonte onde o texto entra */
    LugarDoTexto lugar;
    const Decl *vizinha;
    const char *texto;
    uint32_t onde;
    Buf ins;
} Insercao;

/* Antes da linha que comeca em 'ini': o texto e uma linha em branco; se a linha de cima tem texto, outra em branco antes. */
static void TextoAntesDeLinha(const Consulta *q, Insercao *i, uint32_t ini)
{
    if (ini > ConsultaInicioDoArquivo(q, ini) && EditarLinhaTemTexto(q, ini - 1)) BufAnexar(&i->ins, "\n");
    BufAnexar(&i->ins, i->texto);
    BufAnexar(&i->ins, "\n\n");
}

/* Como o ponto de DepoisDosImports recebe o texto: depois de uma linha, no comeco do arquivo, ou depois do cabecalho de comentario. */
typedef enum { TOPO_LINHA, TOPO_COMECO, TOPO_CABECALHO } PontoTopo;

/* Depois dos imports: o fim da linha do ultimo do arquivo, ou o topo do arquivo se nao ha nenhum (sem topo, vale o fim do arquivo). */
static uint32_t DepoisDosImports(const Consulta *q, uint32_t arquivo, PontoTopo *topo)
{
    const Decl *ultimo = NULL;
    uint32_t onde = 0;
    bool cabecalho = false;

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_IMPORT && EditarArquivoDoOffset(q, d->pos) == arquivo) ultimo = d;
    }
    *topo = TOPO_LINHA;
    if (ultimo != NULL) return EditarFimDaLinha(q, ultimo->pos);
    if (!EditarPontoSemImports(q, arquivo, &onde, &cabecalho)) return EditarFimDoConteudo(q, arquivo);
    *topo = cabecalho ? TOPO_CABECALHO : TOPO_COMECO;
    return onde;
}

/* O texto a gravar e o ponto (i->ins e i->onde), para o lugar pedido. false = erro em p->erro. */
static bool PontoDoTexto(Insercao *i)
{
    const Consulta *q = i->p->q;
    FaixaDecl f;
    PontoTopo topo = TOPO_LINHA;

    if (i->lugar == LUGAR_DEPOIS_DE || i->lugar == LUGAR_ANTES_DE) {
        if (!EditarFaixaDaDecl(i->p, i->vizinha, &f)) return false;
        i->onde = i->lugar == LUGAR_DEPOIS_DE ? TextoDepoisDeLinha(q, f.fim_texto, i->texto, &i->ins) : f.ini_texto;
        if (i->lugar == LUGAR_ANTES_DE) TextoAntesDeLinha(q, i, i->onde);
        return true;
    }
    i->onde = i->lugar == LUGAR_APOS_IMPORTS ? DepoisDosImports(q, i->arquivo, &topo) : EditarFimDoConteudo(q, i->arquivo);
    if (topo == TOPO_COMECO) BufFormatar(&i->ins, "%s\n\n", i->texto);
    else if (topo == TOPO_CABECALHO) BufFormatar(&i->ins, "\n\n%s", i->texto);
    else if (i->onde == ConsultaInicioDoArquivo(q, i->onde)) BufFormatar(&i->ins, "%s\n", i->texto);
    else i->onde = TextoDepoisDeLinha(q, i->onde, i->texto, &i->ins);
    return true;
}

/* LIMITE: parametros 5 (o plano, o arquivo, o lugar, a declaracao vizinha e o texto sao a fronteira da insercao, que o inserir_decl e o mover_decl chamam; a struct Insercao fica por dentro). */
bool EditarInserirTexto(Plano *p, uint32_t arquivo, LugarDoTexto lugar, const Decl *vizinha, const char *texto)
{
    Insercao i = {.p = p, .arquivo = arquivo, .lugar = lugar, .vizinha = vizinha, .texto = TextoAparado(p, texto)};
    bool ok;

    if (i.texto == NULL || !EditarModuloEditavel(p, ModulosPacoteDoArquivo(&p->q->c->modulos, arquivo))) return false;
    ok = PontoDoTexto(&i) && EditarTrocarNoArquivo(p, i.onde, i.onde, i.ins.dados);
    BufLiberar(&i.ins);
    return ok;
}

/* ---------- inserir_decl ---------- */

static bool MarcaLigada(const JsonValor *item, const char *chave)
{
    const JsonValor *v = JsonMembro(item, chave);

    return v != NULL && v->tipo == JSON_BOOL && v->numero != 0;
}

/* A posicao pedida no ops.json: exatamente uma das quatro. false = erro em p->erro. *vizinha = a declaracao de depois_de/antes_de. */
static bool LerLugar(Plano *p, LugarDoTexto *lugar, const Decl **vizinha)
{
    const char *depois = JsonTextoDe(p->item, "depois_de"), *antes = JsonTextoDe(p->item, "antes_de");
    int pedidas = (depois != NULL) + (antes != NULL) + MarcaLigada(p->item, "no_fim") + MarcaLigada(p->item, "no_inicio_depois_dos_imports");
    const char *id = depois != NULL ? depois : antes;

    if (pedidas != 1) return PlanoErro(p, "indique UMA posicao: depois_de (id), antes_de (id), no_fim ou no_inicio_depois_dos_imports");
    *vizinha = NULL;
    if (id != NULL) {
        *vizinha = ConsultaAcharDecl(p->q, id);
        if (*vizinha == NULL) return PlanoErro(p, "'%s' nao e declaracao do projeto (use o nome ou o id de 'forja consultar simbolos')", id);
        if ((*vizinha)->externo) return PlanoErro(p, "'%s' e de um bloco 'foreign': use o proprio bloco como vizinho", id);
    }
    *lugar = depois != NULL ? LUGAR_DEPOIS_DE : (antes != NULL ? LUGAR_ANTES_DE : (MarcaLigada(p->item, "no_fim") ? LUGAR_NO_FIM : LUGAR_APOS_IMPORTS));
    return true;
}

/* O arquivo (trecho da Fonte) da operacao: o da declaracao vizinha ou o de "modulo"/"arquivo" (num pacote de 2+ arquivos, "arquivo" tem de nomear um .fj). Os dois, se vierem, tem de bater no modulo. -1 = erro em p->erro. */
static int ArquivoDaInsercao(Plano *p, const Decl *vizinha)
{
    const char *nome = JsonTextoDe(p->item, "modulo") != NULL ? JsonTextoDe(p->item, "modulo") : JsonTextoDe(p->item, "arquivo");
    uint32_t arquivo = 0;
    int modulo;

    if (nome == NULL && vizinha == NULL) {
        PlanoErro(p, "falta \"modulo\" ou \"arquivo\" (em no_fim e no_inicio_depois_dos_imports)");
        return -1;
    }
    if (vizinha != NULL) {
        modulo = nome != NULL ? EditarAcharModulo(p, nome) : vizinha->modulo;
        arquivo = EditarArquivoDoOffset(p->q, vizinha->pos);
    } else {
        modulo = EditarAcharModuloEArquivo(p, nome, &arquivo);
    }
    if (modulo >= 0 && vizinha != NULL && vizinha->modulo != modulo) {
        PlanoErro(p, "a declaracao de referencia esta em outro modulo que o pedido (\"%s\")", nome);
        return -1;
    }
    return modulo < 0 ? -1 : (int)arquivo;
}

bool EditarInserirDecl(Plano *p, const char *alvo, const char *texto)
{
    LugarDoTexto lugar = LUGAR_NO_FIM;
    const Decl *vizinha = NULL;
    int arquivo;

    (void)alvo;
    if (!LerLugar(p, &lugar, &vizinha)) return false;
    arquivo = ArquivoDaInsercao(p, vizinha);
    return arquivo >= 0 && EditarInserirTexto(p, (uint32_t)arquivo, lugar, vizinha, texto);
}

/* ---------- remover_decl ---------- */

static bool DeclRemovivel(Plano *p, const Decl *d)
{
    bool tipo_ok = ConsultaEhSimbolo(d) || d->tipo_no == D_TEST;

    if (!tipo_ok) return PlanoErro(p, "so se remove proc, struct, enum, union, const, unit e test");
    if (d->externo) return PlanoErro(p, "a declaracao e de um bloco 'foreign': remova o bloco inteiro com trocar_no");
    if (ConsultaEntradaDe(p->q, d) != NULL) return PlanoErro(p, "a declaracao e ponto de entrada (main ou pub export): nao se remove");
    return EditarModuloEditavel(p, d->modulo);
}

/* "arquivo:linha:coluna" do uso, relativo a pasta da raiz. */
static void LocalDoUso(Plano *p, uint32_t off, char *saida, size_t tam)
{
    FontePosicao pos = FonteLocalizarNoArquivo(&p->q->c->fonte, off);
    char rel[EDITAR_CAMINHO_MAX];

    EditarCaminhoRelativo(p, pos.caminho, rel, sizeof rel);
    snprintf(saida, tam, "%.96s:%u:%u", rel, pos.linha, pos.coluna);
}

/* Os usos de fora da declaracao: quantos e os 3 primeiros em 'lista'. */
static uint32_t UsosDeFora(Plano *p, const Decl *d, char *lista, size_t tam)
{
    const Consulta *q = p->q;
    uint32_t ini = ConsultaInicioDecl(q, d), fim = AstFimDecl(&q->fim, d), total = 0;

    lista[0] = '\0';
    for (size_t i = 0; i < q->indice.usos.n; i++) {
        const IndUso *u = VecEm(&q->indice.usos, i);
        char local[128];

        if (u->alvo != d || (u->pos_nome >= ini && u->pos_nome < fim)) continue;
        if (total++ < 3) {
            LocalDoUso(p, u->pos_nome, local, sizeof local);
            snprintf(lista + strlen(lista), tam - strlen(lista), "%s%.120s", total > 1 ? ", " : "", local);
        }
    }
    return total;
}

/* Recusa se a declaracao ainda e usada fora dela, a menos que o ops.json diga "forcar": true. */
static bool SemReferencias(Plano *p, const Decl *d)
{
    char lista[EDITAR_TEXTO_MAX / 2];
    uint32_t usos;

    if (MarcaLigada(p->item, "forcar")) return true;
    usos = UsosDeFora(p, d, lista, sizeof lista);
    if (usos == 0) return true;
    return PlanoErro(p, "'%.*s' ainda tem %u referencia(s) no projeto (%s): troque-as na mesma lista de operacoes e use \"forcar\": true para o check decidir", (int)InternerTam(&p->q->c->nomes, d->nome),
                     InternerTexto(&p->q->c->nomes, d->nome), usos, lista);
}

bool EditarRemoverDecl(Plano *p, const char *id, const char *sem_texto)
{
    const Decl *d = ConsultaAcharDecl(p->q, id);
    FaixaDecl f;
    Imports *imp;
    bool ok;

    (void)sem_texto;
    if (d == NULL) return PlanoErro(p, "'%s' nao e declaracao do projeto (use o nome ou o id de 'forja consultar simbolos')", id);
    if (!DeclRemovivel(p, d) || !SemReferencias(p, d) || !EditarFaixaDaDecl(p, d, &f)) return false;
    imp = ImportsIniciar(p);
    ImportsDescontarDecl(imp, d);
    ok = EditarTrocarNoArquivo(p, f.ini_remocao, f.fim_remocao, "") && ImportsAplicar(imp);
    ImportsLiberar(imp);
    return ok;
}
