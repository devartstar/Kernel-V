# Terminals

1. Understand stdin, stdout, stderr and the process FD table.
2. Understand terminal devices and /dev/tty.
3. Understand /dev/console and kernel console backends.
4. Compare /dev/tty with /dev/console.
5. Map everything into Kernel-V’s VFS and process model.

## 0. Introduction: Layered Modes

1. Application Layer: shell, Vim, Kernel logger, etc.
2. OS Abstraction:

   1. Process opens file descriptors (FILE\_OBJECT)
      `/dev/stdin`, `/dev/stdout`, `/dev/stderr`
   2. file descriptors reference the device node (DEVICE\_OBJECT)
   3. device nodes have pre-registered read/write routine. (DEVICE\_OBJECT->NODE\_OPS->READ/WRITE)
   4. READ/WRITES are backed by **TTY** (What is TTY? will figure out)
   5. TTY uses **console** as a centralized buffer
3. **Device Drivers**. Keyboard driver / VGA driver / Serial driver.
4. **Hardware**. Keyboard / VGA Memory / UART

* Input Sources: Keyboard, UART
* Output Sources: VGA, UART
* Console: combines input sources and an output destination
* TTY: is a kernel interface for terminal behavior. eg. line buffering, echoing, backspace, controls.
* Device nodes like /dev/ttyS0 exposes the kernel interface through Virtual File System (VFS) layer.
* stdin, stdout, stderr - are file descriptors pointing to /dev/ttyS0 type device nodes.

### Plan: Bottom Up Approach

1. RAW Hardware
2. Kernel Driver Interface
3. Console subsystem
4. TTY subsystem
5. VFS and Process

---

## 1. RAW Hardware.

There are two ways to access hardwareperipheral devices.

1. Port Mapped IO (PMIO) - CPU `inb() / outb()`
2. Memory Mapped IO (MMIO) - normal memory instructions `mov reg1, reg2`

---

### 1.1 IO Ports

* x86 provides a seperate IO-Address space containing upto (0-0xFFFF) IO ports.
* CPU needs to know when device ready for an operation: Polling (CPU querying device) / interrupts (device signaling CPU)

  * ***Polling:*** CPU reads a device status register repetadely until device ready.
    UART uses I/O port 0x3FD - reading status register.

    * output status: LSR bit 5 = 0 (UART device not ready) / 1 (UART device ready to get)
    * input status: LSR bit 0 = 0 / 1 (CPU can read data from UART)
  * ***Interrupt:*** when device is ready - it raises an IRQ - cpu runs the handler for the IQR
    eg. Serial bitstream arrives -> UART reconstruct byte -> UART store in data register -> UART raises IRQ4 ->
    PIC signal CPU -> IRQ4 handler -> reads form data register -> store in console buffer
    Write to console buffer (quick) and the process read can compete from console buffer.
* Eg. CPU -> outb(0x3F8, 'A') -> I/O address bus -> I/O port 0x3F8 -> data register -> gets data 'A'(0x41) -> UART TX pin transmits serial bits.

#### Questions:

> 1. IO Address space?
a separate address namespace. CPU uses it access device registers.

```
CPU ---> memory bus ---> RAM/MMIO
---> I/O bus ---> Device
```

> 2. What is a bus?

* Bus: communication path/protocol between CPU and outside CPU
* Bus contains 3 information:

  * address -> location of the device register
  * data -> value to write to register
  * controls -> operation on device register

> 2. How does a device get address space?

1. fixed hardware decoding - eg. UART COM base i/o address = 0x3f8

2. software configurable devices (PCI) -

   bios/uefi does the initial allocation and os may reassign the resources.

   BIOS/UEFI --> PCI enumeration --> Find pci discoverable devices --> Assign BAR

   Every PCI Function --> BARs (Base Address register) --> tells what resource device needs?

   BAR0 = 0xFEB00000 - device needs MMIO space, sizr of the MMIO range

   BAR1 = 0xC00 - device needs I/O Port, I/O port range

> 3. How is MMIO different from i/o port?

