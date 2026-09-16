#ifndef TTY_RECIPE_H
#define TTY_RECIPE_H

#include "drivers/tty_pipeline.h"

/* Shared cooked mode recipes, one copy for whole machine */
extern const tty_pipeline_def_t tty_cooked_in_def;
extern const tty_pipeline_def_t tty_cooked_out_def;

#endif /* TTY_RECIPE_H */
