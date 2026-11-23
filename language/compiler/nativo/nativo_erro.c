/* nativo_erro.c - erros do backend nativo legiveis por agente: campos extras do JSON, pacote de relato dos erros internos e a injecao de teste.
 * Versao: 0.5.0
 * Changelog: 0.5.0 - 0.21.0 F5-a: comentario do etapa NULL (erro antes da 1a etapa, sem o backend C) e LIMITE de FuncaoDaBirNaPosicao; codigo igual.
 *            0.4.0 - 0.21.0 F4 passo 1: sugestao do X0001 (trocar --backend x64 por --backend c) e a linha do LEIAME que a repete saem; o backend C foi aposentado.
 *            0.3.0 - IA1-b03: ErroOrigem.etapa NULL (backend C) tira "alvo" e "etapa" do JSON; os campos do X01xx abrem o objeto de extras.
 *            0.2.2 - M3 F9b-1: decisoes extraidas de GravarPacote.
 *            0.2.1 - M3 P21: FORJA_TESTE_INJETAR=X0001 (etapa bir): o M3 P21 fechou a ultima construcao com X0001 real (callback), e o cli precisa de um X0001 para conferir o formato do diagnostico (etapa, ir, sugestao, interno:false).
 *            0.2.0 - M3 P19: X0101..X0105 (biblioteca de 'link') sao diagnostico como o X0001 (exit 1, sem pacote de relato, "interno": false, sem sugestao); ErroOrigem.campos entra depois de "etapa".
 *            0.1.4 - M2 P11b: TipoDaAssinatura usa BirTipoPassavel (struct por valor passa a ser cobertura da BIR); comentario do X0906 atualizado (tem produtor real em nativo.c).
 *            0.1.3 - M2 P10b: so a assinatura nova de BirClasseDoTipo (recebe os Tipos).
 *            0.1.2 - M2 P9a: ErroContexto (pedido, erro, origem) em MontarExtras e GravarPacote; TextoDaFonte devolve TrechoFonte; a gravacao
 *                    do pacote usa ArquivoGravarCalado (a falha vira o aviso de AvisarSemRelato, sem a linha extra de ArquivoGravar).
 *            0.1.1 - portao M1: sem TEMP/TMP, falha ao criar a pasta, gravar o pacote ou o erro.json viram aviso em stderr e o erro nao aponta pasta
 *                    invalida (antes caia em "." ou engolia o fopen); codigos de saida do nativo.h; X0906 anotado como RESERVADO (M2).
 *            0.1.0 - backend x64 P8b: NativoRelatarErro e NativoInjetarFalha.
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 *
 * Ordem fixa dos extras (diff estavel): alvo, etapa, campos do erro (X01xx: proc, simbolo, biblioteca, procurei_em), ir, interno, relato.
 *   ir.instr = posicao da IrInstr em PRE-ORDEM na funcao (a instrucao, depois cond, corpo, senao e os casos dela); -1 = a assinatura da funcao.
 * Pacote de relato (so X09xx): %TEMP%\forja_relato_<fnv32(comando + fontes)>\ com comando.txt, versao.txt, <k>_<arquivo>.fj (copias), bir.txt (a BIR
 * ate a funcao do erro), erro.json e LEIAME.txt. A mesma linha de comando com as mesmas fontes cai na mesma pasta (os arquivos sao regravados).
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "nativo/bir/bir_interno.h"
#include "nativo/nativo_erro.h"
#include "nativo/nativo_json.h"

#define ERRO_TAM_PASTA 520
#define ERRO_TAM_ARQUIVO 700
#define ERRO_TAM_CAMINHO (ERRO_TAM_PASTA + ERRO_TAM_ARQUIVO + 2) /* pasta + '/' + nome do arquivo */
#define ERRO_PREFIXO_IR_FUNCAO "IR_FUNCAO"

typedef struct {
    const char *etapa;
    DiagCodigo codigo;
} MapaInjecao;

/* X0906 (etapa layout) tem produtor real desde o M2 P11b (nativo_layout.c); a injecao de teste continua valendo. */
static const MapaInjecao MAPA_INJECAO[] = {
    {ETAPA_BIR, X0904}, {ETAPA_BIR, X0001}, {ETAPA_LAYOUT, X0906}, {ETAPA_ISEL, X0901}, {ETAPA_CODIFICAR, X0902}, {ETAPA_LIGAR, X0903}, {ETAPA_ESCREVER, X0905},
};

