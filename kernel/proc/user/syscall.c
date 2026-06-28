#include "proc/syscall.h"
#include "fs/vfs.h"
#include "lib/print_macros.h"
#include "lib/printk.h"
#include "mm/paging.h"
#include "proc/proc.h"
#include "proc/scheduler.h"
#include "proc/user.h"

#define KBUF_CHUNK_SIZE 128

static uint8_t usr_ptr_validate(uint32_t ptr);
static uint8_t usr_range_is_valid(uint32_t ptr, uint32_t len);

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

static int32_t syscall_write(uint32_t fd, uint32_t buf_ptr, uint32_t len,
                             uint32_t _4, uint32_t _5, uint32_t _6) {
    (void)_4;
    (void)_5;
    (void)_6;

    /* Only supports fd = 1 (stdout) */
    if (fd != 1) {
        return -1;
    }

    if (!usr_range_is_valid(buf_ptr, len)) {
        KLOG_ERROR("SYSCALL",
                   "sycall_write: invalid user buffer=0x%08x, length=%u\n",
                   buf_ptr, len);
        return -1;
    }

    /* Copy the buffer in kernel side before printing */

    uint32_t copy_remaining_len = len;
    uint32_t offset = 0;

    while (copy_remaining_len > 0) {
        uint32_t chunk_len = copy_remaining_len;

        if (chunk_len > KBUF_CHUNK_SIZE - 1) {
            chunk_len = KBUF_CHUNK_SIZE - 1;
        }

        char kbuf[KBUF_CHUNK_SIZE];

        memcpy(kbuf, (const void *)(buf_ptr + offset), chunk_len);
        kbuf[chunk_len] = '\0';

        KLOG_INFO("SYSCALL",
                  "sycall_write: (buf_start=0x%08x, len=%u) (msg: %s)\n",
                  (buf_ptr + offset), chunk_len, kbuf, len);

        copy_remaining_len = copy_remaining_len - chunk_len;
        offset += chunk_len;
    }

    return (int32_t)len;
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

void syscall_table_init(void) {
    /* Register default handler (ENOSYS) for all syscalls */
    for (int8_t i = 0; i < NUM_SYSCALLS; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[SYS_EXIT] = syscall_exit;
    syscall_table[SYS_WRITE] = syscall_write;
    syscall_table[SYS_GETPID] = syscall_getpid;
    syscall_table[SYS_SCHED_YIELD] = syscall_sched_yield;
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

    KLOG_VERBOSE("SYSCALL", "Syscall interrupt fired! Number=0x%08lx\n",
                 PRINT_UINT32(num));

    // For now, just handle it as a no-op and return
    // The interrupt will return to user mode automatically

    int32_t retval = ENOSYS;

    if (num < NUM_SYSCALLS && syscall_table[num]) {
        KLOG_VERBOSE("SYSCALL", "Invoking syscall handler at address 0x%08x\n",
                     syscall_table[num]);
        retval = syscall_table[num](arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        KLOG_VERBOSE("SYSCALL", "No syscall handler for Number=0x%08x\n", num);
    }

    KLOG_VERBOSE("SYSCALL", "Syscall handler returned value = 0x%08x\n",
                 retval);

    /* Kernel syscall handler on return value is stored in eax register */
    regs->eax = retval;
}

/* usr_ptr_validate - check if the pointer points to address in user space
 * @ptr - address to validate
 *
 * @return 1 if valid and 0 if invalid
 */
static uint8_t usr_ptr_validate(uint32_t ptr) {
    if (ptr < USER_VIRT_MIN) {
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
                                          ptr) == 0) {
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
static uint8_t usr_range_is_valid(uint32_t ptr, uint32_t len) {
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

    start = PAGE_ALIGN_DOWN(ptr);
    end = PAGE_ALIGN_UP(ptr + len);

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
        "copy to kernel buffer completed. copied content = %s, length = %u.\n",
        dst, len);
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
        "copy to user buffer completed. copied content = %s, length = %u.\n",
        dst, len);
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
static int copy_user_string(char *kdst, char *usrc, uint32_t max_len) {
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
        if (!usr_ptr_validate((const void *)(usrc + i))) {
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
