/* editar_interno.h - estado e funcoes compartilhadas pelos arquivos do 'forja editar' (cmd_editar.c, editar_plano.c, editar_renomear.c, editar_nos.c, editar_copia.c e editar_saida.c).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - Linguagem v1.13 L1b: o ARQUIVO (trecho da Fonte) e a unidade de edicao e de import (ImportsApelido e ImportsContar recebem o arquivo; EditarTrocarNoArquivo; EditarArquivoDoOffset; EditarAcharModuloEArquivo;
 *            EditarInserirTexto recebe o arquivo): o modulo de pasta tem varios arquivos por modulo.
 *            0.2.0 - IA1-I5: Plano.item (a operacao do ops.json em curso); PlanoTrocarLocais; inserir_decl, remover_decl e mover_decl (editar_decl.c, editar_mover.c) e o que elas dividem: tokens e linhas do fonte
 *            (editar_geo.c) e a contagem e o ajuste dos imports (editar_imports.c). EDITAR_VERSAO segue 1.0.0: a resposta JSON nao ganhou campo.
 *            0.1.0 - IA1-I4: primeira versao. Um Plano reune as edicoes de TODAS as operacoes do ops.json (todas sobre o estado original do projeto); o motor de edicoes (edicoes.c) as aplica,
 *            o check roda numa copia do projeto em %TEMP% e so entao o que passou e gravado.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_EDITAR_INTERNO_H
#define FORJA_DRIVER_EDITAR_INTERNO_H

#include "common/arena.h"
#include "driver/compilacao.h"
#include "driver/consultar_interno.h"
#include "driver/corrigir_diag.h"
#include "driver/edicoes.h"
#include "driver/json_leitor.h"
#include "frontend/token.h"

#define EDITAR_VERSAO "1.0.0"

enum { EDITAR_OK = 0, EDITAR_RECUSADO = 1, EDITAR_USO = 2, EDITAR_INTERNO = 3, EDITAR_TEXTO_MAX = 400, EDITAR_CAMINHO_MAX = 600 };

/* Um arquivo que o plano toca: o conteudo original inteiro (com o BOM, se tinha) e as edicoes pedidas sobre ele. */
typedef struct {
    const char *caminho;        /* como a Fonte o conhece (raiz: como foi dado; os outros: absoluto) */
    const char *relativo;       /* o mesmo, relativo a pasta da raiz (EditarCaminhoRelativo): o que sai na resposta e a ordem dos arquivos */
    char *original;
    size_t tam;
    Edicao *edicoes;
    uint32_t n, cap;
} ArquivoEditado;

typedef struct {
    const char *op, *alvo, *novo;   /* para a resposta: tipo da operacao, o que foi pedido e o texto novo */
    uint32_t edicoes;               /* quantas edicoes a operacao gerou */
} Operacao;

/* O que as operacoes pedem. 'sintatica' e a segunda compilacao (so sintaxe, a arvore do parser) onde os ids do 'forja ast' sao resolvidos; nasce na 1a vez que e preciso. */
typedef struct {
    Consulta *q;
    Arena *arena;
    ArquivoEditado *arquivos;
    uint32_t narquivos, cap;
    Operacao *operacoes;
    uint32_t noperacoes;
    Compilacao *sintatica;
    const JsonValor *item;          /* a operacao do ops.json em execucao (as chaves alem do alvo e do texto: depois_de, forcar...) */
    char erro[EDITAR_TEXTO_MAX];    /* "" = sem erro; senao a frase do primeiro erro de edicao */
} Plano;

/* ---- editar_plano.c ---- */
/* Troca [(linha, coluna), (linha_fim, coluna_fim)) do arquivo 'caminho' (posicoes locais ao arquivo, sem o BOM) por 'novo'; as quebras de linha de 'novo' viram as do arquivo. false = erro em plano->erro. */
bool PlanoTrocarFaixa(Plano *p, const char *caminho, const Edicao *faixa, const char *novo);
/* Igual, com a faixa dada por offsets [ini, fim) do texto global da Fonte (os dois no mesmo modulo). */
bool PlanoTrocarOffsets(Plano *p, uint32_t ini, uint32_t fim, const char *novo);
/* Registra a operacao feita (o que saiu na resposta); 'edicoes_antes' = o total de edicoes do plano antes dela.
 * LIMITE: parametros 5 (os quatro campos de uma linha do registro mais o plano; agrupar em struct so criaria um tipo de uso unico). */
