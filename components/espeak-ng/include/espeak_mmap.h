/*
 * Gancho opcional para usar ficheiros de dados do eSpeak NG directamente a
 * partir de memória só de leitura (por ex. flash mapeada no ESP32-S3).
 * Este ficheiro faz parte do patch do projecto cardputer-leitor (GPLv3).
 */
#ifndef ESPEAK_NG_MMAP_H
#define ESPEAK_NG_MMAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Devolve um ponteiro só de leitura para o conteúdo completo de `path`
 * (e o tamanho em *size), ou NULL para cair no fopen/malloc normal.
 * O conteúdo tem de estar alinhado a 4 bytes. */
const void *espeak_ng_map_file(const char *path, int *size);

/* 1 se `ptr` aponta para memória devolvida por espeak_ng_map_file
 * (não pode ser libertada com free). */
int espeak_ng_is_mapped(const void *ptr);

#ifdef __cplusplus
}
#endif

#endif
