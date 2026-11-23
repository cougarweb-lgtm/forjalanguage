/* modulos.h - tabela de modulos de um programa Forja: caminhos de import, apelidos por arquivo e a std embutida.
 * Versao: 0.9.0
 * Changelog: 0.9.0 - Linguagem v1.13 L1b: ModulosTrechosDoPacote.
 *            0.8.0 - Linguagem v1.13 L1a (modulo de pasta): ApelidoDeArquivo.arquivo e RegistroImport.arquivo viram o indice do trecho da Fonte (uint32_t); Modulos.pacotes, ModulosRegistrarArquivo, ModulosPacoteDoArquivo, ModulosApelidoNoPacote.
 *            0.7.0 - M6b P79: Modulos.pasta (pasta do arquivo raiz) e ModulosListarDisponiveis / ModulosParecido (E1001 com a lista completa e "quis dizer"); ModulosStdListar sai, troca por ModulosStdNomes.
 *            0.6.0 - S4 P2: ModulosTextoStdTestes (pedaco testes.fj da std, so math) e ModulosTextoStdComTestes (o texto do modulo com o pedaco no fim).
 *            0.5.0 - IA1-L9b: ModulosTextoStdAlvo.
 *            0.4.0 - IA1-L3d1: ApelidoDeArquivo.novo e ModulosApelidoNovo (o forja migrar troca o qualificador junto com o import).
 *            0.3.0 - Linguagem v1.8b P6: ModuloOrigem ganha MOD_VENDOR e MOD_ENGINE.
 *            0.2.0 - Linguagem v1.8 P2b: ModulosConferirCiclos (E1002) e ModulosOrdenarDecls.
 *            0.1.0 -Linguagem v1.8 P2a: Modulos, ModulosIniciar/Registrar/Apelido/Liberar e a tabela STD (ModulosStd*).
 * Verificacao: construir.bat && python ..\tests\sintaxe\verificar_sintaxe.py
 */
#ifndef FORJA_MODULOS_H
#define FORJA_MODULOS_H

#include "common/arena.h"
#include "common/diag.h"
#include "common/intern.h"
#include "common/vec.h"
#include "frontend/ast.h"

#define MODULOS_MAX 255u /* 0 = raiz; 1..254 = importados */

typedef enum { MOD_RAIZ, MOD_ARQUIVO, MOD_STD, MOD_VENDOR, MOD_ENGINE } ModuloOrigem;

/* caminho = texto do import internado (raiz: nome-base do arquivo sem pasta e sem ".fj"); apelido = ultimo segmento;
 * pos_import = offset do texto no 1o import que trouxe o modulo; importador = modulo desse import. */
typedef struct { int32_t caminho, apelido; uint8_t origem, importador; uint32_t pos_import; } Modulo;

/* novo (L3d1): o apelido que o import teria escrito na forma nova (so no apelido padrao de um import de forma antiga cujo ultimo segmento muda); -1 = nao muda. */
typedef struct { uint32_t arquivo; uint8_t modulo; int32_t nome, novo; } ApelidoDeArquivo; /* arquivo = indice do trecho da Fonte (modulo de pasta: varios por modulo) */

typedef struct {
    Modulo itens[MODULOS_MAX];
    uint32_t n;
    Vec apelidos; /* ApelidoDeArquivo */
    Vec pacotes;  /* uint8_t por trecho da Fonte: o modulo (pacote) dono do arquivo; trecho fora da tabela (a raiz, trecho 0) = 0 */
    Interner *nomes;
    char pasta[300]; /* P79: pasta do arquivo raiz ("." se o caminho nao tem pasta); o vendor\ dela entra no E1001 */
    uint8_t ordem[MODULOS_MAX]; /* pos-ordem das dependencias (preenchida pela carga) */
} Modulos;

/* Um import lido pelo parser: texto/apelido internados (apelido -1 = padrao), arquivo = trecho da Fonte que escreveu o import. */
typedef struct { int32_t texto, apelido; uint32_t arquivo, pos_texto, pos_apelido; } RegistroImport;

/* n = 1; itens[0] = raiz (nome-base de caminho_raiz). */
void ModulosIniciar(Modulos *m, Interner *nomes, const char *caminho_raiz);