/* Posicao da 1a instrucao do main: o erro injetado aponta uma linha de verdade do .fj. */
static uint32_t PosicaoDaInjecao(const NativoPedido *p)
{
    for (uint32_t k = 0; k < p->ir->nfuncoes; k++) {
        const IrFuncao *f = &p->ir->funcoes[k];

        if (f->e_main && f->corpo.primeira != NULL)
            return f->corpo.primeira->pos;
    }
    return 0;
}

bool NativoInjetarFalha(const NativoPedido *pedido, const char *etapa, Diag *erro)
{
    const char *pedido_env = getenv("FORJA_TESTE_INJETAR");

    if (pedido_env == NULL)
        return false;
    for (size_t k = 0; k < sizeof MAPA_INJECAO / sizeof MAPA_INJECAO[0]; k++) {
        if (strcmp(MAPA_INJECAO[k].etapa, etapa) != 0 || strcmp(DiagCodigoTexto(MAPA_INJECAO[k].codigo), pedido_env) != 0)
            continue;
        DiagRelatar(erro, MAPA_INJECAO[k].codigo, PosicaoDaInjecao(pedido), "falha injetada por FORJA_TESTE_INJETAR=%s na etapa %s", pedido_env, etapa);
        return true;
    }
    return false;
}

/* ---- campo ir: de volta do offset do erro para a IrInstr ---- */

typedef struct {
    uint32_t off;
    const char *causa;
    const IrFuncao *corrente, *funcao;
    const IrInstr *instr;       /* NULL = o erro e da assinatura da funcao */
    int32_t indice;
    uint32_t contador;
    bool achou, exato;
} BuscaIr;

/* A causa do X0001 comeca pelo nome da operacao ("IR_MAPA_ACHA ainda nao suportado..."). */
static bool CausaCitaOp(const char *causa, uint8_t op)
{
    const char *nome = BirNomeDoOp(op);
    size_t n = strlen(nome);

    return strncmp(causa, nome, n) == 0 && (causa[n] == ' ' || causa[n] == '\0');
}

static void VisitarBloco(BuscaIr *s, const IrBloco *bloco);

/* Entre varias instrucoes na mesma posicao vale a que a causa cita; sem citacao, a primeira. */
static void VisitarInstr(BuscaIr *s, const IrInstr *i)
{
    bool cita = CausaCitaOp(s->causa, i->op);

    if (i->pos == s->off && (!s->achou || cita)) {
        s->achou = true;
        s->exato = cita;
        s->funcao = s->corrente;
        s->instr = i;
        s->indice = (int32_t)s->contador;
    }
    s->contador++;
    VisitarBloco(s, &i->cond);
    VisitarBloco(s, &i->corpo);
    VisitarBloco(s, &i->senao);
    for (uint32_t k = 0; k < i->ncasos; k++)
        VisitarBloco(s, &i->casos[k].corpo);
}

static void VisitarBloco(BuscaIr *s, const IrBloco *bloco)
{
    for (const IrInstr *i = bloco->primeira; i != NULL && !s->exato; i = i->prox)
        VisitarInstr(s, i);
}

/* X0001 de assinatura ("IR_FUNCAO retorno ..."): a posicao e a da 1a instrucao do corpo da funcao. */
static void BuscarAssinatura(BuscaIr *s, const IrPrograma *ir)
{
    for (uint32_t k = 0; k < ir->nfuncoes && !s->achou; k++) {
        const IrFuncao *f = &ir->funcoes[k];

        if (f->corpo.primeira != NULL && f->corpo.primeira->pos == s->off) {
            s->achou = true;
            s->funcao = f;
            s->indice = -1;
        }
    }
}

static BuscaIr LocalizarNaIr(const NativoPedido *p, uint32_t off, const char *causa)
{
    BuscaIr s = {.off = off, .causa = causa};

    if (strncmp(causa, ERRO_PREFIXO_IR_FUNCAO, strlen(ERRO_PREFIXO_IR_FUNCAO)) == 0) {
        BuscarAssinatura(&s, p->ir);
        return s;
    }
    for (uint32_t k = 0; k < p->ir->nfuncoes && !s.exato; k++) {
        s.corrente = &p->ir->funcoes[k];
        s.contador = 0;
        VisitarBloco(&s, &s.corrente->corpo);
    }
    return s;
}

/* Tipo que a BIR nao cobre na assinatura: o do retorno ou o do 1o parametro sem classe. */
static const char *TipoDaAssinatura(const NativoPedido *p, const IrFuncao *f)
{
    if (f->retorno != TIPO_VAZIO && !BirTipoPassavel(p->tipos, f->retorno))
        return BirNomeDoTipo(p->tipos, f->retorno);
    for (uint32_t k = 0; k < f->nparams; k++) {
        if (!BirTipoPassavel(p->tipos, f->params[k].tipo))
            return BirNomeDoTipo(p->tipos, f->params[k].tipo);
    }
    return "vazio";
}

