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
break yield
break scheduler_pick_next
break switch_to
# Process function breakpoints
break my_test_proc
break preemptive_proc
break my_sleep_proc
# Process inspection commands
define show-processes
  printf "
=== PROCESS LIST ===
"
  set $p = ready_list_head
  while $p
    printf "PID: %d, Name: %s, State: %d
", $p->pid, $p->name, $p->state
    printf "  EIP: 0x%08x, ESP: 0x%08x
", $p->context.eip, $p->context.esp
    set $p = $p->next
  end
  printf "
"
end
# Show breakpoints
info breakpoints
# Ready to debug processes
printf "
=== INTEGRATION TEST DEBUG SESSION ===
"
printf "Available commands:
"
printf "  show-processes - List all processes
"
printf "  continue       - Run to next breakpoint
"
printf "==========================================

"
