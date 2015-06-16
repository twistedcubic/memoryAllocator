Implemented a memory allocator for the heap of a user-level process in a thread-safe way. Built my own Mem_Malloc(), Mem_Free(), and two memory allocators: slab allocator and next-fit allocator. Also produced shared library.

void *Mem_Alloc(int size) takes as input the size in bytes of the object to be allocated and returns a pointer to the start of that object. The function returns NULL if there is not enough contiguous free space available.

int Mem_Free(void *ptr) frees the memory object that ptr points to. If ptr is NULL, then no operation is performed. The function returns 0 on success, and -1 otherwise.

Coalescing: Mem_Free() should make sure to coalesce free space. Coalescing rejoins neighboring freed blocks into one bigger free chunk, thus ensuring that big chunks remain free for subsequent calls to Mem_Alloc(). Coalesce during each free operation.

Provided these routines in a shared library named "libmem.so". 

