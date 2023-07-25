#include "ustack.h"
#include "kernel/riscv.h"

static header *stack_top=0;
static uint free_sapce;

void* ustack_malloc(uint len){
    if(len> MAX_PG_SIZE){
        return (void*)-1;
    }
    len += sizeof(header);
    if(stack_top == 0){
        printf("initializing stack\n");
        stack_top =(header*) sbrk(PGSIZE);
        if(stack_top == (header*)-1){
            return (void*)-1;
        }
        stack_top->dealloc_page=0;
        stack_top->size = 0;
        stack_top->prev = 0;
        free_sapce = PGSIZE;
    }

    header *p;
    if(free_sapce >= len){ 
        printf("using existing page\n");
        p= ((void*)stack_top)+ len;
        p->dealloc_page = 0;
    }
    else{
        printf("allocating new page\n");
        printf("free sapce before allocate new page: %d\n", free_sapce);
        char *x = sbrk(PGSIZE);
        if(x == (char*)-1){
            return (void*)-1;
        }
        free_sapce += PGSIZE;
        printf("free sapce after allocate new page: %d\n", free_sapce);
        p= ((void*)stack_top)+ len;
        p->dealloc_page = 1; //new page allocated
    }
    free_sapce -= len;
    printf("free sapce after malloc: %d\n", free_sapce);
    p->size = len;
    p->prev = stack_top;
    stack_top = p;
    return (void*)(p->prev+1);
}

int ustack_free(void){
    printf("freeing stack\n");
    printf("free sapce before free: %d\n", free_sapce);
    header *p;
    if(stack_top== 0){
        printf("stack is empty\n");
        return -1;
    }
    p = stack_top;
    int len = p->size;
    stack_top = p->prev;
    free_sapce += p->size;
    printf("free sapce after free: %d\n", free_sapce);
    if(stack_top->prev == 0) // stack top now points to the address returned from the first sbrk call
        stack_top = 0;
    if(p->dealloc_page==1 || stack_top == 0){
        printf("deallocating page\n");
        sbrk(-(PGSIZE)); // will never fail
        free_sapce -= PGSIZE;
        printf("free sapce after deallocating page: %d\n", free_sapce);
    }
    return len-sizeof(header);
}