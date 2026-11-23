/* Gerado pela Forja (emit_c 0.12.0). Prototipos dos 'pub export proc'. */
struct fj_Grifo;
struct fj_Vec3 { float f_x; float f_y; float f_z; };
struct fj_Par { int32_t f_a; int32_t f_b; };
void incrementar(int32_t * a0);
float soma_vec(struct fj_Vec3 a0);
struct fj_Vec3 escala(struct fj_Vec3 a0, float a1);
struct fj_Par criar_par(int32_t a0, int32_t a1);
int64_t tamanho(const char * a0);
int32_t ler_opcional(int32_t * a0);
void ferir(struct fj_Grifo * a0, int32_t a1);
void * id_bruto(void * a0);
