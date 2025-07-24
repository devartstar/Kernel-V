#ifndef KERNEL_PRINTK_H
#define KERNEL_PRINTK_H

#define <stdarg.h>
#define <stddef.h>

//
// Max buffer of 1024 Bytes for each call output
//
#define LOG_BUF_SIZE 1024

int printk(cost char *fmt, ...)
{
	__attribute__((format(printf,1,2)));
}

#endif
