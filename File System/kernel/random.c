
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


struct {
  struct spinlock lock;
  uint8 seed;
} rand;


int randomread(int user_dst, uint64 dst, int n){
    int target = n;
    acquire(&rand.lock);
    while(n > 0){
        uint8 new_seed = lfsr_char(rand.seed);
        if(either_copyout(user_dst, dst, &new_seed, 1) == -1)
            break;
        rand.seed = new_seed;
        dst++;
        --n;
    }
    release(&rand.lock);
    return target - n;
}

int randomwrite(int user_src, uint64 src, int n){
    if(n!=1){
        return -1;
    }
    acquire(&rand.lock);
    if(either_copyin(&rand.seed, user_src, src, 1) == -1){
        release(&rand.lock);
        return -1;
    }
    release(&rand.lock);
    return 1;
}

void
randominit(void)
{

  initlock(&rand.lock, "random");

  rand.seed = 0x2A;
  //printf("random: initialized\n");
  //printf("%x\n", lfsr_char(rand.seed));
  
  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[RANDOM].read = randomread;
  devsw[RANDOM].write = randomwrite;
}

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
