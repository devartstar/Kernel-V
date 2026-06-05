## At the time of the syscall interrupt

1. Information about syscall:-
	eax = syscall number
	ebx = arg1
	ecx = arg2
	edx = arg3
	esi = arg4
	edi = arg5
	ebp = arg6
	int 0x80
2. Executing in usermode :
	CPL = 3
	CS	= User code segment address
	EIP = address of the int 0x80 instruction
	SS:ESP = User stack segment
	CPU gets the instruction of 0x80 which it interprets as Software Interrupt
3. CPU has a special register called IDTR
	IDTR points to the base address and the size of the IDT
	for 0x80 - CPU computes
		descriptor_address = IDT.base + (0x80 * sizeof(IDT entry))
		In 32 bit protected mode - each IDT entry size is 8 Bytes
		CPU gets the address of IDT[128] entry
4. IDT[128] entry contains:
	CS::offset -> entry point for the interrupt
	Gate Type -> interrupt or trap
	DPL -> for 128 Privilege Level = 3 (User)
	Present
5. CPU checks CPL <= IDTgate.DPL (3<=3) 
	DPL = Descriptor Privilege level (which privliege is allowed to use this
	interrupt gate)
	- inter syscall interrupr from user privilege level
	- if DPL = 0 - then GP fault
6. IDT Gates target CS DPL = 0, Current CPL = 3
	Hence a privilege level change is needed, cpu needs to switch stacks
7. CPU loads kernel stack from TSS
	for Target CPl = 0 -> it uses SS0 and ESP0 from TSS
8. CPU pushes the old state into the Kernel Stack
	CPU builds the interrupt frame, since privilege change CPU pushes old user
	state in new kernel stack:
	old SS
	old ESP
	EFLAGS
	old CS
	old EIP
	^ using this iret later can return back to user mode
9. CPU updates its current execution state
	CS = Kernel Code selector from IDT gate
	EIP = Handler offset from IDT gate
	Interrupt gate -> CPU clears IF in EFLAGS
	Trap gate -> IF left unchanged
	Now execution is in Kernel Mode
10. CPU starts executing syscall entry stub for isr128: ...
	CPU had saves SSS< ESP, EFLAGS, CS, EIP but not volatile user registers
	It saves EAX, EBX, ECX, EDX, ESI, EDI, EBP, DS/ES/FS/GS -> contains syscall number
	and args from user mode
11. Kernel gets to know the syscall number from eax register
	Accordingly it calls the syscall handler method
12. Return Path
	After handler completes running execution comes to assembly stub isr128:...
	it restores saved state and general regster
	then it executes iret
13. Execution resumes in user mode from the old EIP
	eax register contains the syscall return value