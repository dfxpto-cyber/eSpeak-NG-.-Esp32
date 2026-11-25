/*
 * espeak_tts: síntese de voz com o eSpeak NG no ESP32.
 *
 * - Os dados de voz (~800 KB para o português) são lidos directamente da
 *   flash mapeada (partição "espeak", componente espeak_pack): não gastam RAM.
 * - Com PSRAM e CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y, a RAM estática
 *   do eSpeak (~150 KB) vai para a PSRAM (linker.lf do componente espeak-ng)
 *   e a pilha da tarefa (32 KB) também. Sem PSRAM fica tudo na RAM interna.
 * - Uma tarefa com a pilha na PSRAM não pode escrever na flash (NVS, SPIFFS,
 *   FAT): por isso o eSpeak corre numa tarefa só dele e o programa apenas
 *   lhe pede frases.
 */
#include <stdio.h>
#include <string.h>
#include "espeak_tts.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <espeak-ng/speak_lib.h>
#include "espeak_pack.h"

static const char *TAG = "espeak_tts";

static espeak_tts_sink_t s_sink;
static void *s_ctx;
static int s_out_rate;
static uint32_t s_step;             /* 22050/out_rate em 16.16 */

static TaskHandle_t s_task;
static SemaphoreHandle_t s_req, s_done;
static const char *s_text;          /* frase pedida (válida até s_done) */
static volatile bool s_ready, s_aborted, s_starting;
static volatile int s_rate = 175, s_rate_applied = -1;
static char s_voice[32] = "roa/pt";
static char s_variant[24];
static volatile bool s_voice_dirty;

static void apply_voice(void)
{
	char name[64];
	snprintf(name, sizeof name, "%s%s%s", s_voice, s_variant[0] ? "+" : "", s_variant);
	if (espeak_SetVoiceByName(name) != EE_OK) {
		ESP_LOGE(TAG, "a voz '%s' não existe no pacote de dados", name);
		if (espeak_SetVoiceByName(s_voice) != EE_OK)
			espeak_SetVoiceByName("roa/pt");
	}
	s_rate_applied = -1;                /* mudar de voz repõe a velocidade */
}

/* ---- conversão de 22 050 Hz para a taxa de saída (interpolação linear) ---- */
static uint32_t s_pos;              /* posição fraccionária relativa a s_prev */
static int16_t s_prev;              /* última amostra do bloco anterior */

static int synth_cb(short *wav, int n, espeak_EVENT *ev)
{
	(void)ev;
	if (s_aborted)
		return 1;
	if (!wav || n <= 0)
		return 0;
	if (s_out_rate == ESPEAK_TTS_NATIVE_RATE) {
		if (!s_sink(wav, (size_t)n, s_ctx)) {
			s_aborted = true;
			return 1;
		}
		return 0;
	}
	static int16_t out[512];
	size_t o = 0;
	/* a amostra de índice 0 é s_prev; wav[k] tem o índice k+1 */
	for (;;) {
		uint32_t idx = s_pos >> 16;
		if ((int)idx >= n)
			break;
		int32_t a = idx == 0 ? s_prev : wav[idx - 1];
		int32_t b = wav[idx];
		int32_t f = s_pos & 0xFFFF;
		out[o++] = (int16_t)(a + (((b - a) * f) >> 16));
		s_pos += s_step;
		if (o == sizeof(out) / sizeof(out[0])) {
			if (!s_sink(out, o, s_ctx)) {
				s_aborted = true;
				return 1;
			}
			o = 0;
		}
	}
	s_pos -= (uint32_t)n << 16;
	s_prev = wav[n - 1];
	if (o && !s_sink(out, o, s_ctx)) {
		s_aborted = true;
		return 1;
	}
	return 0;
}

