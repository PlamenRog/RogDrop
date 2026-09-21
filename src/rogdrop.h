#ifndef ROGDROP_H
#define ROGDROP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

#define ROGDROP_NAMESPACE "rogdrop"

#define MAG_PIXELS 15
#define CELL_SIZE 10
#define MAG_SIZE (MAG_PIXELS * CELL_SIZE)
#define MAG_OFFSET 24
#define MAG_RGB_SIZE (MAG_PIXELS * MAG_PIXELS * 3)

struct Rogdrop;
struct Output;

enum CaptureBackend {
	CAPTURE_BACKEND_NONE = 0,
	CAPTURE_BACKEND_EXT,
	CAPTURE_BACKEND_WLR,
};

struct ShmBuffer {
	struct Rogdrop* app;
	struct wl_buffer* wl_buffer;
	void* data;
	size_t size;
	int width;
	int height;
	int stride;
	uint32_t format;
	bool busy;
	void* user_data;
	void (*release_cb)(struct ShmBuffer* buffer);
};

struct Output {
	struct Rogdrop* app;
	struct Output* next;

	uint32_t registry_name;
	struct wl_output* wl_output;
	char name[128];
	int scale;

	struct wl_surface* surface;
	struct zwlr_layer_surface_v1* layer_surface;
	int surface_width;
	int surface_height;
	bool configured;
	bool overlay_redraw_pending;

	struct ShmBuffer overlay[2];

	enum CaptureBackend backend;

	struct ext_image_capture_source_v1* ext_source;
	struct ext_image_copy_capture_session_v1* ext_session;
	struct ext_image_copy_capture_frame_v1* ext_frame;
	bool ext_batch_open;
	bool ext_reconfigure;
	bool ext_stopped;

	struct zwlr_screencopy_frame_v1* wlr_frame;

	uint32_t pending_width;
	uint32_t pending_height;
	uint32_t pending_format;
	uint32_t pending_stride;
	bool pending_format_valid;

	struct ShmBuffer capture[2];
	int capture_front;
	int capture_back;
	bool capture_valid;

	uint32_t capture_transform;
	bool y_invert;
};

struct Rogdrop {
	struct wl_display* display;
	struct wl_registry* registry;
	struct wl_compositor* compositor;
	struct wl_shm* shm;

	struct wl_seat* seat;
	struct wl_pointer* pointer;
	struct wl_keyboard* keyboard;

	struct wl_surface* cursor_surface;
	struct ShmBuffer cursor_buffer;

	struct zwlr_layer_shell_v1* layer_shell;

	struct ext_output_image_capture_source_manager_v1* ext_source_manager;
	struct ext_image_copy_capture_manager_v1* ext_capture_manager;
	struct zwlr_screencopy_manager_v1* wlr_screencopy_manager;
	uint32_t wlr_screencopy_version;

	struct xkb_context* xkb_context;
	struct xkb_keymap* xkb_keymap;
	struct xkb_state* xkb_state;

	struct Output* outputs;
	struct Output* active_output;

	double pointer_x;
	double pointer_y;
	bool pointer_valid;

	bool running;
	int exit_status;
};

#endif
