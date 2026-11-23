/* renomeados.h - tabela UNICA dos nomes em portugues da superficie da Forja que viram ingles (Emenda 4 do contrato IA1, A32), passos L3a e L3c.
 * Uma linha por nome em RENOMEADOS (renomeados.c): classe, onde vale, nome antigo, nome novo, uma nota curta e o flag 'recusar'. A mesma tabela alimenta (1) a secao
 * "RENOMEADOS" de doc\migracao.txt, gerada por tests\ia_first\gerar_migracao_doc.py, (2) o compilador, que aceita as DUAS formas (L3c: a forma antiga vale em silencio e vira a
 * nova na leitura: ver RenomeadoAchar) e (3) nos passos seguintes do L3, o forja migrar. Para acrescentar um nome: uma linha na tabela, regerar a migracao.txt.
 * Recusar a forma antiga (L3f) e trocar 'recusar' de false para true na linha: o compilador que a consulta passa a dar E0110 "forma antiga; rode forja migrar" (RenomeadoRecusar).
 * ESCOPO das classes de modulo (L3c2): proc, tipo e param valem no MODULO INTEIRO (o escopo e o caminho canonico do import, ex. "vendor:db/db"; param nao distingue o proc: o rotulo
 * antigo vale em qualquer proc do modulo); membro vale por enum (escopo = caminho do modulo + o nome NOVO do enum, ex. "vendor:db/db DbError"); pacote e a pasta (1o segmento) de um import
 * "vendor:"; modulo e o caminho inteiro de um import; qualificador e o apelido padrao (ultimo segmento do caminho escrito).
 * Versao: 0.8.0
 * Changelog: 0.8.0 - S3 P4 (L11b-2, D10): classes REN_LIBM (chamada de foreign de libm -> math.<fn> ou o embutido sqrt; a linha da tabela e pelo simbolo C), REN_LIBM_LINHA
 *            (linha inteira: sai a declaracao do foreign, entra o import "std:math"; trocas de linha inteira saem do teste de linhas iguais) e REN_LIBM_AVISO (motivo de nao migrar; nao vira edicao).
 *            0.7.0 - B01d: classe REN_ARGUMENTO_NOVO (a insercao de ', alloc = heap' do forja migrar; sem linha na tabela).
 *            0.6.0 - IA1-L3f: RenomeadoRecusa e RenomeadoTolerarAntiga (modo interno do forja migrar); todas as linhas da tabela com recusar = true.
 *            0.5.0 - IA1-L3d1: RenTroca, RenTrocas, RenomeadoNotar e RenomeadoNotarTabela (renomeados_diag.c): cada ponto do compilador que resolve uma forma antiga da tabela a registra em
 *            Diag.trocas (posicao e texto antigo/novo); o forja migrar (driver\cmd_migrar.c) le a lista e troca no texto. Sem lista ligada (check, build, run) nada e registrado.
 *            0.4.0 - IA1-L3c3: so dados (renomeados.c): json, compression e net; nenhuma API nova.
 *            0.3.0 - IA1-L3c2: classe REN_PARAM (rotulo de argumento nomeado de proc de modulo; REN_ROTULO fica para os embutidos); RenomeadoAcharEm (escopo com mais de uma palavra:
 *            modulo + enum do membro); as entradas de vendor:banco, dos pacotes vendor e dos membros de DbError.
 *            0.2.0 - IA1-L3c: Renomeado.recusar; RenomeadoBuscarNovo, RenomeadoAchar, RenomeadoRecusar (renomeados_diag.c).
 *            0.1.0 - IA1-L3a: primeira versao (8 nomes ja decididos: em, escopo, para, std:arquivo, arquivo., abrir, executar, ErroBanco).
 * Verificacao: construir.bat && teste_common.exe && python -I ..\tests\ia_first\gerar_migracao_doc.py --verificar
 */
#ifndef FORJA_COMMON_RENOMEADOS_H
#define FORJA_COMMON_RENOMEADOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/diag.h"
#include "common/vec.h"

/* A classe diz que tipo de nome e e, com o escopo, onde ele vale (renomeados.c: CLASSE_TEXTO). */
typedef enum {
    REN_ROTULO, REN_VALOR_ALOCADOR, REN_MODULO, REN_QUALIFICADOR, REN_PROC, REN_TIPO, REN_MEMBRO, REN_PACOTE, REN_PARAM,
    REN_ARGUMENTO_NOVO, /* B01d: nao vem da tabela: e a INSERCAO do argumento obrigatorio novo (alloc = heap) que o forja migrar grava (tam = 0) */
    REN_LIBM,           /* S3 P4: chamada de foreign de libm; a tabela tem uma linha por simbolo C (antigo = simbolo C, novo = math.<fn> ou sqrt) */
    REN_LIBM_LINHA,     /* S3 P4: linha inteira (tam > 0: sai a declaracao do foreign; tam = 0: entra o import "std:math") */
    REN_LIBM_AVISO,     /* S3 P4: foreign de libm que NAO migra: novo = o motivo (so relato, nunca edicao) */
    REN_CLASSE_QTD
} RenClasse;

