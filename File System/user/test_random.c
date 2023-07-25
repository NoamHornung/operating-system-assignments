#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char** argv){
    int fd = open("random", O_RDWR);
    if(fd < 0){
        printf("Error opening file\n");
        return 0;
    }
    uint8 buffer1[255];
    uint8 buffer2[255];
    uint8 buffer3[255];
    uint8 buffer4[255];
 
    //test for read
    int test1= read(fd,buffer1,1);
    if(test1 != 1){
        printf("Error in test1\n");
        close(fd);
        return 0;
    }

    printf("the first random number is: hex-%x  dec-%d\n", buffer1[0], buffer1[0]);

    int test2 =read(fd,buffer2,255);
    if(test2 != 255){
        printf("Error in test2\n");
        close(fd);
        return 0;
    }
    printf("should print the same number: hex-%x  dec-%d\n", buffer2[254], buffer2[254]);
    printf("should print 2A in hex 42 in dec: hex-%x  dec-%d\n", buffer2[253], buffer2[253]);

    //test for write
    printf("\n");
    uint8 new_seed = '1';

    int test3= write(fd,&new_seed,1);
    if(test3 != 1){
        printf("Error in test3\n");
        close(fd);
        return 0;
    }
    
    int test4= read(fd,buffer3,1);
    if(test4 != 1){
        printf("Error in test4\n");
        close(fd);
        return 0;
    }
    printf("the new first random number is: hex-%x  dec-%d\n", buffer3[0], buffer3[0]);

    int test5= read(fd,buffer4,255);
    if(test5 != 255){
        printf("Error in test5\n");
        close(fd);
        return 0;
    }
    printf("should print the same number as the last print: hex-%x  dec-%d\n", buffer4[254], buffer4[254]);
    printf("should print 0x31 in hex and 49 in int: hex-%x  dec-%d \n", buffer4[253], buffer4[253]);
    
    close(fd);
    
    return 0;
}