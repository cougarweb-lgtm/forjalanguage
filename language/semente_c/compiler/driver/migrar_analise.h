/* migrar_analise.h - o miolo do 'forja migrar': analisa UM arquivo, junta as formas antigas da tabela de renomeados que o compilador resolveu (Diag.trocas) e decide quais trocas
 * valem (IA1-L3d1).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - S3 P4 (L11b-2): MigrarTroca.linha_fim (troca de linha inteira: sai a declaracao do foreign de libm, entra o import); MigrarResultado.motivo leva os avisos de libm que nao migram.
 *            0.2.0 - IA1-L3d2: --comentarios (MigrarEntrada.comentarios, MigrarTroca.comentario, MigrarRevisao, MigrarResultado.revisoes, MIGRAR_REVISAO_COMENTARIO).
 *            0.1.0 - IA1-L3d1: MigrarArquivo, MigrarResultado e MigrarTroca.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 *
 * A migracao e SEMANTICA: nada de regex. O front-end e a semantica rodam sobre o arquivo (modulos resolvidos como no check) e cada ponto que resolve uma forma antiga registra
 * posicao e texto antigo/novo; aqui as repeticoes saem, o que e de outro arquivo (modulo importado) tambem, e as trocas viram Edicao do motor de edicoes do 'forja corrigir'.
 * Salvaguarda: o texto migrado e analisado de novo e tem de compilar sem erro e sem nenhuma forma antiga. Import e qualificador andam juntos (std:arquivo -> std:file muda tambem
 * os usos arquivo. -> file.): se a releitura falha por causa de um grupo (o novo qualificador bate com um nome do arquivo, por exemplo), o grupo inteiro fica como esta e o estado
 * e 'revisao'.
 */
#ifndef FORJA_DRIVER_MIGRAR_ANALISE_H
#define FORJA_DRIVER_MIGRAR_ANALISE_H

#include "common/arena.h"
#include "common/buf.h"
#include "common/renomeados.h"

/* Uma troca que vale. linha e coluna a partir de 1, em BYTES; coluna_fim exclusiva (a unidade do diagnostico e das edicoes). */
typedef struct {
    uint32_t linha, coluna, coluna_fim;
    RenClasse classe;
    const char *antigo, *novo;
    uint32_t linha_fim;     /* S3 P4: 0 = a troca fica numa linha; > 0 = linha inteira (coluna = coluna_fim = 1 e a faixa vai ate o inicio de linha_fim) */
    int32_t grupo;          /* interno: indice do grupo import+qualificador a que a troca pertence; -1 = nenhum */
    bool comentario;        /* a troca esta dentro de um comentario (--comentarios) */
} MigrarTroca;

/* Um comentario que tem forma antiga e NAO parseia como Forja (prosa, mistura), ou um qualificador antigo duvidoso dentro de um comentario que parseia: so a lista, nunca editado.
 * linha e coluna (a partir de 1, em bytes) do '//' ou da forma duvidosa; trecho: o texto do comentario (cortado em 160 bytes); formas: o que foi achado, cada uma uma vez. */
typedef struct { uint32_t linha, coluna; const char *trecho; const char **formas; uint32_t nformas; } MigrarRevisao;

typedef enum { MIGRAR_SEM_TROCAS, MIGRAR_OK, MIGRAR_NAO_ANALISADO, MIGRAR_REVISAO, MIGRAR_REVISAO_COMENTARIO } MigrarEstado;

/* estado: sem_trocas = compila e nao usa forma antiga; ok = tem trocas e todas valem; nao_analisado = o arquivo tem erro (diagnostico = o codigo; nada e editado);
 * revisao = alguma troca ficou de fora (motivo diz qual e por que; diagnostico = o erro da releitura); revisao_comentario = nada a trocar e so ha comentarios para revisar a mao
 * (revisoes). trocas/ntrocas = as que valem, de codigo e de comentario (podem ser 0 em revisao).
 * novo: o arquivo inteiro migrado (BOM e quebras de linha do original; vazio se ntrocas == 0). Tudo vive na Arena de quem chamou, menos 'novo' (BufLiberar). */
typedef struct {
    MigrarEstado estado;
    const char *diagnostico, *motivo;
    MigrarTroca *trocas;
    uint32_t ntrocas;
    bool linhas_iguais;     /* nenhuma troca cria/remove linha nem mexe fora da linha da forma antiga */
    MigrarRevisao *revisoes;
    uint32_t nrevisoes;
    Buf novo;
} MigrarResultado;

/* O arquivo a migrar: 'original' (tam bytes, lido por inteiro, BOM incluso) que esta em 'caminho' (os imports saem da pasta dele). comentarios: tambem os comentarios (--comentarios). */
typedef struct { const char *caminho, *original; size_t tam; bool comentarios; } MigrarEntrada;

void MigrarArquivo(const MigrarEntrada *entrada, Arena *arena, MigrarResultado *r);

/* O nome do estado como sai no JSON: sem_trocas, ok, nao_analisado, revisao. */
const char *MigrarEstadoTexto(MigrarEstado estado);

#endif
