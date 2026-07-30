#include "proc/syscall.h"
#include "fs/fd.h"
#include "fs/vfs.h"
#include "fs/vfs_utils.h"
#include "lib/print_macros.h"
#include "lib/printk.h"
#include "mm/paging.h"
#include "proc/proc.h"
#include "proc/scheduler.h"
#include "proc/user.h"

#define KBUF_CHUNK_SIZE 128

static uint8_t usr_ptr_validate(const void *ptr);
static uint8_t usr_range_is_valid(const void *ptr, uint32_t len);
static int copy_user_string(char *kdst, const char *usrc, uint32_t max_len);
static int copy_from_user(void *kdst, const void *usrc, uint32_t len);
static int copy_to_user(void *udst, const void *ksrc, uint32_t len);

syscall_handler_t syscall_table[NUM_SYSCALLS] = {0};

static __attribute__((unused)) int32_t syscall_test(uint32_t a, uint32_t b,
                                                    uint32_t c, uint32_t d,
                                                    uint32_t e, uint32_t f) {
    KLOG_VERBOSE("SYSCALL",
                 "SYSCALL TEST METHOD with argument a=%u, b=%u, c=%u, d=%u, "
                 "e=%u, f=%u\n",
                 a, b, c, d, e, f);
    return 0xDEADC0DE;
}

static char *syscall_get_name(uint32_t syscall_num) {
    switch (syscall_num) {
    case SYS_EXIT:
        return "SYSCALL_EXIT";
    case SYS_WRITE:
        return "SYSCALL_WRITE";
    case SYS_GETPID:
        return "SYSCALL_GETPID";
    case SYS_SCHED_YIELD:
        return "SYSCALL_SCHED_YIELD";
    case SYS_OPEN:
        return "SYSCALL_OPEN";
    case SYS_READ:
        return "SYSCALL_READ";
    case SYS_CLOSE:
        return "SYSCALL_CLOSE";
    case SYS_LSEEK:
        return "SYSCALL_SEEK";
    default:
        return "UNKNOWN_SYSCALL";
    }
}

static int32_t syscall_exit(uint32_t code, uint32_t _2, uint32_t _3,
                            uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_2;
    (void)_3;
    (void)_4;
    (void)_5;
    (void)_6;

    if (!current_proc) {
        panik("Syscall Exit: current proc is null\n");
    }

    KLOG_INFO("SYSCALL",
              "syscall_exit called: code=%d, pid=%d name=%s, type=%s\n", code,
              current_proc->pid, current_proc->name,
              proc_type_to_string(current_proc->type));

    if (proc_is_special(current_proc)) {
        panik("syscall_exit called on a speciall process");
    }

    /* Mark the process as terminated and yield */
    proc_mark_terminated(current_proc, code);
    dequeue_ready(current_proc);
    yield();

    /* should not reach here */
    panik("syscall_exit returned after yield, should not reach this code\n");

    return code;
}

static int32_t syscall_write(uint32_t _fd, uint32_t _user_buf, uint32_t _len,
                             uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_4;
    (void)_5;
    (void)_6;

    int32_t fd = (int32_t)_fd;
    uint32_t len_to_write = _len;
    const void *ubuf = (const void *)_user_buf;

    char kbuf[SYSCALL_IO_BUFSZ + 1];
    uint32_t total_write_len = 0;
    uint32_t chunk_to_write = 0;
    int32_t ret;

    /* current process should be valid as fd refers to the file from current
     * process */
    if (!current_proc) {
        KLOG_ERROR(
            "SYSCALL",
            "syscall_write: failed. invalid reference to current process.\n");
        return VFS_ERR_INVALID;
    }

    /* if nothing to write just return */
    if (len_to_write == 0) {
        KLOG_VERBOSE("SYSCALL", "syscall_write: complete. nothing to write.\n");
        return 0;
    }

    /* check for valid user buffer and range to be within user memory region*/
    if (!ubuf) {
        KLOG_ERROR(
            "SYSCALL",
            "syscall_write: failed. invalid reference to user buffer.\n");
        return VFS_ERR_INVALID;
    }

    if (!usr_range_is_valid(ubuf, len_to_write)) {
        KLOG_ERROR("SYSCALL",
                   "sycall_write: invalid user buffer=0x%08x, length=%u\n",
                   ubuf, len_to_write);
        return VFS_ERR_INVALID;
    }

    /* write in size of chunks until entire buffer has been writen */
    while (total_write_len < len_to_write) {
        uint32_t chunk_to_write = len_to_write - total_write_len;
        if (chunk_to_write > KBUF_CHUNK_SIZE) {
            chunk_to_write = KBUF_CHUNK_SIZE;
        }

        /* Copy the buffer in kernel side before writing */
        ret = copy_from_user(kbuf, (const uint8_t *)ubuf + total_write_len,
                             chunk_to_write);
        if (ret != VFS_OK) {
            KLOG_WARN("SYSCALL",
                      "syscall_write: partial. total write len = %u, expected "
                      "write len = %u.\n",
                      total_write_len, len_to_write);
            return total_write_len > 0 ? (int32_t)total_write_len : ret;
        }

        /* write to the file referenced by fd */
        /* for fd = 0/1/2 write to the standart input output */
        ret = fd_write(current_proc, fd, kbuf, chunk_to_write);

        if (ret < 0) {
            KLOG_WARN("SYSCALL",
                      "syscall_write: partial. total write len = %u, expected "
                      "write len = %u.\n",
                      total_write_len, len_to_write);
            return total_write_len > 0 ? (int32_t)total_write_len : ret;
        }

        if (ret == 0) {
            break;
        }

        total_write_len += (uint32_t)ret;
    }

    KLOG_VERBOSE("SYSCALL", "syscall_write: completed. total write len = %u.\n",
                 total_write_len);
    return (int32_t)total_write_len;
}

