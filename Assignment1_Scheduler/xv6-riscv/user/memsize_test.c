#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    fprintf(2, "The number of bytes before malloc in the running process is %d\n", memsize());
    char* memAlloc;
    memAlloc = malloc(20000);
    fprintf(2, "The number of bytes after malloc in the running process is %d\n", memsize());
    free(memAlloc);
    fprintf(2, "The number of bytes after freeing the memory in the running process is %d\n", memsize());
    // char* memAlloc2;
    // memAlloc2 = malloc(20000);
    // fprintf(2, "The number of bytes after second malloc in the running process is %d\n", memsize());
    // free(memAlloc2);
    // fprintf(2, "The number of bytes after second freeing the memory in the running process is %d\n", memsize());
    exit(0,"");
}
