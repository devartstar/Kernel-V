user reads -> fd = 0 (/dev/stdin) 
-> SYSCALL_READ(fd=0) -> fd_read(fd=0) -> find the file ref. by fd=0 by process
-> file -> ref to vfs_node -> read operation registered for that file.
