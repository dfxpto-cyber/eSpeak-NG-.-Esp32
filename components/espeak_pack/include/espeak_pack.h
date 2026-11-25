/*
 * Pacote só de leitura com os dados do eSpeak NG (ver tools/make_espeak_pack.py).
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Prefixo de caminho com que o eSpeak NG é inicializado. */
#define ESPEAK_PACK_MOUNT "/espeak"

/* Valida e regista um pacote já em memória (flash mapeada ou ficheiro mmap).
 * Devolve 0 se ok. */
int espeak_pack_attach(const void *base, size_t size);

/* Procura um ficheiro pelo caminho relativo ("phondata", "lang/roa/pt").
 * Devolve ponteiro para os dados (alinhado a 16) ou NULL. */
const void *espeak_pack_find(const char *rel, int *size);

#if defined(ESP_PLATFORM)
#include "esp_err.h"
/* Mapeia a partição "espeak" e regista o sistema de ficheiros só de leitura
 * em ESPEAK_PACK_MOUNT. */
esp_err_t espeak_pack_mount(void);
#endif

#ifdef __cplusplus
}
#endif
