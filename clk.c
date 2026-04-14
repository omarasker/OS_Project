#include "headers.h"

int shmid;

void cleanup(int signum)
{
    shmctl(shmid, IPC_RMID, NULL);
    printf("Clock terminating!\n");
    exit(0);
}

int main()
{
    printf("Clock starting\n");

    signal(SIGINT, cleanup);

    shmid = shmget(SHKEY, 4, IPC_CREAT | 0644);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    int *clk = (int *) shmat(shmid, NULL, 0);
    if (clk == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }

    *clk = 0;

    while (1) {
        sleep(1);
        (*clk)++;
    }
}