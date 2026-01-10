#include "tests/proc_tests.h"
#include "lib/printk.h"
#include "proc/proc.h"
#include "core/panik.h"

extern pcb_t *current_proc;

static volatile int test_processes_remaining = 3;

void test_process_finished(void) { test_processes_remaining--; }

void my_test_proc(void *arg) {
    (void)arg;
    int max_runs = 3;

    for (int i = 0; i < max_runs; i++) {
        pr_verbose("Process %s is running! iteration %d/%d\n",
                   current_proc->name, i + 1, max_runs);
        yield();
    }

    pr_verbose("[EXIT PROCESS] Process %s finished, terminating\n",
               current_proc->name);
    test_process_finished();
}

void my_sleep_proc(void *arg) {
    (void)arg;
    int i = 0;
    int max_sleep_cycles = 2; // Limit the sleep cycles

    pr_verbose("Sleep process starting with %d cycles\n", max_sleep_cycles);
    
    while (i < max_sleep_cycles) {
        pr_verbose("Thread %s about to sleep cycle %d, state=%d\n", 
                   current_proc->name, i, current_proc->state);
        proc_sleep(20);
        if(i==1) {
            printk("Thread %s woke up from cycle %d! state=%d\n", 
                   current_proc->name, i, current_proc->state);
        }
        i++;
    }
    pr_verbose("[EXIT PROCESS] Process %s finished, terminating (completed %d cycles)\n",
               current_proc->name, i);
    test_process_finished();
}

void preemptive_proc(void *args) {
    (void)args;
    int i = 0;
    int max_iterations = 100000;

    while (i < max_iterations) {
        if (i % 1000 == 0) {
            pr_verbose("Thread %s is running, for [%d / %d]\n",
                       current_proc->name, i / 1000, max_iterations / 1000);
        }
        i++;
    }

    pr_verbose(
        "[EXIT PROCESS] Process %s finished after %d iterations, terminating\n",
        current_proc->name, max_iterations);
    test_process_finished();
}

void create_test_processes(void) {
    pcb_t *test_proc1 = proc_create(my_test_proc, NULL, "thread1");
    pcb_t *test_proc2 = proc_create(preemptive_proc, NULL, "thread2");
    pcb_t *test_proc3 = proc_create(my_sleep_proc, NULL, "thread3");
    // Todo: Check for Preemptive followed by Preemptive process...

    if (test_proc1 && test_proc2 && test_proc3) {
        pr_verbose("Test processes created successfully\n");
        pr_verbose("Starting scheduler with idle process...\n");

        // Let processes run and wait for them to complete
        yield(); // Start the processes
        // Add this before your while loop to get baseline
        extern volatile uint32_t tick_count;
        uint32_t start_ticks = tick_count;
        pr_verbose("Starting wait loop at tick_count = %u\n", start_ticks);

        while (test_processes_remaining > 0) {
            pr_verbose("Waiting for test processes to finish: %d remaining (ticks=%u)\n",
                    test_processes_remaining, tick_count);
            
            // Check if interrupts are enabled
            uint32_t eflags;
            __asm__ __volatile__("pushf; pop %0" : "=r"(eflags));
            if (!(eflags & 0x200)) {
                pr_verbose("ERROR: Interrupts are DISABLED in waiting loop!\n");
            }
            
            // Small delay to prevent log spam
            for (volatile int i = 0; i < 1000000; i++);
            
            yield(); // Keep yielding until all tests complete
        }
    } else {
        pr_verbose("Failed to create test processes!\n");
    }
}
