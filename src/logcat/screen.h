#ifndef VITAARC_LOG_SCREEN_H
#define VITAARC_LOG_SCREEN_H

#include <stdbool.h>

void log_screen_begin(void);

void log_screen_ready(void);

/* Returns false on Circle, unavailable display/input, or an empty version list. */
bool log_screen_select(const char* const* names, unsigned count, unsigned* selected);

/* Keep the final startup log visible until Circle is pressed. */
void log_screen_wait(void);

void log_screen_end(void);

#endif