/* Valida a forma do caminho (E1001), acha ou cria o modulo e registra o apelido no arquivo. false = diagnostico em d. */
bool ModulosRegistrar(Modulos *m, const RegistroImport *r, Diag *d, uint8_t *indice);

/* Modulo do apelido 'nome' no arquivo (trecho da Fonte); 0 = nao e apelido. */
uint8_t ModulosApelido(const Modulos *m, uint32_t arquivo, int32_t nome);

/* v1.13: 'nome' e apelido de import em algum arquivo do pacote (modulo)? */
bool ModulosApelidoNoPacote(const Modulos *m, uint8_t pacote, int32_t nome);

/* v1.13: o arquivo (trecho da Fonte) pertence ao pacote (modulo); a carga registra cada arquivo depois de anexa-lo. */
void ModulosRegistrarArquivo(Modulos *m, uint32_t arquivo, uint8_t pacote);
uint8_t ModulosPacoteDoArquivo(const Modulos *m, uint32_t arquivo);

/* v1.13 L1b: os trechos (arquivos) do pacote: devolve quantos e grava em *primeiro o indice do 1o (os do mesmo pacote sao contiguos). 'ntrechos' = quantos a Fonte tem; 0 (programa de
 * um arquivo so, sem tabela de trechos) = o pacote 0 tem 1 trecho, o 0. Pacote sem trecho: 0. */
uint32_t ModulosTrechosDoPacote(const Modulos *m, uint32_t ntrechos, uint8_t pacote, uint32_t *primeiro);

/* L3d1: o apelido da forma nova do apelido padrao 'nome' do arquivo (ApelidoDeArquivo.novo); -1 = nao muda ou nao e apelido. */
int32_t ModulosApelidoNovo(const Modulos *m, uint32_t arquivo, int32_t nome);

/* DFS a partir da raiz sobre os D_IMPORT (Decl.modulo -> Decl.indice), na ordem da lista de Decl. Ciclo = E1002 no texto do
 * import que o fecha. Sem ciclo, preenche m->ordem em pos-ordem (folhas primeiro, raiz por ultimo). */
bool ModulosConferirCiclos(Modulos *m, const Programa *p, Diag *d);

/* Reencadeia a lista de Decl por modulo na ordem de m->ordem (ordem relativa dentro de cada modulo intacta). So com n > 1. */
void ModulosOrdenarDecls(const Modulos *m, Programa *p);

void ModulosLiberar(Modulos *m);

/* ---- std embutida (modulos_std.c) ---- */
bool ModulosStdTem(const char *nome, uint32_t tam);
size_t ModulosStdNomes(const char **nomes, size_t cap); /* os nomes da std ("mem", "os", ...) em nomes; devolve quantos */

/* ---- lista de modulos disponiveis (modulos_lista.c, P79): std embutida + vendor\ da pasta do arquivo raiz ---- */
/* "std: a, b; vendor: x/y" em destino (ordem alfabetica, sem o nome que nao cabe: termina com " (+N)"). Devolve o tamanho. */
size_t ModulosListarDisponiveis(const char *pasta, char *destino, size_t cap);
/* O modulo parecido com 'import' (ex.: "std:texto" -> "std:text") em destino: sinonimo PT/EN ou Levenshtein <= 2. false = nenhum. */
bool ModulosParecido(const char *pasta, const char *import, char *destino, size_t cap);
const char *ModulosTextoStd(const char *nome, size_t *tam); /* NULL = nao existe (ou ainda nao embutido) */
const char *ModulosTextoStdAlvo(const char *nome, bool alvo_linux, Arena *arena, size_t *tam); /* o mesmo texto; std:os no alvo linux traz TARGET = "x86_64-linux" (copia na arena) */
const char *ModulosTextoStdTestes(const char *nome, size_t *tam); /* S4 P2: o testes.fj da std (NULL = o modulo nao tem); so forja test <std:modulo> o usa */
const char *ModulosTextoStdComTestes(const char *nome, const char *texto, Arena *arena, size_t *tam); /* texto do modulo + '\n' + testes (copia na arena); sem testes, o mesmo texto */

#endif
