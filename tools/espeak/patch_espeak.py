#!/usr/bin/env python3
"""Aplica ao código do eSpeak NG as alterações necessárias para correr no
ESP32-S3 sem PSRAM: os ficheiros grandes (phondata, phontab, phonindex,
intonations, *_dict) passam a ser usados directamente a partir de memória
só de leitura (flash mapeada), em vez de serem copiados para a RAM.

Uso: python3 tools/patch_espeak.py components/espeak-ng/src
O script é idempotente (não aplica duas vezes).
"""
import sys, pathlib

src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "components/espeak-ng/src")
MARK = "/* ESPEAK_MMAP_PATCH */"


def patch(fname, pairs):
    p = src / fname
    t = p.read_text()
    if MARK in t:
        print(f"{fname}: já aplicado")
        return
    for old, new in pairs:
        if t.count(old) != 1:
            sys.exit(f"{fname}: padrão não encontrado ou ambíguo:\n{old}")
        t = t.replace(old, new)
    t = MARK + "\n" + t
    p.write_text(t)
    print(f"{fname}: ok")


patch("synthdata.c", [
    ('#include "synthdata.h"',
     '#include "synthdata.h"\n#include "espeak_mmap.h"'),
    # ReadPhFile: tentar primeiro o ficheiro mapeado
    ('''	snprintf(buf, sizeof(buf), "%s%c%s", path_home, PATHSEP, fname);
	length = GetFileLength(buf);''',
     '''	snprintf(buf, sizeof(buf), "%s%c%s", path_home, PATHSEP, fname);
	{
		int msize = 0;
		const void *m = espeak_ng_map_file(buf, &msize);
		if (m != NULL && msize > 0) {
			if (*ptr != NULL && !espeak_ng_is_mapped(*ptr))
				free(*ptr);
			*ptr = (void *)m;
			if (size != NULL)
				*size = msize;
			return ENS_OK;
		}
	}
	length = GetFileLength(buf);'''),
    ('''	if (*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}''',
     '''	if (*ptr != NULL) {
		if (!espeak_ng_is_mapped(*ptr))
			free(*ptr);
		*ptr = NULL;
	}'''),
    ('''	free(phoneme_tab_data);
	free(phoneme_index);
	free(phondata_ptr);
	free(tunes);''',
     '''	if (!espeak_ng_is_mapped(phoneme_tab_data)) free(phoneme_tab_data);
	if (!espeak_ng_is_mapped(phoneme_index)) free(phoneme_index);
	if (!espeak_ng_is_mapped(phondata_ptr)) free(phondata_ptr);
	if (!espeak_ng_is_mapped(tunes)) free(tunes);'''),
])

patch("dictionary.c", [
    ('#include "dictionary.h"',
     '#include "dictionary.h"\n#include "espeak_mmap.h"'),
    ('''static void release_dictionary(Translator *tr)
{
	free(tr->data_dictlist);''',
     '''static void release_dictionary(Translator *tr)
{
	if (!espeak_ng_is_mapped(tr->data_dictlist))
		free(tr->data_dictlist);'''),
    ('''	release_dictionary(tr);

	f = fopen(fname, "rb");''',
     '''	release_dictionary(tr);

	{
		int msize = 0;
		const void *m = espeak_ng_map_file(fname, &msize);
		if (m != NULL && msize > 0) {
			tr->data_dictlist = (char *)m;
			size = msize;
			goto dictionary_loaded;
		}
	}

	f = fopen(fname, "rb");'''),
    ('''	size = fread(tr->data_dictlist, 1, size, f);
	fclose(f);
''',
     '''	size = fread(tr->data_dictlist, 1, size, f);
	fclose(f);
dictionary_loaded:
'''),
])

# MakePhonemeList tem um array local de ~36 KB (1000 x PHONEME_LIST) que
# rebenta a pilha de uma tarefa FreeRTOS. O eSpeak não é reentrante, por isso
# passa a estático (vai para .bss).
patch("phonemelist.c", [
    ('	PHONEME_LIST ph_list3[N_PHONEME_LIST];',
     '	static PHONEME_LIST ph_list3[N_PHONEME_LIST]; /* ESP32: fora da pilha */'),
])

patch("translate.c", [
    ('#include "translate.h"',
     '#include "translate.h"\n#include "espeak_mmap.h"'),
    ('		free(tr->data_dictlist);',
     '		if (!espeak_ng_is_mapped(tr->data_dictlist))\n			free(tr->data_dictlist);'),
])