void PlanoRegistrar(Plano *p, const char *op, const char *alvo, const char *novo, uint32_t edicoes_antes);
uint32_t PlanoTotalEdicoes(const Plano *p);
bool PlanoErro(Plano *p, const char *fmt, ...) __attribute__((format(gnu_printf, 2, 3)));   /* grava a frase em plano->erro e devolve false */
void PlanoOrdenar(Plano *p);    /* arquivos por caminho (barras normais) e edicoes por posicao */
void PlanoLiberar(Plano *p);

/* Igual, com a faixa dada por offsets [ini, fim) LOCAIS ao arquivo 'caminho' (sem o BOM): o fim do arquivo nao tem posicao no texto global dos modulos. */
bool PlanoTrocarLocais(Plano *p, const char *caminho, uint32_t ini, uint32_t fim, const char *novo);

/* ---- editar_geo.c: tokens e linhas do fonte de um modulo (offsets globais da Fonte) ---- */
enum { TOKEN_AUSENTE = UINT32_MAX };
const Token *EditarToken(const Consulta *q, uint32_t i);
uint32_t EditarTokenExato(const Consulta *q, uint32_t off);     /* indice do token que comeca em off; TOKEN_AUSENTE se nenhum */
uint8_t EditarModuloDoOffset(const Consulta *q, uint32_t off);  /* o modulo (pacote) dono do arquivo que contem o offset */
uint32_t EditarArquivoDoOffset(const Consulta *q, uint32_t off); /* v1.13: o arquivo (trecho da Fonte) que contem o offset; o modulo de pasta tem varios por modulo */
uint32_t EditarInicioDaLinha(const Consulta *q, uint32_t off);  /* comeco da linha que contem off */
uint32_t EditarFimDaLinha(const Consulta *q, uint32_t off);     /* fim do conteudo da linha que contem off (sem o CR/LF) */
uint32_t EditarProximaLinha(const Consulta *q, uint32_t off);   /* comeco da linha seguinte (o fim do modulo se nao ha) */
bool EditarLinhaTemTexto(const Consulta *q, uint32_t off);         /* a linha que contem off nao esta em branco? */
bool EditarLinhaSeguinte(const Consulta *q, uint32_t off, uint32_t *ini, bool *em_branco); /* a linha depois da de off: existe? *ini = comeco dela, *em_branco = so espacos */
uint32_t EditarFimDoConteudo(const Consulta *q, uint32_t arquivo); /* fim da ultima linha com algo escrito no arquivo (trecho da Fonte); o comeco do arquivo se esta vazio */
uint32_t EditarTokenDoNome(const Consulta *q, const IndUso *u); /* o token do NOME da declaracao num uso (o indice pode guardar o 1o segmento de "apelido.nome"); TOKEN_AUSENTE se nao acha */
/* Onde entra o que vai no topo do arquivo quando nao ha import: com cabecalho de comentario separado por linha em branco, *onde = o fim da ultima linha do cabecalho e *apos_cabecalho = true
 * (entra "\n\n" + texto); senao *onde = o comeco do arquivo (entra texto + "\n\n"). false = o arquivo so tem o cabecalho ou nada: entra no fim. */
bool EditarPontoSemImports(const Consulta *q, uint32_t arquivo, uint32_t *onde, bool *apos_cabecalho);
/* Troca [ini, fim) (offsets globais, os dois no mesmo arquivo: o de 'ini') por 'novo'. false = erro em p->erro. */
bool EditarTrocarNoArquivo(Plano *p, uint32_t ini, uint32_t fim, const char *novo);
/* O modulo e arquivo do projeto que se edita (nao std, vendor nem engine)? Senao erro em p->erro. */
bool EditarModuloEditavel(Plano *p, uint8_t modulo);
/* O modulo que o texto designa: caminho do import ("geo/vetor", "main"), o mesmo com ".fj" ou o arquivo relativo a pasta da raiz. -1 = nenhum (erro em p->erro). */
int EditarAcharModulo(Plano *p, const char *texto);
/* Igual, e grava em *arquivo o arquivo (trecho da Fonte) onde a edicao cai: o 1o (e unico) do modulo, ou o que o texto nomeia. Pacote (pasta) de 2+ arquivos nomeado so pelo modulo = erro em p->erro. */
int EditarAcharModuloEArquivo(Plano *p, const char *texto, uint32_t *arquivo);

/* Onde uma declaracao esta no arquivo: [ini_texto, fim_texto) = do comentario '//' colado acima ate o fim da ultima linha dela;
 * [ini_remocao, fim_remocao) = o que sai do arquivo ao remove-la (as linhas e uma linha em branco sobrante; se e a ultima coisa do arquivo, sai a de antes). */
