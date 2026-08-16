# Index

This note is divided into 2 phases:
1. Backgrounf
2. Final Design Decisions
3. Implementation Plan

# Background

## Glossary


| Concept            | Responsibility                                                                                                          |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------- |
| Terminal           | Human-facing input/output interface: keyboard events in, text/graphics out                                              |
| Terminal emulator  | Software implementation of a terminal, such as Kitty, Alacritty, Windows Terminal                                       |
| TTY                | Kernel-managed communication object providing terminal semantics                                                        |
| Line discipline    | Converts an unstructured byte stream into terminal behavior: lines, echo, special characters, signals                   |
| Shell              | Ordinary user program that reads commands and launches processes                                                        |
| UART/serial driver | Moves bytes between the kernel and serial hardware                                                                      |
| PTY                | Virtual connection that lets a user-space terminal emulator behave like terminal hardware                               |
| Console            | A specially designated terminal used for system/kernel interaction; the exact meaning differs between Linux and Windows |


## Software vs Hardware terminals

- terminal is a user facing endpoint
- tty is the kernel abstraction presented to application
- pty is a virtaul connection lets software impersonate terminal hardware.

### Flow for terminal:

```
        SOFTWARE TERMINAL            │             HARDWARE TERMINAL                     
                                     │                                                   
                              ┌──────┴────┐              ┌────┐                          
               ┌──────────────┤  keyboard ├─────────────►│UART│                          
               │              └──────┬────┘              └──┬─┘                          
               │                     │                      │interrup                    
               │                     │               ┌──────▼──────┐                     
               │                                     │SERIAL DRIVER│                     
               │                     │               └──────┬──────┘                     
               │                     │                      │data                        
      ┌────────▼──────────┐          │                      │                            
      │ TERMINAL EMULATOR │                             ┌───▼──────┐                     
      │ eg. Alacritty     │          │                  │tty_buffer│                     
      └───┬───────────────┘          │                  └────┬─────┘                     
          │                          │                       │Processing                 
          │                                                  │                           
          │                          │ ┌─────────────────────┼──────────────────────────┐
          │                          │ │ TTY DRIVER          │                          │
┌─────────┼────────────────────────┐ │ │ (TERMINAL      ┌────▼─────┐    ┌─────────┐     │
│ PTY.    │                        │   │  SCEMANTICS)   │LINE      ├─┬─►│Canonical│     │
│ (virtual│terminal connection)    │ │ │                │DISCIPLINE│ │  ├─────────┴───┐ │
│         │                        │ │ │                └──────────┘ └─►│Non Canonical│ │
│ ┌───────▼────┐ ┌──────────────┐  │ │ │                                └─────────────┘ │
│ │ MASTER     │ │SLAVE         │  │   │                                                │
│ │ terminal   │ │behaves like  │  │ │ │ ┌──────────┐ ┌───────┐ ┌──────────┐            │
│ │   emulator │ │normal TTY    │  │ │ │ │PTY DRIVER│ │QUEUES │ │TERMINAL  │            │
│ │ eg. tmux   │ │eg. bash, zsh ├──┼─┼─┼─►ENDPOINT  │ │ -INPUT│ │SETTINGS  │            │
│ └────────────┘ │  dev/pts/N   │  │ │ │ └──────────┘ │ -ECHO │ │(TERMINOS)│            │
│                └──────────────┘  │   │              └───────┘ └──────────┘            │
│                       ▲          │ │ │   dev/ttyS0                                    │
└───────────────────────┬──────────┘ │ └────────────────────────────────────────────────┘
                        │                           ▲                                    
                        │                           │                                    
                        │                           │                                    
                        │                           │                                    
                        └───┬┬─────────────────┬┬───┘                                    
              fd0:/dev/pts/4││ USER PROCESS    ││                                        
              fd1:/dev/pts/4││     (SHELL)     ││                                        
              fd2:/dev/pts/4└┴─────────────────┴┘                                        
```

### Historical Hardware terminal

it used to be a seperate device with connected keyboard, didsplay, serial device.
```
+----------------------+            +----------------------+
| Physical terminal    |  serial    | Linux computer       |
|                      |  cable     |                      |
| Keyboard             |----------->| UART RX              |
| Screen               |<-----------| UART TX              |
| VT100 processing     |            | serial driver        |
+----------------------+            +----------------------+
```
registerd in kernel as device /dev/ttyS0 (S0 for serial tty)

### Software Based terminal

- pty is kernel mechanish of connecting two user programs. like terminal emulator and shell.
- pty is itself not a terminal emulator, but is tightly linked, closing a terminal
emulator also closes the pty session.
- pty (pseudoterminal) is a pair of connected endpoint:
    pty master <========> pty slave


Q. Why use software terminal and not pipes if data needs to be transferred between
two programs.
A. tty layer provides interface enriched with information that process can use.
>    Pipes can transport bytes but do not provide features:
>    canonical mode/echo/display information etc...
> eg. terminal emulator(supplier process) and VIM (consumer process)
> VIM ask tty for window size, disable canonical mode, enable echo,
> resotres terminal session on exit. Ordinary pipes cannot provide above.

#### Software terminals can be layered.

