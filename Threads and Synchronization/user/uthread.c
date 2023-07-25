#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/uthread.h"

struct uthread *current_thread;
static struct uthread threads_table[MAX_UTHREADS];
int init_threads_table= 0;
int called_before=0;
int first=0;

int uthread_create(void (*start_func)(), enum sched_priority priority){
    if(init_threads_table==0){
        int i;
        for(i=0; i<MAX_UTHREADS; i++){
            threads_table[i].state = FREE;
        }
        init_threads_table = 1;
    }
    int i;
    int found =0;
    for(i=0; i<MAX_UTHREADS; i++){
        if(threads_table[i].state == FREE){
            found = 1;
            break;
        }
    }
    if(found==1){
        struct uthread *t = &threads_table[i];
        t->priority = priority;
        t->context.sp = (uint64)(t->ustack+STACK_SIZE);
        t->context.ra = (uint64)start_func;
        t->state = RUNNABLE;
        return 0;
    }
    return -1;
}

int find_min_p(){
    int max= -1;
    int index= -1;
    for(int i=0; i<MAX_UTHREADS; i++){
        if(threads_table[i].state == RUNNABLE && priority_to_int(threads_table[i].priority) > max){
            max = priority_to_int(threads_table[i].priority);
            index = i;
        }
    }

    return index;
}

int priority_to_int(enum sched_priority priority){
    if(priority == HIGH){
        return 2;
    }
    else if(priority == MEDIUM){
        return 1;
    }
    else{
        return 0;
    }
}

int get_num_of_t_in_p(enum sched_priority priority){
    int count= 0;
    for(int i=0; i<MAX_UTHREADS; i++){
        if(threads_table[i].state != FREE && threads_table[i].priority == priority){ 
            count++;
        }
    }
    return count;
}

void uthread_yield(){
    //printf("uthread_yield\n");
    int index_to_yield = find_min_p();

    //printf("index: %d\n", index_to_yield);
    if(index_to_yield == -1){
        printf("no threads to yield\n");
        return;
    }

    struct uthread *t = &threads_table[index_to_yield];
    
    t->state = RUNNING;

    struct context *curr_context = &current_thread->context;

    current_thread = t;
    
    uswtch(curr_context, &t->context);
}


void uthread_exit(){
    //printf("uthread exit\n");
    
    current_thread->state = FREE;
    
    if(count_threads()==0){
        exit(0);
    }
    
    uthread_yield();
}

int count_threads(){
    int count= 0;
    for(int i=0; i<MAX_UTHREADS; i++){
        //printf("i: %d state: %d  priority: %d\n",i, threads_table[i].state, threads_table[i].priority);
        if(threads_table[i].state != FREE){
            count+=1;
        }
    }
    return count;
}


int uthread_start_all(){
    //printf("uthread_start_all\n");
    if(init_threads_table==0){ 
        return -1;
    }
    if(count_threads()==0){
        return -1;
    }
    if(called_before==1){
        return -1;
    }
    called_before = 1;

    current_thread= malloc(sizeof(struct uthread));
    
    uthread_yield();
    return 0;
}

enum sched_priority uthread_set_priority(enum sched_priority priority){
    enum sched_priority old_priority = current_thread->priority;
    current_thread->priority = priority;
    return old_priority;
}

enum sched_priority uthread_get_priority(){
    return current_thread->priority;
}

struct uthread* uthread_self(){
    return current_thread;
}
