### Nested Interrupt

- *Interrupt Lifecycle:* 
`Interrupt start -> handler -> end of interrupt (eoi)`
- *Nested interrupt:* interrupt being fired when another interrupt handler is still running.
    - CPU pauses in progress handler and runs new one.
    - CPU resumes the first one.
    - Basically handlers stack on top of one another.

- Interrupt Fires:
    1. CPU Saves EFLAGS, CS, EIP and (SS, ESP)
    *NOTE:* EFLAGS contains the IF.
    2. CPU Clears IF (disable interrupts)
    3. Runs the handler
    4. iret - pops back the Saved EFLAGS, CS, IF.

- From above if CPU clears IF, so no interrup can be recieved during handler execution, so how does nested interrup work?
    - When handler explicity sets IF (allows nested interrupt)
    - SO NESTING IS OPTIONAL...

- Nested Interrupt Test:
    1. Define a test interrupt at IDT - 35
    2. Interrupt fired -> CPU disabled interrupt -> handler
    3. handler -> irq save() -> enables interrupt back.
    4. handler -> long work (while loop wait) <--> Allowing nested interrupts to kick in (increment count).
    5. handler -> restore flags

- Interrupt Nesting Concept:
    1. PIC ranks IRQ.
        IRQ 0(timer) is higher priority than IRQ 3(test)
        A higher priority interrupt can nest a lower priority interrupt (which has enabled interrupt using sti)
    2. Masking Interrupts:
        Interrupt Mask register. If we set the timer bit in Interrupt Mask register, masked interrupt can not be nested to original interrupt.

- Problem: Mixing Interrupt nesting with Context Switch.
```
IDT 35(test) handler running (IF enabled, nested interrupt)
    -> Nested interrupt IDT 32(timer)
        -> time-slice(0) -> yeild -> switch_to(new_process)
            -> swaps kernel stack with new process.
                -> switch back → resumes mid-handler, but the unwind keeps getting deferred, so the interrupt never completes.
                    -> never calls iret.
```

```
kernel_main kernel stack (grows downward)
┌─────────────────────────────────────────────┐
│ kernel_main normal code                      │
├─ IRQ35 entry (CPU auto-push) ────────────────┤
│   EFLAGS35, CS35, EIP35  ← return to kernel_main
│   pusha / segs (isr stub)                    │
│   isr_common_handler                         │
│   test_interrupt_handler   ← ran `sti`       │
├─ IRQ32 entry (CPU auto-push, NESTED) ────────┤
│   EFLAGS32, CS32, EIP32  ← return INTO the loop
│   pusha / segs                               │
│   isr_common_handler                         │
│   timer_interrupt_handler                    │
│   timer_interrupt_proc_handler               │
│   yield()                                    │
│   switch_to()  ← ESP saved HERE              │
└─────────────────────────────────────────────┘

What switch_to actually saves to stack
mov [eax + PCBCTX_EBX_OFFSET], ebx   ; general regs
...
mov [eax + PCBCTX_ESP_OFFSET], ecx   ; ESP
mov [eax + PCBCTX_EIP_OFFSET], ecx   ; return EIP

It saves general registers + ESP + EIP only. It has no concept of "I am currently 2 interrupts deep." A context switch was designed to happen at a clean, top-level call boundary (like the yield() inside a normal thread loop), where there is nothing pending underneath.
```