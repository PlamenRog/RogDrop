#include "capture.h"

#include "overlay.h"
#include "shm.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static const char* shm_format_name(uint32_t format) {
	switch (format) {
		case WL_SHM_FORMAT_RGB888:
			return "RGB888";
		case WL_SHM_FORMAT_BGR888:
			return "BGR888";
		case WL_SHM_FORMAT_XRGB8888:
			return "XRGB8888";
		case WL_SHM_FORMAT_ARGB8888:
			return "ARGB8888";
		case WL_SHM_FORMAT_XBGR8888:
			return "XBGR8888";
		case WL_SHM_FORMAT_ABGR8888:
			return "ABGR8888";
		case WL_SHM_FORMAT_RGBX8888:
			return "RGBX8888";
		case WL_SHM_FORMAT_RGBA8888:
			return "RGBA8888";
		case WL_SHM_FORMAT_BGRX8888:
			return "BGRX8888";
		case WL_SHM_FORMAT_BGRA8888:
			return "BGRA8888";
		case WL_SHM_FORMAT_XRGB2101010:
			return "XRGB2101010";
		case WL_SHM_FORMAT_ARGB2101010:
			return "ARGB2101010";
		case WL_SHM_FORMAT_XBGR2101010:
			return "XBGR2101010";
		case WL_SHM_FORMAT_ABGR2101010:
			return "ABGR2101010";
		case WL_SHM_FORMAT_RGBX1010102:
			return "RGBX1010102";
		case WL_SHM_FORMAT_RGBA1010102:
			return "RGBA1010102";
		case WL_SHM_FORMAT_BGRX1010102:
			return "BGRX1010102";
		case WL_SHM_FORMAT_BGRA1010102:
			return "BGRA1010102";
		default:
			return "unknown";
	}
}

static bool shm_format_supported(uint32_t format) {
	switch (format) {
		case WL_SHM_FORMAT_RGB888:
		case WL_SHM_FORMAT_BGR888:
		case WL_SHM_FORMAT_XRGB8888:
		case WL_SHM_FORMAT_ARGB8888:
		case WL_SHM_FORMAT_XBGR8888:
		case WL_SHM_FORMAT_ABGR8888:
		case WL_SHM_FORMAT_RGBX8888:
		case WL_SHM_FORMAT_RGBA8888:
		case WL_SHM_FORMAT_BGRX8888:
		case WL_SHM_FORMAT_BGRA8888:
		case WL_SHM_FORMAT_XRGB2101010:
		case WL_SHM_FORMAT_ARGB2101010:
		case WL_SHM_FORMAT_XBGR2101010:
		case WL_SHM_FORMAT_ABGR2101010:
		case WL_SHM_FORMAT_RGBX1010102:
		case WL_SHM_FORMAT_RGBA1010102:
		case WL_SHM_FORMAT_BGRX1010102:
		case WL_SHM_FORMAT_BGRA1010102:
			return true;
		default:
			return false;
	}
}

static int shm_format_bytes_per_pixel(uint32_t format) {
	switch (format) {
		case WL_SHM_FORMAT_RGB888:
		case WL_SHM_FORMAT_BGR888:
			return 3;

		case WL_SHM_FORMAT_XRGB8888:
		case WL_SHM_FORMAT_ARGB8888:
		case WL_SHM_FORMAT_XBGR8888:
		case WL_SHM_FORMAT_ABGR8888:
		case WL_SHM_FORMAT_RGBX8888:
		case WL_SHM_FORMAT_RGBA8888:
		case WL_SHM_FORMAT_BGRX8888:
		case WL_SHM_FORMAT_BGRA8888:
		case WL_SHM_FORMAT_XRGB2101010:
		case WL_SHM_FORMAT_ARGB2101010:
		case WL_SHM_FORMAT_XBGR2101010:
		case WL_SHM_FORMAT_ABGR2101010:
		case WL_SHM_FORMAT_RGBX1010102:
		case WL_SHM_FORMAT_RGBA1010102:
		case WL_SHM_FORMAT_BGRX1010102:
		case WL_SHM_FORMAT_BGRA1010102:
			return 4;

		default:
			return 0;
	}
}

static uint8_t component_10_to_8(uint32_t value) {
	return (uint8_t)((value * 255u + 511u) / 1023u);
}

