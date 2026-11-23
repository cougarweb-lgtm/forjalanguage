/* ar_ler.h - leitor minimo de arquivo ar (.lib do MSVC e .a/.dll.a do GNU): percorre os membros e resolve o nome de cada um (P19b do M3).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M5b P35: ArEhElf (o membro e ELF pela magica 7f 'E' 'L' 'F'; o resto segue COFF, como antes) e ArRelatarMembroElf (erro do leitor ELF com o membro:
 *                    X0110 para relocacao fora do censo, com o tipo; X0105 para o resto).
 *            0.2.0 - M3b P23d: ArMembro.off (offset do cabecalho do membro) e ArLerIndice: 1o linker member GNU/MS (big-endian) e 2o linker member MS (little-endian), que vence o 1o; ar sem indice = X0105.
 *            0.1.0 - M3 P19b: ArIniciar e ArProximo. Assinatura "!<arch>\n", cabecalho de 60 bytes por membro, nomes "nome/", "/" (indice de simbolos, nao lido aqui),
 *                    "//" (tabela de nomes longos) e "/<n>" (nome longo no offset n da tabela). O indice de simbolos e a selecao de membros sao do P23d.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_AR_LER_H
#define FORJA_NATIVO_OBJ_AR_LER_H

#include <stddef.h>
#include <stdint.h>

#include "common/diag.h"
#include "common/vec.h"
#include "nativo/obj/elf_ler.h"

/* AR_OBJETO = membro comum (objeto COFF ou membro curto de import); AR_INDICE = "/" (um ou dois, o MSVC grava dois); AR_NOMES = "//". */
typedef enum { AR_OBJETO, AR_INDICE, AR_NOMES } ArTipo;

/* nome nao termina em NUL: sao nome_tam bytes dentro dos bytes lidos (vazio nos membros AR_INDICE e AR_NOMES). dados aponta para dentro dos bytes lidos. */
typedef struct {
    ArTipo tipo;
    const char *nome;
    size_t nome_tam;
    const uint8_t *dados;
    size_t tam;
    size_t off;  /* offset do cabecalho de 60 bytes do membro dentro dos bytes lidos: e o que o indice de simbolos guarda */
} ArMembro;

/* Uma entrada do indice de simbolos: o simbolo e o offset do cabecalho do membro que o define. nome aponta para dentro dos bytes lidos e termina em NUL. */
typedef struct {
    const char *nome;
    size_t nome_tam;
    uint32_t membro_off;
} ArSimbolo;

typedef struct {
    const uint8_t *bytes;
    size_t n;
    size_t pos;
    const uint8_t *nomes_longos;
    size_t nomes_tam;
} ArIterador;

/* Confere a assinatura "!<arch>\n". Falso com X0105 em erro se nao for ar. */
bool ArIniciar(ArIterador *it, const uint8_t *bytes, size_t n, Diag *erro);

/* Proximo membro. Falso no fim do arquivo (erro->tem continua falso) ou num ar malformado (cabecalho truncado, tamanho que passa do arquivo, "/<n>" sem a
 * tabela "//": X0105 em erro). Quem chama confere erro->tem depois do laco. */
bool ArProximo(ArIterador *it, ArMembro *membro, Diag *erro);

/* Indice de simbolos: le o 1o linker member (GNU e MS, inteiros big-endian) e, se houver um 2o "/" (MS, little-endian, mais seguro), le o 2o no lugar. simbolos e
 * iniciado aqui (Vec de ArSimbolo, na ordem do indice); libere com VecLiberar mesmo quando devolve false. X0105 em erro: ar sem indice (falta "ar s"/ranlib), "/SYM64/"
 * (offsets de 64 bits), indice truncado ou com numero de membro fora da tabela. Quem usa confere que membro_off e um membro de verdade. */
bool ArLerIndice(const uint8_t *bytes, size_t n, Vec *simbolos, Diag *erro);

/* Membro ELF (mesma magica do ELF: 7f 'E' 'L' 'F') ou COFF (qualquer outra coisa segue o leitor COFF, como antes do P35). */
bool ArEhElf(const uint8_t *dados, size_t n);

/* Relata em erro o erro do leitor ELF de um membro (e.codigo): relocacao fora do censo = X0110 com o tipo; formato = X0105. membro so entra na mensagem. */
bool ArRelatarMembroElf(Diag *erro, const char *membro, const ElfLErro *e);

#endif
