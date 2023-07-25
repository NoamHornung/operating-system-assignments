#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void print_stat(int pid,int cfs_p, int rtime, int stime, int retime);

void run_loop();

//ass1 task6
int
main(int argc, char *argv[])
{
    if(fork()==0){ //low priority
        set_cfs_priority(2);
        set_ps_priority(10);
        run_loop();
        int pid_l= getpid();
        int l_cfs_p, l_rtime, l_stime, l_retime=0;
        get_cfs_stats(pid_l,&l_cfs_p, &l_rtime, &l_stime, &l_retime);
        sleep(7);
        print_stat(pid_l,l_cfs_p, l_rtime, l_stime, l_retime);
        exit(0,"low priority exit\n");
    }
    else{ //parent
        if(fork()==0){ //normal priority
            set_cfs_priority(1);
            set_ps_priority(5);
            run_loop();
            int pid_n= getpid();
            int n_cfs_p, n_rtime, n_stime, n_retime=0;
            get_cfs_stats(pid_n,&n_cfs_p, &n_rtime, &n_stime, &n_retime);
            sleep(3);
            print_stat(pid_n,n_cfs_p, n_rtime, n_stime, n_retime);
            exit(0,"normal priority exit\n");
        }
        else{ //parent
            if(fork()==0){ //high priority
                set_cfs_priority(0);
                set_ps_priority(1);
                run_loop();
                int pid_h= getpid();
                int h_cfs_p, h_rtime, h_stime, h_retime=0;
                get_cfs_stats(pid_h,&h_cfs_p, &h_rtime, &h_stime, &h_retime);
                print_stat(pid_h,h_cfs_p, h_rtime, h_stime, h_retime);
                exit(0,"high priority exit\n");
            }
            else{
                wait(0,0);
                wait(0,0);
                wait(0,0);
            }
        }
    }
    exit(0,"");
}


void run_loop(){
    for(int i=0;i<500000000;i++){
        if(i%50000000==0){
            sleep(10);
        }
    }
}

void print_stat(int pid,int cfs_p, int rtime, int stime, int retime){
    printf("%d cfs_priority: %d\n",pid,cfs_p);
    printf("%d rtime: %d\n",pid,rtime);
    printf("%d stime: %d\n",pid,stime);
    printf("%d retime: %d\n",pid,retime);
    printf("\n");
}