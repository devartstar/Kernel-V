## Debugging Exit Syscall — Kernel Reboot After Usermode Process Exit

### Problem Statement
- Created a process (PID=6) (Name=user_proc_test) (Entry=my_usermode_test_proc)
- When kernel context switches to this process, it runs `test_usermode_process` which:
    1. Maps the user mode stack pages (`0xBFFEF000–0xBFFFF000`)
    2. Maps the user code page (`0x00400000`)
    3. Copies the user program binary (66 bytes) to the code page
    4. Calls `switch_to_usermode()` which does `iret` to ring 3
    5. User program executes 3 syscalls via `INT 0x80`: SYS_WRITE, SYS_GETPID, SYS_EXIT
- SYS_EXIT terminates PID=6, context switches to idle → kernel_main
- **kernel_main immediately page faults at a garbage address → triple fault → reboot**

---

### Symptoms
- Kernel reboots with no crash log or panik — a clean triple fault reset.
- On repeated boots (serial log appending), the third cycle revealed the real fault:
```
[INFO][80][pid=2:kernel_main] PAGE_FAULT at address: 0x25783020, error code: 0x0
[VERBOSE] [PAGE FAULT] Page not present.
```
- Address `0x25783020` is garbage — kernel_main's stack was corrupted.

---

### Root Cause Analysis

#### Background: What is TSS.esp0?
The Task State Segment (TSS) field `esp0` holds the **kernel stack pointer** the CPU
loads automatically during a **ring 3 → ring 0** privilege transition. When a user-mode
process (CPL=3) triggers an interrupt (e.g., `INT 0x80` syscall, timer IRQ), the CPU:

1. Reads `SS0` and `ESP0` from the current TSS
2. Switches to kernel stack: `SS ← TSS.ss0`, `ESP ← TSS.esp0`
3. Pushes the user-mode context (`SS, ESP, EFLAGS, CS, EIP`) onto the kernel stack
4. Jumps to the interrupt handler

**Critical:** `esp0` must always point to the **current process's** kernel stack top.
If it points to another process's stack, the interrupt frame corrupts that stack.

**Note:** `esp0` is only used on ring transitions. Ring 0 → ring 0 interrupts reuse the
current stack, which is why kernel-only processes (idle, thread1-3) were unaffected.

#### The Bug: TSS.esp0 Not Updated for First-Run Processes

The context switch path has two cases:

**Case 1 — Resuming an existing process (works correctly):**
```
yield() → switch_to() → returns into yield() → tss_df.esp0 = current_proc->kernel_stack_top
```
`switch_to()` saves/restores registers and does `jmp eax` to the saved EIP, which is the
return address inside `yield()`. Execution continues at the line after `switch_to()`, where
`tss_df.esp0` is updated.

**Case 2 — Running a new process for the first time (THE BUG):**
```
yield() → switch_to() → jmp thread_entry_wrapper → entry(arg) → ... (esp0 NEVER UPDATED)
```
For a new process, `context.eip = thread_entry_wrapper`. The `jmp eax` in `context_switch.asm`
jumps directly to `thread_entry_wrapper`, **bypassing** the post-`switch_to()` code in `yield()`
that updates `tss_df.esp0`.

#### The Chain of Events

```
Tick 80: Timer preempts kernel_main (PID=2, kernel stack = 0xC2FF0000–0xC3000000)
         Scheduler picks user_proc_test (PID=6, kernel stack = 0x00118000–0x00119000)

         switch_to(kernel_main, user_proc_test)
           → jmp thread_entry_wrapper          ← SKIPS yield() post-switch code
           → tss_df.esp0 is STILL 0xC3000000   ← Should be 0x00119000!

         thread_entry_wrapper → my_usermode_test_proc → test_usermode_process
           → Maps user stack/code, copies binary
           → switch_to_usermode(0x00400000, 0xBFFFEFFC)
           → iret to ring 3 (user mode)

         User program: INT 0x80 (SYS_WRITE)
           → CPU reads TSS.esp0 = 0xC3000000   ← WRONG! This is kernel_main's stack!
           → CPU pushes iret frame (SS,ESP,EFLAGS,CS,EIP) onto kernel_main's stack
           → Syscall handler runs on WRONG stack

         User program: INT 0x80 (SYS_GETPID)   ← More corruption of kernel_main's stack

         User program: INT 0x80 (SYS_EXIT)
           → syscall_exit → yield()
           → Context switch: user_proc_test → idle → kernel_main

         kernel_main resumes
           → Stack is CORRUPTED by the 3 interrupt frames written to it
           → Page fault at garbage address 0x25783020
           → No handler for this address → double fault → triple fault → REBOOT
```

#### Evidence from Serial Log

Register dump during `INT 0x80` shows the stack mismatch:
```
ESP     | 0xc2ffffd4    ← kernel_main's stack range! (should be near 0x00119000)
EBP     | 0x00118f90    ← near PID=6's actual kernel stack (inconsistency)
CS      | 0x0000001b    ← ring 3 code segment (correct — came from user mode)
USERESP | 0xbfffefec    ← user stack (correct)
SS      | 0x00000023    ← ring 3 data segment (correct)
```
The ESP of `0xc2ffffd4` proves the CPU loaded the wrong kernel stack from TSS.esp0.

---

### Fix

**File:** `kernel/proc/proc.c` — `thread_entry_wrapper()`

Set `tss_df.esp0` to the current process's kernel stack top before calling the entry
function, and re-enable interrupts (since `cli` before `switch_to` disables them, and
the `jmp` to `thread_entry_wrapper` skips the `sti` in `yield()`):

```c
// BEFORE (broken):
void thread_entry_wrapper(void (*entry)(void *), void *arg) {
    entry(arg);
    proc_exit();
}

// AFTER (fixed):
void thread_entry_wrapper(void (*entry)(void *), void *arg) {
    tss_df.esp0 = (uint32_t)current_proc->kernel_stack_top;
    __asm__ __volatile__("sti");
    entry(arg);
    proc_exit();
}
```

**Why this fixes it:**
- `tss_df.esp0 = ...` ensures that when PID=6 enters user mode and fires `INT 0x80`,
  the CPU loads ESP from PID=6's kernel stack (`0x00119000`), not kernel_main's.
- `sti` re-enables interrupts so the new process is preemptible by the timer. Without
  this, first-run processes would execute with interrupts disabled until they voluntarily
  `yield()`.

**Why kernel-only processes were unaffected:**
thread1, thread2, thread3 never enter ring 3 (user mode). All their interrupts are
ring 0 → ring 0, which reuse the current stack without consulting TSS.esp0.

---

### Debugging Commands
```bash
make test-build              # builds the disk image
make debug-test-integration  # runs QEMU with debug flags
make gdb-kernel              # starts GDB with kernel symbols
make connect-gdb             # connects GDB to QEMU
```