static bool read_rgb_pixel(uint32_t format, const uint8_t* pixel, uint8_t* r, uint8_t* g, uint8_t* b) {
	switch (format) {
		case WL_SHM_FORMAT_RGB888:
			*b = pixel[0];
			*g = pixel[1];
			*r = pixel[2];
			return true;

		case WL_SHM_FORMAT_BGR888:
			*r = pixel[0];
			*g = pixel[1];
			*b = pixel[2];
			return true;

		case WL_SHM_FORMAT_XRGB8888:
		case WL_SHM_FORMAT_ARGB8888:
			*b = pixel[0];
			*g = pixel[1];
			*r = pixel[2];
			return true;

		case WL_SHM_FORMAT_XBGR8888:
		case WL_SHM_FORMAT_ABGR8888:
			*r = pixel[0];
			*g = pixel[1];
			*b = pixel[2];
			return true;

		case WL_SHM_FORMAT_RGBX8888:
		case WL_SHM_FORMAT_RGBA8888:
			*b = pixel[1];
			*g = pixel[2];
			*r = pixel[3];
			return true;

		case WL_SHM_FORMAT_BGRX8888:
		case WL_SHM_FORMAT_BGRA8888:
			*r = pixel[1];
			*g = pixel[2];
			*b = pixel[3];
			return true;

		case WL_SHM_FORMAT_XRGB2101010:
		case WL_SHM_FORMAT_ARGB2101010: {
			uint32_t value;
			memcpy(&value, pixel, sizeof(value));
			*r = component_10_to_8((value >> 20) & 0x3ffu);
			*g = component_10_to_8((value >> 10) & 0x3ffu);
			*b = component_10_to_8(value & 0x3ffu);
			return true;
		}

		case WL_SHM_FORMAT_XBGR2101010:
		case WL_SHM_FORMAT_ABGR2101010: {
			uint32_t value;
			memcpy(&value, pixel, sizeof(value));
			*b = component_10_to_8((value >> 20) & 0x3ffu);
			*g = component_10_to_8((value >> 10) & 0x3ffu);
			*r = component_10_to_8(value & 0x3ffu);
			return true;
		}

		case WL_SHM_FORMAT_RGBX1010102:
		case WL_SHM_FORMAT_RGBA1010102: {
			uint32_t value;
			memcpy(&value, pixel, sizeof(value));
			*r = component_10_to_8((value >> 22) & 0x3ffu);
			*g = component_10_to_8((value >> 12) & 0x3ffu);
			*b = component_10_to_8((value >> 2) & 0x3ffu);
			return true;
		}

		case WL_SHM_FORMAT_BGRX1010102:
		case WL_SHM_FORMAT_BGRA1010102: {
			uint32_t value;
			memcpy(&value, pixel, sizeof(value));
			*b = component_10_to_8((value >> 22) & 0x3ffu);
			*g = component_10_to_8((value >> 12) & 0x3ffu);
			*r = component_10_to_8((value >> 2) & 0x3ffu);
			return true;
		}

		default:
			return false;
	}
}

static void destroy_capture_buffers(struct Output* output) {
	shm_buffer_destroy(&output->capture[0]);
	shm_buffer_destroy(&output->capture[1]);
	output->capture_valid = false;
	output->capture_front = 0;
	output->capture_back = 1;
}

static bool allocate_capture_buffers(struct Output* output, uint32_t width, uint32_t height, uint32_t stride, uint32_t format) {
	int bytes_per_pixel = shm_format_bytes_per_pixel(format);
	if (!width || !height || !stride || bytes_per_pixel == 0) {
		return false;
	}

	if ((uint64_t)stride < (uint64_t)width * (uint64_t)bytes_per_pixel) {
		return false;
	}

	destroy_capture_buffers(output);

	for (size_t i = 0; i < 2; ++i) {
		if (!shm_buffer_create(output->app, &output->capture[i], (int)width, (int)height, (int)stride, format, NULL)) {
			destroy_capture_buffers(output);
			return false;
		}
	}

	output->capture_front = 0;
	output->capture_back = 1;
	output->capture_valid = false;
	return true;
}

static void swap_capture_buffers(struct Output* output) {
	int completed = output->capture_back;
	output->capture_front = completed;
	output->capture_back = completed == 0 ? 1 : 0;
	output->capture_valid = true;
}

