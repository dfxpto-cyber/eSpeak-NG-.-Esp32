# eSpeak NG (GPLv3) como componente ESP-IDF, com o patch de dados mapeados.
file(GLOB ESPEAK_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/*.c ${CMAKE_CURRENT_LIST_DIR}/ucd/*.c)
# Ficheiros que não são necessários no dispositivo (compilação de dados de
# fonemas, MBROLA, modo assíncrono, Klatt, speechPlayer).
foreach(excl compiledata compilembrola spect mbrowrap synth_mbrola event fifo
             espeak_command sPlayer klatt)
    list(FILTER ESPEAK_SRCS EXCLUDE REGEX "/${excl}\\.c$")
endforeach()

idf_component_register(SRCS ${ESPEAK_SRCS}
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS src
                       LDFRAGMENTS linker.lf)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -w -O2 -Wno-error
    -DLIBESPEAK_NG_EXPORT)
# classificação Unicode correcta (ver src/espeak_esp_compat.h)
target_compile_options(${COMPONENT_LIB} PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_LIST_DIR}/src/espeak_esp_compat.h")
