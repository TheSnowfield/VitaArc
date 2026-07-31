#ifndef __PLATFORM_INPUT_H
#define __PLATFORM_INPUT_H

#include <stdbool.h>

bool vita_input_initialize(void);
void vita_input_poll(void);
bool vita_input_is_paused(void);

#endif /* __PLATFORM_INPUT_H */