* MMIO := device register lives in the ***physical memory address space***.
  driver --> virtual address --> MMU / Page Tables --> physical MMIO address --> backed by PCI device BAR. (not RAM)
  Access: `mov [address], value` --> address = (BAR + offset)
* I/O Port := device register lives in a ***separate i/o address space***
  Access: `out dx, al` or `in al, dx`

> 4. What is UART?

* UART is the hardware which does all the physical tranmission of bits.
* UART registers in I/O address space.

```
0x3F8  data register
0x3F9  interrupt enable
0x3FA  interrupt status
0x3FB  line control
0x3FC  modem control
0x3FD  line status
```

* CPU talks to the UART using I/O port.
  UART talks to the outsize works using TX/RX wires.
* UART (TX pin) --> Reciever (RX pin) also the flow can we opposite way around.

---

### 1.2 VGA text output

VGA test mode provides a hardware backed character grid.

0xB8000 is physical memory address range (MMIO range) reserved for VGA text video memory.

Kernel write -> character -> 0xB8000 physical memory -> VGA reads the buffer -> character displayed on **screen**

**Screen:** 80 cols \* 25 rows = 2000 character cells. each character cell contains 2 byte. -> 4000 bytes starting from 0xB8000

character cell: Byte 0 (ASCII character) \& Byte 1 (color/attributes)

#### Questions

> 1. Address spaces? what is meany by physical address space?

* Virtual address: address used by code when paging is enabled
* physical memory address: address seen after MMU translation used for RAM, MMIO, VGA memory, device registers
* I/O port address: sed by in/out instructions example: 0x3F8, 0x3D4, 0xCF8
* Disk address: sector number / LBA. used by disk controller commands

Virtual address --> page table --> physical address --> MMU --> Hardbare devices

misconception: physical address ≠ RAM address always

Physical   → Backed Hardware (MMIO backed)

0x00100000 → probably RAM

0x000B8000 → VGA text memory

0xFEC00000 → local/APIC-related MMIO

0xFEB00000 → maybe PCI device register window

---

### 1.3 PS/2 keyboard input

* A keyboard doesn't directly send a character input 'A' to the CPU.

* Flow for an keyboard event:

```
Physical key event -> Keyboard sends a scan code -> PS/2 controller stores code -> controller raises IRQ1
-> CPU IRQ1 handling -> keyboard driver reads I/O port 0x60 -> driver interpret the scan code
-> scan code can resemble character / control event
```

* Lets walk through an example.
  User press key `A` on keyboard -> sends scan code "key pressed" (scan code 0x1E) -> PS/2 controller -> ...
  User release key `A` -> sends scan code "key released" (set bit 7 => 0x80 | 0x1E => 0x9E) -> PS/2 controller -> ...

* key pressed is called "Make Code", key released is called "Break Code"
  For ordinary keys "Break Code" = 0x8(7th bit) | "Make Code"

* There are different types of keys:

  * Character key: normal alphanumeric keys.
  * Modifier keys: Shift / Ctrl / Alt are modifier keys -> themselves don't have any affect, needs a follow up key
  * Persisted keys: Caps-Lock key is persisted even on release.

* Q: Why keyboard doesn't directly send character => 'a'
  A: same physical key can produce diff meanings:

  * shift + 'a' => 'A'
  * CapsLock + 'a' => 'A'
  * ctrl + 'a' => control value 0x01 (select all)

* So keyboard only reports key activity. keyboard driver tracks the state and make sense of combination of key activity.

* I/O Port => 0x60 data port

  * Once CPU reads the scan code from 0x60 -> the buffer is cleared. next byte becomes available

* I/O Port => 0x64 status port

  * Bit 0 -> output buffer full (byte is waiting to be read by the CPU) => trigger controller -> CPU
  * Bit 1 -> input buffer being written by CPU => processing CPU -> controller

#### Three different kind of states:

1. Physical state: Shift/Ctrl/Alt are currently held
2. Lock state: Caps Lock/Num Lock mode is enabled
3. Key event: A particular key was pressed or released

A decoder in the keyboard driver combines these scan codes into a normalize event.

