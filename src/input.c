#include "input.h"

#include "capture.h"
#include "overlay.h"
#include "shm.h"

#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define CURSOR_SIZE 64
#define CURSOR_CENTER (CURSOR_SIZE / 2)
#define CURSOR_GAP 14

static inline uint32_t cursor_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void cursor_fill_rect(struct ShmBuffer* buffer, int x, int y, int width, int height, uint32_t color) {
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

static bool create_hollow_cursor(struct Rogdrop* app) {
	if (app->cursor_surface && app->cursor_buffer.wl_buffer) {
		return true;
	}

	app->cursor_surface = wl_compositor_create_surface(app->compositor);
	if (!app->cursor_surface) {
		return false;
	}

	if (!shm_buffer_create(app, &app->cursor_buffer, CURSOR_SIZE, CURSOR_SIZE, CURSOR_SIZE * 4, WL_SHM_FORMAT_ARGB8888, NULL)) {
		wl_surface_destroy(app->cursor_surface);
		app->cursor_surface = NULL;
		return false;
	}

	shm_buffer_clear(&app->cursor_buffer);

	// replaces standart cursor that mango uses with a hollow one to stop getting in the way of the pointer
	const uint32_t black = cursor_argb(255, 0, 0, 0);
	const uint32_t white = cursor_argb(255, 255, 255, 255);
	const int c = CURSOR_CENTER;
	const int gap = CURSOR_GAP;

	// four crosshair arms; black outline white core
	cursor_fill_rect(&app->cursor_buffer, 2, c - 2, c - gap - 2, 5, black);
	cursor_fill_rect(&app->cursor_buffer, 3, c - 1, c - gap - 3, 3, white);

	cursor_fill_rect(&app->cursor_buffer, c + gap, c - 2, CURSOR_SIZE - (c + gap) - 2, 5, black);
	cursor_fill_rect(&app->cursor_buffer, c + gap + 1, c - 1, CURSOR_SIZE - (c + gap) - 4, 3, white);

	cursor_fill_rect(&app->cursor_buffer, c - 2, 2, 5, c - gap - 2, black);
	cursor_fill_rect(&app->cursor_buffer, c - 1, 3, 3, c - gap - 3, white);

	cursor_fill_rect(&app->cursor_buffer, c - 2, c + gap, 5, CURSOR_SIZE - (c + gap) - 2, black);
	cursor_fill_rect(&app->cursor_buffer, c - 1, c + gap + 1, 3, CURSOR_SIZE - (c + gap) - 4, white);

	wl_surface_attach(app->cursor_surface, app->cursor_buffer.wl_buffer, 0, 0);
	wl_surface_damage_buffer(app->cursor_surface, 0, 0, CURSOR_SIZE, CURSOR_SIZE);
	wl_surface_commit(app->cursor_surface);

	return true;
}

static void show_hollow_cursor(struct Rogdrop* app, uint32_t serial) {
	if (!app->pointer) {
		return;
	}

	if (!create_hollow_cursor(app)) {
		wl_pointer_set_cursor(app->pointer, serial, NULL, 0, 0);
		return;
	}

	wl_pointer_set_cursor(app->pointer, serial, app->cursor_surface, CURSOR_CENTER, CURSOR_CENTER);
}

static void set_active_output(struct Rogdrop* app, struct Output* output) {
	if (app->active_output == output) {
		return;
	}

	struct Output* old = app->active_output;
	app->active_output = output;

	if (old && old->layer_surface) {
		zwlr_layer_surface_v1_set_keyboard_interactivity(old->layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
		wl_surface_commit(old->surface);
		overlay_redraw(old);
	}

	if (output && output->layer_surface) {
		zwlr_layer_surface_v1_set_keyboard_interactivity(output->layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
		wl_surface_commit(output->surface);
	}
}

static void pointer_enter(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	(void)pointer;
	struct Rogdrop* app = data;
	struct Output* output = overlay_output_for_surface(app, surface);
	if (!output) {
		return;
	}

	set_active_output(app, output);
	app->pointer_x = wl_fixed_to_double(surface_x);
	app->pointer_y = wl_fixed_to_double(surface_y);
	app->pointer_valid = true;

	show_hollow_cursor(app, serial);
	overlay_redraw(output);
}

static void pointer_leave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface) {
	(void)pointer;
	(void)serial;
	(void)surface;

	struct Rogdrop* app = data;
	struct Output* output = app->active_output;
	app->pointer_valid = false;

	if (output) {
		overlay_redraw(output);
	}
}

static void pointer_motion(void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	(void)pointer;
	(void)time;

	struct Rogdrop* app = data;
	if (!app->active_output) {
		return;
	}

	app->pointer_x = wl_fixed_to_double(surface_x);
	app->pointer_y = wl_fixed_to_double(surface_y);
	app->pointer_valid = true;
	overlay_redraw(app->active_output);
}

static void pointer_button(void* data, struct wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	(void)pointer;
	(void)serial;
	(void)time;

	struct Rogdrop* app = data;
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}

	if (button == BTN_RIGHT) {
		app->exit_status = EXIT_FAILURE;
		app->running = false;
		return;
	}

	if (button != BTN_LEFT || !app->active_output || !app->pointer_valid) {
		return;
	}

	uint8_t pixels[MAG_RGB_SIZE];
	uint8_t r;
	uint8_t g;
	uint8_t b;

	if (!capture_sample_lens(app->active_output, app->pointer_x, app->pointer_y, pixels, &r, &g, &b)) {
		return;
	}

	printf("#%02X%02X%02X\n", r, g, b);
	fflush(stdout);
	app->exit_status = EXIT_SUCCESS;
	app->running = false;
}

static void pointer_axis(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
	(void)data;
	(void)pointer;
	(void)time;
	(void)axis;
	(void)value;
}

static void pointer_frame(void* data, struct wl_pointer* pointer) {
	(void)data;
	(void)pointer;
}

static void pointer_axis_source(void* data, struct wl_pointer* pointer, uint32_t axis_source) {
	(void)data;
	(void)pointer;
	(void)axis_source;
}

static void pointer_axis_stop(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis) {
	(void)data;
	(void)pointer;
	(void)time;
	(void)axis;
}

static void pointer_axis_discrete(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t discrete) {
	(void)data;
	(void)pointer;
	(void)axis;
	(void)discrete;
}

#ifdef WL_POINTER_AXIS_VALUE120_SINCE_VERSION
static void pointer_axis_value120(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t value120) {
	(void)data;
	(void)pointer;
	(void)axis;
	(void)value120;
}
#endif

#ifdef WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION
static void pointer_axis_relative_direction(void* data, struct wl_pointer* pointer, uint32_t axis, uint32_t direction) {
	(void)data;
	(void)pointer;
	(void)axis;
	(void)direction;
}
#endif

static const struct wl_pointer_listener POINTER_LISTENER = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
#ifdef WL_POINTER_AXIS_VALUE120_SINCE_VERSION
    .axis_value120 = pointer_axis_value120,
#endif
#ifdef WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION
    .axis_relative_direction = pointer_axis_relative_direction,
#endif
};

