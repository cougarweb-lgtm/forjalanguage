/* Wrapper de build do miniaudio 0.11.26 (dominio publico OU MIT-0; usada MIT-0). Compilar: ver dependencia.json.
   Desligados (nao usados por um jogo): codificacao (gravar wav/etc.) e geracao (ruido/onda/pulso).
   Mantidos: decodificadores wav/flac/mp3, engine, resource manager, node graph, backends do Windows
   (WASAPI/DirectSound/WinMM, carregados em tempo de execucao por LoadLibrary: o .exe nao importa ole32/winmm).
   Sem Vorbis/ogg: o Odin liga o stb_vorbis a parte; aqui ele ainda nao esta no catalogo. */
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#include <stdlib.h>

/* ---- cola para a Forja (declarada em miniaudio_reduzido.h) ----
   Os tipos ma_engine, ma_sound e ma_engine_config sao grandes e a ponte so os ve como opacos: quem aloca e libera
   e este arquivo, dentro da mesma lib (mesmo malloc/free), com o sizeof REAL. calloc: o ma_*_init zera de novo. */
ma_engine_config* maf_config_nova(void) {
    ma_engine_config* config = (ma_engine_config*)calloc(1, sizeof(ma_engine_config));
    if (config != NULL) {
        *config = ma_engine_config_init();
    }
    return config;
}

void maf_config_liberar(ma_engine_config* config) {
    free(config);
}

void maf_config_sem_dispositivo(ma_engine_config* config, ma_bool32 sem_dispositivo) {
    config->noDevice = sem_dispositivo;
}

void maf_config_formato(ma_engine_config* config, ma_uint32 canais, ma_uint32 taxa) {
    config->channels = canais;
    config->sampleRate = taxa;
}

ma_engine* maf_engine_nova(void) {
    return (ma_engine*)calloc(1, sizeof(ma_engine));
}

void maf_engine_liberar(ma_engine* engine) {
    free(engine);
}

ma_sound* maf_sound_nova(void) {
    return (ma_sound*)calloc(1, sizeof(ma_sound));
}

void maf_sound_liberar(ma_sound* som) {
    free(som);
}
