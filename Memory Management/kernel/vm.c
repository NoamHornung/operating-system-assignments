#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"
#include "proc.h"


/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");
  
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  //printf("uvmunmap: va = %p, npages = %d, do free= %d\n", va, npages, do_free);
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0 && (*pte & PTE_PG) == 0) //cleared but not swaped out 
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free && (*pte & PTE_V)){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    //task2
    #ifndef NONE
    struct proc *p = myproc();
    if(p->pid > 2 && !(p->name[0] == 's' && p->name[1] == 'h' && p->name[3]=='\000') && pagetable== p->pagetable){
      if((*pte & PTE_V) && (*pte & PTE_PG) == 0 && do_free){ //not swaped out
        if(remove_page_from_memory(p, a)==-1)
          printf("uvmunmap: remove_page_from_memory failed\n");
      }
      else if( (*pte & PTE_V) ==0 && (*pte & PTE_PG)){ //swaped out
        if(remove_page_from_swapfile(p, a)==-1) 
          printf("uvmunmap: remove_page_from_swapfile failed\n");
      }
    }
    #endif

    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  //printf("in uvmalloc\n");
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    //task2
    #ifndef NONE
    struct proc *p = myproc();
    if(p->pid > 2 && !(p->name[0] == 's' && p->name[1] == 'h' && p->name[3]=='\000')){
      if(add_to_memory(myproc(), a)== -1){
        printf("uvmalloc: add_to_memory failed\n");
      }
    }
    #endif
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.

//for task2 we copy the swap file in fork

int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  //printf("uvmcopy: old %p, new %p, sz %d\n", old, new, sz);
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0 && (*pte & PTE_PG) == 0) //cleared but not swaped out
      panic("uvmcopy: page not present");
    //task2
    #ifndef NONE
    if((*pte & PTE_V) == 0 && (*pte & PTE_PG)) {//cleared but swaped out
      // set it in the new pagetable to be swaped out
      pte_t *npte = walk(new, i, 1);
      *npte |= PTE_FLAGS(*pte);
      continue;
    }
    #endif
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}


int remove_page_from_memory(struct proc *p, uint64 va){
  //printf("remove_page_from_memory: va=%d\n", va);
  struct paging_metadata *pmd =&p->paging_metadata;
  for(int i=0; i< MAX_PSYC_PAGES; i++){
    struct memory_page_entry *mpe = &pmd->memory_page_entries[i];
    if(mpe->va == va && mpe->present == 1){
      mpe->va = 0;
      mpe->age = 0;
      mpe->present = 0;
      mpe->offset = 0;
      mpe->order = 0;
      pmd->num_in_memory--;
      return 0;
    }
  }
  return -1;
}

int remove_page_from_swapfile(struct proc *p, uint64 va){
  //printf("remove_page_from_swapfile: va=%d\n", va);
  //print_swap_file(p);
  struct paging_metadata *pmd =&p->paging_metadata;
  for(int i=0; i< MAX_TOTAL_PAGES-MAX_PSYC_PAGES; i++){
    struct swap_file_entry *sfe = &pmd->swap_file_entries[i];
    if(sfe->va == va && sfe->present == 1){
      sfe->va = 0;
      sfe->present = 0;
      sfe->offset = 0;
      pmd->num_in_swap--;
      return 0;
    }
  }
  return -1;
}

int add_to_memory(struct proc *p,uint64 a){
  //printf("add_to_memory: a=%d\n", a);
  struct paging_metadata *pmd =&p->paging_metadata;
  //print_memory(p);
  if(pmd->num_in_memory == MAX_PSYC_PAGES){ //memory is full
    if(swap_out_memory(p)==-1){ //swap out file from memory
      printf("add_to_memory: swap_out failed\n");
      return -1;
    }
  }
  //find empty entry in memory
  for(int i=0; i< MAX_PSYC_PAGES; i++){
    struct memory_page_entry *mpe = &pmd->memory_page_entries[i];
    if(mpe->present == 0){
      mpe->present =1;
      mpe->va = a;
      mpe->offset = i;
      pmd->num_in_memory++;
      #ifdef NFUA
      mpe->age = 0;
      #endif
      #ifdef LAPA
      mpe->age = 0xFFFFFFFF;
      #endif
      #ifdef SCFIFO
      mpe->order = pmd->order_counter;
      pmd->order_counter++;
      #endif

      break;
    }
  }
  // set the flags
  pte_t *pte = walk(p->pagetable, a, 0);
  *pte &= ~PTE_PG; //clear PTE_PG - not in swap file 0
  *pte |= PTE_V; //set PTE_V - in memory 1
  //printf("finished add_to_memory\n");
  return 0;
}

