/* Teste: cada frase tem de produzir áudio com uma duração plausível. */
#include <stdio.h>
#include <stdlib.h>
#include "espeak_tts.h"

static size_t s_n;
static int s_peak;

static bool count_sink(const int16_t *pcm, size_t n, void *ctx)
{
	(void)ctx;
	for (size_t i = 0; i < n; i++)
		if (abs(pcm[i]) > s_peak)
			s_peak = abs(pcm[i]);
	s_n += n;
	return true;
}

static int frase(const char *t, int rate)
{
	s_n = 0;
	s_peak = 0;
	espeak_tts_speak(t);
	double seg = (double)s_n / rate;
	int ok = seg > 0.4 && seg < 20 && s_peak > 1000;
	printf("TESTE %s: %.2f s, pico %d: \"%s\"\n", ok ? "OK" : "FALHOU", seg, s_peak, t);
	return ok;
}

void app_main(void)
{
	espeak_tts_config_t cfg = { .sink = count_sink, .out_rate = 16000 };
	if (!espeak_tts_init(&cfg) || !espeak_tts_wait_ready(60000)) {
		printf("TESTE FALHOU: o eSpeak não arrancou\n");
		return;
	}
	int ok = 1;
	ok &= frase("Olá! Sou um ESP32 a falar português.", 16000);
	ok &= frase("Terça-feira, 24 de Novembro, às 21 horas.", 16000);
	espeak_tts_set_variant("f3");
	ok &= frase("Timbre f3.", 16000);
	espeak_tts_set_variant("Diogo");
	ok &= frase("Timbre Diogo.", 16000);
	espeak_tts_set_variant(NULL);
	espeak_tts_set_rate(350);
	ok &= frase("Velocidade de trezentas e cinquenta palavras por minuto.", 16000);
	printf("TESTE %s\n", ok ? "TUDO OK" : "COM FALHAS");
}