#### Keyboard Controller:

A hardware controller (called 8042 PS/2 controller) sits between hardware (keyboard) and the CPU.

keyboard --> generate scan code --> serial PS/2 connection --> PS/2 controller (stores scan code) --> IRQ1 (controller buffer is ready)

--> CPU IRQ1 handler --> CPU reads buffer --> I/O port 0x60 --> CPU (kernel driver) gets the keyboard event

--> keyboard driver make an action based on sequence of events.

NOTE: Reading from the controller and completing the interrupt are separate acknowledgement
PS/2 controller --> IRQ1 --> PIC -> generated the IDT index -> CPU invoke keyboard handler -> reads buffer from IO port 0x60

1. -> controller buffer is free now for next byte --> IRQ1
2. -> In background -> keyboard handler process events and make an action -> sends EOI -> PIC

#### keyboard input needs a software buffer

* keyboard input can be async without a process requesting it. so we need to preserve the events until process request it.
* keyboard key press -> PS/2 controller -> IRQ1 -> interrupt handler reads 0x60 -> kernel store in software buffer -> later -> process reads
* software buffer is then filled with events -> maintains 2 position one for read/write.
* process read --> CASE 1. --> Buffer contains data --> returns the data

  --> CASE 2. --> Buffer is empty -> block calling process -> schedule another process.

      keyboard -> IRQ1 -> ISR invoke the handler routine to store the value in buffer -> ISR marks blocked process as runnable.

      process completes read.

* CASE 2. process waits without consuming/blocking CPU.
* software buffer can exists at multiple layers
  -> keyboard event buffer: stores normalized events.
  -> TTY input buffer: stores terminal oriented events.
* What happens on buffer full? Kernel needs to have an explicit policy.

#### Keyboard Input to process stdin

* Keyboard driver doesn't directly write to process stdin.
* Keyboard driver supplies input to terminal / tty device object. the vfs layer exposes this as a device file object /dev/tty
* keyboard hardware --> PS/2 controller --> IRQ1 --> keyboard driver (buffer) --> character --> TTY input processing (buffer) --> buffer --> VFS device file registered write routine --> process file descriptor.
* The TTY sits between keyboard and process:
  The keyboard layer produces characters or symbolic key events. The TTY determines how terminal input should behave.
  eg. Keyboard produces: h e l l o Enter -> TTY collects "hello\\n" -> Process read() receives the line
* this configuration of TTY in between allows TTY interface to receive input from different hardware
* TTY Processing has 2 modes:

  * Canonical Mode: buffers an entire line. read remains blocked until "enter" key pressed.
  * Raw Mode: reads every character immediately.
* TTY echoing: keyboard character 'A' -> TTY input buffer -> console output -> VGA display
  Input: keyboard -> TTY -> stdin
  Output: stdout -> TTY -> VGA

---

### 1.4 Combining terminal input/output path

* Summarizing the flow:
  ***Write:*** Process write (fd 1) --> VFS file write --> write to TTY buffer --> console buffer --> VGA
  ***Read:*** keyboard --> PS/2 controller --> interrupt --> keyboard driver --> console buffer --> TTY buffer --> VFS file --> Process read (fd 0)

* what is a terminal? kernel interface to exchange information between process and user.
* terminal will have an input path and an output path with different handling hardware.
* keyboard --> input path handling --> process reads
  process --> output path handling --> Display
* local terminal: input hardware = PS/2 keyboard, output hardware = VGA device
* serial terminal: input hardware = UART receiver, output hardware - UART transmitter
* Device Driver => controls specific hardware
  TTY => provides terminal behavior
  stdin/out => process descriptors referring to open tty

#### Console:

Terminal is a software layer connecting process with hardware devices.
* tty is a generic term for terminal behaviour. console is the terminal selected for system interaction.
* `/dev/console` => is a device node. this node does not contain ordinary file data.
  This device node -> file operations -> connects to driver -> for underlying device which can be VGA/ UART.
* Process -> Opening `/dev/console` device -> file object ref and a fd (console_fd).
  reading/writing on console_fd -> invokes console operations VGA/UART -> not writing to a disk block.
