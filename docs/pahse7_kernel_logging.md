# Kernel-V Advanced Logging & Console Subsystem

## Objective

Design and implement an advanced, modular logging and console subsystem for Kernel-V, inspired by industry-grade kernel architectures (Linux, BSD, Windows, seL4), enabling robust diagnostics, debugging, and real-world deployment readiness.

### Key Goals

1. **Multi-Backend Logging Framework**
   - Log output simultaneously to VGA console, serial port (COM1/16550), and an in-memory circular buffer.
   - Modular design: backends are pluggable, configurable at boot or runtime.

2. **Log Levels & Filtering**
   - Support for standard log levels: EMERG, ERROR, WARN, INFO, VERBOSE.
   - Configurable global log level threshold for selective log emission (compile-time and/or runtime).
   - Color-coded output per log level for VGA/terminal.

3. **High-Performance Ring Buffer**
   - All log messages are stored in a fixed-size, lockless circular buffer in RAM.
   - Buffer survives console/serial overflows and can be dumped on-demand or after panic.
   - Efficient APIs for reading, dumping, and searching historical log data.

4. **Serial Console Output**
   - Implement a robust 16550 UART (COM1) serial driver for headless and QEMU-based debugging.
   - Mirror all kernel logs and panics to serial (with optional QEMU file capture).
   - Support for runtime backend enable/disable (VGA, serial, both, or future backends).

5. **Advanced Console Features**
   - (Optional) Implement a VGA scrollback buffer for true multi-page scroll history.
   - Structured log prefixes: timestamp, log level, source tag, CPU/core ID (if SMP).
   - Clear separation of early boot logging path for diagnostics before full init.

6. **Crash & Panic Diagnostics**
   - On kernel panic, automatically dump the entire in-memory log buffer to all enabled backends.
   - Provide developer APIs to query or export logs via kernel commands or syscalls.

7. **Future-Proof Extensibility**
   - Framework easily supports new backends: persistent disk log, network log, file, etc.
   - Designed for SMP scalability (per-core log buffers) and high concurrency.

### Why This Matters

- **Production-Grade Diagnostics**: Enables deep-dive debugging and forensics, even post-crash.
- **Headless Debugging**: Serial log output is vital for real hardware/QEMU where VGA isn’t accessible.
- **Reliability**: No lost logs—critical for panic analysis and long-running system health.
- **Learning & Demonstration**: Makes Kernel-V a teaching reference for best-practice kernel diagnostics.

### Deliverables

- [ ] Modular logging core with VGA, serial, and ring buffer outputs.
- [ ] Configurable log levels and runtime backend control.
- [ ] Complete, robust serial driver (init, transmit, reliability).
- [ ] In-memory scrollback/ring buffer and API for log retrieval/dump.
- [ ] Panic/crash log dump facility.
- [ ] (Optional) VGA console scrollback and advanced UI.
- [ ] Technical documentation and sample code/tests.

---

## Getting Started

See [docs/roadmap.md](docs/roadmap.md) for the full technical breakdown and milestone plan.


