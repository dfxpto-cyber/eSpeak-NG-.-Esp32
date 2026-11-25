idf_component_register(SRCS espeak_pack_core.c espeak_pack_esp.c
                       INCLUDE_DIRS include
                       REQUIRES espeak-ng esp_partition vfs)
