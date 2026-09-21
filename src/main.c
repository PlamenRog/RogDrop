#include "capture.h"
#include "overlay.h"
#include "rogdrop.h"
#include "wayland.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
	struct Rogdrop app = {
	    .running = true,
	    .exit_status = EXIT_FAILURE,
	};

	if (!wayland_init(&app)) {
		wayland_destroy(&app);
		return EXIT_FAILURE;
	}

	bool have_output = false;
	for (struct Output* output = app.outputs; output; output = output->next) {
		have_output = true;

		if (!overlay_create(output)) {
			fprintf(stderr, "Failed to create overlay for output %s\n", output->name);
			goto out;
		}

		if (!capture_start_output(output)) {
			fprintf(stderr, "Failed to start capture for output %s\n", output->name);
			goto out;
		}
	}

	if (!have_output) {
		fprintf(stderr, "No Wayland outputs are available\n");
		goto out;
	}

	wl_display_flush(app.display);

	while (app.running) {
		if (wl_display_dispatch(app.display) < 0) {
			fprintf(stderr, "Wayland connection closed\n");
			app.exit_status = EXIT_FAILURE;
			break;
		}
	}

out:
	wayland_destroy(&app);
	return app.exit_status;
}
