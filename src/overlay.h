#ifndef ROGDROP_OVERLAY_H
#define ROGDROP_OVERLAY_H

#include "rogdrop.h"

bool overlay_create(struct Output* output);
void overlay_destroy(struct Output* output);
void overlay_redraw(struct Output* output);
void overlay_redraw_active(struct Rogdrop* app);
struct Output* overlay_output_for_surface(struct Rogdrop* app, struct wl_surface* surface);

#endif
