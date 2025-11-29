#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Base Hardware frequency */
#define PIT_FREQ 1193182
#define PIT_DEFAULT_HZ 100

/**
 * pit_init - Initialize the programable interrupt timer.
 *
 * @hz - frequency set to the timer ticks.
 * @return - void
 */
void pit_init(uint32_t hz);

/**
 * timer_interrupt_handler - Handler to handle timer interrupts
 * then the timer hits 0 - an isr interrupt is triggered
 *
 * @return - void
 */
void timer_interrupt_handler(void);

/**
 * tick_count - counter of the number of ticks
 */
extern volatile uint32_t tick_count;

#endif //  TIMER_H
