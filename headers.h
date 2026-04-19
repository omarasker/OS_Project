#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define MAX_SIZE 100
#define SHKEY 300

typedef short bool;
#define true 1
#define false 0

/* ================= PROCESS ================= */
struct Process {
    int id;
    int arrival_time;
    int running_time;
    int priority;
    int MEMSIZE;

    int remaining_time;
    int finish_time;
    int state;
    int start_time;
    int last_stop_time;
    int waiting_time;
    pid_t pid;
};

/* ================= MESSAGE ================= */
struct msgbuff {
    long mtype;
    struct Process process;
};

/* ================= SEMAPHORE ================= */
union Semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* ================= CLOCK SHM ================= */
extern int *shmaddr;

int getClk();
void initClk();
void destroyClk(bool terminateAll);

/* ================= SEMAPHORE OPS ================= */
void down(int sem);
void up(int sem);

#endif