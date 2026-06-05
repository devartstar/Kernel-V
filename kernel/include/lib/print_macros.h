#include <stdint.h>

// Always cast fixed-width types to their corresponding printf type.
// For your my_vsnprintf, %u/%x expect unsigned int, %d expects int.

#define PRINT_UINT32(val)   ((unsigned int)(val))
#define PRINT_INT32(val)    ((int)(val))
#define PRINT_UINT16(val)   ((unsigned int)(val))   // promoted to unsigned int
#define PRINT_INT16(val)    ((int)(val))
#define PRINT_UINT8(val)    ((unsigned int)(val))   // promoted to unsigned int
#define PRINT_INT8(val)     ((int)(val))

// 64-bit values must be split, because you don't have %llx/%llu in your implementation
#define PRINT_UINT64_HI(val) ((unsigned int)((uint64_t)(val) >> 32))
#define PRINT_UINT64_LO(val) ((unsigned int)((uint64_t)(val) & 0xFFFFFFFF))
#define PRINT_INT64_HI(val)  ((int)((int64_t)(val) >> 32))
#define PRINT_INT64_LO(val)  ((int)((int64_t)(val) & 0xFFFFFFFF))

// Pointer values (print as hex)
#define PRINT_PTR(val)      ((uintptr_t)(val)) // Pass as unsigned long or split, depending on your implementation

// For size_t/ssize_t: Cast to unsigned int/int for printing with %u/%d
#define PRINT_SIZE(val)     ((unsigned int)(val))
#define PRINT_SSIZE(val)    ((int)(val))