static void map_pointer_to_capture(const struct Output* output, double pointer_x, double pointer_y, int* capture_x, int* capture_y) {
	const struct ShmBuffer* buffer = &output->capture[output->capture_front];

	double u = output->surface_width > 0 ? pointer_x / (double)output->surface_width : 0.0;
	double v = output->surface_height > 0 ? pointer_y / (double)output->surface_height : 0.0;

	if (u < 0.0) {
		u = 0.0;
	}
	if (u > 1.0) {
		u = 1.0;
	}
	if (v < 0.0) {
		v = 0.0;
	}
	if (v > 1.0) {
		v = 1.0;
	}

	double bx = u;
	double by = v;

	switch (output->capture_transform) {
		case WL_OUTPUT_TRANSFORM_90:
			bx = v;
			by = 1.0 - u;
			break;
		case WL_OUTPUT_TRANSFORM_180:
			bx = 1.0 - u;
			by = 1.0 - v;
			break;
		case WL_OUTPUT_TRANSFORM_270:
			bx = 1.0 - v;
			by = u;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED:
			bx = 1.0 - u;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED_90:
			bx = 1.0 - v;
			by = 1.0 - u;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED_180:
			by = 1.0 - v;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED_270:
			bx = v;
			by = u;
			break;
		case WL_OUTPUT_TRANSFORM_NORMAL:
		default:
			break;
	}

	int x = (int)(bx * buffer->width);
	int y = (int)(by * buffer->height);

	if (x < 0) {
		x = 0;
	}
	if (y < 0) {
		y = 0;
	}
	if (x >= buffer->width) {
		x = buffer->width - 1;
	}
	if (y >= buffer->height) {
		y = buffer->height - 1;
	}

	if (output->y_invert) {
		y = buffer->height - 1 - y;
	}

	*capture_x = x;
	*capture_y = y;
}

bool capture_sample_lens(struct Output* output, double pointer_x, double pointer_y, uint8_t pixels[MAG_RGB_SIZE], uint8_t* center_r, uint8_t* center_g, uint8_t* center_b) {
	if (!output || !output->capture_valid || !output->configured) {
		return false;
	}

	const struct ShmBuffer* buffer = &output->capture[output->capture_front];
	int bytes_per_pixel = shm_format_bytes_per_pixel(buffer->format);
	if (!buffer->data || buffer->stride <= 0 || bytes_per_pixel == 0) {
		return false;
	}

	int selected_x;
	int selected_y;
	map_pointer_to_capture(output, pointer_x, pointer_y, &selected_x, &selected_y);

	const int radius = MAG_PIXELS / 2;

	for (int lens_y = 0; lens_y < MAG_PIXELS; ++lens_y) {
		int source_y = selected_y + lens_y - radius;
		if (source_y < 0) {
			source_y = 0;
		}
		if (source_y >= buffer->height) {
			source_y = buffer->height - 1;
		}

		const uint8_t* row = (const uint8_t*)buffer->data + (size_t)source_y * (size_t)buffer->stride;

		for (int lens_x = 0; lens_x < MAG_PIXELS; ++lens_x) {
			int source_x = selected_x + lens_x - radius;
			if (source_x < 0) {
				source_x = 0;
			}
			if (source_x >= buffer->width) {
				source_x = buffer->width - 1;
			}

			const uint8_t* source = row + (size_t)source_x * (size_t)bytes_per_pixel;
			size_t destination = ((size_t)lens_y * MAG_PIXELS + (size_t)lens_x) * 3;

			if (!read_rgb_pixel(buffer->format, source, &pixels[destination + 0], &pixels[destination + 1], &pixels[destination + 2])) {
				return false;
			}
		}
	}

	const int center = MAG_PIXELS / 2;
	size_t index = ((size_t)center * MAG_PIXELS + (size_t)center) * 3;
	*center_r = pixels[index + 0];
	*center_g = pixels[index + 1];
	*center_b = pixels[index + 2];
	return true;
}

static void ext_request_frame(struct Output* output);

static void ext_begin_constraint_batch(struct Output* output) {
	if (output->ext_batch_open) {
		return;
	}

	output->ext_batch_open = true;
	output->pending_width = 0;
	output->pending_height = 0;
	output->pending_stride = 0;
	output->pending_format = 0;
	output->pending_format_valid = false;
}

