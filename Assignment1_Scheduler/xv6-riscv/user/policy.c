#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main (int argc, char* argv[]){
    if (argc == 2)
    {
        int policy = atoi(argv[1]);
        if (set_policy(policy) == 0){
            printf("Priority succesfully changed to: %s", argv[1]);
            exit(0, "\n");
        }
    }
    exit(1, "Wrong arguments\n");
}