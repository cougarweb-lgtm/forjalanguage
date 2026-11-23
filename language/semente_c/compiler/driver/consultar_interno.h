/* consultar_interno.h - estado e funcoes compartilhadas pelos arquivos de 'forja consultar' (cmd_consultar.c, consultar_json.c, consultar_decl.c, consultar_refs.c,
 * consultar_pos.c, consultar_escopo.c, consultar_grafo.c, consultar_modulos.c e consultar_api.c; o cmd_mapa.c e o mapa_resumo.c leem as mesmas funcoes).
 * Versao: 0.5.0
 * Changelog: 0.5.0 - Linguagem v1.13 L1b: ConsultaQtdArquivos, ConsultaTrechoDoOffset, ConsultaInicioDoArquivo, ConsultaFimDoArquivo, ConsultaModuloDoOffset, ConsultaChaveArquivoSePacote, ConsultaChaveArquivos, ConsultaBytesDoModulo, ConsultaItemCaminho.
 *            0.4.0 - IA1-I5: ConsultaInicioComentario (o 'forja editar' leva o comentario colado junto com a declaracao).
 *            0.3.0 - IA1-I4: PedidoConsulta.contexto; AlvoRefs, ListaAlvos, ConsultaAlvosDe, ConsultaUsoEhDoAlvo e ConsultaPosDoAlvo saem do consultar_refs.c para o 'forja editar' reusar o resolvedor.
 *            0.2.0 - IA1-I3: PedidoConsulta ganha resumo/limite (mapa); ConsultaExecutar e ConsultaEntradaDe saem do cmd_consultar.c/consultar_decl.c para o cmd_mapa.c reusar; as 6 perguntas novas
 *            (callers, callees, imports, api, testes, entrada) e os trechos de JSON comuns ao 'forja mapa' (modulo, item da API, teste, entrada).
 *            0.1.0 - IA1-I2: primeira versao.
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#ifndef FORJA_CONSULTAR_INTERNO_H
#define FORJA_CONSULTAR_INTERNO_H

#include "common/buf.h"
#include "driver/compilacao.h"
#include "frontend/ast_fim.h"
#include "semantic/indice.h"

enum { CONSULTAR_OK = 0, CONSULTAR_DIAGNOSTICO = 1, CONSULTAR_USO = 2, CONSULTAR_INTERNO = 3 };

#define CONSULTAR_VERSAO "1.1.0"

/* O que o usuario pediu: arg = o argumento da pergunta (nome ou arq:L:C), alvo = arquivo .fj ou pasta do projeto, modulo = filtro de 'simbolos' e do 'mapa';
 * resumo/limite so no 'mapa' (limite 0 = o padrao de cada modo); contexto = dado de quem chama ConsultaExecutar para o seu 'responder' (o 'forja editar'; NULL nas perguntas). */
typedef struct { const char *pergunta, *arg, *alvo, *modulo; bool resumo; uint32_t limite; void *contexto; } PedidoConsulta;

/* Uma consulta em andamento: a compilacao analisada, o indice dos usos, as posicoes finais e a resposta JSON em escrita. */
typedef struct {
    Compilacao *c;
    Indice indice;
    AstFim fim;
    Buf saida;
    bool primeiro; /* o proximo elemento e o primeiro do objeto ou da lista aberta (sem virgula antes) */
} Consulta;

/* ---- cmd_consultar.c: compila o projeto do pedido (alvo ou o arquivo da posicao), monta o indice e chama 'responder'; a resposta sai em stdout se devolver CONSULTAR_OK. ---- */
typedef int (*FuncaoResposta)(Consulta *q, const PedidoConsulta *p);
int ConsultaExecutar(const PedidoConsulta *p, FuncaoResposta responder);

