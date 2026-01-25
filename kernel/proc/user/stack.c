#include "mm/paging.h"
#include "mm/stack_map.h"
#include "proc/proc.h"
#include "proc/user.h"

int user_stack_init(pcb_t *proc) {
    if (!proc) {
        return -1;
    }

    map_high_stack(USER_STACK_BOTTOM_VIRT, USER_STACK_TOP_VIRT,
                   PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    proc->user_stack_top = USER_STACK_TOP_VIRT;
    proc->user_stack_size = USER_SPACE_STACK_SIZE;

    return 0;
}
