/*
 * Exemplo do eSpeak NG no ESP32: diz algumas frases em português por um
 * amplificador I2S (por exemplo MAX98357A) e mostra os timbres e a velocidade.
 *
 * Ligações (mudar em idf.py menuconfig, "Exemplo eSpeak: pinos do I2S"):
 *   BCLK -> GPIO 5, LRC/WS -> GPIO 6, DIN -> GPIO 7, mais 3,3 V ou 5 V e GND.
 */
#include <stdio.h>
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "espeak_tts.h"

static const char *TAG = "ola_mundo";
static i2s_chan_handle_t s_tx;

/* Recebe o áudio do eSpeak (mono, 16 bits) e envia-o para o I2S. */
static bool i2s_sink(const int16_t *pcm, size_t n, void *ctx)
{
	(void)ctx;
	size_t written = 0;
	return i2s_channel_write(s_tx, pcm, n * sizeof(int16_t), &written, portMAX_DELAY) == ESP_OK;
}

static void i2s_init(int rate)
{
	i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
	chan.auto_clear = true;                 /* silêncio quando não há voz */
	ESP_ERROR_CHECK(i2s_new_channel(&chan, &s_tx, NULL));
	i2s_std_config_t std = {
		.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
		.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
		.gpio_cfg = {
			.mclk = I2S_GPIO_UNUSED,
			.bclk = CONFIG_EXEMPLO_I2S_BCLK,
			.ws = CONFIG_EXEMPLO_I2S_WS,
			.dout = CONFIG_EXEMPLO_I2S_DOUT,
			.din = I2S_GPIO_UNUSED,
		},
	};
	ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std));
	ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
}

void app_main(void)
{
	i2s_init(CONFIG_EXEMPLO_TAXA);

	espeak_tts_config_t cfg = {
		.sink = i2s_sink,
		.out_rate = CONFIG_EXEMPLO_TAXA,
		.voice = "roa/pt",
	};
	if (!espeak_tts_init(&cfg) || !espeak_tts_wait_ready(10000)) {
		ESP_LOGE(TAG, "o eSpeak não arrancou: gravou a partição espeak com idf.py flash?");
		return;
	}

	espeak_tts_speak("Olá! Sou um ESP32 a falar português com o eSpeak.");
	espeak_tts_speak("Hoje é terça-feira, 24 de Novembro de 2026, e são 21 horas e 52 minutos.");
	espeak_tts_speak("A temperatura é de 18,5 graus.");

	static const char *timbres[] = { "f3", "m1", "Andrea", "Diogo" };
	for (int i = 0; i < sizeof timbres / sizeof timbres[0]; i++) {
		char frase[64];
		espeak_tts_set_variant(timbres[i]);
		snprintf(frase, sizeof frase, "Este é o timbre %s.", timbres[i]);
		espeak_tts_speak(frase);
	}

	espeak_tts_set_variant(NULL);
	espeak_tts_set_rate(300);
	espeak_tts_speak("E esta é a velocidade de trezentas palavras por minuto, para quem usa leitor de ecrã.");
	espeak_tts_set_rate(175);

	for (int n = 1;; n++) {
		char frase[48];
		snprintf(frase, sizeof frase, "Contagem: %d.", n);
		espeak_tts_speak(frase);
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}
