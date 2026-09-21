#ifndef ROGDROP_INPUT_H
#define ROGDROP_INPUT_H

#include "rogdrop.h"

void input_handle_seat_capabilities(struct Rogdrop* app, uint32_t capabilities);
void input_destroy(struct Rogdrop* app);

#endif
