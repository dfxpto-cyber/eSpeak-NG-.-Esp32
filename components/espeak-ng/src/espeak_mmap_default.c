/* Implementações por omissão (sem mapeamento). A aplicação substitui-as
 * com versões fortes; ver main/espeak_pack.c. */
#include <stddef.h>
#include "espeak_mmap.h"

__attribute__((weak)) const void *espeak_ng_map_file(const char *path, int *size)
{
	(void)path;
	(void)size;
	return NULL;
}

__attribute__((weak)) int espeak_ng_is_mapped(const void *ptr)
{
	(void)ptr;
	return 0;
}