static void
ext_session_buffer_size(void* data, struct ext_image_copy_capture_session_v1* session, uint32_t width, uint32_t height) {
	(void)session;
	struct Output* output = data;
	ext_begin_constraint_batch(output);
	output->pending_width = width;
	output->pending_height = height;
}

static void
ext_session_shm_format(void* data, struct ext_image_copy_capture_session_v1* session, uint32_t format) {
	(void)session;
	struct Output* output = data;
	ext_begin_constraint_batch(output);

	if (shm_format_supported(format)) {
		if (!output->pending_format_valid) {
			output->pending_format = format;
			output->pending_format_valid = true;
		}
	}
	else {
		fprintf(stderr, "%s: ignoring unsupported ext SHM format %s (0x%08x)\n",
		        output->name, shm_format_name(format), format);
	}
}

static void
ext_session_dmabuf_device(void* data, struct ext_image_copy_capture_session_v1* session, struct wl_array* device) {
	(void)session;
	(void)device;
	ext_begin_constraint_batch(data);
}

static void
ext_session_dmabuf_format(void* data, struct ext_image_copy_capture_session_v1* session, uint32_t format, struct wl_array* modifiers) {
	(void)session;
	(void)format;
	(void)modifiers;
	ext_begin_constraint_batch(data);
}

static bool ext_apply_constraints(struct Output* output) {
	if (!output->pending_width || !output->pending_height || !output->pending_format_valid) {
		return false;
	}

	int bytes_per_pixel = shm_format_bytes_per_pixel(output->pending_format);
	if (bytes_per_pixel == 0 || output->pending_width > UINT32_MAX / (uint32_t)bytes_per_pixel) {
		return false;
	}

	uint32_t stride = output->pending_width * (uint32_t)bytes_per_pixel;
	if (!allocate_capture_buffers(output, output->pending_width, output->pending_height, stride, output->pending_format)) {
		return false;
	}

	output->capture_transform = WL_OUTPUT_TRANSFORM_NORMAL;
	output->y_invert = false;
	output->ext_reconfigure = false;
	return true;
}

static bool start_wlr_backend(struct Output* output);

static void
ext_session_done(void* data, struct ext_image_copy_capture_session_v1* session) {
	(void)session;
	struct Output* output = data;
	output->ext_batch_open = false;

	if (!output->pending_format_valid) {
		fprintf(stderr, "%s: ext-image-copy-capture offered no supported SHM format", output->name);

		if (output->app->wlr_screencopy_manager) {
			fprintf(stderr, "; falling back to wlr-screencopy\n");
			capture_stop_output(output);
			start_wlr_backend(output);
		}
		else {
			fprintf(stderr, "\n");
			output->app->running = false;
			output->app->exit_status = 1;
		}
		return;
	}

	if (output->ext_frame) {
		output->ext_reconfigure = true;
		return;
	}

	if (!ext_apply_constraints(output)) {
		fprintf(stderr, "%s: failed to allocate capture buffers\n", output->name);
		output->app->running = false;
		output->app->exit_status = 1;
		return;
	}

	ext_request_frame(output);
}

static void
ext_session_stopped(void* data, struct ext_image_copy_capture_session_v1* session) {
	(void)session;
	struct Output* output = data;
	output->ext_stopped = true;
	fprintf(stderr, "%s: capture session stopped\n", output->name);
	output->app->running = false;
	output->app->exit_status = 1;
}

static const struct ext_image_copy_capture_session_v1_listener
    EXT_SESSION_LISTENER = {
        .buffer_size = ext_session_buffer_size,
        .shm_format = ext_session_shm_format,
        .dmabuf_device = ext_session_dmabuf_device,
        .dmabuf_format = ext_session_dmabuf_format,
        .done = ext_session_done,
        .stopped = ext_session_stopped,
};

static void ext_frame_transform(void* data, struct ext_image_copy_capture_frame_v1* frame, uint32_t transform) {
	(void)frame;
	struct Output* output = data;
	output->capture_transform = transform;
}

static void ext_frame_damage(void* data, struct ext_image_copy_capture_frame_v1* frame, int32_t x, int32_t y, int32_t width, int32_t height) {
	(void)data;
	(void)frame;
	(void)x;
	(void)y;
	(void)width;
	(void)height;
}

