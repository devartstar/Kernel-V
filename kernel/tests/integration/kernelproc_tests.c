#include "core/panik.h"
#include "lib/printk.h"
#include "proc/proc.h"
#include "tests/proc_tests.h"

static volatile int test_processes_remaining = 3;

void test_process_finished(void) { test_processes_remaining--; }

void my_test_proc(void *arg) {
    (void)arg;
    int max_runs = 3;

    for (int i = 0; i < max_runs; i++) {
        KLOG_VERBOSE("TEST", "Process %s is running! iteration %d/%d\n",
                     current_proc->name, i + 1, max_runs);
        yield();
    }

    KLOG_VERBOSE("TEST", "[EXIT PROCESS] Process %s finished, terminating\n",
                 current_proc->name);
    test_process_finished();
}

void my_sleep_proc(void *arg) {
    (void)arg;
    int i = 0;
    int max_sleep_cycles = 2;

    KLOG_VERBOSE("TEST", "Sleep process starting with %d cycles\n",
                 max_sleep_cycles);

    while (i < max_sleep_cycles) {
        KLOG_VERBOSE("TEST", "Thread %s about to sleep cycle %d, state=%d\n",
                     current_proc->name, i, current_proc->state);
        proc_sleep(20);
        i++;
    }

    KLOG_VERBOSE("TEST",
                 "[EXIT PROCESS] Process %s finished, terminating (completed "
                 "%d cycles)\n",
                 current_proc->name, i);
    test_process_finished();
}

void preemptive_proc(void *args) {
    (void)args;
    int i = 0;
    int max_iterations = 1000;

    while (i < max_iterations) {
        if (i % 100 == 0) {
            KLOG_VERBOSE("TEST", "Thread %s is running, for [%d / %d]\n",
                         current_proc->name, i / 100, max_iterations / 100);
        }
        i++;
    }

    KLOG_VERBOSE(
        "TEST",
        "[EXIT PROCESS] Process %s finished after %d iterations, terminating\n",
        current_proc->name, max_iterations);
    test_process_finished();
}

void create_test_processes(void) {
    pcb_t *test_proc1 = proc_create(my_test_proc, NULL, "thread1");
    pcb_t *test_proc2 = proc_create(preemptive_proc, NULL, "thread2");
    pcb_t *test_proc3 = proc_create(my_sleep_proc, NULL, "thread3");

    proc_set_type(test_proc1, PROC_TYPE_KERNEL);
    proc_set_type(test_proc2, PROC_TYPE_KERNEL);
    proc_set_type(test_proc3, PROC_TYPE_KERNEL);

    proc_mark_ready(test_proc1);
    proc_mark_ready(test_proc2);
    proc_mark_ready(test_proc3);

    if (test_proc1 && test_proc2 && test_proc3) {
        KLOG_VERBOSE("TEST", "Test processes created successfully\n");

        extern volatile uint32_t tick_count;
        uint32_t start_ticks = tick_count;
        KLOG_VERBOSE("TEST", "Starting wait loop at tick_count = %u\n",
                     start_ticks);

        while (test_processes_remaining > 0) {
            KLOG_VERBOSE("TEST",
                         "Waiting for test processes to finish: %d remaining "
                         "(ticks=%u)\n",
                         test_processes_remaining, tick_count);

            /* todo(remove): Check if interrupts are still enabled for TIMER */
            uint32_t eflags;
            __asm__ __volatile__("pushf; pop %0" : "=r"(eflags));
            if (!(eflags & 0x200)) {
                KLOG_VERBOSE("TEST", "ERROR: Interrupts are DISABLED in "
                                     "waiting loop! ENABLING\n");
                __asm__ __volatile__("sti");
            }
            __asm__ volatile("hlt");
        }
    } else {
        KLOG_VERBOSE("TEST", "Failed to create test processes!\n");
    }
}
