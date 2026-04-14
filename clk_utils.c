#include "headers.h"

static int shmid = -1;
int *shmaddr = NULL;

int getClk() {
    return *shmaddr;
}

void initClk() {
    while ((shmid = shmget(SHKEY, 4, 0444)) == -1) {
        sleep(1);
    }

    shmaddr = (int *) shmat(shmid, NULL, 0);
    if (shmaddr == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }
}

void destroyClk(bool terminateAll) {
    if (shmaddr != NULL) {
        if (shmdt(shmaddr) == -1) {
            perror("shmdt failed");
        }
        shmaddr = NULL;
    }
    if (terminateAll && shmid != -1) {
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl failed");
        }
        shmid = -1;
    }
}

void down(int sem) {
    struct sembuf op = {0, -1, 0};
    if (semop(sem, &op, 1) == -1) {
        perror("semop down failed");
        exit(1);
    }
}

void up(int sem) {
    struct sembuf op = {0, 1, 0};
    if (semop(sem, &op, 1) == -1) {
        perror("semop up failed");
        exit(1);
    }
}
