#ifndef ROGDROP_CAPTURE_H
#define ROGDROP_CAPTURE_H

#include "rogdrop.h"

bool capture_start_output(struct Output* output);
void capture_stop_output(struct Output* output);

bool capture_sample_lens(struct Output* output, double pointer_x, double pointer_y, uint8_t pixels[MAG_RGB_SIZE], uint8_t* center_r, uint8_t* center_g, uint8_t* center_b);

const char* capture_backend_name(enum CaptureBackend backend);

#endif
