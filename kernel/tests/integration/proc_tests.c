#include "tests/proc_tests.h"
#include "lib/printk.h"
#include "proc/proc.h"

extern pcb_t* current_proc;

static volatile int test_processes_remaining = 3;

void test_process_finished(void)
{
    test_processes_remaining--;
}

void my_test_proc(void* arg)
{
	(void)arg;
	int max_runs = 3;

	for (int i = 0; i < max_runs; i++)
	{
		pr_verbose("Process %s is running! iteration %d/%d\n",
				   current_proc->name,
				   i + 1,
				   max_runs);
		yield();
	}

	pr_verbose("[EXIT PROCESS] Process %s finished, terminating\n", current_proc->name);
	test_process_finished();
}

void my_sleep_proc(void* arg)
{
    (void)arg;
    int i = 0;
    int max_sleep_cycles = 3;  // Limit the sleep cycles
    
    while (i < max_sleep_cycles)
    {
        pr_verbose("Thread %s sleeping, i=%d\n", current_proc->name, i);
        proc_sleep(50);
        pr_verbose("Thread %s woke up!\n", current_proc->name);
        i++;
    }
    pr_verbose("[EXIT PROCESS] Process %s finished, terminating\n", current_proc->name);
    test_process_finished();
}

void preemptive_proc(void* args)
{
    (void)args;
    int i = 0;
    int max_iterations = 100000;
    
    while (i < max_iterations)
    {
        if (i % 1000 == 0)
        {
            pr_verbose(
                "Thread %s is running, for [%d / %d]\n", current_proc->name, i/1000, max_iterations/1000);
        }
        i++;
    }
    
    pr_verbose("[EXIT PROCESS] Process %s finished after %d iterations, terminating\n", 
               current_proc->name, max_iterations);
	test_process_finished();
}

void create_test_processes(void)
{
	pcb_t* test_proc1 = proc_create(my_test_proc, NULL, "thread1");
	pcb_t* test_proc2 = proc_create(preemptive_proc, NULL, "thread2");
	pcb_t* test_proc3 = proc_create(my_sleep_proc, NULL, "thread3");
	// Todo: Check for Preemptive followed by Preemptive process...
	
	if (test_proc1 && test_proc2 && test_proc3)
	{
		pr_verbose("Test processes created successfully\n");
		pr_verbose("Starting scheduler with idle process...\n");

        test_processes_remaining = 3;

        // Let processes run and wait for them to complete
        yield(); // Start the processes
        
        // Wait for all test processes to finish
        while (test_processes_remaining > 0) {
			pr_verbose("Waiting for test processes to finish: %d remaining\n", test_processes_remaining);
            yield(); // Keep yielding until all tests complete
        }
	}
	else
	{
		pr_verbose("Failed to create test processes!\n");
	}
}