/* ---- consultar_json.c: escrita da resposta e utilitarios. Todo ConsultaChave* separa por ", " sozinho (o primeiro de cada objeto ou lista nao leva virgula). ---- */
void ConsultaAbrir(Consulta *q, const char *pergunta); /* {"consultar_versao": "1.0.0", "pergunta": "<pergunta>" */
void ConsultaFechar(Consulta *q);                      /* }\n */
void ConsultaAbrirObj(Consulta *q);                    /* { (elemento de lista) */
void ConsultaFecharObj(Consulta *q);
void ConsultaAbrirLista(Consulta *q, const char *chave); /* "chave": [ */
void ConsultaFecharLista(Consulta *q);
void ConsultaChaveTexto(Consulta *q, const char *chave, const char *texto, size_t n);
void ConsultaChaveNome(Consulta *q, const char *chave, int32_t nome); /* nome do Interner; null se < 0 */
void ConsultaChaveTipo(Consulta *q, const char *chave, TipoId tipo);  /* texto do tipo; null se nao resolvido */
void ConsultaChaveBool(Consulta *q, const char *chave, bool valor);
void ConsultaChaveInt(Consulta *q, const char *chave, int64_t valor);
void ConsultaChavePos(Consulta *q, const char *chave, uint32_t off); /* "chave": [linha, coluna] locais ao arquivo do modulo */
void ConsultaChaveArquivo(Consulta *q, uint32_t off);                /* "arquivo": caminho do arquivo que contem o offset */
void ConsultaChaveId(Consulta *q, const char *chave, const Decl *d); /* "chave": "<modulo>:<nome>" (o id do AST 1.0) */
void ConsultaChaveModulo(Consulta *q, const char *chave, uint8_t modulo);
void ConsultaItemTexto(Consulta *q, const char *texto, size_t n);  /* elemento de lista: so um texto */
void ConsultaItemCaminho(Consulta *q, const char *caminho);        /* elemento de lista: um caminho de arquivo (barras normais) */
void ConsultaItemModulo(Consulta *q, uint8_t modulo);              /* elemento de lista: o caminho do modulo como texto */
void ConsultaChaveTrecho(Consulta *q, const char *chave, uint32_t ini, uint32_t fim); /* "chave": texto do fonte em [ini, fim), no maximo 160 bytes */
int ConsultaUsoErrado(const char *pergunta, const char *argumento, const char *motivo); /* stderr: "'argumento' motivo"; devolve CONSULTAR_USO */

/* Declaracao global pelo nome ("soma") ou pelo id do AST 1.0 ("<caminho do modulo>:soma"); NULL se nao ha ou se o modulo dito nao e o da declaracao. */
const Decl *ConsultaAcharDecl(Consulta *q, const char *texto);

/* Offset do inicio de uma declaracao, com o 'pub'/'export' que a precede. */
uint32_t ConsultaInicioDecl(const Consulta *q, const Decl *d);

/* proc, foreign, struct, enum, union, const ou unit: o que 'simbolos' lista (test, import, link e static_assert nao). */
bool ConsultaEhSimbolo(const Decl *d);

/* Linha de assinatura legivel: "pub proc soma(a: i32, b: i32 = 2) -> i32 ! Erro". */
void ConsultaAssinaturaTexto(Consulta *q, const Decl *d, Buf *saida);

/* "main" (proc da raiz sem corpo externo) ou "export" (pub export proc, vira simbolo da DLL); NULL nos demais. */
const char *ConsultaEntradaDe(const Consulta *q, const Decl *d);

/* ---- as perguntas; cada uma devolve CONSULTAR_OK ou CONSULTAR_USO (a resposta fica em q->saida) ---- */
int ConsultarSimbolos(Consulta *q, const PedidoConsulta *p);
int ConsultarAssinatura(Consulta *q, const PedidoConsulta *p);
int ConsultarCampos(Consulta *q, const PedidoConsulta *p);
int ConsultarRefs(Consulta *q, const PedidoConsulta *p);
int ConsultarEscopo(Consulta *q, const PedidoConsulta *p);
int ConsultarTipo(Consulta *q, const PedidoConsulta *p);
int ConsultarCallers(Consulta *q, const PedidoConsulta *p); /* consultar_grafo.c */
int ConsultarCallees(Consulta *q, const PedidoConsulta *p); /* consultar_grafo.c */
int ConsultarImports(Consulta *q, const PedidoConsulta *p); /* consultar_modulos.c */
int ConsultarApi(Consulta *q, const PedidoConsulta *p);     /* consultar_api.c */
int ConsultarTestes(Consulta *q, const PedidoConsulta *p);  /* consultar_api.c */
int ConsultarEntrada(Consulta *q, const PedidoConsulta *p); /* consultar_api.c */

