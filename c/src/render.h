#ifndef RENDER_H
#define RENDER_H

#include "proc.h"

void render_init(void);
void render_clear(void);
void render_table(const ProcessList *list, const char *filter, unsigned top);

#endif
