## Phase 4: FD Table

### 4.1 Add fd table to `pcb_t`
Add `vfs_file_t *fds[PROCESS_MAX_FDS]`.

### 4.2 Add `fd.h` / `fd.c`
Create fd management functions.

### 4.3 Implement `fd_alloc`
Find free fd slot and attach `vfs_file_t`.

### 4.4 Implement `fd_get`
Validate fd and return open file object.

### 4.5 Implement `fd_close`
Clear fd slot and release file object.

### 4.6 Add close-all-on-process-cleanup
Before freeing process, close all open fds.

### 4.7 Build fd tests
Validate allocation, lookup, close, reuse, invalid fd.
