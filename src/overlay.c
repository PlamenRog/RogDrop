#include "overlay.h"

#include "capture.h"
#include "shm.h"

#include <stdio.h>
#include <string.h>

static inline uint32_t argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void fill_rect(struct ShmBuffer* buffer, int x, int y, int width, int height, uint32_t color) {
	int x0 = x < 0 ? 0 : x;
	int y0 = y < 0 ? 0 : y;
	int x1 = x + width > buffer->width ? buffer->width : x + width;
	int y1 = y + height > buffer->height ? buffer->height : y + height;

	for (int py = y0; py < y1; ++py) {
		uint8_t* row = (uint8_t*)buffer->data + (size_t)py * (size_t)buffer->stride;
		uint32_t* pixels = (uint32_t*)row;
		for (int px = x0; px < x1; ++px) {
			pixels[px] = color;
		}
	}
}

static void outline_rect(struct ShmBuffer* buffer, int x, int y, int width, int height, int thickness, uint32_t color) {
	fill_rect(buffer, x, y, width, thickness, color);
	fill_rect(buffer, x, y + height - thickness, width, thickness, color);
	fill_rect(buffer, x, y, thickness, height, color);
	fill_rect(buffer, x + width - thickness, y, thickness, height, color);
}

static uint32_t darken(uint8_t r, uint8_t g, uint8_t b) {
	return argb(255, (uint8_t)(r * 3 / 4), (uint8_t)(g * 3 / 4), (uint8_t)(b * 3 / 4));
}

static void choose_lens_position(const struct Output* output, double pointer_x, double pointer_y, int* lens_x, int* lens_y) {
	int x = (int)pointer_x + MAG_OFFSET;
	int y = (int)pointer_y + MAG_OFFSET;

	if (x + MAG_SIZE + 8 > output->surface_width) {
		x = (int)pointer_x - MAG_OFFSET - MAG_SIZE;
	}
	if (y + MAG_SIZE + 8 > output->surface_height) {
		y = (int)pointer_y - MAG_OFFSET - MAG_SIZE;
	}

	if (x < 4) {
		x = 4;
	}
	if (y < 4) {
		y = 4;
	}

	*lens_x = x;
	*lens_y = y;
}

static void draw_magnifier(struct Output* output, struct ShmBuffer* buffer) {
	struct Rogdrop* app = output->app;
	uint8_t lens[MAG_RGB_SIZE];
	uint8_t center_r;
	uint8_t center_g;
	uint8_t center_b;

	if (app->active_output != output || !app->pointer_valid) {
		return;
	}

	if (!capture_sample_lens(output, app->pointer_x, app->pointer_y, lens, &center_r, &center_g, &center_b)) {
		return;
	}

	(void)center_r;
	(void)center_g;
	(void)center_b;

	int lens_x;
	int lens_y;
	choose_lens_position(output, app->pointer_x, app->pointer_y, &lens_x, &lens_y);

	fill_rect(buffer, lens_x + 5, lens_y + 5, MAG_SIZE + 4, MAG_SIZE + 4, argb(110, 0, 0, 0));
	fill_rect(buffer, lens_x - 3, lens_y - 3, MAG_SIZE + 6, MAG_SIZE + 6, argb(255, 0, 0, 0));
	fill_rect(buffer, lens_x - 1, lens_y - 1, MAG_SIZE + 2, MAG_SIZE + 2, argb(255, 255, 255, 255));

	for (int row = 0; row < MAG_PIXELS; ++row) {
		for (int column = 0; column < MAG_PIXELS; ++column) {
			size_t index = ((size_t)row * MAG_PIXELS + (size_t)column) * 3;
			uint8_t r = lens[index + 0];
			uint8_t g = lens[index + 1];
			uint8_t b = lens[index + 2];

			int x = lens_x + column * CELL_SIZE;
			int y = lens_y + row * CELL_SIZE;
			fill_rect(buffer, x, y, CELL_SIZE, CELL_SIZE, argb(255, r, g, b));

			// one pixel grid
			uint32_t grid = darken(r, g, b);
			fill_rect(buffer, x, y, CELL_SIZE, 1, grid);
			fill_rect(buffer, x, y, 1, CELL_SIZE, grid);
		}
	}

	int center = MAG_PIXELS / 2;
	int center_x = lens_x + center * CELL_SIZE;
	int center_y = lens_y + center * CELL_SIZE;

	outline_rect(buffer, center_x, center_y, CELL_SIZE, CELL_SIZE, 3, argb(255, 255, 255, 255));
	outline_rect(buffer, center_x + 1, center_y + 1, CELL_SIZE - 2, CELL_SIZE - 2, 2, argb(255, 255, 0, 0));
}

