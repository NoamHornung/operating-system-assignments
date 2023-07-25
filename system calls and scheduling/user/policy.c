#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int n=atoi(argv[1]); //negative number is not allowed! will be read as 0
    if(set_policy(n)==0)
        printf("policy changed to %d\n",n);
    else
        printf("policy not changed\n");
    exit(0,"");
}
