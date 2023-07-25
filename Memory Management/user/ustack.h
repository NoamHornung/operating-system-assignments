#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"


#define MAX_PG_SIZE 512

typedef struct header{
    struct header* prev;
    uint size;
    uint dealloc_page;
} header;


void* ustack_malloc(uint len);
int ustack_free(void);