static void overlay_buffer_released(struct ShmBuffer* buffer) {
	struct Output* output = buffer->user_data;
	if (output && output->overlay_redraw_pending) {
		overlay_redraw(output);
	}
}

static bool create_overlay_buffers(struct Output* output) {
	for (size_t i = 0; i < 2; ++i) {
		if (!shm_buffer_create(
		        output->app, &output->overlay[i], output->surface_width,
		        output->surface_height, output->surface_width * 4,
		        WL_SHM_FORMAT_ARGB8888, overlay_buffer_released)) {
			return false;
		}
		output->overlay[i].user_data = output;
	}

	return true;
}

static void layer_configure(void* data, struct zwlr_layer_surface_v1* layer_surface, uint32_t serial, uint32_t width, uint32_t height) {
	struct Output* output = data;
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

	if (width == 0 || height == 0) {
		return;
	}

	if (!output->configured) {
		output->surface_width = (int)width;
		output->surface_height = (int)height;
		output->configured = true;

		if (!create_overlay_buffers(output)) {
			fprintf(stderr, "%s: failed to allocate overlay buffers\n", output->name);
			output->app->running = false;
			output->app->exit_status = 1;
			return;
		}
	}
	else if (output->surface_width != (int)width || output->surface_height != (int)height) {
		// reconfiguration is rare, so defer replacing buffers until they are free
		bool busy = output->overlay[0].busy || output->overlay[1].busy;
		if (busy) {
			output->surface_width = (int)width;
			output->surface_height = (int)height;
			return;
		}

		shm_buffer_destroy(&output->overlay[0]);
		shm_buffer_destroy(&output->overlay[1]);
		output->surface_width = (int)width;
		output->surface_height = (int)height;

		if (!create_overlay_buffers(output)) {
			output->app->running = false;
			output->app->exit_status = 1;
			return;
		}
	}

	overlay_redraw(output);
}

static void layer_closed(void* data, struct zwlr_layer_surface_v1* layer_surface) {
	(void)layer_surface;
	struct Output* output = data;
	output->app->running = false;
	output->app->exit_status = 1;
}

static const struct zwlr_layer_surface_v1_listener LAYER_LISTENER = {
    .configure = layer_configure,
    .closed = layer_closed,
};

bool overlay_create(struct Output* output) {
	struct Rogdrop* app = output->app;

	output->surface = wl_compositor_create_surface(app->compositor);
	if (!output->surface) {
		return false;
	}

	output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(app->layer_shell, output->surface, output->wl_output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, ROGDROP_NAMESPACE);
	if (!output->layer_surface) {
		return false;
	}

	zwlr_layer_surface_v1_add_listener(output->layer_surface, &LAYER_LISTENER, output);

	zwlr_layer_surface_v1_set_anchor(output->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_size(output->layer_surface, 0, 0);
	zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);
	zwlr_layer_surface_v1_set_keyboard_interactivity(output->layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

	wl_surface_commit(output->surface);
	return true;
}

void overlay_destroy(struct Output* output) {
	if (!output) {
		return;
	}

	shm_buffer_destroy(&output->overlay[0]);
	shm_buffer_destroy(&output->overlay[1]);

	if (output->layer_surface) {
		zwlr_layer_surface_v1_destroy(output->layer_surface);
		output->layer_surface = NULL;
	}

	if (output->surface) {
		wl_surface_destroy(output->surface);
		output->surface = NULL;
	}
}

void overlay_redraw(struct Output* output) {
	if (!output || !output->configured || !output->surface) {
		return;
	}

	struct ShmBuffer* buffer = NULL;
	for (size_t i = 0; i < 2; ++i) {
		if (output->overlay[i].wl_buffer && !output->overlay[i].busy) {
			buffer = &output->overlay[i];
			break;
		}
	}

	if (!buffer) {
		output->overlay_redraw_pending = true;
		return;
	}

	output->overlay_redraw_pending = false;
	shm_buffer_clear(buffer);
	draw_magnifier(output, buffer);

	buffer->busy = true;
	wl_surface_attach(output->surface, buffer->wl_buffer, 0, 0);
	wl_surface_damage(output->surface, 0, 0, output->surface_width, output->surface_height);
	wl_surface_commit(output->surface);
}

void overlay_redraw_active(struct Rogdrop* app) {
	if (app && app->active_output) {
		overlay_redraw(app->active_output);
	}
}

struct Output* overlay_output_for_surface(struct Rogdrop* app, struct wl_surface* surface) {
	for (struct Output* output = app->outputs; output; output = output->next) {
		if (output->surface == surface) {
			return output;
		}
	}
	return NULL;
}
