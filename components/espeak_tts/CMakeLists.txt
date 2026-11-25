idf_component_register(SRCS "espeak_tts.c"
                       INCLUDE_DIRS "include"
                       REQUIRES espeak-ng espeak_pack
                       PRIV_REQUIRES esp_timer)