/* Tipo da instrucao; as que nao tem destino (print) guardam o tipo no valor 'a'. "-" quando nenhum dos dois diz. */
static const char *TipoDaInstr(const NativoPedido *p, const IrInstr *i)
{
    TipoId tipo = i->tipo != 0 ? i->tipo : i->a.tipo;

    return tipo != 0 ? BirNomeDoTipo(p->tipos, tipo) : "-";
}

bool NativoAnexarIr(Buf *b, const NativoPedido *p, uint32_t off, const char *causa)
{
    BuscaIr achado = LocalizarNaIr(p, off, causa);
    const BuscaIr *s = &achado;
    const char *nome;

    if (!s->achou)
        return false;
    nome = InternerTexto(p->nomes, s->funcao->nome);
    BufAnexar(b, ", \"ir\": {\"funcao\": ");
    JsonTexto(b, nome, strlen(nome));
    BufFormatar(b, ", \"instr\": %d, \"op\": \"%s\", \"op_aux\": %u, \"tipo\": ", (int)s->indice,
                s->instr != NULL ? BirNomeDoOp(s->instr->op) : ERRO_PREFIXO_IR_FUNCAO, s->instr != NULL ? (unsigned)s->instr->op_aux : 0u);
    nome = s->instr != NULL ? TipoDaInstr(p, s->instr) : TipoDaAssinatura(p, s->funcao);
    JsonTexto(b, nome, strlen(nome));
    BufAnexarN(b, "}", 1);
    return true;
}

/* O que um erro do backend precisa para montar os extras: o pedido, o diagnostico e a origem (etapa e BIR). */
typedef struct {
    const NativoPedido *pedido;
    const Diag *erro;
    const ErroOrigem *origem;
} ErroContexto;

/* Diagnostico (exit 1, sem pacote de relato): o X0001 de recurso fora do backend e os X01xx de biblioteca; o resto (X09xx) e erro interno (exit 3). */
static bool EhDiagnostico(DiagCodigo codigo)
{
    return codigo == X0001 || strncmp(DiagCodigoTexto(codigo), "X01", 3) == 0;
}

static void MontarExtras(Buf *b, const ErroContexto *c, const char *relato)
{
    const NativoPedido *p = c->pedido;
    const Diag *erro = c->erro;
    bool interno = !EhDiagnostico(erro->codigo);

    if (c->origem->etapa != NULL) {
        BufFormatar(b, "\"alvo\": \"%s\", \"etapa\": \"%s\"", p->alvo != NULL ? p->alvo : NATIVO_ALVO_PADRAO, c->origem->etapa);
        if (c->origem->campos != NULL)
            BufFormatar(b, ", %s", c->origem->campos);
    } else {
        BufAnexar(b, c->origem->campos); /* etapa NULL (erro antes da 1a etapa): sem alvo e etapa, os campos do X01xx abrem o objeto */
    }
    NativoAnexarIr(b, p, erro->off, erro->causa);
    BufFormatar(b, ", \"interno\": %s", interno ? "true" : "false");
    if (relato != NULL) {
        BufAnexar(b, ", \"relato\": ");
        JsonCaminho(b, relato);
    }
}

/* ---- pacote de relato ---- */

/* Caminho, texto e tamanho do trecho k do fonte (sem trechos, o arquivo inteiro). */
typedef struct {
    const char *caminho;
    const char *texto;
    size_t tam;
} TrechoFonte;

static TrechoFonte TextoDaFonte(const Fonte *f, uint32_t k)
{
    TrechoFonte t = {f->caminho, f->texto, f->tam};

    if (f->ntrechos == 0)
        return t;
    t.caminho = f->trechos[k].caminho;
    t.texto = f->texto + f->trechos[k].inicio;
    t.tam = f->trechos[k].fim - f->trechos[k].inicio;
    return t;
}

static uint32_t QtdFontes(const Fonte *f)
{
    return f->ntrechos == 0 ? 1 : f->ntrechos;
}

static uint32_t HashDoRelato(const NativoPedido *p)
{
    const char *comando = p->comando != NULL ? p->comando : "";
    uint32_t h = Fnv32(FNV32_INICIO, comando, strlen(comando));

    for (uint32_t k = 0; k < QtdFontes(p->fonte); k++) {
        TrechoFonte t = TextoDaFonte(p->fonte, k);

        h = Fnv32(h, t.texto, t.tam);
    }
    return h;
}

