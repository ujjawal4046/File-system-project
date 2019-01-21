# File-system-project

Memory resident filesystem implemented as part of Operating System laboratory 

Design-
- Exposes basic API (Application Programming Interface) for 
  - Filesystem creation
  - Copying file from PC to created file system and vice-versa
  - directory related functionalities like ls,mkdir,chdir (unix based).
  - reading and writing to a file using file descriptors
- The file system resides in shared_memory along with file descriptor table.
- The file descriptor are shared between for all concurrent access and it is assumed a file will be
accessed by only a single process at a time.
- File descriptor table stores offset,inode_no and fd no for each file descriptor.
- Mutex is used to avoid race conditions while updating inodes,data blocks,super block which is
shared between all processes spawned by the parent process which create file system.
