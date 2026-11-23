#ifndef FIM_H
#define FIM_H
/* 0.9.0 (M6d P92g): header de teste do --propor com 'ends': so x_shutdown e x_global_cleanup (zero parametros, nome da lista fechada) ganham a clausula; x_os_end nao. */
int x_init(void);
int x_shutdown(void);
void x_global_cleanup(void);
int x_os_end(void);
void x_shutdown_com(int motivo);
#endif
