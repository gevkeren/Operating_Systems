# include "kernel/types.h"
# include "kernel/stat.h"
# include "user/user.h"
# include "user/uthread.h"

// #include <stdio.h>
// #include <stdlib.h>

// #define STACK_SIZE  4000
// #define MAX_UTHREADS  4

struct uthread threads[MAX_UTHREADS];
struct uthread* curr_thread;
int next_thread_index = -1;
int num_of_threads = 0;

int uthread_create(void (*start_func)(), enum sched_priority priority){
    struct uthread *t;
    for (t = threads; t < &threads[MAX_UTHREADS]; t++){
        if (t->state == FREE){ //found an empty entry
            t->state = RUNNABLE;
            t->priority = priority;
            t->context.ra = (uint64)start_func;
            t->context.sp =(uint64) &t->ustack[STACK_SIZE - 1]; 
            num_of_threads++;
            break;
        }
    }
    if (t == &threads[MAX_UTHREADS]){
        printf("Error: Thread table is full. Cannot create a new thread.");
        return -1;
    }
    return 0;
}

int find_next_thread(){
    int highest_priority = -1;
    int found_runnable = 0;

    for (int i = 0; i < MAX_UTHREADS; i++){
        if (threads[i].state != FREE){
            found_runnable = 1;
            if (threads[i].state == RUNNABLE && (int)threads[i].priority > highest_priority){
                next_thread_index = i;
                highest_priority = (int)threads[i].priority;
            }
        }
    }
    if (!found_runnable){
        return -1;
    }
    return next_thread_index;
}

void uthread_yield(){
    int prev_thread_index = next_thread_index;
    struct uthread* prev_thread = &threads[prev_thread_index];

    // if find_next_thread succeed and found a different thread
    if (find_next_thread() != -1 && prev_thread_index != next_thread_index){
        curr_thread->state = RUNNABLE;
        curr_thread = &threads[next_thread_index];
        curr_thread->state = RUNNING;
        uswtch(&prev_thread->context, &curr_thread->context);
    }
}

void uthread_exit(){
    
    int prev_thread_index = next_thread_index;
    struct uthread* prev_thread = &threads[prev_thread_index];

    curr_thread->state = FREE;
    num_of_threads--;

    int func_return = find_next_thread();

    if (func_return != -1 && prev_thread_index != next_thread_index){
        curr_thread = &threads[next_thread_index];
        curr_thread->state = RUNNING;
        uswtch(&prev_thread->context, &curr_thread->context);
    }
    else{
        if (func_return == -1){ // find_next_thread failed
            exit(0);
        }
    }
}

enum sched_priority uthread_set_priority(enum sched_priority priority){
    enum sched_priority previous_priority = curr_thread->priority;
    curr_thread->priority = priority;
    return previous_priority;
}


enum sched_priority uthread_get_priority(){
    return curr_thread->priority;
}

int uthread_start_all() {
    static int cond = 0;
    if (num_of_threads == 0 || cond == 1){
        return -1;
    }
    cond = 1;
    if (find_next_thread() != -1){
        curr_thread = &threads[next_thread_index];
        curr_thread->state = RUNNING;
        struct context empty_context;
        uswtch(&empty_context, &curr_thread->context);
    }
    exit(0);
}

struct uthread* uthread_self(){
    return curr_thread; 
}
