#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define SYS_insdump 359
#define SYS_rmdump 360
#define OPEN_ADDR "dumpstack_open"
#define READ_ADDR "dumpstack_read"
#define WRITE_ADDR "dumpstack_write"


struct data_user_insert
{
    int dump_mode;
    char *func_name;
};

struct data_user_remove
{
    unsigned int dumpid;
};


int main()
{
    int x;
    int ret;
    int fd;
    unsigned int y;
    char *c = "dumpstack_read";
    //c = "hello";
    x = 5;
    y = 10;
    ret = syscall(SYS_insdump,OPEN_ADDR,x);
    printf("return value of insdump = %d \n",ret);
    x = 0;
    ret = syscall(SYS_insdump,READ_ADDR,x);
    printf("return value of insdump = %d \n",ret);
    x = 10;
    ret = syscall(SYS_insdump,WRITE_ADDR,x);
    printf("return value of insdump = %d \n",ret);


    fd = open("/dev/dumpstack_drv",O_RDWR);
    
    struct data_user_insert *buff = (struct data_user_insert *)malloc(sizeof(struct data_user_insert));
    buff->dump_mode = 2;
    
    buff->func_name = c;

    printf("%s\n",buff->func_name);

    struct data_user_remove *buffer = (struct data_user_remove *)malloc(sizeof(struct data_user_remove));
    

    y = write(fd,buff,sizeof(buff));
    printf("done with write call \n");
     

    buffer->dumpid = y;

    read(fd,buff,sizeof(buff));
    printf("done with read call \n");
     
    ret = syscall(SYS_rmdump,2);
    ret = syscall(SYS_rmdump,0);

    printf("KILL NOW \n");
    sleep(10);

    ret = syscall(SYS_rmdump,1);
    read(fd,buff,sizeof(buff));
    printf("done with read call \n");

    printf("return value of rmdump = %d \n",ret);
    close(fd);
    return 0;
}