/* ---- consultar_modulos.c: a tabela de modulos do projeto ---- */
const char *ConsultaOrigemDoModulo(const Consulta *q, uint8_t modulo); /* "raiz", "arquivo", "std", "vendor" ou "engine" */
uint32_t ConsultaOffsetDoModulo(const Consulta *q, uint8_t modulo);    /* offset do 1o byte do texto do modulo */
uint64_t ConsultaLinhasDoModulo(const Consulta *q, uint8_t modulo);    /* soma das linhas de todos os arquivos do modulo */
/* v1.13 L1b: modulo de pasta = varios trechos (arquivos) da Fonte; o offset de uma declaracao diz o arquivo dela. */
uint64_t ConsultaBytesDoModulo(const Consulta *q, uint8_t modulo);     /* soma dos bytes dos arquivos do modulo */
uint32_t ConsultaQtdArquivos(const Consulta *q, uint8_t modulo);       /* quantos arquivos tem o modulo (1 num modulo de arquivo so) */
uint32_t ConsultaTrechoDoOffset(const Consulta *q, uint32_t off);      /* indice do trecho (arquivo) da Fonte que contem o offset */
uint32_t ConsultaInicioDoArquivo(const Consulta *q, uint32_t off);     /* offset do 1o byte do arquivo que contem off */
uint32_t ConsultaFimDoArquivo(const Consulta *q, uint32_t off);        /* offset logo apos o ultimo byte do arquivo que contem off */
uint8_t ConsultaModuloDoOffset(const Consulta *q, uint32_t off);       /* o modulo (pacote) dono do arquivo que contem off */
void ConsultaChaveArquivoSePacote(Consulta *q, uint32_t off);          /* "arquivo" do offset, so se o modulo dele tem 2+ arquivos (a saida de um programa sem pasta nao muda) */
void ConsultaChaveArquivos(Consulta *q, uint8_t modulo);               /* "arquivos": [caminhos], so num modulo de 2+ arquivos */
int ConsultaOrdemDosModulos(const Consulta *q, uint8_t *ordem);        /* raiz primeiro, depois as dependencias na ordem do programa; devolve quantos */
int ConsultaAcharModulo(const Consulta *q, const char *caminho);       /* indice do modulo pelo caminho do import ("std:mem", "jogo/fisica", "main"); -1 se nao ha */
void ConsultaModuloJson(Consulta *q, uint8_t modulo);                  /* objeto: modulo, origem, arquivo, linhas, importa, importado_por, links */
void ConsultaLinksJson(Consulta *q, const char *chave, int modulo);    /* lista de objetos {lib, modulo, pos} dos 'link'; modulo < 0 = o projeto todo */

/* ---- consultar_api.c: declaracoes publicas, testes e entradas (a 'forja mapa' escreve estas mesmas linhas) ---- */
void ConsultaComentarioAcima(const Consulta *q, const Decl *d, Buf *saida); /* o bloco de '//' colado acima da declaracao (linhas separadas por LF); vazio = sem comentario */
uint32_t ConsultaInicioComentario(const Consulta *q, const Decl *d);        /* offset do comeco da 1a linha do bloco de '//' colado acima; o comeco da linha da declaracao se nao ha */
bool ConsultaEhApi(const Decl *d);                       /* declaracao publica que 'simbolos' lista */
void ConsultaApiItemJson(Consulta *q, const Decl *d);      /* {nome, categoria, id, exportado, pos, assinatura, comentario} */
int ConsultaApiDoModuloJson(Consulta *q, uint8_t modulo);  /* "api": [itens] do modulo; devolve quantos itens */
bool ConsultaEhTesteDoProjeto(const Consulta *q, const Decl *d); /* test de modulo que nao e o std embutido */
void ConsultaTesteJson(Consulta *q, const Decl *d);
void ConsultaEntradaJson(Consulta *q, const Decl *d, const char *entrada);
void ConsultaAssinaturaDaApi(Consulta *q, const Decl *d, Buf *saida); /* proc = assinatura; const = "const N: T"; o resto = "<categoria> <nome>" */

/* ---- consultar_refs.c: os alvos de um nome e os usos de cada um (o 'forja editar renomear' usa os mesmos) ---- */
typedef struct {
    IndAlvo tipo;
    const Decl *decl; /* a declaracao global (dono do campo/membro/parametro/local); NULL no embutido */
    int32_t indice;   /* campo, membro ou parametro; -1 nos demais */
    int32_t nome;     /* nome do alvo (Interner) */
    uint32_t decl_pos; /* offset que declara o local/parametro; AST_REF_SEM_POS nos demais */
} AlvoRefs;
typedef struct { AlvoRefs itens[64]; uint32_t n; } ListaAlvos;
/* Os alvos que o argumento de 'refs' designa (nome, "<modulo>:<nome>", Tipo.campo, Enum.membro, proc.parametro/local, embutido); n == 0 = nao e declaracao do projeto. */
void ConsultaAlvosDe(Consulta *q, const char *arg, ListaAlvos *alvos);
bool ConsultaUsoEhDoAlvo(const IndUso *u, const AlvoRefs *a);
/* Offset onde o alvo e declarado (nome da declaracao, campo, membro, parametro; o 'let' do local); AST_REF_SEM_POS no embutido. */
uint32_t ConsultaPosDoAlvo(Consulta *q, const AlvoRefs *a);

/* ---- consultar_pos.c: arq:L:C -> offset global ---- */
typedef struct { uint32_t off, linha, coluna; const char *arquivo; } PosicaoPedida;
/* Le "arq:L:C" e acha o offset no fonte do projeto. false = mensagem no stderr (uso errado). */
bool ConsultaLerPosicao(Consulta *q, const char *texto, PosicaoPedida *pos);
/* Arquivo da posicao "arq:L:C" (antes do analise do projeto): copia o trecho antes dos dois ultimos ':' em destino. false se nao tem a forma. */
bool ConsultaArquivoDaPosicao(const char *texto, char *destino, size_t tam);
/* A declaracao (de qualquer categoria, menos import e link) que contem o offset, ou NULL. */
const Decl *ConsultaDeclEm(const Consulta *q, uint32_t off);

#endif
