## **Phase 7: Code Refactorign and Testing Framework**

## Interrupts
### 1 IDT Refactor & Trap Handler Abstraction
Instead of hardcoding interrupt handlers, build macros to register for ISR and
IRQs

### 2 Automated ISR/IRQ Stub Generation
Eliminate manual ISR asm. Use macros/script to auto-generate all 256 stubs, each
pushes a vector number, calls the `isr_common_handler` and cleans up.

### 3 Central Interrupt Routine
All exceptions/IRQs go through one C router.

### 4 Context Structure Unification
All ISRs recieve the exact same struct (register dump, error code and cpu state)

### 5 Interrupt nesting, priorities and masking
Add mask and unmask logic and allow higher priority IRQs to preempt lower
priority ISRs.

## Syscalls
### 1 Privelege seperation - Ring 3 entry
User processes running at CPL=3 (ring 3)

### 2 User to Kernel transition
Set up a syscall vector 0x80 with dedicated handler

### 3 System call tables and Dispatcher
A table of system call vectors indexed by their syscall numbers

### 4 User/Kernel AbI and Arguments
Decide argument passing ABI:
- Classic: All in registers (eax=syscall#, ebx/ecx/edx/esi/edi/ebp for args)
- Modern: Sysenter/syscall (fast path), or stack-based (for >5 args)

### 5 Syscenter/Syscall faster implementation
Add support for syscenter for faster entry

### 6 Syscall implementation CORE
Implement a minimal but modern syscall API:
write, read, exit, fork/clone, exec, getpid, yield, sbrk, mmap, etc.

### 7 Error handeling and Return
Define consistent error codes, propagate via eax, use errno convention for POSIX
compatibility.

## Advanced Features
### 1 Syscall filtering/Whitelisting
Like seccomp-bpf—allow/deny syscalls per process for sandboxing.

### 2 Tracing/Logging
Add tracing/logging hooks for every interrupt/syscall for debug, security, and performance.

### 3 Per Process SIGNAL/INTERRUPT handling
Allow user processes to register signal/interrupt handlers (SIGSEGV, SIGINT, etc
)

### 4 Vectoring Syscall: Multi ABI Compatibility
Support alternate syscall entrypoints (e.g., both int 0x80 and sysenter, or 32/64-bit syscall ABI).

### 5 Security Features
Stack canaries, syscall argument validation, privilege checks.

### 6 Performance Optimizations
Lazy context save/restore, syscall batching, interrupt coalescing, fastpath syscalls for common cases.

## Validation, Testing, and Tools

- Unit tests for handlers, nested IRQs, privilege switches.
- Integration tests: userland calling kernel syscalls, stress test for concurrent syscalls/interrupts.
- Debug macros for step-by-step logging (with tracing levels).
- Userland test programs (in assembly and C) to validate every syscall and interrupt scenario.


## References and Inspirations:

- Linux x86 entry_64.S / entry_32.S (see how modern kernels handle hundreds of syscalls and vectorized ISRs)

- OSDev Wiki: IDT, PIC, System Calls, Ring transitions

- Plan9/9front syscall mechanism (clean, elegant C-centric syscall dispatch)

- Windows NT/XP syscall stubs (for inspiration on fastpath and tracing)

### Syscall Folder Structure
```
kernel/
├── arch/                    # Architecture-specific code
│   └── x86/
│       ├── boot/           # Boot and initialization
│       ├── cpu/            # CPU management (GDT, IDT, TSS)
│       ├── interrupt/      # Interrupt handling
│       └── memory/         # Architecture-specific memory management
├── core/                   # Core kernel functionality
│   ├── init/              # Kernel initialization
│   ├── panic/             # Panic handling
│   └── debug/             # Debug utilities
├── drivers/                # Device drivers
│   ├── char/              # Character devices
│   ├── block/             # Block devices
│   └── video/             # Video devices
├── fs/                     # File system support
├── include/                # Header files (organized by subsystem)
│   ├── arch/
│   ├── core/
│   ├── drivers/
│   ├── mm/
│   ├── proc/
│   └── lib/
├── ipc/                    # Inter-process communication
├── lib/                    # Kernel library functions
│   ├── string/            # String manipulation
│   ├── printf/            # Formatted printing
│   └── data_structures/   # Data structures (lists, trees, etc.)
├── mm/                     # Memory management
│   ├── physical/          # Physical memory management
│   ├── virtual/           # Virtual memory management
│   └── allocators/        # Memory allocators
├── net/                    # Network stack
├── proc/                   # Process and task management
│   ├── scheduler/         # Scheduling algorithms
│   ├── context/           # Context switching
│   └── sync/              # Synchronization primitives
├── security/               # Security subsystem
├── time/                   # Time management
└── tests/                  # Testing framework (empty for now)
    ├── unit/              # Unit tests
    ├── integration/       # Integration tests
    └── framework/         # Testing framework code
```

