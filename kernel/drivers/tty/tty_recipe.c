#include "drivers/tty_recipe.h"
#include "drivers/tty_stage_canon.h"
#include "drivers/tty_stage_icrnl.h"
#include "drivers/tty_stage_onlcr.h"

static const tty_stage_def_t *const cooked_in_stages[] = {
    &tty_stage_icrnl_def,
    &tty_stage_canon_def,
};

const tty_pipeline_def_t tty_cooked_in_def = {
    .stages = cooked_in_stages,
    .count = 2,
};

static const tty_stage_def_t *const cooked_out_stages[] = {
    &tty_stage_onlcr_def,
};

const tty_pipeline_def_t tty_cooked_out_def = {
    .stages = cooked_out_stages,
    .count = 1,
};