static void ext_frame_presentation_time(
    void* data, struct ext_image_copy_capture_frame_v1* frame,
    uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
	(void)data;
	(void)frame;
	(void)tv_sec_hi;
	(void)tv_sec_lo;
	(void)tv_nsec;
}

static void ext_frame_ready(void* data, struct ext_image_copy_capture_frame_v1* frame) {
	struct Output* output = data;

	ext_image_copy_capture_frame_v1_destroy(frame);
	output->ext_frame = NULL;
	swap_capture_buffers(output);

	if (output->app->active_output == output) {
		overlay_redraw(output);
	}

	if (output->ext_reconfigure) {
		if (!ext_apply_constraints(output)) {
			fprintf(stderr, "%s: failed to apply new capture constraints\n", output->name);
			output->app->running = false;
			output->app->exit_status = 1;
			return;
		}
	}

	if (!output->ext_stopped) {
		ext_request_frame(output);
	}
}

static void ext_frame_failed(void* data, struct ext_image_copy_capture_frame_v1* frame, uint32_t reason) {
	struct Output* output = data;
	ext_image_copy_capture_frame_v1_destroy(frame);
	output->ext_frame = NULL;

	if (reason == EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED) {
		output->app->running = false;
		output->app->exit_status = 1;
		return;
	}

	if (output->ext_reconfigure) {
		if (!ext_apply_constraints(output)) {
			fprintf(stderr, "%s: capture constraints changed to an unsupported format\n", output->name);
			output->app->running = false;
			output->app->exit_status = 1;
			return;
		}
	}

	ext_request_frame(output);
}

static const struct ext_image_copy_capture_frame_v1_listener
    EXT_FRAME_LISTENER = {
        .transform = ext_frame_transform,
        .damage = ext_frame_damage,
        .presentation_time = ext_frame_presentation_time,
        .ready = ext_frame_ready,
        .failed = ext_frame_failed,
};

static void ext_request_frame(struct Output* output) {
	if (!output->ext_session || output->ext_frame || output->ext_stopped) {
		return;
	}

	struct ShmBuffer* buffer = &output->capture[output->capture_back];
	if (!buffer->wl_buffer) {
		return;
	}

	output->ext_frame = ext_image_copy_capture_session_v1_create_frame(output->ext_session);
	ext_image_copy_capture_frame_v1_add_listener(output->ext_frame, &EXT_FRAME_LISTENER, output);

	ext_image_copy_capture_frame_v1_attach_buffer(output->ext_frame, buffer->wl_buffer);
	ext_image_copy_capture_frame_v1_damage_buffer(output->ext_frame, 0, 0, buffer->width, buffer->height);
	ext_image_copy_capture_frame_v1_capture(output->ext_frame);
}

static bool start_ext_backend(struct Output* output) {
	struct Rogdrop* app = output->app;

	output->backend = CAPTURE_BACKEND_EXT;
	output->ext_source = ext_output_image_capture_source_manager_v1_create_source(app->ext_source_manager, output->wl_output);
	if (!output->ext_source) {
		return false;
	}

	output->ext_session = ext_image_copy_capture_manager_v1_create_session( app->ext_capture_manager, output->ext_source, 0);
	if (!output->ext_session) {
		return false;
	}

	ext_image_copy_capture_session_v1_add_listener(output->ext_session, &EXT_SESSION_LISTENER, output);

	return true;
}

static void wlr_request_frame(struct Output* output);

static bool wlr_prepare_buffers(struct Output* output) {
	if (!output->pending_width || !output->pending_height ||
	    !output->pending_stride || !output->pending_format_valid) {
		return false;
	}

	struct ShmBuffer* existing = &output->capture[0];
	if (existing->wl_buffer && existing->width == (int)output->pending_width && existing->height == (int)output->pending_height && existing->stride == (int)output->pending_stride && existing->format == output->pending_format) {
		return true;
	}

	return allocate_capture_buffers(output, output->pending_width, output->pending_height, output->pending_stride, output->pending_format);
}

static void wlr_copy_frame(struct Output* output) {
	if (!wlr_prepare_buffers(output)) {
		fprintf(stderr, "%s: unsupported wlr-screencopy SHM format\n", output->name);
		output->app->running = false;
		output->app->exit_status = 1;
		return;
	}

	struct ShmBuffer* buffer = &output->capture[output->capture_back];
	zwlr_screencopy_frame_v1_copy(output->wlr_frame, buffer->wl_buffer);
}

