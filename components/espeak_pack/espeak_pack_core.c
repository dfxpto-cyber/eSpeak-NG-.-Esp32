/*
 * Leitura do pacote de dados e ganchos de mapeamento para o eSpeak NG.
 * Código portátil: usado no firmware e no teste no PC (tests/host).
 */
#include <string.h>
#include "espeak_pack.h"
#include "espeak_mmap.h"

#define NAME_LEN 56

typedef struct {
	char name[NAME_LEN];
	uint32_t offset;
	uint32_t size;
} pack_entry_t;

static const uint8_t *s_base;
static size_t s_size;
static uint32_t s_count;

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int espeak_pack_attach(const void *base, size_t size)
{
	const uint8_t *b = (const uint8_t *)base;
	if (size < 16 || memcmp(b, "ESPK", 4) != 0 || rd32(b + 4) != 1)
		return -1;
	uint32_t n = rd32(b + 8);
	if (16 + (size_t)n * sizeof(pack_entry_t) > size)
		return -2;
	const pack_entry_t *e = (const pack_entry_t *)(b + 16);
	for (uint32_t i = 0; i < n; i++) {
		if ((size_t)e[i].offset + e[i].size > size || (e[i].offset & 3))
			return -3;
	}
	s_base = b;
	s_size = size;
	s_count = n;
	return 0;
}

const void *espeak_pack_find(const char *rel, int *size)
{
	if (!s_base || !rel)
		return NULL;
	while (*rel == '/')
		rel++;
	const pack_entry_t *e = (const pack_entry_t *)(s_base + 16);
	for (uint32_t i = 0; i < s_count; i++) {
		if (strncmp(e[i].name, rel, NAME_LEN) == 0) {
			if (size)
				*size = (int)e[i].size;
			return s_base + e[i].offset;
		}
	}
	return NULL;
}

/* ---- Ganchos chamados pelo eSpeak NG (ver espeak_mmap.h) ---- */

const void *espeak_ng_map_file(const char *path, int *size)
{
	static const char prefix[] = ESPEAK_PACK_MOUNT "/";
	if (!path || strncmp(path, prefix, sizeof(prefix) - 1) != 0)
		return NULL;
	return espeak_pack_find(path + sizeof(prefix) - 1, size);
}

int espeak_ng_is_mapped(const void *ptr)
{
	const uint8_t *p = (const uint8_t *)ptr;
	return s_base && p >= s_base && p < s_base + s_size;
}