PTY is not limited to GUI terminal. eg. ssh to remote shell.
```
                           ┌────────────────────────────┐                   
                           │ LOCAL PTY                  │                   
┌───────────────┐          │ ┌──────────┐   ┌─────────┐ │                   
│ LOCAL TERINAL │          │ │          │   │         │ │    ┌─────────────┐
│   EMULATOR    │ssh client│ │PTY MASTER│   │PTY SLAVE│ │    │SHELL PROCESS│
└───────────────┴──────────┼─►          ├──►│         ├─┼───►│(ssh client) │
                           │ └──────────┘   └─────────┘ │    └─────────────┘
                           │                /dev/pty/4  │           *       
                           └────────────────────────────┘        network    
                                                                    *       
                           ┌────────────────────────────┐           *       
                           │ REMOTE PTY                 │      ┌────────┐   
                           │ ┌─────────┐   ┌──────────┐ │      │  sshd  │   
 ┌─────────────┐           │ │         │   │          │ │      └────┬───┘   
 │   REMOTE    │           │ │PTY SLAVE│   │PTY MASTER│ │           │       
 │SHELL PROCESS│◄──────────┼─┤         │◄──┤          ◄─┼───────────┘       
 └─────────────┘           │ └─────────┘   └──────────┘ │                   
                           │ /dev/pty/7                 │                   
                           └────────────────────────────┘
```

#### Console Terminal (Type of Software terminal)

instead of the input going to shell, it gpoes to the console display backend.
process → TTY → VT emulator → VGA/framebuffer backend

### Different Terminal identifiers...
/dev/ttyS0 --> hardware terminal
/dev/pty/N --> software terminal
/dev/ptmx  --> Allocator/interface for creating a PTY master
/dev/ttyN  --> console terminal -> N=0 current active virtual console
/dev/console --> system console terminal


## TTY Principles

### Why TTY exists

TTY provides an structured scemantics to help process interacting with terminal.
TTY is an kernel object with exposed endpoints, process fd's can reference to
these endpoints.
The basic pattern is:
```
                         PROCESS
                            │
                    read()/write()
                            │
                            ▼
                           VFS
                            │
                            ▼
                    tty endpoint (/dev/ttyN)
                    ┌─────────────┐
                    │ TTY CORE    │
                    │             │
                    │ input state │
                    │ mode/policy │
                    │ wait state  │
                    │ output path │
                    └──────┬──────┘
                           │
             ┌─────────────┴─────────────┐
             │                           │
        input backend               output backend
             │                           │
        input driver                output driver
```

### Exploring TTY Core Architecture

TTY buffer management is the core of the tty module. The bytes might be copied
from the input source to the tty buffer but it might not be readable based on
the semantics. based on the buffer processing, we can categorize into.
1. Classic TTY 
2. Terminal stream object
3. Pipeline oriented terminal

#### Classic TTY...

UART -> bytes ready -> send interrupt -> serial driver -> tty buffer 
-> tty core -> tty line processing
Process requests bytes -> stdin_read -> reads data from tty buffer

#### Terminal Stream Object...

UART -> bytes ready -> send interrupt ->
stdin_read -> terminal object => input queue
terminal object will hold:
    -> input queue
    -> output queue 
    -> policy (line discipline)
=> output queue -> VFS

#### Pipeline oriented terminal...

UART -> bytes ready -> send interrupt ->
stdin_read (input source) -> byte transport ->
input filter -> terminal session -> consumer.

## Decision making points:

1. wait ownership -> global wait or per tty wait object
2. stream -> terminal offset/lseek needed?
3. syscall_read (read_len) for ->
    -> regular files => return after entire read_len present
    -> stream/tty => return avaialable data 

## Terminal in Linux vs Windows

# Design Decisions:

## 1. TTY independent module - from Hardware drivers and Process Implementation

1. Hardware driver transport - independent - terminal semantics.
2. Process operation impl - independent - terminal semantics.
3. Input byte in tty buffer --> X --> process ready to read
4. tty recieve implementation --> execute from interrupt context.
5. Blocked Process --> should not wake up and find data not available.
6. tty gets concurrent interrupts from UARt -->  process does concurrent
   read/write.
7. stdin/stdout/stderr - can refer to terminal object (tty) but are not
   themselves.


```
                         PROCESS
                            │
                    read()/write()
                            │
                            ▼
                           VFS
                            │
                            ▼
                    ┌─────────────┐
                    │     TTY     │
                    │             │
                    │ input state │
                    │ mode/policy │
                    │ wait state  │
                    │ output path │
                    └──────┬──────┘
                           │
             ┌─────────────┴─────────────┐
             │                           │
        input backend               output backend
             │                           │
          UART RX                  UART / VGA
             │                           │
         hardware                    hardware
```

# Implementation Plan

1. Minimal TTY object
2. Move RX queue from console_input → TTY
3. Connect UART RX → TTY
4. Route /dev/console and stdin through TTY
5. Blocking/wakeup semantics
6. Raw mode
7. Canonical input
8. Echo + erase/backspace
9. CR/LF transformations
10. terminal configuration API
11. adversarial tests
12. architecture retrospective

```
┌────┐  ┌─────┐  ┌─────────┐    ┌────────┐
│USER├─►│UART ├─►│TTY INPUT├───►│PRROCESS│
└────┘  └─────┘  └────┬────┘    └────────┘
                      │                   
              ┌────┐  │                   
            ┌─┤echo│◄─┘                   
            │ └────┘                      
            ▼                             
┌───┐    ┌──────────┐  ┌───────┐  ┌──────┐
│VGA│◄───┤TTY OUTPUT│◄─┤PROCESS│◄─┤OUTPUT│
└───┘    └──────────┘  └───────┘  └──────┘
```


