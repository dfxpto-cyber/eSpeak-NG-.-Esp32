/*
 * Parte específica do ESP32: mapeia a partição "espeak" na memória e expõe
 * o conteúdo como sistema de ficheiros só de leitura em ESPEAK_PACK_MOUNT
 * (para os ficheiros pequenos que o eSpeak NG abre com fopen, como a voz).
 * Os ficheiros grandes são usados directamente pelo gancho de mapeamento.
 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs.h"
#include "espeak_pack.h"

static const char *TAG = "espeak_pack";

#define PACK_MAX_FDS 4

typedef struct {
	const uint8_t *data;
	int size;
	int pos;
	bool used;
} pack_fd_t;

static pack_fd_t s_fds[PACK_MAX_FDS];

static bool is_root(const char *path)
{
	return path[0] == '\0' || (path[0] == '/' && path[1] == '\0');
}

static int vfs_open(const char *path, int flags, int mode)
{
	(void)mode;
	if ((flags & O_ACCMODE) != O_RDONLY) {
		errno = EROFS;
		return -1;
	}
	int size = 0;
	const void *d = espeak_pack_find(path, &size);
	if (!d) {
		errno = ENOENT;
		return -1;
	}
	for (int i = 0; i < PACK_MAX_FDS; i++) {
		if (!s_fds[i].used) {
			s_fds[i] = (pack_fd_t){ .data = d, .size = size, .pos = 0, .used = true };
			return i;
		}
	}
	errno = ENFILE;
	return -1;
}

static ssize_t vfs_read(int fd, void *dst, size_t len)
{
	if (fd < 0 || fd >= PACK_MAX_FDS || !s_fds[fd].used) {
		errno = EBADF;
		return -1;
	}
	pack_fd_t *f = &s_fds[fd];
	int left = f->size - f->pos;
	if ((int)len > left)
		len = left;
	memcpy(dst, f->data + f->pos, len);
	f->pos += len;
	return len;
}

static off_t vfs_lseek(int fd, off_t off, int whence)
{
	if (fd < 0 || fd >= PACK_MAX_FDS || !s_fds[fd].used) {
		errno = EBADF;
		return -1;
	}
	pack_fd_t *f = &s_fds[fd];
	off_t np = whence == SEEK_SET ? off : whence == SEEK_CUR ? f->pos + off : f->size + off;
	if (np < 0 || np > f->size) {
		errno = EINVAL;
		return -1;
	}
	f->pos = np;
	return np;
}

static int vfs_close(int fd)
{
	if (fd < 0 || fd >= PACK_MAX_FDS || !s_fds[fd].used) {
		errno = EBADF;
		return -1;
	}
	s_fds[fd].used = false;
	return 0;
}

static int vfs_fstat(int fd, struct stat *st)
{
	if (fd < 0 || fd >= PACK_MAX_FDS || !s_fds[fd].used) {
		errno = EBADF;
		return -1;
	}
	memset(st, 0, sizeof(*st));
	st->st_mode = S_IFREG | 0444;
	st->st_size = s_fds[fd].size;
	return 0;
}

static int vfs_stat(const char *path, struct stat *st)
{
	memset(st, 0, sizeof(*st));
	if (is_root(path)) {
		st->st_mode = S_IFDIR | 0555;
		return 0;
	}
	int size = 0;
	if (!espeak_pack_find(path, &size)) {
		errno = ENOENT;
		return -1;
	}
	st->st_mode = S_IFREG | 0444;
	st->st_size = size;
	return 0;
}

esp_err_t espeak_pack_mount(void)
{
	const esp_partition_t *part = esp_partition_find_first(
		ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "espeak");
	if (!part) {
		ESP_LOGE(TAG, "partição 'espeak' não encontrada (partitions.csv)");
		return ESP_ERR_NOT_FOUND;
	}
	const void *ptr = NULL;
	esp_partition_mmap_handle_t h;
	esp_err_t err = esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA, &ptr, &h);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_partition_mmap falhou: %s", esp_err_to_name(err));
		return err;
	}
	int r = espeak_pack_attach(ptr, part->size);
	if (r != 0) {
		ESP_LOGE(TAG, "dados do eSpeak inválidos (%d). Gravaste data/espeak-pt.bin? "
		              "Usa: idf.py flash (o CMake grava a partição automaticamente)", r);
		esp_partition_munmap(h);
		return ESP_ERR_INVALID_STATE;
	}
	const esp_vfs_t vfs = {
		.flags = ESP_VFS_FLAG_DEFAULT,
		.open = vfs_open,
		.read = vfs_read,
		.lseek = vfs_lseek,
		.close = vfs_close,
		.fstat = vfs_fstat,
		.stat = vfs_stat,
	};
	err = esp_vfs_register(ESPEAK_PACK_MOUNT, &vfs, NULL);
	if (err == ESP_OK)
		ESP_LOGI(TAG, "dados do eSpeak mapeados em %p (%u KiB)", ptr, (unsigned)(part->size / 1024));
	return err;
}