/* escopo: onde o nome vale (palavras separadas por espaco, ex. "make new delete format format_c"; para proc/tipo/membro, o caminho do import do modulo); antigo e novo: o nome como
 * se escreve no .fj; nota: uma frase curta com a forma nova; recusar: true = a forma antiga e E0110 (L3f); false = vale em silencio (L3c). */
typedef struct {
    RenClasse classe;
    const char *escopo;
    const char *antigo;
    const char *novo;
    const char *nota;
    bool recusar;
} Renomeado;

/* Quantas entradas a tabela tem. */
size_t RenomeadosContagem(void);

/* A entrada 'indice' (0 .. RenomeadosContagem() - 1); NULL fora da faixa. */
const Renomeado *RenomeadoPorIndice(size_t indice);

/* A 1a entrada da classe com esse nome antigo; NULL se nao ha. */
const Renomeado *RenomeadoBuscar(RenClasse classe, const char *antigo);

/* A 1a entrada da classe com esse nome novo; NULL se nao ha. */
const Renomeado *RenomeadoBuscarNovo(RenClasse classe, const char *novo);

/* A entrada da classe cujo nome (antigo OU novo) e 'nome' e cujo escopo tem a palavra 'palavra' (NULL = qualquer escopo); *era_antigo diz qual dos dois casou. NULL se nao ha. */
const Renomeado *RenomeadoAchar(RenClasse classe, const char *palavra, const char *nome, bool *era_antigo);

/* Como RenomeadoAchar, mas o escopo tem de ter todas as palavras da lista 'palavras', terminada em NULL (ex.: {"vendor:db/db", "DbError", NULL} para o membro de um enum de um modulo). */
const Renomeado *RenomeadoAcharEm(RenClasse classe, const char *const *palavras, const char *nome, bool *era_antigo);

/* O nome da classe como aparece na migracao.txt ("rotulo", "valor-alocador", ...); "?" fora da faixa. */
const char *RenomeadoClasseTexto(RenClasse classe);

/* L3f: a forma antiga 'r->antigo' foi escrita em [pos, pos + tam) e r->recusar e true: relata E0110 "forma antiga; rode forja migrar" com a troca pela forma nova (a sugestao
 * so leva edicao se o texto novo cabe em DIAG_NOVO_MAX). Devolve sempre false, para "return RenomeadoRecusar(...)". */
bool RenomeadoRecusar(Diag *d, uint32_t pos, uint32_t tam, const Renomeado *r);

/* L3d1: uma forma antiga usada no texto. off e tam: onde ela foi escrita (offset da Fonte, em bytes); antigo e novo apontam para a tabela ou para o Interner (valem enquanto a
 * compilacao vive). Quem quer a lista (o forja migrar) faz RenTrocasIniciar, poe o ponteiro em Diag.trocas e le itens (Vec de RenTroca) depois da analise. */
typedef struct { uint32_t off, tam; RenClasse classe; const char *antigo, *novo; } RenTroca;
typedef struct RenTrocas { Vec itens; } RenTrocas;

void RenTrocasIniciar(RenTrocas *t);
void RenTrocasLiberar(RenTrocas *t);

/* Registra a troca em d->trocas (nada se NULL). A mesma forma pode chegar mais de uma vez (a analise revisita expressoes): quem le a lista descarta as repetidas. */
void RenomeadoNotar(Diag *d, const RenTroca *troca);

/* Atalho: a forma antiga 'r->antigo' foi escrita em [pos, pos + tam) e vale (recusar false) ou vai ser recusada; registra a troca por r->novo. */
void RenomeadoNotarTabela(Diag *d, uint32_t pos, uint32_t tam, const Renomeado *r);

/* L3f: a forma antiga 'r' deve ser recusada (E0110)? So se r->recusar e o modo "tolerar forma antiga" esta desligado. */
bool RenomeadoRecusa(const Renomeado *r);

/* L3f: liga/desliga o modo interno "tolerar forma antiga": so o forja migrar o liga, para ler a forma antiga e gravar as trocas sem o E0110 (check, build e run nunca o ligam). */
void RenomeadoTolerarAntiga(bool ligado);

#endif
