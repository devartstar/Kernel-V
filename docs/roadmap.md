# Kernel-V Project Roadmap

A comprehensive, milestone-based roadmap to guide the development and learning journey for Kernel-V—a modular, UNIX-like kernel, built and understood from scratch.  
Each phase is broken into focused goals, mini-projects, learning objectives, and key references.

---

## Table of Contents

1. [Preparation & Environment](#phase-0-preparation--environment)
2. [Minimal Kernel Bootstrap](#phase-1-minimal-kernel-bootstrap)
3. [Memory Management](#phase-2-memory-management)
4. [Process & Scheduling](#phase-3-process--scheduling)
5. [Interrupts & System Calls](#phase-4-interrupts--system-calls)
6. [File System Foundations](#phase-5-file-system-foundations)
7. [Advanced Process Management](#phase-6-advanced-process-management)
8. [Device Drivers & I/O](#phase-7-device-drivers--io)
9. [Userland & Shell](#phase-8-userland--shell)
10. [Interprocess Communication (IPC)](#phase-9-interprocess-communication-ipc)
11. [Extension & Advanced Topics](#phase-10-extensions--advanced-topics)
12. [Ongoing Learning & Mini-Projects](#ongoing-learning--mini-projects)
13. [References](#references)

---

## Phase 0: Preparation & Environment

**Objectives:**
- Set up build, emulation, and debugging environment.
- Understand the project structure and workflow.
- Review relevant sections of UNIX and OS architecture.

**Checklist:**
- [ ] Install: `nasm`, `gcc`, `qemu`, `gdb`, `make`, `git`
- [ ] Clone Kernel-V repo; review structure and docs
- [ ] Build & run the provided "Hello World" kernel with your bootloader
- [ ] Read: Bach Ch.1–2; APUE Ch.1–2

---

## Phase 1: Minimal Kernel Bootstrap

**Objectives:**
- Create the simplest possible kernel image that boots, prints to screen, and halts.
- Build custom Stage 1/Stage 2 bootloader (or use your existing one).
- Set up linker script and kernel entry.

**Milestones:**
- [ ] Bootable kernel image
- [ ] VGA/serial "Hello, Kernel-V" print
- [ ] Panic/assert handler
- [ ] Infinite main loop

**Key Learnings:**
- BIOS boot process
- x86 protected mode basics
- ELF binary basics

---

## Phase 2: Memory Management

**Objectives:**
- Parse BIOS/bootloader-provided memory map (e820)
- Implement a physical page frame allocator (bitmap)
- Set up basic paging (identity mapping, kernel heap)
- Provide kernel `malloc`/`free`

**Milestones:**
- [ ] Memory map parser
- [ ] Bitmap page/frame allocator
- [ ] Simple paging/MMU enable
- [ ] Kernel heap with bump/stack allocator

**Key Learnings:**
- x86 segmentation vs. paging
- Memory regions, address spaces

**Book References:**
- Bach: Ch. 2.2, 9  
- APUE: Ch. 7.8, 7.11

---

## Phase 3: Process & Scheduling

**Objectives:**
- Implement the Process Control Block (PCB)
- Basic process creation, state management, and cleanup
- Cooperative context switching (kernel threads/tasks)
- Initial simple scheduler

**Milestones:**
- [ ] PCB data structure
- [ ] Process creation/termination API
- [ ] Cooperative context switch routine
- [ ] Round-robin scheduler (manual yield)

**Key Learnings:**
- Stack switching, context save/restore
- Process states

**Book References:**
- Bach: Ch. 6, 7  
- APUE: Ch. 8

---

## Phase 4: Interrupts & System Calls

**Objectives:**
- Set up the Interrupt Descriptor Table (IDT) and remap the PIC
- Timer (PIT) and keyboard interrupts
- Basic system call interface (int 0x80/sysenter)
- User-to-kernel transitions

**Milestones:**
- [ ] Hardware interrupt handlers (timer, keyboard)
- [ ] User process triggers syscall to kernel
- [ ] Kernel-to-user privilege transition
- [ ] Basic syscall table and dispatcher

**Key Learnings:**
- IDT, PIC, IRQs
- Syscall mechanisms (int, sysenter, syscall)

**Book References:**
- Bach: Ch. 1.5.1, 2.2.2, 10  
- APUE: Ch. 10, 14

---

## Phase 5: File System Foundations

**Objectives:**
- Design a minimal file system: RAMFS, toy ext2, or FAT
- Implement block device abstraction
- Support file metadata (superblock, inodes)
- Provide file operations: open, read, write, close

**Milestones:**
- [ ] RAM disk or block device driver
- [ ] Superblock/inode/data structures
- [ ] Path parsing, directory structure
- [ ] File descriptor table

**Key Learnings:**
- Files vs. devices, buffer cache, inodes
- Pathname resolution

**Book References:**
- Bach: Ch. 3, 4, 5

---

## Phase 6: Advanced Process Management

**Objectives:**
- Preemptive multitasking using timer interrupts
- Implement process priorities
- Process sleep/wakeup, kill/exit syscalls
- Process tree, parent-child relationships

**Milestones:**
- [ ] Timer-based context switching
- [ ] Process priority and time slices
- [ ] sleep, wakeup, kill, exit syscalls

**Key Learnings:**
- Preemption, process lifecycle, parent/child

**Book References:**
- Bach: Ch. 8, 6.4

---

## Phase 7: Device Drivers & I/O

**Objectives:**
- Abstract device interface (char/block)
- Implement basic drivers: keyboard, screen, serial, RAM/disk
- Userland access to devices via special files

**Milestones:**
- [ ] Keyboard driver (interrupts)
- [ ] Screen/VGA and serial output/input
- [ ] RAM or block device (for fs)

**Key Learnings:**
- Device files, driver architecture

**Book References:**
- Bach: Ch. 10, 4.24

---

## Phase 8: Userland & Shell

**Objectives:**
- Support user programs (ELF loader, user stack/setup)
- Minimal userland libc
- Basic shell (interactive commands)
- Program loading, fork/exec

**Milestones:**
- [ ] User program loader (ELF or raw bin)
- [ ] System call stubs in libc
- [ ] Simple shell (built-in commands)
- [ ] fork/exec implementation

**Key Learnings:**
- User/kernel address separation
- exec, fork, wait mechanisms

**Book References:**
- Bach: Ch. 7.8, APUE: Ch. 1, 8

---

## Phase 9: Interprocess Communication (IPC)

**Objectives:**
- Pipes (anonymous, then named)
- Signals and process notification
- Shared memory or message queues (advanced)

**Milestones:**
- [ ] Kernel-level pipes and redirection
- [ ] Basic signal handling (kill, alarm, etc.)
- [ ] Shared memory (optional)

**Key Learnings:**
- IPC APIs, signal delivery

**Book References:**
- Bach: Ch. 11  
- APUE: Ch. 15–17

---

## Phase 10: Extensions & Advanced Topics

**Objectives:**
- Virtual memory: paging, swapping
- Advanced file system (ext2, journaling)
- Networking stack (TCP/IP basics)
- Security/multiuser (permissions, user IDs)
- Performance optimization and profiling

**Milestones:**
- [ ] Virtual memory manager
- [ ] Swapping (to disk)
- [ ] File system enhancements (journaling, permissions)
- [ ] Simple network stack

**Key Learnings:**
- VM design, kernel security, network protocols

**Book References:**
- As needed (refer to Minix/xv6/Linux, advanced OS texts)

---

## Ongoing Learning & Mini-Projects

Parallel to the main development, each major concept or tricky subsystem will be first explored as a **mini-project** in the `learning/` folder:

- Boot sectors, protected mode, GDT/IDT/PIC
- VGA/serial printing
- Paging and memory allocation
- Context switching, PCB
- Disk/RAMFS file system experiments
- Interrupt and syscall stubs
- ELF loader prototypes
- More...

Keep a log for each in `learning/README.md` for future reference and rapid prototyping.

---

## References

- *The Design of the UNIX Operating System* — Maurice J. Bach
- *Advanced Programming in the UNIX Environment* — W. Richard Stevens & Stephen Rago
- [xv6 (MIT)](https://pdos.csail.mit.edu/6.828/2022/xv6.html)
- [Minix](https://www.minix3.org/)
- [Linux 0.11](https://github.com/mengning/linuxkernel)
- [osdev.org](https://wiki.osdev.org/)

---

**Progress is tracked via the project table in [README.md](../README.md) and in `learning/` for rapid prototyping. Update this roadmap as new milestones are added or revised!**

---

*Happy hacking, and document your journey!*