static bool CriarPasta(const char *caminho)
{
#ifdef _WIN32
    int r = _mkdir(caminho);
#else
    int r = mkdir(caminho, 0777);
#endif
    return r == 0 || errno == EEXIST;
}

/* O erro ja saiu em stdout; o que faltou do pacote de relato vai para stderr, para o usuario nao procurar uma pasta que nao existe. */
static void AvisarSemRelato(const char *motivo, const char *detalhe)
{
    fprintf(stderr, "forja: aviso: pacote de relato nao gerado: %s%s\n", motivo, detalhe);
}

static bool AbrirPastaDoRelato(const NativoPedido *p, char *pasta, size_t n)
{
    const char *base = getenv("TEMP");

    if (base == NULL)
        base = getenv("TMP");
    if (base == NULL) {
        AvisarSemRelato("o ambiente nao tem TEMP nem TMP", "");
        return false;
    }
    snprintf(pasta, n, "%s/forja_relato_%08x", base, (unsigned)HashDoRelato(p));
    if (!CriarPasta(pasta)) {
        AvisarSemRelato("nao consegui criar a pasta ", pasta);
        return false;
    }
    return true;
}

static bool GravarNaPasta(const char *pasta, const char *nome, const char *dados, size_t tam)
{
    char caminho[ERRO_TAM_CAMINHO];

    snprintf(caminho, sizeof caminho, "%s/%s", pasta, nome);
    return ArquivoGravarCalado(caminho, dados, tam, NULL);
}

static bool GravarTexto(const char *pasta, const char *nome, const char *texto)
{
    return GravarNaPasta(pasta, nome, texto, strlen(texto));
}

static const char *NomeDoArquivo(const char *caminho)
{
    const char *barra = strrchr(caminho, '/');
    const char *contra = strrchr(caminho, '\\');

    if (contra != NULL && (barra == NULL || contra > barra))
        barra = contra;
    return barra != NULL ? barra + 1 : caminho;
}

static bool GravarFontes(const char *pasta, const Fonte *f)
{
    bool ok = true;

    for (uint32_t k = 0; k < QtdFontes(f); k++) {
        TrechoFonte t = TextoDaFonte(f, k);
        char nome[ERRO_TAM_ARQUIVO];

        if (k == 0)
            snprintf(nome, sizeof nome, "%s", NomeDoArquivo(t.caminho));
        else
            snprintf(nome, sizeof nome, "%u_%s", (unsigned)k, NomeDoArquivo(t.caminho));
        ok = GravarNaPasta(pasta, nome, t.texto, t.tam) && ok;
    }
    return ok;
}

/* Indice+1 da 1a funcao da BIR (entre as n completas) que tem uma instrucao na posicao do erro; 0 se nenhuma.
 * LIMITE: niveis 4 (funcao, bloco, instrucao e a comparacao da posicao; partir criaria helper de uso unico). */
static uint32_t FuncaoDaBirNaPosicao(const BirPrograma *bir, uint32_t n, uint32_t off)
{
    for (uint32_t k = 0; k < n; k++) {
        const BirFuncao *f = &bir->funcoes[k];

        for (uint32_t b = 0; b < f->nblocos; b++) {
            for (uint32_t i = 0; i < f->blocos[b].n; i++) {
                if (f->blocos[b].instrs[i].pos == off)
                    return k + 1;
            }
        }
    }
    return 0;
}

/* A BIR ate a funcao do erro (inclusive); se a posicao nao cai em funcao nenhuma, todas as completas. */
static bool GravarBir(const char *pasta, const Diag *erro, const ErroOrigem *origem)
{
    Buf texto = {0};
    BirPrograma parcial;
    bool ok;

    if (origem->bir == NULL) {
        return GravarTexto(pasta, "bir.txt", "(sem BIR: o erro veio antes de ela existir)\n");
    }
    parcial = *origem->bir;
    parcial.nfuncoes = FuncaoDaBirNaPosicao(origem->bir, origem->bir_funcoes, erro->off);
    if (parcial.nfuncoes == 0)
        parcial.nfuncoes = origem->bir_funcoes;
    BirImprimir(&parcial, &texto);
    ok = GravarNaPasta(pasta, "bir.txt", texto.dados != NULL ? texto.dados : "", texto.tam);
    BufLiberar(&texto);
    return ok;
}

