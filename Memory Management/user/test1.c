#include "ustack.h"



int
main(int argc, char *argv[])
{
    char * p = (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    int i = ustack_free();
    printf("freed: %d\n", i);
    p= (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    p = (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    p = (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    p = (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    p = (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    p = (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    p = (char*)ustack_malloc(500);
    printf("address: %p\n", p);
    p = (char*)ustack_malloc(342);
    printf("address: %p\n", p);
    i = ustack_free();
    printf("freed: %d\n", i);
    char * p4 = (char*)ustack_malloc(600);
    if(p4 == (char*)-1){
        printf("good\n");
    }
    i = ustack_free();
    printf("freed: %d\n", i);
    i = ustack_free();
    printf("freed: %d\n", i);
    i = ustack_free();
    printf("freed: %d\n", i);
    i = ustack_free();
    printf("freed: %d\n", i);
    i = ustack_free();
    printf("freed: %d\n", i);
    i = ustack_free();
    printf("freed: %d\n", i);
    i = ustack_free();
    printf("freed: %d\n", i);
    i = ustack_free();
    printf("freed: %d\n", i);
    printf("....free space should be 0....\n");
    i = ustack_free();
    printf("freed: %d\n", i); //should be -1

    return 0;
}