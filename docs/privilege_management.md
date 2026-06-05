# Privilege Management

Each GDT Entry = 8 Bytes
segment selector = [(GDT index) << 3 ] | RPL

## Global Descriptor Table


```
           GDTR.base
              │
              ▼
+---------------------------+
| Offset |   Address        | Content
+---------------------------+
|  +0    | GDTR + 0x00      | NULL Descriptor
|        |                  | (mandatory, selector 0)
+---------------------------+
|  +8    | GDTR + 0x08      | Segment 1 Descriptor
|        |                  | (e.g. Kernel Code)
+---------------------------+
| +16    | GDTR + 0x10      | Segment 2 Descriptor
|        |                  | (e.g. Kernel Data)
+---------------------------+
| +24    | GDTR + 0x18      | Segment 3 Descriptor
|        |                  | (optional: User Code)
+---------------------------+
|  ...   | ...              | ...
+---------------------------+
```

## GDT ENTRY:

```
   64                    56      52      48           40           32
   ┌─────────────────────┬───────┬───────┬────────────┬────────────┐
   │        [8] Base     │ [4]   │ [4]   │   [8]      │   [8]      │
   │                     │ Flags │ Limit │  Access    │   Base     │
   └─────────────────────┴───────┴───────┴────────────┴────────────┘
   ┌─────────────────────────────────────┬─────────────────────────┐
   │             [16] Base               │       [16] Limit        │
   └─────────────────────────────────────┴─────────────────────────┘
   32                                   16                         0
```

## Terminology

| Level   | Privilege Full Name          | Description                       | Where stored                 |
| ---     | ---                          | ---                               | ---                          |
| DPL     | Display Privilege Level      | Who is allowed to access this     | Stored inside GDT / IDT      |
| RPL     | Requestor Privilige level    | Who is requesting to access this  | Stored in segment selector   |
| CPL     | Current Privilege level      | Reflects the current exec ring    | Hardware maintained          |

CPU internally computes:
- Effective Privilege Level (EPL) = max(CPL, RPL)
    - EPL > DPL -> #GP Fault

## Folder Structure:

```
kernel/
 └── proc/
      ├── user/
      │    ├── loader.c          # Loading ELF/flat binaries into address space
      │    ├── stack.c           # User stack allocation, mapping, setup
      │    ├── exec.c            # execve-like routines (replace address space, args, etc.)
      │    ├── syscall.c         # User-to-kernel syscall entrypoints
      │    ├── signal.c          # Signal delivery/trampoline logic (when you add signals)
      │    ├── user.h            # APIs/structs for user process management
      ├── scheduler.c            # Process scheduling logic (already present)
      ├── proc.c                 # Core process struct (PCB), alloc/free, PID management, etc.
      ├── context/
      │    └── context_switch.asm
      └── ...
```

Tets User mode process

Kernel Main -> enter_user_mode(
1.  Create the GDT entry for the user mode code and data segment
2.  Set up the stack and the segment selector.
3. Jump to user mode -> enter_user_mode(user eip, user esp)
4. Write a user mode function
5. map user mode stack to virtual address.
6. call enter_user_mode() after kernel init.
7. User_test runs -> calls INT 0x80 -> syscall.
8. 