typedef struct { uint32_t ini_texto, fim_texto, ini_remocao, fim_remocao; } FaixaDecl;
bool EditarFaixaDaDecl(Plano *p, const Decl *d, FaixaDecl *f);

/* ---- editar_imports.c: que imports cada modulo passa a usar ou deixa de usar ---- */
typedef struct Imports Imports;
Imports *ImportsIniciar(Plano *p);
void ImportsLiberar(Imports *imp);
/* v1.13: o import vale por ARQUIVO; x e o arquivo (trecho da Fonte, EditarArquivoDoOffset) e k o modulo importado.
 * O apelido com que o arquivo x chama o modulo k; se x ainda nao o importa, agenda o import (apelido 'preferido' = nome internado, ou -1 = o padrao). NULL = erro em p->erro. */
const char *ImportsApelido(Imports *imp, uint32_t x, uint8_t k, int32_t preferido);
void ImportsContar(Imports *imp, uint32_t x, uint8_t k, int delta);  /* o arquivo x passa a usar (delta > 0) ou deixa de usar (delta < 0) o modulo k 'delta' vezes */
void ImportsDescontarDecl(Imports *imp, const Decl *d);              /* tira do arquivo de d o que d usa de outros modulos (d sai do arquivo) */
bool ImportsAplicar(Imports *imp);                                   /* apaga o import que ficou sem uso (E1005) e acrescenta os agendados; false = erro em p->erro */
bool EditarUsoEhDeDecl(const IndUso *u);                             /* o uso e do NOME de uma declaracao global (nao de campo, membro, parametro ou local) */
bool EditarUsoQualificado(const Consulta *q, const IndUso *u, uint32_t *token_nome); /* o nome vem depois de "apelido." do modulo da declaracao; *token_nome = indice do token do nome */

/* ---- editar_decl.c e editar_mover.c ---- */
typedef enum { LUGAR_DEPOIS_DE, LUGAR_ANTES_DE, LUGAR_NO_FIM, LUGAR_APOS_IMPORTS } LugarDoTexto;
/* Acrescenta ao plano a insercao de 'texto' (declaracoes sem quebra no fim) no lugar, com as linhas em branco certas; 'vizinha' so em DEPOIS_DE e ANTES_DE. 'arquivo' = o trecho da Fonte onde entra (o da vizinha, se ha). */
bool EditarInserirTexto(Plano *p, uint32_t arquivo, LugarDoTexto lugar, const Decl *vizinha, const char *texto);
bool EditarInserirDecl(Plano *p, const char *alvo, const char *texto);
bool EditarRemoverDecl(Plano *p, const char *id, const char *sem_texto);
bool EditarMoverDecl(Plano *p, const char *id, const char *para);

/* ---- editar_renomear.c ---- */
bool EditarRenomear(Plano *p, const char *alvo, const char *novo);

/* ---- editar_nos.c ---- */
bool EditarTrocarNo(Plano *p, const char *id, const char *texto);
bool EditarTrocarCorpo(Plano *p, const char *id, const char *texto);

/* ---- editar_copia.c: o check de "depois" numa copia do projeto em %TEMP% ---- */
/* Copia o projeto (cada arquivo de modulo com o texto novo em 'novos', se foi editado), roda "forja check" na raiz da copia e apaga a copia. 'novos' tem plano->narquivos buffers.
 * *prefixo recebe (arena do plano) a pasta da copia em barras normais com a barra final, para trocar nos diagnosticos. false = o check nao rodou (erro em plano->erro).
 * LIMITE: parametros 5 (plano e novos sao a entrada; arena, depois e prefixo sao as tres saidas da fronteira; um chamador so). */
bool EditarVerificarCopia(Plano *p, const Buf *novos, Arena *arena, ResultadoCheck *depois, const char **prefixo);

/* ---- editar_saida.c: a resposta JSON ---- */
typedef struct {
    const char *alvo;
    bool aplicar, valido, aplicado;
    int64_t erros_depois;           /* -1 = o check de depois nao rodou */
    const char *prefixo_copia;      /* trocado por "" nos diagnosticos (a copia some; o caminho fica relativo ao projeto) */
    const char *motivo;             /* NULL = nenhum */
    const ResultadoCheck *depois;   /* NULL = nao rodou */
    const Buf *diff;
} Resposta;
/* Caminho para mostrar: relativo a pasta da raiz do projeto, com barras normais (absoluto se estiver fora dela). */
void EditarCaminhoRelativo(const Plano *p, const char *caminho, char *saida, size_t tam);
void EditarEmitirResposta(const Plano *p, const Resposta *r);

#endif
