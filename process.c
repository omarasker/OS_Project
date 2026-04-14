#include "headers.h"

/* Modify this file as needed*/
int remainingtime;

int main(int agrc, char *argv[])
{
    union Semun semun;
    
    key_t semcsync = ftok("process_generator", 110);
    if (semcsync == -1) {
        perror("ftok failed");
        exit(1);
    }
    
    int semsyncid = semget(semcsync, 1, IPC_CREAT | 0666);
    if (semsyncid == -1) {
        perror("semget failed");
        exit(1);
    }

    semun.val = 0;
    if (semctl(semsyncid, 0, SETVAL, semun) == -1) {
        perror("semctl failed during initialization");
        exit(1);
    }
    
    key_t semcsync2 = ftok("process_generator", 111);  
    if (semcsync2 == -1) {
        perror("ftok failed for second semaphore");
        exit(1);
    }
    
    int semsyncid2 = semget(semcsync2, 1, IPC_CREAT | 0666);
    if (semsyncid2 == -1) {
        perror("semget failed for second semaphore");
        exit(1);
    }
    // Initialize the second semaphore to 0
    semun.val = 0;
    if (semctl(semsyncid2, 0, SETVAL, semun) == -1) {
        perror("semctl failed during initialization of second semaphore");
        exit(1);
    }
    initClk();
    remainingtime = atoi(argv[1]);

    printf("the remaining time for the created process is %d ...\n",remainingtime);
    int temp = getClk(); 
    //TODO The process needs to get the remaining time from somewhere
    //remainingtime = ??;
while (remainingtime > 0)
{
    int current_time = getClk();
    if (temp != current_time) {
        up(semsyncid);     
        down(semsyncid2); 
        remainingtime--;
        temp = current_time;
    }
}

        
    shmdt(shmaddr);
    destroyClk(false);
    printf("Process exited\n");
    exit(0);
    return 0;
}