int swap_out_memory(struct proc *p){ //swap in to the swap file
  #ifdef YES
  printf("in swap_out_memory\n");
  printf("memory before swap out:\n");
  print_memory(p);
  printf("swap file before swap out:\n");
  print_swap_file(p);
  #endif
  struct paging_metadata *pmd =&p->paging_metadata;
  if(pmd->num_in_swap+ pmd->num_in_memory == MAX_TOTAL_PAGES){ //swap file is full
    panic("more than 32 pages per proccess\n");
  }
  int index= index_to_swap(p);
  struct memory_page_entry *mpe_to_swap = &pmd->memory_page_entries[index];

  //find empty entry in swap file
  for(int i=0; i< MAX_TOTAL_PAGES-MAX_PSYC_PAGES; i++){
    struct swap_file_entry *sfe = &pmd->swap_file_entries[i];
    if(sfe->present == 0){
      sfe->present =1;
      sfe->va = mpe_to_swap->va;
      sfe->offset = i;
      
      pte_t *pte = walk(p->pagetable, mpe_to_swap->va, 0);
      uint64 pa = PTE2PA(*pte);
      if(writeToSwapFile(p, (char*)pa, i*PGSIZE, PGSIZE)!=PGSIZE){
        printf("swap_out_memory: writeToSwapFile failed\n");
        return -1;
      }
      kfree((void*)pa);
      *pte &= ~PTE_V; //clear PTE_V - not in memory 0
      *pte |= PTE_PG; //set PTE_PG - in swap file 1
      mpe_to_swap->present = 0;
      mpe_to_swap->age = 0;
      mpe_to_swap->offset = 0;
      mpe_to_swap->va = 0;
      mpe_to_swap->order = 0;

      pmd->num_in_swap++;
      pmd->num_in_memory--;
      sfence_vma();
      #ifdef YES
      printf("swap_out_memory: finished\n");
      printf("memory after swap out:\n");
      print_memory(p);
      printf("swap file after swap out:\n");
      print_swap_file(p);
      #endif
      return 0;
    }
  }
  

  return -1;
}

int swap_in_memory(struct proc *p, uint64 address){
  #ifdef YES
  printf("in swap_in_memory\n");
  printf("memory before swap in:\n");
  print_memory(p);
  #endif
  uint64 va1 = PGROUNDDOWN(address);
  pte_t *pte = walk(p->pagetable, va1, 0); 
  if((*pte & PTE_PG)){ //is in swap file
    struct paging_metadata *pmd =&p->paging_metadata;
    if(pmd->num_in_memory == MAX_PSYC_PAGES){ //memory is full
      if(swap_out_memory(p)==-1){ //swap out file from memory
        printf("swap_in_memory: swap_out failed\n");
        return -1;
      }
    }
    // find the address in the swap file and remove it from the struct
    struct swap_file_entry *sfe;
    int offset=0;
    for(int i=0; i< MAX_TOTAL_PAGES-MAX_PSYC_PAGES; i++){
      sfe = &pmd->swap_file_entries[i];
      if(sfe->va == va1 && sfe->present == 1){
        offset = i;
        sfe->present = 0;
        sfe->va = 0;
        sfe->offset = 0;
        pmd->num_in_swap--;
        break;
      }
    }
    // find empty entry in memory and fill it in the struct
    struct memory_page_entry *mpe;
    for(int i=0; i< MAX_PSYC_PAGES; i++){
      mpe = &pmd->memory_page_entries[i];
      if(mpe->present == 0){
        mpe->present =1;
        mpe->va = va1;
        mpe->offset = i;
        pmd->num_in_memory++;
        #ifdef NFUA
        mpe->age = 0;
        #endif
        #ifdef LAPA
        mpe->age = 0xFFFFFFFF;
        #endif
        #ifdef SCFIFO
        mpe->order = pmd->order_counter;
        pmd->order_counter++;
        #endif
        break;
      }
    }
    // read from swap file to memory
    char* pa = kalloc();
    if(readFromSwapFile(p, pa, offset*PGSIZE, PGSIZE)!=PGSIZE){
      printf("swap_in_memory: readFromSwapFile failed\n");
      return -1;
    }
    //map the new address in the memory and set the flags
    *pte = PA2PTE((uint64)pa) | PTE_FLAGS(*pte);
    *pte &= ~PTE_PG; //clear PTE_PG - not in swap file 0
    *pte |= PTE_V; //set PTE_V - in memory 1
    sfence_vma();
    return 0;
  }
  else{
    panic("segfault - not in swap file\n");
    return -1;
  }
}


void print_memory(struct proc *p){
  printf("--------printing memory----------\n");
  struct paging_metadata *pmd =&p->paging_metadata;
  printf("num in memory: %d, num in swap file: %d\n", pmd->num_in_memory, pmd->num_in_swap);
  for(int i=0; i< MAX_PSYC_PAGES; i++){
    struct memory_page_entry *mpe = &pmd->memory_page_entries[i];
    printf("present: %d, va: %d , offset: %d, age:%d, order:%d\n",mpe->present, mpe->va,mpe->offset, mpe->age, mpe->order);
  }
  printf("-------------------------\n");
}