static void tts_task(void *arg)
{
	(void)arg;
	int64_t t0 = esp_timer_get_time();
	size_t heap0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
	int rate = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 100, ESPEAK_PACK_MOUNT, 0);
	if (rate != ESPEAK_TTS_NATIVE_RATE) {
		ESP_LOGE(TAG, "o eSpeak não arrancou (espeak_Initialize devolveu %d)", rate);
		s_starting = false;
		vTaskDelete(NULL);
		return;
	}
	espeak_SetSynthCallback(synth_cb);
	apply_voice();
	s_voice_dirty = false;
	s_ready = true;
	s_starting = false;
	ESP_LOGI(TAG, "eSpeak pronto em %lld ms (RAM interna gasta: %d B)",
	         (esp_timer_get_time() - t0) / 1000,
	         (int)(heap0 - heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));

	for (;;) {
		xSemaphoreTake(s_req, portMAX_DELAY);
		s_aborted = false;
		if (s_voice_dirty) {
			s_voice_dirty = false;
			apply_voice();
		}
		if (s_rate_applied != s_rate) {
			s_rate_applied = s_rate;
			espeak_SetParameter(espeakRATE, s_rate_applied, 0);
		}
		s_pos = 0;
		s_prev = 0;
		espeak_Synth(s_text, strlen(s_text) + 1, 0, POS_CHARACTER, 0, espeakCHARS_UTF8, NULL, NULL);
		xSemaphoreGive(s_done);
	}
}

bool espeak_tts_init(const espeak_tts_config_t *cfg)
{
	if (!cfg || !cfg->sink) {
		ESP_LOGE(TAG, "falta o sink na configuração");
		return false;
	}
	if (s_task)
		return true;
	if (espeak_pack_mount() != ESP_OK)
		return false;
	s_sink = cfg->sink;
	s_ctx = cfg->ctx;
	s_out_rate = cfg->out_rate > 0 ? cfg->out_rate : ESPEAK_TTS_NATIVE_RATE;
	s_step = (uint32_t)(((uint64_t)ESPEAK_TTS_NATIVE_RATE << 16) / (uint32_t)s_out_rate);
	if (cfg->voice)
		strlcpy(s_voice, cfg->voice, sizeof s_voice);
	if (cfg->variant)
		strlcpy(s_variant, cfg->variant, sizeof s_variant);
	if (cfg->rate_wpm)
		espeak_tts_set_rate(cfg->rate_wpm);
	int stack = cfg->stack_size > 0 ? cfg->stack_size : 32 * 1024;
	int prio = cfg->priority > 0 ? cfg->priority : 4;

	s_req = xSemaphoreCreateBinary();
	s_done = xSemaphoreCreateBinary();
	if (!s_req || !s_done) {
		ESP_LOGE(TAG, "sem memória para os semáforos");
		return false;
	}
	s_starting = true;
	BaseType_t ok = pdFAIL;
#if CONFIG_SPIRAM
	ok = xTaskCreatePinnedToCoreWithCaps(tts_task, "espeak", stack, NULL, prio, &s_task,
	                                     tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
	if (ok != pdPASS) /* sem PSRAM: pilha na RAM interna */
		ok = xTaskCreatePinnedToCore(tts_task, "espeak", stack, NULL, prio, &s_task, tskNO_AFFINITY);
	if (ok != pdPASS) {
		ESP_LOGE(TAG, "sem memória para a tarefa do eSpeak");
		s_starting = false;
		s_task = NULL;
		return false;
	}
	return true;
}

bool espeak_tts_ready(void)
{
	return s_ready;
}

bool espeak_tts_wait_ready(int max_ms)
{
	for (int t = 0; s_starting && t < max_ms; t += 10)
		vTaskDelay(pdMS_TO_TICKS(10));
	return s_ready;
}

void espeak_tts_speak(const char *utf8)
{
	if (!s_ready || !utf8 || !*utf8)
		return;
	s_text = utf8;
	xSemaphoreGive(s_req);
	xSemaphoreTake(s_done, portMAX_DELAY);
}

void espeak_tts_stop(void)
{
	s_aborted = true;
}

void espeak_tts_set_rate(int wpm)
{
	s_rate = wpm < 80 ? 80 : wpm > 450 ? 450 : wpm;
}

int espeak_tts_get_rate(void)
{
	return s_rate;
}

void espeak_tts_set_variant(const char *variant)
{
	strlcpy(s_variant, variant ? variant : "", sizeof s_variant);
	s_voice_dirty = true;
}

const char *espeak_tts_get_variant(void)
{
	return s_variant;
}

void espeak_tts_set_voice(const char *voice)
{
	if (!voice || !*voice)
		return;
	strlcpy(s_voice, voice, sizeof s_voice);
	s_voice_dirty = true;
}

const char *espeak_tts_get_voice(void)
{
	return s_voice;
}