static int32_t syscall_getpid(uint32_t _1, uint32_t _2, uint32_t _3,
                              uint32_t _4, uint32_t _5, uint32_t _6) {

    (void)_1;
    (void)_2;
    (void)_3;
    (void)_4;
    (void)_5;
    (void)_6;

    KLOG_INFO("SYSCALL", "syscall_getpid: %d\n", current_proc->pid);

    return current_proc->pid;
}

static int32_t syscall_sched_yield(uint32_t _1, uint32_t _2, uint32_t _3,
                                   uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_1;
    (void)_2;
    (void)_3;
    (void)_4;
    (void)_5;
    (void)_6;

    KLOG_VERBOSE("SYSCALL", "syscall_sched_yield: pid=%d name=%s\n",
                 current_proc->pid, current_proc->name);

    yield();

    return 0;
}

static int32_t syscall_open(uint32_t path_ptr, uint32_t flags, uint32_t _3,
                            uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_3;
    (void)_4;
    (void)_5;
    (void)_6;

    char kpath[VFS_PATH_MAX];
    int ret;

    /* never pass path directly to VFS. Copy to kernel owned buffer first */
    ret = copy_user_string(kpath, (const char *)path_ptr, VFS_PATH_MAX);
    if (ret != VFS_OK) {
        KLOG_ERROR("SYSCALL",
                   "syscall_open failed. failed to copy path from user "
                   "ptr=0x%08x to kernel buffer. status = %s.\n",
                   path_ptr, vfs_get_status_string(ret));
        return ret;
    }

    KLOG_VERBOSE("SYSCALL",
                 "syscall_open: path=%s flags=0x%x for process %s (pid=%u).\n",
                 kpath, flags, current_proc->name, current_proc->pid);
    return fd_open_path(current_proc, kpath, flags);
}