static void keyboard_keymap(void* data, struct wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size) {
	(void)keyboard;
	struct Rogdrop* app = data;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	char* map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED) {
		return;
	}

	struct xkb_keymap* keymap = xkb_keymap_new_from_string(app->xkb_context, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map, size);

	if (!keymap) {
		return;
	}

	struct xkb_state* state = xkb_state_new(keymap);
	if (!state) {
		xkb_keymap_unref(keymap);
		return;
	}

	if (app->xkb_state) {
		xkb_state_unref(app->xkb_state);
	}
	if (app->xkb_keymap) {
		xkb_keymap_unref(app->xkb_keymap);
	}

	app->xkb_keymap = keymap;
	app->xkb_state = state;
}

static void keyboard_enter(void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface, struct wl_array* keys) {
	(void)data;
	(void)keyboard;
	(void)serial;
	(void)surface;
	(void)keys;
}

static void keyboard_leave(void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface) {
	(void)data;
	(void)keyboard;
	(void)serial;
	(void)surface;
}

static void keyboard_key(void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	(void)keyboard;
	(void)serial;
	(void)time;

	struct Rogdrop* app = data;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !app->xkb_state) {
		return;
	}

	xkb_keysym_t symbol = xkb_state_key_get_one_sym(app->xkb_state, key + 8);
	if (symbol == XKB_KEY_Escape) {
		app->exit_status = EXIT_FAILURE;
		app->running = false;
	}
}

static void keyboard_modifiers(void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
	(void)keyboard;
	(void)serial;

	struct Rogdrop* app = data;
	if (!app->xkb_state) {
		return;
	}

	xkb_state_update_mask(app->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void keyboard_repeat_info(void* data, struct wl_keyboard* keyboard, int32_t rate, int32_t delay) {
	(void)data;
	(void)keyboard;
	(void)rate;
	(void)delay;
}

static const struct wl_keyboard_listener KEYBOARD_LISTENER = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

void input_handle_seat_capabilities(struct Rogdrop* app, uint32_t capabilities) {
	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {
		app->pointer = wl_seat_get_pointer(app->seat);
		wl_pointer_add_listener(app->pointer, &POINTER_LISTENER, app);
	}
	else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && app->pointer) {
		wl_pointer_destroy(app->pointer);
		app->pointer = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) {
		app->keyboard = wl_seat_get_keyboard(app->seat);
		wl_keyboard_add_listener(app->keyboard, &KEYBOARD_LISTENER, app);
	}
	else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && app->keyboard) {
		wl_keyboard_destroy(app->keyboard);
		app->keyboard = NULL;
	}
}

void input_destroy(struct Rogdrop* app) {
	// destroy the cursor before removing compositor state
	shm_buffer_destroy(&app->cursor_buffer);
	if (app->cursor_surface) {
		wl_surface_destroy(app->cursor_surface);
	}
	app->cursor_surface = NULL;

	if (app->pointer) {
		wl_pointer_destroy(app->pointer);
	}
	if (app->keyboard) {
		wl_keyboard_destroy(app->keyboard);
	}
	if (app->xkb_state) {
		xkb_state_unref(app->xkb_state);
	}
	if (app->xkb_keymap) {
		xkb_keymap_unref(app->xkb_keymap);
	}
	if (app->xkb_context) {
		xkb_context_unref(app->xkb_context);
	}

	app->pointer = NULL;
	app->keyboard = NULL;
	app->xkb_state = NULL;
	app->xkb_keymap = NULL;
	app->xkb_context = NULL;
}
