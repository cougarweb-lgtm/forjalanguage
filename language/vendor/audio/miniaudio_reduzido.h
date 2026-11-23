/* Cabecalho REDUZIDO do miniaudio 0.11.26 para 'forja ligar': so o que um jogo usa (motor de som por arquivo).
   Por que reduzido: o miniaudio.h inteiro tem centenas de structs com ponteiro de funcao (G0003/G0019 em cascata)
   e ma_engine/ma_sound/ma_engine_config sao structs enormes. Aqui os tres viram tipos opacos (incompletos) e quem os
   cria e a cola C em miniaudio_impl.c (funcoes maf_*), que usa o sizeof REAL do miniaudio.h oficial.
   As funcoes ma_* abaixo tem a assinatura identica a do miniaudio.h (ma_result e um enum de int; aqui int).
   Valores conferidos por _Static_assert contra o miniaudio.h oficial (ver dependencia.json, "verificacao"). */
#include <stdint.h>

typedef int ma_result;
typedef uint32_t ma_uint32;
typedef uint32_t ma_bool32;

typedef struct ma_engine ma_engine;
typedef struct ma_sound ma_sound;
typedef struct ma_engine_config ma_engine_config;
typedef struct ma_sound_group ma_sound_group;
typedef struct ma_fence ma_fence;

#define MA_SUCCESS 0
#define MA_ERROR (-1)
#define MA_INVALID_ARGS (-2)
#define MA_INVALID_OPERATION (-3)
#define MA_OUT_OF_MEMORY (-4)
#define MA_DOES_NOT_EXIST (-7)
#define MA_INVALID_FILE (-10)
#define MA_NO_BACKEND (-203)
#define MA_NO_DEVICE (-204)

#define MA_SOUND_FLAG_STREAM 0x00000001
#define MA_SOUND_FLAG_DECODE 0x00000002
#define MA_SOUND_FLAG_ASYNC 0x00000004
#define MA_SOUND_FLAG_LOOPING 0x00000020
#define MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT 0x00001000
#define MA_SOUND_FLAG_NO_PITCH 0x00002000
#define MA_SOUND_FLAG_NO_SPATIALIZATION 0x00004000

/* ---- miniaudio.h oficial (mesma assinatura) ---- */
const char* ma_version_string(void);
void ma_version(ma_uint32* pMajor, ma_uint32* pMinor, ma_uint32* pRevision);
const char* ma_result_description(ma_result result);

ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine);
void ma_engine_uninit(ma_engine* pEngine);
ma_result ma_engine_read_pcm_frames(ma_engine* pEngine, void* pFramesOut, uint64_t frameCount, uint64_t* pFramesRead);
ma_uint32 ma_engine_get_channels(const ma_engine* pEngine);
ma_uint32 ma_engine_get_sample_rate(const ma_engine* pEngine);
ma_result ma_engine_start(ma_engine* pEngine);
ma_result ma_engine_set_volume(ma_engine* pEngine, float volume);
ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup);

ma_result ma_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pDoneFence, ma_sound* pSound);
void ma_sound_uninit(ma_sound* pSound);
ma_result ma_sound_start(ma_sound* pSound);
ma_result ma_sound_stop(ma_sound* pSound);
void ma_sound_set_volume(ma_sound* pSound, float volume);
void ma_sound_set_pan(ma_sound* pSound, float pan);
void ma_sound_set_pitch(ma_sound* pSound, float pitch);
void ma_sound_set_looping(ma_sound* pSound, ma_bool32 isLooping);
ma_bool32 ma_sound_is_playing(const ma_sound* pSound);
ma_bool32 ma_sound_at_end(const ma_sound* pSound);

/* ---- cola C (miniaudio_impl.c): aloca/libera os tipos opacos com o tamanho certo e monta o ma_engine_config ----
   maf_config_nova: malloc + ma_engine_config_init() (padroes do miniaudio); NULL se faltar memoria.
   maf_config_sem_dispositivo: noDevice (sem placa de som; o audio sai por ma_engine_read_pcm_frames).
   maf_config_formato: canais e taxa de amostragem (obrigatorios com noDevice; 0 = nativo do dispositivo). */
ma_engine_config* maf_config_nova(void);
void maf_config_liberar(ma_engine_config* config);
void maf_config_sem_dispositivo(ma_engine_config* config, ma_bool32 sem_dispositivo);
void maf_config_formato(ma_engine_config* config, ma_uint32 canais, ma_uint32 taxa);
ma_engine* maf_engine_nova(void);
void maf_engine_liberar(ma_engine* engine);
ma_sound* maf_sound_nova(void);
void maf_sound_liberar(ma_sound* som);