* console vs process terminal:
  * /dev/console -> system-selected console.
  * /dev/tty -> process controlling TTY
  * each process will have its own /dev/tty reference but same /dev/console

| Device node    | Resolves to                       | Selection basis                           |
| -------------- | --------------------------------- | ----------------------------------------- |
| `/dev/console` | System console                    | Chosen globally by the kernel             |
| `/dev/tty`     | Calling process’s controlling TTY | Depends on the process                    |
| `/dev/tty0`    | Currently active virtual console  | Depends on which local console is visible |
| `/dev/tty1`    | Virtual console 1                 | Fixed virtual terminal                    |
| `/dev/ttyS0`   | First serial terminal             | Fixed UART device                         |


- /dev/tty0 - this is the active virtual terminal. ie. the terminal displayed on screen VGA. 
  - Only one terminal is normally visible on a screen at a time. /dev/tty0 is the terminal currently active.
- /dev/ttyS0 - this is a first serial terminal backed by UART.
  - Unlike /dev/tty - the target of /dev/ttyS0 does not depend on calling
  process.
  - Unlike /dev/tty0 - the target of /dev/ttyS0 does not depend on the virtual
    console visible

1. What is software terminal? or virtual console?
- Hardware terminal - historically terminal used to be a seperate hardware.
- Software terminal - kernel created interface. ie. kernel stores information
like: cursor position, screen state, echo and line processing, etc.

2. difference between /dev/tty and /dev/tty0 ?
/dev/tty is a callers controlling terminal. where as /dev/tty0 is the currently
displayed virtual console. eg. a running process is controlled by virtual
console /dev/tty2. the console currently visible on screen is /dev/tty1.
so for the process -> /dev/tty = tty2 and /dev/tty0 = tty1.

3. Lets clarify the understanding based on an example.

Setup:
```
machine is connected to only one keyboard and VGA screen.
kernel -> creates 2 virtual/software terminals:
-> tty1: kernel object (screen buffer 1, cursor position 1)
-> tty2: kernel object (screen buffer 2, cursor position 2)
-> tty3: kernel object (screen buffer 3, cursor position 3)
```

Action:
```
current active: tty1 -> /dev/tty => /dev/tty1
-> user -> types a line -> PS/2 controller -> CPU interrupt handle 
  -> driver write -> char device /dev/tty -> kernel updates object for tty1 
  -> /dev/tty0 is /dev/tty1 -> update VGA display.
-> user switch active terminal to tty2 
  -> /dev/tty0 is updated to /dev/tty2.
  -> only changes to char device /dev/tty2 will affect VGA display.
-> in background the process keeps writing to /dev/tty1 
  -> it updates the kernel object for tty1 
  -> dev/tty0 is /dev/tty2 so nothing on VGA display.
```

#### tty and file desctiptors.

- each process has 3 standard file descriptors. fd0 (stdin), fd1(stdout),
fd2(stderr).
- they all ultimately connect to the same tty (access same kernel object)
```
Process FD table
┌──────┬──────────────────┐
│ fd 0 │ terminal file    │──┐
│ fd 1 │ terminal file    │──┼──► tty1 ──► PS/2 + VGA
│ fd 2 │ terminal file    │──┘
└──────┴──────────────────┘
```

does that mean fd0, fd1, fd3 - each referenct to a sparate file - backed by sa  metty devic?Not necessarity
- case 1: 3 different file objects. each can have seperate access levels.
  - fd0 -> input vfs_file -> tty1
  - fd1 -> output vfs_file -> tty1
  - fd2 -> error vfs_file -> tty1
- case 2: same file_object

- its a good idea to have sperate vfs_file object for stdout and stderr.
setup: `fd0 -> tty1, fd1 -> output.txt, fd2 -> tty1`
  In this setup normal output gets added in the file output.txt and error gets
  displayed on the screen.
- does tty use offset ? file_objects have an offset entry in their object. read/write on that file object reacts on that offset.
- lseek has no meanning in case of character device, as they dont deal with
offset as tty generally deals with streams.