static void wlr_frame_buffer(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
	(void)frame;
	struct Output* output = data;

	output->pending_width = width;
	output->pending_height = height;
	output->pending_stride = stride;
	output->pending_format = format;
	output->pending_format_valid = shm_format_supported(format);

	if (!output->pending_format_valid) {
		fprintf(stderr, "%s: wlr-screencopy offered unsupported SHM format\n", output->name);
	}

	wlr_copy_frame(output);
}

static void wlr_frame_flags(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t flags) {
	(void)frame;
	struct Output* output = data;
	output->y_invert = (flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
}

static void wlr_frame_ready(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
	(void)tv_sec_hi;
	(void)tv_sec_lo;
	(void)tv_nsec;

	struct Output* output = data;
	zwlr_screencopy_frame_v1_destroy(frame);
	output->wlr_frame = NULL;
	swap_capture_buffers(output);

	if (output->app->active_output == output) {
		overlay_redraw(output);
	}

	wlr_request_frame(output);
}

static void wlr_frame_failed(void* data, struct zwlr_screencopy_frame_v1* frame) {
	struct Output* output = data;
	zwlr_screencopy_frame_v1_destroy(frame);
	output->wlr_frame = NULL;
	fprintf(stderr, "%s: wlr-screencopy frame failed\n", output->name);
	output->app->running = false;
	output->app->exit_status = 1;
}

static const struct zwlr_screencopy_frame_v1_listener WLR_FRAME_LISTENER = {
    .buffer = wlr_frame_buffer,
    .flags = wlr_frame_flags,
    .ready = wlr_frame_ready,
    .failed = wlr_frame_failed,
};

static void wlr_request_frame(struct Output* output) {
	if (!output->app->wlr_screencopy_manager || output->wlr_frame) {
		return;
	}

	output->pending_width = 0;
	output->pending_height = 0;
	output->pending_stride = 0;
	output->pending_format_valid = false;
	output->capture_transform = WL_OUTPUT_TRANSFORM_NORMAL;

	output->wlr_frame = zwlr_screencopy_manager_v1_capture_output(output->app->wlr_screencopy_manager, 0, output->wl_output);

	zwlr_screencopy_frame_v1_add_listener(output->wlr_frame, &WLR_FRAME_LISTENER, output);
}

static bool start_wlr_backend(struct Output* output) {
	if (!output->app->wlr_screencopy_manager) {
		return false;
	}

	output->backend = CAPTURE_BACKEND_WLR;
	output->y_invert = false;
	wlr_request_frame(output);
	return true;
}

bool capture_start_output(struct Output* output) {
	output->capture_front = 0;
	output->capture_back = 1;
	output->capture_transform = WL_OUTPUT_TRANSFORM_NORMAL;

	if (output->app->ext_source_manager && output->app->ext_capture_manager) {
		return start_ext_backend(output);
	}

	if (output->app->wlr_screencopy_manager) {
		return start_wlr_backend(output);
	}

	fprintf(stderr, "Compositor exposes neither ext-image-copy-capture-v1 nor wlr-screencopy\n");
	return false;
}

void capture_stop_output(struct Output* output) {
	if (!output) {
		return;
	}

	if (output->ext_frame) {
		ext_image_copy_capture_frame_v1_destroy(output->ext_frame);
		output->ext_frame = NULL;
	}

	if (output->ext_session) {
		ext_image_copy_capture_session_v1_destroy(output->ext_session);
		output->ext_session = NULL;
	}

	if (output->ext_source) {
		ext_image_capture_source_v1_destroy(output->ext_source);
		output->ext_source = NULL;
	}

	if (output->wlr_frame) {
		zwlr_screencopy_frame_v1_destroy(output->wlr_frame);
		output->wlr_frame = NULL;
	}

	destroy_capture_buffers(output);
	output->backend = CAPTURE_BACKEND_NONE;
}

const char* capture_backend_name(enum CaptureBackend backend) {
	switch (backend) {
		case CAPTURE_BACKEND_EXT:
			return "ext-image-copy-capture-v1";
		case CAPTURE_BACKEND_WLR:
			return "wlr-screencopy";
		default:
			return "none";
	}
}
