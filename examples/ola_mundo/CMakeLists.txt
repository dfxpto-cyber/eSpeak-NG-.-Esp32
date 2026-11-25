# Exemplo: o ESP32 fala em português com o eSpeak NG, por um amplificador I2S
# (por exemplo MAX98357A). Ver o README.md da raiz do repositório.
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ola_mundo)
