/*
 * Incluído à força em todos os ficheiros do eSpeak NG (ver CMakeLists.txt).
 * A newlib do ESP-IDF só classifica ASCII nas funções iswalpha/towlower/...
 * (sem _MB_CAPABLE), o que fazia o eSpeak soletrar letras acentuadas
 * ("terça" -> "ter cê-cedilha a"). Redirecciona-as para as tabelas Unicode
 * do ucd-tools, que já fazem parte do eSpeak NG.
 */
#ifndef ESPEAK_ESP_COMPAT_H
#define ESPEAK_ESP_COMPAT_H
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <ucd/ucd.h>

#define iswalnum(c) ucd_isalnum((codepoint_t)(c))
#define iswalpha(c) ucd_isalpha((codepoint_t)(c))
#define iswdigit(c) ucd_isdigit((codepoint_t)(c))
#define iswspace(c) ucd_isspace((codepoint_t)(c))
#define iswupper(c) ucd_isupper((codepoint_t)(c))
#define iswlower(c) ucd_islower((codepoint_t)(c))
#define iswpunct(c) ucd_ispunct((codepoint_t)(c))
#define towlower(c) ucd_tolower((codepoint_t)(c))
#define towupper(c) ucd_toupper((codepoint_t)(c))
#endif