static int32_t syscall_read(uint32_t _fd, uint32_t _user_buf, uint32_t _len,
                            uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_4;
    (void)_5;
    (void)_6;

    int fd = (int)_fd;
    const void *ubuf = (const void *)_user_buf;
    uint32_t len_to_read = _len;

    char kbuf[SYSCALL_IO_BUFSZ + 1];
    int ret;
    uint32_t chunk_to_read;
    uint32_t total_read_len = 0;

    /* fd belongs to the current process, hence it should be running */
    if (!current_proc) {
        KLOG_ERROR("SYSCALL",
                   "syscall_read failed for fd %u. invalid reference to "
                   "current process.\n",
                   fd);
        return VFS_ERR_INVALID;
    }

    /* handle case of nothing to read */
    if (len_to_read == 0) {
        return 0;
    }

    /* validate the user buffer passed as argument
     * 1. the buffer should be valid
     * 2. entire buffer should be inside user memory region.
     */
    if (!ubuf) {
        KLOG_ERROR("SYSCALL",
                   "syscall_read failed for fd %u. invalid user buffer to read "
                   "into.\n",
                   fd);
        return VFS_ERR_INVALID;
    }

    if (!usr_range_is_valid(ubuf, len_to_read)) {
        KLOG_ERROR("SYSCALL",
                   "syscall_read failed for fd = %u. user buffer %p of length "
                   "%u references memory outside user region.\n",
                   fd, ubuf, len_to_read);
        return VFS_ERR_INVALID;
    }

    /* read in chunks of max buffer into kernel buffer and copy it to user
     * buffer */
    while (total_read_len < len_to_read) {
        /* construct the chunk size based of the size left to read */
        chunk_to_read = len_to_read - total_read_len;
        if (chunk_to_read > SYSCALL_IO_BUFSZ) {
            chunk_to_read = SYSCALL_IO_BUFSZ;
        }

        /* read chunk size of data from the file referenced by fd */
        ret = fd_read(current_proc, fd, kbuf, chunk_to_read);

        /* verify for successful read before copying to user buffer
         * if read of current chunk failed, return the total read upto now as
         * data is already copied to buffer. */
        if (ret < 0) {
            KLOG_WARN("SYSCALL",
                      "syscall_read: failed to read complete data. total read "
                      "length = %u, expected read length = %u.\n",
                      total_read_len, len_to_read);
            return total_read_len > 0 ? (int32_t)total_read_len : ret;
        }

        if (ret == 0) {
            break;
        }

        /* copy the chunk of data read into the user buffer */
        if (copy_to_user(ubuf + total_read_len, kbuf, (uint32_t)ret) !=
            VFS_OK) {
            KLOG_WARN("SYSCALL",
                      "syscall_read: failed to copy data to user buffer. total "
                      "read length = %u, expected read length = %u.\n",
                      total_read_len, len_to_read);
            return total_read_len;
        }

        /* update the toal read length */
        total_read_len += (uint32_t)ret;
    }

    KLOG_VERBOSE("SYSCALL",
                 "syscall_read: completed. fd=%d requested=%u read=%u\n", fd,
                 len_to_read, total_read_len);
    return total_read_len;
}

static int32_t syscall_close(uint32_t _fd, uint32_t _2, uint32_t _3,
                             uint32_t _4, uint32_t _5, uint32_t _6) {
    int32_t fd = (int32_t)_fd;

    (void)_2;
    (void)_3;
    (void)_4;
    (void)_5;
    (void)_6;

    if (!current_proc) {
        KLOG_ERROR(
            "SYSCALL",
            "syscall_close: failed. invalid refernece for current process.\n");
        return VFS_ERR_INVALID;
    }

    return fd_close(current_proc, fd);
}

static int32_t syscall_lseek(uint32_t _fd, uint32_t _offset, uint32_t _whence,
                             uint32_t _4, uint32_t _5, uint32_t _6) {
    int32_t fd = (int32_t)_fd;
    int32_t offset = (int32_t)_offset;
    int32_t whence = (int32_t)_whence;

    (void)_4;
    (void)_5;
    (void)_6;

    if (!current_proc) {
        KLOG_ERROR(
            "SYSCALL",
            "syscall_lseek: failed. invalid reference to current process.\n");
        return VFS_ERR_INVALID;
    }

    return fd_lseek(current_proc, fd, offset, whence);
}

