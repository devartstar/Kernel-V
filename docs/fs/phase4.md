## Phase 4: File Descriptor Table

### Subphase 4.1
Add fd table storage to `process_t`.

### Subphase 4.2
Initialize fd table during process creation.

### Subphase 4.3
Implement `fd_alloc()`.

### Subphase 4.4
Implement `fd_get()`.

### Subphase 4.5
Implement `fd_close()`.

### Subphase 4.6
Validate fd allocation, lookup, close, and reuse.
