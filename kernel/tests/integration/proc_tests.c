#include "tests/proc_tests.h"
#include "lib/printk.h"
#include "proc/proc.h"

extern pcb_t* current_proc;

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

	pr_verbose("Process %s finished, terminating\n", current_proc->name);
	current_proc->state = PROC_TERMINATED;
}

void my_sleep_proc(void* arg)
{
	(void)arg;
	int i = 0;
	while (1)
	{
		pr_verbose("Thread %s sleeping, i=%d\n", current_proc->name, i++);
		proc_sleep(50);
		pr_verbose("Thread %s woke up!\n", current_proc->name);
	}
}

void preemptive_proc(void* args)
{
	(void)args;
	int i = 0;
	while (1)
	{
		if (i % 1000 == 0)
		{
			pr_verbose(
				"Thread %s is running, for i=%d\n", current_proc->name, i);
		}
		i++;
	}
}

void create_test_processes(void)
{
	pcb_t* test_proc1 = proc_create(my_test_proc, NULL, "thread1");
	pcb_t* test_proc2 = proc_create(preemptive_proc, NULL, "thread2");
	pcb_t* test_proc3 = proc_create(preemptive_proc, NULL, "thread3");
	pcb_t* test_proc4 = proc_create(my_sleep_proc, NULL, "thread4");

	pr_info("DEBUG: thread1 eflags=0x%x\n", PRINT_UINT32(test_proc1->context.eflags));
    pr_info("DEBUG: thread2 eflags=0x%x\n", PRINT_UINT32(test_proc2->context.eflags));
    pr_info("DEBUG: thread3 eflags=0x%x\n", PRINT_UINT32(test_proc3->context.eflags));
    pr_info("DEBUG: thread4 eflags=0x%x\n", PRINT_UINT32(test_proc4->context.eflags));
    

	if (test_proc1 && test_proc2 && test_proc3 && test_proc4)
	{
		pr_verbose("Test processes created successfully\n");
		pr_verbose("Starting scheduler with idle process...\n");
		yield();
	}
	else
	{
		pr_verbose("Failed to create test processes!\n");
	}
}
