global isr_timer
extern timer_interrupt_handler

section .text

isr_timer:
	
	; CPU automatically pushes into the stack
	; EFLAGS, CS, EIP, SS:EP

	pusha
	call timer_interrupt_handler
	popa

	; Send EOI (0x20) - End of Interrupt handler.
	; PIC - manages hardware interrupt and allows only one interrupt at a time.
	; If interrupt handler finishes, EOI tells PIC it can send other interrupts.
	; For IRQ 0-7 -> Master PIC -> listens at port 0x20

	mov al, 0x20
	out 0x20, al

	iretd
