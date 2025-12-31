#pragma once
#include <stdint.h>

void interrupt_pagefault_handler(uint32_t idt_index, regs_t *regs);

