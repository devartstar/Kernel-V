| Register   | Type                | Usage / Function                                                                                 |
| ---------- | ------------------- | ------------------------------------------------------------------------------------------------ |
| **EAX**    | General-Purpose     | Accumulator: arithmetic, I/O, and string ops (e.g. `mul`, `div`).                                |
| **EBX**    | General-Purpose     | Base register: addressing data in DS (real mode), pointer to data structures.                    |
| **ECX**    | General-Purpose     | Counter: loop (`loop`), shift/rotate count, string ops (`rep`).                                  |
| **EDX**    | General-Purpose     | Data register: I/O, arithmetic (high dword in `mul`/`div`), port I/O.                            |
| **ESI**    | Source Index        | Source pointer for string and memory ops (`movs`, `lods`).                                       |
| **EDI**    | Destination Index   | Destination pointer for string and memory ops (`movs`, `stos`).                                  |
| **EBP**    | Base Pointer        | Stack-frame base: points to start of current stack frame (function prologue/epilogue).           |
| **ESP**    | Stack Pointer       | Top of stack. Pushed/popped automatically by `call`/`ret`/`push`/`pop`.                          |
| **CS**     | Code Segment        | Base for instruction fetch. CS×16 + EIP → physical address in real mode.                         |
| **DS**     | Data Segment        | Default segment for data references.                                                             |
| **ES**     | Extra Segment       | Default “extra” data segment (e.g. destination for string ops).                                  |
| **SS**     | Stack Segment       | Segment for stack operations (stack pushes/pops use SS\:ESP).                                    |
| **FS**     | Additional Segment† | User-defined; often used by OS/thread libraries for thread-local storage (protected mode).       |
| **GS**     | Additional Segment† | Same as FS: extra user-defined segment register.                                                 |
| **EIP**    | Instruction Pointer | Offset of next instruction within CS.                                                            |
| **EFLAGS** | Flags Register      | Status/control flags (ZF, CF, SF, IF, TF, etc.). Controls CPU state and reflects results of ops. |
