#ifndef PROPOR_H
#define PROPOR_H
/* 0.8.0 (M6c P89): header de teste do --propor: open/close/create/destroy/parse/delete/free e um T** de saida. */
typedef struct Pr Pr;
Pr *pr_open(const char *caminho);
void pr_close(Pr *p);
Pr *pr_create(int n);
void pr_destroy(Pr *p);
Pr *pr_parse(const char *texto);
void pr_delete(Pr *p);
void pr_free(void *memoria);
int pr_init(Pr **saida, int flags);
int pr_open_ex(const char *caminho, Pr **saida);
char *pr_strdup(const char *texto);
int pr_close_id(int id);
int pr_open_dupla(Pr *rdb, Pr **db); /* 0.8.1: 'rdb' termina em 'db' e nao pode ser alterado por 'creates db' */
int pr_valor(Pr *p);
void pr_shutdown(void);
#endif
