/*
 * espeak_tts: síntese de voz com o eSpeak NG no ESP32, numa tarefa própria.
 *
 * Os dados da voz são lidos directamente da partição "espeak" (flash
 * mapeada, ver componente espeak_pack). O áudio é entregue a uma função
 * do programa (sink), em mono, 16 bits, à taxa pedida em espeak_tts_init.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Taxa nativa do eSpeak NG. */
#define ESPEAK_TTS_NATIVE_RATE 22050

/* Recebe um bloco de áudio (mono, 16 bits). Devolve false para interromper
 * a frase. É chamada na tarefa do eSpeak. */
typedef bool (*espeak_tts_sink_t)(const int16_t *pcm, size_t n, void *ctx);

typedef struct {
	espeak_tts_sink_t sink;   /* obrigatório */
	void *ctx;                /* passado ao sink */
	int out_rate;             /* taxa de saída em Hz (0 = 22050, sem conversão) */
	const char *voice;        /* voz do eSpeak (NULL = "roa/pt") */
	const char *variant;      /* timbre ("f3", "m1", "Andrea"...; NULL = voz normal) */
	int rate_wpm;             /* velocidade em palavras por minuto (0 = 175) */
	int stack_size;           /* pilha da tarefa (0 = 32 KB) */
	int priority;             /* prioridade da tarefa (0 = 4) */
} espeak_tts_config_t;

/* Monta a partição "espeak" e arranca a tarefa do eSpeak. Se houver PSRAM,
 * a pilha da tarefa vai para lá. Devolve false se faltarem os dados ou a
 * memória. O arranque continua em segundo plano: use espeak_tts_wait_ready. */
bool espeak_tts_init(const espeak_tts_config_t *cfg);

/* true quando o eSpeak está pronto a falar. */
bool espeak_tts_ready(void);

/* Espera até max_ms que o arranque termine. Devolve espeak_tts_ready(). */
bool espeak_tts_wait_ready(int max_ms);

/* Fala um texto em UTF-8 e bloqueia até acabar (ou até o sink devolver
 * false, ou até espeak_tts_stop). Não chamar a partir do sink. */
void espeak_tts_speak(const char *utf8);

/* Interrompe a frase em curso (pode ser chamada de outra tarefa). */
void espeak_tts_stop(void);

/* Velocidade em palavras por minuto (80 a 450). Aplica-se na frase seguinte. */
void espeak_tts_set_rate(int wpm);
int espeak_tts_get_rate(void);

/* Timbre (variante). "" ou NULL = voz normal. Aplica-se na frase seguinte. */
void espeak_tts_set_variant(const char *variant);
const char *espeak_tts_get_variant(void);

/* Voz do eSpeak, por exemplo "roa/pt" ou "gmw/en", desde que esteja no
 * pacote de dados. Aplica-se na frase seguinte. */
void espeak_tts_set_voice(const char *voice);
const char *espeak_tts_get_voice(void);

#ifdef __cplusplus
}
#endif
