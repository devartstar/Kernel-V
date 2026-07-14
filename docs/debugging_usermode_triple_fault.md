# Debugging a Triple Fault During the Usermode Test

> A step-by-step, kernel-level walkthrough of how a mysterious reboot loop was
> traced from a serial log all the way down to a single uninitialized pointer in
> a unit test. Written to be read top-to-bottom as a teaching document on
> low-level x86 kernel debugging.

---

## Table of contents

1. [The symptom: a kernel that reboots itself](#1-the-symptom-a-kernel-that-reboots-itself)
2. [Background you need: paging, higher-half, and address spaces](#2-background-you-need-paging-higher-half-and-address-spaces) *(pending)*
3. [Reproducing the crash headlessly and capturing the fault vectors](#3-reproducing-the-crash-headlessly-and-capturing-the-fault-vectors) *(pending)*
4. [From a hex address to a line of source: addr2line](#4-from-a-hex-address-to-a-line-of-source-addr2line) *(pending)*
5. [Forming the first hypothesis: the CR3 load faults](#5-forming-the-first-hypothesis-the-cr3-load-faults) *(pending)*
6. [GDB probe #1: dumping the new page directory](#6-gdb-probe-1-dumping-the-new-page-directory) *(pending)*
7. [GDB probe #2: the NULL global pointers](#7-gdb-probe-2-the-null-global-pointers) *(pending)*
8. [Static analysis: the source says they are initialized](#8-static-analysis-the-source-says-they-are-initialized) *(pending)*
9. [Interrogating the ELF: objdump and readelf](#9-interrogating-the-elf-objdump-and-readelf) *(pending)*
10. [Bootloader sector math: ruling out a truncated load](#10-bootloader-sector-math-ruling-out-a-truncated-load) *(pending)*
11. [GDB probe #3: scanning the kernel tail in memory](#11-gdb-probe-3-scanning-the-kernel-tail-in-memory) *(pending)*
12. [GDB probe #4: it was correct at boot](#12-gdb-probe-4-it-was-correct-at-boot) *(pending)*
13. [GDB probe #5: the hardware watchpoint that caught the culprit](#13-gdb-probe-5-the-hardware-watchpoint-that-caught-the-culprit) *(pending)*
14. [Root cause: an uninitialized pcb pointer](#14-root-cause-an-uninitialized-pcb-pointer) *(pending)*
15. [The fix](#15-the-fix) *(pending)*
16. [Verifying the fix](#16-verifying-the-fix) *(pending)*
17. [The three stale syscall unit tests](#17-the-three-stale-syscall-unit-tests) *(pending)*
18. [Lessons: a reusable kernel-debugging playbook](#18-lessons-a-reusable-kernel-debugging-playbook) *(pending)*

---

## 1. The symptom: a kernel that reboots itself

### What we were handed

The only artifact to start with was a serial log (`serial.log`) captured from a
QEMU run of the test build. Two things stood out immediately:

- The kernel's **boot banner appeared twice** in the log — once near the very
  top (line 1) and again roughly 3,000 lines later (~line 3131).
- Between the first banner and the second, the last meaningful lines were the
  start of the **usermode process / syscall test**:

  ```
  [TEST] RUNNING usermode process / syscall tests
  ```

  followed by a VFS lookup of `/hello.txt` — and then nothing. No
  `print_proc_info` output for the first user process ever appeared.

### Reading the symptom like a kernel developer

A banner that prints **twice** in a single QEMU invocation is the classic
signature of an unhandled **triple fault**. Here is the mechanism, from the CPU's
point of view:

- On x86, when the CPU hits an exception it looks up a handler in the
  **Interrupt Descriptor Table (IDT)**.
- If delivering that handler itself raises a second exception, the CPU escalates
  to a **double fault** (vector `#DF`, `0x08`).
- If delivering the double-fault handler *also* faults, the CPU has run out of
  options. It performs a **triple fault**, which on real hardware and in QEMU
  causes an immediate **CPU reset** — i.e. the machine reboots.

A reboot means execution jumps back to the reset vector, the bootloader runs
again, the kernel is reloaded, and the banner prints a second time. So "two
banners" is really telling us: *"something faulted so badly that exception
handling itself collapsed."*

### Narrowing the window

The position of the crash is just as informative as its nature. Because:

- the log stops right after `RUNNING usermode process / syscall tests`, and
- we never saw the per-process `print_proc_info` output that
  `userproc_create_from_blob` emits,

the fault must occur **very early in the creation of the first user process** —
after the test harness announces itself, but before the first user process is
fully constructed and printed. That single observation let us focus the entire
investigation on the user-process creation path instead of the thousands of
lines of tests that ran successfully before it.

### What we knew going in (from the build configuration)

The build under test had these Kconfig options enabled, which shaped how we
reproduced and instrumented the crash:

- `CONFIG_DEBUG_ENABLED=y`, `CONFIG_TRACE_LEVEL=4` — verbose serial logging.
- `CONFIG_BUILD_TEST=y`, `CONFIG_TESTS_UNIT=y`, `CONFIG_TESTS_INTEGRATION=y` —
  both the unit-test suite *and* the integration (usermode) suite are linked
  into a single combined image, `build/tests/disk_test.img`.
- `CONFIG_DEBUG_TSS=y` — extra task-state-segment sanity output.

The key takeaway from the config: this is the **combined** test image, so the
unit tests run first (in kernel context) and *then* the usermode/integration
tests run. Keep that ordering in mind — it becomes the whole story later.

---

> **Next up — Section 2:** the paging and address-space background you need
> before the fault vectors will make sense (identity mapping, the higher-half
> kernel, `CR3`, and how a new user address space is built). Say the word and
> I'll write it.
