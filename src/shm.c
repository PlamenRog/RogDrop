#define _GNU_SOURCE

#include "shm.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static int create_shm_fd(size_t size) {
	int fd = memfd_create("rogdrop", MFD_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "memfd_create failed: %s\n", strerror(errno));
		return -1;
	}

	if (ftruncate(fd, (off_t)size) < 0) {
		fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

static void on_buffer_release(void* data, struct wl_buffer* wl_buffer) {
	(void)wl_buffer;
	struct ShmBuffer* buffer = data;
	buffer->busy = false;
	if (buffer->release_cb) {
		buffer->release_cb(buffer);
	}
}

static const struct wl_buffer_listener BUFFER_LISTENER = {
    .release = on_buffer_release,
};

bool shm_buffer_create(struct Rogdrop* app, struct ShmBuffer* buffer, int width, int height, int stride, uint32_t format, void (*release_cb)(struct ShmBuffer* buffer)) {
	shm_buffer_destroy(buffer);

	if (width <= 0 || height <= 0 || stride <= 0) {
		return false;
	}

	size_t size = (size_t)stride * (size_t)height;
	int fd = create_shm_fd(size);
	if (fd < 0) {
		return false;
	}

	void* data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		close(fd);
		return false;
	}

	struct wl_shm_pool* pool = wl_shm_create_pool(app->shm, fd, (int)size);
	struct wl_buffer* wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);

	wl_shm_pool_destroy(pool);
	close(fd);

	if (!wl_buffer) {
		munmap(data, size);
		return false;
	}

	buffer->app = app;
	buffer->wl_buffer = wl_buffer;
	buffer->data = data;
	buffer->size = size;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->format = format;
	buffer->busy = false;
	buffer->release_cb = release_cb;

	if (release_cb) {
		wl_buffer_add_listener(wl_buffer, &BUFFER_LISTENER, buffer);
	}

	return true;
}

void shm_buffer_destroy(struct ShmBuffer* buffer) {
	if (!buffer) {
		return;
	}

	if (buffer->wl_buffer) {
		wl_buffer_destroy(buffer->wl_buffer);
	}

	if (buffer->data && buffer->size) {
		munmap(buffer->data, buffer->size);
	}

	memset(buffer, 0, sizeof(*buffer));
}

void shm_buffer_clear(struct ShmBuffer* buffer) {
	if (buffer && buffer->data && buffer->size) {
		memset(buffer->data, 0, buffer->size);
	}
}