void print_swap_file(struct proc *p){
  printf("--------printing swap file----------\n");
  struct paging_metadata *pmd =&p->paging_metadata;
  printf("num in memory: %d, num in swap file: %d\n", pmd->num_in_memory, pmd->num_in_swap);
  for(int i=0; i< MAX_PSYC_PAGES; i++){
    struct swap_file_entry *sfe = &pmd->swap_file_entries[i];
    printf("present: %d, va: %d , offset: %d\n",sfe->present, sfe->va,sfe->offset);
  }
  printf("-------------------------\n");
}


int index_to_swap(struct proc *p){
  #ifdef NFUA
  return index_to_swap_NFUA(p);
  #endif
  #ifdef LAPA
  return index_to_swap_LAPA(p);
  #endif
  #ifdef SCFIFO
  return index_to_swap_SCFIFO(p);
  #endif
  panic("index_to_swap: no algorithm defined\n");
}

int index_to_swap_NFUA(struct proc *p){
  struct paging_metadata *pmd =&p->paging_metadata;
  uint min =pmd->memory_page_entries[0].age;
  int index =0;
  for(int i=1; i< MAX_PSYC_PAGES; i++){
    struct memory_page_entry *mpe = &pmd->memory_page_entries[i];
    if(mpe->present == 1 && mpe->age < min){
      min = mpe->age;
      index = i;
    }
  }
  #ifdef YES
  //print_memory(p);
  printf("index_to_swap_NFUA: index: %d with age:%d\n", index, min);
  #endif
  return index;
}


int index_to_swap_LAPA(struct proc *p){
  //printf("index_to_swap_LAPA\n");
  int index = 0;
  struct paging_metadata *pmd =&p->paging_metadata;
  int min_count = countSetBits(pmd->memory_page_entries[0].age);
  for(int i=1; i< MAX_PSYC_PAGES; i++){
    struct memory_page_entry *mpe = &pmd->memory_page_entries[i];
    int count = countSetBits(mpe->age);
    if(count < min_count){
      min_count = count;
      index = i;
    }
    else if(count == min_count){
      if(mpe->age < pmd->memory_page_entries[index].age){
        index = i;
      }
    }
  }
  #ifdef YES
  //print_memory(p);
  printf("index_to_swap_LAPA: index: %d with min count:%d\n", index, min_count);
  #endif
  return index;
}

int countSetBits(uint n)
{
    int count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

int index_to_swap_SCFIFO(struct proc *p){
  int index = find_min_order(p);
  struct paging_metadata *pmd =&p->paging_metadata;
  int bool = 0;
  while(bool==0){
    struct memory_page_entry *found_mpe = &pmd->memory_page_entries[index];
    pte_t* pte = walk(p->pagetable,found_mpe->va,0);
    if((*pte & PTE_A)!=0) { //access flag is on
      *pte &=~ PTE_A; //trun off the access flag
      found_mpe->order= pmd->order_counter; //gets new order
      pmd->order_counter++;
      #ifdef YES
      printf("index_to_swap_SCFIFO: index: %d with new order:%d\n", index, found_mpe->order);
      #endif
      index = find_min_order(p);
    }
    else{
      bool = 1;
    }
  }
  #ifdef YES
  printf("the final index_to_swap_SCFIFO: index: %d with order:%d\n", index, pmd->memory_page_entries[index].order);
  #endif
  return index;
}

int find_min_order(struct proc *p){
  int index= -1;
  int min_order = __INT_MAX__;
  struct paging_metadata *pmd =&p->paging_metadata;
  for(int i=0; i< MAX_PSYC_PAGES; i++){
    struct memory_page_entry *mpe = &pmd->memory_page_entries[i];
    if(mpe->present == 1 && mpe->order < min_order){
      min_order = mpe->order;
      index = i;
    }
  }
  #ifdef YES
  printf("find_min_order: index: %d with order:%d\n", index, min_order);
  #endif
  return index;

}

void update_age(struct proc *p){
  struct paging_metadata *pmd =&p->paging_metadata;
  for(int i=0; i< MAX_PSYC_PAGES; i++){
    struct memory_page_entry *mpe = &pmd->memory_page_entries[i];
    if(mpe->present == 1){
      pte_t *pte = walk(p->pagetable, mpe->va, 0);
      mpe->age = (mpe->age >> 1); //shift right by one bit
      if(*pte & PTE_A){ //if accessed
        mpe->age |= 1 << 31; //add 1 to msb
        *pte &= ~PTE_A; //reset PTE_A flag
      }
    }
  }
}