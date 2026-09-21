#include "wayland.h"

#include "capture.h"
#include "input.h"
#include "overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t min_version(uint32_t offered, uint32_t wanted) {
	return offered < wanted ? offered : wanted;
}

static void output_geometry(void* data, struct wl_output* wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char* make, const char* model, int32_t transform) {
	(void)data;
	(void)wl_output;
	(void)x;
	(void)y;
	(void)physical_width;
	(void)physical_height;
	(void)subpixel;
	(void)make;
	(void)model;
	(void)transform;
}

static void output_mode(void* data, struct wl_output* wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	(void)data;
	(void)wl_output;
	(void)flags;
	(void)width;
	(void)height;
	(void)refresh;
}

static void output_done(void* data, struct wl_output* wl_output) {
	(void)data;
	(void)wl_output;
}

static void output_scale(void* data, struct wl_output* wl_output, int32_t factor) {
	(void)wl_output;
	struct Output* output = data;
	output->scale = factor > 0 ? factor : 1;
}

static void output_name(void* data, struct wl_output* wl_output, const char* name) {
	(void)wl_output;
	struct Output* output = data;
	if (name && *name) {
		snprintf(output->name, sizeof(output->name), "%s", name);
	}
}

static void output_description(void* data, struct wl_output* wl_output, const char* description) {
	(void)data;
	(void)wl_output;
	(void)description;
}

static const struct wl_output_listener OUTPUT_LISTENER = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void seat_capabilities(void* data, struct wl_seat* seat, uint32_t capabilities) {
	(void)seat;
	input_handle_seat_capabilities(data, capabilities);
}

static void seat_name(void* data, struct wl_seat* seat, const char* name) {
	(void)data;
	(void)seat;
	(void)name;
}

static const struct wl_seat_listener SEAT_LISTENER = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static struct Output* add_output(struct Rogdrop* app, struct wl_registry* registry, uint32_t name, uint32_t version) {
	struct Output* output = calloc(1, sizeof(*output));
	if (!output) {
		return NULL;
	}

	output->app = app;
	output->registry_name = name;
	output->scale = 1;
	output->capture_front = 0;
	output->capture_back = 1;
	output->capture_transform = WL_OUTPUT_TRANSFORM_NORMAL;
	snprintf(output->name, sizeof(output->name), "output-%u", name);

	output->wl_output = wl_registry_bind(registry, name, &wl_output_interface,
	                                     min_version(version, 4));

	if (!output->wl_output) {
		free(output);
		return NULL;
	}

	wl_output_add_listener(output->wl_output, &OUTPUT_LISTENER, output);
	output->next = app->outputs;
	app->outputs = output;

	return output;
}

static void registry_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
	struct Rogdrop* app = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		app->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, min_version(version, 4));
	}
	else if (strcmp(interface, wl_shm_interface.name) == 0) {
		app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	}
	else if (strcmp(interface, wl_output_interface.name) == 0) {
		add_output(app, registry, name, version);
	}
	else if (strcmp(interface, wl_seat_interface.name) == 0 && !app->seat) {
		app->seat = wl_registry_bind(registry, name, &wl_seat_interface, min_version(version, 7));
		wl_seat_add_listener(app->seat, &SEAT_LISTENER, app);
	}
	else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		app->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, min_version(version, 4));
	}
	else if (strcmp(interface, ext_output_image_capture_source_manager_v1_interface.name) == 0) {
		app->ext_source_manager = wl_registry_bind(registry, name, &ext_output_image_capture_source_manager_v1_interface, 1);
	}
	else if (strcmp(interface, ext_image_copy_capture_manager_v1_interface.name) == 0) {
		app->ext_capture_manager = wl_registry_bind(registry, name, &ext_image_copy_capture_manager_v1_interface, 1);
	}
	else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
		app->wlr_screencopy_version = 1;
		app->wlr_screencopy_manager = wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, app->wlr_screencopy_version);
	}
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
	(void)registry;
	struct Rogdrop* app = data;

	for (struct Output* output = app->outputs; output; output = output->next) {
		if (output->registry_name != name) {
			continue;
		}

		fprintf(stderr, "%s was removed\n", output->name);
		app->exit_status = EXIT_FAILURE;
		app->running = false;
		return;
	}
}

static const struct wl_registry_listener REGISTRY_LISTENER = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

bool wayland_init(struct Rogdrop* app) {
	app->display = wl_display_connect(NULL);
	if (!app->display) {
		fprintf(stderr, "Unable to connect to Wayland display\n");
		return false;
	}

	app->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!app->xkb_context) {
		fprintf(stderr, "Unable to create xkb context\n");
		return false;
	}

	app->registry = wl_display_get_registry(app->display);
	wl_registry_add_listener(app->registry, &REGISTRY_LISTENER, app);

	if (wl_display_roundtrip(app->display) < 0) {
		return false;
	}

	if (!app->compositor || !app->shm) {
		fprintf(stderr, "Compositor is missing core Wayland interfaces\n");
		return false;
	}

	if (!app->layer_shell) {
		fprintf(stderr, "Compositor does not support wlr-layer-shell\n");
		return false;
	}

	if ((!app->ext_source_manager || !app->ext_capture_manager) &&
	    !app->wlr_screencopy_manager) {
		fprintf(stderr, "Compositor supports neither ext-image-copy-capture-v1 "
		                "nor wlr-screencopy\n");
		return false;
	}

	if (!app->seat) {
		fprintf(stderr, "No Wayland seat is available\n");
		return false;
	}

	if (wl_display_roundtrip(app->display) < 0) {
		return false;
	}

	if (!app->pointer) {
		fprintf(stderr, "Wayland seat has no pointer\n");
		return false;
	}

	return true;
}

void wayland_destroy(struct Rogdrop* app) {
	struct Output* output = app->outputs;

	while (output) {
		struct Output* next = output->next;

		capture_stop_output(output);
		overlay_destroy(output);

		if (output->wl_output)
			wl_output_destroy(output->wl_output);

		free(output);
		output = next;
	}

	app->outputs = NULL;

	input_destroy(app);

	if (app->seat) {
		wl_seat_destroy(app->seat);
	}
	if (app->ext_capture_manager) {
		ext_image_copy_capture_manager_v1_destroy(app->ext_capture_manager);
	}
	if (app->ext_source_manager) {
		ext_output_image_capture_source_manager_v1_destroy(app->ext_source_manager);
	}
	if (app->wlr_screencopy_manager) {
		zwlr_screencopy_manager_v1_destroy(app->wlr_screencopy_manager);
	}
	if (app->layer_shell) {
		zwlr_layer_shell_v1_destroy(app->layer_shell);
	}
	if (app->shm) {
		wl_shm_destroy(app->shm);
	}
	if (app->compositor) {
		wl_compositor_destroy(app->compositor);
	}
	if (app->registry) {
		wl_registry_destroy(app->registry);
	}
	if (app->display) {
		wl_display_disconnect(app->display);
	}

	memset(app, 0, sizeof(*app));
}
