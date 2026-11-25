idf_component_register(SRCS "main.c" PRIV_REQUIRES espeak_tts)
esptool_py_flash_to_partition(flash "espeak" "${CMAKE_CURRENT_LIST_DIR}/../../../data/espeak-pt.bin")
