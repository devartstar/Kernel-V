## **Phase 5: Process & Scheduling — 4-Week Expert Coding Plan**

This plan is designed for **maximum code exposure**, deep technical learning, and modern UNIX kernel concepts. The goal is to *not just reimplement old-school process management*, but to *build up towards a modern, advanced, and flexible process subsystem*, ready for advanced features later (e.g., real-time scheduling, namespaces, multi-core, and advanced debugging).

---

### **WEEK 1: Process Control Block (PCB) Deep Dive + Minimal Process System**

**Objectives**

* Understand and design a minimal but extensible PCB structure
* Learn about process states, process lists, PID allocation, stack layout
* Start with kernel thread/task (not full user process) support

**Tasks**

1. **Study Modern PCB Internals**

   * Reference: Bach Ch. 6, APUE Ch. 7/8, Linux/FreeBSD sources, and your UNIX-ARCHITECTURE-BOOK.
   * Learn what info modern PCBs store (pid, state, regs, kernel/user stack, parent, children, signal handlers, file table, etc.).
2. **Design your PCB struct**

   * Draw/diagram the structure (keep fields for future extensibility, e.g., for user-mode).
   * Write and review the C struct for PCB in your kernel.
3. **Implement a minimal process table/list**

   * Support for up to N processes (array/list/hash-table as per your design).
   * Implement PID allocation/recycling.
   * Functions: `proc_alloc()`, `proc_free()`, `proc_find(pid)`, etc.

**Outcomes**

* [ ] PCB struct and design doc/diagram (commit to learning folder!)
* [ ] Minimal process table and API in code (test cases for alloc/free/find)
* [ ] PCB dump command for your debug CLI/serial console

---

### **WEEK 2: Process Creation, Stack & Context, State Management**

**Objectives**

* Implement kernel-level process (thread/task) creation, initialization, and termination
* Set up stacks and context (regs, stack, entry point, argument passing)
* Handle process state transitions (NEW, READY, RUNNING, WAITING, TERMINATED)

**Tasks**

1. **Process Creation API**

   * Implement a `proc_create(fn, arg)` API that creates a new kernel process/thread.
   * Allocate stack, initialize PCB, set entry/exit routines, add to process table/list.
2. **Context Initialization**

   * Set up initial CPU context (registers) for the new process.
   * If on x86, study/trap frame layout for switching (e.g., iret/qemu/gdb step-through).
3. **Process State Management**

   * Implement state transitions and process lifecycle.
   * Design (and log) state transitions for debugging: NEW → READY → RUNNING → WAITING → TERMINATED.
   * Implement process cleanup/termination, with safe resource cleanup.

**Outcomes**

* [ ] Process creation API with test/demo process creation
* [ ] Context init code for x86 (or your arch)
* [ ] Logging and debug tools for process states

---

### **WEEK 3: Cooperative Context Switching & Stack Switching**

**Objectives**

* Implement low-level context switch routine (`switch_to()`) for switching between PCBs
* Implement stack switching and full context save/restore
* Integrate with basic scheduler (yield-based round robin)

**Tasks**

1. **Context Switch Routine**

   * Implement assembly or C routine for saving/restoring regs (EIP, ESP, EBP, general-purpose regs, etc.).
   * Add switch-to/prev/next process logic in kernel.
2. **Stack Switching**

   * Validate context switch with kernel threads running on different stacks.
   * Add logs for stack pointer, regs, and process state during switch.
3. **Manual Yield**

   * Implement `yield()` for cooperative multitasking.
   * Allow processes to call yield to switch to another READY process.

**Outcomes**

* [ ] `switch_to()` routine working, with registers and stack switching
* [ ] Multiple kernel processes running and yielding
* [ ] Debug logs of stack/reg values on switch for verification

---

### **WEEK 4: Minimal Scheduler + Process API Polish + Advanced Extensions**

**Objectives**

* Build a simple round-robin scheduler
* Polish APIs for process creation/termination
* Add hooks for advanced features: signals, sleeping, user processes, priorities, SMP

**Tasks**

1. **Scheduler Implementation**

   * Implement scheduler loop/ISR (timer or manual round robin)
   * Pick next READY process, call context switch.
2. **API Polish & Testing**

   * Add process wait/kill APIs, enhance error handling.
   * Add extensive test/demo programs for kernel threads.
3. **Prepare for Extensions**

   * Add TODOs and design docs for: signal support, usermode, preemption, priority scheduling, multi-core (if in roadmap).
   * Commit detailed code comments and markdown docs in the repo.

**Outcomes**

* [ ] Minimal round-robin scheduler, processes run/yield/switch/terminate
* [ ] Process API, documentation, and demo/test apps
* [ ] Design doc for next steps: signals, user processes, preemption, SMP

---

### **Continuous: Learning by Coding**

* At the end of each week, **write a brief doc/diagram** in your `learning/` folder summarizing what you built, learned, and *how your design relates to modern UNIX/Linux/FreeBSD*.
* Use QEMU/GDB to debug and log kernel state for each process switch/creation/termination.
* Implement at least 2-3 test "kernel threads" (counter, printer, sleep-loop) to demo your system.

