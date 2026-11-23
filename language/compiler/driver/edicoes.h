/* edicoes.h - motor de edicao de texto do driver: aplica uma lista de edicoes {linha, coluna, coluna_fim, antigo, novo} a um texto em memoria, valida cada uma e
 * gera o diff unificado (IA1-b05, D-IA1-22: o 'forja corrigir' usa; o 'forja editar' do passo I4 reaproveita o mesmo nucleo).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-I4: Edicao.linha_fim (faixa de varias linhas; 0 = uma linha so, como era; o diff e o JSON so mudam quando ha linha_fim); o resto do nucleo do 'forja corrigir' veio do cmd_corrigir.c (comportamento igual) para o 'forja editar' reusar: EdicoesTemBom, EdicoesMontarArquivo
 *            (BOM + aplicar), EdicoesDesfazer (devolve o arquivo ao original, conferido byte a byte) e a escrita JSON (EdicoesJsonTexto, EdicoesJsonTextoOuNulo, EdicoesJsonEdicao).
 *            0.1.0 - IA1-b05: Edicao, EdicoesAplicar, EdicoesDiff, EdicoesConflitam, EdicoesIguais, EdicoesLinhaNova e EdicaoStatusTexto.
 * Verificacao: construir.bat && teste_common.exe
 *
 * Posicao: linha a partir de 1; coluna e coluna_fim em BYTES a partir de 1, faixa [coluna, coluna_fim) dentro de UMA linha (ou, com linha_fim, de (linha, coluna) ate
 * (linha_fim, coluna_fim)), sem contar o '\r' do fim de linha CRLF (a mesma unidade do diagnostico do forja). Inserir = coluna == coluna_fim e antigo "". O BOM UTF-8 do inicio nao faz parte do texto: quem le o arquivo o tira antes
 * e o devolve depois. O motor trabalha de frente para tras sobre a lista ordenada por (linha, coluna), o que e equivalente a aplicar de tras para frente e deixa a
 * validacao de sobreposicao em uma comparacao entre vizinhas.
 */
#ifndef FORJA_DRIVER_EDICOES_H
#define FORJA_DRIVER_EDICOES_H

#include "common/buf.h"

typedef struct {
    uint32_t linha, coluna, coluna_fim;
    const char *antigo;     /* o texto que tem de estar na faixa (confere byte a byte) */
    const char *novo;       /* o que entra no lugar */
    uint32_t linha_fim;     /* 0 (ou <= linha) = a faixa fica na linha 'linha'; maior = a faixa vai ate (linha_fim, coluna_fim) e leva os fins de linha do meio (IA1-I4) */
} Edicao;

typedef enum {
    EDICAO_OK,
    EDICAO_SEM_LINHA,       /* a linha nao existe no texto */
    EDICAO_FAIXA_INVALIDA,  /* coluna 0, fim antes do inicio, ou faixa fora do texto da linha */
    EDICAO_ANTIGO_DIFERE,   /* o texto da faixa nao e o 'antigo' da edicao */
    EDICAO_SOBREPOSTA       /* duas edicoes tocam o mesmo trecho (ou inserem no mesmo ponto) */
} EdicaoStatus;

/* Frase curta em PT-BR para o status (mensagem de erro). */
const char *EdicaoStatusTexto(EdicaoStatus status);

/* Aplica as n edicoes de 'lista' (qualquer ordem) a texto[0..tam) e ANEXA o resultado a *saida (saida vazia = texto novo). Se alguma edicao e invalida, nada e anexado,
 * devolve o motivo e *indice_ruim (se nao NULL) recebe a posicao dela em 'lista'. n == 0 anexa o texto igual. */
EdicaoStatus EdicoesAplicar(const char *texto, size_t tam, const Edicao *lista, uint32_t n, Buf *saida, uint32_t *indice_ruim);

/* Diff unificado (3 linhas de contexto, "--- a/<nome>" e "+++ b/<nome>") do texto contra o texto com as edicoes aplicadas, ANEXADO a *saida. Sem edicoes, nada e
 * anexado. As mesmas validacoes do EdicoesAplicar; invalida = nada anexado. */
EdicaoStatus EdicoesDiff(const char *texto, size_t tam, const Edicao *lista, uint32_t n, const char *nome, Buf *saida);

/* As duas editam o mesmo trecho da mesma linha, ou inserem no mesmo ponto (aplicar as duas seria ambiguo). */
bool EdicoesConflitam(const Edicao *a, const Edicao *b);

/* Mesma posicao, mesmo 'antigo' e mesmo 'novo' (duas sugestoes que pedem a mesma coisa). */
bool EdicoesIguais(const Edicao *a, const Edicao *b);

/* Numero que a linha 'linha' do texto original passa a ter no texto novo (edicoes cujo 'novo' tem '\n' empurram as linhas de baixo). */
uint32_t EdicoesLinhaNova(const Edicao *lista, uint32_t n, uint32_t linha);

enum { EDICOES_BOM_TAM = 3 };

/* O texto comeca com o BOM UTF-8 (EF BB BF)? */
bool EdicoesTemBom(const char *dados, size_t tam);

/* ANEXA a *novo o conteudo de um arquivo lido por inteiro ('original', com BOM se tinha) depois de aplicar as edicoes (posicoes sem contar o BOM, como no EdicoesAplicar);
 * o BOM do original vai junto. false = as edicoes nao se aplicam (nada garantido em *novo alem do BOM). */
bool EdicoesMontarArquivo(const char *original, size_t tam, const Edicao *lista, uint32_t n, Buf *novo);

/* Regrava 'arquivo' com o conteudo 'original' (gravacao atomica) e confere byte a byte. false = nao conseguiu (situacao grave: o arquivo pode ficar diferente do original). */
bool EdicoesDesfazer(const char *arquivo, const char *original, size_t tam);

/* JSON: texto entre aspas ('\n', '\r' e '\t' em uma letra so, os outros controles como \u00xx; o diff fica legivel e curto), "null" para texto NULL ou vazio, e o objeto de uma edicao
 * {"arquivo", "linha", "coluna", "coluna_fim", "antigo", "novo"} com o caminho de 'arquivo' em barras normais. */
void EdicoesJsonTexto(Buf *b, const char *texto, size_t n);
void EdicoesJsonTextoOuNulo(Buf *b, const char *texto);
void EdicoesJsonEdicao(Buf *b, const Edicao *e, const char *arquivo);

#endif
