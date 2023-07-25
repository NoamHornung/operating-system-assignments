#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int size= memsize();
  //write(1, &size, sizeof(size));
  //write(1, "\n", strlen("\n"));
  printf("the process is using %d bytes\n", size);
  char* ptr= malloc(20000);
  size= memsize();
  printf("the process is using %d bytes after the allocation\n", size);
  free(ptr);
  size= memsize();
  printf("the process is using %d bytes after the release\n", size);
  exit(0,"");
}
