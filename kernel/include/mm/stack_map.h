#ifndef KERNEL_STACK_MAP_H
#define KERNEL_STACK_MAP_H

#include <stdint.h>

void map_high_stack(uint32_t stack_bottom, uint32_t stack_top, uint32_t flags);

#endif /* KERNEL_STACK_MAP_H */
