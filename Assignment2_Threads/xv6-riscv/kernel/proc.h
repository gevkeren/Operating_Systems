#include "kthread.h"

enum procstate { UNUSED, USED, ZOMBIE };

// Per-process state
struct proc {
    struct spinlock lock;                 // Lock for public properties of the PCB
    int thread_id_counter;                // Counter for allocating thread IDs
    struct spinlock thread_id_lock;       // Lock for thread ID allocation
    enum procstate state;                 // Process state
    int killed;                           // If non-zero, process has been killed
    int exit_status;                      // Exit status to be returned to parent's wait
    int pid;                              // Process ID
    struct kthread kthreads[NKT];          // kthread group table
    struct trapframe* base_trapframes;    // Pointer to the base of threads' trapframes page
    struct proc* parent;                  // Pointer to the parent process
    uint64 sz;                            // Size of process memory (bytes)
    pagetable_t pagetable;                // User page table
    struct file* ofile[NOFILE];           // Open files
    struct inode* cwd;                    // Current directory
    char name[16];                        // Process name (debugging)
    struct context context;               ////????????? 
};







