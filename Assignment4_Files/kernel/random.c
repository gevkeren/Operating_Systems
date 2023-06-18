#include <stdarg.h>
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

// anonymous struct, to create only one instance
struct{
    struct spinlock lock;
    uint8 random_seed ;
} random;

uint8 lfsr_char(uint8 lfsr);
void randominit(void);
int randomwrite(int fd, const uint64 src, int n);
int randomread(int fd, uint64 dst, int n);
// Linear feedback shift register
// Returns the next pseudo-random number
// The seed is updated with the returned value
uint8 lfsr_char(uint8 lfsr)
{
    uint8 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
    lfsr = (lfsr >> 1) | (bit << 7);
    return lfsr;
}

void randominit(void) {
    initlock(&random.lock, "random");
    uartinit();

    random.random_seed = 0x2A;
    devsw[RANDOM].read = randomread;
    devsw[RANDOM].write = randomwrite;
}

int randomwrite(int fd, const uint64 src, int n) {
    if (n != 1){
        return -1;
    }
    
    acquire(&random.lock);
    if(either_copyin(&random.random_seed, fd, src, 1) == -1) {
        release(&random.lock);
        return -1;
    }
    release(&random.lock);

    return 1;
}

int randomread(int fd, uint64 dst, int n) {
    int counter;
    counter = 0;
    
    while (n > 0) {
        acquire(&random.lock);
        random.random_seed = lfsr_char(random.random_seed);

        if (either_copyout(fd, dst, &random.random_seed, 1) == -1){
            release(&random.lock);
            return counter;
        }
        counter++;
        n--;
        release(&random.lock);
    }

    return counter;
}




