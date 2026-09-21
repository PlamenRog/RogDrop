#ifndef ROGDROP_SHM_H
#define ROGDROP_SHM_H

#include "rogdrop.h"

bool shm_buffer_create(struct Rogdrop* app, struct ShmBuffer* buffer, int width, int height, int stride, uint32_t format, void (*release_cb)(struct ShmBuffer* buffer));

void shm_buffer_destroy(struct ShmBuffer* buffer);
void shm_buffer_clear(struct ShmBuffer* buffer);

#endif
