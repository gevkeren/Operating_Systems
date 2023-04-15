#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(){
    int p1_id, p2_id, p3_id, p4_id; 
    if ((p1_id  = fork()) == 0){
        //Child 1
        sleep(1);
        set_cfs_priority(2);
        sleep(1);
        for (int i = 0; i < 100000000; i++){
            if (i % 10000000 == 0){
                sleep(10);
            }
        }
        int mypid = getpid();
        int rtime, cfs_priority, retime, stime;
        get_cfs_stats(mypid, &cfs_priority, &stime, &rtime, &retime);
        sleep(10);
        printf("pid: %d, priority: %d, rtime: %d, retime: %d, stime: %d\n", 
                mypid, cfs_priority, rtime, retime, stime);

    }
    else{
        if ((p2_id = fork()) == 0){
            //Child 2
            sleep(1);
            set_cfs_priority(1);
            sleep(1);
            for (int i = 0; i < 100000000; i++){
                if (i % 10000000 == 0){
                    sleep(10);
                }
            }
            int mypid = getpid();
            int rtime, cfs_priority, retime, stime;
            get_cfs_stats(mypid, &cfs_priority, &stime, &rtime, &retime);
            sleep(15);
            printf("pid: %d, priority: %d, rtime: %d, retime: %d, stime: %d\n", 
                    mypid, cfs_priority, rtime, retime, stime);
        }
        else{
            if ((p3_id = fork()) == 0){
                //Child 3
                sleep(1);
                set_cfs_priority(1);
                sleep(1);
                for (int i = 0; i < 100000000; i++){
                    if (i % 10000000 == 0){
                        sleep(10);
                    }
                }
                int mypid = getpid();
                int rtime, cfs_priority, retime, stime;
                get_cfs_stats(mypid, &cfs_priority, &stime, &rtime, &retime);
                sleep(25);
                printf("pid: %d, priority: %d, rtime: %d, retime: %d, stime: %d\n", 
                        mypid, cfs_priority, rtime, retime, stime);
            }
            else{
                if ((p4_id = fork()) == 0){
                    //Child 4
                    sleep(1);
                    set_cfs_priority(0);
                    sleep(1);
                    for (int i = 0; i < 100000000; i++){
                        if (i % 10000000 == 0){
                            sleep(10);
                        }
                    }
                    int mypid = getpid();
                    int rtime, cfs_priority, retime, stime;
                    get_cfs_stats(mypid, &cfs_priority, &stime, &rtime, &retime);
                    sleep(35);
                    printf("pid: %d, priority: %d, rtime: %d, retime: %d, stime: %d\n", 
                            mypid, cfs_priority, rtime, retime, stime);
                }
                else{
                    //Parent
                    sleep(1);
                    set_cfs_priority(0);
                    sleep(1);
                    for (int i = 0; i < 100000000; i++){
                        if (i % 10000000 == 0){
                            sleep(10);
                        }
                    }
                    int mypid = getpid();
                    int rtime, cfs_priority, retime, stime;
                    get_cfs_stats(mypid, &cfs_priority, &stime, &rtime, &retime);
                    sleep(20);
                    printf("pid: %d, priority: %d, rtime: %d, retime: %d, stime: %d\n", 
                            mypid, cfs_priority, rtime, retime, stime);
                }
            }
        }
    }
    return 0; 
}
