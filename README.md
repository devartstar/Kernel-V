# Kernel-V

A project-based, modular UNIX-like kernel developed from scratch for deep learning and experimentation.  
**Kernel-V** is built as a series of hands-on mini-projects following the design principles of classic UNIX, inspired by *The Design of the UNIX Operating System* (Maurice J. Bach).

---

## 🌟 **Project Vision**

- Learn UNIX kernel architecture by building each subsystem as a mini-project.
- Bootstrapped with a custom Stage 1/Stage 2 bootloader.
- Incrementally build features: process management, memory, file system, drivers, userland, and more.

---

## 📂 **Repository Structure**

- **bootloader/**: Stage 1 & 2 bootloaders (assembly)
- **kernel/**: Core kernel source code, organized by subsystems
    - `arch/` - architecture-specific code
    - `include/` - kernel headers
    - `mm/` - memory management
    - `fs/` - file systems
    - `proc/` - process management
    - `drivers/` - device drivers
    - `syscall/` - syscall layer
    - `ipc/` - interprocess comm.
    - `libk/` - kernel utility library
- **user/**: Userland programs, minimal libc, shell, etc.
- **scripts/**: Build, run, and debug scripts
- **tools/**: Helper tools (disk image builder, mkfs, etc.)
- **docs/**: Design docs, diagrams, and learning notes
- **tests/**: Unit and integration tests for kernel modules

See [docs/folder_structure.md](docs/folder_structure.md) for the full project roadmap.
---

## 🚀 **Getting Started**

1. **Build the bootloader and kernel:**
    ```sh
    make
    ```

2. **Run in QEMU:**
    ```sh
    ./scripts/run.sh
    ```

3. **Development Cycle:**
    - Edit kernel or user source.
    - Rebuild with `make`.
    - Test in QEMU or with scripts.

---

## 🛠️ **Features & Progress**

See [docs/roadmap.md](docs/roadmap.md) for the full project roadmap.

---

## 🧑‍💻 **Learning Resources**

- Book: *The Design of the UNIX Operating System* (Maurice J. Bach)
- APUE: *Advanced Programming in the UNIX Environment* (Stevens/Rago)
- Reference implementations: [xv6](https://pdos.csail.mit.edu/6.828/2022/xv6.html), [Minix], [Linux 0.11]

---

## 🤝 **Contributing & License**

This is a personal learning project, but contributions, bug reports, or feedback are welcome!
See [LICENSE](LICENSE) for details.

---

## 📝 **Documentation**

See the [docs/](docs/) directory for design docs, implementation notes, diagrams, and the full learning log.

---

Happy hacking! 🚀