void syscall_table_init(void) {
    /* Register default handler (ENOSYS) for all syscalls */
    for (int8_t i = 0; i < NUM_SYSCALLS; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[SYS_EXIT] = syscall_exit;
    syscall_table[SYS_WRITE] = syscall_write;
    syscall_table[SYS_GETPID] = syscall_getpid;
    syscall_table[SYS_SCHED_YIELD] = syscall_sched_yield;
    syscall_table[SYS_OPEN] = syscall_open;
    syscall_table[SYS_READ] = syscall_read;
    syscall_table[SYS_CLOSE] = syscall_close;
    syscall_table[SYS_LSEEK] = syscall_lseek;
}

void syscall_interrupt_handler(uint32_t idt_index, regs_t *regs) {
    (void)idt_index;

    uint32_t num = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;
    uint32_t arg4 = regs->esi;
    uint32_t arg5 = regs->edi;
    uint32_t arg6 = regs->ebp;

    /* Assign a fresh, unique trace id for this syscall. Stashing the previous
     * (background) id in the PCB and restoring it on exit means each operation
     * gets its own sc - distinct even from other syscalls in the same
     * timeslice - and because the active id is kept in pcb_t.trace_id, it is
     * restored by yield() if this syscall blocks/yields or is preempted.
     *
     * INVARIANT (load-bearing): trace_id_saved is a 1-DEEP save slot and lives
     * on the PCB (per process). This is correct only because syscalls never
     * nest/re-enter for the same process, and a process is single-threaded:
     *   - nested/re-entrant syscalls (kernel-issued int 0x80, signal handlers)
     *     would clobber the saved id -> make this a small per-PCB stack.
     *   - multiple threads per process would share these fields -> move
     *     trace_id/trace_id_saved into a per-thread TCB. */
    pcb_t *sc_proc = current_proc;
    if (sc_proc) {
        sc_proc->trace_id_saved = sc_proc->trace_id;
        sc_proc->trace_id = log_trace_begin();
    } else {
        log_trace_begin();
    }

    KLOG_VERBOSE("SYSCALL", "Syscall interrupt fired! Number=0x%08lx (%s)\n",
                 PRINT_UINT32(num), syscall_get_name(num));

    // For now, just handle it as a no-op and return
    // The interrupt will return to user mode automatically

    int32_t retval = ENOSYS;

    if (num < NUM_SYSCALLS && syscall_table[num]) {
        KLOG_VERBOSE("SYSCALL",
                     "Invoking syscall handler for %s at address 0x%08x\n",
                     syscall_get_name(num), syscall_table[num]);
        retval = syscall_table[num](arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        KLOG_VERBOSE("SYSCALL", "No syscall handler for Number=0x%08x (%s)\n",
                     num, syscall_get_name(num));
    }

    KLOG_VERBOSE("SYSCALL", "Syscall handler returned value = 0x%08x\n",
                 retval);

    /* Kernel syscall handler on return value is stored in eax register */
    regs->eax = retval;

    /* Restore the process background trace id. Skipped implicitly for syscalls
     * that never return here (e.g. SYS_EXIT switches away permanently). */
    if (sc_proc && current_proc == sc_proc) {
        sc_proc->trace_id = sc_proc->trace_id_saved;
        log_trace_set(sc_proc->trace_id);
    }
}

/* usr_ptr_validate - check if the pointer points to address in user space
 * @ptr - address to validate
 *
 * @return 1 if valid and 0 if invalid
 */
static uint8_t usr_ptr_validate(const void *ptr) {
    if ((uint32_t)ptr < USER_VIRT_MIN) {
        KLOG_ERROR(
            "SYSCALL",
            "Invalid vitual address for the buffer=0x%08x < Min=0x%08x\n", ptr,
            USER_VIRT_MIN);

        return 0;
    }

    if (ptr >= USER_VIRT_MAX) {
        KLOG_ERROR("SYSCALL",
                   "Invalid virtual address for the buffer=0x%08x, Max=%u\n",
                   ptr, USER_VIRT_MAX);

        return 0;
    }

    if (paging_get_physical_address_in_pd(current_proc->page_directory_virt,
                                          (uint32_t)ptr) == 0) {
        KLOG_ERROR("SYSCALL",
                   "Invalid physical address for the buffer=0x%08x\n", ptr);

        return 0;
    }

    return 1;
}

/* usr_range_is_valid - check if the range of address is in user space
 * @ptr - starting address of the range
 * @len - length of the address
 *
 * @return 1 if valid and 0 if invalid
 */
static uint8_t usr_range_is_valid(const void *ptr, uint32_t len) {
    uint32_t start, end;

    if (len == 0) {
        return 1;
    }
    if (!usr_ptr_validate(ptr)) {
        return 0;
    }

    if (ptr + len < ptr) {
        /* overflow */
        KLOG_ERROR("SYSCALL",
                   "Invalid address range for the buffer=0x%08x, len=%u\n", ptr,
                   len);
        return 0;
    }

    start = PAGE_ALIGN_DOWN((uint32_t)ptr);
    end = PAGE_ALIGN_UP((uint32_t)ptr + len);

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        if (paging_get_physical_address_in_pd(current_proc->page_directory_virt,
                                              addr) == 0) {
            KLOG_ERROR(
                "SYSCALL",
                "Invalid physical address for the buffer=0x%08x, len=%u\n", ptr,
                len);
            return 0;
        }
    }

    return 1;
}

/* copy_from_user - syscall utility which helps to copy data from userspace
 * source buffer to kernelspace destination buffer.
 *
 * @kdst - ref. to the kernel space destination buffer to write into
 * @usrc - ref. to the user space source buffer to read from
 * @len - number of charaters to read.
 *
 * @return VFS status code
 */
static int copy_from_user(void *kdst, const void *usrc, uint32_t len) {
    uint8_t *dst;
    const uint8_t *src;
    uint32_t i;

    /* validate the input arguments */
    if (len == 0) {
        KLOG_INFO("SYSCALL",
                  "copy to kernel buffer completed. copied length = 0.\n");
        return VFS_OK;
    }

    if (!kdst) {
        KLOG_ERROR(
            "SYSCALL",
            "copy to kernel buffer failed. dest. kernel buffer is NULL.\n");
        return VFS_ERR_INVALID;
    }

    if (!usrc) {
        KLOG_ERROR(
            "SYSCALL",
            "copy to kernel buffer failed. source user buffer is NULL.\n");
        return VFS_ERR_INVALID;
    }

    /* validate if the memory ref. from usrc with length all lies within the
     * user space memory region */
    if (!usr_range_is_valid(usrc, len)) {
        KLOG_ERROR(
            "SYSCALL",
            "copy to kernel buffer failed. user buffer range is invalid. "
            "start = %p, length = %u.\n",
            usrc, len);
        return VFS_ERR_INVALID;
    }

    dst = (uint8_t *)kdst;
    src = (const uint8_t *)usrc;

    for (i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    KLOG_INFO(
        "SYSCALL",
        "copy to kernel buffer completed. copied content = %.*s, length = %u.\n",
        (int)len, dst, len);
    return VFS_OK;
}

/* copy_to_user - syscall utility which helps to copy data from kernelspace
 * source buffer to userspace destination buffer.
 *
 * @udst - ref. to the user space destination buffer to write into
 * @ksrc - ref. to the kernel space source buffer to read from
 * @len - number of charaters to read.
 *
 * @return VFS status code
 */
static int copy_to_user(void *udst, const void *ksrc, uint32_t len) {
    uint8_t *dst;
    const uint8_t *src;
    uint32_t i;

    /* validate the input arguments */
    if (len == 0) {
        KLOG_INFO("SYSCALL",
                  "copy to user buffer completed. copied length = 0.\n");
        return VFS_OK;
    }

    if (!udst) {
        KLOG_ERROR("SYSCALL",
                   "copy to user buffer failed. dest. user buffer is NULL.\n");
        return VFS_ERR_INVALID;
    }

    if (!ksrc) {
        KLOG_ERROR(
            "SYSCALL",
            "copy to user buffer failed. source kernel buffer is NULL.\n");
        return VFS_ERR_INVALID;
    }

    /* validate if the memory ref. from udest with length all lies within the
     * user space memory region */
    if (!usr_range_is_valid(udst, len)) {
        KLOG_ERROR("SYSCALL",
                   "copy to user buffer failed. user buffer range is invalid. "
                   "start = %p, length = %u.\n",
                   udst, len);
        return VFS_ERR_INVALID;
    }

    dst = (uint8_t *)udst;
    src = (const uint8_t *)ksrc;

    for (i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    KLOG_INFO(
        "SYSCALL",
        "copy to user buffer completed. copied content = %.*s, length = %u.\n",
        (int)len, dst, len);
    return VFS_OK;
}

/**
 * copy_user_string - copies string content from user space memory to kernel
 * space memory.
 * passing the max length we can copy as we might know the exact
 * string length to copy. complete copying when we see the null character \0.
 * before copying from user buufer, validate each buffer block to be in user
 * memory region.
 * if length of string to copy is greater than max_len, copy partially upto
 * max_len and return error.
 *
 * @kdst - ref. to the kernel space destination buffer to copy to.
 * @usrc - ref. to the user space source buffer to copy from.
 * @max_len - maximum length of the string to copy.
 *
 * @return VFS status code
 */
static int copy_user_string(char *kdst, const char *usrc, uint32_t max_len) {
    uint32_t i;

    /* validate input arguments */
    if (!kdst) {
        KLOG_ERROR("SYSCALL", "copy string to kernel buffer failed. dest. "
                              "kernel buffer is NULL.\n");
        return VFS_ERR_INVALID;
    }

    if (!usrc) {
        KLOG_ERROR("SYSCALL", "copy string to kernel buffer failed. source "
                              "user buffer is NULL.\n");
        return VFS_ERR_INVALID;
    }

    if (max_len == 0) {
        KLOG_ERROR(
            "SYSCALL",
            "copy string to kernel buffer failed. max length to copy is 0.\n");
        return VFS_ERR_INVALID;
    }

    /* copy the contents */
    for (i = 0; i < max_len; i++) {
        /* validate if the memory ref. is in user region
         * if fails before copying entire string, update dest buffer with empty
         * string */
        if (!usr_ptr_validate((const void *)usrc + i)) {
            kdst[0] = '\0';
        }

        kdst[i] = usrc[i];
        if (kdst[i] == '\0') {
            KLOG_INFO("SYSCALL",
                      "copy string to kernel buffer completed. string = %s, "
                      "length = %u.\n",
                      kdst, i + 1);
            return VFS_OK;
        }
    }

    /* string to copy was too long or not null terminated within max-len */
    kdst[max_len - 1] = '\0';
    return VFS_ERR_INVALID;
}
