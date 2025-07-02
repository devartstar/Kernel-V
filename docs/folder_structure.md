Kernel-V/
├── bootloader/              # Stage 1 and Stage 2 bootloader source
│   ├── stage1.asm
│   ├── stage2.asm
│   └── README.md
├── kernel/                  # Core kernel source code
│   ├── arch/                # Architecture-specific code (x86, etc.)
│   │   └── x86/
│   ├── include/             # Kernel header files
│   ├── mm/                  # Memory management (paging, allocators)
│   ├── fs/                  # File system code
│   ├── proc/                # Process and scheduling code
│   ├── drivers/             # Device drivers
│   ├── syscall/             # System call interface
│   ├── ipc/                 # Inter-process communication
│   ├── libk/                # Kernel utility library
│   ├── main.c               # Kernel entry point
│   └── Makefile
├── user/                    # Userland programs & minimal libc
│   ├── init/                # First user program (init/shell)
│   ├── bin/                 # Other user programs (cat, echo, etc.)
│   └── libc/                # Minimal user C library
├── scripts/                 # Build, test, debug scripts
│   ├── build.sh
│   ├── run.sh
│   └── debug.sh
├── tools/                   # Auxiliary tools (disk image, fs builder, etc.)
│   └── mkfs.c
├── docs/                    # Documentation, design, specs, diagrams
│   ├── architecture.md
│   ├── memory.md
│   ├── fs.md
│   ├── process.md
│   ├── roadmap.md
│   └── diagrams/
├── tests/                   # Automated/unit tests for kernel components
├── learning/                 # ← All learning mini-projects & experimental code
│   ├── 00-hello-boot/
│   ├── 01-vga-print/
│   ├── 02-memory-map/
│   ├── 03-page-allocator/
│   ├── ...                   # Each subfolder: focused, isolated project
│   └── README.md             # Index & short description of all learning projects
├── LICENSE
├── README.md
└── Makefile                 # Top-level Makefile
