## Kernel version 0.7.4 enhancement plans

### Phase A — Formalize the process model
This phase is now more important than before.
#### A1. Add process identity fields to PCB
Add:
- proc_type_t type
- int32_t exit_code
- uint8_t has_exited
Also normalize user-process metadata:
user_entry
user_code_start
user_code_size

Reason:
Your current PCB knows too little about what the process is and what it owns.

#### A2. Classify all process creation paths

Make the kernel explicitly distinguish:

PROC_TYPE_BOOTSTRAP → kernel_main
PROC_TYPE_IDLE
PROC_TYPE_KERNEL
PROC_TYPE_USER

Reason:
Your code currently has at least four semantically different process kinds, but the PCB does not express that.

#### A3. Split creation APIs

Instead of one path plus tests, define:

proc_create_kernel(...)
userproc_create_from_blob(...)

Reason:
Your current user process launch logic in test_usermode_process() already contains the exact ingredients of a constructor. It should become one.

#### A4. Make exit semantics real

Refactor sys_exit() so it:

records exit_code
marks has_exited
sets PROC_TERMINATED
yields
never returns

Reason:
Current exit works operationally, but not yet as a real lifecycle primitive.

#### A5. Make cleanup type-aware

Split cleanup into:

idle cleanup: never
bootstrap cleanup: special / none for now
kernel cleanup: free kernel-owned resources only
user cleanup: free user mappings + kernel stack + PCB-owned resources

Reason:
Your current proc_free() is too generic for the architecture you’re building.

### Phase B — Turn user process launch into a subsystem

This phase should now come earlier and more explicitly than before.

#### B1. Extract test_usermode_process() into userproc_create_from_blob()

Right now your test helper is already 80% of a loader.

Move this logic into a real function.

It should:

allocate PCB
assign PROC_TYPE_USER
create kernel stack
map user stack
map user code
copy blob
populate user metadata
prepare process for ring 3 entry

Reason:
This removes hand-written user launch logic from tests and makes it reusable.

#### B2. Introduce a user process entry wrapper

Normal kernel threads start at thread_entry_wrapper().
User processes should get an analogous controlled launch path.

Something like:

kernel thread starts
wrapper sets up/enters user mode
from then on process behaves as user process

Reason:
Right now switch_to_usermode() is called directly from a test helper. That should become a defined execution path.

#### B3. Support multiple user blobs cleanly

Once blob-based creation is a real API, let tests launch:

user_hello
user_syscall_test
user_exit_test

Reason:
You are ready to move from “one stub proving int 0x80 works” to “user program test suite.”

### Phase C — Harden the syscall layer

This phase changes from “clean syscall subsystem” to “make syscall subsystem safe enough to grow.”

#### C1. Keep canonical syscall ABI/header

This part of your earlier plan still stands.

Keep:

syscall enum in one header
table registration centralized

That is already mostly in place.

#### C2. Add user pointer validation helpers

Before adding richer syscalls, add helpers like:

user_ptr_valid(ptr)
user_range_valid(ptr, len)

Use them first in:

sys_write

Reason:
Current sys_write() trusts user memory blindly.

#### C3. Harden sys_write()

Add:

user range validation
bounded copy
possibly page-by-page safe access later

Reason:
This is the first syscall that crosses user-memory boundary. It should be your model for safe syscall design.

#### C4. Add syscall tracing toggle

You already have excellent logs. Formalize them behind a trace flag.

Reason:
You have enough logs now that selective visibility matters.

#### C5. Add explicit syscall return/error convention

Keep ENOSYS, but normalize all syscall return behavior:

non-negative success
negative error codes
no mixed conventions

Reason:
You’re about to add more syscalls; now is the time to freeze conventions.

### Phase D — Parent/child and process observability

This phase should come before exec and far before ELF.

#### D1. Add parent-child semantics

You already have parent in PCB. Start using it.

When user process is created:

set parent properly
record exit code on termination

#### D2. Add wait() / waitpid() minimal version

This is the natural next syscall after exit().

Reason:
Exit without wait means dead processes are only kernel-internal artifacts.
Wait makes process lifecycle observable.

#### D3. Add zombie state if needed

Right now you only have:

NEW
READY
RUNNING
WAITING
TERMINATED

You may soon need:

PROC_ZOMBIE

Reason:
If parent must read child exit code before cleanup, terminated-vs-cleaned-up should be separated.

I would not add it immediately unless you implement wait(), but it’s coming.

### Phase E — Program loading evolution

Only after A–D are solid.

#### E1. Keep flat binary loader, but make it reusable

Right now blob loading is fine.

#### E2. Add second user program

This is the best bridge milestone before ELF.

#### E3. Add exec model

Only after you can create/wait/exit cleanly.

#### E4. Move to ELF loading

Only once flat-binary process lifecycle is clean.

Reason:
ELF is not just “better loading.” It depends on a stable user process abstraction.

### Folder Structure
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
### Testing Framework
