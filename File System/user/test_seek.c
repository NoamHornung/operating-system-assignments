#include "kernel/types.h"
#include "user.h"
#include "kernel/fcntl.h"

void test_seek()
{
    // Open a file
    int fd = open("testfile.txt", O_RDWR | O_CREATE);
    if (fd < 0)
    {
        printf("Error opening file\n");
        return;
    }

    // Write some data to the file
    char data[] = "Hello, World!";
    write(fd, data, sizeof(data));

    printf("test 1 seek set to 0: should print Hello, World!\n");
    // Seek to the beginning of the file (offset 0)
    int seek1 = seek(fd, 0, SEEK_SET);
    if (seek1 == -1)
    {
        printf("Error seeking in seek1\n");
        close(fd);
        return;
    }
    // Read the data from the file
    char buffer[14];
    read(fd, buffer, sizeof(buffer));

    // Print the read data
    printf("test1: %s\n", buffer);

    // Seek to the beginning of the file (offset 0)
    int seek2 = seek(fd, 0, SEEK_SET);
    if (seek2 == -1)
    {
        printf("Error seeking in seek2\n");
        close(fd);
        return;
    }

    //seek cur to 7
    printf("\n");
    printf("test 2 seek cur to 7: should print World!\n");
    int seek3= seek(fd, 7, SEEK_CUR);
    if (seek3 == -1)
    {
        printf("Error seeking in seek3\n");
        close(fd);
        return;
    }
    char buffer2[7];
    read(fd, buffer2, sizeof(buffer2));
    printf("test2: %s\n", buffer2);

    //checking we get error when giving invalid whence
    printf("\n");
    printf("test 3 invalid whence: should not print error\n");
    int seek4 = seek(fd, 0, 3);
    if(seek4 != -1){
        printf("Error seeking in seek4\n");
        close(fd);
        return;
    }

    //giving negative offset in seek set
    printf("\n");
    printf("test 4 seek set offset<0: should print Hello, World!\n");
    int seek5 = seek(fd, -2, SEEK_SET);
    if(seek5 == -1){
        printf("Error seeking in seek5\n");
        close(fd);
        return;
    }
    char buffer3[14];
    read(fd, buffer3, sizeof(buffer3));
    printf("test4: %s\n", buffer3);
    
    //giving negative offset in seek cur
    printf("\n");
    printf("test 5 seek curr offset<0: should print Hello, World!\n");
    int seek6 = seek(fd, -15, SEEK_CUR);
    if(seek6 == -1){
        printf("Error seeking in seek6\n");
        close(fd);
        return;
    }
    char buffer4[14];
    read(fd, buffer4, sizeof(buffer4));
    printf("test5: %s\n", buffer4);

    //giving offset greater than file size in seek set
    printf("\n");
    printf("test 6 seek set offset>file size: should print nothing\n");
    int seek7 = seek(fd, 16, SEEK_SET);
    if(seek7 == -1){
        printf("Error seeking in seek7\n");
        close(fd);
        return;
    }
    char buffer5[14];
    read(fd, buffer5, sizeof(buffer5));
    printf("test6: %s\n", buffer5);


    //giving offset greater than file size in seek cur
    printf("\n");
    printf("test 7 seek curr offset>file size: should print nothing\n");
    int seek8 = seek(fd, 1, SEEK_CUR);
    if(seek8 == -1){
        printf("Error seeking in seek8\n");
        close(fd);
        return;
    }
    char buffer6[14];
    read(fd, buffer6, sizeof(buffer6));
    printf("test7: %s\n", buffer6);

    
    // Close the file
    close(fd);

    //seeking in file descriptor that is not open
    printf("\n");
    printf("test 8 seek in file descriptor that is not open: should not print error\n");
    int seek9= seek(fd, 0, SEEK_SET);
    if(seek9 != -1){
        printf("Error seeking in seek9\n");
        return;
    }
}

int main(void)
{
    test_seek();
    exit(0);
}