static bool GravarLeiame(const char *pasta, const NativoPedido *p, const Diag *erro, const char *etapa)
{
    Buf texto = {0};
    TrechoFonte t = TextoDaFonte(p->fonte, 0);
    bool ok;

    BufFormatar(&texto, "Pacote de relato do forja %s: erro interno %s na etapa '%s' do backend nativo x64.\n", p->versao, DiagCodigoTexto(erro->codigo), etapa);
    BufFormatar(&texto, "Reproduza nesta pasta: forja build --backend x64 %s -o x.exe (a linha original esta em comando.txt).\n", NomeDoArquivo(t.caminho));
    BufAnexar(&texto, "Conteudo: comando.txt, versao.txt, os .fj (o principal e os modulos como <k>_<nome>), bir.txt (a BIR ate a funcao) e erro.json.\n");
    BufAnexar(&texto, "Nada sai desta maquina sozinho: envie esta pasta inteira a quem mantem a Forja.\n");
    ok = GravarNaPasta(pasta, "LEIAME.txt", texto.dados, texto.tam);
    BufLiberar(&texto);
    return ok;
}

static bool GravarVersao(const char *pasta, const NativoPedido *p, const char *etapa)
{
    char texto[200];

    snprintf(texto, sizeof texto, "forja %s\nalvo %s\nmodo %s\netapa %s\n", p->versao, p->alvo != NULL ? p->alvo : NATIVO_ALVO_PADRAO,
             p->modo == BM_RELEASE ? "release" : "dev", etapa);
    return GravarTexto(pasta, "versao.txt", texto);
}

static bool GravarComando(const char *pasta, const NativoPedido *p)
{
    Buf comando = {0};
    bool ok;

    BufFormatar(&comando, "%s\n", p->comando != NULL ? p->comando : "");
    ok = GravarNaPasta(pasta, "comando.txt", comando.dados, comando.tam);
    BufLiberar(&comando);
    return ok;
}

/* Os arquivos do pacote, todos tentados mesmo que um falhe; false = algum nao gravou. */
static bool GravarArquivosDoPacote(const ErroContexto *c, const char *pasta)
{
    const NativoPedido *p = c->pedido;
    bool ok = GravarComando(pasta, p);

    ok = GravarVersao(pasta, p, c->origem->etapa) && ok;
    ok = GravarFontes(pasta, p->fonte) && ok;
    ok = GravarBir(pasta, c->erro, c->origem) && ok;
    ok = GravarLeiame(pasta, p, c->erro, c->origem->etapa) && ok;
    return ok;
}

/* Tudo menos o erro.json, que precisa do caminho da pasta dentro dos extras. false = nao deu para montar a pasta. */
static bool GravarPacote(const ErroContexto *c, char *pasta, size_t n)
{
    bool ok;

    if (!AbrirPastaDoRelato(c->pedido, pasta, n))
        return false;
    ok = GravarArquivosDoPacote(c, pasta);
    if (!ok)
        AvisarSemRelato("nao consegui gravar todos os arquivos em ", pasta);
    return ok;
}

static bool GravarErroJson(const char *pasta, const NativoPedido *p, const Diag *erro, const char *extras)
{
    char caminho[ERRO_TAM_CAMINHO];
    FILE *arquivo;

    snprintf(caminho, sizeof caminho, "%s/erro.json", pasta);
    arquivo = fopen(caminho, "wb");
    if (arquivo == NULL) {
        AvisarSemRelato("nao consegui gravar ", caminho);
        return false;
    }
    DiagImprimirJsonExtras(erro, p->fonte, extras, arquivo);
    return fclose(arquivo) == 0;
}

/* Os extras apontam a pasta do relato; se o erro.json nao pode ser gravado nela, sai sem a pasta (nao aponta caminho invalido). */
static void MontarExtrasComRelato(Buf *extras, const NativoPedido *p, const Diag *erro, const ErroOrigem *origem)
{
    ErroContexto c = {p, erro, origem};
    char pasta[ERRO_TAM_PASTA];
    bool com_relato = !EhDiagnostico(erro->codigo) && GravarPacote(&c, pasta, sizeof pasta);

    MontarExtras(extras, &c, com_relato ? pasta : NULL);
    if (com_relato && !GravarErroJson(pasta, p, erro, extras->dados)) {
        BufLiberar(extras);
        MontarExtras(extras, &c, NULL);
    }
}

int NativoRelatarErro(const NativoPedido *pedido, const Diag *erro, const ErroOrigem *origem)
{
    Buf extras = {0};

    MontarExtrasComRelato(&extras, pedido, erro, origem);
    DiagImprimirJsonExtras(erro, pedido->fonte, extras.dados, stdout);
    BufLiberar(&extras);
    return EhDiagnostico(erro->codigo) ? SAIDA_DIAGNOSTICO : SAIDA_INTERNO;
}
