set architecture i386
target remote :1234
symbol-file build/tests/kernel_integration.elf  
# Enable TUI mode with source layout
tui enable
layout src
focus cmd
# Process debugging breakpoints
break kernel_main
break run_kernel_tests
break create_test_processes
break proc_create
break proc_exit
break proc_exit
break cleanup_terminated_processes
break yield
break scheduler_pick_next
break switch_to
# Process function breakpoints
break my_test_proc
break preemptive_proc
break my_sleep_proc
# Process inspection commands
define show-processes
  printf "\n=== PROCESS LIST ===\n"
  set $p = ready_list_head
  while $p
    printf "PID: %d, Name: %s, State: %d\n", $p->pid, $p->name, $p->state
    printf "  EIP: 0x%08x, ESP: 0x%08x\n", $p->context.eip, $p->context.esp
    set $p = $p->next
  end
  printf "\n"
end
# Show breakpoints
info breakpoints
# Ready to debug processes
printf "\n=== INTEGRATION TEST DEBUG SESSION ===\n"
printf "Available commands:\n"
printf "  show-processes - List all processes\n"
printf "  continue       - Run to next breakpoint\n"
printf "==========================================\n\n"
