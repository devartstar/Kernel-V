#include "time/timer.h"
#include "core/io.h"
#include "lib/printk.h"
#include "proc/proc.h"
#include <stdint.h>

volatile uint32_t tick_count = 0;

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_FREQ / hz;

    /* Setting value to I/O port 0x43
     0x36 - 0011 0110
     Channel		(bits 6-7) - 00  - channel 0
     Access Mode	(bits 4-5) - 11	 - lobyte/hibyte
     Operating Mode	(bits 1-3) - 011 - square wave generator
    */
    outb(0x43, 0x36);

    /* Access mode - 11 Two consecutive writes
     First write is low 8 bits
     Next write is high 8 bits
    */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 0x8) & 0xFF));

    pr_info("[PIT] Initialized Successfully at %u Hz\n", PRINT_UINT32(hz));
}

void timer_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    static int first_call = 1;
    if (first_call) {
        pr_info("[TIMER] First timer interrupt received!\n");
        first_call = 0;
    }

    tick_count++;

    // More verbose debugging
    if (tick_count % 1000 == 0) { // Every 5 ticks
        pr_info("[TIMER] Tick %u\n", PRINT_UINT32(tick_count));
    }

    /* Handle waking up sleeping process on timer interrupt */
    timer_interrupt_proc_handler(tick_count);
}
